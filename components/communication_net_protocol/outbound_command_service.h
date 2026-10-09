#pragma once

#include <limits>
#include "command_arguments.h"
#include "outbound_communication_transaction_runner.h"

namespace esphome::communication_net_protocol::outbound {

enum class SubmissionStatus : uint8_t { ADMITTED, BUSY, INVALID, UNAVAILABLE };
enum class SubmissionKind : uint8_t { NORMAL, PRIORITY_INTERRUPT };
struct CommandDraft {
  BoundedText<63> destination;
  CommandTarget target;
  CommandIntent command;
  uint32_t timeout_ms{0};
  SubmissionKind kind{SubmissionKind::NORMAL};
};
struct Submission {
  SubmissionStatus status{SubmissionStatus::INVALID};
  TransactionId transaction_id{0};
};
struct SubmissionContext {
  BoundedText<63> destination;
  CommandTarget target;
  TransactionId transaction_id{0};
};
class SubmissionObserver {
 public:
  virtual ~SubmissionObserver() = default;
  virtual void on_submission_result(const SubmissionContext &, const CommandResult &, bool terminal) = 0;
};
class CommandSubmissionPort {
 public:
  virtual ~CommandSubmissionPort() = default;
  virtual Submission submit(const CommandDraft &, uint32_t now_ms) = 0;
};

// Shared scheduling/admission only: the existing transaction runner owns
// acceptance, fallback, deadlines and late-result handling. Each lane requires
// its own adapters and registry; registries cannot share adapter instances.
class CommandSubmissionService : public CommandSubmissionPort {
 public:
  CommandSubmissionService(CommunicationTransportRegistry &first,
                           CommunicationTransportRegistry &second,
                           CommunicationTransportRegistry &interrupt)
      : lanes_{Lane(first), Lane(second), Lane(interrupt)} {}
  CommandSubmissionService(const CommandSubmissionService &) = delete;
  CommandSubmissionService &operator=(const CommandSubmissionService &) = delete;

  bool configure_session(const char *source, uint64_t boot_id) {
    if (boot_id_ != 0 || boot_id == 0 || !source_.assign(source) || source_.empty()) return false;
    boot_id_ = boot_id;
    return true;
  }
  bool add_policy(const char *id, const CommunicationPolicy &policy) {
    if (policy_count_ == 16 || policy_(id) != nullptr || !policy.is_valid()) return false;
    auto &entry = policies_[policy_count_];
    if (!entry.id.assign(id) || entry.id.empty()) return false;
    entry.policy = policy;
    ++policy_count_;
    return true;
  }
  bool add_destination(const char *id, const char *device, const char *policy) {
    if (destination_count_ == 16 || destination_(id) != nullptr || policy_(policy) == nullptr) return false;
    auto &entry = destinations_[destination_count_];
    if (!entry.id.assign(id) || entry.id.empty() || !entry.device.assign(device) || entry.device.empty() ||
        !entry.policy.assign(policy)) return false;
    ++destination_count_;
    return true;
  }
  bool add_observer(SubmissionObserver *observer) {
    if (observer == nullptr || observer_count_ == 4) return false;
    for (size_t i = 0; i < observer_count_; ++i) if (observers_[i] == observer) return false;
    observers_[observer_count_++] = observer;
    return true;
  }

  Submission submit(const CommandDraft &draft, uint32_t now_ms) override {
    const auto *destination = destination_(draft.destination.c_str());
    if (destination == nullptr || !draft.target.is_valid() || draft.target.resource.empty() ||
        !draft.command.is_valid() || draft.timeout_ms == 0 ||
        std::strcmp(destination->device.c_str(), draft.target.device_id.c_str()) != 0)
      return {SubmissionStatus::INVALID, 0};
    CommandArguments arguments;
    if (!decode_command_arguments(draft.command.payload.data(), draft.command.payload.size(), arguments))
      return {SubmissionStatus::INVALID, 0};
    const auto *policy = policy_(destination->policy.c_str());
    if (boot_id_ == 0 || policy == nullptr) return {SubmissionStatus::UNAVAILABLE, 0};
    // No new normal command may overtake another command for its resource,
    // including an active interrupt. Callers retry BUSY without a new effect.
    if (draft.kind == SubmissionKind::NORMAL) {
      for (const auto &lane : lanes_)
        if (lane.occupied && same_target_(lane.context.target, draft.target)) return {SubmissionStatus::BUSY, 0};
    }
    const size_t first = draft.kind == SubmissionKind::PRIORITY_INTERRUPT ? 2 : 0;
    const size_t limit = draft.kind == SubmissionKind::PRIORITY_INTERRUPT ? 3 : 2;
    Lane *selected = nullptr;
    bool free_lane = false;
    for (size_t i = first; i < limit; ++i) {
      auto &lane = lanes_[i];
      if (lane.occupied) continue;
      free_lane = true;
      if (!identities_match_(lane.registry, policy->policy)) continue;
      if (!has_available_(lane.registry, policy->policy)) continue;
      selected = &lane;
      break;
    }
    if (selected == nullptr) return {free_lane ? SubmissionStatus::UNAVAILABLE : SubmissionStatus::BUSY, 0};
    if (sequence_ == UINT32_MAX || boot_id_ > std::numeric_limits<uint64_t>::max() - sequence_ - 1U)
      return {SubmissionStatus::UNAVAILABLE, 0};
    CommandRequest request;
    request.transaction_id = boot_id_ + ++sequence_;
    request.target = draft.target;
    request.command = draft.command;
    request.timeout_ms = draft.timeout_ms;
    request.policy_id = destination->policy;
    selected->context = {draft.destination, draft.target, request.transaction_id};
    selected->occupied = true;
    if (!selected->runner.start(request, policy->policy, now_ms)) {
      selected->occupied = false;
      return {SubmissionStatus::INVALID, 0};
    }
    return {SubmissionStatus::ADMITTED, request.transaction_id};
  }

  void loop(uint32_t now_ms) {
    for (auto &lane : lanes_) {
      lane.runner.loop(now_ms);
      if (!lane.occupied) continue;
      CommandResult result;
      if (lane.runner.take_progress_result(result)) notify_(lane.context, result, false);
      if (lane.runner.take_result(result)) {
        const SubmissionContext context = lane.context;
        lane.occupied = false;  // Release before notifying a reentrant caller.
        notify_(context, result, true);
      }
    }
  }

 protected:
  struct PolicyEntry { BoundedText<31> id; CommunicationPolicy policy; };
  struct DestinationEntry { BoundedText<63> id, device; BoundedText<31> policy; };
  struct Lane {
    explicit Lane(CommunicationTransportRegistry &registry) : registry(registry), runner(registry) {}
    CommunicationTransportRegistry &registry;
    CommunicationTransactionRunner runner;
    SubmissionContext context;
    bool occupied{false};
  };
  const PolicyEntry *policy_(const char *id) const {
    if (id == nullptr) return nullptr;
    for (size_t i = 0; i < policy_count_; ++i) if (std::strcmp(policies_[i].id.c_str(), id) == 0) return &policies_[i];
    return nullptr;
  }
  const DestinationEntry *destination_(const char *id) const {
    if (id == nullptr) return nullptr;
    for (size_t i = 0; i < destination_count_; ++i) if (std::strcmp(destinations_[i].id.c_str(), id) == 0) return &destinations_[i];
    return nullptr;
  }
  static bool same_target_(const CommandTarget &a, const CommandTarget &b) {
    return std::strcmp(a.device_id.c_str(), b.device_id.c_str()) == 0 &&
           std::strcmp(a.resource.c_str(), b.resource.c_str()) == 0;
  }
  bool identities_match_(const CommunicationTransportRegistry &registry, const CommunicationPolicy &policy) const {
    for (size_t i = 0; i < policy.transport_count(); ++i) {
      const auto *adapter = registry.find(policy.transport_at(i));
      if (adapter == nullptr || !adapter->application_identity_matches(source_.c_str(), boot_id_)) return false;
      for (const auto &other : lanes_)
        if (&other.registry != &registry && other.registry.find(policy.transport_at(i)) == adapter) return false;
    }
    // Reusing the registry itself between lanes would also reuse its adapters.
    size_t owners = 0;
    for (const auto &lane : lanes_) if (&lane.registry == &registry) ++owners;
    return owners == 1;
  }
  static bool has_available_(const CommunicationTransportRegistry &registry, const CommunicationPolicy &policy) {
    const auto available = registry.availability();
    for (size_t i = 0; i < policy.transport_count(); ++i)
      if (available.is_available(policy.transport_at(i))) return true;
    return false;
  }
  void notify_(const SubmissionContext &context, const CommandResult &result, bool terminal) {
    for (size_t i = 0; i < observer_count_; ++i) observers_[i]->on_submission_result(context, result, terminal);
  }
  Lane lanes_[3];
  PolicyEntry policies_[16];
  DestinationEntry destinations_[16];
  SubmissionObserver *observers_[4]{};
  BoundedText<63> source_;
  uint64_t boot_id_{0};
  uint32_t sequence_{0};
  size_t policy_count_{0}, destination_count_{0}, observer_count_{0};
};
}  // namespace esphome::communication_net_protocol::outbound

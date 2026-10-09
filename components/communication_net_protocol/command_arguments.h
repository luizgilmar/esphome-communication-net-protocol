#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace esphome {
namespace communication_net_protocol {

enum class ArgumentKind : uint8_t { NONE = 0, VALUE = 1, RGB = 2 };

struct CommandArguments {
  ArgumentKind kind{ArgumentKind::NONE};
  uint16_t value{0};
  uint8_t red{0}, green{0}, blue{0};
  uint8_t canonical[4]{};
  size_t canonical_size{0};
};

// Bounded, transport-independent JSON subset for numeric actuator parameters.
// Object order and whitespace do not change replay identity. Unknown keys,
// duplicate keys, fractional values, overflow and trailing input are rejected.
class ArgumentReader {
 public:
  ArgumentReader(const uint8_t *data, size_t size) : data_(data), size_(size) {}
  bool read(CommandArguments &out, size_t &consumed) {
    out = {};
    consumed = 0;
    if (data_ == nullptr || size_ == 0 || size_ > 128 || !take_('{')) return false;
    if (take_('}')) { consumed = pos_; return true; }
    uint8_t seen = 0;
    uint16_t channels[3]{};
    do {
      char key[8]{};
      if (!key_(key) || !take_(':')) return false;
      const uint8_t bit = std::strcmp(key, "value") == 0 ? 1 :
                          std::strcmp(key, "red") == 0 ? 2 :
                          std::strcmp(key, "green") == 0 ? 4 :
                          std::strcmp(key, "blue") == 0 ? 8 : 0;
      if (bit == 0 || (seen & bit)) return false;
      uint16_t number = 0;
      if (!number_(number)) return false;
      if (bit == 1) out.value = number;
      else {
        if (number > 255) return false;
        channels[bit == 2 ? 0 : bit == 4 ? 1 : 2] = number;
      }
      seen |= bit;
      if (take_('}')) break;
      if (!take_(',')) return false;
    } while (true);
    if (seen == 1) {
      out.kind = ArgumentKind::VALUE;
      out.canonical[0] = static_cast<uint8_t>(ArgumentKind::VALUE);
      out.canonical[1] = static_cast<uint8_t>(out.value >> 8);
      out.canonical[2] = static_cast<uint8_t>(out.value);
      out.canonical_size = 3;
    } else if (seen == 14) {
      out.kind = ArgumentKind::RGB;
      out.red = static_cast<uint8_t>(channels[0]);
      out.green = static_cast<uint8_t>(channels[1]);
      out.blue = static_cast<uint8_t>(channels[2]);
      out.canonical[0] = static_cast<uint8_t>(ArgumentKind::RGB);
      out.canonical[1] = out.red;
      out.canonical[2] = out.green;
      out.canonical[3] = out.blue;
      out.canonical_size = 4;
    } else return false;
    consumed = pos_;
    return true;
  }
 private:
  void spaces_() {
    while (pos_ < size_ && (data_[pos_] == ' ' || data_[pos_] == '\n' || data_[pos_] == '\r' || data_[pos_] == '\t')) ++pos_;
  }
  bool take_(char c) {
    spaces_();
    if (pos_ == size_ || data_[pos_] != static_cast<uint8_t>(c)) return false;
    ++pos_; return true;
  }
  bool key_(char *out) {
    if (!take_('"')) return false;
    size_t n = 0;
    while (pos_ < size_ && data_[pos_] != '"') {
      if (n == 7 || data_[pos_] < 'a' || data_[pos_] > 'z') return false;
      out[n++] = static_cast<char>(data_[pos_++]);
    }
    return n != 0 && take_('"');
  }
  bool number_(uint16_t &value) {
    spaces_();
    size_t digits = 0;
    const bool zero = pos_ < size_ && data_[pos_] == '0';
    while (pos_ < size_ && data_[pos_] >= '0' && data_[pos_] <= '9') {
      const uint8_t digit = data_[pos_++] - '0';
      if (value > (UINT16_MAX - digit) / 10) return false;
      value = static_cast<uint16_t>(value * 10 + digit);
      ++digits;
    }
    return digits != 0 && !(zero && digits > 1);
  }
  const uint8_t *data_;
  size_t size_, pos_{0};
};

inline bool decode_command_arguments(const uint8_t *data, size_t size, CommandArguments &out) {
  out = {};
  if (size == 0) return true;
  size_t consumed = 0;
  ArgumentReader reader(data, size);
  if (!reader.read(out, consumed)) return false;
  while (consumed < size && (data[consumed] == ' ' || data[consumed] == '\n' || data[consumed] == '\r' || data[consumed] == '\t')) ++consumed;
  return consumed == size;
}

}  // namespace communication_net_protocol
}  // namespace esphome

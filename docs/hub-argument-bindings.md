# Argumentos dos recursos do HUB — 23/11/2026

## Implementado no componente genérico

As ações communication_net_protocol.apply_rgb, apply_brightness e apply_position
consomem diretamente a tupla validada do binding de entrada. Não usam valores
globais nem lambdas no YAML. RGB usa 0..255; brilho 1..100%; posição 0..100%.
RGB limpa efeito ativo como o executor anterior. Brilho preserva o efeito.
Posição chama o objeto cover; nunca acessa relés de direção.
Ações só podem ser geradas no trigger inbound com a assinatura esperada.
A configuração rejeita tipo/faixa incompatível, inclusive em ações aninhadas.
Não há arredondamento, clamp ou conversão de um pedido inválido em outro comando.

## Pacotes reutilizáveis

packages/light_argument_bindings.yaml recebe IDs de binding, recurso, escopo de
execução, luz, alimentação, tempo de estabilização e timeout. Pode ser incluído
mais de uma vez; Superior/Mesa não estão codificados no componente C++.
A alimentação liga antes do ajuste e respeita o atraso declarado. O escopo deve
ser o mesmo dos bindings do grupo e individuais para coordenar a alimentação.
Este pacote contém apenas RGB/brilho. Os bindings de ligar/desligar/alternar e
consulta devem ser consolidados com a regra de manter a fonte enquanto outra
fita estiver ligada; não substituir essa regra ao instalá-lo.

packages/cover_position_binding.yaml recebe recurso, cover e timeout. Deve ser
combinado com abrir/fechar/parar e cover_cycle.yaml no mesmo recurso. O binding
stop precisa interrupts_active e completion idle. O ajuste de posição continua
no time_based cover, inclusive a estimativa por tempo; não implica sensor físico.

## Evidências e limitações

44 testes Python passaram (contrato NSPanel, limites de snapshot, rotas inbound,
dependencias e schema outbound), incluindo
sete novos casos de compatibilidade de ações. A geração ESPHome 2026.9.1 passou
nos perfis nspanel_hub_schema_check e nspanel_hub_argument_packages_check.
Este último inclui o mesmo pacote duas vezes com luzes independentes e uma fonte
compartilhada, além do pacote de posição. Só há saídas virtuais; não instalar.
Os dois perfis de validação antigos tiveram suas conversões migradas para ações.
O efeito vazio de teste existente em all_bindings ainda é uma lambda herdada;
não declarar todo esse perfil nem o HUB integralmente migrado.

A tentativa de compilação binária não passou pela preparação do ESP-IDF: o
ambiente tentou baixá-lo e falhou na resolução de github.com. Não é evidência
de falha ou sucesso C++ destas ações. Teste de hardware e geração all_bindings
completa ainda pendentes. Nenhuma referência Git instalada foi alterada,
nenhum push, firmware ou comando para equipamento real foi enviado.

## Trabalho ainda necessário antes do perfil instalável

- Consolidar os bindings individuais completos de FitaMesa e Superior, mantendo
  a coordenação da fonte com Meio e o grupo de três fitas.
- Incorporar ciclo e posição ao perfil real do HUB com o mesmo recurso.
- Corrigir a associação de banheiro_geral para light/saida_livre_3 no perfil real;
  o alias de teste light/banheiro_geral não é o binding do HUB atual.
- Publicar e receber estados individuais; o snapshot legado tem spot, strips,
  panel1 e panel2. strips agrega três fitas e não representa uma fita individual.
- O snapshot atual suporta luz/binário, mas não posição/movimento de cover;
  será preciso estender o contrato ou uma observação tipada, sem inventar campos.
- O cadastro NSPanel tem adaptador de envio, mas ainda não recebe snapshots do
  HUB pelo modelo genérico. Reutilizar a infraestrutura de observação já validada
  no TX, com limites de payload, freshness e recuperação sem rajadas.
- Publicar versões Git fixas somente após revisão e autorização de publicação.

Estas pendências impedem tratar o perfil full atual como instalável.


## Complemento: comandos individuais e alimentação compartilhada

powered_rgb_light.yaml compõe as conversões e acrescenta on/off/toggle/get.
IDs de scripts/bindings, recurso, alvo, fonte, escopo e tempos vêm do perfil.
As sequências usam ações/condições nativas ESPHome, sem lambdas. A regra de
liberar a fonte está em C++ (LightSupplyGroup): verifica current_values e
remote_values de todos os membros antes de desligar. Isso preserva alimentação
na transição de desligamento e diante de um pedido de ligar ainda em transição.
Se algum membro ainda estiver aceso, a fonte permanece ligada. Não há tentativa
periódica de corte posterior: uma transição mais longa que o atraso configurado
pode deixar a fonte ligada com todas apagadas, opção conservadora sem corte
prematuro. O perfil usa 1100ms de espera; calibrar no hardware se necessário.
Os grupos exigem membros únicos, não vazios, diferentes da própria fonte;
uma mesma fonte não pode ter dois grupos parciais no componente.

quartogian_nspanel_bindings.yaml é um candidato de perfil: inclui o mesmo pacote
para Superior e Mesa, com membros Superior/Meio/Mesa e escopo alimentacao_fitas;
acrescenta posição e ciclo da persiana. Não redefine abrir/fechar/parar existentes.
O stop existente precisa continuar interrupts_active com completion idle.
Este arquivo não foi aplicado ao HUB, nem deve ser anexado às extensões antigas
que já declaram as mesmas rotas. A migração deve substituir bindings duplicados
e associar os bindings legados do grupo ao mesmo execution_scope antes de instalar.
O pacote não intercepta comandos HA diretos às entidades; coordenação fora dos
bindings e o on_turn_on da alimentação permanecem responsabilidade do perfil.

O exemplo quartogian_nspanel_bindings_check gera o candidato com saídas virtuais,
incluindo stop correspondente da persiana. A geração C++ ESPHome 2026.9.1 passou;
49 testes Python passaram, cinco novos para a configuração de alimentação.
A geração anterior com duas inclusões distintas também passou. Não equivale a
compilação binária nem teste físico de transições/concorrência.

Para compilar este bloco completo, a partir de qualquer diretório:

```powershell
& "C:\Users\luizg\OneDrive\projetos\Esphome\esphome-communication-net-protocol\tools\compilar-bindings-hub.ps1"
```

O script verifica ESPHome 2026.9.1, mantém build/cache/log em AppData fora do
OneDrive e não possui upload. -OnlyGenerate é a alternativa de geração apenas.
O firmware resultante usa equipamentos virtuais e não é instalável no HUB.
Observação de estados e a migração final de escopos/rotas continuam pendentes.


## Conexão direta: estados individuais (23/11/2026)

Compilação binária anterior confirmada: compile-20261123-163505.log,
config_hash 0x54e23493, imagem 882491 bytes, DRAM 72824 (40,3%), flash 48,1%.
É a validação de bindings anterior à ampliação de snapshot abaixo; sem upload.

A extensão quartogian_nspanel_states.yaml acrescenta general/SaidaLivre3,
superior/FitaSuperior e mesa/FitaMesa ao snapshot. Os quatro campos legados
spot/strips/panel1/panel2 são preservados pelo perfil chamador. O peer criptografado
NSPanel e o roteamento MQTT de respostas precisam ser configurados no perfil;
o pacote recebe nspanel_peer_id e registra a autorização de observação desse peer.
Não aplicar duas vezes os campos ou o observer. A fixture já tem origem e peer
NSPanel declarados, com chaves exclusivamente de teste.

A mensagem completa tem sete campos: pior caso MQTT 536 bytes, rádio 93 bytes.
O orçamento JSON passou de 447 para 767 bytes, centralizado no header para
produtor/decoder/NSPanel. Buffer C do produtor 768 bytes; nenhum aumento no
limite binário de 256 bytes nem mudança de state-fields/v2. O limite de oito
campos também permanece. Testes rejeitam sete campos todos RGB (acima do JSON),
nomes que excedam o orçamento de rádio e divergência entre limites Python/C++.
O TX atual recebe apenas campos próprios e ignora adicionais (fonte local
retained_state_observer.cpp conferida); compatibilidade física ainda a testar.
O aumento do produtor é 320 bytes de buffer temporário; o NSPanel tem mais
320 bytes por mailbox MQTT e 320 no temporário de processamento, com quatro
mailboxes alocadas no modelo atual. Medir a RAM real na próxima compilação.

51 testes Python de protocolo passaram. A geração C++ do HUB com sete campos
passou no ESPHome 2026.9.1. Essa revisão ainda não foi compilada em binário.
O receptor nspanel_snapshot já existia e foi validado na bancada; não houve
nova implementação paralela de lease/query/handover. Seu perfil candidato
está no repositório NSPanel, packages/gian_hub_state_mappings.yaml.
Cover ainda não tem telemetria nesse formato. Posição/movimento continuam
pendentes; não afirmar que o estado completo do HUB já foi integrado.


## Correção de isolamento library_only (23/11/2026)

O log NSPanel compile-20261123-171445.log falhou no main.cpp ao incluir
argument_actions.h: light/light_state.h ausente. ESPHome inclui todos os headers
externos diretamente em esphome.h, portanto a proteção apenas no header pai não
isolava os novos arquivos. argument_actions.h e powered_light.h agora protegem
seu próprio conteúdo com os respectivos defines de capacidade, antes de incluir
luz/cover. O NSPanel library_only não habilita essas ações; o HUB continua
habilitando-as quando usadas. Não foi acrescentado componente light/cover ao
NSPanel para contornar o erro. Regressão cobre os dois pontos de entrada.
Compilação binária de confirmação ainda pendente; não atribuir a falha aos avisos
de bootloader ou de caminho longo anteriores ao erro fatal.


## Telemetria independente de cover

cover_observation_binding.yaml expõe uma rota de leitura parametrizada com
completion expected observed e nenhuma ação no cover. O perfil Gian escolhe
state/cover/persiana. Dois slots normais permitem leitura enquanto um comando
acompanha o movimento pela rota cover/persiana. A rota de observação não é um
atalho para relés. As permissões/origens/peers existentes continuam obrigatórias.

As respostas cover-state/v1 via MQTT agora incluem data_hex além de preservar
o objeto value anterior. Isso permite ao cliente compartilhado recuperar os
mesmos dois bytes que recebe pelo rádio, sem alterar state-fields/v2 ou o TX.
cover_state_decoder.h valida posição 0..100, operação 0..2 e exatamente dois
bytes. As duas fixtures incluem regressões constexpr para as fronteiras e
frames inválidos; elas serão verificadas na compilação binária seguinte.

O consumidor genérico nspanel_cover_observer está no repositório NSPanel e usa
o runtime compartilhado. O perfil consulta a cada 10s com jitter, uma consulta
pendente e retentativas limitadas. Essa é telemetria por consulta, ainda não
push/lease de cover. Posição é estimada pelo time_based do HUB. Movimento e
direção são campos independentes; a sinalização visual dedicada de direção no
HMI ainda não foi configurada. 50 testes Python selecionados do protocolo e
62 do NSPanel passaram; geração dos dois perfis aprovada. Sem compilação binária
ou teste físico desta revisão, sem publicação Git remota nem upload.

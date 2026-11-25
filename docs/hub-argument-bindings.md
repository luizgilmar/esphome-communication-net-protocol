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


## Bancada integrada preparada

O produtor de estados pode receber nspanel_source_id (default painel-gian-porta)
para que a bancada use sua identidade sem duplicar pacotes. O runtime de envio
registra SUBMIT com comando/recurso/status/tx; o registro significa só admissão,
não resultado ou estado. A fixture direct-publisher.yaml da bancada inclui os
pacotes reais de fitas, cover e snapshots e usa saídas estritamente virtuais.
O consumer usa os observadores reais e tópicos bench/nspanel-direct/*.
Um teste manual de 22 casos está em bancada-cc2/tools/testar-direto.ps1;
consulta ambas as identidades e testa comandos/estado/alimentação/ciclo e
fallback por desabilitar somente MQTT do consumer, sem depender do HA.
Compilação e execução dessa bancada continuam pendentes. Geração dos dois perfis
aprovada; nenhuma gravação foi executada. A preparação também corrigiu o prazo
curto dos comandos de curso: timeout por operação é agora declarativo no perfil
NSPanel (25s movimentos, 3s parar/ciclo), sem alterar o prazo 5s das luzes.


## Resultado parcial da bancada e correção de concorrência (23/11/2026)

O teste direct-test-20261123-182132.log aprovou os primeiros 11 casos (luzes,
cor/brilho, fonte compartilhada, binários e cover fechado) e parou em abertura.
A abertura foi executada e concluída, mas consultas state/cover/persiana foram
recusadas localmente com status UNAVAILABLE enquanto o comando longo ocupava a
primeira via. A segunda via possui apenas MQTT: não aceita a política mista de
comandos que também exige adaptador de rádio, mesmo com broker disponível.
Não foi falha de movimento, timeout curto ou payload do HUB.

A correção permite override de política para state/cover/... exclusivamente em
get sem argumentos. O observador compartilha a prova de saúde MQTT do receptor
já existente: usa a política MQTT dedicada quando fresca, ou a política rádio
quando não. A via reservada de parada não é usada para consultas. Nenhum novo
adaptador ESP-NOW ou slot foi acrescentado. O receptor de cover e o perfil NSPanel
foram ajustados; o publisher de bancada não precisa ser recompilado/gravadado.

A regressão nativa outbound_command_service_test.cpp reproduz a recusa com
política mista e verifica admissão MQTT independente e STOP ainda reservado;
foi acrescentada, mas não executada localmente por ausência de compilador host.
A confirmação operacional é o próximo teste das duas placas. No modo somente
ESP-NOW, leitura durante comando longo ainda pode esperar a liberação da via
normal. Isso continua uma limitação pendente; não reclassificar os 11 PASS como
22 PASS nem afirmar telemetria de movimento offline validada.


## Movimento por eventos — revisão de 24/11/2026

Esta revisão substitui a consulta periódica do perfil Gian descrita acima.
Os dois firmwares da bancada precisam recompilar; o HUB instalado não foi alterado.
`motion_cover` mantém um cover público e encaminha comandos para um `time_based`
interno. Os tempos são obtidos da configuração desse backend, sem repetir a
calibração, acessar membros privados ou copiar o componente nativo. Teclas,
HA/API e protocolo devem usar o ID público; o backend é interno e tem um único
proprietário. Um acesso direto ao backend perde o plano e não produz destino inventado.

`cover_motion_publisher` publica início, inversão/novo destino, parada e término,
com heartbeat de 60s no perfil. O frame `cover-motion/v1` contém geração persistida,
revisão, ID do movimento, recurso, posição/destino em centésimos de percentual,
operação e duração restante. O frame é amostrado no momento do envio. Não exige
relógios sincronizados; atraso de transporte afeta a estimativa visual. Posição
continua estimada pelo time_based, não medida por sensor.

MQTT usa tópico próprio, QoS1 e retained; retained não substitui consulta fresca
no consumidor. ESP-NOW entrega somente aos peers com lease e interesse explícito
`cover_resources`. Luzes e covers compartilham o orçamento global do agendador,
com coalescência por peer/recurso, limites por nó/peer e recuo após falha. Não há
publicação de posição a cada 2s. O envio de estado pode ocorrer durante movimento
normal; resultados prontos e STOP têm precedência na admissão. Isto ainda precisa
ser confirmado nas placas. TX sem interesse declarado continua recebendo só luzes.

Uma parada/alteração externa encerra o acompanhamento antigo com INTERRUPTED,
sem parar o novo comando do outro controlador nem aguardar o timeout anterior.
O serviço conserva a consulta somente-leitura para sincronização/recuperação;
ela usa a política dedicada e não ocupa a via reservada para STOP.

Validação desta revisão: 44 testes Python selecionados do protocolo e geração
C++ do perfil HUB e das duas placas aprovados. Checagens constexpr do codec,
projeção e interrupção passaram com Clang em modo syntax-only. O executável
nativo do serviço não pôde ser executado neste ambiente; sua regressão não está
aprovada como teste comportamental. Compilação binária ESP32 e teste de 27 casos
nas placas permanecem pendentes. Nenhum upload ou push Git foi executado.

### Correção de exportação do cabeçalho (24/11/2026)

O build publisher compile-20261124-114048.log falhou por ausência de
motion_cover/motion_cover.h. A plataforma é carregada de motion_cover/cover;
o cabeçalho foi movido para essa pasta e o include do publisher foi corrigido.
Geração do HUB e publisher aprovada, presença do cabeçalho e ambos os includes
conferidos na árvore gerada. Seis testes de schema passaram. Repetir compilação
binária somente do publisher; compilação binária desta correção ainda pendente.


## Pacote de migração conferido contra o HUB instalado

A configuração enviada em 24/11/2026 tem 1213 linhas e difere da base candidata
somente pela ausência de cinco bindings: wardrobe_on/off e
bathroom_general_on/off/toggle. A base de controle foi alinhada ao arquivo
instalado; esses cinco bindings agora pertencem ao pacote de migração
packages/quartogian_nspanel_installation.yaml deste repositório.

O pacote inclui os bindings individuais, campos de estado e eventos de cover
por includes relativos no próprio Git. Acrescenta origem MQTT/reply e peer do
NSPanel; preserva TX. Os 19 bindings do grupo existente recebem o mesmo escopo
das operações individuais. O proxy público persiana preserva nome e o backend
interno recebe ações e durações da configuração anterior.

Consumidor HA: incluir esse pacote remoto por commit fixo e atualizar a fonte
external_components do protocolo para o core testado 234bc9d, com
[communication_net_protocol, motion_cover, cover_motion_publisher]. O pacote
não muda fontes de Wi-Fi/rádio, SX1509, efeitos existentes ou secrets do HUB.
Adicionar ao secrets do HA a mesma espnow_hub_nspanel_lmk preparada no NSPanel.
Não copiar fontes dos componentes para my_components.

Quatro testes de merge/preservação no repositório TX passaram, incluindo nomes
que determinam restauração de posição, TX, escopo sem alterar ações e inclusão
das cinco rotas ausentes. Isso não é compilação completa do HUB no ambiente HA.
Compilar e conferir o resultado antes de qualquer upload.


## Capacidade do cadastro: HUB real com 45 comandos

O aviso HA "length of value must be at most 40" corresponde ao limite da lista
inbound.bindings. A expansão do arquivo instalado soma 25 bindings existentes,
5 binários novos, 12 das duas fitas individuais e 3 de cover: total 45.
A comparação anterior de merge não havia expandido todos os pacotes aninhados;
foi acrescentada uma verificação dessa composição completa.

Schema e static_assert do InboundRouteRegistry agora permitem até 64 entradas.
COMMUNICATION_NET_INBOUND_CAPACITY continua sendo gerado com o comprimento real
da lista (45 aqui), sem reservar 64 em todos os dispositivos. normal_executions
continua 2, filas/replay/tentativas e a via de interrupção não foram ampliados.
Isso amplia o cadastro, não o paralelismo. Novos comandos ainda precisam dos
seus objetos/ações; conferir RAM na compilação real do HUB.

Três testes Python do contrato/capacidade e cinco do overlay do HUB passaram.
Fronteiras 45/64 aceitas e 0/65 recusadas; o teste nativo inclui preenchimento e
recusa de overflow em 64. Este último passou somente em syntax-only com Clang,
não foi executado como binário. Compilação binária do HUB no HA permanece pendente.


## Compatibilidade dos relés após trocar o firmware do painel

O Blackymas instalado expõe os relés usados pelo HUB como light; o NSPanel Easy
candidato registra relay_1/relay_2 como switches (conferido no código gerado do
binário compilado). O HUB não deve continuar atuando/observando as entidades
light após a migração. Essa dependência não estava encerrada na preparação
anterior e foi identificada antes de qualquer upload no painel.

O pacote genérico ha_relay_associations.yaml associa binding, entidade de
comando e entidade observada por parâmetros de include. O perfil Gian escolhe
switch.painel_gian_porta_relay_1/2 e switch.toggle. Os recursos de protocolo
light/painel_gian_porta_relay_1/2 são preservados para o TX; apenas a execução
HA muda. Os IDs dos sensores/completion também são preservados.

A composição remove primeiro as ações herdadas e depois coloca a ação nova.
Isso é necessário porque listas then são concatenadas em !extend: apenas
acrescentar switch.toggle deixaria também o light.toggle antigo. O teste exige
exatamente uma ação por binding e a mesma entidade na observação correspondente.
Não há novas lambdas, alteração de GPIO ou ações de retorno duplicadas.

Seis testes no repositório TX passaram usando os passes reais de pacotes,
substituições e resolução !extend/!remove. A expansão cobre o grafo real de
comandos, observação e cover, excluindo includes de efeitos/hardware que só
estão no HA. Total preservado: 45 comandos únicos, 2 execuções normais.
Compilar essa atualização do HUB antes da transição; manter o binário preparado,
sem instalar enquanto os relés ainda estiverem no firmware antigo. Coordenar
com a instalação do NSPanel e conferir os IDs switch no HA após a troca.
O firmware NSPanel e o TFT já compilados não mudam por essa correção YAML.

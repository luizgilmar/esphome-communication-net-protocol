# Mapeamento de tela — NSPanel Gian Porta US

Base: parâmetros de Blueprint_Gian_Porta.txt e painel-gian-porta.yaml fornecidos pelo usuário. Este é o inventário da configuração explícita; não houve inspeção da tela física, expansão do blueprint instalado nem confirmação de seus valores padrão. A numeração indica slots lógicos; posição visual e gestos exatos dependem do TFT/blueprint instalado.

## Tela inicial

| Item | Campo | Entidade/origem | Apresentação configurada | Dependência atual |
|---|---|---|---|---|
| Tempo e previsão | weather_entity | weather.forecast_casa | Nome/ícone não sobrescritos | HA e integração meteorológica |
| Temperatura interna | indoortemp | sensor.ac_gian_room_temperature | Formatação em português, vírgula decimal | HA; origem do sensor precisa ser confirmada |
| Umidade | chip01 | sensor.ar_escritorio_humidity | mdi:water-percent | HA; sensor está nomeado como escritório, preservar até confirmação |
| Acesso ao clima | climate | climate.ac_gian | climate_chip_always_visible: true | HA + LG ThinQ |
| Atalho 1 | home_custom_button01 | light.quartogian_saida_livre_5 | PersianaUP / mdi:curtains | HA atualmente; relação com persiana no HUB a confirmar |
| Atalho 2 | home_custom_button02 | light.quartogian_saida_livre_6 | PersianaDW / mdi:curtains-closed | HA atualmente; relação com persiana no HUB a confirmar |
| Atalho 3 | home_custom_button03 | scene.game | Game / mdi:controller-classic | HA; ações da cena não fornecidas |
| Data | date_format | Relógio/origem conforme pacote | %A, %d/%m | Fonte de hora não especificada no arquivo local |

Correção posterior do usuário: os atalhos PersianaUP/PersianaDW foram eliminados ao adotar o controle temporizado no HUB. As entradas acima registram o arquivo histórico recebido, não a configuração atual desejada. Não migrar esses atalhos, não criar comandos diretos para as saídas 5/6 e não contornar o componente da persiana. A configuração em uso diverge nesse ponto do TXT fornecido.

## Teclas físicas e barras de estado

| Tecla | Nome | Entidade de ação e estado | Fallback explícito | Apresentação |
|---|---|---|---|---|
| Esquerda | Central | switch.painel_gian_porta_relay_1 | relay_1_local_fallback: true | Barra configurada para sempre aparecer |
| Direita | Mesa | switch.painel_gian_porta_relay_2 | relay_2_local_fallback: true | Barra configurada para sempre aparecer |

O usuário confirmou que os relés 1/2 alimentam diretamente essas luzes. Pressão longa direita: Default, sem ação personalizada (lista vazia). Pressão longa esquerda: sem sobrescrita explícita. Não considerar os gestos sem ação até conferir os padrões do blueprint.

## Página 1 — Quarto Gian

| Slot | Nome | Entidade configurada | Ícone | Cor explícita | Executor candidato |
|---|---|---|---|---|---|
| 01 | Central | switch.painel_gian_porta_relay_1 | mdi:lightbulb-spot | Padrão do blueprint | NSPanel, relé 1 |
| 02 | Armario | light.quarto_gian_quartogian_spot_armario | mdi:lightbulb-spot | Padrão do blueprint | HUB, confirmar binding |
| 03 | Mesa | switch.painel_gian_porta_relay_2 | mdi:lightbulb-spot | RGB 250,250,0 | NSPanel, relé 2 |
| 04 | Led Sup | light.quarto_gian_quartogian_fita_superior | mdi:led-strip-variant | RGB 250,250,0 | HUB, preservar controle individual da fita |
| 05 | Ar Gian | switch.botao_ar_gian | mdi:air-conditioner | Padrão do blueprint | Ainda não identificado |
| 06 | Persiana | cover.quarto_gian_quartogian_persiana_gian | mdi:blinds-horizontal | Padrão do blueprint | HUB, recurso cover/persiana |
| 07 | PersianaUP | Não atribuída no arquivo | mdi:curtains | Padrão do blueprint | Nenhuma rota explícita |
| 08 | PersianaDW | Não atribuída no arquivo | mdi:curtains-closed | Padrão do blueprint | Nenhuma rota explícita |

Slots 07/08 possuem somente nome e ícone no arquivo recebido. PersianaUP/PersianaDW não devem ser recriados na migração; excluir também esses slots do perfil local. O único item de persiana previsto é o slot 06, vinculado ao componente de controle do HUB.

## Página 2 — Banheiro Gian

| Slot | Nome | Entidade configurada | Ícone | Cor explícita | Executor candidato |
|---|---|---|---|---|---|
| 09 | Geral | light.quartogian_saida_livre_3 | mdi:lightbulb-spot | Padrão do blueprint | HUB, confirmar binding |
| 10 | Chuveiro | light.spot_chuveiro_gian | mdi:lightbulb-spot | RGB 250,250,0 | HUB, confirmar equivalência com light/spot_chuveiro |
| 11 | Sem nome explícito | Não atribuída | Vazio explícito | RGB 250,250,0 | Nenhuma rota explícita |
| 12 | Sem nome explícito | Não atribuída | Vazio explícito | Padrão do blueprint | Nenhuma rota explícita |
| 13 | Sem nome explícito | Não atribuída | Vazio explícito | Padrão do blueprint | Nenhuma rota explícita |
| 14–16 | Sem configuração explícita | Não atribuída no arquivo | Padrão do blueprint | Padrão do blueprint | Nenhuma rota explícita |

Slots 17–32 e títulos das páginas 3/4 não foram configurados explicitamente. Não inferir páginas visíveis ou slots preenchidos sem ler o blueprint instalado.

## Controles detalhados e navegação

| Área | Vínculo conhecido | O que preservar / verificar |
|---|---|---|
| Clima | climate.ac_gian, SmartThinQ LGE Sensors | Preservar submenu por toque longo, ajustes de temperatura e demais controles atuais compatíveis com a entidade; rota HA/ThinQ nesta etapa |
| Persiana | Slot 06, entidade cover | Preservar submenu correspondente, abrir/fechar/STOP e slider de posição. Todo comando deve passar pelo componente do HUB; adicionar rota de posição ao protocolo se necessária, após conferir capacidade e semântica do componente |
| Luzes | Slots 02,04,09,10, entidades light | Preservar submenus por toque longo com brilho, cor e temperatura de cor conforme recursos do item. Conferir capacidades de cada entidade para não oferecer ajuste sem suporte |
| Configurações da tela | Dependem do pacote/TFT | Inventariar brilho, redução de brilho, repouso, temporizadores e reinício; preservar os ajustes existentes |
| Navegação | Dependente do TFT/blueprint | Confirmar gestos, troca de página, retorno, pressão longa e página ao acordar; não definidos na automação recebida |
| Inicialização | Firmware/TFT Blackymas | Substituir dependência de configuração remota por perfil local consistente; manter verificação da tela e compatibilidade |

## Apresentação global

- language: pt.
- decimal_separator: vírgula.
- timezone: America/Sao_Paulo (<-03>3), conforme configuração atual.
- button_pages_icon_size: '10'; preservar o parâmetro sem interpretar como pixels.
- Data: dia da semana e dia/mês, segundo %A, %d/%m.
- Preservar rótulos como escritos, incluindo Armario, Led Sup, PersianaUP e PersianaDW.
- As cores e ícones acima são parâmetros; cor efetivamente apresentada conforme estado depende da implementação.

## Perfil local proposto

1. Carregar páginas, rótulos, ícones e vínculos pelo firmware no boot, mesmo em uma primeira inicialização sem HA.
2. Central e Mesa na tela e nas teclas devem chamar a mesma execução local; atualização visual pelo estado local dos relés, sem alternância dupla na recuperação do HA.
3. Cargas do HUB devem usar communication_net_protocol, preservando ACK de aceitação, deduplicação, resultado final, ordenação por recurso e STOP.
4. Preservar a página de clima via HA/ThinQ. Rota independente do HA/internet foi deixada pelo usuário para revisão futura; não adicionar emissor IR nesta etapa.
5. Dados externos devem indicar indisponibilidade ou última atualização. Não converter valor em cache em confirmação de estado atual.
6. Preservar o controle individual de Led Sup e seus ajustes detalhados: o binding CC2 que agrupa fitas superior/meio não corresponde automaticamente a esse item.
7. Cena Game e switch.botao_ar_gian precisam ter suas ações levantadas antes de definir comportamento sem HA.

## Pendências objetivas

- Implementação e versão do blueprint instalado no HA, para resolver defaults, gestos e chamadas exatas.
- Definição de switch.botao_ar_gian e conteúdo de scene.game.
- Confirmar capacidades do componente atual de persiana para posição e seu estado publicado; saídas 5/6 estão excluídas da interface de comando.
- Recursos suportados e estados das entidades light, cover e climate usados na tela.
- Fontes reais dos sensores de temperatura/umidade e comportamento atual de relógio/repouso.

Nenhum item sem entidade recebeu uma ação inventada. Este documento não é um firmware para instalar; nenhum arquivo original foi alterado.

## Submenus confirmados pelo usuário e contrato de controle

O usuário confirmou que o blueprint possui páginas ocultas acessadas por toque longo, correspondentes ao tipo de item pressionado. Esses submenus integram o escopo obrigatório da migração, não são funcionalidades opcionais ou apenas páginas a descobrir. A configuração explícita de botões não descreve toda a interface.

| Tipo do item | Ajustes a preservar | Rota e confirmação |
|---|---|---|
| Luz compatível | Brilho, cor, temperatura de cor e demais ajustes existentes | Comandos ao executor da luz; retorno do estado efetivo. Inventariar capacidades por item; manter ajuste individual |
| Ar-condicionado | Temperatura e demais ajustes apresentados hoje | HA/ThinQ; sem essa rota, apresentar indisponibilidade e não executar comandos pendentes após recuperação |
| Persiana | Slider de posição e comandos de movimento/STOP apresentados hoje | Exclusivamente pelo componente de persiana do HUB; nunca acionar seus relés diretamente |

Cada submenu deve conservar o vínculo ao item que o abriu, carregar valores e capacidades desse item, atualizar com o estado recebido e retornar à página de origem. O perfil local deve carregar também essa navegação e seus controles, sem aguardar o blueprint no boot. A abertura da interface pode ser local mesmo quando o executor remoto está indisponível; o estado exibido deve indicar essa condição.

Para sliders, definir no protocolo comandos com valor e unidade (porcentagem de brilho/posição, cor, temperatura de cor ou temperatura alvo), conforme o contrato real do executor. Não substituir ajustes contínuos por uma lista de presets existentes no TX. Limitar a frequência e manter apenas a intenção mais recente ainda não enviada durante um gesto, preservando ordenação por recurso e prioridade de STOP; não reproduzir uma fila de posições antigas ao recuperar conexão.

ACK de aceitação confirma que o executor admitiu o comando; não confirma conclusão do movimento. O resultado da persiana deve acompanhar o estado real do componente, incluindo parada e interrupção, mantendo correlação/deduplicação do CC2. O temporizador pertence ao componente do HUB e não deve ser duplicado na tela ou no NSPanel.

Sem inspeção do TFT/blueprint instalado, ainda não foram confirmados identificadores internos das páginas, componentes Nextion, faixas/unidades, recursos por entidade ou o comportamento de emissão dos sliders. Esses dados são necessários para implementar o perfil sem perder os submenus.

## Círculo de cores — rastreamento dos fontes históricos

Consulta realizada em 06/10/2026. Foram recuperados o export textual da página light do NSPanel US na tag v4.3.13, o firmware core e o blueprint na tag v4.3.16. Essas versões correspondem às versões TFT/firmware informadas; não houve comparação binária com o TFT instalado nem captura serial do dispositivo.

### Resultado confirmado nos fontes

O círculo usa posição de toque internamente, mas envia ao ESP32 os canais RGB já calculados, em inteiros de 0 a 255. Não é necessário reconstruir a cor a partir de coordenadas no HUB ou HA.

1. Touch Press de colorwheel lê tch0/tch1, transforma coordenadas em posição relativa ao centro e determina anel e setor.
2. Touch Release usa setor para matiz e anel para saturação, fixa o valor HSV em 255 e executa hsv2rgb no próprio Nextion.
3. A rotina rgb888to565 também é chamada, mas o evento transmitido usa r/g/b, não o inteiro RGB565.
4. A página serializa um JSON de evento local com page=light, component=rgb_color e value=[R,G,B].
5. O core 4.3.16 lê esse array, associa detailed_entity e encaminha os canais ao blueprint; o blueprint monta rgb_color para light.turn_on.

Exemplo ilustrativo do corpo do evento (não é captura do painel):

```json
{"page":"light","component":"rgb_color","value":[255,0,0]}
```

O envelope serial histórico é 0x92, texto localevent, 0x00, JSON, 0x00, 0xFF 0xFF 0xFF. O componente colorwheel também habilita eventos padrão de ID no pressionar/soltar; o evento semântico de cor é o JSON enviado ao soltar. Evitar executar duas vezes ao processar ambos os tipos.

O cálculo é por setores/anéis, não uma leitura de cor de pixel. Matiz usa 24 setores. Como v é fixado no máximo, brilho é um ajuste separado; o RGB recebido não incorpora automaticamente a posição do slider de brilho. O código calcula a posição no pressionar e transmite a cor no soltar, portanto não presumir atualização contínua por arraste nessa versão.

Fontes oficiais:

- [TFT US 4.3.13 — export da página light](https://github.com/Blackymas/NSPanel_HA_Blueprint/blob/v4.3.13/hmi/dev/nspanel_us_code/light.txt).
- [Core 4.3.16 — receptor do evento local](https://github.com/Blackymas/NSPanel_HA_Blueprint/blob/v4.3.16/esphome/nspanel_esphome_core.yaml).
- [Blueprint 4.3.16 — chamada RGB](https://github.com/Blackymas/NSPanel_HA_Blueprint/blob/v4.3.16/nspanel_blueprint.yaml).

### Diferenças de versão e implicações para migração

O page_light de main recuperado nesta consulta descreve CSV light,rgb_color,R,G,B, distinto do JSON histórico. O formato do adaptador deve corresponder ao TFT escolhido e ser fixado com sua versão. O protocolo entre NSPanel e HUB pode representar RGB de forma estável, independentemente do envelope serial da tela.

Fonte: [page_light em main](https://github.com/Blackymas/NSPanel_HA_Blueprint/blob/main/esphome/nspanel_esphome_page_light.yaml).

Na página histórica também existe bloqueio de entrada: se api==0, o preinitialize retorna à página inicial. Para operar submenus sem HA, não basta substituir o envio do comando: é necessário revisar essa condição no TFT/perfil de operação, sem fingir saúde da API ou de transportes indisponíveis.

Contrato proposto para a integração: validar três inteiros 0..255, associar ao recurso do item que abriu o submenu e enviar comando semântico de cor pelo communication_net_protocol. Preservar brilho separadamente e atualizar a interface pelo estado retornado pelo executor. Validar fisicamente cores primárias, cor intermediária, região central, brilho independente, toque/arraste/soltar e indisponibilidade do HA antes de declarar compatibilidade do TFT instalado.

## Comparação de versões e base candidata — 06/10/2026

Foram comparados os exports US de light, cover, climate, boot e Program.s da tag 4.3.13 com main do Blackymas fixado no commit 05ecefde2bac5db8bc96c02148058518d78a06fb. A comparação oficial v2026041...main mostra somente README.md alterado em dois commits: os artefatos de tela/firmware examinados correspondem à base v2026041, commit 6ff0e4eb545bae515a65de7f782d2a4a76c5371d. O boot da tela declara 2026041.

| Aspecto | US 4.3.13 | US Blackymas 2026041 | Efeito na implementação |
|---|---|---|---|
| Eventos locais | Corpos JSON | Campos CSV | Adaptador serial deve corresponder ao TFT |
| Círculo de cor | RGB calculado na tela | Mesmo cálculo RGB; envio CSV | Preservar canais 0..255 e brilho separado |
| Brilho | Envio ao soltar | Envio ao soltar e atualização do texto ao mover | Não confundir atualização visual com envio contínuo de comando |
| Temperatura de cor | color_temp, faixa histórica 153..500 | color_temp_kelvin | Usar Kelvin e configurar limites reais da luz; export ainda tem faixa inicial histórica, substituída pela configuração do executor/blueprint |
| Persiana | JSON position/open_cover/close_cover/stop_cover | CSV equivalente e texto atualizado ao mover slider | Manter exclusivamente executor cover do HUB |
| Clima | Eventos JSON | Eventos CSV, atualização visual ao mover sliders, configuração de fonte dos botões | Validar limites, passo e unidades conforme entidade |
| Página atual | Mensagem customizada current_page | sendme | Receptor deve entender a notificação padrão Nextion |
| Serial | Configuração persistente de baud não forçada no Program.s antigo | bauds=115200 no programa | Coordenar baud da tela e ESP32 na migração |
| Dependência HA | Bloqueios de API nos submenus | Bloqueios permanecem em light/cover e condição API/embedded em climate | Versão nova não entrega autonomia automaticamente |

Fontes: [comparação oficial](https://github.com/Blackymas/NSPanel_HA_Blueprint/compare/v4.3.13...v2026041), [light US](https://github.com/Blackymas/NSPanel_HA_Blueprint/blob/v2026041/hmi/dev/nspanel_us_code/light.txt), [cover US](https://github.com/Blackymas/NSPanel_HA_Blueprint/blob/v2026041/hmi/dev/nspanel_us_code/cover.txt), [climate US](https://github.com/Blackymas/NSPanel_HA_Blueprint/blob/v2026041/hmi/dev/nspanel_us_code/climate.txt), [programa US](https://github.com/Blackymas/NSPanel_HA_Blueprint/blob/v2026041/hmi/dev/nspanel_us_code/Program.s.txt).

### Continuidade do projeto

O commit v2026041 do Blackymas registra a transição do desenvolvimento para NSPanel Easy. O sucessor possui release v2026.10.0, publicada em 01/10/2026, e guia de migração do Blackymas. Essa é uma base mais recente para avaliar, distinta da última base do repositório original.

NSPanel Easy v2026.10.0 exige ESPHome >=2026.8.0 e TFT >=21.6, conforme seu versionamento. Portanto não compila sem alteração usando o alvo 2026.7.3 consolidado no HUB/TX. Atualizar somente o compilador do projeto NSPanel não exige por si só atualizar os dispositivos HUB/TX, mas os componentes compartilhados precisam ser validados nessa nova versão.

O guia também muda localização para language no YAML, explicita migração da senha OTA e altera a semântica de nextion_update_url para override completo. Não reutilizar a URL local antiga de TFT sem fixar conteúdo e compatibilidade. O parser de luz do sucessor continua recebendo light,rgb_color,R,G,B. Seu boot também aguarda blueprint; é uma base para adaptar, não comprovação de autonomia.

Fontes: [release v2026.10.0](https://github.com/edwardtfn/NSPanel-Easy/releases/tag/v2026.10.0), [requisitos](https://github.com/edwardtfn/NSPanel-Easy/blob/v2026.10.0/esphome/nspanel_esphome_version.yaml), [migração](https://github.com/edwardtfn/NSPanel-Easy/blob/v2026.10.0/docs/migration_from_blackymas.md), [boot](https://github.com/edwardtfn/NSPanel-Easy/blob/v2026.10.0/esphome/nspanel_esphome_boot.yaml).

Correção da leitura inicial de releases: o rótulo latest/v1.0.1 Blueprint no Blackymas corresponde a publicação de 04/10/2022, segundo a API do GitHub; não é uma release nova de outubro de 2026 nem base recomendada. A ordenação visual não determina a versão técnica atual.

Direção recomendada: avaliar NSPanel Easy v2026.10.0 fixado, com compilador compatível e conjunto TFT/firmware coerente, antes de implementar o perfil autônomo. Se for necessário manter exatamente ESPHome 2026.7.3, usar Blackymas v2026041 como candidata sujeita a compilação/validação. Nenhuma das bases foi compilada ou instalada nesta análise; não houve comparação visual renderizada nem teste físico da migração.

## Ambientes de desenvolvimento

O usuário confirmou o ambiente ativo do TX fora do OneDrive: C:/Users/luizg/AppData/Local/esphome-dev/venv. O executável desse ambiente respondeu ESPHome 2026.7.3 na verificação local. A .venv antiga encontrada no OneDrive não representa o ambiente usado pelo TX.

Para NSPanel, o pacote de VS Code aponta para C:/Users/luizg/AppData/Local/esphome-nspanel-dev/venv, independente do TX, com ESPHome 2026.8.0 como primeira base candidata de compilação. O ambiente novo ainda não foi instalado. Prever build/cache fora do OneDrive no YAML futuro; a seleção de Python no VS Code não desloca o build automaticamente.

## Marco de compilação confirmado pelo usuário

O usuário confirmou a instalação/versão do ambiente NSPanel e, após receber nspanel-base-2026.10.0.zip, relatou que a base compilou. Considerar NSPanel Easy v2026.10.0 com ESPHome 2026.8.0 validado para compilação nesse ambiente. A referência anterior a ambiente ainda não instalado está superada por essa confirmação.

Essa validação é da base sem communication_net_protocol/ESP-NOW e sem perfil autônomo; não comprova compatibilidade dos componentes compartilhados nessa versão. Não há confirmação de OTA, upload TFT, migração de blueprint, teste físico ou funcionamento sem HA. HUB/TX continuam em sua frente posterior.

Próxima etapa: incorporar configuração local da tela, remover as esperas pelo blueprint do caminho essencial de inicialização e adaptar os submenus/roteamento. Preservar os controles existentes e preparar TFT US compatível; não instruir instalação isolada do firmware sobre o TFT antigo.

## Etapa: relés locais no firmware — 06/10/2026

Pacote nspanel-reles-locais: fork local de NSPanel Easy v2026.10.0. Os botões físicos curtos esquerdo/direito acionam os relés 1/2 por código local. O modo local é aplicado na inicialização e preservado quando o HA envia relay_settings. O evento HA de toque curto físico é suprimido para evitar execução dupla; toque longo permanece no fluxo original. API/Wi-Fi não provocam reinício por indisponibilidade.

Ainda não implementado: botões na tela com roteamento local, inicialização independente de blueprint, reaplicação do perfil após reinício do Nextion e comunicação com HUB. A proposta HMI permanece textual, sem TFT compilado. Esta etapa não autoriza concluir que toda a interface funciona sem rede.

## Protótipo de partida local — 06/10/2026

No pacote nspanel-boot-local, gian_local_profile=true seleciona um caminho de boot que não aguarda Wi-Fi/API/blueprint. Ele aguarda a configuração do display, a versão mínima real do TFT e o contrato Gian 1 emitido pelo HMI adaptado. A prontidão local é independente de blueprint_ready e api; nenhuma dessas condições é falsificada.

Nesta etapa a máscara da página Quarto Gian é 5: Central no slot 1 e Mesa no slot 3. Toque curto na tela alterna o relé local; toque longo abre o submenu switch e suas ações são consumidas localmente, sem encaminhar a mesma ação ao HA. Estado visual é atualizado após mudanças reais dos relés. Retorno e wake-up usam Quarto Gian. Reinício do Nextion invalida a prontidão e reaplica o perfil.

Os outros itens continuam no contrato completo de mapeamento, mas ficam ocultos neste protótipo enquanto o roteamento externo não estiver implementado. Ainda não há transporte MQTT/ESP-NOW para HUB, nem controle local de RGB, cover ou AC neste pacote. A preservação dos submenus completos continua requisito da migração final.

Entrega HMI: original US portrait inalterado mais cinco arquivos de código/eventos completos para aplicar no Nextion Editor; não foi gerado TFT. Validação: configuração ESPHome 2026.8.0 e execução das tarefas internas de geração do código C++; não equivale a compilação. A geração normal do projeto foi interrompida por WinError 5 na extração da dependência noise-c. Nenhum teste físico ou OTA foi realizado.

## Auditoria de rotas HUB — 06/10/2026

A fonte de fechamento CC2 contém 24 bindings em quatro recursos. Não há recurso individual FitaSuperior, comando parametrizado de posição da persiana, peer NSPanel ou roteamento de resultados por origem NSPanel. O MQTT do executor está configurado para a origem/resposta do TX. Logo, não basta acrescentar o emissor no firmware do painel.

As rotas HUB anotadas no perfil são objetivos, não disponibilidade confirmada do protocolo atual. Armario não foi identificado nos bindings. O nome light.spot_chuveiro_gian não prova equivalência com o SpotChuveiro binary do HUB. A entidade da fita superior deve continuar individual; não substituir pelo grupo superior/meio.

A análise completa e o catálogo extraído estão em outputs/rotas-nspanel. A atualização geral HUB/TX permanece adiada conforme a orientação anterior; foi apresentada a opção de antecipar apenas a inclusão mínima do NSPanel. Até definir essa opção, nenhum arquivo do HUB/TX foi alterado.


## Atualização: perfil declarativo e extensão do HUB (06/10/2026)

O pacote nspanel-hub-declarativo centraliza entidades, nomes e componentes de tela em packages/gian_profile.yaml. As novas lambdas do HUB apenas convertem RGB 0–255 e percentuais 0–100 para 0–1.

A extensão do HUB acrescenta comandos individuais para FitaSuperior (ligar, desligar, alternar, RGB, brilho e consulta), posição/consulta da persiana temporizada e alternância de SaidaLivre3. São 33 bindings ao combinar os 24 existentes com 9 novos; o snapshot legado continua com quatro campos. Os controles individuais não acionam FitaMeio.

Coordenação declarativa acrescentada: o recurso individual e o grupo superior/meio compartilham execution_scope light/alimentacao_fitas. Comandos simultâneos de recursos diferentes nesse escopo recebem BUSY. Ambos os desligamentos preservam a alimentação se outra fita estiver ligada. A validação de configuração e geração de código passou após essa alteração; compilação e teste físico continuam pendentes.

A tela continua disponibilizando somente Central/Mesa neste protótipo. Ainda faltam remetente/transporte do NSPanel e os submenus externos. Armário, Ar Gian e a identidade física do Chuveiro aguardam confirmação. AC permanece via SmartThinQ; IR adiado. Nenhum comando direto de relé da persiana foi reintroduzido.

Validação: 47 testes Python aprovados; configuração e tarefas de geração C++ do ESPHome 2026.8.0 aprovadas, incluindo os 33 bindings em fixture com hardware simulado. Não houve compilação C++, compilação TFT, instalação ou teste físico.


## Cadastro genérico: validação de associações

A implementação de desenvolvimento esphome-nspanel-interface separa recursos locais, executores remotos, itens de tela e gestos. Não converteu ainda o perfil Gian nem mudou os slots ativos do protótipo. Relés locais podem existir sem botão ou item associado. Alias de dispositivo e identidade de rede são campos separados quando necessário. As referências MQTT/ESP-NOW são verificadas contra o executor declarado.

Validação atual: 52 testes de configuração e geração de código com ambos os componentes. Sem envio real, integração Nextion, compilação C++ ou instalação. Nenhum arquivo novo deve ser copiado para o ESPHome da instalação nesta etapa.


## API compartilhada de envio em desenvolvimento

O código de admissão/coordenação está em esphome-communication-net-protocol, usando o runner existente. O cadastro genérico possui um adaptador condicional, ainda desativado. A operação pode declarar interrupt para usar a via reservada; a persiana continua sendo recurso temporizado do HUB e STOP não aciona relés diretamente. A identidade permanece executor + recurso, com transação por sessão.

Ainda não foram provisionados os adaptadores concretos nem integrados estados, recepção e Nextion. O perfil Gian e seus slots ativos não foram alterados. Passaram 52 testes Python do cadastro, 47 do protocolo e a geração de código com ambos os componentes; o novo teste nativo C++ não foi executado. O compilador ESP32 instalado falhou ao iniciar no ambiente com erro de DLL. Não houve publicação nem instalação, e não é necessário copiar arquivos.

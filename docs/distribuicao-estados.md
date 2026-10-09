# Distribuição ativa de estados sem Home Assistant

## Objetivo e separação de responsabilidades

A instalação precisa de publicação, observação, cache, disponibilidade e resincronização independentes de HA. O executor é a autoridade dos seus recursos: observa alterações reais do componente e publica estados, inclusive mudanças causadas por botão físico, outro controlador ou outra integração. NSPanels/TXs e outros consumidores declaram quais recursos/campos observam. A relação existe independentemente de uma transação de comando.

- Cadastro/UI: campos e sinalizadores, cache e apresentação, sem roteamento de rede.
- communication_net_protocol: publicações, assinaturas/interesses, versões, confirmação/consulta, políticas por consumidor e deduplicação entre transportes.
- Adaptadores MQTT/ESP-NOW: transporte e integração com a infraestrutura de cada meio.
- Eventual encaminhamento multi-hop: roteamento entre nós, separado do protocolo de recursos e da UI.

MQTT pode continuar funcionando com HA desligado: consumidores e publicadores utilizam diretamente o broker. Sem infraestrutura Wi-Fi/broker, pares ESP-NOW alcançáveis assumem o transporte. Sem caminho por nenhum transporte, declarar fonte indisponível/desatualizada, sem inventar estado.

## Três classes de mensagens

1. Comando e resposta correlacionada: solicitação de atuação, admissão, progresso e conclusão para a origem solicitante.
2. Publicação de estado: alteração observada no executor, destinada aos consumidores interessados, mesmo sem comando local.
3. Controle de observação: anúncio, sincronização/consulta, disponibilidade e, se forem usadas assinaturas dinâmicas, registro/renovação/expiração.

Não simular publicação contínua por repetidas execuções de toggle/on/off. Uma consulta de estado também não deve ficar presa na fila de uma atuação longa quando existe estado observável; o caminho de leitura/publicação precisa ser independente da fila normal de atuação.

## Mesmo contrato sobre ambos os transportes

A identidade de uma publicação inclui proprietário do recurso, recurso, geração/sessão, revisão e campos presentes. A mesma observação mantém a mesma versão ao circular por MQTT e ESP-NOW. O consumidor descarta duplicatas e revisões anteriores, preserva campos ausentes e só renova a validade de dados aceitos como atuais. Repetir pacote antigo não torna o estado novo.

Sessão/boot aleatório não é ordenação temporal: não escolher estado novo comparando numericamente IDs de boot. O protocolo deve definir geração persistente e revisão, ou estabelecer a sessão atual por sincronização autenticada, incluindo tratamento de perda de persistência. Metadados de completude devem distinguir snapshot integral e atualização parcial. Lacuna de deltas exige snapshot ou campos versionados de maneira que a recomposição seja segura.

Uma publicação de estado não volta a ser comando nem é republicada como se o observador fosse o proprietário. Pontes entre MQTT e ESP-NOW preservam origem/versão e suprimem ecos.

## Distribuição por consumidor

Cada executor mantém interesses autorizados por consumidor, recurso e campos. A configuração básica deve permitir interesses estáticos compilados, reconstruídos no boot sem HA/broker. Assinaturas dinâmicas, se acrescentadas, exigem validade e renovação e não podem ser o único mecanismo para reconstruir a instalação offline.

Cada consumidor possui destino/política e posição de sincronização próprios. Estado MQTT conectado no publicador não significa entrega ao consumidor. Para decisão de fallback, é preciso confirmação/heartbeat de consumo ou indicação confiável de preferência/disponibilidade do consumidor. Uma estratégia inicial alternativa é distribuição simultânea configurada para observadores específicos, com deduplicação; não prometer fallback seletivo sem a informação necessária.

Exemplo: HUB publica via MQTT para painéis A/B; A perdeu Wi-Fi, B continua conectado. A continua recebendo via ESP-NOW se o caminho estiver disponível. O HUB não corta o rádio apenas porque seu próprio MQTT está conectado. Ao A recuperar MQTT, o contrato de versão impede aplicar um retained antigo por cima do estado já recebido por rádio.

No ESP-NOW, o publicador pode distribuir diretamente aos pares interessados. Isso não requer todos os nós conectados a todos os outros. Limites de pares, filas, payload e taxa são validados por nó/topologia, considerando cerca de dez HUBs, dez NSPanels e dezenas de TXs. Priorizar comandos/interrupções; coalescer estados para a revisão mais recente por consumidor/recurso, com limites de taxa e retransmissão. Não coalescer comandos de atuação como se fossem estados.

## Chaveamento solicitado pelo consumidor

A estratégia inicial adotada é ativação explícita da publicação direta pelo consumidor. Ao iniciar sem MQTT ou detectar perda de conexão/recepção, ele envia por ESP-NOW uma consulta de sincronização com pedido de atualizações diretas. O responsável autentica o consumidor, verifica seus interesses autorizados, registra a assinatura e responde com snapshot e confirmação do registro. A assinatura é registrada antes da captura do snapshot para não perder alterações durante essa transição.

O registro é por consumidor e conjunto de recursos/campos autorizados, não uma flag global do dispositivo. Contém modo de entrega, sessão do consumidor, validade e última renovação. Identidades, recursos, tempos e políticas vêm da configuração declarativa. O pedido apenas ativa interesses permitidos; não cria permissão de comando nem adiciona arbitrariamente novos peers de rádio.

Enquanto a assinatura direta estiver ativa, alterações reais são enviadas ao consumidor por ESP-NOW, mesmo que o responsável continue publicando MQTT. O consumidor renova o registro periodicamente, com intervalo e validade configuráveis, confirmação e novas tentativas limitadas. Falha de renovação não confirma disponibilidade MQTT: o consumidor marca a fonte desatualizada quando necessário e continua tentando sincronizar. O responsável pode expirar a assinatura para limitar tráfego a consumidores ausentes.

Na recuperação, conexão ao broker isoladamente não encerra a assinatura direta. O consumidor restabelece as assinaturas MQTT e comprova recepção de estado atual pelo MQTT mediante sincronização correlacionada; retained antigo não basta. Só então solicita a desativação direta e aguarda confirmação. Durante a sobreposição, o cache deduplica pela identidade/versão da publicação. Pedidos de ativação, renovação e desativação são idempotentes e ordenados por sessão e sequência de controle: uma desativação atrasada não cancela uma ativação posterior.

Ao reiniciar, o consumidor reconstrói seus interesses da configuração e solicita novamente o transporte necessário. O responsável reiniciado anuncia sua nova sessão ou responde a consultas/renovações com registro e snapshot; a renovação também recria uma assinatura perdida. Não depender de persistir a flag dinâmica em flash. O cliente mantém consulta/renovação periódica mesmo quando o recurso não muda.

Essa estratégia pressupõe um peer autorizado alcançável diretamente. O registro de assinatura não muda canal de rádio nem cria encaminhamento mesh. O canal e a topologia continuam sujeitos à configuração do transporte existente. A implementação genérica deste controle e de sua integração com publicações permanece pendente.

## Recuperação sem rajadas

A consulta existente a state/snapshot deve ser preservada; o controle de assinatura será uma evolução versionada e compatível, sem reinterpretar silenciosamente consultas legadas como assinaturas. O push legado já possui intervalo mínimo, espera para consolidar alterações e uma operação em andamento. Esses limites locais não demonstram proteção contra recuperação simultânea da instalação.

Requisitos da nova distribuição:

- Espalhar a primeira consulta após boot, perda de MQTT e reconexão por atraso aleatório configurável; não consultar em cada iteração do loop ou evento repetido de desconexão.
- Manter apenas uma sincronização pendente por consumidor/responsável; consolidar recursos autorizados em pedidos limitados por tamanho. Renovação e consulta não abrem transações concorrentes equivalentes.
- Usar espera progressiva limitada com aleatoriedade nas novas tentativas, respeitando limite global por nó e por peer. Não reiniciar a espera a cada notificação de falha; zerar após recuperação confirmada. Aplicar também espaçamento às respostas e ao envio após registro.
- Usar filas de tamanho fixo e limite de trabalho por passagem do loop. Sob sobrecarga, consolidar estados mantendo a revisão mais recente por consumidor/recurso; nunca acumular histórico ilimitado nem coalescer comandos de atuação.
- Reservar capacidade para STOP, comandos e respostas correlacionadas. Estados e consultas de recuperação consomem orçamento limitado, com atendimento justo entre peers; prioridade não autoriza esgotar CPU ou rádio.
- Respostas de sobrecarga devem ser limitadas e, quando suportado, indicar espera antes de nova tentativa. ACKs, renovações e mensagens de erro também contam no orçamento; não criar rajadas de respostas a pacotes repetidos.
- Desduplicar consultas e publicações antes de processamento oneroso; limitar tentativas e manter dados desatualizados explicitamente quando não houver capacidade/caminho.
- Se houver encaminhamento futuro, deduplicar antes de retransmitir e limitar saltos, filas e tráfego por nó. Preferir unicast aos responsáveis conhecidos; não usar descoberta por broadcast ou inundação a cada falha.
- Expor contadores de consolidação, descarte, fila, timeout e tentativas limitadas, com logs também limitados. Tempos, capacidades e políticas são declarativos e validados; seus valores dependem de medição no hardware.

Aceitação adicional: queda e retorno simultâneos de broker/AP e reinício de vários nós, responsável ausente ou lento, perda elevada de pacotes e alterações contínuas de RGB/posição. Verificar filas/memória limitadas, ausência de watchdog, atendimento entre peers e latência de STOP/comandos sob carga. Não afirmar resistência a rajadas sem estes ensaios. Estes controles ainda não foram implementados/ensaiados na nova distribuição genérica.

## ESP-NOW e mesh

A API ESP-NOW básica fornece comunicação entre pares. O protocolo local inspecionado mantém pares, comandos/resultados e observadores; não foi encontrado roteamento multi-hop, TTL ou tabela de encaminhamento implementada nessa camada. Não presumir que mesh já exista porque vários equipamentos usam ESP-NOW.

Se os destinos estiverem ao alcance direto, publicação seletiva entre pares atende à distribuição. Se precisarmos atravessar nós intermediários, é necessária uma camada de encaminhamento sobre ESP-NOW, preservando origem e destinatário, limitando saltos e duplicatas, tratando rotas indisponíveis e autenticando a origem fim a fim. O reconhecimento do salto local não equivale à entrega ao consumidor final. Não substituir silenciosamente o rádio atual por ESP-WIFI-MESH, que é outra solução.

Referências oficiais: https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/api-reference/network/esp_now.html e https://github.com/espressif/esp-now/blob/master/src/espnow/src/espnow.c (o componente adicional da Espressif possui encaminhamento configurável; isso não está automaticamente presente na API básica nem no nosso componente).

## Resincronização e sinalizadores

Ao iniciar, restabelecer a fonte ou detectar lacuna, o consumidor solicita/recebe snapshot atual pelo transporte disponível. Retained MQTT é uma possibilidade de obtenção inicial, não confirmação de atualidade. Publicação por mudança mantém os sinais; confirmação periódica/heartbeat pode renovar disponibilidade sem redesenho redundante. Expiração e perda de fonte marcam os campos como desatualizados. Ao abrir página oculta, os sinais são reconstruídos do cache, sem depender de blueprint.

O consumidor pode enviar uma confirmação agregada por revisão/intervalo; não é obrigatório confirmar separadamente cada pixel/campo. A política deve manter a informação suficiente para decidir o transporte de cada consumidor, limitando tráfego. Autorizações de observar e de comandar são diferentes; interesse em um recurso não concede permissão de atuação.

## Limitações encontradas e compatibilidade

LightStateSnapshot atual possui quatro campos, push_peer_id único e try_push retorna sem enviar quando mqtt_connected é true. A capacidade e o formato atuais atendem ao fluxo legado HUB/TX, não ao barramento genérico multiobservador. Não ampliar seu snapshot indiscriminadamente: o receptor TX tem capacidade/formato correspondente.

Criar evolução versionada do canal de estados, preservando o snapshot legado enquanto houver consumidores antigos. A nova distribuição deve admitir recursos/campos genéricos, vários consumidores e decisão de entrega por consumidor. Não usar só respostas de comandos nem manter a supressão global do ESP-NOW quando o publicador tem MQTT.

O cadastro/cache de sinais já foi escrito; a nova distribuição ativa, assinaturas multiobservador e encaminhamento mesh não estão implementados. Não houve mudança de firmware/instalação nesta análise.

## Cenários de aceitação

- HA parado com broker disponível: atuação e sinalização continuam.
- Broker/AP indisponível: consumidores ao alcance recebem estado por ESP-NOW.
- HUB com MQTT e consumidor sem Wi-Fi: consumidor continua pelo rádio.
- Dois consumidores com preferências distintas e alterações por terceiro controlador.
- Reinício offline de publicador/consumidor com reconstrução de interesses e snapshot.
- Retained antigo chegando após atualização ESP-NOW, duplicatas e respostas fora de ordem.
- Perda de deltas, expiração, fonte indisponível e reconexão sem regressão visual.
- Sob carga, STOP/comandos não são bloqueados por atualizações de slider ou RGB.
- Se houver multi-hop: ausência de ciclos, limitação de saltos, autenticação de origem e entrega final diferenciada do ACK de salto.

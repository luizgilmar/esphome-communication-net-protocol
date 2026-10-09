# Interface de envio compartilhada: requisitos para implementação

A API C++ experimental agora existe em outbound_command_service.h, no repositório compartilhado de desenvolvimento. Ela ainda não é uma API ESPHome provisionada nem foi publicada. O runner de comunicação existente permanece responsável por tentativas, aceitação, prazos, fallback e respostas tardias. Os adaptadores atualmente pertencentes ao TX devem ser evoluídos/movidos no projeto compartilhado, com compatibilidade; não ser copiados para o NSPanel.

A entrada declarativa deve resolver: destino configurado, identidade do executor, recurso, comando, argumentos tipados e prazo. A identidade da origem e a sessão de boot pertencem ao serviço de comunicação. O serviço aloca a transação e devolve status de admissão e identificador de correlação. Não fornecer uma origem/boot nova a cada tentativa de transporte.

Admissão ao serviço, aceitação pelo executor e conclusão da atuação são eventos distintos. Recurso ocupado retorna BUSY ou segue fila explicitamente configurada; não substituir silenciosamente uma solicitação anterior pelo último toque. O STOP autorizado da persiana deve ter acesso à via de interrupção e não aguardar a operação que pretende interromper.

Uma transação mantém origem, sessão, alvo, comando e argumentos normalizados em MQTT e ESP-NOW. A resposta deve ser validada contra a transação/rota e associada ao executor + recurso, nunca ao último item tocado. Estado distribuído para vários controladores é separado da resposta à transação. Reinício invalida correlações anteriores segundo o contrato de sessão.

A recepção de comando para um relé local passa pelo executor de comunicação que já valida a origem e deduplica. Só após admissão chama o adaptador local. O estado lógico real da saída fornece a conclusão e a atualização de observadores. Não implementar recepção diretamente por automação MQTT que contorne essa validação.

Testes necessários antes de publicar: dois executores com mesmo endereço de recurso; duas origens autorizadas no mesmo executor; toggle repetido entre transportes executado uma vez; fallback após falta de aceitação com identidade preservada; resposta MQTT tardia; resposta de outro executor rejeitada; STOP entre origens autorizadas; controle e boot local sem rede; atualização de todas as referências visíveis ao mesmo recurso. Configuração e geração de código não substituem esses testes de execução.


## Código preparado nesta etapa

CommandSubmissionPort define a entrada; CommandSubmissionService implementa admissão e coordenação usando o runner existente. CommandDraft declara destino, alvo, comando, payload, prazo e tipo normal/interrupção. Submission devolve admissão/BUSY/inválido/indisponível e a correlação da transação. SubmissionObserver recebe progresso e conclusão com o alvo original.

O serviço exige duas registries normais e uma de interrupção, com adaptadores independentes. Cada adaptador precisa confirmar application_identity_matches para origem e sessão. A implementação padrão é false; adaptadores legados não ingressam na nova API até serem adaptados. Isso preserva os chamadores antigos e impede ativar fallback com identidade divergente. Sessão é configurada uma única vez; o serviço aloca IDs e nunca os troca durante uma tentativa de fallback. A correlação e o avanço de protocolos permanecem com o runner/adaptadores.

CommunicationAdapter no componente de recursos traduz operações genéricas para CommandDraft. Rejeita valor fracionário, negativo, maior que 65535 ou não finito; RGB é serializado como três bytes; texto não é convertido silenciosamente. A prioridade vem do campo declarativo interrupt da operação, sem procurar nomes de entidades. O adaptador é compilado somente quando USE_NSPANEL_COMMUNICATION_SUBMISSION for definido pela futura integração; não está ativado nos exemplos atuais.

Ainda faltam provisionar os adaptadores concretos MQTT/ESP-NOW no componente compartilhado, construir o serviço com armazenamento de longa duração, conectar a factory ao cadastro, integrar recepção/estados/Nextion e compilar/testar tudo. O teste C++ de serviço foi escrito, mas não compilado/executado. Os 52 testes Python do cadastro e 47 testes Python do protocolo passaram, além da geração de código ESPHome. Esses testes não confirmam o novo comportamento C++.

# Confirmacao de cor RGB — 19/10/2026

A confirmacao de bindings RGB estaticos e rgb_from_arguments usa canais normalizados do ESPHome, sem aplicar brilho ou gamma. LightColorValues::normalize_color da versao local 2026.9.1 normaliza pelo maior canal e transforma RGB todo zero em branco. Por isso um pedido (20,80,160) deve ser comparado com aproximadamente (32,128,255), e nao com os bytes originais.

rgb_completion.h aplica a mesma normalizacao ao valor esperado, aceita arredondamento de um byte e rejeita canais observados invalidos. Brilho continua sendo confirmado separadamente. A resposta de estado continua informando os canais efetivos normalizados; nao e alterada pelo criterio de conclusao.

Reproducao em bancada: oito timeouts de conclusao RGB no ensaio outbound-command, enquanto ligar/brilho/STOP funcionaram. A correcao e candidata; a nova compilacao e a confirmacao em hardware ainda estao pendentes. tests/rgb_completion_compile.h tem verificacoes C++ em tempo de compilacao e e incluido pelo publisher de bancada. Nao houve atualizacao de pins Git ou dispositivos instalados.

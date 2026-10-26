# Fire Emblem: The Sacred Stones — port nativo para navegador

Port em desenvolvimento em C/WebAssembly, sem emulador. Esta pasta contém o código da decompilação FE8U e as adaptações em `web/`. A base citada pelo README original é `laqieer/fireemblem8u`; o pacote local não contém o histórico Git nem um commit de origem verificável. O README original e avisos de ferramentas foram preservados. Não se atribui uma licença nova ao código ou aos recursos originais.

## Estado

Título, menu, controles, mapas, combate, fases, diálogos, retratos e áudio nativo estão integrados até o capítulo6, com validação específica em Normal/Difficult. A campanha contínua seed42 completa prólogo..6 nas duas dificuldades, na mesma instância WASM e sem reiniciar o RNG entre capítulos, até MNCH8 (capítulo7 ainda não integrado). O piloto usa comandos normais, incluindo Rescue de Eirika por Seth no capítulo6, e valida o checkpoint SRAM original com a equipe herdada em uma instância independente.

Esse percurso admite perdas: no capítulo6 morrem cinco aliados adicionais e os três civis em cada dificuldade; Eirika e Seth sobrevivem e o prêmio dos civis não é concedido. O cenário específico com todos os civis vivos continua coberto separadamente. A vitória contínua não significa campanha sem baixas, nem valida todas as estratégias/seeds ou todas as funcionalidades. Os testes anteriores de5x conservam as equipes originais e o retorno ao grupo de Eirika.
Névoa, Torch e cura inimiga com Heal/Mend/Recover estão integrados no capítulo6. Preparações livres, mapa-múndi completo e capítulo7 continuam pendentes. A implantação usa posições originais fixas para aliados existentes; animações/efeitos completos e tradução BR ainda não estão integrados. O início dos capítulos integrados pode ser gravado explicitamente no slot SRAM original; suspensão do turno atual continua pendente. Consulte `web/STATUS.md` para adaptações e evidências de validação. Ainda não é o jogo completo.

## Compilar localmente

São necessários Ubuntu/WSL, Python 3, Node.js, Emscripten e as dependências da compilação original descritas no README original. Disponibilize sua própria ROM americana em `baserom.gba` (SHA-1 `c25b145e37456171ada4b0d440bf88a19f4d509f`) e compile a base original para obter ELF/mapa usados pelos geradores.

```bash
bash web/build.sh
bash web/check_memory.sh
python3 -m http.server 8000 --bind 127.0.0.1 --directory web/dist
```

Abra `http://127.0.0.1:8000/`. O áudio precisa de um gesto no navegador. Não limpe o armazenamento local: os saves existentes ficam no navegador.

## O que fica local

ROMs, ELF/mapas, saves, `web/generated/`, `web/dist/` e `work/` não devem ser enviados. Os geradores regeneram dados e recursos a partir da ROM local validada. O repositório conserva fontes, geradores, testes e documentação; binários compilados contêm recursos originais e não são publicação automática. Os workflows herdados da decompilação não validam este port e devem ser revistos antes de habilitar CI/Pages.

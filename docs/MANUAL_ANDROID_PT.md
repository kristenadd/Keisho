# Manual de Configuração Android — Keisho

Emulador Sega Model 1, Model 2 e Model 3 para Android (`arm64-v8a`), com suporte a arranque direto pela interface do emulador ou integração com frontends (RetroFeVLC, Daijishō, ES-DE, Pegasus, LaunchBox).

---

## 1. Permissões e Estrutura de Pastas no Android

### Permissão de Armazenamento
Para ler pastas em `/storage/emulated/0/...` (ROMs, capas, bezels, `exclude.txt`), o Keisho pede a permissão **Acesso a todos os ficheiros** (`MANAGE_EXTERNAL_STORAGE`) no Android 11+ ou leitura/escrita em Android 6–10.

### Ficheiros de Configuração da Aplicação
Localizados por defeito em:
```text
/storage/emulated/0/Android/data/io.github.keisho_emu.keisho/files/
```
Principais ficheiros e subpastas gerados automaticamente:

| Caminho | Função |
|---|---|
| `keisho.ini` | Todas as definições de Vídeo, Áudio, Controlos, Pastas e Sistema |
| `filters.ini` | Listas editáveis de `Includes` e `Excludes` para Model 1, Model 2 e Model 3 |
| `nvram/` | Gravações internas das placas arcade (`.nv`, `.eeprom`, pontuações) |
| `nvram/states/` | Save states rápidos e slots numerados (`<jogo>.<slot>.kstate`) |
| `nvram/textures/<jogo>/` | Pastas `dump/` e `load/` para texturas personalizadas |
| `artwork/` | Cache automática caso o scraper online esteja ativo |
| `screenshots/` | Capturas de ecrã |

---

## 2. Pastas por Sistema (ROMs, Capas e Bezels)

No menu **Definições → Pastas** (ou **Sistema → Pastas** / `keisho.ini`), cada placa tem as suas próprias pastas independentes.  
Mesmo que aponte várias placas para uma pasta partilhada (por exemplo uma pasta MAME com 4000 ZIPs), **Model 1 só lista jogos Model 1**, **Model 2 só lista Model 2** e **Model 3 só lista Model 3**, e apenas para os ficheiros `.zip`/`.7z` realmente presentes na pasta.

### Exemplo prático no `keisho.ini` (`[paths]`):

```ini
[paths]
model1_rom_dir = /storage/emulated/0/RetroFeVLC/collections/Arcade/roms
model2_rom_dir = /storage/emulated/0/RetroFeVLC/collections/Sega Model 2/roms
model3_rom_dir = /storage/emulated/0/RetroFeVLC/collections/Sega Model 3/roms

model1_artwork_dir = /storage/emulated/0/RetroFeVLC/collections/Arcade/medium_artwork/artwork_front
model2_artwork_dir = /storage/emulated/0/RetroFeVLC/collections/Sega Model 2/medium_artwork/artwork_front
model3_artwork_dir = /storage/emulated/0/RetroFeVLC/collections/Sega Model 3/medium_artwork/artwork_front

model1_bezel_dir = /storage/emulated/0/RetroFeVLC/collections/Arcade/medium_artwork/bezel
model2_bezel_dir = /storage/emulated/0/RetroFeVLC/collections/Sega Model 2/medium_artwork/bezel
model3_bezel_dir = /storage/emulated/0/RetroFeVLC/collections/Sega Model 3/medium_artwork/bezel
```

### Regras de nomes de ficheiros para Capas e Bezels
- **Capas (`artwork_dir`):** imagem com o **mesmo nome exato** do ficheiro da ROM (`.png`, `.jpg`, `.jpeg`, `.bmp`, `.tga`).
  - Exemplo: `srally2.zip` → `srally2.png` (ou `.jpg`)
- **Bezels (`bezel_dir`):** requer **Vídeo → Bezel** ativado. Procura pela seguinte ordem:
  1. `<nome_da_rom>.png` (ex.: `daytona.png`)
  2. `<set_pai>.png` (ex.: para `doaa.zip`, procura `doaa.png` e depois `doa.png`)
  3. `default.png` (moldura predefinida daquele sistema)

### Requisitos especiais de Model 2 (`daytona`, `desert`, `vcop`)
Certifique-se de que tem os conjuntos de firmware da placa de I/O na pasta de ROMs de **Model 2** (do set MAME):
- `model1io.zip` (`epr-14869.25` / `epr-14869b.25` / `epr-14869c.25`) — necessário para **Daytona USA** e **Desert Tank**
- `model1io2.zip` (`epr-17181.6` / `epr-16891.6` / `epr-18021.6`) — necessário para **Virtua Cop**

---

## 3. Listas Editáveis: `filters.ini` (`Includes` e `Excludes`)

Na primeira execução, o Keisho cria automaticamente o ficheiro:
```text
/storage/emulated/0/Android/data/io.github.keisho_emu.keisho/files/filters.ini
```
Este ficheiro é criado **vazio por defeito**, pelo que nenhum jogo fica oculto inicialmente. Quando apontar um slot (ex.: Model 1) para uma pasta partilhada completa do MAME, pode editar o ficheiro para filtrar apenas os jogos desejados.

### Secções disponíveis no `filters.ini`
```ini
[Model 1 Includes]
; Vazio = mostra todos os jogos Model 1 encontrados na pasta (exceto os Excludes).
; Se escrever nomes aqui (um por linha), apenas esses serão mostrados.

[Model 1 Excludes]
; Exemplo: swaj

[Model 2 Includes]
; Exemplo: se quiser limitar Model 2 apenas a 3 jogos:
; daytona
; srallyc
; vcop

[Model 2 Excludes]
; Exemplo (útil para pastas MAME completas):
; model1io
; daytonagtx

[Model 3 Includes]

[Model 3 Excludes]
; Exemplo: srally2p
```

### Como funciona e dicas úteis
1. **Lista negra (`Excludes`):** escreva um nome de ROM por linha em `[Model X Excludes]` para ocultar clones, protótipos ou firmware I/O de pastas MAME; comente com `;` ou `#` (ou apague a linha) para voltar a mostrar.
2. **Lista branca (`Includes`):** se colocar qualquer nome dentro de `[Model X Includes]`, o emulador passa a mostrar **apenas** os jogos dessa lista para esse modelo (sobrepondo os `Excludes`).
3. **Compatibilidade com RetroFeVLC:** além do `filters.ini`, o Keisho também lê automaticamente `exclude.txt` e `include.txt` colocados dentro da pasta de ROMs ou na pasta principal da coleção (ex.: `Sega Model 3/exclude.txt`).

---

## 4. Vídeo: Vulkan, OpenGL ES e Software

No menu **Definições → Vídeo**:

| Opção | Descrição |
|---|---|
| **Renderer** (`graphics_backend`) | `vulkan` (opção predefinida recomendada para GPUs modernas como Adreno/Mali), `opengl` (OpenGL ES 3.1) ou `software` (CPU). Ao alterar nas definições e fechar o menu, o motor gráfico reinicia automaticamente já no novo renderer. |
| **Escala 3D** (`render_scale`) | `1x` (496×384 nativo) até `8x` (~4K). Aplicada nos renderers Vulkan e OpenGL ES. |
| **Formato** (`aspect_mode`) | `4:3`, `square` (píxeis quadrados), `16:9` (widescreen 3D real em Vulkan/GL) ou `stretch`. |
| **Bezel** (`bezel`) | Desenha a moldura do jogo usando a pasta de bezels do respetivo modelo. |
| **Contador FPS** (`show_fps`) | Mostra no ecrã a taxa em Hz/FPS e o renderer realmente ativo (ex.: `57.5 FPS [Vulkan]`). |

Exemplo de `[video]` no `keisho.ini`:
```ini
[video]
graphics_backend = vulkan
render_scale = 2
aspect_mode = 4:3
bezel = true
scaling_method = sharp
texture_filter = anisotropic
anisotropy = 4
show_fps = true
```

---

## 5. Comando Virtual no Ecrã vs Comando Físico (Bluetooth / USB)

### Modos do Comando Virtual (`virtual_gamepad`)
Em **Definições → Entrada → Comando → Comando virtual** (ou no `keisho.ini` / argumentos de frontend):

| Modo | Valor INI / CLI | Comportamento |
|---|---|---|
| **Automático** | `auto` | Ativa o comando tátil **apenas** quando não há comando físico nem volante ligado. Ao ligar um comando Bluetooth/USB, os botões táteis são ocultados automaticamente. |
| **Ligado** | `on` (`true`) | Mostra sempre os controlos no ecrã, mesmo com comando físico ligado. |
| **Desligado** | `off` (`false`) | Desativa totalmente o comando tátil e o HUD virtual (ideal para consolas portáteis Android ou uso exclusivo com comando físico + bezels). |

### Botões disponíveis no ecrã durante o jogo
- **Topo:** `FPS` | `SETTINGS/MENU` | **`SERVICE`** | **`TEST`** | `EXIT/SAIR`
  - **`TEST`** entra no menu de operador/serviço da placa (para ativar *Free Play*, calibrar direção/pistola, configurar dificuldade/região).
  - **`SERVICE`** navega pelas opções do menu de teste (nos jogos de 2 botões/pistola sem joystick de menu, use **`SERVICE`** para mover o cursor entre opções e **`TEST`** para confirmar/selecionar; durante o jogo adiciona créditos de serviço).
- **Centro inferior:** `COIN` | `SAVE` | `SLOT 1..4` | `LOAD` | `START`
- **Lateral direita adaptada por jogo:**
  - *Sega Rally 2*: `GAS`, `BRAKE`, `GEAR-`, `GEAR+` + **`CÂMARA` (`VIEW`)** e **`TRAV. MÃO` (`H.BRAKE`)**
  - *Sega Rally / ECA / Dirt Devils / Super GT 24h*: `GAS`, `BRAKE`, `GEAR-`, `GEAR+` + **`CÂMARA` (`VIEW`)**
  - *Daytona USA 1/2 / Scud Race / Le Mans 24 / Virtua Racing*: `GAS`, `BRAKE`, `GEAR-`, `GEAR+` + **`VR1`, `VR2`, `VR3`, `VR4`**
  - *Harley-Davidson*: `GAS`, `BRAKE`, `GEAR-`, `GEAR+` + **`MÚSICA`** e **`CÂMARA`**
  - *Motor Raid*: `GAS`, `BRAKE` + **`MURRO`** e **`PONTAPÉ`** *(sem seletor de mudanças fictício)*
  - *Jogos de Luta / Ação / Desporto / Pistola / Voos / Esqui*: etiquetas e disposições adaptadas à respetiva cabina — só os botões reais do jogo aparecem (*Virtual On*, *Virtua Fighter*, *SpikeOut*, *Virtua Striker*, *Hattrick Hero / Harikiri Pro Yakyuu*, *Desert Tank*, *Star Wars Trilogy*, *Wing War*, *Sega Bass Fishing*, *Top Skater*, *Ski Champ*, *Sega Water Ski*, *Virtua Cop*, *House of the Dead*, *Behind Enemy Lines*, etc.).

### Personalizar os botões visíveis no ecrã por jogo (`touch_hidden`) e inverter eixos do stick
Em **Definições → Entrada → Comando → Comando no ecrã**:
- Escolha o jogo em **Botões para** (*Predefinição* para todos os jogos, ou um título específico como `Daytona USA`, `Virtua Striker`, `Motor Raid`, `Star Wars Trilogy`, etc.).
- Use **Inverter stick** (`Desligado` / `Horizontal` / `Vertical` / `Ambos`) para inverter apenas o eixo esquerda/direita, apenas cima/baixo (ex.: manípulo de voo em *Star Wars*) ou ambos, tanto no stick virtual como em comandos físicos (`pad1_axis_aim_x_invert` / `pad1_axis_aim_y_invert` na secção `[<jogo>]`).
- Ative ou desative individualmente cada botão/analógico tátil ou barra (`COIN`, `START`, `GRAVAR / SLOT / ABRIR`, `SERVICE / TEST`, `FPS`).
- Use **Mostrar todos** para voltar a mostrar tudo rapidamente nesse perfil (`SETTINGS/MENU` e `EXIT/SAIR` ficam sempre visíveis para nunca perder o acesso ao menu).
- As preferências por jogo ficam guardadas automaticamente no `keisho.ini` na secção `[<jogo>]`.

### Jogos de Tiro / Pistola de Luz (`Virtua Cop 1/2`, `House of the Dead`, `Behind Enemy Lines`, `Gunblade NY`, `Rail Chase 2`, `The Lost World`, `LA Machineguns`, `Ocean Hunter`)
As definições de mira aplicam-se automaticamente a **todos** os títulos de pistola/tiro (Model 2 e Model 3), a partir dos metadados de `data/games.xml`.

Em **Definições → Entrada → Comando → Comando no ecrã** (e também em **Definições → Entrada → Pistola de luz**):

| Opção | Chave INI | Comportamento |
|---|---|---|
| **Mira tátil (pistola)** | `touch_gun_aim = direct` *(predefinido)* | **Toque direto:** toque ou arraste em qualquer ponto do ecrã para apontar e disparar sem ter o analógico no meio. Os cantos inferiores mantêm `FIRE` e `RELOAD` (ou `MSL` em *Behind Enemy Lines*). |
| **Mira tátil (pistola)** | `touch_gun_aim = stick` | **Analógico fixo:** coloca um analógico virtual fixo no canto inferior esquerdo para mover a mira suavemente sem tapar o ecrã com os dedos, e `FIRE` + `RELOAD`/`MSL` à direita. |
| **Sensibilidade do analógico (mira)** | `lightgun_stick_sensitivity` (`25`–`300`, predefinição `100`) | Ajusta a velocidade de deslocação da mira em modo **Analógico fixo** (ou comando físico), e a resposta da curva quando **Mira centrada no analógico** (`lightgun_pad_centred`) está ativo. |
| **Mira centrada no analógico** | `lightgun_pad_centred` | Desligado: o analógico move a mira como um rato. Ligado: a mira acompanha proporcionalmente a inclinação do analógico a partir do centro. |

> **Notas específicas para *Behind Enemy Lines* (`bel.zip`):**
> - A calibração de fábrica da EEPROM `93C46` (`MIN=00, DEL=FF`) é aplicada automaticamente quando o ficheiro `.eeprom` é criado ou está por calibrar, desbloqueando a mira em todo o ecrã.
> - O botão **`MSL`** (mísseis) envia o sinal de hardware `IN1 0x10`, mas o Nível 1 começa com **`MISSILE 0`** no contador superior esquerdo até apanhar caixas/recargas de mísseis durante a missão.

### Atalhos com Comando Físico (botão `Guide` / `Home`)
- `Guide + A` → Abrir Menu / Pausa (também acessível pelo botão **Voltar / Back** do Android)
- `Guide + L3` → **Service**
- `Guide + R3` → **Test**
- `Guide + X` / `Y` → Quick-Save / Quick-Load
- `Guide + Start` → Sair do jogo (regressa ao frontend se lançado externamente, ou à lista de jogos se aberto diretamente)

---

## 6. Integração com Frontends Android (RetroFeVLC, Daijishō, ES-DE, Pegasus)

Quando lançado por um frontend externo (com ROM passada via argumento/Intent), ao sair do jogo o Keisho **fecha diretamente** para regressar ao frontend. Quando aberto diretamente pelo ícone da app, regressa à lista de jogos.

### A. Opções de Linha de Comandos (CLI / Launchers baseados em argumentos)
| Flag | Valores | Descrição |
|---|---|---|
| `--virtual-gamepad <modo>` | `auto`, `on`, `off` | Define se o comando virtual aparece no ecrã |
| `--no-virtual-gamepad` | — | Atalho para `--virtual-gamepad off` |
| `--graphics-backend <nome>` | `vulkan`, `opengl`, `software` | Força o renderer apenas para esta execução |
| `--game <set>` | ex.: `srally2` | Lança um set específico (ou clone dentro de um ZIP merged) |
| `--config <pasta>` | caminho | Usa outra pasta para `keisho.ini` |
| `--filters <ficheiro>` | caminho | Usa um ficheiro `filters.ini` alternativo |
| `--model1-rom-dir <pasta>` | caminho | Substitui a pasta de ROMs Model 1 nesta sessão |
| `--model2-rom-dir <pasta>` | caminho | Substitui a pasta de ROMs Model 2 nesta sessão |
| `--model3-rom-dir <pasta>` | caminho | Substitui a pasta de ROMs Model 3 nesta sessão |
| `--render-scale <1..8>` | `1` a `8` | Escala interna 3D |

---

### B. Android Intents / Extras Suportados (`KeishoActivity`)

O `io.github.keisho_emu.keisho/.KeishoActivity` aceita:
- **ROM por Intent Extra:** `rom`, `ROM`, `rom_path`, `file`, `filepath` (caminho completo do `.zip`/`.7z`)
- **Nome de set por Extra:** `game` (ex.: `daytona`, `srally2`, usa a pasta do sistema no `keisho.ini`)
- **ROM por URI Data:** `file:///...`, caminho direto sem esquema ou `content://...`
- **Controlo virtual por Extra:** `virtual_gamepad`, `virtual-gamepad`, `touch_gamepad`, `touch_controls` ou `virtual_controller` (aceita texto `auto` / `on` / `off` ou booleano `true` / `false`)
- **Backend gráfico por Extra:** `graphics_backend`, `graphics-backend`, `backend` ou `renderer` (`vulkan` / `opengl` / `software`)
- **Argumentos extra livres:** `arguments`, `args` ou `cli_args` (ex.: `"--virtual-gamepad off --graphics-backend vulkan"`)

---

### Exemplos Prontos a Copiar

#### 1. RetroFeVLC Android (`RetroFeVLC/launchers/Sega Model 2.conf` e `Sega Model 3.conf`)
Com comando virtual automático (oculta sozinho quando há um comando físico ligado):
```ini
# Android Standalone Launcher: Keisho (Sega Model 1 / 2 / 3)
executable = /system/bin/am
arguments = start -n io.github.keisho_emu.keisho/.KeishoActivity -e rom "%ITEM_FILEPATH%" -e virtual_gamepad auto --activity-clear-task --activity-exclude-from-recents
```

Forçar comando virtual **desligado** e renderer **Vulkan** (ideal para telemóvel com comando Bluetooth fixo ou consola portátil):
```ini
# Android Standalone Launcher: Keisho (Sega Model 1 / 2 / 3)
executable = /system/bin/am
arguments = start -n io.github.keisho_emu.keisho/.KeishoActivity -e rom "%ITEM_FILEPATH%" -e virtual_gamepad off -e graphics_backend vulkan --activity-clear-task --activity-exclude-from-recents
```

#### 2. Daijishō (Custom Player / Am Start command)
Com `virtual_gamepad` desligado quando há comando configurado:
```text
am start -n io.github.keisho_emu.keisho/.KeishoActivity
  -a android.intent.action.VIEW
  -e rom "{file.path}"
  -e virtual_gamepad off
  -e graphics_backend vulkan
```
*(Ou `-e virtual_gamepad auto` para detetar automaticamente se existe um gamepad ligado.)*

#### 3. Pegasus Frontend (`metadata.pegasus.txt`)
```text
collection: Sega Model 3
shortname: model3
extensions: zip, 7z
launch: am start
  -n io.github.keisho_emu.keisho/.KeishoActivity
  -a android.intent.action.VIEW
  -d "{file.uri}"
  --es virtual_gamepad auto
  --es graphics_backend vulkan
```

#### 4. ES-DE (`es_systems.xml` no Android)
```xml
<command label="Keisho (Vulkan / Sem comando tátil)">
  am start -n io.github.keisho_emu.keisho/.KeishoActivity -a android.intent.action.VIEW -e rom "%ROM%" -e virtual_gamepad off -e graphics_backend vulkan
</command>
```

#### 5. Teste rápido por ADB (`am start`)
Lançar Sega Rally 2 com Vulkan e sem comando virtual:
```sh
adb shell am start -n io.github.keisho_emu.keisho/.KeishoActivity \
  -e rom "/storage/emulated/0/RetroFeVLC/collections/Sega Model 3/roms/srally2.zip" \
  -e virtual_gamepad off \
  -e graphics_backend vulkan \
  --activity-clear-task --activity-exclude-from-recents
```

Lançar Daytona USA com comando tátil ativo no modo automático:
```sh
adb shell am start -n io.github.keisho_emu.keisho/.KeishoActivity \
  -e rom "/storage/emulated/0/RetroFeVLC/collections/Sega Model 2/roms/daytona.zip" \
  -e virtual_gamepad auto \
  --activity-clear-task --activity-exclude-from-recents
```

---

## 7. Compilar o APK Android (`arm64-v8a`)

Consulte também o [README.md](../README.md) e o [BUILDING.md](../BUILDING.md) para detalhes completos de todas as plataformas.

### Requisitos
- **JDK 17+** (ou o JBR incluído no Android Studio)
- **Android SDK** (`compileSdk 35`, `minSdk 24`) com **NDK `29.0.14206865`**, **CMake 3.24+** e **Ninja**
- Submódulos inicializados: `git submodule update --init --recursive`

### Passos de compilação
1. Crie o ficheiro `android/local.properties` com o caminho do Android SDK:
   ```properties
   sdk.dir=C\:\\Users\\<utilizador>\\AppData\\Local\\Android\\Sdk
   ```
2. Compile o APK release (`Vulkan` + `OpenGL ES 3.1` + `software`, alinhado a 16 KB):
   ```powershell
   # Windows PowerShell
   $env:JAVA_HOME = 'C:\Program Files\Android\Android Studio\jbr'
   cd android
   .\gradlew.bat assembleRelease --console=plain
   ```
   ```sh
   # Linux / macOS
   cd android
   ./gradlew assembleRelease
   ```
3. O APK é gerado em `android/app/build/outputs/apk/release/app-release.apk`. Valide o alinhamento de 16 KB e instale via ADB:
   ```sh
   zipalign -c -v -P 16 4 android/app/build/outputs/apk/release/app-release.apk
   adb install -r android/app/build/outputs/apk/release/app-release.apk
   ```

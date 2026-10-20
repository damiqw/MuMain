# MuEmu (fork)

Ajustes para jugar en servidores basados en MuEmu sin cambiar el comportamiento del cliente
cuando no están configurados. El código propio vive en carpetas `MuEmu/`; en archivos de upstream
solo hay puntos de enganche mínimos, para poder seguir trayendo cambios de `upstream`.

## Cifrado (`ClientLibrary/MuEmu/`)

El GameServer MuEmu envuelve todo el stream TCP en una capa XOR por byte (`EncDecKey1/2`) y puede usar
sus propias claves SimpleModulus. Se configura en `config.ini`, junto a `Main.exe`:

```ini
[MuEmu]
Enabled = 1
; Claves de la capa XOR: se calculan igual que en el server y en Main.dll...
CustomerName = <CustomerName de GameServerInfo - Common.dat>
ServerSerial = <ServerSerial de GameServerInfo - Common.dat>
; ...o se indican directamente (decimal o 0x..):
; XorKey1 = 0x..
; XorKey2 = 0x..
; Segunda capa opcional (MHPServer.ini), valores finales:
; MhpXorKey1 = 0
; MhpXorKey2 = 0
; Claves SimpleModulus propias (contraparte de Hack\Dec1.dat y Hack\Enc2.dat del server).
; Si se omiten, se usan las claves por defecto.
EncryptKeyFile = Data\Enc1.dat
DecryptKeyFile = Data\Dec2.dat
```

Sin la sección `[MuEmu]` (o con `Enabled = 0`) se usa el cifrado original de OpenMU.

Punto de enganche en upstream: `ClientLibrary/ConnectionManager.cs` (`ConnectInner`).

## Items del servidor (`src/source/MuEmu/MuEmuTools.cpp`)

Este cliente no lee `Item.bmd`: usa `Data/Items/Group*.json`. Para usar los items del servidor
(incluidos los custom), copiar la carpeta `Data` del cliente MuEmu sobre la de este cliente y correr:

```
Main.exe --muemu-import-items
```

Convierte `Data/Local/<Eng|Por|Spn>/Item_<idioma>.bmd` a `Data/Items/*.json` y cierra. El tipo de arma se
completa desde el grupo. Los items con slots de equipo que este cliente no tiene (12, 13, 14...) quedan sin
slot (no equipables) y se listan en `MuError.log`.

### Modelos de los items custom

```
Main.exe --muemu-import-models="<cliente MuEmu>\MAIN_INFO\Custom"
```

Lee las tablas de GetMainInfo con modelo (`CustomItem`, `CustomJewel` seccion 0, `CustomPet` y `CustomFlag`) y
agrega o actualiza las entradas en `Data/Items/Models/Group*.json`, con las mismas carpetas que Main.dll:
`Data/Player` para los sets (grupos 7 a 11), `Data/Custom/CustomFlag` para las banderas y `Data/Item` para el
resto. Sin argumento usa `MAIN_INFO/Custom` junto a `Main.exe`.

Tambien aplica `CustomItemPosition.txt` (traslacion, rotacion y escala de inventario; escala en el piso)
sobre los modelos que existan, como lo hace Main.dll.

Punto de enganche en upstream: `WinMain` en `src/source/App/Platform/Windows/Winmain.cpp`.

## Opciones visuales

En `config.ini`:

```ini
[MuEmu]
; FOV vertical (grados) de la seleccion de personaje. 0 = el de upstream (~30). Los clientes MuEmu usan 35 o mas.
CharacterSceneFov = 45
; Barras de vida del servidor (paquete F3 E2), como Main.dll: monstruos con nombre y jugador seleccionado.
MonsterHpBar = 1
UserHpBar = 1
; Ventanas de la derecha (inventario, personaje, MU Helper...) centradas en vertical (0 = sobre el HUD, como upstream).
DockRightMiddle = 1

[UI]
; Escala de la interfaz: el fork permite desde 50% (upstream: 75%). 67% se parece a la proporcion de los
; clientes MuEmu en 1080p.
UIScalePercent = 67
```

## Tooltips custom (`src/source/MuEmu/MuEmuTooltip.cpp`)

Copiar `CustomItemToolTip.txt` y `CustomItemTooltipText.txt` de `MAIN_INFO/Custom` a `Data/MuEmu/`. El tooltip
del item muestra al final el titulo y las lineas (hasta 15) con sus colores, como Main.dll. Los colores 1 a 13
son los de Webzen; 14 es blanco y 15 a 24 se aproximan a los del cliente.

## Precios custom (`src/source/MuEmu/MuEmuItemPrice.cpp`)

Copiar `ItemValue.txt` de `MAIN_INFO/Custom` a `Data/MuEmu/`. Cambia el precio de compra y venta del NPC (la
primera linea que coincide por item, nivel, excelente, ancient y 380; `*` es cualquiera). Sin precio de venta se
usa la tercera parte del de compra, y los items apilables se multiplican por la cantidad, como Main.dll. Los items
que se pagan con WCoinC, WCoinP, Goblin Point o Ruud muestran esa moneda en el tooltip, sin impuesto.

## Joyas custom (`src/source/MuEmu/MuEmuJewel.cpp`)

Copiar `CustomJewel.txt` de `MAIN_INFO/Custom` a `Data/MuEmu/`. De la seccion 0 se usan el item de la joya, los
slots habilitados (armas, armaduras, alas y accesorios) y el SalePrice. Soltar la joya sobre un item permitido
envia el uso al servidor (que valida niveles, opciones y probabilidad), marca la celda en verde, y la joya se ve
en amarillo y con su precio en el NPC, como en Main.dll.

## Monstruos y NPC custom (`src/source/MuEmu/MuEmuMonster.cpp`)

Copiar `CustomMonster.txt` y `CustomNPCName.txt` de `MAIN_INFO/Custom` a `Data/MuEmu/` (los .bmd y texturas
quedan donde dice cada linea, por ejemplo `Data/Custom/Monster/`). Cada clase custom usa uno de los modelos de
monstruo que el cliente deja libres (183), con su nombre, tamano y tipo (0 NPC, 1 monstruo). CustomNPCName cambia
el nombre del NPC que aparece en ese mapa y posicion.

## Mapas custom (`src/source/MuEmu/MuEmuMap.cpp`)

Copiar `CustomMap.txt` a `Data/MuEmu/`. El mapa toma el nombre de su linea de texto si el cliente la tiene (si no,
el de la imagen: `mundioca.tga` es "Mundioca"), la imagen del nombre de `Local/<idioma>/ImgsMapName/` y la musica
de `Data/Music/` (solo si el archivo esta).

## Lista de servidores (`src/source/MuEmu/MuEmuServerInfo.cpp`)

Copiar `CustomServerInfo.txt` a `Data/MuEmu/`. El servidor N del grupo indicado muestra el texto custom en lugar
de "Grupo-N" y su estado, como Main.dll.

## Buffs custom (`src/source/MuEmu/MuEmuBuff.cpp`)

Copiar `CustomBuffIcon.txt` a `Data/MuEmu/`. Sus lineas reemplazan o agregan buffs sobre `BuffEffect` (el cliente
acepta buffs hasta el 255). Los buffs desde el 205 usan como icono el de la segunda columna. Los `%d` de la
descripcion se llenan con los valores del paquete C1 FB 13 del servidor (si no llego, se ve `?`).

## Niveles de eventos (`src/source/MuEmu/MuEmuEventLevel.cpp`)

Copiar `CustomEventEntryLevel.txt` a `Data/MuEmu/`. Cambia los niveles de entrada que muestra y valida el cliente
para Blood Castle, Devil Square, Chaos Castle, Kalima e Illusion Temple (columnas comunes y de MG/DL/RF).

## Efectos visuales (`src/source/MuEmu/MuEmuEffect.cpp`)

Copiar `CustomEffectStatic.txt`, `CustomModelEffect.txt` y `CustomRuneEffect.txt` a `Data/MuEmu/`:

- `CustomEffectStatic`: sprites sobre los huesos del item (alas, armas...); el item deja de dibujar los suyos.
- `CustomModelEffect`: efectos sobre el personaje que lleva el casco, la armadura, los pantalones o las botas con
  el nivel y el excelente pedidos (los guantes no tienen efecto, como en Main.dll).
- `CustomRuneEffect`: runa en el piso segun el VIP (o PK nivel 6) que manda el servidor en C1 F3 EF.

Las texturas usan los numeros de 1.04E y se pasan a los del cliente con `MuEmu::Texture` (ver "Numeros de
textura"). Los efectos de skill con modelos (numeros menores) se omiten porque en este cliente tienen otros numeros.
Los efectos de las mascotas custom estan en "Efectos de mascotas custom".

## Brillo de items custom (`src/source/MuEmu/MuEmuItemGlow.cpp`)

Copiar `CustomItem.txt` y `CustomFlag.txt` a `Data/MuEmu/`. Sus columnas R G B (0 a 255) son el color del brillo
de los items +7 en adelante, como GetCustomItemColor de Main.dll.

## Mascotas custom (`src/source/MuEmu/MuEmuPet.cpp`)

Copiar `CustomPet.txt` a `Data/MuEmu/` (los modelos se registran con `--muemu-import-models`). El servidor manda la
mascota en el slot de helper de la apariencia extendida; segun la columna Type, como Pet.cpp y PetMontura.cpp de
Main.dll:

- 6 (Dark Horse), 5 (Fenrir), 7 (Uniria) y 11 (Dinorant): el personaje la monta. Se mueve con las animaciones de la
  montura original y el jugador usa las de esa montura (para el cliente el helper es el original: el caballo
  custom habilita el Earthquake, como en Main.dll). Se oculta en zona segura, igual que la original.
- 8 (Muun) y el resto: camina detras del personaje (a 150 de distancia, corre mas rapido si se aleja y salta a su
  lado a 1000), con las acciones 0 (quieta), 1 y 2 (corriendo) del modelo.

La escala es la columna View (+0.3 en la seleccion de personaje). La columna Fenrir no se usa.

Puntos de enganche en upstream: `CreateMount` de las mascotas en `ReadEquipmentExtended` y `SetCharacterClass`
(`Engine/Object/ZzzCharacter.cpp`) y en `ReceiveChangePlayer` (`Network/Server/WSclient.cpp`); movimiento y escala
en `MoveMount` y `RenderMount` (`Engine/AI/GOBoid.cpp`).

## Efectos de mascotas custom (`src/source/MuEmu/MuEmuPetEffect.cpp`)

Copiar `CustomPetEffectStatic.txt` y `CustomPetGlow.txt` a `Data/MuEmu/`. Se dibujan sobre la montura (o la mascota
que camina) despues de su modelo, como CustomPetEffect.cpp de Main.dll:

- `CustomPetEffectStatic`: sprites sobre los huesos (mismas columnas que `CustomEffectStatic`: item, textura, hueso,
  nivel, R G B, tamano, rotacion y subtipo).
- `CustomPetGlow`: la malla indicada se dibuja otra vez (dos veces, como Main.dll) con el tipo de render de 1.04E
  (los mismos flags `RENDER_*` del cliente: 2 textura, 68 brillo + cromo, 70 brillo + cromo + textura) y el color
  R G B de 0 a 255.

### Numeros de textura (`src/source/MuEmu/MuEmuTexture.cpp`)

Los numeros de textura de 1.04E coinciden con los del cliente solo hasta 32274. Desde ahi se corren (por ejemplo
`Effect\ShockWave.jpg` es 32279 en 1.04E y `BITMAP_SHOCK_WAVE` = 32278 aca; `Effect\lightmarks.jpg` es 32380 y
`BITMAP_LIGHT_MARKS` = 32356). `MuEmu::Texture::From104E` usa una tabla generada comparando el archivo que carga cada
numero en `main.exe` 1.04E con los `LoadBitmap` del cliente; los numeros que el cliente no tiene se omiten.

Punto de enganche en upstream: `RenderMount` (`Engine/AI/GOBoid.cpp`).

## Earthquake de caballos custom (`src/source/MuEmu/MuEmuPetEffect.cpp`)

Copiar `CustomEarthQuakeSkill.txt` a `Data/MuEmu/`. Cuando el caballo custom (tipo 6) hace el Earthquake, en lugar de
los efectos del Dark Horse se usan los de su linea, como CustomEarthQuakeSkill.cpp de Main.dll:

- `EffectCode`: la onda que sale cada 400 ms, con el color R G B si `UseRGB` es 1 (Main.dll pasa los valores de 0 a
  255 sin dividir, asi que cada canal queda en 0 o 1) y `EffectSize` como subtipo.
- `Skill1`: el efecto del centro al final del golpe; `Skill2`: las piedras alrededor (por ejemplo 228 hielo y
  229 fuego). Los modelos de skill de 1.04E coinciden con los del cliente entre 226 (Dark Horse) y 332 (piedras);
  los demas y el 0 se omiten.

Como en Main.dll, un caballo custom sin linea no dibuja efectos del Earthquake, y la linea del Dark Horse original
(13,4) no cambia nada.

Punto de enganche en upstream: `RenderDarkHorseSkill` (`Engine/AI/GOBoid.cpp`).

## Ventanas custom del servidor (`src/source/MuEmu/MuEmuUi.cpp` y `MuEmuPanel.cpp`)

Una ventana RmlUi compartida (`muemu_panel.rml`, arrastrable) para las funciones custom de Main.dll, con sus
mismas teclas: **N** party (publicar, quitar publicacion y buscar), **H** horarios de eventos y **J** banco de
joyas (reemplaza la lista de comandos de SVEN). El resto sale del boton **Menu** de arriba de la pantalla
(`muemu_menu_button.rml`): Event Timer, Jewel Bank, Commands Info, Daily Reward, Custom Ranking, invasiones y
medidor de dano. El banco de joyas muestra las 30 joyas de a 10 por pagina. El Storage muestra "Storage [2/20]" y
flechas para cambiar de baul cuando el servidor manda los baules (C1 FB D5). Copiar `CustomEventTimeText.txt` y `CustomCommandInfo.txt` de
`MAIN_INFO/Common` a `Data/MuEmu/`.

| Ventana | Paquetes | Archivo |
|---|---|---|
| Eventos (cuenta regresiva y bonus) | pide C1 F3 E8, recibe C2 F3 E8 | `MuEmuEvents.cpp` |
| Comandos | `CustomCommandInfo.txt` | `MuEmuEvents.cpp` |
| Banco de joyas (depositar y retirar 1/10/20/30) | C1 F3 F4/F5/F6 | `MuEmuBank.cpp` |
| Cambiar baul (flechas del Storage) | recibe C1 FB D5, envia C1 F3 F7 | `MuEmuBank.cpp`, `StorageInventory.cpp` |
| Rankings (anterior/siguiente) | C1 F3 E7, C1/C2 F3 E6 | `MuEmuRanking.cpp` |
| Rango sobre los jugadores (nivel, resets y VIP del seleccionado) | C1 F3 E5 | `MuEmuRanking.cpp` |
| Medidor de dano (`[MuEmu] DpsMeter`, 1 por defecto) | C1 F3 F7 | `MuEmuRanking.cpp` |
| Recompensa diaria (ver y reclamar) | C1 FF 02, C1 FB 03/04 | `MuEmuRewards.cpp` |
| Invasiones (y monstruos de la activa) | C2 FA F8/F9, C1 D3 4C | `MuEmuRewards.cpp` |
| Party: publicar para todas las clases (FF 04), buscar y unirse sin clave | C1 FF 04/05/07, C2 F3 F0 | `MuEmuRewards.cpp` |
| Recompensas de guild (misiones, donar, cofre, reclamar) | C2 FF 30/32, C1 FF 31/33 | `MuEmuGuildRewards.cpp` |

Las posiciones de los datos salen de las estructuras del GameServer (empaquetado por defecto de MSVC). Falta:
elegir clases, nivel y clave al publicar, y unirse a partys con clave.

Los NPC no tienen barra de vida y su nombre se ve siempre (`[MuEmu] NpcNames`, 1 por defecto). Las mascotas de
`CustomPet.txt` toman la escala de inventario y de piso de sus columnas ScaleInv y ScaleDrop al importar los modelos.

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

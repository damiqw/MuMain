# Lo que falta portar de Main.dll al cliente SVEN

Inventario de los modulos de `Main_EX603/Main` (el Main.dll del proyecto) comparado con lo que ya tiene el fork
(`feature/muemu`) y con lo que SVEN ya trae de fabrica. La prioridad es una propuesta: **A** afecta el juego de
todos los dias o lo pide el servidor, **B** es contenido custom que usan los jugadores, **C** es cosmetico o ya
lo resuelve SVEN de otra forma.

## Ya portado

| Main.dll | En el fork |
|---|---|
| Item.txt, CustomItem, CustomJewel (modelos), CustomPet, CustomFlag, CustomItemPosition | importadores `--muemu-import-items/models` |
| ItemTooltip / CustomItemToolTip | `MuEmuTooltip` |
| ItemShopValue (ItemValue.txt) | `MuEmuItemPrice` |
| CustomJewel (aplicar joyas) | `MuEmuJewel` |
| CustomMonster, CustomNpcName | `MuEmuMonster` |
| CustomMap, CustomServerInfo, CustomBuffIcon, EventEntryLevel | `MuEmuMap`, `MuEmuServerInfo`, `MuEmuBuff`, `MuEmuEventLevel` |
| StaticEffect, CustomModelEffect, RuneEffect, CustomItem (brillo) | `MuEmuEffect`, `MuEmuItemGlow` |
| CustomPet, PetMontura, CustomPetEffect, CustomPetGlow, CustomEarthQuakeSkill | sesion de mascotas (montura y efectos) |
| HealthBar | `MuEmuHealthBar` (y nombres siempre visibles) |
| CustomEventTime (H), CustomCommandInfo, CustomJewelBank (J), Warehouse (flechas) | `MuEmuEvents`, `MuEmuBank` |
| CustomRanking, CustomRankUser, DpsMeter | `MuEmuRanking` |
| DailyReward, GuildRewardsUI, EventInvasion, PartySearch (N) | `MuEmuRewards`, `MuEmuGuildRewards` |
| Boton Menu (MENU_NEW) | `MuEmuMenuButton` |
| MuHelper (offhelper) | `MuEmuOffHelper` |
| LoginBox (conexion rapida) | `MuEmuQuickConnect` |
| Stats de mas de 65.000, OffTrade (tienda offline), ver equipo (ranking), post_item | `MuEmuStats`, `MyShopInventory`, `MuEmuRanking`, `MuEmuPostItem` |
| HWID (limites y baneos por PC), set ancient completo, cartas con equipo | `MuEmuHwid` + GameServer (`HwidManager`, `SvenProtocol`) |
| BuyVip, Antilag, ShowInfoItemDrop (tooltip del item en el piso) | `MuEmuVip`, `MuEmuAntiLag`, `NameWindow` |
| Lucky Wheel V2 (VongQuay), SPK_xShop, ThueFlag (alquiler de banderas) | `MuEmuShops` |
| NPCRuud, CustomItemColor (armaduras), CustomStatsAdvance | `MuEmuExtras` |
| CustomPing, CustomQuickAddStats, CreateAccount | `MuEmuMisc` (+ GameServer: version SVEN al crear cuenta) |
| PartyBuffs, PartyAssignHelper, Minimap/MiniMapInfo (lugares y party del servidor) | `MuEmuParty` |
| WingInvisible | `MuEmuAntiLag` (otros jugadores y sus alas) |

## Prioridad A: juego diario o pedido por el servidor

| Funcion | Main.dll | Paquetes | Nota |
|---|---|---|---|
| Slots de equipo custom (aros, 12-14) | NewUIMyInventory | C1 A9 | Los items se importan sin slot: no se pueden equipar. Hoy no se usan en el servidor. |

## Prioridad B: contenido custom

| Funcion | Main.dll | Paquetes | Nota |
|---|---|---|---|

No se portan porque el servidor actual no los usa: PKNotice (el servidor nunca manda el aviso tipo 0xD4) y ItemSmoke
(CustomSmokeEffect vacio).

## Prioridad C: cosmetico o SVEN ya lo hace

| Main.dll | Por que |
|---|---|
| CustomInterface, InterfaceS8AdvancedUI, GCCharacterS13, SceneSystemS8, CUiDIV, CustomDinhox, CChatEx, ChatExpanded, InterfaceElemental, CNewUIWindowsClient, CBNewUiEx | Pieles de interfaz (S8/S13) del cliente viejo; SVEN tiene su propia interfaz RmlUi y temas. |
| WideScreen, Resolution, CustomFont, Camera, CameraMove, SmoothCamera | SVEN ya maneja resolucion, escala, fuentes y camara. |
| Reconnect | SVEN tiene reconexion (`ReconnectManager`). |
| QuickCommand | SVEN tiene su menu de comandos rapidos. |
| TrayMode, discord | Extras de escritorio; se pueden sumar al final. |
| FixAttack, AtlansAbbys, ItemMove, Performance, BAiFindPath, CTimCheck | Arreglos de bugs y rendimiento del main.exe 1.04E: no aplican. |
| AntiVM, HackCheck, Protect | Protecciones del cliente viejo; el anticheat de SVEN seria otro trabajo. |

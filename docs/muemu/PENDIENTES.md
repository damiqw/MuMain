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

## Prioridad A: juego diario o pedido por el servidor

| Funcion | Main.dll | Paquetes | Nota |
|---|---|---|---|
| Slots de equipo custom (aros, 12-14) | NewUIMyInventory | C1 A9 | Los items se importan sin slot: no se pueden equipar. Hoy no se usan en el servidor. |
| HWID | HWID | login | El servidor no lo recibe del cliente SVEN (limites por PC). |
| Bandera de set ancient completo | (protocolo) | apariencia | No se manda: el brillo del set completo no se ve. |
| Cartas (letters) | (protocolo) | OpenLetterExtended | Muestran la clase sin equipo. |

## Prioridad B: contenido custom

| Funcion | Main.dll | Paquetes | Nota |
|---|---|---|---|
| Comprar VIP | BuyVip | F3 FB (lista), envio propio | Estaba en el menu original (Buy Vip). |
| Ruleta (Lucky Wheel) | Interface (VongQuay) | CustomVongQuay | Ventana con premios y giro. |
| Tienda XShop | SPK_xShop | D5 12/13 | Tienda custom por monedas. |
| Alquiler de banderas | ThueFlag | FF 03 | Elegir y comprar bandera (las banderas ya se ven). |
| Tienda Ruud | NPCRuud | EA, EE, 0x31 | Ventana del NPC Ruud. |
| Color de items (tienda de tintes) | CustomItemColor | EF 10, NpcOpen/Answer/ViewportSync | Teñir items y ver los de otros. |
| Stats avanzados | CustomStatsAdvance | F3 F4 | Panel con los stats finales del personaje. |
| Sumar puntos rapido | CustomQuickAddStats | (comandos) | Ventanas para sumar N puntos a cada stat. |
| Party: ayuda de asignacion | PartyAssignHelper | FF 38 | Marca el objetivo del lider. |
| Buffs de la party | PartyBuffs | 2E | Iconos de buffs de los miembros. |
| Minimapa con datos del servidor | Minimap, MiniMapInfo | pide F3 37, recibe info y party | SVEN tiene minimapa; faltan NPC/party del servidor. |
| Ping en pantalla | CustomPing | F3 F1 | Mostrar el ping. |
| Crear cuenta en el login | CreateAccount | F1 12 | Registro desde el cliente. |
| Panel AntiLag | Antilag | (cliente) | Apagar efectos para PCs lentas (estaba en el menu). |
| Nombres de items en el piso | ShowInfoItemDrop | (cliente) | Mostrar siempre el nombre de lo que cae. |
| Aviso de PK | PKNotice | (cliente) | Cartel del sistema PK. |
| Ocultar alas | WingInvisible | (cliente) | Opcion para no dibujar alas. |
| Humo de items | ItemSmoke (CustomSmokeEffect) | (cliente) | El archivo esta vacio en el servidor actual. |

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

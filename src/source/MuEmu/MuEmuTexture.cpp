#include "stdafx.h"
#include "MuEmu/MuEmuTexture.h"

#include <algorithm>
#include <iterator>

namespace
{
// Ultima textura con el mismo numero en los dos clientes.
constexpr int LastSharedTexture = 32274;

struct TextureMap
{
    int id104E;
    int texture;
};

// Generada comparando el archivo que carga cada numero en main.exe 1.04E con los LoadBitmap de este cliente.
// Los numeros de 1.04E que no estan aca son texturas que este cliente no tiene.
constexpr TextureMap Textures[] = {
    { 32275, BITMAP_BLUECHROME }, // item/nfm03.jpg
    { 32276, BITMAP_PIERCING }, // effect/piercing.jpg
    { 32277, BITMAP_MAGIC_EMBLEM }, // effect/magic_b.jpg
    { 32278, BITMAP_DARK_LOAD_SKIRT }, // player/kaa.tga
    { 32279, BITMAP_SHOCK_WAVE }, // effect/shockwave.jpg
    { 32280, BITMAP_DAMAGE_01_MONO }, // effect/damage01mono.jpg
    { 32281, BITMAP_SWORD_EFFECT_MONO }, // effect/swordeff_mono.jpg
    { 32282, BITMAP_FLAMESTANI }, // effect/flamestani.jpg
    { 32283, BITMAP_TRUE_FIRE }, // effect/fantaf.jpg
    { 32284, BITMAP_TRUE_BLUE }, // effect/fantab.jpg
    { 32285, BITMAP_JOINT_SPIRIT2 }, // effect/jointspirit02.jpg
    { 32286, BITMAP_PINK_WAVE }, // skill/pinkwave.jpg
    { 32287, BITMAP_WATERFALL_1 }, // effect/waterfall1.jpg
    { 32288, BITMAP_WATERFALL_2 }, // effect/waterfall2.jpg
    { 32289, BITMAP_WATERFALL_3 }, // effect/waterfall3.jpg
    { 32290, BITMAP_WATERFALL_4 }, // effect/waterfall4.jpg
    { 32291, BITMAP_WATERFALL_5 }, // effect/waterfall5.jpg
    { 32292, BITMAP_IMPACT }, // effect/impack03.jpg
    { 32293, BITMAP_EFFECT }, // logo/chasellight.jpg
    { 32296, BITMAP_BERSERK_EFFECT }, // monster/buserbody_r.jpg
    { 32297, BITMAP_BERSERK_WP_EFFECT }, // monster/busersword_r.jpg
    { 32298, BITMAP_TWINTAIL_EFFECT }, // monster/illumi_r.jpg
    { 32299, BITMAP_PRSONA_EFFECT }, // monster/prsona_r.jpg
    { 32300, BITMAP_PRSONA_EFFECT2 }, // monster/prsonass_r.jpg
    { 32302, BITMAP_KANTURU_2ND_EFFECT1 }, // object39/k_effect_01.jpg
    { 32303, BITMAP_KANTURU_2ND_NPC1 }, // npc/khs_kan2gate001.jpg
    { 32304, BITMAP_KANTURU_2ND_NPC2 }, // npc/khs_kan2gate003.jpg
    { 32305, BITMAP_KANTURU_2ND_NPC3 }, // npc/khs_kan2gate004.jpg
    { 32306, BITMAP_BLADEHUNTER_EFFECT }, // monster/bladeeff2_r.jpg
    { 32307, BITMAP_JACK_1 }, // skill/jack04.jpg
    { 32308, BITMAP_JACK_2 }, // skill/jack05.jpg
    { 32309, BITMAP_SNOW_EFFECT_1 }, // effect/snowseff01.jpg
    { 32310, BITMAP_SNOW_EFFECT_2 }, // effect/snowseff02.jpg
    { 32311, BITMAP_GM_HAIR_1 }, // skill/younghtest1.tga
    { 32312, BITMAP_GM_HAIR_3 }, // skill/younghtest3.tga
    { 32313, BITMAP_GM_AURORA }, // skill/gmmzine.jpg
    { 32314, BITMAP_FREETICKET_R }, // item/partcharge1/entrance_r.jpg
    { 32315, BITMAP_CHAOSCARD_R }, // item/partcharge1/juju_r.jpg
    { 32316, BITMAP_RAREITEM1_R }, // item/partcharge1/expensiveitem01_r.jpg
    { 32317, BITMAP_RAREITEM2_R }, // item/partcharge1/expensiveitem02a_r.jpg
    { 32318, BITMAP_RAREITEM3_R }, // item/partcharge1/expensiveitem02b_r.jpg
    { 32319, BITMAP_RAREITEM4_R }, // item/partcharge1/expensiveitem03a_r.jpg
    { 32320, BITMAP_RAREITEM5_R }, // item/partcharge1/expensiveitem03b_r.jpg
    { 32321, BITMAP_LUCKY_CHARM_EFFECT53 }, // item/partcharge1/bujuck01alpa.jpg
    { 32322, BITMAP_LUCKY_SEAL_EFFECT43 }, // item/partcharge1/monmark01a.jpg
    { 32323, BITMAP_LUCKY_SEAL_EFFECT44 }, // item/partcharge1/monmark02a.jpg
    { 32324, BITMAP_LUCKY_SEAL_EFFECT45 }, // item/partcharge1/monmark03a.jpg
    { 32325, BITMAP_LUCKY_SEAL_EFFECT }, // effect/partcharge1/bujuckline.jpg
    { 32326, BITMAP_CHARACTERCARD_R }, // item/partcharge3/alicecard_r.tga
    { 32327, BITMAP_CHARACTERCARD_R_MA }, // item/ingameshop/kacama_r.jpg
    { 32328, BITMAP_CHARACTERCARD_R_DA }, // item/ingameshop/kacada_r.jpg
    { 32329, BITMAP_CHARACTERCARD_R_MA }, // item/ingameshop/kacama_r.jpg
    { 32352, BITMAP_NEWCHAOSCARD_GOLD_R }, // item/partcharge3/jujug_r.jpg
    { 32353, BITMAP_NEWCHAOSCARD_RARE_R }, // item/partcharge3/jujul_r.jpg
    { 32354, BITMAP_NEWCHAOSCARD_MINI_R }, // item/partcharge3/jujum_r.jpg
    { 32355, BITMAP_CHERRYBLOSSOM_EVENT_PETAL }, // effect/cherryblossom/sakuras01.jpg
    { 32356, BITMAP_CHERRYBLOSSOM_EVENT_FLOWER }, // effect/cherryblossom/sakuras02.jpg
    { 32357, BITMAP_MAYA_BODY }, // object40/maya01_r.jpg
    { 32358, BITMAP_EXPLOTION_MONO }, // effect/explotion01mono.jpg
    { 32359, BITMAP_KANTURU3RD_OBJECT }, // object40/mtowereffe.jpg
    { 32360, BITMAP_KANTURU_SUCCESS }, // interface/success_kantru.tga
    { 32361, BITMAP_KANTURU_FAILED }, // interface/failure_kantru.tga
    { 32362, BITMAP_KANTURU_COUNTER }, // interface/monstercount.tga
    { 32363, BITMAP_ENERGY_RING }, // effect/bluering0001_r.jpg
    { 32364, BITMAP_ENERGY_FIELD }, // effect/bluewave0001_r.jpg
    { 32365, BITMAP_ITEM_EFFECT_DBSTONE_R }, // item/deathbeamstone_r.jpg
    { 32366, BITMAP_ITEM_EFFECT_HELLHORN_R }, // item/hellhorn_r.jpg
    { 32367, BITMAP_ITEM_EFFECT_PFEATHER_R }, // item/phoenixfeather_r.tga
    { 32368, BITMAP_ITEM_EFFECT_DEYE_R }, // item/deye_r.jpg
    { 32369, BITMAP_ITEM_NIGHT_3RDWING_R }, // item/wing3chaking2.jpg
    { 32370, BITMAP_FIRE_RED }, // effect/firered.jpg
    { 32371, BITMAP_FIRE_CURSEDLICH }, // effect/firehik02.jpg
    { 32372, BITMAP_LEAF_TOTEMGOLEM }, // monster/totemgolem_leaf.tga
    { 32373, BITMAP_SUMMON_IMPACT }, // effect/empact01.jpg
    { 32374, BITMAP_SUMMON_SAHAMUTT_EXPLOSION }, // effect/loungexflow.jpg
    { 32375, BITMAP_ROOLOFPAPER_EFFECT_R }, // item/rollofpaper_r.jpg
    { 32376, BITMAP_PIN_LIGHT }, // effect/pin_lights.jpg
    { 32377, BITMAP_DRAIN_LIFE_GHOST }, // effect/gostmark01.jpg
    { 32378, BITMAP_MAGIC_ZIN }, // effect/mzine_typer2.jpg
    { 32379, BITMAP_ORORA }, // effect/hikorora.jpg
    { 32380, BITMAP_LIGHT_MARKS }, // effect/lightmarks.jpg
    { 32381, BITMAP_TARGET_POSITION_EFFECT1 }, // effect/cursorpin01.jpg
    { 32382, BITMAP_TARGET_POSITION_EFFECT2 }, // effect/cursorpin02.jpg
    { 32383, BITMAP_SHADOW_PAWN_RED }, // monster/red_shadows.jpg
    { 32384, BITMAP_SHADOW_KINGHT_BLUE }, // monster/blue_shadows.jpg
    { 32385, BITMAP_SHADOW_ROOK_GREEN }, // monster/green_shadows.jpg
    { 32386, BITMAP_SMOKELINE1 }, // effect/smokelines01.jpg
    { 32387, BITMAP_SMOKELINE2 }, // effect/smokelines02.jpg
    { 32388, BITMAP_SMOKELINE3 }, // effect/smokelines03.jpg
    { 32389, BITMAP_LIGHTNING_MEGA1 }, // effect/lighting_mega01.jpg
    { 32390, BITMAP_LIGHTNING_MEGA2 }, // effect/lighting_mega02.jpg
    { 32391, BITMAP_LIGHTNING_MEGA3 }, // effect/lighting_mega03.jpg
    { 32392, BITMAP_FIRE_HIK1 }, // effect/firehik01.jpg
    { 32393, BITMAP_FIRE_HIK3 }, // effect/firehik03.jpg
    { 32394, BITMAP_FIRE_HIK1_MONO }, // effect/firehik_mono01.jpg
    { 32395, BITMAP_FIRE_HIK2_MONO }, // effect/firehik_mono02.jpg
    { 32396, BITMAP_FIRE_HIK3_MONO }, // effect/firehik_mono03.jpg
    { 32397, BITMAP_SERUFAN_WAND_R }, // monster/serufanwand_r.jpg
    { 32398, BITMAP_SERUFAN_ARM_R }, // monster/serufanarm_r.jpg
    { 32399, BITMAP_RAKLION_CLOUDS }, // effect/clouds3.jpg
    { 32400, BITMAP_IRONKNIGHT_BODY_BRIGHT }, // monster/icenightlight.jpg
    { 32401, BITMAP_SOCKETSTAFF }, // item/soketmagic_stape02.jpg
    { 32402, BITMAP_LIGHTMARKS }, // effect/lightmarks.jpg
    { 32403, BITMAP_LIGHTMARKS_FOREIGN }, // effect/lightmarks.jpg
    { 32404, BITMAP_TWLIGHT }, // skill/twlighthik01.jpg
    { 32405, BITMAP_2LINE_GHOST }, // skill/2line_gost.jpg
    { 32406, BITMAP_FIRECRACKER0001 }, // effect/firecracker0001.jpg
    { 32407, BITMAP_FIRECRACKER0002 }, // effect/firecracker0002.jpg
    { 32408, BITMAP_FIRECRACKER0003 }, // effect/firecracker0003.jpg
    { 32409, BITMAP_FIRECRACKER0004 }, // effect/firecracker0004.jpg
    { 32410, BITMAP_FIRECRACKER0005 }, // effect/firecracker0005.jpg
    { 32411, BITMAP_FIRECRACKER0006 }, // effect/firecracker0006.jpg
    { 32412, BITMAP_FIRECRACKER0007 }, // effect/firecracker0007.jpg
    { 32415, BITMAP_PKMON01 }, // effect/pk_mon02_fire.jpg
    { 32416, BITMAP_PKMON02 }, // effect/pk_mon03_red.jpg
    { 32417, BITMAP_PKMON03 }, // effect/pk_mon02_green.jpg
    { 32418, BITMAP_PKMON04 }, // effect/pk_mon03_green.jpg
    { 32419, BITMAP_PKMON05 }, // effect/lavagiantaa_e.jpg
    { 32420, BITMAP_PKMON06 }, // effect/lavagiantba_e.jpg
    { 32421, BITMAP_LAVAGIANT_FOOTPRINT_R }, // effect/eff_magma_red.jpg
    { 32422, BITMAP_LAVAGIANT_FOOTPRINT_V }, // effect/eff_magma_violet.jpg
    { 32423, BITMAP_BUGBEAR_R }, // effect/bugbear_r.jpg
    { 32425, BITMAP_DOPPELGANGER_ICEWALKER0 }, // monster/dgicewalker_body.jpg
    { 32426, BITMAP_DOPPELGANGER_ICEWALKER1 }, // monster/dgicewalker_r.jpg
    { 32427, BITMAP_DOPPELGANGER_SNAKE01 }, // monster/snake1.jpg
    { 32430, BITMAP_DEASULER_CLOTH }, // monster/deasuler_cloth.tga
    { 32434, BITMAP_RAYMOND_SWORD }, // effect/raymond_sword_r.jpg
    { 32435, BITMAP_ASSASSIN_EFFECT1 }, // monster/assassinleader_body_r.jpg
    { 32436, BITMAP_AG_ADDITION_EFFECT }, // effect/mist01.jpg
    { 32437, BITMAP_PACKAGEBOX_BLUE }, // item/ingameshop/membership_item_blue.jpg
    { 32438, BITMAP_PACKAGEBOX_GOLD }, // item/ingameshop/membership_item_gold.jpg
    { 32439, BITMAP_PACKAGEBOX_GREEN }, // item/ingameshop/membership_item_green.jpg
    { 32440, BITMAP_PACKAGEBOX_PUPLE }, // item/ingameshop/membership_item_pouple.jpg
    { 32441, BITMAP_PACKAGEBOX_RED }, // item/ingameshop/membership_item_red.jpg
    { 32442, BITMAP_PACKAGEBOX_SKY }, // item/ingameshop/membership_item_sky.jpg
    { 32443, BITMAP_INGAMESHOP_PRIMIUM6 }, // item/ingameshop/mebership_3items_green.jpg
    { 32444, BITMAP_INGAMESHOP_COMMUTERTICKET4 }, // item/ingameshop/mebership_3items_red.jpg
    { 32445, BITMAP_INGAMESHOP_SIZECOMMUTERTICKET3 }, // item/ingameshop/mebership_3items_yellow.jpg
    { 32458, BITMAP_RGB_MIX }, // item/rgb_mix.jpg
    { 32459, BITMAP_RAREITEM7 }, // item/partcharge8/rareitem_ticket_7_body.jpg
    { 32460, BITMAP_RAREITEM8 }, // item/partcharge8/rareitem_ticket_8_body.jpg
    { 32461, BITMAP_RAREITEM9 }, // item/partcharge8/rareitem_ticket_9_body.jpg
    { 32462, BITMAP_RAREITEM10 }, // item/partcharge8/rareitem_ticket_10_body.jpg
    { 32463, BITMAP_RAREITEM11 }, // item/partcharge8/rareitem_ticket_11_body.jpg
    { 32464, BITMAP_RAREITEM12 }, // item/partcharge8/rareitem_ticket_12_body.jpg
    { 32465, BITMAP_DOPPLEGANGGER_FREETICKET }, // item/partcharge8/doppelcard.jpg
    { 32466, BITMAP_BARCA_FREETICKET }, // item/partcharge8/barcacard.jpg
    { 32467, BITMAP_BARCA7TH_FREETICKET }, // item/partcharge8/barca7card.jpg
    { 32472, BITMAP_SBUMB }, // effect/sbumb.jpg
    { 32473, BITMAP_FORCEPILLAR }, // effect/force_pillar.jpg
    { 32474, BITMAP_SWORDEFF }, // effect/!swordeff.jpg
    { 32475, BITMAP_DAMAGE1 }, // effect/damage1.jpg
    { 32476, BITMAP_GROUND_WIND }, // effect/ground_wind.jpg
    { 32477, BITMAP_KWAVE2 }, // effect/kwave2.jpg
    { 32478, BITMAP_DAMAGE2 }, // effect/damage2.jpg
    { 32479, BITMAP_VOLCANO_CORE }, // effect/volcano_core.jpg
    { 32480, BITMAP_NCCAPE }, // item/nccape.tga
    { 32481, BITMAP_MANTO01 }, // item/monk_manto01.tga
    { 32482, BITMAP_MANTOE }, // item/monke_manto.tga
    { 32483, BITMAP_GROUND_SMOKE }, // effect/ground_smoke.tga
    { 32484, BITMAP_KNIGHTST_BLUE }, // effect/knightst_blue.jpg
    { 32485, BITMAP_BONE_SCORPION_SKIN_EFFECT }, // monster/bonese.jpg
    { 32486, BITMAP_KRYPTA_BALL_EFFECT }, // monster/kryptaball2.jpg
    { 32487, BITMAP_CONDRA_SKIN_EFFECT }, // monster/bora_golem_effect.jpg
    { 32488, BITMAP_CONDRA_SKIN_EFFECT2 }, // monster/bora_golem2_effect.jpg
    { 32489, BITMAP_NARCONDRA_SKIN_EFFECT }, // monster/king_golem01_effect.jpg
    { 32490, BITMAP_NARCONDRA_SKIN_EFFECT2 }, // monster/king_golem02_effect.jpg
    { 32491, BITMAP_NARCONDRA_SKIN_EFFECT3 }, // monster/king_golem03_effect.jpg
    { 32502, BITMAP_PHOENIXSOULWING }, // item/phoenixsoul_render.jpg
};
} // namespace

int MuEmu::Texture::From104E(int id)
{
    if (id <= LastSharedTexture)
        return id;

    const auto found = std::lower_bound(std::begin(Textures), std::end(Textures), id,
                                        [](const TextureMap& entry, int value) { return entry.id104E < value; });
    if (found == std::end(Textures) || found->id104E != id)
        return -1;
    return found->texture;
}

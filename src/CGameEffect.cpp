#include "CGameEffect.h"

#include "CAIScript.h"
#include "CBaldurChitin.h"
#include "CBaldurProjector.h"
#include "CBlood.h"
#include "CCreatureFile.h"
#include "CGameAnimationType.h"
#include "CGameArea.h"
#include "CGameObject84D624.h"
#include "CGameSprite.h"
#include "CInfCursor.h"
#include "CInfGame.h"
#include "CPathSearch.h"
#include "CProjectile.h"
#include "CProjectile84DFA4.h"
#include "CProjectile84E940.h"
#include "CProjectile84E9C0.h"
#include "CProjectileArea.h"
#include "CProjectileBAM.h"
#include "CProjectileShaft.h"
#include "CResEffect.h"
#include "CScreenWorld.h"
#include "CSevenEyes.h"
#include "CSpell.h"
#include "CUIPanel.h"
#include "CUtil.h"
#include "CVidPalette.h"
#include "Icewind586B70.h"
#include "IcewindCGameEffects.h"
#include "IcewindMisc.h"

#include <cstdio>
#include <list>
#include <string>

// NOTE: Not used. It does not include many effects. Formatting is odd.
//
// 0x8D45F0
const CString CGameEffect::EFFECT_NAME[] = {
    "AC                        ",
    "ATTACKNO                  ",
    "AWAKEN                    ",
    "BERSERK                   ",
    "CALM                      ",
    "CHARM                     ",
    "CHR                       ",
    "COLORCHANGE               ",
    "COLORGLOW_SOLID           ",
    "COLORGLOW_PULSE           ",
    "CON                       ",
    "CUREPOISON                ",
    "DAMAGE                    ",
    "DEATH                     ",
    "DEFROST                   ",
    "DEX                       ",
    "HASTE                     ",
    "HEAL                      ",
    "HITPOINTS                 ",
    "INT                       ",
    "INVISIBLE                 ",
    "LORE                      ",
    "LUCK                      ",
    "MORALE                    ",
    "PANIC                     ",
    "POISON                    ",
    "REMOVECURSE               ",
    "RESISTACID                ",
    "RESISTCOLD                ",
    "RESISTELECTRICITY         ",
    "RESISTFIRE                ",
    "RESISTMAGIC               ",
    "RESURRECT                 ",
    "SAVEFORTITUDE             ",
    "SAVEREFLEX                ",
    "SAVEWILL                  ",
    "SILENCE                   ",
    "SLEEP                     ",
    "SLOW                      ",
    "SPARKLE                   ",
    "SPELLMEMORIZATIONMAGE     ",
    "STONETOFLESH              ",
    "STR                       ",
    "STUN                      ",
    "UNSTUN                    ",
    "VISIBLE                   ",
    "VOCALIZE                  ",
    "WIS                       ",
    "SINGLECOLORPULSEALL       ",
    "COLORTINT_SOLID           ",
    "COLORLIGHT_SOLID          ",
    "ANIMATION_CHANGE          ",
    "THAC0                     ",
    "SLAY                      ",
    "ALIGNMENTREVERSAL         ",
    "ALIGNMENTCHANGE           ",
    "DISPELEFFECTS             ",
    "SKILLSTEALTH              ",
    "CASTINGFAILURE            ",
    "",
    "SPELLMEMORIZATIONCLERIC   ",
    "INFRAVISION               ",
    "INFRAVISIONOFF            ",
    "BLUR                      ",
    "TRANSLUCENT               ",
    "SUMMON                    ",
    "UNSUMMON                  ",
    "NONDETECTION              ",
    "ENDNONDETECTION           ",
    "SEXCHANGE                 ",
    "AICHANGE                  ",
    "DAMAGEMOD                 ",
    "BLINDNESS                 ",
    "CUREBLINDESS              ",
    "FEEBLEMINDEDNESS          ",
    "CUREFEEBLEMINDEDNESS      ",
    "DISEASE                   ",
    "CUREDISEASE               ",
    "DEAFNESS                  ",
    "CUREDEAFNESS              ",
    "SETAISCRIPT               ",
    "IMMUNITYTOPROJECTILE      ",
    "RESISTMAGICFIRE \t\t  ",
    "RESISTMAGICCOLD \t\t  ",
    "RESISTSLASHING \t\t",
    "RESISTBLUDGEONING\t\t",
    "RESISTPIERCING \t\t",
    "RESISTMISSILE  \t\t",
    "SKILLLOCKPICKING\t\t  ",
    "SKILLTRAPS\t\t\t\t  ",
    "SKILLPICKPOCKET \t\t  ",
    "FATIGUE \t\t\t\t  ",
    "INTOXICATION \t\t\t  ",
    "SKILLTRACKING  \t\t",
    "LEVEL \t\t\t\t\t  ",
    "STREXTRA \t\t\t\t  ",
    "REGENERATION\t\t\t  ",
    "DURATIONMOD \t\t\t  ",
    "PROTECTIONFROMCREATURE\t  ",
    "IMMUNITYTOEFFECT    \t  ",
    "IMMUNITYTOSPELLLEVEL  \t  ",
    "NAME                  \t  ",
    "XP                    \t  ",
    "GOLD                  \t  ",
    "MORALEBREAK           \t  ",
    "PORTRAIT              \t  ",
    "REPUTATION            \t  ",
    "HOLD_CREATURE             ",
    "RETREAT_FROM              ",
    "CREATE_WEAPON             ",
    "DESTROY_WEAPON            ",
    "EQUIP_WEAPON              ",
    "DITHER                    ",
    "DETECT_ALIGNMENT          ",
    "DISPEL_INVISIBLE          ",
    "SHOW_AREA                 ",
    "SHOW_CREATURES            ",
    "MIRROR_IMAGE              ",
    "IMMUNITY_TO_WEAPON        ",
    "VISUAL_ANIMATION_EFFECT   ",
    "CREATE_ITEM               ",
    "DESTROY_ITEM              ",
    "TELEPORT                  ",
    "KNOCK                     ",
    "MOVEMENT_RATE             ",
    "RANDOM_SUMMON             ",
    "CONFUSION                 ",
    "NON_CUMULATIVE_AID        ",
    "NON_CUMULATIVE_BLESS      ",
    "NON_CUMULATIVE_CHANT      ",
    "NON_CUMULATIVE_DRAW_UPON_HOLY_MIGHT",
    "NON_CUMULATIVE_LUCK       ",
    "PETRIFICATION             ",
    "POLYMORPH                 ",
    "FORCE_VISIBLE             ",
    "NON_CUMULATIVE_CHANTBAD   ",
    "SETSEQUENCE               ",
    "DISPLAYSTRING             ",
    "CASTINGGLOW               ",
    "VISUALSPELLHIT            ",
    "PORTRAITICON              ",
    "REPLACE_ITEM              ",
    "DISABLE_BUTTON            ",
    "DISABLE_SPELLTYPE         ",
    "CAST_SPELL                ",
    "LEARN_SPELL               ",
    "CAST_SPELL_POINT          ",
    "IDENTIFY                  ",
    "DETECTTRAPS",
    "REPLACESELF",
    "PLAYMOVIE",
    "SANCTUARY",
    "e",
    "f",
    "g",
    "h",
    "i",
    "j",
    "k",
    "l",
    "m",
    "n",
    "o",
    "p",
    "q",
    "q",
    "q",
    "q",
    "q",
    "q",
};

// NOTE: Inlined.
CGameEffectBase::CGameEffectBase()
{
    memset(this, 0, sizeof(*this));

    m_version = "EFF V2.0";
    m_effectID = 0;
    m_targetType = 0;
    m_spellLevel = 0;
    m_effectAmount = 0;
    m_dwFlags = 0;
    m_durationType = 0;
    m_duration = 0;
    m_probabilityUpper = 100;
    m_probabilityLower = 0;
    m_res = "";
    m_numDice = 0;
    m_diceSize = 0;
    m_savingThrow = 0;
    m_saveMod = 0;
    m_special = 0;
    m_school = 0;
    field_48 = 0;
    m_minLevel = 0;
    m_maxLevel = 0;
    m_flags = 0;
    m_effectAmount2 = 0;
    m_effectAmount3 = 0;
    m_effectAmount4 = 0;
    m_effectAmount5 = 0;
    m_res2 = "";
    m_res3 = "";
    m_source.x = -1;
    m_source.y = -1;
    m_target.x = -1;
    m_target.y = -1;
    m_sourceType = 0;
    m_sourceRes = "";
    m_sourceFlags = 0;
    m_projectileType = 0;
    m_slotNum = -1;
    m_casterLevel = 0;
    m_spellPenetrationBonus = 0;
    m_secondaryType = 1;
    memset(m_pad, 0, sizeof(m_pad));
}

// 0x48C800
CGameEffect* CGameEffect::DecodeEffect(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, const CPoint& target)
{
    if (effect->diceSize == -1) {
        effect->diceSize = 0;
    }

    if (effect->effectID == 36 || effect->effectID == 37) {
        CString sError;
        sError.Format("Old saving throw effect (breath or spell) detected:  id=%d  res=%s",
            effect->effectID,
            effect->res);
        effect->effectID = CGAMEEFFECT_SAVEVSWILL;
    }

    switch (effect->effectID) {
    case CGAMEEFFECT_AC:
        return new CGameEffectAC(effect, source, sourceID, target);
    case CGAMEEFFECT_ATTACKNO:
        return new CGameEffectAttackNo(effect, source, sourceID, target);
    case CGAMEEFFECT_AWAKEN:
        return new CGameEffectAwaken(effect, source, sourceID, target);
    case CGAMEEFFECT_BERSERK:
        return new CGameEffectBerserk(effect, source, sourceID, target);
    case CGAMEEFFECT_CALM:
        return new CGameEffectCalm(effect, source, sourceID, target);
    case CGAMEEFFECT_CHARM:
        return new CGameEffectCharm(effect, source, sourceID, target);
    case CGAMEEFFECT_CHR:
        return new CGameEffectCHR(effect, source, sourceID, target);
    case CGAMEEFFECT_COLORCHANGE:
        return new CGameEffectColorChange(effect, source, sourceID, target);
    case CGAMEEFFECT_COLORGLOWSOLID:
        return new CGameEffectColorGlowSolid(effect, source, sourceID, target);
    case CGAMEEFFECT_COLORGLOWPULSE:
        return new CGameEffectColorGlowPulse(effect, source, sourceID, target);
    case CGAMEEFFECT_CON:
        return new CGameEffectCON(effect, source, sourceID, target);
    case CGAMEEFFECT_CUREPOISON:
        return new CGameEffectCurePoison(effect, source, sourceID, target);
    case CGAMEEFFECT_DAMAGE:
        return new CGameEffectDamage(effect, source, sourceID, target);
    case CGAMEEFFECT_DEATH:
        return new CGameEffectDeath(effect, source, sourceID, target);
    case CGAMEEFFECT_DEFROST:
        return new CGameEffectDefrost(effect, source, sourceID, target);
    case CGAMEEFFECT_DEX:
        return new CGameEffectDEX(effect, source, sourceID, target);
    case CGAMEEFFECT_HASTE:
        return new CGameEffectHaste(effect, source, sourceID, target);
    case CGAMEEFFECT_HEAL:
        return new CGameEffectHeal(effect, source, sourceID, target);
    case CGAMEEFFECT_HITPOINTS:
        return new CGameEffectHitPoints(effect, source, sourceID, target);
    case CGAMEEFFECT_INT:
        return new CGameEffectINT(effect, source, sourceID, target);
    case CGAMEEFFECT_INVISIBLE:
        return new CGameEffectInvisible(effect, source, sourceID, target);
    case CGAMEEFFECT_LORE:
        return new CGameEffectLore(effect, source, sourceID, target);
    case CGAMEEFFECT_LUCK:
        return new CGameEffectLuck(effect, source, sourceID, target);
    case CGAMEEFFECT_MORALE:
        return new CGameEffectMorale(effect, source, sourceID, target);
    case CGAMEEFFECT_PANIC:
        return new CGameEffectPanic(effect, source, sourceID, target);
    case CGAMEEFFECT_POISON:
        return new CGameEffectPoison(effect, source, sourceID, target);
    case CGAMEEFFECT_REMOVECURSE:
        return new CGameEffectRemoveCurse(effect, source, sourceID, target);
    case CGAMEEFFECT_RESISTACID:
        return new CGameEffectResistAcid(effect, source, sourceID, target);
    case CGAMEEFFECT_RESISTCOLD:
        return new CGameEffectResistCold(effect, source, sourceID, target);
    case CGAMEEFFECT_RESISTELECTRICITY:
        return new CGameEffectResistElectricity(effect, source, sourceID, target);
    case CGAMEEFFECT_RESISTFIRE:
        return new CGameEffectResistFire(effect, source, sourceID, target);
    case CGAMEEFFECT_RESISTMAGIC:
        return new CGameEffectResistMagic(effect, source, sourceID, target);
    case CGAMEEFFECT_RESURRECT:
        return new CGameEffectResurrect(effect, source, sourceID, target);
    case CGAMEEFFECT_SAVEVSFORTITUDE:
        return new CGameEffectSaveVsFortitude(effect, source, sourceID, target);
    case CGAMEEFFECT_SAVEVSRELFEX:
        return new CGameEffectSaveVsRelfex(effect, source, sourceID, target);
    case CGAMEEFFECT_SAVEVSWILL:
        return new CGameEffectSaveVsWill(effect, source, sourceID, target);
    case CGAMEEFFECT_SILENCE:
        return new CGameEffectSilence(effect, source, sourceID, target);
    case CGAMEEFFECT_SLEEP:
        return new CGameEffectSleep(effect, source, sourceID, target);
    case CGAMEEFFECT_SLOW:
        return new CGameEffectSlow(effect, source, sourceID, target);
    case CGAMEEFFECT_SPARKLE:
        return new CGameEffectSparkle(effect, source, sourceID, target);
    case CGAMEEFFECT_SPELLMEMORIZATIONMAGE:
        return new CGameEffectSpellMemorizationMage(effect, source, sourceID, target);
    case CGAMEEFFECT_STONETOFLESH:
        return new CGameEffectStoneToFlesh(effect, source, sourceID, target);
    case CGAMEEFFECT_STR:
        return new CGameEffectSTR(effect, source, sourceID, target);
    case CGAMEEFFECT_STUN:
        return new CGameEffectStun(effect, source, sourceID, target);
    case CGAMEEFFECT_UNSTUN:
        return new CGameEffectUnstun(effect, source, sourceID, target);
    case CGAMEEFFECT_VISIBLE:
        return new CGameEffectVisible(effect, source, sourceID, target);
    case CGAMEEFFECT_VOCALIZE:
        return new CGameEffectVocalize(effect, source, sourceID, target);
    case CGAMEEFFECT_WIS:
        return new CGameEffectWIS(effect, source, sourceID, target);
    case CGAMEEFFECT_SINGLECOLORPULSEALL:
        return new CGameEffectSingleColorPulseAll(effect, source, sourceID, target);
    case CGAMEEFFECT_COLORTINTSOLID:
        return new CGameEffectColorTintSolid(effect, source, sourceID, target);
    case CGAMEEFFECT_COLORLIGHTSOLID:
        return new CGameEffectColorLightSolid(effect, source, sourceID, target);
    case CGAMEEFFECT_ANIMATIONCHANGE:
        return new CGameEffectAnimationChange(effect, source, sourceID, target);
    case CGAMEEFFECT_BASEATTACKBONUS:
        return new CGameEffectBaseAttackBonus(effect, source, sourceID, target);
    case CGAMEEFFECT_SLAY:
        return new CGameEffectSlay(effect, source, sourceID, target);
    case CGAMEEFFECT_ALIGNMENTREVERSAL:
        return new CGameEffectAlignmentReversal(effect, source, sourceID, target);
    case CGAMEEFFECT_ALIGNMENTCHANGE:
        return new CGameEffectAlignmentChange(effect, source, sourceID, target);
    case CGAMEEFFECT_DISPELEFFECTS:
        return new CGameEffectDispelEffects(effect, source, sourceID, target);
    case CGAMEEFFECT_SKILLSTEALTH:
        return new CGameEffectSkillStealth(effect, source, sourceID, target);
    case CGAMEEFFECT_CASTINGFAILURE:
        return new CGameEffectCastingFailure(effect, source, sourceID, target);
    case CGAMEEFFECT_SKILLALCHEMY:
        return new CGameEffectSkillAlchemy(effect, source, sourceID, target);
    case CGAMEEFFECT_SPELLMEMOIZATIONCLERIC:
        return new CGameEffectSpellMemoizationCleric(effect, source, sourceID, target);
    case CGAMEEFFECT_INFRAVISION:
        return new CGameEffectInfravision(effect, source, sourceID, target);
    case CGAMEEFFECT_INFRAVISIONOFF:
        return new CGameEffectInfravisionOff(effect, source, sourceID, target);
    case CGAMEEFFECT_BLUR:
        return new CGameEffectBlur(effect, source, sourceID, target);
    case CGAMEEFFECT_TRANSLUCENT:
        return new CGameEffectTranslucent(effect, source, sourceID, target);
    case CGAMEEFFECT_SUMMON:
        return new IcewindCGameEffectSummon(effect, source, sourceID, target);
    case CGAMEEFFECT_SKILLUNSUMMON:
        return new CGameEffectSkillUnsummon(effect, source, sourceID, target);
    case CGAMEEFFECT_NONDETECTION:
        return new CGameEffectNonDetection(effect, source, sourceID, target);
    case CGAMEEFFECT_ENDNONDETECTION:
        return new CGameEffectEndNonDetection(effect, source, sourceID, target);
    case CGAMEEFFECT_SEXCHANGE:
        return new CGameEffectSexChange(effect, source, sourceID, target);
    case CGAMEEFFECT_AICHANGE:
        return new CGameEffectAIChange(effect, source, sourceID, target);
    case CGAMEEFFECT_DAMAGEMOD:
        return new CGameEffectDamageMod(effect, source, sourceID, target);
    case CGAMEEFFECT_BLINDNESS:
        return new CGameEffectBlindness(effect, source, sourceID, target);
    case CGAMEEFFECT_CUREBLINDNESS:
        return new CGameEffectCureBlindness(effect, source, sourceID, target);
    case CGAMEEFFECT_FEEBLEMINDEDNESS:
        return new CGameEffectFeebleMindedness(effect, source, sourceID, target);
    case CGAMEEFFECT_CUREFEEBLEMINDEDNESS:
        return new CGameEffectCureFeebleMindedness(effect, source, sourceID, target);
    case CGAMEEFFECT_DISEASE:
        return new CGameEffectDisease(effect, source, sourceID, target);
    case CGAMEEFFECT_CUREDISEASE:
        return new CGameEffectCureDisease(effect, source, sourceID, target);
    case CGAMEEFFECT_DEAFNESS:
        return new CGameEffectDeafness(effect, source, sourceID, target);
    case CGAMEEFFECT_CUREDEAFNESS:
        return new CGameEffectCureDeafness(effect, source, sourceID, target);
    case CGAMEEFFECT_SETAISCRIPT:
        return new CGameEffectSetAIScript(effect, source, sourceID, target);
    case CGAMEEFFECT_IMMUNITYTOPROJECTILE:
        return new CGameEffectImmunityToProjectile(effect, source, sourceID, target);
    case 84:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 890
        UTIL_ASSERT_MSG(FALSE, "CGameEffectResistMagicFire game effect not supported in Icewind II (-rjf)");
    case 85:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 894
        UTIL_ASSERT_MSG(FALSE, "CGameEffectResistMagicCold game effect not supported in Icewind II (-rjf)");
    case CGAMEEFFECT_RESISTSLASHING:
        return new CGameEffectResistSlashing(effect, source, sourceID, target);
    case CGAMEEFFECT_RESISTCRUSHING:
        return new CGameEffectResistCrushing(effect, source, sourceID, target);
    case CGAMEEFFECT_RESISTPIERCING:
        return new CGameEffectResistPiercing(effect, source, sourceID, target);
    case CGAMEEFFECT_RESISTMISSILE:
        return new CGameEffectResistMissile(effect, source, sourceID, target);
    case CGAMEEFFECT_SKILLLOCKPICKING:
        return new CGameEffectSkillLockPicking(effect, source, sourceID, target);
    case CGAMEEFFECT_SKILLTRAPS:
        return new CGameEffectSkillTraps(effect, source, sourceID, target);
    case CGAMEEFFECT_SKILLPICKPOCKET:
        return new CGameEffectSkillPickPocket(effect, source, sourceID, target);
    case CGAMEEFFECT_FATIGUE:
        return new CGameEffectFatigue(effect, source, sourceID, target);
    case CGAMEEFFECT_INTOXICATION:
        return new CGameEffectIntoxication(effect, source, sourceID, target);
    case CGAMEEFFECT_SKILLTRACKING:
        return new CGameEffectSkillTracking(effect, source, sourceID, target);
    case CGAMEEFFECT_LEVEL:
        return new CGameEffectLevel(effect, source, sourceID, target);
    case CGAMEEFFECT_STREXTRA:
        return new CGameEffectSTRExtra(effect, source, sourceID, target);
    case CGAMEEFFECT_REGENERATION:
        return new CGameEffectRegeneration(effect, source, sourceID, target);
    case CGAMEEFFECT_DURATIONCASTING:
        return new CGameEffectDurationCasting(effect, source, sourceID, target);
    case CGAMEEFFECT_PROTECTIONFROMCREATURE:
        return new CGameEffectProtectionFromCreature(effect, source, sourceID, target);
    case CGAMEEFFECT_IMMUNITYTOEFFECT:
        return new CGameEffectImmunityToEffect(effect, source, sourceID, target);
    case CGAMEEFFECT_IMMUNITYTOSPELLLEVEL:
        return new CGameEffectImmunityToSpellLevel(effect, source, sourceID, target);
    case CGAMEEFFECT_NAME:
        return new CGameEffectName(effect, source, sourceID, target);
    case CGAMEEFFECT_XP:
        return new CGameEffectXP(effect, source, sourceID, target);
    case CGAMEEFFECT_GOLD:
        return new CGameEffectGold(effect, source, sourceID, target);
    case CGAMEEFFECT_MORALBREAK:
        return new CGameEffectMoralBreak(effect, source, sourceID, target);
    case CGAMEEFFECT_PORTRAIT:
        return new CGameEffectPortrait(effect, source, sourceID, target);
    case CGAMEEFFECT_REPUTATION:
        return new CGameEffectReputation(effect, source, sourceID, target);
    case CGAMEEFFECT_HOLDCREATURE:
        return new CGameEffectHoldCreature(effect, source, sourceID, target);
    case CGAMEEFFECT_RETREATFROM:
        return new CGameEffectRetreatFrom(effect, source, sourceID, target);
    case CGAMEEFFECT_CREATEWEAPON:
        return new CGameEffectCreateWeapon(effect, source, sourceID, target);
    case CGAMEEFFECT_DESTROYWEAPON:
        return new CGameEffectDestroyWeapon(effect, source, sourceID, target);
    case CGAMEEFFECT_EQUIPWEAPON:
        return new CGameEffectEquipWeapon(effect, source, sourceID, target);
    case CGAMEEFFECT_DITHER:
        return new CGameEffectDither(effect, source, sourceID, target);
    case 115:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 988
        UTIL_ASSERT_MSG(FALSE, "CGameEffectDetectAlignment game effect not supported in Icewind II (-rjf)");
    case CGAMEEFFECT_DISPELINVISIBLE:
        return new CGameEffectDispelInvisible(effect, source, sourceID, target);
    case CGAMEEFFECT_SHOWAREA:
        return new CGameEffectShowArea(effect, source, sourceID, target);
    case CGAMEEFFECT_SHOWCREATURES:
        return new CGameEffectShowCreatures(effect, source, sourceID, target);
    case CGAMEEFFECT_MIRRORIMAGE:
        return new CGameEffectMirrorImage(effect, source, sourceID, target);
    case CGAMEEFFECT_IMMUNETOWEAPON:
        return new CGameEffectImmuneToWeapon(effect, source, sourceID, target);
    case CGAMEEFFECT_VISUALANIMATIONEFFECT:
        return new CGameEffectVisualAnimationEffect(effect, source, sourceID, target);
    case CGAMEEFFECT_CREATEITEM:
        return new CGameEffectCreateItem(effect, source, sourceID, target);
    case CGAMEEFFECT_DESTROYITEM:
        return new CGameEffectDestroyItem(effect, source, sourceID, target);
    case CGAMEEFFECT_TELEPORT:
        return new CGameEffectTeleport(effect, source, sourceID, target);
    case CGAMEEFFECT_KNOCK:
        return new CGameEffectKnock(effect, source, sourceID, target);
    case CGAMEEFFECT_MOVEMENTRATE_126:
        return new CGameEffectMovementRate(effect, source, sourceID, target);
    case 127:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 1025
        UTIL_ASSERT_MSG(FALSE, "CGameEffectSummon game effect not supported in Icewind II (-rjf)");
    case CGAMEEFFECT_CONFUSION:
        return new CGameEffectConfusion(effect, source, sourceID, target);
    case CGAMEEFFECT_NON_CUMULATIVEAID:
        return new CGameEffectNon_CumulativeAid(effect, source, sourceID, target);
    case CGAMEEFFECT_NON_CUMULATIVEBLESS:
        return new CGameEffectNon_CumulativeBless(effect, source, sourceID, target);
    case CGAMEEFFECT_NON_CUMULATIVECHANT:
        return new CGameEffectNon_CumulativeChant(effect, source, sourceID, target);
    case CGAMEEFFECT_NON_CUMULATIVEDRAWUPONHOLYMIGHT:
        return new CGameEffectNon_CumulativeDrawUponHolyMight(effect, source, sourceID, target);
    case CGAMEEFFECT_NON_CUMULATIVELUCK:
        return new CGameEffectNon_CumulativeLuck(effect, source, sourceID, target);
    case CGAMEEFFECT_PETRIFICATION:
        return new CGameEffectPetrification(effect, source, sourceID, target);
    case CGAMEEFFECT_POLYMORPH:
        return new CGameEffectPolymorph(effect, source, sourceID, target);
    case CGAMEEFFECT_FORCEVISIBLE:
        return new CGameEffectForceVisible(effect, source, sourceID, target);
    case 137:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 1056
        UTIL_ASSERT_MSG(FALSE, "CGameEffectNon_CumulativeChantBad game effect not supported in Icewind II (-rjf)");
    case CGAMEEFFECT_SETSEQUENCE:
        return new CGameEffectSetSequence(effect, source, sourceID, target);
    case CGAMEEFFECT_DISPLAYSTRING:
        return new CGameEffectDisplayString(effect, source, sourceID, target);
    case CGAMEEFFECT_CASTINGGLOW:
        return new IcewindCGameEffectCastingGlow(effect, source, sourceID, target);
    case CGAMEEFFECT_VISUALSPELLHIT:
        return new CGameEffectVisualSpellHit(effect, source, sourceID, target);
    case CGAMEEFFECT_PORTRAITICON:
        return new CGameEffectPortraitIcon(effect, source, sourceID, target);
    case CGAMEEFFECT_REPLACEITEM:
        return new CGameEffectReplaceItem(effect, source, sourceID, target);
    case CGAMEEFFECT_DISABLEBUTTON:
        return new CGameEffectDisableButton(effect, source, sourceID, target);
    case CGAMEEFFECT_DISABLESPELLTYPE:
        return new CGameEffectDisableSpellType(effect, source, sourceID, target);
    case CGAMEEFFECT_CASTSPELL:
        return new CGameEffectCastSpell(effect, source, sourceID, target);
    case CGAMEEFFECT_LEARNSPELL:
        return new CGameEffectLearnSpell(effect, source, sourceID, target);
    case CGAMEEFFECT_CASTSPELLPOINT:
        return new CGameEffectCastSpellPoint(effect, source, sourceID, target);
    case CGAMEEFFECT_IDENTIFY:
        return new CGameEffectIdentify(effect, source, sourceID, target);
    case CGAMEEFFECT_DETECTTRAPS:
        return new CGameEffectDetectTraps(effect, source, sourceID, target);
    case CGAMEEFFECT_REPLACESELF:
        return new CGameEffectReplaceSelf(effect, source, sourceID, target);
    case CGAMEEFFECT_PLAYMOVIE:
        return new CGameEffectPlayMovie(effect, source, sourceID, target);
    case CGAMEEFFECT_SANCTUARY:
        return new CGameEffectSanctuary(effect, source, sourceID, target);
    case CGAMEEFFECT_ENTANGLE:
        return new CGameEffectEntangle(effect, source, sourceID, target);
    case 155:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 818
        UTIL_ASSERT_MSG(FALSE, "CGameEffectMinorGlobe game effect not supported in Icewind II (-rjf)");
    case 156:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 822
        UTIL_ASSERT_MSG(FALSE, "CGameEffectShieldGlobe game effect not supported in Icewind II (-rjf)");
    case CGAMEEFFECT_WEBHOLD:
        return new CGameEffectWebHold(effect, source, sourceID, target);
    case CGAMEEFFECT_GREASEPOOL:
        return new CGameEffectGreasePool(effect, source, sourceID, target);
    case CGAMEEFFECT_MIRRORIMAGERUN:
        return new CGameEffectMirrorImageRun(effect, source, sourceID, target);
    case CGAMEEFFECT_DISPELSANCTUARY:
        return new CGameEffectDispelSanctuary(effect, source, sourceID, target);
    case CGAMEEFFECT_REMOVEPANIC:
        return new CGameEffectRemovePanic(effect, source, sourceID, target);
    case CGAMEEFFECT_REMOVEHOLD:
        return new CGameEffectRemoveHold(effect, source, sourceID, target);
    case CGAMEEFFECT_RESTOREMOVEMENTRATE:
        return new CGameEffectRestoreMovementRate(effect, source, sourceID, target);
    case CGAMEEFFECT_REMOVEINTOXICATION:
        return new CGameEffectRemoveIntoxication(effect, source, sourceID, target);
    case CGAMEEFFECT_PAUSECASTER:
        return new CGameEffectPauseCaster(effect, source, sourceID, target);
    case CGAMEEFFECT_RESISTANCETOMAGIC:
        return new CGameEffectResistanceToMagic(effect, source, sourceID, target);
    case CGAMEEFFECT_MISSILETHAC0BONUS:
        return new CGameEffectMissileTHAC0Bonus(effect, source, sourceID, target);
    case CGAMEEFFECT_DESTROYSELF:
        return new CGameEffectDestroySelf(effect, source, sourceID, target);
    case CGAMEEFFECT_IMMUNITYTOPORTRAITICON:
        return new CGameEffectImmunityToPortraitIcon(effect, source, sourceID, target);
    case CGAMEEFFECT_DAMAGEVISUALEFFECT:
        return new CGameEffectDamageVisualEffect(effect, source, sourceID, target);
    case CGAMEEFFECT_ADDINNATEABILITY:
        return new CGameEffectAddInnateAbility(effect, source, sourceID, target);
    case CGAMEEFFECT_REMOVEINNATEABILITY:
        return new CGameEffectRemoveInnateAbility(effect, source, sourceID, target);
    case CGAMEEFFECT_RESISTANCETOPOISON:
        return new CGameEffectResistanceToPoison(effect, source, sourceID, target);
    case CGAMEEFFECT_PLAYSOUND:
        return new CGameEffectPlaySound(effect, source, sourceID, target);
    case CGAMEEFFECT_HOLDCREATURESPELL_175:
        return new CGameEffectHoldCreatureSpell(effect, source, sourceID, target);
    case CGAMEEFFECT_MOVEMENTRATE_176:
        return new CGameEffectMovementRate(effect, source, sourceID, target);
    case CGAMEEFFECT_APPLYEFFECT:
        return new CGameEffectApplyEffect(effect, source, sourceID, target);
    case CGAMEEFFECT_SELECTIVETOHITBONUS:
        return new CGameEffectSelectiveToHitBonus(effect, source, sourceID, target);
    case CGAMEEFFECT_SELECTDAMAGEBONUS:
        return new CGameEffectSelectiveDamageBonus(effect, source, sourceID, target);
    case CGAMEEFFECT_RESTRICTEQUIPITEM:
        return new CGameEffectRestrictEquipItem(effect, source, sourceID, target);
    case CGAMEEFFECT_RESTRICTEQUIPITEMTYPE:
        return new CGameEffectRestrictEquipItemType(effect, source, sourceID, target);
    case CGAMEEFFECT_APPLYEFFECTEQUIPITEM:
        return new CGameEffectApplyEffectEquipItem(effect, source, sourceID, target);
    case CGAMEEFFECT_APPLYEFFECTEQUIPITEMTYPE:
        return new CGameEffectApplyEffectEquipItemType(effect, source, sourceID, target);
    case CGAMEEFFECT_SETDONOJUMP:
        return new CGameEffectSetDoNoJump(effect, source, sourceID, target);
    case CGAMEEFFECT_HOLDCREATURESPELL_185:
        return new CGameEffectHoldCreatureSpell(effect, source, sourceID, target);
    case CGAMEEFFECT_JUMPTOAREA:
        return new CGameEffectJumpToArea(effect, source, sourceID, target);
    case CGAMEEFFECT_SETLOCALVARIABLE:
        return new CGameEffectSetLocalVariable(effect, source, sourceID, target);
    case CGAMEEFFECT_AURACLEANSING:
        return new CGameEffectAuraCleansing(effect, source, sourceID, target);
    case CGAMEEFFECT_MENTALSPEED:
        return new CGameEffectMentalSpeed(effect, source, sourceID, target);
    case CGAMEEFFECT_PHYSICALSPEED:
        return new CGameEffectPhysicalSpeed(effect, source, sourceID, target);
    case CGAMEEFFECT_CASTINGLEVELBONUS:
        return new CGameEffectCastingLevelBonus(effect, source, sourceID, target);
    case CGAMEEFFECT_FINDFAMILIAR:
        return new CGameEffectFindFamiliar(effect, source, sourceID, target);
    case CGAMEEFFECT_SEEINVISIBLE:
        return new CGameEffectSeeInvisible(effect, source, sourceID, target);
    case CGAMEEFFECT_IGNOREDIALOGPAUSE:
        return new CGameEffectIgnoreDialogPause(effect, source, sourceID, target);
    case CGAMEEFFECT_HITPOINTSONDEATH:
        return new CGameEffectHitPointsOnDeath(effect, source, sourceID, target);
    case CGAMEEFFECT_FAMILIARBLOCK:
        return new CGameEffectFamiliarBlock(effect, source, sourceID, target);
    case CGAMEEFFECT_BOUNCEPROJECTILE:
        return new CGameEffectBounceProjectile(effect, source, sourceID, target);
    case CGAMEEFFECT_BOUNCEEFFECT:
        return new CGameEffectBounceEffect(effect, source, sourceID, target);
    case CGAMEEFFECT_BOUNCELEVEL:
        return new CGameEffectBounceLevel(effect, source, sourceID, target);
    case CGAMEEFFECT_BOUNCELEVELDECREMENT:
        return new CGameEffectBounceLevelDecrement(effect, source, sourceID, target);
    case CGAMEEFFECT_IMMUNITYLEVELDECREMENT:
        return new CGameEffectImmunityLevelDecrement(effect, source, sourceID, target);
    case CGAMEEFFECT_BOUNCESCHOOL:
        return new CGameEffectBounceSchool(effect, source, sourceID, target);
    case CGAMEEFFECT_BOUNCESECONDARYTYPE:
        return new CGameEffectBounceSecondaryType(effect, source, sourceID, target);
    case CGAMEEFFECT_IMMUNITYSCHOOL:
        return new CGameEffectImmunitySchool(effect, source, sourceID, target);
    case CGAMEEFFECT_IMMUNITYSECONDARYTYPE:
        return new CGameEffectImmunitySecondaryType(effect, source, sourceID, target);
    case CGAMEEFFECT_IMMUNITYSPELL:
        return new CGameEffectImmunitySpell(effect, source, sourceID, target);
    case CGAMEEFFECT_BOUNCESPELL:
        return new CGameEffectBounceSpell(effect, source, sourceID, target);
    case CGAMEEFFECT_MINHITPOINTS:
        return new CGameEffectMinHitPoints(effect, source, sourceID, target);
    case CGAMEEFFECT_POWERWORDKILL:
        return new CGameEffectPowerWordKill(effect, source, sourceID, target);
    case CGAMEEFFECT_POWERWORDSTUN:
        return new CGameEffectPowerWordStun(effect, source, sourceID, target);
    case CGAMEEFFECT_IMPRISONMENT:
        return new CGameEffectImprisonment(effect, source, sourceID, target);
    case CGAMEEFFECT_FREEDOM:
        return new CGameEffectFreedom(effect, source, sourceID, target);
    case CGAMEEFFECT_MAZE:
        return new CGameEffectMaze(effect, source, sourceID, target);
    case CGAMEEFFECT_SECONDARYCASTLIST:
        return new CGameEffectSecondaryCastList(effect, source, sourceID, target);
    case CGAMEEFFECT_VISUALEFFECT:
        return new CGameEffectVisualEffect(effect, source, sourceID, target);
    case CGAMEEFFECT_LEVELDRAIN:
        return new CGameEffectLevelDrain(effect, source, sourceID, target);
    case CGAMEEFFECT_POWERWORDSLEEP:
        return new CGameEffectPowerWordSleep(effect, source, sourceID, target);
    case CGAMEEFFECT_STONESKINS:
        return new CGameEffectStoneSkins(effect, source, sourceID, target);
    case CGAMEEFFECT_PROTECTIONCIRCLE:
        return new CGameEffectProtectionCircle(effect, source, sourceID, target);
    case CGAMEEFFECT_DISPELSCHOOL:
        return new CGameEffectDispelSchool(effect, source, sourceID, target);
    case CGAMEEFFECT_DISPELSECONDARYTYPE:
        return new CGameEffectDispelSecondaryType(effect, source, sourceID, target);
    case CGAMEEFFECT_RANDOMTELEPORT:
        return new CGameEffectRandomTeleport(effect, source, sourceID, target);
    case CGAMEEFFECT_IMMUNITYSCHOOLDECREMENT:
        return new CGameEffectImmunitySchoolDecrement(effect, source, sourceID, target);
    case CGAMEEFFECT_LEVELRESTORATION:
        return new CGameEffectLevelRestoration(effect, source, sourceID, target);
    case CGAMEEFFECT_REVEALMAGIC:
        return new CGameEffectRevealMagic(effect, source, sourceID, target);
    case CGAMEEFFECT_SECONDARYTYPEDECREMENT:
        return new CGameEffectSecondaryTypeDecrement(effect, source, sourceID, target);
    case CGAMEEFFECT_BOUNCESCHOOLDECREMENT:
        return new CGameEffectBounceSchoolDecrement(effect, source, sourceID, target);
    case CGAMEEFFECT_BOUNCESECONDARYTYPEDECREMENT:
        return new CGameEffectBounceSecondaryTypeDecrement(effect, source, sourceID, target);
    case CGAMEEFFECT_DISPELSCHOOLONE:
        return new CGameEffectDispelSchoolOne(effect, source, sourceID, target);
    case CGAMEEFFECT_DISPELSECONDARYTYPEONE:
        return new CGameEffectDispelSecondaryTypeOne(effect, source, sourceID, target);
    case CGAMEEFFECT_TIMESTOP:
        return new CGameEffectTimeStop(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_COLORGLOWDISSIPATE:
        return new IcewindCGameEffectColorGlowDissipate(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_VISUALSPELLHIT:
        return new IcewindCGameEffectVisualSpellHit(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_SNILLOCSSNOWBALLSWARMDAMAGE:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 1329
        UTIL_ASSERT_MSG(FALSE, "IcewindCGameEffectSnillocsSnowballSwarmDamage game effect not supported in Icewind II (-rjf)");
    case ICEWIND_CGAMEEFFECT_CASTINGGLOW:
        return new IcewindCGameEffectCastingGlow(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_PANIC:
        return new CGameEffectPanic(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_MAGICALSTONEDAMAGE:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 1340
        UTIL_ASSERT_MSG(FALSE, "IcewindCGameEffectMagicalStoneDamage game effect not supported in Icewind II (-rjf)");
    case ICEWIND_CGAMEEFFECT_SAVINGTHROWCHANGE:
        return new IcewindCGameEffectSavingThrowChange(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_SLOWPOISON:
        return new IcewindCGameEffectSlowPoison(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_SUMMONINGSPELL:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 1350
        UTIL_ASSERT_MSG(FALSE, "IcewindCGameEffectSummoningSpell game effect not supported in Icewind II (-rjf)");
    case ICEWIND_CGAMEEFFECT_VAMPIRICTOUCH:
        return new IcewindCGameEffectVampiricTouch(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_CREATUREAFFECTEDANIMATION:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 1357
        UTIL_ASSERT_MSG(FALSE, "IcewindCGameEffectCreatureAffectedAnimation game effect not supported in Icewind II (-rjf)");
    case ICEWIND_CGAMEEFFECT_ANIMATEDEAD:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 1361
        UTIL_ASSERT_MSG(FALSE, "IcewindCGameEffectAnimateDead game effect not supported in Icewind II (-rjf)");
    case ICEWIND_CGAMEEFFECT_PRAYER:
        return new IcewindCGameEffectPrayer(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_PRAYERBAD:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 1368
        UTIL_ASSERT_MSG(FALSE, "IcewindCGameEffectPrayerBad game effect not supported in Icewind II (-rjf)");
    case ICEWIND_CGAMEEFFECT_SUMMONINGSPELLMIXED:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 1372
        UTIL_ASSERT_MSG(FALSE, "IcewindCGameEffectSummoningSpellMixed game effect not supported in Icewind II (-rjf)");
    case ICEWIND_CGAMEEFFECT_BELTYNSBURNINGBLOOD:
        return new IcewindCGameEffectBeltynsBurningBlood(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_SHADOWMONSTERS:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 1379
        UTIL_ASSERT_MSG(FALSE, "IcewindCGameEffectShadowMonsters game effect not supported in Icewind II (-rjf)");
    case ICEWIND_CGAMEEFFECT_RECITATION:
        return new IcewindCGameEffectRecitation(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_RECITATIONBAD:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 1386
        UTIL_ASSERT_MSG(FALSE, "IcewindCGameEffectRecitationBad game effect not supported in Icewind II (-rjf)");
    case ICEWIND_CGAMEEFFECT_LICHTOUCH:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 1390
        UTIL_ASSERT_MSG(FALSE, "IcewindCGameEffectLichTouch game effect not supported in Icewind II (-rjf)");
    case ICEWIND_CGAMEEFFECT_SOLSSEARINGORB:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 1394
        UTIL_ASSERT_MSG(FALSE, "IcewindCGameEffectSolsSearingOrb game effect not supported in Icewind II (-rjf)");
    case ICEWIND_CGAMEEFFECT_OILOFSERPENTSSCALES:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 1398
        UTIL_ASSERT_MSG(FALSE, "Oil of the Serpent's Scales game effect not supported in Icewind II (-rjf)");
    case ICEWIND_CGAMEEFFECT_REMOVEEFFECTOFRESOURCE:
        return new IcewindCGameEffectRemoveEffectOfResource(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_SALAMANDERAURA:
        return new IcewindCGameEffectSalamanderAura(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_UMBERHULKGAZE:
        return new IcewindCGameEffectUmberHulkGaze(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_ZOMBIELORDAURA:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 1411
        UTIL_ASSERT_MSG(FALSE, "Zombie Lord Aura game effect not supported in Icewind II (-rjf)");
        (effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_IMMUNITYTORESOURCE:
        return new IcewindCGameEffectImmunityToResource(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_SUMMONCREATURE:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 1418
        UTIL_ASSERT_MSG(FALSE, "IcewindCGameEffectSummonCreature game effect not supported in Icewind II (-rjf)");
        (effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_ANIMATIONREMOVE:
        return new IcewindCGameEffectAnimationRemove(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_IMMUNITYTOEFFECTRESOURCE:
        return new IcewindCGameEffectImmunityToEffectAndResource(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_SUMMONPOMABS:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 1428
        UTIL_ASSERT_MSG(FALSE, "IcewindCGameEffectSummonPomabs game effect not supported in Icewind II (-rjf)");
        (effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_DOMINATE:
        return new IcewindCGameEffectDominate(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_STATICCHARGE:
        return new IcewindCGameEffectStaticCharge(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_CLOAKOFFEAR:
        return new IcewindCGameEffectCloakOfFear(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_FORCEMOVEMENTRATE:
        if (1) {
            IcewindCGameEffectForceMovementRate* pEffect = new IcewindCGameEffectForceMovementRate(effect, source, sourceID, target);
            pEffect->SetEffectID(ICEWIND_CGAMEEFFECT_FORCEMOVEMENTRATE);
            return pEffect;
        }
    case ICEWIND_CGAMEEFFECT_REMOVECONFUSION:
        return new IcewindCGameEffectRemoveConfusion(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_EYEOFMIND:
        return new IcewindCGameEffectEyeOfMind(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_EYEYOFSWORD:
        return new IcewindCGameEffectEyeOfSword(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_EYEOFMAGE:
        return new IcewindCGameEffectEyeOfMage(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_EYEOFVENOM:
        return new IcewindCGameEffectEyeOfVenom(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_EYEOFSPIRIT:
        return new IcewindCGameEffectEyeOfSpirit(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_EYEOFFORTITUDE:
        return new IcewindCGameEffectEyeOfFortitude(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_EYEOFSTONE:
        return new IcewindCGameEffectEyeOfStone(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_REMOVESEVENEYES:
        return new IcewindCGameEffectRemoveSevenEyes(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_REMOVEALLOFTYPE:
        return new IcewindCGameEffectRemoveAllOfType(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_SOULEATER:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 1477
        UTIL_ASSERT_MSG(FALSE, "IcewindCGameEffectSoulEater game effect not supported in Icewind II (-rjf)");
        (effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_SHROUDOFFLAME:
        return new IcewindCGameEffectShroudOfFlame(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_ANIMALRAGE:
        return new IcewindCGameEffectAnimalRage(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_TURNUNDEAD:
        return new IcewindCGameEffectTurnUndead(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_VITRIOLICSPHERE:
        return new IcewindCGameEffectVitriolicSphere(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_SUPPRESSHPDISPLAY:
        return new IcewindCGameEffectSuppressHPDisplay(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_FLOATTEXT:
        return new IcewindCGameEffectFloatText(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_MACEOFDISRUPTION:
        return new IcewindCGameEffectMaceOfDisruption(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_SLEEP:
        if (1) {
            CGameEffectSleep* pEffect = new CGameEffectSleep(effect, source, sourceID, target);
            pEffect->SetEffectID(ICEWIND_CGAMEEFFECT_SLEEP);
            return pEffect;
        }
    case ICEWIND_CGAMEEFFECT_RANGERTRACKING:
        return new IcewindCGameEffectRangerTracking(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_IMMUNITYTOBACKSTAB:
        return new IcewindCGameEffectImmunityToBackstab(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_SETSPELLSTATE:
        return new IcewindCGameEffectSetSpellState(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_DRAGONGEMCUTSCENE:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 1515
        UTIL_ASSERT_MSG(FALSE, "IcewindCGameEffectDragonGemCutscene game effect not supported in Icewind II (-rjf)");
        (effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_IMMUNITYSPELL:
        if (1) {
            CGameEffectImmunitySpell* pEffect = new CGameEffectImmunitySpell(effect, source, sourceID, target);
            pEffect->SetEffectID(ICEWIND_CGAMEEFFECT_IMMUNITYSPELL);
            return pEffect;
        }
    case ICEWIND_CGAMEEFFECT_RODOFSMITING:
        return new IcewindCGameEffectRodOfSmiting(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_MAGICALREST:
        return new IcewindCGameEffectMagicalRest(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_BEHOLDERDISPELMAGIC:
        return new IcewindCGameEffectBeholderDispelMagic(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_HARPYWAIL:
        return new IcewindCGameEffectHarpyWail(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_JACKALWEREGAZE:
        return new IcewindCGameEffectJackalwereGaze(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_SETGLOBAL:
        return new IcewindCGameEffectSetGlobal(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_HIDEINSHADOWS:
        return new CGameEffectHideInShadows(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_SKILLUSEMAGICDEVICE:
        return new CGameEffectSkillUseMagicDevice(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_HOPELESSNESS:
        return new IcewindCGameEffectHopelessness(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_PROTECTIONFROMEVIL:
        return new IcewindCGameEffectProtectionFromEvil(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_APPLYEFFECTSLIST:
        if (1) {
            IcewindCGameEffectApplyEffectsList* pEffect = new IcewindCGameEffectApplyEffectsList(effect, source, sourceID, target);
            pEffect->sub_493400(TRUE);
            return pEffect;
        }
    case ICEWIND_CGAMEEFFECT_ARMOROFFAITH:
        return new IcewindCGameEffectArmorOfFaith(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_NAUSEA:
        return new IcewindCGameEffectNausea(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_ENFEEBLEMENT:
        return new IcewindCGameEffectEnfeeblement(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_FIRESHIELD:
        return new IcewindCGameEffectFireshield(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_DEATHWARD:
        return new IcewindCGameEffectDeathWard(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_HOLYPOWER:
        return new IcewindCGameEffectHolyPower(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_RIGHTEOUSWRATHOFFAITHFUL:
        return new IcewindCGameEffectRighteousWrathOfFaithful(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_SUMMONALLY:
        if (1) {
            IcewindCGameEffectSummon* pEffect = new IcewindCGameEffectSummon(effect, source, sourceID, target);
            pEffect->sub_49DB60(1);
            return pEffect;
        }
    case ICEWIND_CGAMEEFFECT_SUMMONENEMY:
        if (1) {
            IcewindCGameEffectSummon* pEffect = new IcewindCGameEffectSummon(effect, source, sourceID, target);
            pEffect->sub_49DB60(2);
            return pEffect;
        }
    case ICEWIND_CGAMEEFFECT_CHARM:
        if (1) {
            CGameEffectCharm* pEffect = new CGameEffectCharm(effect, source, sourceID, target);
            pEffect->SetEffectID(ICEWIND_CGAMEEFFECT_CHARM);
            return pEffect;
        }
    case ICEWIND_CGAMEEFFECT_VISUALEFFECT:
        return new IcewindCGameEffectVisualEffect(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_OTILUKESRESILIENTSPHERE:
        return new IcewindCGameEffectOtilukesResilientSphere(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_BARKSKIN:
        return new IcewindCGameEffectBarkskin(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_BLEEDINGWOUNDS:
        return new IcewindCGameEffectBleedingWounds(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_AREAOFEFFECT:
        return new IcewindCGameEffectAreaOfEffect(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_FREEACTION:
        return new IcewindCGameEffectFreeAction(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_UNCONSCIOUSNESS:
        return new IcewindCGameEffectUnconsciousness(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_MAGIC_DEATH:
        if (1) {
            CGameEffectDeath* pEffect = new CGameEffectDeath(effect, source, sourceID, target);
            pEffect->SetEffectID(ICEWIND_CGAMEEFFECT_MAGIC_DEATH);
            return pEffect;
        }
    case ICEWIND_CGAMEEFFECT_ENTROPYSHIELD:
        return new IcewindCGameEffectEntropyShield(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_STORMSHELL:
        return new IcewindCGameEffectStormShell(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_PROTECTIONFROMELEMENTS:
        return new IcewindCGameEffectProtectionFromElements(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_HOLDUNDEAD:
        if (1) {
            CGameEffectHoldCreature* pEffect = new CGameEffectHoldCreature(effect, source, sourceID, target);
            pEffect->SetEffectID(ICEWIND_CGAMEEFFECT_HOLDUNDEAD);
            return pEffect;
        }
    case ICEWIND_CGAMEEFFECT_CONTROLUNDEAD:
        if (1) {
            CGameEffectCharm* pEffect = new CGameEffectCharm(effect, source, sourceID, target);
            pEffect->SetEffectID(ICEWIND_CGAMEEFFECT_CONTROLUNDEAD);
            pEffect->SetDWFlags(0x1);
            return pEffect;
        }
    case ICEWIND_CGAMEEFFECT_AEGIS:
        return new IcewindCGameEffectAegis(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_EXECUTIONERSEYES:
        return new IcewindCGameEffectExecutionersEyes(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_BANISH:
        if (1) {
            CGameEffectDeath* pEffect = new CGameEffectDeath(effect, source, sourceID, target);
            pEffect->SetEffectID(ICEWIND_CGAMEEFFECT_MAGIC_DEATH);
            pEffect->SetDWFlags(0x800);
            return pEffect;
        }
    case ICEWIND_CGAMEEFFECT_WHENSTRUCK:
        return new IcewindCGameEffectWhenStruck(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_PROJECTILE:
        return new IcewindCGameEffectProjectile(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_ENERGYDRAIN:
        return new IcewindCGameEffectEnergyDrain(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_TORTOISESHELL:
        return new IcewindCGameEffectTortoiseShell(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_BLINK:
        return new IcewindCGameEffectBlink(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_PERSISTANTEFFECT:
        return new IcewindCGameEffectPersistantEffect(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_DAYBLINDNESS:
        return new CGameEffectDayBlindness(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_DAMAGEREDUCTION:
        return new IcewindCGameEffectDamageReduction(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_DISGUISE:
        return new CGameEffectDisguise(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_HEROICINSPIRATION:
        return new CGameEffectHeroicInspiration(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_PREVENTAISLOWDOWN:
        return new CGameEffectPreventAISlowdown(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_BARBARIANRAGE:
        return new CGameEffectBarbarianRage(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_MOVEMENTRATEWITHPORTRAIT:
        return new CGameEffectMovementRateWithPortrait(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_FEATCLEAVE:
        return new CGameEffectFeatCleave(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_PROTECTIONFORMARROWS:
        return new IcewindCGameEffectProtectionFromArrows(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_TENSERSTRANSFORMATION:
        return new IcewindCGameEffectTensersTransformation(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_SLIPPERYMIND:
        return new CGameEffectSlipperyMind(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_SMITEEVIL:
        return new IcewindCGameEffectSmiteEvil(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_RESTORATION:
        return new IcewindCGameEffectRestoration(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_ALICORNLANCEGLOW:
        return new IcewindCGameEffectAlicornLanceGlow(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_CALLLIGHTNING:
        return new IcewindCGameEffectCallLightning(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_GLOBEOFINVULNERABILITY:
        return new IcewindCGameEffectGlobeOfInvulnerability(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_LOWERRESISTANCE:
        return new IcewindCGameEffectLowerResistance(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_BANE:
        return new IcewindCGameEffectBane(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_FEATPOWERATTACK:
        return new IcewindCGameEffectFeatPowerAttack(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_FEATEXPERTISE:
        return new IcewindCGameEffectFeatExpertise(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_FEATARTERIALSTRIKE:
        return new IcewindCGameEffectFeatArterialStrike(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_FEATHAMSTRING:
        return new IcewindCGameEffectFeatHamstring(effect, source, sourceID, target);
    case ICEWIND_CGAMEEFFECT_FEATRAPIDSHOT:
        return new IcewindCGameEffectFeatRapidShot(effect, source, sourceID, target);
    }

    CString sError;
    sError.Format("Unknown effect id:  %d", effect->effectID);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
    // __LINE__: 1744
    UTIL_ASSERT_MSG(FALSE, (LPCSTR)sError);
}

// 0x452C80
CGameEffect::CGameEffect()
{
    m_sourceID = -1;
    m_done = FALSE;
    m_forceRepass = FALSE;
    field_118 = 0;
    m_compareIdOnly = FALSE;
    m_compareIdAndFlagsOnly = FALSE;
    m_secondaryType = 1;
    field_188 = 0;
}

// 0x48C310
CGameEffect::CGameEffect(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target, BOOL usesDice)
{
    m_effectID = effect->effectID;
    m_targetType = effect->targetType;
    m_spellLevel = effect->spellLevel;
    m_effectAmount = effect->effectAmount;
    m_dwFlags = effect->dwFlags;

    if ((effect->durationType & 0x100) != 0) {
        m_flags |= 0x1;
    }

    if ((effect->durationType & 0x200) != 0) {
        m_flags |= 0x2;
    }

    m_durationType = effect->durationType & ~0x300;
    m_duration = effect->duration;
    m_probabilityUpper = effect->probabilityUpper;
    m_probabilityLower = effect->probabilityLower;

    m_res = effect->res;

    if (usesDice) {
        m_numDice = effect->numDice;
        m_diceSize = effect->diceSize;
    } else {
        m_minLevel = effect->diceSize;
        m_maxLevel = effect->numDice;
    }

    m_savingThrow = effect->savingThrow;
    m_saveMod = effect->saveMod;
    m_special = effect->special;

    m_source = source;
    m_sourceID = sourceID;
    m_target = target;
    m_done = FALSE;
    m_compareIdOnly = FALSE;
    m_compareIdAndFlagsOnly = FALSE;
    m_forceRepass = FALSE;
    field_188 = 0;
    field_118 = m_duration;

    // NOTE: Object offset +0xCC, the first-call flag the effects in
    // IcewindCGameEffects.cpp test as m_secondaryType.
    m_secondaryType = 1;
}

// 0x499B10
CGameEffect::~CGameEffect()
{
}

// 0x452E20
CGameEffect* CGameEffect::Copy()
{
    // __FILE__: .\Include\CGameEffect.h
    // __LINE__: 496
    UTIL_ASSERT(FALSE);
}

// 0x452E40
BOOL CGameEffect::ApplyEffect(CGameSprite* pSprite)
{
    // __FILE__: .\Include\CGameEffect.h
    // __LINE__: 565
    UTIL_ASSERT(FALSE);
}

// 0x4A3030
BOOL CGameEffect::ResolveEffect(CGameSprite* pSprite)
{
    // NOTE: Inlined CAIObjectType::Equal.
    if (pSprite->GetAIType().Equal(CAIObjectType::NOT_SPRITE)) {
        return FALSE;
    }

    ULONG nGameTime = g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime;

    switch (m_durationType) {
    case 0x1000:
        if (m_duration <= nGameTime) {
            m_done = TRUE;
            OnRemove(pSprite);
            return TRUE;
        }
        break;
    case 0:
        m_durationType = 0x1000;
        m_duration = nGameTime + m_duration * 15;
        break;
    case 3:
        m_durationType = 6;
        m_duration = nGameTime + m_duration * 15;
        return TRUE;
    case 4:
        m_durationType = 7;
        m_duration = nGameTime + m_duration * 15;
        return TRUE;
    case 5:
        m_durationType = 8;
        m_duration = nGameTime + m_duration * 15;
        return TRUE;
    case 6:
        if (nGameTime < m_duration) {
            return TRUE;
        }
        m_durationType = 0x1000;
        m_duration = nGameTime + field_118 * 15;
        sub_4A3FF0(pSprite);
        DisplayString(pSprite);
        break;
    case 7:
        if (nGameTime < m_duration) {
            return TRUE;
        }
        m_durationType = 1;
        sub_4A3FF0(pSprite);
        DisplayString(pSprite);
        break;
    case 8:
        if (nGameTime < m_duration) {
            return TRUE;
        }
        m_durationType = 2;
        sub_4A3FF0(pSprite);
        DisplayString(pSprite);
        break;
    }

    BOOL bResult = ApplyEffect(pSprite);

    // NOTE: Object offset +0xCC, the first-call flag the effects in
    // IcewindCGameEffects.cpp test as m_secondaryType.
    m_secondaryType = 0;

    return bResult;
}

// 0x799E60
void CGameEffect::OnAddSpecific(CGameSprite* pSprite)
{
    // NOTE: The vtable's slot +0x10 is the shared empty `ret 4` that
    // identical code folding made of OnLoad and this slot.
}

// 0x799E60
void CGameEffect::OnLoad(CGameSprite* pSprite)
{
}

// 0x78E6E0
BOOL CGameEffect::UsesDice()
{
    return FALSE;
}

// 0x4A2E40
void CGameEffect::PlaySound(const CResRef& soundResRef, CGameSprite* pSprite)
{
    m_sound.Stop();

    CMessage* message = new CMessagePlaySoundRef(soundResRef,
        pSprite->GetId(),
        pSprite->GetId());
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

    m_sound.SetResRef(soundResRef, TRUE, TRUE);

    if (m_sound.m_nLooping == 0) {
        m_sound.SetFireForget(TRUE);
    }

    m_sound.SetChannel(14, reinterpret_cast<DWORD_PTR>(pSprite->GetArea()));
    m_sound.Play(pSprite->GetPos().x, pSprite->GetPos().y, 0, FALSE);
}

// 0x4A3270
BOOL CGameEffect::CheckExpiration()
{
    switch (m_durationType) {
    case 0:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 1990
        UTIL_ASSERT(FALSE);
    case 6:
    case 7:
    case 8:
    case 0x1000:
        return g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime >= m_duration;
    }
    return FALSE;
}

// 0x4A4B00
ITEM_EFFECT* CGameEffect::GetItemEffect()
{
    ITEM_EFFECT* effect = new ITEM_EFFECT;
    effect->effectID = static_cast<WORD>(m_effectID);
    effect->targetType = static_cast<BYTE>(m_targetType);
    effect->spellLevel = static_cast<BYTE>(m_spellLevel);
    effect->effectAmount = m_effectAmount;
    effect->dwFlags = m_dwFlags;
    effect->durationType = static_cast<WORD>(m_durationType);

    if ((m_flags & 0x1) != 0) {
        effect->durationType |= 0x100;
    }

    if ((m_flags & 0x2) != 0) {
        effect->durationType |= 0x200;
    }

    effect->duration = m_duration;
    effect->probabilityUpper = static_cast<BYTE>(m_probabilityUpper);
    effect->probabilityLower = static_cast<BYTE>(m_probabilityLower);
    m_res.GetResRef(effect->res);

    if (UsesDice()) {
        effect->numDice = m_numDice;
        effect->diceSize = m_diceSize;
    } else {
        effect->numDice = m_maxLevel;
        effect->diceSize = m_minLevel;
    }

    effect->savingThrow = m_savingThrow;
    effect->saveMod = m_saveMod;
    effect->special = m_special;

    return effect;
}

// 0x4A4BB0
void CGameEffect::DisplayString(CGameSprite* pSprite)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if ((pGame->GetOptions()->m_nEffectTextLevel & 0x8) != 0
        && pGame->m_bInLoadGame != TRUE
        && pGame->m_bInLoadArea != TRUE) {
        CString sEffectID;
        sEffectID.Format("%d", m_effectID);

        STRREF str = atol(pGame->GetRuleTables().m_tEffectText.GetAt(CRuleTables::STRREF_STR, sEffectID));
        if (str != -1) {
            g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(pSprite->GetNameRef(),
                str,
                CVidPalette::RANGE_COLORS[pSprite->GetBaseStats()->m_colors[2]],
                RGB(0xD7, 0xD7, 0xBE),
                -1,
                pSprite->GetId(),
                pSprite->GetId());
        }
    }
}

// 0x4A51D0
void CGameEffect::OnRemove(CGameSprite* pSprite)
{
}

// 0x4A4D10
void CGameEffect::DisplayStringRef(CGameSprite* pSprite, STRREF str)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if ((pGame->GetOptions()->m_nEffectTextLevel & 0x8) != 0
        && pGame->m_bInLoadGame != TRUE
        && pGame->m_bInLoadArea != TRUE) {
        g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(pSprite->GetNameRef(),
            str,
            CVidPalette::RANGE_COLORS[pSprite->GetBaseStats()->m_colors[2]],
            RGB(0xD7, 0xD7, 0xBE),
            -1,
            pSprite->GetId(),
            pSprite->GetId());
    }
}

// 0x4A4D80
void CGameEffect::sub_4A4D80(CGameSprite* pSprite, INT nAmount)
{
    if (nAmount < 1) {
        return;
    }

    CString sText;
    LONG id = pSprite->GetId();

    // NOTE: The original formats the number with _itoa(nAmount, buf, 10).
    CString sAmount;
    sAmount.Format("%d", nAmount);
    g_pBaldurChitin->GetTlkTable().SetToken(CString("HIT_POINTS"), sAmount);

    if (nAmount == 1) {
        sText = CBaldurEngine::FetchString(8667);
    } else if (nAmount >= 2) {
        sText = CBaldurEngine::FetchString(8666);
    }

    g_pBaldurChitin->GetBaldurMessage()->DisplayText(pSprite->GetName(),
        sText,
        CVidPalette::RANGE_COLORS[pSprite->GetBaseStats()->m_colors[2]],
        RGB(0xD7, 0xD7, 0xBE),
        -1,
        id,
        id);
}

// 0x4A4EF0
void CGameEffect::AddColorEffect(CGameSprite* pSprite, BYTE r, BYTE g, BYTE b, BYTE periodLength)
{
    pSprite->m_hasColorEffects = 1;

    COLORREF tintColor = RGB(r, g, b);

    // NOTE: Uninline.
    pSprite->GetAnimation()->SetColorEffectAll(1, tintColor, periodLength);

    CColorEffect* pColorEffect = new CColorEffect;
    pColorEffect->m_effectType = 1;
    pColorEffect->m_range = -1;
    pColorEffect->m_tintColor = tintColor;
    pColorEffect->m_periodLength = periodLength;
    pSprite->GetDerivedStats()->m_appliedColorEffects.AddTail(pColorEffect);
}

// 0x4A4FA0
void CGameEffect::AddColorEffect(CGameSprite* pSprite, BYTE r, BYTE g, BYTE b)
{
    AddColorEffect(pSprite, r, g, b, 1);
}

// 0x4A3310
BOOL CGameEffect::sub_4A3310(CGameSprite* pSprite, BYTE* pRoll1, BYTE* pRoll2, BYTE* pRoll3, BYTE* pRoll4, BYTE* pProbabilityRoll)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    if (((pSprite->m_baseStats.m_flags & 0x20000) != 0
            && ((m_sourceFlags & 0x400) != 0
                || m_effectID == 12
                || m_effectID == 13
                || m_effectID == 3
                || m_effectID == 5
                || m_effectID == 38
                || m_effectID == 39
                || m_effectID == 55
                || m_effectID == 58
                || m_effectID == 60
                || m_effectID == 72
                || m_effectID == 74
                || m_effectID == 76
                || m_effectID == 40
                || m_effectID == 45
                || m_effectID == 78
                || m_effectID == 80
                || m_effectID == 82
                || m_effectID == 110
                || m_effectID == 128
                || m_effectID == 109
                || m_effectID == 134
                || m_effectID == 135
                || m_effectID == 136
                || m_effectID == 137
                || m_effectID == 154
                || m_effectID == 157
                || m_effectID == 158
                || m_effectID == 160
                || m_effectID == 165
                || m_effectID == 168
                || m_effectID == 172
                || m_effectID == 177
                || m_effectID == 255
                || m_effectID == 256
                || m_effectID == 257
                || m_effectID == 263
                || m_effectID == 241
                || m_effectID == 245
                || m_effectID == 247
                || m_effectID == 250
                || m_effectID == 264
                || m_effectID == 277
                || m_effectID == 278
                || m_effectID == 279
                || m_effectID == 280
                || m_effectID == 281
                || m_effectID == 282
                || m_effectID == 284
                || m_effectID == 285
                || m_effectID == 293
                || m_effectID == 294
                || m_effectID == 295
                || m_effectID == 400
                || m_effectID == 404
                || m_effectID == 405
                || m_effectID == 412
                || m_effectID == 414
                || m_effectID == 416
                || m_effectID == 419
                || m_effectID == 420
                || m_effectID == 424
                || m_effectID == 425
                || m_effectID == 428
                || m_effectID == 431
                || m_effectID == 25
                || m_effectID == 24
                || m_effectID == 435))
        || (m_effectAmount < 0 && (m_effectID == 18 || m_effectID == 23))
        || (m_effectAmount > 0 && (m_effectID == 93 || m_effectID == 94))) {
        if (m_sourceID == CGameObjectArray::INVALID_INDEX) {
            return FALSE;
        }

        // The effect is refused, but the target still knows it was attacked.
        CGameObject* pSource;
        BYTE rc;
        do {
            rc = pGame->GetObjectArray()->GetShare(m_sourceID,
                CGameObjectArray::THREAD_ASYNCH,
                &pSource,
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc != CGameObjectArray::SUCCESS) {
            return FALSE;
        }

        if ((pSprite->field_58C & 0x200000) == 0
            && pSource->GetAIType().m_nEnemyAlly <= CAIObjectType::EA_GOODCUTOFF) {
            pSprite->field_58C |= 0x200000;

            pSprite->sub_6F50A0();
        }

        CAITrigger hitBy(CAITRIGGER_HITBY, pSource->GetAIType(), 0);
        CMessage* message = new CMessageSetTrigger(hitBy, m_sourceID, pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

        CAITrigger attackedBy(CAITRIGGER_ATTACKEDBY, pSource->GetAIType(), 0);
        message = new CMessageSetTrigger(attackedBy, m_sourceID, pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

        pGame->GetObjectArray()->ReleaseShare(m_sourceID,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);

        return FALSE;
    }

    // NOTE: The original draws a number here and discards it (0x4A38A6).
    rand();

    if (m_probabilityUpper < *pProbabilityRoll) {
        return FALSE;
    }

    if (*pProbabilityRoll < m_probabilityLower) {
        return FALSE;
    }

    std::string sSourceRes(static_cast<const char*>(m_sourceRes.GetResRefStr()));
    if (pSprite->m_derivedStats.m_resourceImmunities.find(sSourceRes) != pSprite->m_derivedStats.m_resourceImmunities.end()) {
        return FALSE;
    }

    if (CSevenEyes::CheckEyeOfStone(pSprite, this) == TRUE
        || CSevenEyes::CheckEyeOfSpirit(pSprite, this) == TRUE
        || CSevenEyes::CheckEyeOfMind(pSprite, this) == TRUE
        || CSevenEyes::CheckEyeOfFortitude(pSprite, this) == TRUE
        || CSevenEyes::CheckEyeOfVenom(pSprite, this) == TRUE
        || CSevenEyes::CheckEyeOfMage(pSprite, this) == TRUE
        || CSevenEyes::CheckEyeOfSword(pSprite, this) == TRUE) {
        return FALSE;
    }

    BYTE nEnemyAlly = pSprite->GetAIType().m_nEnemyAlly;
    const BYTE* pSourceRes = m_sourceRes.GetResRef();
    if (nEnemyAlly <= CAIObjectType::EA_GOODCUTOFF
        && pSourceRes[0] != '\0'
        && memcmp(pSourceRes, "SPWI420", 7) == 0
        && pGame->GetCharacterPortraitNum(pSprite->GetId()) != -1
        && (pSprite->m_derivedStats.m_generalState & STATE_DEAD) == 0
        && pSprite->m_derivedStats.GetClassLevel(CAIOBJECTTYPE_C_PALADIN) >= 2) {
        if (m_effectID == 24 && m_sourceRes.GetResRefStr().GetLength() != 0) {
            // NOTE: Inlined CSpell::CSpell.
            CSpell cSpell(m_sourceRes);
            pSprite->FeedBack(CGameSprite::FEEDBACK_IMMUNE_TO_RESOURCE, 0, 0, 0, cSpell.GetGenericName(), 0, 0);
        }

        // NOTE: The original releases a share it did not take here
        // (0x4A3B96).
        pGame->GetObjectArray()->ReleaseShare(m_sourceID,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        return FALSE;
    }

    if (pSprite->m_derivedStats.m_naturalImmunities.find(m_effectID) != pSprite->m_derivedStats.m_naturalImmunities.end()) {
        CGameObject* pSource;
        BYTE rc = pGame->GetObjectArray()->GetShare(m_sourceID,
            CGameObjectArray::THREAD_ASYNCH,
            &pSource,
            INFINITE);
        if (rc != CGameObjectArray::SUCCESS) {
            return FALSE;
        }

        if (m_sourceRes.GetResRefStr().GetLength() != 0) {
            FeedBackImmuneToResource(pSprite, m_sourceRes);

            CGameEffect* pImmunity = IcewindMisc::CreateEffectImmunitySpell(pSource,
                m_sourceRes.GetResRef(),
                0,
                0,
                0,
                2);
            pImmunity->m_casterLevel = m_casterLevel;
            pSprite->AddEffect(pImmunity, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);
        }

        pGame->GetObjectArray()->ReleaseShare(m_sourceID,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        return FALSE;
    }

    if (m_savingThrow != 0 && m_sourceID != CGameObjectArray::INVALID_INDEX) {
        CGameObject* pSource;
        BYTE rc;
        do {
            rc = pGame->GetObjectArray()->GetShare(m_sourceID,
                CGameObjectArray::THREAD_ASYNCH,
                &pSource,
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc != CGameObjectArray::SUCCESS) {
            return FALSE;
        }

        BYTE nClass = static_cast<BYTE>(m_casterLevel >> 8);
        if (nClass != 0 && pSource->GetObjectType() == CGameObject::TYPE_SPRITE) {
            const CRuleTables& ruleTables = pGame->GetRuleTables();
            DWORD nSpecialization = 0;
            if (nClass == CAIOBJECTTYPE_C_CLERIC) {
                nSpecialization = ruleTables.GetSpecializationMask(CAIOBJECTTYPE_C_CLERIC, static_cast<BYTE>(m_casterLevel >> 16));
            }

            m_saveMod += ruleTables.GetSaveAdjustmentForSourceSprite(static_cast<CGameSprite*>(pSource),
                m_sourceRes,
                m_savingThrow,
                nClass,
                nSpecialization,
                static_cast<BYTE>(m_casterLevel));
        }

        // NOTE: The CResRef at 0x8F8E60 is CGameSprite::SPIN108. The source's
        // type is not checked here.
        if (m_sourceRes == CGameSprite::SPIN108) {
            m_saveMod += pGame->GetRuleTables().sub_547620(1,
                static_cast<CGameSprite*>(pSource),
                m_saveMod,
                m_spellLevel);
        }

        pGame->GetObjectArray()->ReleaseShare(m_sourceID,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    if (!CheckSave(pSprite, pRoll1, pRoll2, pRoll3, pRoll4)) {
        return FALSE;
    }

    if (pSprite->m_derivedStats.m_cImmunitiesEffect.OnList(this)) {
        return FALSE;
    }

    BYTE nSpellLevel = static_cast<BYTE>(m_spellLevel);
    if (nSpellLevel < 9 && pSprite->m_derivedStats.m_cImmunitiesSpellLevel.m_levels[nSpellLevel]) {
        UINT nID = 0;
        if (m_spellLevel != 1) {
            return FALSE;
        }

        if (pGame->m_innateSpells.Find(m_res, nID) != TRUE
            && pGame->m_songs.Find(m_res, nID) != TRUE
            && pGame->m_shapeshifts.Find(m_res, nID) != TRUE) {
            return FALSE;
        }
    }

    if (!UsesDice()) {
        if (m_maxLevel != 0 && pSprite->m_derivedStats.GetClassMaskLevel(0xFFF) > m_maxLevel) {
            return FALSE;
        }

        if (m_minLevel > 0xFFFF) {
            m_minLevel = 0;
        }

        if (pSprite->m_derivedStats.GetClassMaskLevel(0xFFF) < m_minLevel) {
            return FALSE;
        }
    }

    if (m_durationType == 0x1000
        || m_durationType == 0
        || m_durationType == 1
        || m_durationType == 2
        || m_durationType == 9) {
        sub_4A3FF0(pSprite);
        DisplayString(pSprite);
    }

    return TRUE;
}

// 0x4A3FF0
void CGameEffect::sub_4A3FF0(CGameSprite* pSprite)
{
    if ((m_sourceFlags & 0x400) != 0 && m_sourceID != CGameObjectArray::INVALID_INDEX) {
        CGameObject* pSource;
        BYTE rc;
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_sourceID,
                CGameObjectArray::THREAD_ASYNCH,
                &pSource,
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc != CGameObjectArray::SUCCESS) {
            // NOTE: The original returns here without the vtable +0x10 call
            // (0x4A408E).
            return;
        }

        // A hostile effect from a party-side source makes the target's teams
        // the party's enemies.
        if ((pSprite->field_58C & 0x200000) == 0
            && pSource->GetAIType().m_nEnemyAlly <= CAIObjectType::EA_GOODCUTOFF) {
            pSprite->field_58C |= 0x200000;

            pSprite->sub_6F50A0();
        }

        CMessage* message;

        if (m_effectID == CGAMEEFFECT_DAMAGE) {
            CAITrigger trigger(CAITRIGGER_HITBY, pSource->GetAIType(), 0);
            message = new CMessageSetTrigger(trigger, m_sourceID, pSprite->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }

        CAITrigger trigger(CAITRIGGER_ATTACKEDBY, pSource->GetAIType(), 0);
        message = new CMessageSetTrigger(trigger, m_sourceID, pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceID,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    OnAddSpecific(pSprite);
}

// NOTE: An IcewindMisc helper by its address, not declared in IcewindMisc.h;
// kept here, file-static, until IcewindMisc.cpp's owner takes it. Only
// CheckSave calls it: the name of the item (identified) or spell (generic) a
// resref names, empty when the resref is empty or names neither.
// NOTE: Ruling M3-12: on the fold list; it moves into IcewindMisc.
// 0x5864F0
static CString IcewindMisc_GetResRefName5864F0(const CResRef& res)
{
    if (res.GetResRefStr().GetLength() == 0) {
        return CString("");
    }

    CString sName("");
    STR_RES strRes;
    if (g_pBaldurChitin->cDimm.m_cKeyTable.FindKey(res, 1005, FALSE)) {
        CItem cItem(res, 1, 0, 0, 0, 0);
        g_pBaldurChitin->GetTlkTable().Fetch(cItem.GetIdentifiedName(), strRes);
        sName = strRes.szText;
    } else if (g_pBaldurChitin->cDimm.m_cKeyTable.FindKey(res, 1006, FALSE)) {
        // NOTE: Inlined CSpell::CSpell.
        CSpell cSpell(res);
        g_pBaldurChitin->GetTlkTable().Fetch(cSpell.GetGenericName(), strRes);
        sName = strRes.szText;
    }

    return sName;
}

// NOTE: A CGameSprite member by its address (CGameSprite.cpp is another
// stream's); kept here, file-static, until CGameSprite.cpp's owner takes it.
// Only CheckSave calls it: with the effect text option's bit 1 it shows the
// save's name, the source's name, with bit 8 the roll, the bonus and the
// total, and whether the save was made.
// NOTE: Ruling M3-12: on the fold list; it moves into CGameSprite.
// 0x765830
static void CGameSprite_DisplaySaveString765830(CGameSprite* pSprite, DWORD nSaveType, BYTE bSaved, INT nRoll, INT nBonus, const CString& sSourceName)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if ((pGame->GetOptions()->m_nEffectTextLevel & 0x1) == 0) {
        return;
    }

    CString sRoll;
    COLORREF rgbNameColor = CVidPalette::RANGE_COLORS[pSprite->GetBaseStats()->m_colors[2]];
    CString sTemp;

    sTemp.Format("%d", nRoll);
    sRoll += sTemp;

    if (nBonus < 0) {
        sTemp.Format(" - %d", -nBonus);
    } else {
        sTemp.Format(" + %d", nBonus);
    }
    sRoll += sTemp;

    sTemp.Format(" = %d", nRoll + nBonus);
    sRoll += sTemp;

    STRREF strSave = -1;
    if (nSaveType == 4) {
        strSave = 14009; // Fortitude
    } else if (nSaveType == 8) {
        strSave = 14006; // Reflex
    } else if (nSaveType == 16) {
        strSave = 14005; // Will
    } else {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
        // __LINE__: 31452
        UTIL_ASSERT(FALSE);
    }

    STR_RES strRes;
    g_pBaldurChitin->GetTlkTable().Fetch(strSave, strRes);
    CString sText(strRes.szText);

    if (sSourceName.GetLength() != 0) {
        sText += " (" + sSourceName + ')';
    }

    if ((pGame->GetOptions()->m_nEffectTextLevel & 0x8) != 0) {
        sText += " : " + sRoll;
    }

    // 33753: saved, 33752: not saved.
    STR_RES strResult;
    g_pBaldurChitin->GetTlkTable().Fetch(bSaved == 1 ? 33753 : 33752, strResult);
    sText += " : " + strResult.szText;

    g_pBaldurChitin->GetBaldurMessage()->DisplayText(pSprite->GetName(),
        sText,
        rgbNameColor,
        RGB(0xD7, 0xD7, 0xBE),
        -1,
        pSprite->GetId(),
        pSprite->GetId());
}

// 0x4A42F0
BOOL CGameEffect::CheckSave(CGameSprite* pSprite, BYTE* pFortitudeRoll, BYTE* pReflexRoll, BYTE* pWillRoll, BYTE* pResistMagicRoll)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // NOTE: Bit 0x80 of each roll marks it as shown, so one roll is
    // reported once however many effects test it.
    BYTE nFortitudeRoll = *pFortitudeRoll & 0x7F;
    BYTE nReflexRoll = *pReflexRoll & 0x7F;
    BYTE nWillRoll = *pWillRoll & 0x7F;
    BYTE nResistMagicRoll = *pResistMagicRoll & 0x7F;

    INT nBonus = 0;
    BOOL bDisplay = FALSE;

    if (pSprite->GetAIType().m_nRace == CAIOBJECTTYPE_R_190) {
        return TRUE;
    }

    if (pGame->m_gameSave.m_cutScene == TRUE) {
        if (pSprite->GetId() == m_sourceID) {
            return TRUE;
        }

        if (m_effectID == 104) {
            return TRUE;
        }

        if (m_effectID != CGAMEEFFECT_DEATH) {
            return FALSE;
        }

        // NOTE: The object's +0x18C, CGameEffectDeath::field_18C for a death
        // (the only effect that gets here).
        if (static_cast<CGameEffectDeath*>(this)->field_18C == 0) {
            return FALSE;
        }
    }

    // Mirror images and stone skins turn poison away.
    if (m_effectID == CGAMEEFFECT_POISON
        && (static_cast<BYTE>(pSprite->m_derivedStats.m_nMirrorImages) != 0
            || pSprite->m_derivedStats.m_nStoneSkins > 0)) {
        return FALSE;
    }

    if (m_effectID == CGAMEEFFECT_PORTRAITICON
        && m_dwFlags == 6
        && (static_cast<BYTE>(pSprite->m_derivedStats.m_nMirrorImages) != 0
            || pSprite->m_derivedStats.m_nStoneSkins > 0)) {
        return FALSE;
    }

    // Spell resistance.
    if ((m_sourceFlags & 0x400) != 0
        && (m_flags & 0x2) == 0
        && (m_flags & 0x1) != 0) {
        INT nResistMagic = pSprite->m_derivedStats.m_nResistMagic;
        if (m_effectID == 451) {
            nResistMagic /= 2;
        }

        if ((*pResistMagicRoll & 0x80) == 0) {
            // NOTE: The original pushes the roll and the caster level as
            // words with stale upper halves; FeedBack shows them only with
            // the EnableRollFeedback INI option.
            pSprite->FeedBack(CGameSprite::FEEDBACK_ROLL,
                nResistMagic,
                nResistMagicRoll,
                static_cast<BYTE>(m_casterLevel),
                39673,
                static_cast<WORD>(m_spellPenetrationBonus),
                0);
        }

        // NOTE: An unsigned compare (0x4A44AA): a negative resistance
        // always resists.
        DWORD nPenetration = static_cast<BYTE>(m_casterLevel) + m_spellPenetrationBonus + nResistMagicRoll;
        if (static_cast<DWORD>(nResistMagic) > nPenetration) {
            if ((*pResistMagicRoll & 0x80) == 0) {
                COLORREF rgbNameColor = CVidPalette::RANGE_COLORS[pSprite->GetBaseStats()->m_colors[2]];

                STR_RES strRes;
                g_pBaldurChitin->GetTlkTable().Fetch(19224, strRes);
                CString sText(strRes.szText);

                CString sSourceName = IcewindMisc_GetResRefName5864F0(m_sourceRes);
                if (sSourceName.GetLength() != 0) {
                    sText += " (" + sSourceName + ')';
                }

                g_pBaldurChitin->GetBaldurMessage()->DisplayText(pSprite->GetName(),
                    sText,
                    rgbNameColor,
                    RGB(0xD7, 0xD7, 0xBE),
                    -1,
                    pSprite->GetId(),
                    pSprite->GetId());

                m_dwFlags &= ~0x3;
                *pResistMagicRoll |= 0x80;
            }

            return FALSE;
        }
    }

    if (m_savingThrow == 0) {
        return TRUE;
    }

    INT nDC = m_saveMod + m_spellLevel + 10;
    DWORD nSaveType = 0;

    // NOTE: The roll the result reads (0x4A4A5E). Without a fortitude,
    // reflex or will bit the original reads it as a dword whose low byte is
    // the spell resistance roll and whose upper bytes were never written;
    // ours takes them as zero.
    INT nRoll = nResistMagicRoll;

    if ((m_savingThrow & 0x4) != 0) {
        nRoll = nFortitudeRoll;
        nBonus = pSprite->m_derivedStats.m_nSaveVSFortitude + nRoll;
        nSaveType = 4;

        if (m_effectID == CGAMEEFFECT_POISON) {
            if (pSprite->GetAIType().m_nRace == CAIOBJECTTYPE_R_DWARF) {
                nBonus += 2;
            }

            if (pSprite->sub_763150(CGAMESPRITE_FEAT_SNAKE_BLOOD)) {
                nBonus += 2;
            }

            if (pSprite->sub_763150(CGAMESPRITE_FEAT_RESIST_POISON)) {
                nBonus += 4;
            }
        }

        if ((*pFortitudeRoll & 0x80) == 0) {
            *pFortitudeRoll |= 0x80;
            bDisplay = TRUE;
        }
    }

    if ((m_savingThrow & 0x8) != 0) {
        nSaveType = 8;

        // Trap sense against a trap's effects.
        INT nTrapSense = 0;
        if (m_sourceID != CGameObjectArray::INVALID_INDEX) {
            CGameObject* pSource;
            BYTE rc;
            do {
                rc = pGame->GetObjectArray()->GetShare(m_sourceID,
                    CGameObjectArray::THREAD_ASYNCH,
                    &pSource,
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

            if (rc == CGameObjectArray::SUCCESS) {
                if (pSource->GetObjectType() == CGameObject::TYPE_TRIGGER) {
                    nTrapSense = pGame->GetRuleTables().GetTrapSenseBonus(pSprite->m_derivedStats);
                }

                pGame->GetObjectArray()->ReleaseShare(m_sourceID,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            }
        }

        nRoll = nReflexRoll;
        nBonus = pSprite->m_derivedStats.m_nSaveVSReflex + nRoll + nTrapSense;

        if ((*pReflexRoll & 0x80) == 0) {
            *pReflexRoll |= 0x80;
            bDisplay = TRUE;
        }
    }

    if ((m_savingThrow & 0x10) != 0) {
        nSaveType = 16;
        nRoll = nWillRoll;
        nBonus = pSprite->m_derivedStats.m_nSaveVSWill + nRoll;

        // A paladin's aura of courage: +4 against SPWI420 for a party-side
        // target within 100 of a living party paladin of level 2 or more.
        BOOL bGood = pSprite->GetAIType().m_nEnemyAlly <= CAIObjectType::EA_GOODCUTOFF;
        const BYTE* pSourceRes = m_sourceRes.GetResRef();
        if (bGood
            && pSourceRes[0] != '\0'
            && memcmp(pSourceRes, "SPWI420", 7) == 0) {
            for (SHORT nIndex = 0; nIndex < pGame->m_nCharacters; nIndex++) {
                LONG nCharacterId = nIndex < pGame->m_nCharacters
                    ? pGame->m_characterPortraits[nIndex]
                    : CGameObjectArray::INVALID_INDEX;
                if (nCharacterId == CGameObjectArray::INVALID_INDEX) {
                    continue;
                }

                // NOTE: The original waits for SUCCESS with no other way out
                // (0x4A4896).
                CGameObject* pObject;
                while (pGame->GetObjectArray()->GetShare(nCharacterId,
                           CGameObjectArray::THREAD_ASYNCH,
                           &pObject,
                           INFINITE)
                    != CGameObjectArray::SUCCESS) {
                }

                CGameSprite* pCharacter = static_cast<CGameSprite*>(pObject);
                CPoint ptCharacter = pCharacter->GetPos();
                CPoint ptSprite = pSprite->GetPos();
                LONG dx = ptSprite.x - ptCharacter.x;
                LONG dy = ptSprite.y - ptCharacter.y;
                INT nPaladinLevel = static_cast<INT>(pCharacter->m_derivedStats.GetClassLevel(CAIOBJECTTYPE_C_PALADIN));
                DWORD dwState = pCharacter->m_derivedStats.m_generalState;

                pGame->GetObjectArray()->ReleaseShare(nCharacterId,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);

                if ((dwState & STATE_DEAD) == 0
                    && nPaladinLevel >= 2
                    && dx * dx + dy * dy <= 10000) {
                    nBonus += 4;
                    break;
                }
            }
        }

        if (m_effectID == CGAMEEFFECT_PANIC
            && pSprite->GetAIType().m_nRace == CAIOBJECTTYPE_R_HALFLING) {
            nBonus += 2;
        }

        // NOTE: The sprite's own m_typeAI (+0x26, +0x27), not the virtual
        // GetAIType: a drow.
        if (pSprite->m_typeAI.m_nRace == CAIOBJECTTYPE_R_ELF
            && pSprite->m_typeAI.m_nSubRace == CAIOBJECTTYPE_SUBRACE_ELF_DROW) {
            nBonus += 2;
        }

        if ((*pWillRoll & 0x80) == 0) {
            *pWillRoll |= 0x80;
            bDisplay = TRUE;
        }
    }

    if ((pSprite->m_baseStats.field_2FB & 0x1) != 0) {
        nBonus += 5;
    }

    // The difficulty's penalty for creatures outside the party: -4 at the
    // first level (the INT at 0x84EB04), -2 at the second (0x84EB08).
    if (pSprite->GetAIType().m_nEnemyAlly != CAIObjectType::EA_PC) {
        switch (pGame->GetOptions()->m_nDifficultyLevel) {
        case 1:
            nBonus += -4;
            break;
        case 2:
            nBonus += -2;
            break;
        }
    }

    BYTE nRace = pSprite->GetAIType().m_nRace;
    if ((nRace == CAIOBJECTTYPE_R_ELF || nRace == CAIOBJECTTYPE_R_HALF_ELF)
        && m_school == 4) {
        // Enchantment.
        nBonus += 2;
    }

    if (nRace == CAIOBJECTTYPE_R_GNOME && m_school == 5) {
        // Illusion.
        nBonus += 2;
    }

    if ((m_flags & 0x2) == 0 && nRace == CAIOBJECTTYPE_R_DWARF) {
        nBonus += 2;
    }

    if (pSprite->GetClassLevel(CAIOBJECTTYPE_C_MONK) >= 3 && m_school == 4) {
        nBonus += 2;
    }

    if (m_school != 0
        && pGame->GetRuleTables().MapCharacterSpecializationToSchool(static_cast<WORD>(pSprite->m_baseStats.m_specialization)) == m_school) {
        nBonus += 2;
    }

    BYTE bSaved;
    if (nRoll == 20 || (nBonus >= nDC && nRoll != 1)) {
        bSaved = TRUE;
    } else {
        bSaved = FALSE;
    }

    if (bDisplay) {
        CString sSourceName = IcewindMisc_GetResRefName5864F0(m_sourceRes);
        CGameSprite_DisplaySaveString765830(pSprite, nSaveType, bSaved, nRoll, nBonus - nRoll, sSourceName);
    }

    return bSaved == FALSE;
}

// 0x4A4FC0
BOOL CGameEffect::sub_4A4FC0(CGameEffect* pOther, BOOL bCheckSource)
{
    if (strcmp(m_sourceRes.GetResRefStr(), pOther->m_sourceRes.GetResRefStr()) != 0) {
        return FALSE;
    }

    if (m_effectID != pOther->m_effectID) {
        return FALSE;
    }

    if (m_effectAmount != pOther->m_effectAmount) {
        return FALSE;
    }

    if (m_dwFlags != pOther->m_dwFlags) {
        return FALSE;
    }

    if (m_durationType != pOther->m_durationType) {
        return FALSE;
    }

    if (m_duration != pOther->m_duration) {
        return FALSE;
    }

    if ((m_source.x != pOther->m_source.x || m_source.y != pOther->m_source.y) && bCheckSource) {
        return FALSE;
    }

    if (m_sourceID != pOther->m_sourceID && bCheckSource) {
        return FALSE;
    }

    if (m_targetType != pOther->m_targetType) {
        return FALSE;
    }

    if (m_probabilityUpper != pOther->m_probabilityUpper) {
        return FALSE;
    }

    if (m_probabilityLower != pOther->m_probabilityLower) {
        return FALSE;
    }

    return m_spellLevel == pOther->m_spellLevel;
}

// 0x48C5B0
BOOL CGameEffect::Compare(const CGameEffect& other)
{
    return m_effectID == other.m_effectID
        && m_effectAmount == other.m_effectAmount
        && m_dwFlags == other.m_dwFlags
        && m_durationType == other.m_durationType
        && m_duration == other.m_duration
        && m_source == other.m_source
        && m_sourceID == other.m_sourceID
        && m_targetType == other.m_targetType
        && m_probabilityUpper == other.m_probabilityUpper
        && m_probabilityLower == other.m_probabilityLower
        && m_spellLevel == other.m_spellLevel;
}

// 0x48C670
void CGameEffect::CopyFromBase(CGameEffectBase* pBase)
{
    m_version = pBase->m_version;
    m_effectID = pBase->m_effectID;
    m_targetType = pBase->m_targetType;
    m_spellLevel = pBase->m_spellLevel;
    m_effectAmount = pBase->m_effectAmount;
    m_dwFlags = pBase->m_dwFlags;
    m_durationType = pBase->m_durationType;
    m_duration = pBase->m_duration;
    m_probabilityUpper = pBase->m_probabilityUpper;
    m_probabilityLower = pBase->m_probabilityLower;
    m_res = pBase->m_res;
    m_numDice = pBase->m_numDice;
    m_diceSize = pBase->m_diceSize;
    m_savingThrow = pBase->m_savingThrow;
    m_saveMod = pBase->m_saveMod;
    m_special = pBase->m_special;
    m_school = pBase->m_school;
    field_48 = pBase->field_48;
    m_minLevel = pBase->m_minLevel;
    m_maxLevel = pBase->m_maxLevel;
    m_flags = pBase->m_flags;
    m_effectAmount2 = pBase->m_effectAmount2;
    m_effectAmount3 = pBase->m_effectAmount3;
    m_effectAmount4 = pBase->m_effectAmount4;
    m_effectAmount5 = pBase->m_effectAmount5;
    m_res2 = pBase->m_res2;
    m_res3 = pBase->m_res3;
    m_source = pBase->m_source;
    m_target = pBase->m_target;
    m_sourceType = pBase->m_sourceType;
    m_sourceRes = pBase->m_sourceRes;
    m_sourceFlags = pBase->m_sourceFlags;
    m_projectileType = pBase->m_projectileType;
    m_slotNum = pBase->m_slotNum;
    memcpy(m_scriptName, pBase->m_scriptName, 32);
    m_casterLevel = pBase->m_casterLevel;
    m_spellPenetrationBonus = pBase->m_spellPenetrationBonus;
    m_secondaryType = pBase->m_secondaryType;
}

// 0x4933E0
void CGameEffect::SetEffectID(WORD effectID)
{
    m_effectID = effectID;
}

// 0x4933F0
void CGameEffect::SetDWFlags(DWORD dwFlags)
{
    m_dwFlags = dwFlags;
}

// 0x493400
void CGameEffect::sub_493400(BOOL a1)
{
    if (a1) {
        m_flags |= 0x2;
    } else {
        m_flags &= ~0x2;
    }
}

// 0x594020
void CGameEffect::sub_594020(BOOL bEnabled)
{
    if (bEnabled) {
        m_flags |= 0x1;
    } else {
        m_flags &= ~0x1;
    }
}

// 0x594040
void CGameEffect::SetSource(const CPoint& pt)
{
    m_source = pt;
}

// 0x594060
void CGameEffect::SetSourceId(LONG sourceID)
{
    m_sourceID = sourceID;
}

// 0x4C3F30
void CGameEffect::sub_4C3F30(CGameSprite* pSprite, INT nValue)
{
    pSprite->GetDerivedStats()->field_8 = max(pSprite->GetDerivedStats()->field_8, nValue);
}

// 0x4C3F60
void CGameEffect::AdjustResistMagic(CGameSprite* pSprite, SHORT nValue)
{
    pSprite->GetDerivedStats()->m_nResistMagic += nValue;
    if (pSprite->GetDerivedStats()->m_nResistMagic < 0) {
        pSprite->GetDerivedStats()->m_nResistMagic = 0;
    } else if (pSprite->GetDerivedStats()->m_nResistMagic > 50) {
        pSprite->GetDerivedStats()->m_nResistMagic = 50;
    }
}

// 0x4C3FA0
void CGameEffect::AdjustTHAC0(CGameSprite* pSprite, SHORT nValue)
{
    pSprite->GetDerivedStats()->m_nTHAC0 += nValue;
}

// NOTE: Convenience.
void CGameEffect::AdjustSTR(CGameSprite* pSprite, INT nValue)
{
    if ((nValue > 0 && nValue > pSprite->GetDerivedStats()->field_128)
        || (nValue < 0 && nValue < pSprite->GetDerivedStats()->field_128)) {
        pSprite->m_bonusStats.m_nSTR += static_cast<SHORT>(nValue) - static_cast<SHORT>(pSprite->GetDerivedStats()->field_128);
        pSprite->GetDerivedStats()->field_128 = nValue;
    }
}

// 0x4C3FC0
void CGameEffect::AdjustDEX(CGameSprite* pSprite, INT nValue)
{
    if ((nValue > 0 && nValue > pSprite->GetDerivedStats()->field_12C)
        || (nValue < 0 && nValue < pSprite->GetDerivedStats()->field_12C)) {
        pSprite->m_bonusStats.m_nDEX += static_cast<SHORT>(nValue) - static_cast<SHORT>(pSprite->GetDerivedStats()->field_12C);
        pSprite->GetDerivedStats()->field_12C = nValue;
    }
}

// NOTE: Convenience.
void CGameEffect::AdjustINT(CGameSprite* pSprite, INT nValue)
{
    if ((nValue > 0 && nValue > pSprite->GetDerivedStats()->field_134)
        || (nValue < 0 && nValue < pSprite->GetDerivedStats()->field_134)) {
        pSprite->m_bonusStats.m_nINT += static_cast<SHORT>(nValue) - static_cast<SHORT>(pSprite->GetDerivedStats()->field_134);
        pSprite->GetDerivedStats()->field_134 = nValue;
    }
}

// NOTE: Convenience.
void CGameEffect::AdjustWIS(CGameSprite* pSprite, INT nValue)
{
    if ((nValue > 0 && nValue > pSprite->GetDerivedStats()->field_138)
        || (nValue < 0 && nValue < pSprite->GetDerivedStats()->field_138)) {
        pSprite->m_bonusStats.m_nWIS += static_cast<SHORT>(nValue) - static_cast<SHORT>(pSprite->GetDerivedStats()->field_138);
        pSprite->GetDerivedStats()->field_138 = nValue;
    }
}

// NOTE: Convenience.
void CGameEffect::AdjustCON(CGameSprite* pSprite, INT nValue)
{
    if ((nValue > 0 && nValue > pSprite->GetDerivedStats()->field_130)
        || (nValue < 0 && nValue < pSprite->GetDerivedStats()->field_130)) {
        pSprite->m_bonusStats.m_nCON += static_cast<SHORT>(nValue) - static_cast<SHORT>(pSprite->GetDerivedStats()->field_130);
        pSprite->GetDerivedStats()->field_130 = nValue;
    }
}

// 0x4C4000
void CGameEffect::Immobilize(CGameSprite* pSprite)
{
    // NOTE: Uninline.
    pSprite->GetAnimation()->SetMoveScale(0);

    if (m_secondaryType) {
        CMessageDropPath* pMessage = new CMessageDropPath(pSprite->GetId(),
            pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
    }
}

// 0x4C4080
void CGameEffect::AddSlowEffect(CGameSprite* pSprite)
{
    if ((pSprite->GetDerivedStats()->m_generalState & STATE_HASTED) != 0) {
        pSprite->GetEquipedEffectList()->RemoveAllOfType(pSprite,
            CGAMEEFFECT_HASTE,
            pSprite->GetEquipedEffectList()->GetPosCurrent(),
            -1);
        pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
            CGAMEEFFECT_HASTE,
            pSprite->GetTimedEffectList()->GetPosCurrent(),
            -1);
    }

    pSprite->GetDerivedStats()->m_generalState |= STATE_SLOWED;
    pSprite->GetDerivedStats()->field_C -= 2;
    pSprite->GetDerivedStats()->m_nTHAC0 -= 2;
    pSprite->GetDerivedStats()->m_nSaveVSReflex -= 2;
    pSprite->m_bonusStats.m_nDamageBonus -= 2;

    if (pSprite->GetDerivedStats()->m_nNumberOfAttacks > 2) {
        pSprite->GetDerivedStats()->m_nNumberOfAttacks -= 1;
    }

    if (pSprite->GetAnimation()->GetMoveScale() != 0) {
        pSprite->GetAnimation()->SetMoveScale(pSprite->GetAnimation()->GetMoveScaleDefault() / 2);

        if (m_secondaryType != 0) {
            pSprite->DropPath();
        }
    }

    // NOTE: Uninline.
    AddPortraitIcon(pSprite, 41);
}

// 0x4C4200
void CGameEffect::AddPanicEffect(CGameSprite* pSprite)
{
    pSprite->sub_761650();
    pSprite->GetDerivedStats()->m_generalState |= STATE_PANIC;

    // NOTE: Uninline.
    AddPortraitIcon(pSprite, 36);
}

// 0x4C4260
void CGameEffect::RemoveAllOfType(CGameSprite* pSprite, WORD effectID, LONG effectAmount)
{
    pSprite->GetEquipedEffectList()->RemoveAllOfType(pSprite,
        effectID,
        pSprite->GetEquipedEffectList()->GetPosCurrent(),
        effectAmount);
    pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
        effectID,
        pSprite->GetTimedEffectList()->GetPosCurrent(),
        effectAmount);
}

// 0x4C42A0
void CGameEffect::AddPortraitIcon(CGameSprite* pSprite, int icon)
{
    // NOTE: Uninline.
    pSprite->AddPortraitIcon(icon);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    pGame->UpdatePortrait(pGame->GetCharacterPortraitNum(pSprite->GetId()), 1);
}

// 0x4C42F0
void CGameEffect::SetSpellState(CGameSprite* pSprite, DWORD dwSpellState)
{
    pSprite->GetDerivedStats()->m_spellStates.set(dwSpellState);
}

// 0x4C4330
void CGameEffect::FeedBackImmuneToResource(CGameSprite* pSprite, const CResRef& resRef)
{
    if (g_pBaldurChitin->cDimm.m_cKeyTable.FindKey(resRef, 1005, TRUE)) {
        CItem* pItem = new CItem(resRef, 1, 0, 0, 0, 0);
        pSprite->FeedBack(CGameSprite::FEEDBACK_IMMUNE_TO_RESOURCE,
            0,
            0,
            0,
            pItem->GetIdentifiedName(),
            0,
            0);
        delete pItem;
    } else if (g_pBaldurChitin->cDimm.m_cKeyTable.FindKey(resRef, 1006, TRUE)) {
        CSpell* pSpell = new CSpell(resRef);
        pSprite->FeedBack(CGameSprite::FEEDBACK_IMMUNE_TO_RESOURCE,
            0,
            0,
            0,
            pSpell->GetGenericName(),
            0,
            0);
        delete pSpell;
    }
}

// 0x4A2E00
void CGameEffect::ClearItemEffect(ITEM_EFFECT* itemEffect, WORD newEffectId)
{
    itemEffect->effectID = newEffectId;
    itemEffect->targetType = 0;
    itemEffect->spellLevel = 0;
    itemEffect->effectAmount = 0;
    itemEffect->dwFlags = 0;
    itemEffect->durationType = 0;
    itemEffect->duration = 0;
    itemEffect->probabilityUpper = 100;
    itemEffect->probabilityLower = 0;
    itemEffect->numDice = 0;
    itemEffect->diceSize = 0;
    itemEffect->savingThrow = 0;
    itemEffect->saveMod = 0;
    itemEffect->special = 0;
}

// 0x4B8730
void CGameEffect::sub_4B8730(CGameSprite* pSprite, INT slotNum)
{
    CItem* pItem = pSprite->GetEquipment()->m_items[slotNum];
    if (pItem != NULL) {
        pSprite->GetEquipment()->m_items[slotNum] = NULL;
        pItem->Unequip(pSprite, slotNum, TRUE, FALSE);
        g_pBaldurChitin->GetObjectGame()->AddDisposableItem(pItem);

        if (pSprite->InControl()
            && g_pChitin->cNetwork.GetSessionOpen() == TRUE) {
            CMessage* message = new CMessageSpriteEquipment(pSprite,
                pSprite->GetId(),
                pSprite->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }

        if (pSprite->GetEquipment()->m_selectedWeapon == slotNum) {
            pSprite->EquipMostDamagingMelee();
        }
    }
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectAC::CGameEffectAC(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x493420
CGameEffect* CGameEffectAC::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectAC* copy = new CGameEffectAC(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4A51E0
BOOL CGameEffectAC::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        pSprite->GetDerivedStats()->field_C += static_cast<SHORT>(m_effectAmount);
        break;
    case 1:
        pSprite->GetDerivedStats()->field_6 = max(pSprite->GetDerivedStats()->field_6, static_cast<SHORT>(m_effectAmount));
        break;
    case 2:
        pSprite->GetDerivedStats()->field_8 = max(pSprite->GetDerivedStats()->field_8, static_cast<SHORT>(m_effectAmount));
        break;
    case 3:
        pSprite->GetDerivedStats()->field_A = max(pSprite->GetDerivedStats()->field_A, static_cast<SHORT>(m_effectAmount));
        break;
    case 4:
        pSprite->GetDerivedStats()->m_nACCrushingMod += static_cast<SHORT>(m_effectAmount);
        break;
    case 5:
        pSprite->GetDerivedStats()->m_nACPiercingMod += static_cast<SHORT>(m_effectAmount);
        break;
    case 6:
        pSprite->GetDerivedStats()->m_nACSlashingMod += static_cast<SHORT>(m_effectAmount);
        break;
    case 7:
        pSprite->GetDerivedStats()->m_nACMissileMod += static_cast<SHORT>(m_effectAmount);
        break;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectAttackNo::CGameEffectAttackNo(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4934D0
CGameEffect* CGameEffectAttackNo::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectAttackNo* copy = new CGameEffectAttackNo(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4A52F0
BOOL CGameEffectAttackNo::ApplyEffect(CGameSprite* pSprite)
{
    BYTE nBase;
    INT nValue;
    SHORT nAttacks;

    switch (m_dwFlags) {
    case 0:
        if (m_durationType == 1) {
            nBase = pSprite->m_baseStats.m_numberOfAttacksBase;
            if (nBase < 5) {
                nBase += static_cast<BYTE>(m_effectAmount);
                pSprite->m_baseStats.m_numberOfAttacksBase = nBase;
                if (nBase > 5) {
                    pSprite->m_baseStats.m_numberOfAttacksBase = 5;
                }
            } else {
                pSprite->m_baseStats.m_numberOfAttacksBase = static_cast<BYTE>(m_effectAmount) + nBase;
            }
            m_forceRepass = TRUE;
            m_done = TRUE;
            break;
        }

        pSprite->m_derivedStats.m_nNumberOfAttacks += static_cast<SHORT>(m_effectAmount);
        m_done = FALSE;
        break;
    case 1:
        if (m_durationType == 1) {
            pSprite->m_baseStats.m_numberOfAttacksBase = static_cast<BYTE>(m_effectAmount);
            m_forceRepass = TRUE;
            m_done = TRUE;
            break;
        }

        pSprite->m_derivedStats.m_nNumberOfAttacks = static_cast<BYTE>(m_effectAmount);
        m_done = FALSE;
        break;
    case 2:
        if (m_durationType == 1) {
            nBase = pSprite->m_baseStats.m_numberOfAttacksBase;
            if (nBase < 6) {
                nValue = static_cast<SHORT>(m_effectAmount) * nBase;
                nBase = static_cast<BYTE>(nValue / 100);
                pSprite->m_baseStats.m_numberOfAttacksBase = nBase;
                if (nBase > 5) {
                    pSprite->m_baseStats.m_numberOfAttacksBase = 5;
                }
                nBase = pSprite->m_baseStats.m_numberOfAttacksBase;
            } else {
                nValue = (nBase - 5) * static_cast<SHORT>(m_effectAmount);
                nBase = static_cast<BYTE>(nValue / 100) + 5;
                pSprite->m_baseStats.m_numberOfAttacksBase = nBase;
            }
            if (nBase < 2) {
                pSprite->m_baseStats.m_numberOfAttacksBase = 1;
            }
            m_forceRepass = TRUE;
            m_done = TRUE;
            break;
        }

        // NOTE: The original compares the derived number of attacks with 6
        // but scales the base number.
        if (pSprite->m_derivedStats.m_nNumberOfAttacks < 6) {
            nValue = pSprite->m_baseStats.m_numberOfAttacksBase * static_cast<SHORT>(m_effectAmount);
            nAttacks = static_cast<SHORT>(nValue / 100);
            pSprite->m_derivedStats.m_nNumberOfAttacks = nAttacks;
            if (nAttacks > 5) {
                pSprite->m_derivedStats.m_nNumberOfAttacks = 5;
            }
            if (pSprite->m_derivedStats.m_nNumberOfAttacks < 0) {
                pSprite->m_derivedStats.m_nNumberOfAttacks = 0;
            }
            if (pSprite->m_derivedStats.m_nNumberOfAttacks < 2) {
                pSprite->m_derivedStats.m_nNumberOfAttacks = 1;
            }
        } else {
            nValue = (pSprite->m_baseStats.m_numberOfAttacksBase - 5) * static_cast<SHORT>(m_effectAmount);
            nAttacks = static_cast<SHORT>(nValue / 100 + 5);
            pSprite->m_derivedStats.m_nNumberOfAttacks = nAttacks;
            if (nAttacks < 2) {
                pSprite->m_derivedStats.m_nNumberOfAttacks = 1;
            }
        }
        m_done = FALSE;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 3417
        UTIL_ASSERT(FALSE);
        break;
    }

    nBase = pSprite->m_baseStats.m_numberOfAttacksBase;
    if (nBase > 4) {
        nBase = 5;
    }
    pSprite->m_baseStats.m_numberOfAttacksBase = nBase;

    nAttacks = pSprite->m_derivedStats.m_nNumberOfAttacks;
    if (nAttacks > 4) {
        nAttacks = 5;
    }
    pSprite->m_derivedStats.m_nNumberOfAttacks = nAttacks;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectAwaken::CGameEffectAwaken(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x493580
CGameEffect* CGameEffectAwaken::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectAwaken* copy = new CGameEffectAwaken(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4A5540
BOOL CGameEffectAwaken::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetBaseStats()->m_generalState &= ~STATE_SLEEPING;
    pSprite->GetDerivedStats()->m_generalState &= ~STATE_SLEEPING;

    pSprite->GetEquipedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_SLEEP,
        pSprite->GetEquipedEffectList()->GetPosCurrent(),
        -1);
    pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_SLEEP,
        pSprite->GetTimedEffectList()->GetPosCurrent(),
        -1);

    pSprite->GetEquipedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_PORTRAITICON,
        pSprite->GetEquipedEffectList()->GetPosCurrent(),
        14);
    pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_PORTRAITICON,
        pSprite->GetTimedEffectList()->GetPosCurrent(),
        14);

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectBerserk::CGameEffectBerserk(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x493630
CGameEffect* CGameEffectBerserk::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectBerserk* copy = new CGameEffectBerserk(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4A55E0
BOOL CGameEffectBerserk::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->m_derivedStats.m_generalState |= STATE_BERSERK;

    if (m_dwFlags == 1) {
        if (m_secondaryType) {
            // NOTE: Uninline.
            DisplayStringRef(pSprite, 14036);

            pSprite->EquipMostDamagingMelee();
        }

        // NOTE: Uninline.
        pSprite->AddPortraitIcon(4);

        pSprite->m_derivedStats.m_spellStates.set(SPLSTATE_BERSERK_ALWAYS_ACTIVE, true);
    } else if (m_dwFlags == 2) {
        // Blood rage.
        if (m_secondaryType) {
            // NOTE: Uninline.
            DisplayStringRef(pSprite, 14036);

            pSprite->EquipMostDamagingMelee();
        }

        // NOTE: Uninline.
        pSprite->AddPortraitIcon(76);

        pSprite->m_derivedStats.m_spellStates.set(SPLSTATE_BERSERK_ALWAYS_ACTIVE, true);
        pSprite->m_derivedStats.m_spellStates.set(SPLSTATE_BLOOD_RAGE, true);

        pSprite->m_derivedStats.m_naturalImmunities.insert(17);
        pSprite->m_derivedStats.m_naturalImmunities.insert(5);
        pSprite->m_derivedStats.m_naturalImmunities.insert(39);
        pSprite->m_derivedStats.m_naturalImmunities.insert(24);
        pSprite->m_derivedStats.m_naturalImmunities.insert(109);
        pSprite->m_derivedStats.m_naturalImmunities.insert(45);
        pSprite->m_derivedStats.m_naturalImmunities.insert(128);
        pSprite->m_derivedStats.m_naturalImmunities.insert(400);

        // NOTE: The original tests the bonus stats' fields and updates the
        // derived stats' ones (+0x128 and +0x130 of each).
        if (pSprite->m_bonusStats.field_128 < 6) {
            pSprite->m_bonusStats.m_nSTR += 6 - static_cast<SHORT>(pSprite->m_derivedStats.field_128);
            pSprite->m_derivedStats.field_128 = 6;
        }

        if (pSprite->m_bonusStats.field_130 < 2) {
            pSprite->m_bonusStats.m_nCON += 2 - static_cast<SHORT>(pSprite->m_derivedStats.field_130);
            pSprite->m_derivedStats.field_130 = 2;
        }

        pSprite->m_derivedStats.m_spellStates.set(SPLSTATE_SUPPRESS_HP_INFO, true);

        if (pSprite->GetAnimation()->GetMoveScale() != 0) {
            pSprite->GetAnimation()->SetMoveScale(pSprite->GetAnimation()->GetMoveScaleDefault() + 2);

            if (m_secondaryType) {
                pSprite->DropPath();
            }
        }

        AddColorEffect(pSprite, 128, 0, 0, 15);
    } else {
        if (m_secondaryType) {
            // NOTE: Uninline.
            DisplayStringRef(pSprite, 14036);
        }

        // NOTE: Uninline.
        pSprite->AddPortraitIcon(4);
    }

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    pGame->UpdatePortrait(pGame->GetCharacterPortraitNum(pSprite->GetId()), 1);

    pSprite->sub_761650();

    return TRUE;
}

// 0x4A5AB0
void CGameEffectBerserk::OnRemove(CGameSprite* pSprite)
{
    if (m_dwFlags == 2) {
        std::list<CGameEffect*> effects = IcewindMisc::GetSpellResrefEffects(pSprite,
            reinterpret_cast<const BYTE*>("EffBR"),
            static_cast<BYTE>(m_casterLevel));
        for (std::list<CGameEffect*>::iterator it = effects.begin(); it != effects.end(); it++) {
            (*it)->m_sourceRes = m_sourceRes;
            (*it)->m_target = m_target;
            (*it)->m_casterLevel = m_casterLevel;
            pSprite->AddEffect(*it, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);
        }
    }

    PlaySound(CResRef("EFF_E05"), pSprite);
}

// -----------------------------------------------------------------------------

// 0x4936E0
CGameEffectBarbarianRage::CGameEffectBarbarianRage(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x493720
CGameEffect* CGameEffectBarbarianRage::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectBarbarianRage* copy = new CGameEffectBarbarianRage(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4A5BF0
BOOL CGameEffectBarbarianRage::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetDerivedStats()->m_disabledSpellTypes[0] = TRUE;
    pSprite->GetDerivedStats()->m_disabledSpellTypes[1] = TRUE;
    pSprite->GetDerivedStats()->m_disabledButtons[2] = TRUE;
    pSprite->GetDerivedStats()->m_disabledButtons[3] = TRUE;
    pSprite->GetDerivedStats()->m_disabledButtons[4] = TRUE;
    pSprite->GetDerivedStats()->m_disabledButtons[5] = TRUE;
    pSprite->GetDerivedStats()->m_disabledButtons[6] = TRUE;
    pSprite->GetDerivedStats()->m_disabledButtons[7] = TRUE;
    pSprite->GetDerivedStats()->m_disabledButtons[8] = TRUE;
    pSprite->GetDerivedStats()->m_disabledButtons[9] = TRUE;
    pSprite->GetDerivedStats()->m_disabledButtons[10] = TRUE;
    pSprite->GetDerivedStats()->m_disabledButtons[0] = TRUE;
    return TRUE;
}

// 0x4A5C50
void CGameEffectBarbarianRage::OnRemove(CGameSprite* pSprite)
{
    if (pSprite->m_derivedStats.GetClassLevel(CAIOBJECTTYPE_C_BARBARIAN) < 20
        && pSprite->m_derivedStats.m_naturalImmunities.find(CGAMEEFFECT_FATIGUE) == pSprite->m_derivedStats.m_naturalImmunities.end()) {
        std::list<CGameEffect*> effects = IcewindMisc::GetSpellResrefEffects(pSprite,
            reinterpret_cast<const BYTE*>("EffBBR"),
            static_cast<BYTE>(m_casterLevel));
        for (std::list<CGameEffect*>::iterator it = effects.begin(); it != effects.end(); it++) {
            (*it)->m_sourceRes = m_sourceRes;
            (*it)->m_target = m_target;
            (*it)->m_casterLevel = m_casterLevel;
            pSprite->AddEffect(*it, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);
        }
    }

    PlaySound(CResRef("EFF_E05"), pSprite);
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectCalm::CGameEffectCalm(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4937D0
CGameEffect* CGameEffectCalm::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectCalm* copy = new CGameEffectCalm(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4A5E50
BOOL CGameEffectCalm::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetBaseStats()->m_generalState &= ~STATE_BERSERK;
    pSprite->GetDerivedStats()->m_generalState &= ~STATE_BERSERK;

    pSprite->GetEquipedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_BERSERK,
        pSprite->GetEquipedEffectList()->GetPosCurrent(),
        -1);
    pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_BERSERK,
        pSprite->GetTimedEffectList()->GetPosCurrent(),
        -1);

    pSprite->GetEquipedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_PORTRAITICON,
        pSprite->GetEquipedEffectList()->GetPosCurrent(),
        4);
    pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_PORTRAITICON,
        pSprite->GetTimedEffectList()->GetPosCurrent(),
        4);

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x493880
CGameEffectCharm::CGameEffectCharm(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4938C0
CGameEffect* CGameEffectCharm::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectCharm* copy = new CGameEffectCharm(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4A6230
BOOL CGameEffectCharm::ApplyEffect(CGameSprite* pSprite)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    switch (m_dwFlags) {
    case 0:
        if (m_secondaryType) {
            // NOTE: Uninline.
            DisplayStringRef(pSprite, 14672);
        }

        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 0);
        break;
    case 1:
        if (m_secondaryType) {
            // NOTE: Uninline.
            DisplayStringRef(pSprite, 1691);
        }

        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 53);

        pSprite->m_derivedStats.m_spellStates.set(72, true);
        break;
    case 2:
        if (m_secondaryType) {
            // NOTE: Uninline.
            DisplayStringRef(pSprite, 39887);
        }

        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 53);

        pSprite->m_derivedStats.m_nTHAC0 -= 2;
        pSprite->m_derivedStats.m_spellStates.set(72, true);
        break;
    }

    pSprite->sub_761650();

    CAIObjectType typeAI;
    typeAI.Set(pSprite->GetAIType());
    CAIObjectType liveTypeAI;
    liveTypeAI.Set(pSprite->m_liveTypeAI);
    CAIObjectType startTypeAI;
    startTypeAI.Set(pSprite->m_startTypeAI);
    CAIObjectType enemyTypeAI(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);

    // m_numDice and m_diceSize keep the new allegiance and whether a party
    // member charms a party member once the first pass has worked them out.
    BYTE nEnemyAlly = 0;
    BOOL bPartyCharm = FALSE;
    if (m_numDice != static_cast<DWORD>(-1)) {
        nEnemyAlly = static_cast<BYTE>(m_numDice);
        bPartyCharm = m_diceSize;

        CAIObjectType type(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
        type.m_nEnemyAlly = nEnemyAlly;
        enemyTypeAI.Set(type.GetEnemyOf());
    } else {
        if (m_sourceID != CGameObjectArray::INVALID_INDEX) {
            CGameObject* pSource;
            BYTE rc;
            do {
                rc = pGame->GetObjectArray()->GetShare(m_sourceID,
                    CGameObjectArray::THREAD_ASYNCH,
                    &pSource,
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

            if (rc == CGameObjectArray::SUCCESS) {
                if (pSource->GetAIType().m_nEnemyAlly > CAIObjectType::EA_GOODCUTOFF) {
                    nEnemyAlly = CAIObjectType::EA_ENEMY;
                }

                enemyTypeAI.Set(pSource->GetAIType().GetEnemyOf());

                if (pGame->GetCharacterPortraitNum(pSprite->GetId()) != -1
                    && pGame->GetCharacterPortraitNum(pSource->GetId()) != -1) {
                    bPartyCharm = TRUE;
                }

                pGame->GetObjectArray()->ReleaseShare(m_sourceID,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            }
        }

        // A rogue with Slippery Mind gets a second save (effect 445), told
        // the three allegiances to restore.
        if (m_secondaryType
            && pSprite->m_derivedStats.GetClassLevel(CAIOBJECTTYPE_C_ROGUE) > 0
            && pSprite->sub_763150(CGAMESPRITE_FEAT_SLIPPERY_MIND) == TRUE) {
            ITEM_EFFECT effect;
            effect.effectID = ICEWIND_CGAMEEFFECT_SLIPPERYMIND;
            effect.targetType = 1;
            effect.spellLevel = static_cast<BYTE>(m_spellLevel);
            effect.effectAmount = m_effectAmount;
            effect.dwFlags = m_flags;
            effect.durationType = 0;
            effect.duration = 7;
            effect.probabilityUpper = 100;
            effect.probabilityLower = 0;
            // NOTE: The original leaves the resref uninitialised.
            memset(effect.res, 0, sizeof(effect.res));
            effect.numDice = m_numDice;
            effect.diceSize = m_diceSize;
            effect.savingThrow = m_savingThrow;
            effect.saveMod = m_saveMod;
            effect.special = m_special;

            CGameEffect* pEffect = CGameEffect::DecodeEffect(&effect, CPoint(-1, -1), -1, CPoint(-1, -1));
            pEffect->m_source = pSprite->GetPos();
            pEffect->m_sourceID = pSprite->GetId();
            pEffect->m_effectAmount2 = typeAI.m_nEnemyAlly;
            pEffect->m_effectAmount3 = liveTypeAI.m_nEnemyAlly;
            pEffect->m_effectAmount4 = startTypeAI.m_nEnemyAlly;
            pEffect->m_flags &= ~0x1;

            CMessage* message = new CMessageAddEffect(pEffect, pSprite->GetId(), pSprite->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
    }

    if (nEnemyAlly == 0) {
        nEnemyAlly = CAIObjectType::EA_0x847C3A;
    }

    if (nEnemyAlly == CAIObjectType::EA_ENEMY) {
        nEnemyAlly = CAIObjectType::EA_CHARMED_PC;
    }

    if (m_durationType == 1) {
        startTypeAI.m_nEnemyAlly = nEnemyAlly;
        pSprite->m_startTypeAI.Set(startTypeAI);
        m_done = TRUE;
    } else if (!bPartyCharm && pGame->GetCharacterPortraitNum(pSprite->GetId()) == -1) {
        startTypeAI.m_nEnemyAlly = enemyTypeAI.m_nEnemyAlly;
        pSprite->m_startTypeAI.Set(startTypeAI);
        m_done = FALSE;
    }

    typeAI.m_nEnemyAlly = nEnemyAlly;
    pSprite->SetAIType(typeAI, FALSE, FALSE);
    liveTypeAI.m_nEnemyAlly = nEnemyAlly;
    pSprite->m_liveTypeAI.Set(liveTypeAI);

    pSprite->m_derivedStats.m_generalState |= STATE_CHARMED;

    m_diceSize = bPartyCharm;
    m_numDice = nEnemyAlly;

    return TRUE;
}

// 0x4A6220
void CGameEffectCharm::OnAddSpecific(CGameSprite* pSprite)
{
    // The party flag and the old allegiance ApplyEffect keeps here are
    // taken again on the first call.
    m_numDice = -1;
    m_diceSize = -1;
}

// 0x4A6200
void CGameEffectCharm::OnRemove(CGameSprite* pSprite)
{
    pSprite->GetDerivedStats()->m_generalState &= ~STATE_CHARMED;
}

// -----------------------------------------------------------------------------

// 0x493970
CGameEffectSlipperyMind::CGameEffectSlipperyMind(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4939B0
CGameEffect* CGameEffectSlipperyMind::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectSlipperyMind* copy = new CGameEffectSlipperyMind(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x799C90
BOOL CGameEffectSlipperyMind::ApplyEffect(CGameSprite* pSprite)
{
    // NOTE: The vtable's slot +8 is the shared "return 1" (ret 4) that
    // identical code folding made of every such override; the effect works
    // through the code that looks for it, not here.
    return TRUE;
}

// 0x4A5EF0
void CGameEffectSlipperyMind::OnRemove(CGameSprite* pSprite)
{
    // When the effect ends the charmed creature saves again; a made save
    // breaks the charm. The rolls are copies, so the original's shown marks
    // stay off the sprite.
    BYTE nFortitudeRoll = pSprite->field_70F6;
    BYTE nReflexRoll = pSprite->field_70F7;
    BYTE nWillRoll = pSprite->field_70F8;
    BYTE nResistMagicRoll = pSprite->field_70F9;

    if (!CheckSave(pSprite, &nFortitudeRoll, &nReflexRoll, &nWillRoll, &nResistMagicRoll)) {
        CAIObjectType typeAI;
        typeAI.Set(pSprite->GetAIType());

        CAIObjectType liveTypeAI;
        liveTypeAI.Set(pSprite->m_liveTypeAI);

        CAIObjectType startTypeAI;
        startTypeAI.Set(pSprite->m_startTypeAI);
        startTypeAI.m_nEnemyAlly = static_cast<BYTE>(m_effectAmount4);
        pSprite->m_startTypeAI.Set(startTypeAI);

        typeAI.m_nEnemyAlly = static_cast<BYTE>(m_effectAmount2);
        pSprite->SetAIType(typeAI, FALSE, FALSE);

        liveTypeAI.m_nEnemyAlly = static_cast<BYTE>(m_effectAmount3);
        pSprite->m_liveTypeAI.Set(liveTypeAI);

        pSprite->m_derivedStats.m_generalState &= ~STATE_CHARMED;

        STRREF strText;
        int nIcon;
        BOOL bKnown = TRUE;
        switch (m_dwFlags) {
        case 0:
            strText = 39884;
            nIcon = 0;
            break;
        case 1:
            strText = 39885;
            nIcon = 53;
            pSprite->m_derivedStats.m_spellStates.set(SPLSTATE_MENTAL_DOMINATION, false);
            break;
        case 2:
            pSprite->m_derivedStats.m_nTHAC0 -= 2;
            strText = 39886;
            nIcon = 53;
            pSprite->m_derivedStats.m_spellStates.set(SPLSTATE_MENTAL_DOMINATION, false);
            break;
        default:
            // NOTE: The original then reads the text and the icon from a
            // local it never wrote (0x4A6097); no data file gives this
            // effect other flags. Ours shows nothing and removes no icon.
            strText = -1;
            nIcon = 0;
            bKnown = FALSE;
            break;
        }

        if (bKnown) {
            // NOTE: Uninline.
            DisplayStringRef(pSprite, strText);

            POSITION pos = pSprite->m_portraitIcons.Find(reinterpret_cast<int*>(static_cast<INT_PTR>(nIcon)), NULL);
            if (pos != NULL) {
                pSprite->m_portraitIcons.RemoveAt(pos);

                CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
                pGame->UpdatePortrait(pGame->GetCharacterPortraitNum(pSprite->GetId()), 1);
            }
        }

        CGameEffect* pEffect = IcewindMisc::CreateEffectRemoveAllOfType(pSprite, CGAMEEFFECT_CHARM, 0);
        CMessage* message = new CMessageAddEffect(pEffect, pSprite->GetId(), pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    m_done = TRUE;
    PlaySound(CResRef("EFF_E05"), pSprite);
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectCHR::CGameEffectCHR(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x493A60
CGameEffect* CGameEffectCHR::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectCHR* copy = new CGameEffectCHR(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4A69E0
BOOL CGameEffectCHR::ApplyEffect(CGameSprite* pSprite)
{
    SHORT nValue = 1;
    SHORT nSum;

    switch (m_dwFlags) {
    case 0:
        if (m_durationType == 1) {
            nSum = static_cast<SHORT>(m_effectAmount) + pSprite->m_baseStats.m_CHRBase;
            if (nSum > 0) {
                nValue = nSum;
                if (nSum > 40) {
                    nValue = 40;
                }
            }
            pSprite->m_baseStats.m_CHRBase = static_cast<BYTE>(nValue);
            m_forceRepass = TRUE;
            m_done = TRUE;
            return TRUE;
        }

        // A temporary bonus counts only by its excess over the largest
        // bonus already applied this pass (m_derivedStats.field_13C).
        if (m_effectAmount < 1) {
            if (m_effectAmount >= 0) {
                m_done = FALSE;
                return TRUE;
            }
            if (pSprite->m_derivedStats.field_13C <= m_effectAmount) {
                return TRUE;
            }
        } else if (m_effectAmount <= pSprite->m_derivedStats.field_13C) {
            return TRUE;
        }
        pSprite->m_bonusStats.m_nCHR += static_cast<SHORT>(m_effectAmount) - static_cast<SHORT>(pSprite->m_derivedStats.field_13C);
        pSprite->m_derivedStats.field_13C = m_effectAmount;
        m_done = FALSE;
        return TRUE;
    case 1:
        nValue = static_cast<SHORT>(m_effectAmount);
        if (m_durationType != 1) {
            if (nValue < 1) {
                pSprite->m_derivedStats.m_nCHR = 1;
                m_done = FALSE;
                return TRUE;
            }
            if (nValue > 40) {
                nValue = 40;
            }
            pSprite->m_derivedStats.m_nCHR = nValue;
            m_done = FALSE;
            return TRUE;
        }
        if (nValue < 1) {
            nValue = 1;
        } else if (nValue > 40) {
            nValue = 40;
        }
        pSprite->m_baseStats.m_CHRBase = static_cast<BYTE>(nValue);
        m_forceRepass = TRUE;
        m_done = TRUE;
        return TRUE;
    case 2:
        if (m_durationType == 1) {
            nValue = static_cast<SHORT>(static_cast<SHORT>(m_effectAmount) * pSprite->m_baseStats.m_CHRBase / 100);
            if (nValue < 1) {
                nValue = 1;
            } else if (nValue > 40) {
                nValue = 40;
            }
            pSprite->m_baseStats.m_CHRBase = static_cast<BYTE>(nValue);
            m_forceRepass = TRUE;
            m_done = TRUE;
            return TRUE;
        }
        nValue = static_cast<SHORT>(pSprite->m_baseStats.m_CHRBase * static_cast<SHORT>(m_effectAmount) / 100);
        if (nValue < 1) {
            pSprite->m_derivedStats.m_nCHR = 1;
            m_done = FALSE;
            return TRUE;
        }
        if (nValue > 40) {
            nValue = 40;
        }
        pSprite->m_derivedStats.m_nCHR = nValue;
        m_done = FALSE;
        return TRUE;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 4176
        UTIL_ASSERT(FALSE);
        return TRUE;
    }
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectColorChange::CGameEffectColorChange(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x493B10
CGameEffect* CGameEffectColorChange::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectColorChange* copy = new CGameEffectColorChange(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4A6BE0
BOOL CGameEffectColorChange::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->m_hasColorRangeEffects = TRUE;

    if (m_slotNum == pSprite->sub_726800()
        && (m_dwFlags & 0xF0) == CGameAnimationType::RANGE_WEAPON) {
        m_dwFlags &= 0xF;
        m_dwFlags |= CGameAnimationType::RANGE_SHIELD;
    }

    if (m_dwFlags == 255) {
        // NOTE: Uninline.
        pSprite->GetAnimation()->SetColorRangeAll(static_cast<BYTE>(m_effectAmount));

        CColorRange* pColorRange = new CColorRange();
        pColorRange->m_range = -1;
        pColorRange->m_color = static_cast<BYTE>(m_effectAmount);
        pSprite->GetDerivedStats()->m_appliedColorRanges.AddTail(pColorRange);
    } else {
        CColorRange* pColorRange = new CColorRange();
        pColorRange->m_range = static_cast<BYTE>(m_dwFlags);
        pColorRange->m_color = static_cast<BYTE>(m_effectAmount);
        pSprite->GetDerivedStats()->m_appliedColorRanges.AddTail(pColorRange);

        // NOTE: Uninline.
        pSprite->GetAnimation()->SetColorRange(static_cast<BYTE>(m_dwFlags),
            static_cast<BYTE>(m_effectAmount));
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectColorGlowSolid::CGameEffectColorGlowSolid(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x493BC0
CGameEffect* CGameEffectColorGlowSolid::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectColorGlowSolid* copy = new CGameEffectColorGlowSolid(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4A6D30
BOOL CGameEffectColorGlowSolid::ApplyEffect(CGameSprite* pSprite)
{
    if (m_slotNum == pSprite->sub_726800()
        && (m_dwFlags & 0xF0) == CGameAnimationType::RANGE_WEAPON) {
        m_dwFlags &= 0xF;
        m_dwFlags |= CGameAnimationType::RANGE_SHIELD;
    }

    pSprite->m_hasColorEffects = TRUE;

    COLORREF tintColor = m_effectAmount >> 8;

    if (m_dwFlags == 255) {
        // NOTE: Uninline.
        pSprite->GetAnimation()->SetColorEffectAll(1, tintColor, 1);

        CColorEffect* pColorEffect = new CColorEffect();
        pColorEffect->m_effectType = 1;
        pColorEffect->m_range = -1;
        pColorEffect->m_tintColor = tintColor;
        pColorEffect->m_periodLength = 1;
        pSprite->GetDerivedStats()->m_appliedColorEffects.AddTail(pColorEffect);
    } else {
        // NOTE: Uninline.
        pSprite->GetAnimation()->SetColorEffect(1, static_cast<BYTE>(m_dwFlags), tintColor, 1);

        CColorEffect* pColorEffect = new CColorEffect();
        pColorEffect->m_effectType = 1;
        pColorEffect->m_range = static_cast<BYTE>(m_dwFlags);
        pColorEffect->m_tintColor = tintColor;
        pColorEffect->m_periodLength = 1;
        pSprite->GetDerivedStats()->m_appliedColorEffects.AddTail(pColorEffect);
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectColorGlowPulse::CGameEffectColorGlowPulse(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x493C70
CGameEffect* CGameEffectColorGlowPulse::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectColorGlowPulse* copy = new CGameEffectColorGlowPulse(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4A6E90
BOOL CGameEffectColorGlowPulse::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->m_hasColorEffects = TRUE;

    COLORREF tintColor = m_effectAmount >> 8;
    WORD range = m_dwFlags & 0xFFFF;
    WORD period = (m_dwFlags >> 16) & 0xFFFF;

    if (m_slotNum == pSprite->sub_726800()
        && (range & 0xF0) == CGameAnimationType::RANGE_WEAPON) {
        range &= 0xF;
        range |= CGameAnimationType::RANGE_SHIELD;
    }

    if (range == 255) {
        // NOTE: Uninline.
        pSprite->GetAnimation()->SetColorEffectAll(1, tintColor, static_cast<BYTE>(period));

        CColorEffect* pColorEffect = new CColorEffect();
        pColorEffect->m_effectType = 1;
        pColorEffect->m_range = -1;
        pColorEffect->m_tintColor = tintColor;
        pColorEffect->m_periodLength = static_cast<BYTE>(period);
        pSprite->GetDerivedStats()->m_appliedColorEffects.AddTail(pColorEffect);
    } else {
        // NOTE: Uninline.
        pSprite->GetAnimation()->SetColorEffect(1, static_cast<BYTE>(range), tintColor, static_cast<BYTE>(period));

        CColorEffect* pColorEffect = new CColorEffect();
        pColorEffect->m_effectType = 1;
        pColorEffect->m_range = static_cast<BYTE>(range);
        pColorEffect->m_tintColor = tintColor;
        pColorEffect->m_periodLength = static_cast<BYTE>(period);
        pSprite->GetDerivedStats()->m_appliedColorEffects.AddTail(pColorEffect);
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectCON::CGameEffectCON(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x493F30
CGameEffect* CGameEffectCON::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectCON* copy = new CGameEffectCON(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4A74B0
BOOL CGameEffectCON::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_CONBase = min(max(pSprite->GetBaseStats()->m_CONBase + static_cast<SHORT>(m_effectAmount), 1), 40);
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            // NOTE: Uninline.
            AdjustCON(pSprite, m_effectAmount);
            m_done = FALSE;
        }
        break;
    case 1:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_CONBase = min(max(static_cast<SHORT>(m_effectAmount), 1), 40);
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->GetDerivedStats()->m_nCON = min(max(static_cast<SHORT>(m_effectAmount), 1), 40);
            m_done = FALSE;
        }
        break;
    case 2:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_CONBase = min(max(pSprite->GetBaseStats()->m_CONBase * static_cast<SHORT>(m_effectAmount) / 100, 1), 40);
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->GetDerivedStats()->m_nCON = min(max(pSprite->GetBaseStats()->m_CONBase * static_cast<SHORT>(m_effectAmount) / 100, 1), 40);
            m_done = FALSE;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 4660
        UTIL_ASSERT(FALSE);
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectCurePoison::CGameEffectCurePoison(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x493FE0
CGameEffect* CGameEffectCurePoison::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectCurePoison* copy = new CGameEffectCurePoison(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4A76B0
BOOL CGameEffectCurePoison::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetEquipedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_POISON,
        pSprite->GetEquipedEffectList()->GetPosCurrent(),
        -1);
    pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_POISON,
        pSprite->GetTimedEffectList()->GetPosCurrent(),
        -1);

    pSprite->GetEquipedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_PORTRAITICON,
        pSprite->GetEquipedEffectList()->GetPosCurrent(),
        6);
    pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_PORTRAITICON,
        pSprite->GetTimedEffectList()->GetPosCurrent(),
        6);

    pSprite->GetBaseStats()->m_generalState &= ~STATE_POISONED;
    pSprite->GetDerivedStats()->m_generalState &= ~STATE_POISONED;

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectDamage::CGameEffectDamage()
{
    m_effectID = CGAMEEFFECT_DAMAGE;
    m_special = -1;
}

// NOTE: Inlined.
CGameEffectDamage::CGameEffectDamage(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, TRUE)
{
    if (m_special == 0) {
        m_special = -1;
    }
}

// 0x494090
CGameEffect* CGameEffectDamage::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectDamage* copy = new CGameEffectDamage(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4A7750
void CGameEffectDamage::sub_4A7750(CGameSprite* pSprite)
{
    if ((pSprite->GetBaseStats()->m_generalState & STATE_DEAD) == 0) {
        if ((pSprite->GetBaseStats()->m_generalState & STATE_SLEEPING) != 0
            || (pSprite->GetDerivedStats()->m_generalState & STATE_SLEEPING) != 0) {
            if (!pSprite->GetDerivedStats()->m_spellStates.test(SPLSTATE_DOESNT_AWAKEN_ON_DAMAGE)) {
                pSprite->GetBaseStats()->m_generalState &= ~(STATE_SLEEPING | STATE_HELPLESS);
                pSprite->GetDerivedStats()->m_generalState &= ~(STATE_SLEEPING | STATE_HELPLESS);

                // NOTE: Uninline.
                RemoveAllOfType(pSprite, CGAMEEFFECT_SLEEP, -1);

                // NOTE: Uninline.
                RemoveAllOfType(pSprite, CGAMEEFFECT_PORTRAITICON, 14);
            }
        }

        if ((pSprite->GetDerivedStats()->m_generalState & STATE_STUNNED) != 0) {
            if (pSprite->GetDerivedStats()->m_spellStates.test(SPLSTATE_UNSTUN_ON_DAMAGE)) {
                pSprite->GetBaseStats()->m_generalState &= ~STATE_STUNNED;
                pSprite->GetDerivedStats()->m_generalState &= ~STATE_STUNNED;

                // NOTE: Uninline.
                RemoveAllOfType(pSprite, CGAMEEFFECT_STUN, -1);

                // NOTE: Uninline.
                RemoveAllOfType(pSprite, CGAMEEFFECT_PORTRAITICON, 44);

                // NOTE: Uninline.
                RemoveAllOfType(pSprite, CGAMEEFFECT_HOLDCREATURE, -1);

                // NOTE: Uninline.
                RemoveAllOfType(pSprite, CGAMEEFFECT_PORTRAITICON, 13);
            }
        }
    }
}

// 0x4A7900
BOOL CGameEffectDamage::ApplyEffect(CGameSprite* pSprite)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameObject* pSource;
    BYTE rc;

    pSprite->m_hasColorEffects = TRUE;

    WORD nMode = static_cast<WORD>(m_dwFlags);
    DWORD dwDamageType = m_dwFlags & 0xFFFF0000;

    // Monks above level 8 shrug off poison damage.
    if (dwDamageType == 0x200000 && pSprite->m_derivedStats.GetClassLevel(4) > 8) {
        m_done = TRUE;
        return TRUE;
    }

    INT nOldHitPoints = pSprite->m_baseStats.m_hitPoints;

    // NOTE: Inlined CGameAnimation::IsInvulnerable (ObjAnimation.h line 2100
    // asserts m_animation).
    if (pSprite->GetAnimation()->m_animation->IsInvulnerable()) {
        m_done = TRUE;
        return TRUE;
    }

    if (pGame->m_gameSave.m_cutScene == TRUE) {
        m_done = TRUE;
        return TRUE;
    }

    SHORT nDirection;
    if (m_sourceID == CGameObjectArray::INVALID_INDEX) {
        nDirection = pSprite->GetDirection(m_source);
    } else {
        do {
            rc = pGame->GetObjectArray()->GetShare(m_sourceID,
                CGameObjectArray::THREAD_ASYNCH,
                &pSource,
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            nDirection = pSprite->GetDirection(pSource->GetPos());
            pGame->GetObjectArray()->ReleaseShare(m_sourceID,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        } else {
            nDirection = 0;
        }
    }

    CGameEffect* pDeath;
    if ((pSprite->m_baseStats.m_generalState & STATE_STONE_DEATH) != 0) {
        pDeath = new CGameEffectDeath();
        pDeath->m_source = m_source;
        pDeath->m_sourceID = m_sourceID;
        pDeath->m_dwFlags = 0x40;
        pDeath->m_casterLevel = m_casterLevel;
        pSprite->AddEffect(pDeath, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);
        m_done = TRUE;
        return TRUE;
    }

    if ((pSprite->m_baseStats.m_generalState & STATE_FROZEN_DEATH) != 0) {
        pDeath = new CGameEffectDeath();
        pDeath->m_source = m_source;
        pDeath->m_sourceID = m_sourceID;
        pDeath->m_dwFlags = 0x80;
        pDeath->m_casterLevel = m_casterLevel;
        pSprite->AddEffect(pDeath, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);
        m_done = TRUE;
        return TRUE;
    }

    if (pSprite->m_baseStats.m_hitPoints < -9
        || (pSprite->m_baseStats.m_generalState & STATE_DEAD) != 0) {
        m_done = TRUE;
        return TRUE;
    }

    INT nRolled = 0;
    for (BYTE nDie = 0; nDie < m_numDice; nDie++) {
        nRolled += CUtil::UtilRandInt(m_diceSize, -pSprite->m_tempStats.m_nLuck) + 1;
    }
    m_effectAmount += nRolled;

    if (m_effectAmount <= 0) {
        m_done = TRUE;
        return TRUE;
    }

    SHORT nOldSequence = pSprite->m_nSequence;

    if (nMode == 3) {
        m_effectAmount /= 2;
        if (m_effectAmount < 1) {
            m_effectAmount = 1;
        }
    }

    INT nResisted = 0;
    if (nMode == 0 || nMode == 3) {
        if (pGame->GetCharacterPortraitNum(pSprite->GetId()) != -1) {
            // The difficulty bonus: a harder setting adds to the damage a
            // party member takes, unless both sides are party members.
            BOOLEAN bApply = TRUE;

            do {
                rc = pGame->GetObjectArray()->GetShare(m_sourceID,
                    CGameObjectArray::THREAD_ASYNCH,
                    &pSource,
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

            if (rc == CGameObjectArray::SUCCESS) {
                if (pSource->GetObjectType() == CGameObject::TYPE_SPRITE
                    && IcewindMisc::sub_585310(pSprite) == TRUE
                    && IcewindMisc::sub_585310(static_cast<CGameSprite*>(pSource)) == TRUE) {
                    bApply = FALSE;
                }

                pGame->GetObjectArray()->ReleaseShare(m_sourceID,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            }

            LONG nMultiplier;
            if (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL) {
                nMultiplier = pGame->GetOptions()->m_nDifficultyMultiplier;
            } else {
                nMultiplier = pGame->GetOptions()->m_nMPDifficultyMultiplier;
            }

            if (pGame->GetOptions()->m_bSuppressExtraDifficultyDamage == TRUE) {
                bApply = FALSE;
            }

            if (nMultiplier <= 0 || bApply) {
                m_effectAmount += m_effectAmount * nMultiplier / 100;
            }
        }

        INT nOriginalAmount = m_effectAmount;

        if (pSprite->m_derivedStats.m_spellStates.test(SPLSTATE_TORTOISE_SHELL)) {
            // The shell takes the damage.
            pSprite->m_derivedStats.field_124 -= nOriginalAmount;

            POSITION pos = pSprite->m_timedEffectList.GetHeadPosition();
            while (pos != NULL) {
                CGameEffect* pEffect = pSprite->m_timedEffectList.GetNext(pos);
                if (pEffect->m_effectID == ICEWIND_CGAMEEFFECT_TORTOISESHELL) {
                    pEffect->m_effectAmount = pSprite->m_derivedStats.field_124;
                }
            }

            pos = pSprite->m_equipedEffectList.GetHeadPosition();
            INT nRemaining = pSprite->m_derivedStats.field_124;
            while (pos != NULL) {
                CGameEffect* pEffect = pSprite->m_equipedEffectList.GetNext(pos);
                if (pEffect->m_effectID == ICEWIND_CGAMEEFFECT_TORTOISESHELL) {
                    pEffect->m_effectAmount = nRemaining;
                }
            }

            sub_4AB0A0(pSprite, m_effectAmount);

            if (pSprite->m_derivedStats.field_124 <= 0) {
                pSprite->m_timedEffectList.RemoveAllOfType(pSprite,
                    ICEWIND_CGAMEEFFECT_TORTOISESHELL,
                    pSprite->m_timedEffectList.m_posCurrent,
                    -1);
                pSprite->m_equipedEffectList.RemoveAllOfType(pSprite,
                    ICEWIND_CGAMEEFFECT_TORTOISESHELL,
                    pSprite->m_equipedEffectList.m_posCurrent,
                    -1);

                // NOTE: Uninline.
                DisplayStringRef(pSprite, 17096);
            }

            sub_4A7750(pSprite);

            m_done = TRUE;
            return TRUE;
        }

        do {
            rc = pGame->GetObjectArray()->GetShare(m_sourceID,
                CGameObjectArray::THREAD_ASYNCH,
                &pSource,
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        // Resistances; the attacker's elemental feats first add a fifth.
        switch (dwDamageType) {
        case 0x0:
            // Crushing.
            m_effectAmount -= pSprite->m_tempStats.m_nResistCrushing;
            break;
        case 0x10000:
            // Acid.
            if (rc == CGameObjectArray::SUCCESS
                && pSource->GetObjectType() == CGameObject::TYPE_SPRITE
                && static_cast<CGameSprite*>(pSource)->sub_763150(CGAMESPRITE_FEAT_AQUA_MORTIS) == TRUE) {
                m_effectAmount = static_cast<LONG>(static_cast<float>(m_effectAmount) * 1.2);
            }
            m_effectAmount -= pSprite->m_tempStats.m_nResistAcid;
            if (pSprite->m_derivedStats.m_spellStates.test(SPLSTATE_IRON_BODY)) {
                m_effectAmount /= 2;
            }
            break;
        case 0x20000:
            // Cold.
            if (rc == CGameObjectArray::SUCCESS
                && pSource->GetObjectType() == CGameObject::TYPE_SPRITE
                && static_cast<CGameSprite*>(pSource)->sub_763150(CGAMESPRITE_FEAT_AEGIS_OF_RIME) == TRUE) {
                m_effectAmount = static_cast<LONG>(static_cast<float>(m_effectAmount) * 1.2);
            }
            m_effectAmount -= pSprite->m_tempStats.m_nResistCold;
            if (pSprite->m_derivedStats.m_spellStates.test(SPLSTATE_RED_FIRESHIELD)) {
                m_effectAmount /= 2;
            }
            break;
        case 0x40000:
            // Electricity.
            if (rc == CGameObjectArray::SUCCESS
                && pSource->GetObjectType() == CGameObject::TYPE_SPRITE
                && static_cast<CGameSprite*>(pSource)->sub_763150(CGAMESPRITE_FEAT_SCION_OF_STORMS) == TRUE) {
                m_effectAmount = static_cast<LONG>(static_cast<float>(m_effectAmount) * 1.2);
            }
            m_effectAmount -= pSprite->m_tempStats.m_nResistElectricity;
            if (pSprite->m_derivedStats.m_spellStates.test(SPLSTATE_IRON_BODY)) {
                m_effectAmount = 0;
            }
            break;
        case 0x80000:
            // Fire.
            if (rc == CGameObjectArray::SUCCESS
                && pSource->GetObjectType() == CGameObject::TYPE_SPRITE
                && static_cast<CGameSprite*>(pSource)->sub_763150(CGAMESPRITE_FEAT_SPIRIT_OF_FLAME) == TRUE) {
                m_effectAmount = static_cast<LONG>(static_cast<float>(m_effectAmount) * 1.2);
            }
            m_effectAmount -= pSprite->m_tempStats.m_nResistFire;
            if (pSprite->m_derivedStats.m_spellStates.test(SPLSTATE_BLUE_FIRESHIELD)) {
                m_effectAmount /= 2;
            }
            if (pSprite->m_derivedStats.m_spellStates.test(SPLSTATE_IRON_BODY)) {
                m_effectAmount /= 2;
            }
            break;
        case 0x100000:
            // Piercing.
            m_effectAmount -= pSprite->m_tempStats.m_nResistPiercing;
            break;
        case 0x200000:
            // Poison.
            m_effectAmount -= pSprite->m_tempStats.m_nResistPoison;
            break;
        case 0x400000:
            // Magic.
            m_effectAmount -= pSprite->m_tempStats.m_nMagicDamageResistance;
            break;
        case 0x1000000:
            // Slashing.
            m_effectAmount -= pSprite->m_tempStats.m_nResistSlashing;
            break;
        case 0x2000000:
            // Missile, then piercing.
            m_effectAmount -= pSprite->m_tempStats.m_nResistMissile;
            m_effectAmount -= pSprite->m_tempStats.m_nResistPiercing;
            break;
        case 0x4000000:
            // Missile, then crushing.
            m_effectAmount -= pSprite->m_tempStats.m_nResistMissile;
            m_effectAmount -= pSprite->m_tempStats.m_nResistCrushing;
            break;
        }

        if (rc == CGameObjectArray::SUCCESS) {
            pGame->GetObjectArray()->ReleaseShare(m_sourceID,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }

        if (m_effectAmount < 0) {
            m_effectAmount = 0;
        }

        // A low-level party member is not killed outright by one hit.
        if (pGame->GetCharacterPortraitNum(pSprite->GetId()) != -1) {
            SHORT nMaxHitPoints = pSprite->m_tempStats.m_nMaxHitPoints;
            if (m_effectAmount > nMaxHitPoints && nMaxHitPoints < 14) {
                m_effectAmount = nMaxHitPoints - 1;
            }
        }

        if (m_special != -1) {
            m_effectAmount = pSprite->m_derivedStats.m_damageReductions.Apply(m_effectAmount, m_special, dwDamageType);
        }

        if (m_effectAmount < 0) {
            m_effectAmount = 0;
        }

        nResisted = nOriginalAmount - m_effectAmount;
    }

    if (m_effectAmount > 0) {
        pSprite->field_5582 = m_effectAmount;
    }

    CMessage* message;
    if (dwDamageType == 0x200000) {
        if (pSprite->field_72A0 < 1) {
            pSprite->field_72A0 = 100;
            message = new CMessagePlaySound(6, TRUE, TRUE, pSprite->GetId(), pSprite->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
    } else {
        message = new CMessagePlaySound(6, TRUE, TRUE, pSprite->GetId(), pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    SHORT nHitPoints;
    SHORT nHitPointsBefore;
    INT nProduct;
    switch (nMode) {
    case 0:
    case 3:
        // NOTE: The original calls it through the vtable (+0x28).
        sub_4AA6D0(pSprite, nResisted);
        pSprite->m_baseStats.m_hitPoints -= static_cast<SHORT>(m_effectAmount);
        break;
    case 1:
        nHitPoints = pSprite->m_tempStats.m_nMaxHitPoints;
        nHitPointsBefore = pSprite->m_baseStats.m_hitPoints;
        if (m_effectAmount < nHitPoints) {
            nHitPoints = static_cast<SHORT>(m_effectAmount);
        }
        pSprite->m_baseStats.m_hitPoints = nHitPoints;
        m_effectAmount = nHitPointsBefore - m_effectAmount;
        sub_4AA6D0(pSprite, nResisted);
        break;
    case 2:
        nProduct = pSprite->m_baseStats.m_hitPoints * m_effectAmount;
        pSprite->m_baseStats.m_hitPoints = static_cast<SHORT>(nProduct / 100);
        break;
    }

    pSprite->m_bBloodFlashOn = TRUE;
    pSprite->m_nBloodFlashAmount = 128;
    pSprite->m_nDamageLocatorTime = 128;

    sub_4A8DF0(dwDamageType, pSprite);

    BYTE nDuration = static_cast<BYTE>(3 * nOldHitPoints - 3 * pSprite->m_baseStats.m_hitPoints);
    switch (dwDamageType) {
    case 0x10000:
        PlaySound(CResRef("EFF_M47"), pSprite);
        pSprite->StartSpriteEffect(4, 0, nDuration, TRUE);
        break;
    case 0x20000:
        PlaySound(CResRef("EFF_M46"), pSprite);
        pSprite->StartSpriteEffect(3, 0, nDuration, TRUE);
        break;
    case 0x40000:
        PlaySound(CResRef("MISC_02C"), pSprite);
        pSprite->StartSpriteEffect(2, 0, nDuration, TRUE);
        break;
    case 0x80000:
        PlaySound(CResRef("MISC_01C"), pSprite);
        pSprite->StartSpriteEffect(1, 0, nDuration, TRUE);
        break;
    case 0x0:
    case 0x100000:
    case 0x1000000:
    case 0x2000000:
    case 0x4000000:
        pSprite->AddBlood(0, nDirection, nOldHitPoints - pSprite->m_baseStats.m_hitPoints < 6 ? 5 : 4);
        break;
    case 0x400000:
        PlaySound(CResRef("HIT_07"), pSprite);
        break;
    }

    if (nOldSequence != CGAMESPRITE_SEQ_DAMAGE
        && nOldSequence != CGAMESPRITE_SEQ_DIE
        && nOldSequence != CGAMESPRITE_SEQ_SLEEP
        && nOldSequence != CGAMESPRITE_SEQ_TWITCH) {
        pSprite->SetSequence(CGAMESPRITE_SEQ_DAMAGE);
    }

    if (pSprite->m_derivedStats.m_nMinHitPoints > 0 && pSprite->m_baseStats.m_hitPoints < 1) {
        pSprite->m_baseStats.m_hitPoints = static_cast<SHORT>(pSprite->m_derivedStats.m_nMinHitPoints);
    }

    BOOL bAlive = TRUE;
    if (pSprite->m_baseStats.m_hitPoints > 0) {
        if (pSprite->sub_763150(CGAMESPRITE_FEAT_HEROIC_INSPIRATION) == TRUE
            && pSprite->m_baseStats.m_hitPoints <= static_cast<SHORT>(pSprite->m_baseStats.m_maxHitPointsBase >> 1)) {
            POSITION pos = pSprite->m_timedEffectList.GetHeadPosition();
            while (TRUE) {
                if (pos == NULL) {
                    ITEM_EFFECT effect;
                    memset(&effect, 0, sizeof(ITEM_EFFECT));
                    effect.effectID = ICEWIND_CGAMEEFFECT_HEROICINSPIRATION;
                    effect.durationType = 1;
                    effect.probabilityUpper = 100;

                    CGameEffect* pEffect = CGameEffect::DecodeEffect(&effect,
                        pSprite->GetPos(),
                        pSprite->GetId(),
                        CPoint(-1, -1));
                    pEffect->m_flags &= ~0x1;

                    message = new CMessageAddEffect(pEffect,
                        pSprite->GetId(),
                        pSprite->GetId());
                    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
                    break;
                }

                CGameEffect* pEffect = pSprite->m_timedEffectList.GetNext(pos);
                if (pEffect->m_effectID == ICEWIND_CGAMEEFFECT_HEROICINSPIRATION) {
                    break;
                }
            }

            pSprite->FeedBack(80, 0, 0, 0, -1, 0, 0);
        }
    } else {
        bAlive = FALSE;

        CGameEffect* pEffect = new CGameEffectDeath();
        pEffect->m_source = m_source;
        pEffect->m_sourceID = m_sourceID;

        switch (dwDamageType) {
        case 0x8000000:
            if (IcewindMisc::IsUndead(pSprite) == TRUE) {
                dwDamageType = 0;
                if (pSprite->GetAIType().m_nEnemyAlly != 2 || pSprite->m_baseStats.m_hitPoints > -31) {
                    if (pSprite->GetAIType().m_nEnemyAlly == 2) {
                        pEffect->m_dwFlags = 4;
                    } else if (pSprite->m_baseStats.m_hitPoints > -11) {
                        pEffect->m_dwFlags = 4;
                    } else {
                        pEffect->m_dwFlags = 8;
                    }
                } else {
                    pEffect->m_dwFlags = 8;
                }
                break;
            }

            // Non-lethal damage: a sprite not immune to sleep sleeps it
            // off instead of dying.
            if (pSprite->m_derivedStats.m_naturalImmunities.find(CGAMEEFFECT_SLEEP) == pSprite->m_derivedStats.m_naturalImmunities.end()) {
                delete pEffect;

                ITEM_EFFECT* effect = new ITEM_EFFECT;
                memset(effect, 0, sizeof(ITEM_EFFECT));
                effect->effectID = CGAMEEFFECT_SLEEP;
                effect->durationType = 0;
                effect->probabilityUpper = 100;
                effect->duration = pSprite->m_baseStats.m_hitPoints * -15 + 15;

                pEffect = CGameEffect::DecodeEffect(effect, CPoint(-1, -1), -1, CPoint(-1, -1));
                pSprite->m_baseStats.m_hitPoints = 1;
                delete effect;
            }
            break;
        case 0x0:
        case 0x100000:
        case 0x1000000:
            if (pSprite->GetAIType().m_nEnemyAlly != 2 || pSprite->m_baseStats.m_hitPoints > -31) {
                if (pSprite->GetAIType().m_nEnemyAlly == 2) {
                    pEffect->m_dwFlags = 4;
                } else if (pSprite->m_baseStats.m_hitPoints > -11) {
                    pEffect->m_dwFlags = 4;
                } else {
                    pEffect->m_dwFlags = 8;
                }
            } else {
                pEffect->m_dwFlags = 8;
            }
            break;
        case 0x10000:
            if (pSprite->m_baseStats.m_hitPoints > -6) {
                pEffect->m_dwFlags = 4;
            } else {
                pEffect->m_dwFlags = 0;
            }
            break;
        case 0x20000:
            if (pSprite->m_baseStats.m_hitPoints > -6) {
                pEffect->m_dwFlags = 4;
            } else {
                pEffect->m_dwFlags = 0x20;
            }
            break;
        case 0x40000:
            if (pSprite->m_baseStats.m_hitPoints > -6) {
                pEffect->m_dwFlags = 4;
            } else {
                pEffect->m_dwFlags = 0x100;
            }
            break;
        case 0x80000:
            if (pSprite->m_baseStats.m_hitPoints < -5) {
                pEffect->m_dwFlags = 1;
            } else {
                pEffect->m_dwFlags = 4;
            }
            break;
        case 0x200000:
        case 0x400000:
        case 0x2000000:
        case 0x4000000:
        case 0x20000000:
        case 0x40000000:
            pEffect->m_dwFlags = 4;
            break;
        case 0x10000000:
            pEffect->m_dwFlags = 8;
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
            // __LINE__: 5402
            UTIL_ASSERT_MSG(FALSE, "WIERD damage");
            break;
        }

        CPoint ptPos = pSprite->GetPos();
        pEffect->m_casterLevel = m_casterLevel;
        pSprite->AddEffect(pEffect, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);

        if (dwDamageType == 0x10000000) {
            rc = pGame->GetObjectArray()->GetShare(m_sourceID,
                CGameObjectArray::THREAD_ASYNCH,
                &pSource,
                INFINITE);
            if (rc == CGameObjectArray::SUCCESS) {
                std::list<CGameEffect*> effects = IcewindMisc::GetSpellResrefEffects(pSource,
                    reinterpret_cast<const BYTE*>("EffSE"),
                    static_cast<BYTE>(m_casterLevel));
                for (std::list<CGameEffect*>::iterator it = effects.begin(); it != effects.end(); it++) {
                    (*it)->m_sourceRes = m_sourceRes;
                    (*it)->m_target = ptPos;
                    (*it)->m_casterLevel = m_casterLevel;
                    // NOTE: Unchecked, as the original calls vtable +0x78 on
                    // the source whatever its type (0x4A8AC3).
                    static_cast<CGameAIBase*>(pSource)->AddEffect(*it, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);
                }
            }

            // NOTE: The original releases the share even when GetShare
            // failed.
            pGame->GetObjectArray()->ReleaseShare(m_sourceID,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }

    CInfCursor* pCursor = g_pBaldurChitin->GetObjectCursor();
    if (pCursor->m_nCurrentCursor == 101
        && pSprite->GetArea()->m_iPicked == pSprite->GetId()) {
        pSprite->SetCharacterToolTip(pCursor->field_A02);
    }

    // The hurt cry: when hit points fall below 30% of the maximum, or now
    // and then while below it.
    INT nMaxHitPoints = pSprite->m_tempStats.m_nMaxHitPoints;
    INT nPercent;
    if (nOldHitPoints * 100 / nMaxHitPoints < 31
        || (nPercent = pSprite->m_baseStats.m_hitPoints * 100 / nMaxHitPoints) > 29
        || nPercent < 1) {
        nPercent = pSprite->m_baseStats.m_hitPoints * 100 / nMaxHitPoints;
        if (nPercent < 30 && nPercent > 0 && rand() % 10 == 0) {
            message = new CMessagePlaySound(8, TRUE, FALSE, pSprite->GetId(), pSprite->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            pSprite->AutoPause(8);
        }
    } else {
        message = new CMessagePlaySound(8, TRUE, FALSE, pSprite->GetId(), pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        pSprite->AutoPause(8);
    }

    if (bAlive == TRUE) {
        sub_4A7750(pSprite);
    }

    m_done = TRUE;
    return TRUE;
}

// 0x4A8DF0
void CGameEffectDamage::sub_4A8DF0(DWORD dwDamageType, CGameSprite* pSprite)
{
    // The hit sound: "H_" + the damage's sound group + the target's armour
    // material + a random digit, the digit moved on by one (wrapping) when
    // the name equals the sprite's last hit sound.
    CString sPrefix("H_");

    USHORT nAnimationID = pSprite->GetAnimation()->m_animation->m_animationID;
    BYTE nPersonalSpace = pSprite->GetAnimation()->GetPersonalSpace();

    CString sSound;
    BYTE nArmor[2];
    nArmor[0] = 0;
    nArmor[1] = 0;

    WORD nArmorType = 0;
    if (pSprite->m_equipment.m_items[CGameSpriteEquipment::SLOT_ARMOR] != NULL) {
        nArmorType = pSprite->m_equipment.m_items[CGameSpriteEquipment::SLOT_ARMOR]->GetAnimationType();
        if (nArmorType != 0) {
            nArmor[0] = static_cast<BYTE>(nArmorType >> 8);
            nArmor[1] = static_cast<BYTE>(nArmorType);
        }
    } else {
        nAnimationID = pSprite->GetAnimation()->m_animation->m_animationID;
    }

    // The armour's material from its animation code ("1A".."4A"), else
    // from the creature: three animations and the big creatures are bone
    // or plate.
    const char* szMaterial;
    INT nDigits;
    switch (dwDamageType) {
    case 0x0:
        sPrefix += "BL_";
        if (nArmorType != 0 && nArmor[1] == 'A') {
            switch (nArmor[0]) {
            case '1':
                szMaterial = "CL";
                nDigits = 3;
                break;
            case '2':
                szMaterial = "LR";
                nDigits = 3;
                break;
            case '3':
                szMaterial = "Ch";
                nDigits = 4;
                break;
            case '4':
                szMaterial = "PT";
                nDigits = 3;
                break;
            default:
                return;
            }
        } else if (nAnimationID == 0x6403 || nAnimationID == 0xE62C || nAnimationID == 0xF019) {
            szMaterial = "BN";
            nDigits = 3;
        } else if (nPersonalSpace < 3) {
            szMaterial = "MS";
            nDigits = 3;
        } else if (nPersonalSpace <= 5) {
            szMaterial = "MM";
            nDigits = 3;
        } else {
            szMaterial = "ML";
            nDigits = 4;
        }
        break;
    case 0x100000:
        sPrefix += "PC_";
        if (nArmorType != 0 && nArmor[1] == 'A') {
            switch (nArmor[0]) {
            case '1':
                szMaterial = "CL";
                nDigits = 4;
                break;
            case '2':
                szMaterial = "LR";
                nDigits = 3;
                break;
            case '3':
                szMaterial = "Ch";
                nDigits = 4;
                break;
            case '4':
                szMaterial = "PT";
                nDigits = 5;
                break;
            default:
                return;
            }
        } else if (nAnimationID == 0x6403 || nAnimationID == 0xE62C || nAnimationID == 0xF019) {
            szMaterial = "BN";
            nDigits = 3;
        } else if (nPersonalSpace < 3) {
            szMaterial = "MS";
            nDigits = 5;
        } else if (nPersonalSpace <= 5) {
            szMaterial = "MM";
            nDigits = 4;
        } else {
            szMaterial = "ML";
            nDigits = 4;
        }
        break;
    case 0x1000000:
        sPrefix += "SL_";
        if (nArmorType != 0 && nArmor[1] == 'A') {
            switch (nArmor[0]) {
            case '1':
                szMaterial = "CL";
                nDigits = 3;
                break;
            case '2':
                szMaterial = "LR";
                nDigits = 3;
                break;
            case '3':
                szMaterial = "Ch";
                nDigits = 3;
                break;
            case '4':
                szMaterial = "PT";
                nDigits = 4;
                break;
            default:
                return;
            }
        } else if (nAnimationID == 0x6403 || nAnimationID == 0xE62C || nAnimationID == 0xF019) {
            szMaterial = "BN";
            nDigits = 3;
        } else if (nPersonalSpace < 3) {
            szMaterial = "MS";
            nDigits = 4;
        } else if (nPersonalSpace <= 5) {
            szMaterial = "MM";
            nDigits = 3;
        } else {
            szMaterial = "ML";
            nDigits = 3;
        }
        break;
    case 0x2000000:
    case 0x4000000:
        sPrefix += "ML_";
        if (nArmorType != 0 && nArmor[1] == 'A') {
            switch (nArmor[0]) {
            case '1':
                szMaterial = "CL";
                nDigits = 4;
                break;
            case '2':
                szMaterial = "LR";
                nDigits = 3;
                break;
            case '3':
                szMaterial = "Ch";
                nDigits = 5;
                break;
            case '4':
                szMaterial = "PT";
                nDigits = 5;
                break;
            default:
                return;
            }
        } else if (nAnimationID == 0x6403 || nAnimationID == 0xE62C || nAnimationID == 0xF019) {
            szMaterial = "BN";
            nDigits = 4;
        } else if (nPersonalSpace < 3) {
            szMaterial = "MS";
            nDigits = 5;
        } else if (nPersonalSpace <= 5) {
            szMaterial = "MM";
            nDigits = 4;
        } else {
            szMaterial = "ML";
            nDigits = 5;
        }
        break;
    case 0x8000000:
        sPrefix = "HIT_03A";
        szMaterial = NULL;
        nDigits = 0;
        break;
    default:
        return;
    }

    if (szMaterial != NULL) {
        sPrefix += szMaterial;

        char nDigit = static_cast<char>(rand() % nDigits + '1');
        sSound = sPrefix;
        sSound += nDigit;

        // The same sound twice in a row moves on to the next, wrapping to
        // the first.
        if (strcmp(sSound, pSprite->m_sLastHitSound) == 0) {
            if (nDigit - '1' == nDigits - 1) {
                nDigit = '1';
            } else {
                nDigit++;
            }
        }
        sPrefix += nDigit;

        pSprite->m_sLastHitSound = sPrefix;
    }

    CResRef res(sPrefix);

    CMessage* message = new CMessagePlaySoundRef(res, pSprite->GetId(), pSprite->GetId());
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

    CSound cSound;
    cSound.SetResRef(res, TRUE, TRUE);
    cSound.SetChannel(14, reinterpret_cast<DWORD_PTR>(pSprite->GetArea()));

    if (cSound.m_nLooping == 0) {
        cSound.SetFireForget(TRUE);
    }

    cSound.Play(pSprite->GetPos().x, pSprite->GetPos().y, 0, FALSE);
}

// 0x4A9B50
void CGameEffectDamage::OnAddSpecific(CGameSprite* pSprite)
{
    if (m_sourceID == CGameObjectArray::INVALID_INDEX) {
        return;
    }

    CGameObject* pSource;
    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_sourceID,
            CGameObjectArray::THREAD_ASYNCH,
            &pSource,
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        return;
    }

    // Damage from a party-side source makes the target's teams the party's
    // enemies.
    if ((pSprite->field_58C & 0x200000) == 0
        && pSource->GetAIType().m_nEnemyAlly <= CAIObjectType::EA_GOODCUTOFF) {
        pSprite->field_58C |= 0x200000;

        pSprite->sub_6F50A0();
    }

    // HitBy carries the damage type (the flags' upper word).
    CAITrigger hitBy(CAITRIGGER_HITBY, pSource->GetAIType(), m_dwFlags >> 16);
    CMessage* message = new CMessageSetTrigger(hitBy, m_sourceID, pSprite->GetId());
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

    CAITrigger attackedBy(CAITRIGGER_ATTACKEDBY, pSource->GetAIType(), 0);
    message = new CMessageSetTrigger(attackedBy, m_sourceID, pSprite->GetId());
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceID,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);
}

// NOTE: The original inlines this feedback twice in CheckSave (0x4AA118 and
// 0x4AA3B1): the evaded effect's item or spell name, else no name.
static void FeedBackEffectsEvaded(CGameSprite* pSprite, const CResRef& res)
{
    if (g_pBaldurChitin->cDimm.m_cKeyTable.FindKey(res, 1005, FALSE)) {
        if (res.GetResRefStr().GetLength() != 0) {
            CItem cItem(res, 1, 0, 0, 0, 0);
            pSprite->FeedBack(CGameSprite::FEEDBACK_EFFECTSEVADED, 0, 0, 0, cItem.GetIdentifiedName(), 0, 0);
        }
    } else if (g_pBaldurChitin->cDimm.m_cKeyTable.FindKey(res, 1006, FALSE)) {
        if (res.GetResRefStr().GetLength() != 0) {
            // NOTE: Inlined CSpell::CSpell.
            CSpell cSpell(res);
            pSprite->FeedBack(CGameSprite::FEEDBACK_EFFECTSEVADED, 0, 0, 0, cSpell.GetGenericName(), 0, 0);
        }
    } else {
        pSprite->FeedBack(CGameSprite::FEEDBACK_EFFECTSEVADED, 0, 0, 0, -1, 0, 0);
    }
}

// 0x4A9E30
BOOL CGameEffectDamage::CheckSave(CGameSprite* pSprite, BYTE* pFortitudeRoll, BYTE* pReflexRoll, BYTE* pWillRoll, BYTE* pResistMagicRoll)
{
    POSITION pos;
    CMessage* message;

    if (pSprite->GetAIType().m_nRace == CAIOBJECTTYPE_R_190) {
        return TRUE;
    }

    // A mirror image takes the hit instead.
    if (static_cast<BYTE>(pSprite->m_derivedStats.m_nMirrorImages) != 0) {
        INT nImages = static_cast<BYTE>(pSprite->m_derivedStats.m_nMirrorImages) - 1;

        pos = pSprite->m_timedEffectList.GetHeadPosition();
        while (pos != NULL) {
            CGameEffect* pEffect = pSprite->m_timedEffectList.GetNext(pos);
            if (pEffect->m_effectID == CGAMEEFFECT_MIRRORIMAGE) {
                pEffect->m_effectAmount = nImages;
            }
        }

        pos = pSprite->m_equipedEffectList.GetHeadPosition();
        while (pos != NULL) {
            CGameEffect* pEffect = pSprite->m_equipedEffectList.GetNext(pos);
            if (pEffect->m_effectID == CGAMEEFFECT_MIRRORIMAGE) {
                pEffect->m_effectAmount = nImages;
            }
        }

        pSprite->m_derivedStats.m_nMirrorImages = nImages;

        // NOTE: The visual effect type is the BYTE at 0x84CFE4 (3).
        message = new CMessageVisualEffect(3,
            static_cast<BYTE>(nImages),
            pSprite->GetId(),
            pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

        return FALSE;
    }

    // An iron skin takes one off the count; the damage still goes on.
    if (pSprite->m_derivedStats.m_nStoneSkins > 0) {
        LONG nSkins = pSprite->m_derivedStats.m_nStoneSkins - 1;

        pos = pSprite->m_timedEffectList.GetHeadPosition();
        while (pos != NULL) {
            CGameEffect* pEffect = pSprite->m_timedEffectList.GetNext(pos);
            if (pEffect->m_effectID == CGAMEEFFECT_STONESKINS) {
                pEffect->m_effectAmount = nSkins;
            }
        }

        pos = pSprite->m_equipedEffectList.GetHeadPosition();
        while (pos != NULL) {
            CGameEffect* pEffect = pSprite->m_equipedEffectList.GetNext(pos);
            if (pEffect->m_effectID == CGAMEEFFECT_STONESKINS) {
                pEffect->m_effectAmount = nSkins;
            }
        }

        pos = pSprite->m_timedEffectList.GetHeadPosition();
        while (pos != NULL) {
            CGameEffect* pEffect = pSprite->m_timedEffectList.GetNext(pos);
            if (pEffect->m_effectID == ICEWIND_CGAMEEFFECT_AEGIS) {
                pEffect->m_effectAmount = nSkins;
            }
        }

        pos = pSprite->m_equipedEffectList.GetHeadPosition();
        while (pos != NULL) {
            CGameEffect* pEffect = pSprite->m_equipedEffectList.GetNext(pos);
            if (pEffect->m_effectID == ICEWIND_CGAMEEFFECT_AEGIS) {
                pEffect->m_effectAmount = nSkins;
            }
        }

        if (nSkins == 0) {
            pSprite->m_equipedEffectList.RemoveAllOfType(pSprite,
                CGAMEEFFECT_STONESKINS,
                pSprite->m_equipedEffectList.m_posCurrent,
                -1);
            pSprite->m_timedEffectList.RemoveAllOfType(pSprite,
                CGAMEEFFECT_STONESKINS,
                pSprite->m_timedEffectList.m_posCurrent,
                -1);
        }

        pSprite->m_derivedStats.m_nStoneSkins = nSkins;

        // NOTE: The visual effect type is the BYTE at 0x84CFF8 (23).
        message = new CMessageVisualEffect(23,
            static_cast<BYTE>(nSkins),
            pSprite->GetId(),
            pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    BOOL bResult = CGameEffect::CheckSave(pSprite, pFortitudeRoll, pReflexRoll, pWillRoll, pResistMagicRoll);

    // Mode 3: half damage on a save.
    if (static_cast<WORD>(m_dwFlags) != 3) {
        return bResult;
    }

    if (!bResult) {
        if ((m_savingThrow & 0x8) == 0) {
            // The save halves the damage.
            return TRUE;
        }

        // Evasion: a made reflex save avoids the damage.
        if (IcewindMisc::sub_585DA0(pSprite)) {
            FeedBackEffectsEvaded(pSprite, m_sourceRes);
            return FALSE;
        }
    }

    // Improved evasion: half damage even on a failed reflex save, in no
    // armour or in light armour.
    if ((m_savingThrow & 0x8) != 0
        && pSprite->sub_763150(CGAMESPRITE_FEAT_IMPROVED_EVASION) == TRUE
        && (pSprite->m_equipment.m_items[CGameSpriteEquipment::SLOT_ARMOR] == NULL
            || pSprite->sub_724690(0)
            || pSprite->sub_724690(1))) {
        FeedBackEffectsEvaded(pSprite, m_sourceRes);
        return TRUE;
    }

    if (!bResult) {
        // The save halves the damage.
        return TRUE;
    }

    // Not saved: full damage.
    m_dwFlags &= ~0x3;
    return bResult;
}

// 0x4A51D0
void CGameEffectDamage::DisplayString(CGameSprite* pSprite)
{
    // NOTE: The vtable's slot +0x20 (0x84AE5C) is the shared empty `ret 4`
    // (identical code folding with CGameEffect::OnRemove), not the base's
    // effect text line (0x4A4BB0): a damage effect's only line is its
    // feedback (0x4AA6D0, slot +0x28).
}

// 0x4AA6D0
void CGameEffectDamage::sub_4AA6D0(CGameSprite* pSprite, INT nResisted)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameObject* pSource;
    CString sValue;

    // The plain line: the string as fetched, or the name and the amount.
    BOOL bTokens = FALSE;
    BOOL bNoDamage = FALSE;
    STRREF strText;

    CString sName(pSprite->GetName());

    if ((pGame->GetOptions()->m_nEffectTextLevel & 0x2) == 0) {
        if (m_effectAmount > 0) {
            return;
        }
    }

    if (m_effectAmount <= 0) {
        // No damage: "<DAMAGER> fails to damage <DAMAGEE>" under the
        // source's name.
        bTokens = FALSE;
        bNoDamage = TRUE;
        strText = 25038;

        if (m_sourceID == CGameObjectArray::INVALID_INDEX || m_sourceID == pSprite->GetId()) {
            return;
        }

        g_pBaldurChitin->GetTlkTable().SetToken(CString("DAMAGEE"), pSprite->GetName());
        g_pBaldurChitin->GetTlkTable().SetToken(CString("TYPE"), sub_4AB370());

        // NOTE: One try, no retry loop (0x4AA86E).
        if (pGame->GetObjectArray()->GetShare(m_sourceID,
                CGameObjectArray::THREAD_ASYNCH,
                &pSource,
                INFINITE)
            != CGameObjectArray::SUCCESS) {
            return;
        }

        if (pSource->GetObjectType() != CGameObject::TYPE_SPRITE) {
            pGame->GetObjectArray()->ReleaseShare(m_sourceID,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            return;
        }

        if (strcmp(static_cast<CGameSprite*>(pSource)->GetName(), "") == 0) {
            pGame->GetObjectArray()->ReleaseShare(m_sourceID,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            return;
        }

        sName = static_cast<CGameSprite*>(pSource)->GetName();
        pGame->GetObjectArray()->ReleaseShare(m_sourceID,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    } else {
        // The effect's own line from EFFTEXT.2DA.
        CString sEffectID;
        sEffectID.Format("%d", m_effectID);
        CString sStrRef(pGame->GetRuleTables().m_tEffectText.GetAt(CRuleTables::STRREF_STR, sEffectID));
        STRREF strEffect = atol(sStrRef);
        strText = strEffect;
        if (strEffect == -1) {
            return;
        }

        if (m_sourceID == pSprite->GetId()) {
            // Self-inflicted.
            bTokens = TRUE;
            if (nResisted < 0) {
                strText = 26222;
            } else {
                strText = nResisted != 0 ? 25040 : 25039;
            }

            sName = pSprite->GetName();

            sValue.Format("%d", m_effectAmount);
            g_pBaldurChitin->GetTlkTable().SetToken(CString("AMOUNT"), sValue);

            sValue.Format("%d", nResisted);
            g_pBaldurChitin->GetTlkTable().SetToken(CString("RESISTED"), sValue);

            g_pBaldurChitin->GetTlkTable().SetToken(CString("TYPE"), sub_4AB370());
        }

        if (m_sourceID != CGameObjectArray::INVALID_INDEX && m_sourceID != pSprite->GetId()) {
            bTokens = TRUE;
            if (nResisted < 0) {
                strText = 26223;
            } else {
                strText = nResisted != 0 ? 25017 : 25028;
            }

            sName = pSprite->GetName();

            sValue.Format("%d", m_effectAmount);
            g_pBaldurChitin->GetTlkTable().SetToken(CString("AMOUNT"), sValue);

            sValue.Format("%d", nResisted < 0 ? -nResisted : nResisted);
            g_pBaldurChitin->GetTlkTable().SetToken(CString("RESISTED"), sValue);

            g_pBaldurChitin->GetTlkTable().SetToken(CString("TYPE"), sub_4AB370());

            // NOTE: One try, no retry loop (0x4AAD3C).
            if (pGame->GetObjectArray()->GetShare(m_sourceID,
                    CGameObjectArray::THREAD_ASYNCH,
                    &pSource,
                    INFINITE)
                == CGameObjectArray::SUCCESS) {
                if (pSource->GetObjectType() == CGameObject::TYPE_SPRITE) {
                    // An unnamed damager falls back to the effect's line.
                    if (strcmp(static_cast<CGameSprite*>(pSource)->GetName(), "") == 0) {
                        bTokens = FALSE;
                        strText = strEffect;
                    }

                    g_pBaldurChitin->GetTlkTable().SetToken(CString("DAMAGER"), static_cast<CGameSprite*>(pSource)->GetName());
                } else {
                    bTokens = FALSE;
                    strText = strEffect;
                }

                pGame->GetObjectArray()->ReleaseShare(m_sourceID,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            } else {
                bTokens = FALSE;
                strText = strEffect;
            }
        }
    }

    STR_RES strRes;
    g_pBaldurChitin->GetTlkTable().Fetch(strText, strRes);

    COLORREF rgbNameColor = CVidPalette::RANGE_COLORS[pSprite->GetBaseStats()->m_colors[2]];

    // The string's sound, at the sprite.
    if (strRes.cSound.pRes != NULL) {
        CMessage* message = new CMessagePlaySoundRef(strRes.cSound.cResRef, pSprite->GetId(), pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

        if (strRes.cSound.m_nLooping == 0) {
            strRes.cSound.SetFireForget(TRUE);
        }

        strRes.cSound.SetChannel(14, reinterpret_cast<DWORD_PTR>(pSprite->GetArea()));
        strRes.cSound.Play(pSprite->GetPos().x, pSprite->GetPos().y, 0, FALSE);
    }

    if (strcmp(strRes.szText, "") != 0) {
        CString sText;
        if (bNoDamage) {
            sText = strRes.szText;
        } else if (!bTokens) {
            sName = pSprite->GetName();
            sText.Format("%s (%d)", static_cast<const char*>(strRes.szText), m_effectAmount);
        } else {
            sText = strRes.szText;
        }

        g_pBaldurChitin->GetBaldurMessage()->DisplayText(sName,
            sText,
            rgbNameColor,
            RGB(0xD7, 0xD7, 0xBE),
            -1,
            pSprite->GetId(),
            pSprite->GetId());
    }
}

// 0x4AB370
CString CGameEffectDamage::sub_4AB370()
{
    // The damage type's name.
    STRREF strType;
    switch (m_dwFlags & 0xFFFF0000) {
    case 0x0:
    case 0x4000000:
    case 0x8000000:
        strType = 25023;
        break;
    case 0x10000:
        strType = 25027;
        break;
    case 0x20000:
        strType = 25026;
        break;
    case 0x40000:
        strType = 25025;
        break;
    case 0x80000:
        strType = 25024;
        break;
    case 0x100000:
    case 0x2000000:
        strType = 25022;
        break;
    case 0x200000:
        strType = 25018;
        break;
    case 0x400000:
    case 0x10000000:
        strType = 25019;
        break;
    case 0x1000000:
        strType = 25021;
        break;
    case 0x20000000:
        strType = 10912;
        break;
    case 0x40000000:
        strType = 10911;
        break;
    default:
        // NOTE: For any other type the original fetches the address of the
        // string it returns as the strref (0x4AB44F); ours fetches -1, which
        // gives an empty name as that huge strref does.
        strType = -1;
        break;
    }

    STR_RES strRes;
    g_pBaldurChitin->GetTlkTable().Fetch(strType, strRes);
    return strRes.szText;
}

// 0x4AB0A0
void CGameEffectDamage::sub_4AB0A0(CGameSprite* pSprite, INT nAmount)
{
    CString sName(pSprite->GetName());

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if ((pGame->GetOptions()->m_nEffectTextLevel & 0x2) == 0) {
        return;
    }

    STRREF strRef = 17075;
    if (m_sourceID != -1 && m_sourceID != pSprite->GetId()) {
        CGameObject* pSource;
        BYTE rc = pGame->GetObjectArray()->GetShare(m_sourceID,
            CGameObjectArray::THREAD_ASYNCH,
            &pSource,
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            BOOL bNamed = FALSE;
            if (pSource->GetObjectType() == CGameObject::TYPE_SPRITE
                && strcmp(static_cast<CGameSprite*>(pSource)->GetName(), "") != 0) {
                bNamed = TRUE;
                g_pBaldurChitin->GetTlkTable().SetToken(CString("DAMAGER"),
                    static_cast<CGameSprite*>(pSource)->GetName());
            }

            pGame->GetObjectArray()->ReleaseShare(m_sourceID,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);

            if (bNamed) {
                strRef = 17067;
            }
        }
    }

    CString sAmount;
    sAmount.Format("%d", nAmount);
    g_pBaldurChitin->GetTlkTable().SetToken(CString("AMOUNT"), sAmount);

    STR_RES strRes;
    g_pBaldurChitin->GetTlkTable().Fetch(strRef, strRes);
    sAmount = strRes.szText;

    g_pBaldurChitin->GetBaldurMessage()->DisplayText(sName,
        sAmount,
        CVidPalette::RANGE_COLORS[pSprite->GetBaseStats()->m_colors[2]],
        RGB(0xD7, 0xD7, 0xBE),
        -1,
        pSprite->GetId(),
        pSprite->GetId());
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectDeath::CGameEffectDeath()
{
    m_effectID = CGAMEEFFECT_DEATH;

    // NOTE: Both inlined copies write 0 (0x4A7ADB in
    // CGameEffectDamage::ApplyEffect, 0x56AA56).
    field_18C = 0;
}

// NOTE: Inlined.
CGameEffectDeath::CGameEffectDeath(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
    field_18C = 0;
}

// 0x494150
CGameEffectDeath::CGameEffectDeath(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target, int a5)
{
    field_18C = a5;
}

// 0x452E70
CGameEffect* CGameEffectDeath::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectDeath* copy = new CGameEffectDeath(effect, m_source, m_sourceID, m_target);
    copy->field_18C = field_18C;
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4AD3C0
void CGameEffectDeath::OnAddSpecific(CGameSprite* pSprite)
{
    CMessage* message;

    if (m_sourceID != CGameObjectArray::INVALID_INDEX) {
        // The killer learns of the kill (and its experience value) unless
        // the creature is dead already.
        if ((pSprite->m_baseStats.m_generalState & (STATE_DEAD | STATE_STONE_DEATH | STATE_FROZEN_DEATH)) == 0) {
            CAITrigger killed(CAITRIGGER_KILLED, pSprite->GetAIType(), 0);
            killed.m_specificID = pSprite->m_baseStats.m_xpValue;
            message = new CMessageSetTrigger(killed, pSprite->GetId(), m_sourceID);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }

        CAITrigger die(CAITRIGGER_DIE, 0);
        message = new CMessageSetTrigger(die, m_sourceID, pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    } else {
        CAITrigger die(CAITRIGGER_DIE, 0);
        message = new CMessageSetTrigger(die, pSprite->GetId(), pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    // Everyone within help range hears of the death.
    CTypedPtrList<CPtrList, LONG*> targets;
    CGameArea* pArea = pSprite->GetArea();
    if (pArea != NULL) {
        pArea->GetAllInRange(pSprite->GetPos(),
            CAIObjectType::ANYONE,
            pSprite->GetHelpRange(),
            pSprite->GetTerrainTable(),
            targets,
            FALSE,
            FALSE);
    }

    CAITrigger died(CAITRIGGER_DIED, pSprite->GetAIType(), 0);

    // The global "_DEAD<script name>" counts the deaths.
    CVariable cVariable;
    CString sName;
    sName = CGameAIBase::DEAD_GLOBAL_PREFIX + reinterpret_cast<const char*>(pSprite->m_scriptName);

    CMessageSetVariable* pSetVariable;
    if (g_pBaldurChitin->GetObjectGame()->GetVariables()->FindKey(sName) != NULL) {
        pSetVariable = new CMessageSetVariable(CString("GLOBAL"), sName, 1, pSprite->GetId(), pSprite->GetId());
        pSetVariable->m_bIncrement = TRUE;
    } else {
        cVariable.SetName(sName);
        cVariable.m_intValue = 1;
        g_pBaldurChitin->GetObjectGame()->m_variables.AddKey(cVariable);

        pSetVariable = new CMessageSetVariable(CString("GLOBAL"), sName, 1, pSprite->GetId(), pSprite->GetId());
    }
    g_pBaldurChitin->GetMessageHandler()->AddMessage(pSetVariable, FALSE);

    POSITION pos = targets.GetHeadPosition();
    while (pos != NULL) {
        LONG nId = static_cast<LONG>(reinterpret_cast<INT_PTR>(targets.GetNext(pos)));

        message = new CMessageSetTrigger(died, pSprite->GetId(), nId);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    // A party member's death reaches every party member (trigger 0x88,
    // which TRIGGER.IDS does not name).
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (pGame->GetCharacterPortraitNum(pSprite->GetId()) != -1) {
        for (SHORT nIndex = 0; nIndex < pGame->m_nCharacters; nIndex++) {
            CAITrigger trigger(0x88, pSprite->GetAIType(), 0);

            // NOTE: An inlined accessor of m_characterPortraits with a bounds
            // check (0x4ADAEA).
            LONG nCharacterId = nIndex < pGame->m_nCharacters
                ? pGame->m_characterPortraits[nIndex]
                : CGameObjectArray::INVALID_INDEX;

            message = new CMessageSetTrigger(trigger, pSprite->GetId(), nCharacterId);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
    }
}

// 0x4ADC90
void CGameEffectDeath::DisplayString(CGameSprite* pSprite)
{
    if ((pSprite->GetBaseStats()->m_generalState & STATE_DEAD) == 0
        && (pSprite->GetBaseStats()->m_generalState & (STATE_STONE_DEATH | STATE_FROZEN_DEATH)) == 0
        && m_effectAmount != 1) {
        CGameEffect::DisplayString(pSprite);
    }
}

// 0x4AB510
BOOL CGameEffectDeath::ApplyEffect(CGameSprite* pSprite)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CMessage* message;
    CGameObject* pObject;
    BYTE rc;

    BOOLEAN bDestroy = FALSE;

    // NOTE: Inlined CGameAnimation::IsInvulnerable (ObjAnimation.h line
    // 2100 asserts m_animation).
    if (pSprite->GetAnimation()->m_animation->IsInvulnerable()) {
        m_done = TRUE;
        return TRUE;
    }

    if (field_18C == 0 && pGame->m_gameSave.m_cutScene == TRUE) {
        m_done = TRUE;
        return TRUE;
    }

    pSprite->sub_6F5330();

    if ((pSprite->m_derivedStats.m_generalState & STATE_DEAD) != 0) {
        if (pSprite->m_nSequence == CGameSprite::SEQ_SLEEP) {
            pSprite->SetSequence(CGameSprite::SEQ_TWITCH);
            m_done = TRUE;
            return TRUE;
        }

        if (pSprite->m_nSequence != CGameSprite::SEQ_DIE
            && pSprite->m_nSequence != CGameSprite::SEQ_TWITCH) {
            pSprite->SetSequence(CGameSprite::SEQ_DIE);
        }

        m_done = TRUE;
        return TRUE;
    }

    pSprite->AutoPause(16);

    if (pGame->GetCharacterPortraitNum(pSprite->GetId()) != -1) {
        g_pBaldurChitin->m_pEngineWorld->StartDeathReaction(CString(reinterpret_cast<const char*>(pSprite->m_scriptName)));
    }

    if ((pSprite->m_baseStats.m_flags & 0x2000) != 0) {
        pGame->ReadyCharacterTerminationSequence(1, -1);
    } else {
        // The game ends when no other party member is alive.
        BOOLEAN bPartyAlive = FALSE;
        for (BYTE nIndex = 0; nIndex < 6; nIndex++) {
            // NOTE: An inlined accessor of m_characters with a bounds check
            // (0x4AB6C6).
            LONG nCharacterId = nIndex < 6
                ? pGame->m_characters[nIndex]
                : CGameObjectArray::INVALID_INDEX;
            if (nCharacterId != pSprite->GetId() && nCharacterId != CGameObjectArray::INVALID_INDEX) {
                do {
                    rc = pGame->GetObjectArray()->GetShare(nCharacterId,
                        CGameObjectArray::THREAD_ASYNCH,
                        &pObject,
                        INFINITE);
                } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

                if (rc == CGameObjectArray::SUCCESS) {
                    if (static_cast<CGameSprite*>(pObject)->m_baseStats.m_hitPoints > 0) {
                        bPartyAlive = TRUE;
                    }

                    pGame->GetObjectArray()->ReleaseShare(nCharacterId,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);
                }
            }
        }

        if (!bPartyAlive) {
            pGame->ReadyCharacterTerminationSequence(2, -1);
        }
    }

    pSprite->m_hasColorEffects = TRUE;

    // The death cry; with gore off the original plays sound 6 instead of
    // SOUND_DYING (the BYTE at 0x85BB69).
    if (pGame->GetOptions()->m_nGore != 0) {
        if (!pSprite->field_70FE) {
            message = new CMessagePlaySound(CGameSprite::SOUND_DYING, TRUE, FALSE, pSprite->GetId(), pSprite->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
    } else {
        if (!pSprite->field_70FE) {
            message = new CMessagePlaySound(6, TRUE, FALSE, pSprite->GetId(), pSprite->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
    }

    pSprite->m_animationRunning = TRUE;

    pSprite->m_timedEffectList.RemoveAllOfType(pSprite, CGAMEEFFECT_HOLDCREATURE, pSprite->m_timedEffectList.m_posCurrent, -1);
    pSprite->m_equipedEffectList.RemoveAllOfType(pSprite, CGAMEEFFECT_HOLDCREATURE, pSprite->m_equipedEffectList.m_posCurrent, -1);
    pSprite->m_equipedEffectList.RemoveAllOfType(pSprite, CGAMEEFFECT_PORTRAITICON, pSprite->m_equipedEffectList.m_posCurrent, 13);
    pSprite->m_timedEffectList.RemoveAllOfType(pSprite, CGAMEEFFECT_PORTRAITICON, pSprite->m_timedEffectList.m_posCurrent, 13);

    SHORT nDirection = pSprite->GetDirection(m_source);
    LONG nPosZ = pSprite->m_posZ;
    BOOL bStatue = FALSE;

    if (m_dwFlags == 0x10) {
        // A stone death: the original queues another death, flags 4 (0x40
        // with gore), delayed (duration type 4) by 5.
        ITEM_EFFECT* effect = new ITEM_EFFECT;
        memset(effect, 0, sizeof(ITEM_EFFECT));
        effect->effectID = CGAMEEFFECT_DEATH;
        effect->probabilityUpper = 100;
        effect->dwFlags = (pGame->GetOptions()->m_nGore != 0 ? 0x3C : 0) + 4;
        effect->durationType = 4;
        effect->duration = 5;

        CGameEffect* pEffect = CGameEffect::DecodeEffect(effect, pSprite->GetPos(), pSprite->GetId(), CPoint(-1, -1));
        message = new CMessageAddEffect(pEffect->Copy(), pSprite->GetId(), pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        if (pEffect != NULL) {
            delete pEffect;
        }
        delete effect;
    }

    if (m_dwFlags == 0x20) {
        // A frozen death: likewise with flags 0x80.
        ITEM_EFFECT* effect = new ITEM_EFFECT;
        memset(effect, 0, sizeof(ITEM_EFFECT));
        effect->effectID = CGAMEEFFECT_DEATH;
        effect->probabilityUpper = 100;
        effect->dwFlags = 0x80;
        effect->durationType = 4;
        effect->duration = 5;

        CGameEffect* pEffect = CGameEffect::DecodeEffect(effect, pSprite->GetPos(), pSprite->GetId(), CPoint(-1, -1));
        message = new CMessageAddEffect(pEffect->Copy(), pSprite->GetId(), pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        if (pEffect != NULL) {
            delete pEffect;
        }
        delete effect;
    }

    // NOTE: Inlined CGameObject::InControl.
    if (pSprite->InControl() && g_pChitin->cNetwork.m_bConnectionEstablished == TRUE) {
        message = new CMessageSpriteDeath(m_dwFlags, pSprite->GetId(), pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    if (pSprite->InControl()) {
        CAITrigger trigger(CAITRIGGER_DIE, pSprite->GetAIType(), 0);
        message = new CMessageSetTrigger(trigger, pSprite->GetId(), pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    BYTE nShatterType = 0xFF;

    pSprite->m_timedEffectList.RemoveAllEffectsIgnoreMoreThenPermanent(pSprite->m_timedEffectList.m_posCurrent, TRUE, FALSE, 0, 0);

    pSprite->m_derivedStats.m_generalState &= ~0x4000;
    pSprite->m_baseStats.m_generalState &= ~0x4000;
    pSprite->m_derivedStats.m_generalState &= ~0x80000;
    pSprite->m_baseStats.m_generalState &= ~0x80000;

    pSprite->m_timedEffectList.RemoveAllOfType(pSprite, CGAMEEFFECT_DISEASE, pSprite->m_timedEffectList.m_posCurrent, -1);
    pSprite->m_equipedEffectList.RemoveAllOfType(pSprite, CGAMEEFFECT_DISEASE, pSprite->m_equipedEffectList.m_posCurrent, -1);
    pSprite->m_timedEffectList.RemoveAllOfType(pSprite, CGAMEEFFECT_POISON, pSprite->m_timedEffectList.m_posCurrent, -1);
    pSprite->m_equipedEffectList.RemoveAllOfType(pSprite, CGAMEEFFECT_POISON, pSprite->m_equipedEffectList.m_posCurrent, -1);
    pSprite->m_equipedEffectList.RemoveAllOfType(pSprite, CGAMEEFFECT_PORTRAITICON, pSprite->m_equipedEffectList.m_posCurrent, 6);
    pSprite->m_timedEffectList.RemoveAllOfType(pSprite, CGAMEEFFECT_PORTRAITICON, pSprite->m_timedEffectList.m_posCurrent, 6);
    pSprite->m_equipedEffectList.RemoveAllOfType(pSprite, CGAMEEFFECT_PORTRAITICON, pSprite->m_equipedEffectList.m_posCurrent, 7);
    pSprite->m_timedEffectList.RemoveAllOfType(pSprite, CGAMEEFFECT_PORTRAITICON, pSprite->m_timedEffectList.m_posCurrent, 7);
    pSprite->m_timedEffectList.RemoveAllOfType(pSprite, CGAMEEFFECT_GREASEPOOL, pSprite->m_timedEffectList.m_posCurrent, -1);
    pSprite->m_timedEffectList.RemoveAllOfType(pSprite, CGAMEEFFECT_ENTANGLE, pSprite->m_timedEffectList.m_posCurrent, -1);
    pSprite->m_timedEffectList.RemoveAllOfType(pSprite, CGAMEEFFECT_WEBHOLD, pSprite->m_timedEffectList.m_posCurrent, -1);

    // NOTE: The original passes the timed list's current position to the
    // equipped list's calls below (0x4ABE10..0x4ABE3C).
    pSprite->m_equipedEffectList.RemoveAllOfType(pSprite, CGAMEEFFECT_GREASEPOOL, pSprite->m_timedEffectList.m_posCurrent, -1);
    pSprite->m_equipedEffectList.RemoveAllOfType(pSprite, CGAMEEFFECT_ENTANGLE, pSprite->m_timedEffectList.m_posCurrent, -1);
    pSprite->m_equipedEffectList.RemoveAllOfType(pSprite, CGAMEEFFECT_WEBHOLD, pSprite->m_timedEffectList.m_posCurrent, -1);

    if (pSprite->Orderable(FALSE)) {
        pSprite->Unselect();
        pGame->UpdatePortrait(pGame->GetCharacterPortraitNum(pSprite->GetId()), 1);
        pGame->SelectToolbar();

        // NOTE: Inlined CGameObject::InControl.
        if (pSprite->InControl()) {
            pSprite->m_bSendSpriteUpdate = TRUE;
        }
    }

    // Without gore every death but stone, frozen and the persistent ones is
    // a plain one.
    if (pGame->GetOptions()->m_nGore == 0) {
        switch (m_dwFlags) {
        case 0x0:
        case 0x1:
        case 0x2:
        case 0x8:
        case 0x40:
        case 0x80:
        case 0x100:
            m_dwFlags = 4;
            break;
        }
    }

    // Some animations have no gore sequences.
    CString sResRef;
    pSprite->GetAnimation()->m_animation->GetAnimationResRef(sResRef, CGameAnimationType::RANGE_BODY);
    if (strcmp(sResRef, "MGEN") == 0
        || strcmp(sResRef, "MKG1") == 0
        || strcmp(sResRef, "MKG2") == 0
        || strcmp(sResRef, "MKG3") == 0) {
        m_dwFlags = 4;
    }

    pSprite->m_derivedStats.m_bDoNotJump = TRUE;

    switch (m_dwFlags) {
    case 0x0:
        pSprite->m_baseStats.m_hitPoints = -10;
        pSprite->m_baseStats.m_generalState |= 0xC00;
        pSprite->m_derivedStats.m_generalState |= 0xC00;
        break;
    case 0x1:
        // Burning.
        pSprite->m_baseStats.m_hitPoints = -10;
        pSprite->m_baseStats.m_generalState |= 0xA00;
        pSprite->m_derivedStats.m_generalState |= 0xA00;
        pSprite->m_persistantEffects.AddTail(new CPersistantEffectFireDeath());
        break;
    case 0x2:
        pSprite->m_baseStats.m_hitPoints = -10;
        pSprite->m_baseStats.m_generalState |= 0x900;
        pSprite->m_derivedStats.m_generalState |= 0x900;
        break;
    case 0x4:
        pSprite->m_baseStats.m_hitPoints = 0;
        pSprite->m_baseStats.m_generalState |= STATE_DEAD;
        pSprite->m_derivedStats.m_generalState |= STATE_DEAD;
        pSprite->AddBlood(0, nDirection, CBlood::GUSH_HIGH);
        pSprite->SetSequence(CGameSprite::SEQ_DIE);
        break;
    case 0x8:
    case 0x400:
        // Chunks.
        bDestroy = TRUE;
        pSprite->m_baseStats.m_hitPoints = -10;
        pSprite->m_baseStats.m_generalState |= 0x900;
        pSprite->m_derivedStats.m_generalState |= 0x900;
        pSprite->SetSequence(CGameSprite::SEQ_DIE);

        // NOTE: Inlined CGameAnimation::GetColorChunks (ObjAnimation.h line
        // 2085).
        if (pSprite->GetAnimation()->m_animation->GetColorChunks() == 0xFF) {
            pSprite->AddBlood(0, nDirection, CBlood::GUSH_HIGH);
        } else {
            PlaySound(CResRef("GORE"), pSprite);
            nShatterType = 2;
        }
        break;
    case 0x10:
        // Stone.
        pSprite->m_baseStats.m_hitPoints = 0;
        pSprite->m_baseStats.m_generalState |= STATE_STONE_DEATH;
        pSprite->m_derivedStats.m_generalState |= STATE_STONE_DEATH;
        pSprite->m_animationRunning = FALSE;

        // NOTE: Inlined CGameAnimation::IsFalseColor (line 2099) and
        // SetColorRange (line 2111).
        if (pSprite->GetAnimation()->m_animation->IsFalseColor()) {
            pSprite->m_hasColorRangeEffects = TRUE;
            for (INT nRange = 0; nRange < 7; nRange++) {
                CColorRange* pColorRange = new CColorRange();
                pColorRange->m_range = static_cast<BYTE>(nRange);
                pColorRange->m_color = CVidPalette::STONE;
                pSprite->m_derivedStats.m_appliedColorRanges.AddTail(pColorRange);

                pSprite->GetAnimation()->m_animation->SetColorRange(static_cast<BYTE>(nRange), CVidPalette::STONE);
            }
        }

        bStatue = TRUE;
        break;
    case 0x20:
        // Frozen: a statue with gore, else a plain death.
        pSprite->m_baseStats.m_hitPoints = 0;
        if (pGame->GetOptions()->m_nGore == 0) {
            pSprite->m_baseStats.m_generalState |= STATE_DEAD;
            pSprite->m_derivedStats.m_generalState |= STATE_DEAD;
            pSprite->SetSequence(CGameSprite::SEQ_DIE);
        } else {
            pSprite->m_baseStats.m_generalState |= STATE_FROZEN_DEATH;
            pSprite->m_derivedStats.m_generalState |= STATE_FROZEN_DEATH;
            pSprite->m_animationRunning = FALSE;
            bStatue = TRUE;
        }

        // NOTE: Inlined CGameAnimation::IsFalseColor and SetColorRange.
        if (pSprite->GetAnimation()->m_animation->IsFalseColor()) {
            pSprite->m_hasColorRangeEffects = TRUE;
            for (INT nRange = 0; nRange < 7; nRange++) {
                CColorRange* pColorRange = new CColorRange();
                pColorRange->m_range = static_cast<BYTE>(nRange);
                pColorRange->m_color = CVidPalette::ICE;
                pSprite->m_derivedStats.m_appliedColorRanges.AddTail(pColorRange);

                pSprite->GetAnimation()->m_animation->SetColorRange(static_cast<BYTE>(nRange), CVidPalette::ICE);
            }
        }
        break;
    case 0x40:
        // Shattered statue.
        pSprite->m_baseStats.m_hitPoints = -10;
        pSprite->m_baseStats.m_generalState |= 0x900;
        pSprite->m_derivedStats.m_generalState |= 0x900;
        pSprite->SetSequence(CGameSprite::SEQ_DIE);
        PlaySound(CResRef("SHATTER"), pSprite);
        nShatterType = 1;

        // Two stone explosions at the sprite, half as high as its fx rect,
        // flying away from the effect's source and from its mirror.
        {
            CGameObject84D624* pExplosion = new CGameObject84D624(CGameObject84D624::COLORTYPE_STONE,
                CGameObject84D624::TYPE_EXPLOSION,
                CGameObject84D624::EXPLOSION_PUFFS);
            // NOTE: Inlined CGameObject84D624::SetSource.
            pExplosion->SetSource(m_source);
            CRect rFx;
            CPoint ptReference;
            pSprite->GetAnimation()->CalculateFxRect(rFx, ptReference, nPosZ);
            SHORT nHeight = static_cast<SHORT>((rFx.bottom - rFx.top) * 5 / 10);
            pExplosion->m_nGravity = 100;
            pExplosion->AddToArea(pSprite->GetArea(), pSprite->GetPos(), nHeight, CGameObject::LIST_BACK);

            pExplosion = new CGameObject84D624(CGameObject84D624::COLORTYPE_STONE,
                CGameObject84D624::TYPE_EXPLOSION,
                CGameObject84D624::EXPLOSION_PUFFS);
            pExplosion->SetSource(CPoint(-m_source.x, -m_source.y));
            pExplosion->m_nGravity = 100;
            pExplosion->AddToArea(pSprite->GetArea(), pSprite->GetPos(), nHeight, CGameObject::LIST_BACK);
        }
        break;
    case 0x80:
        // Shattered ice.
        pSprite->m_baseStats.m_hitPoints = -10;
        pSprite->m_baseStats.m_generalState |= 0x900;
        pSprite->m_derivedStats.m_generalState |= 0x900;
        PlaySound(CResRef("SHATTER"), pSprite);
        nShatterType = 0;

        // The ice explosions, as for 0x40.
        {
            CGameObject84D624* pExplosion = new CGameObject84D624(CGameObject84D624::COLORTYPE_ICE,
                CGameObject84D624::TYPE_EXPLOSION,
                CGameObject84D624::EXPLOSION_PUFFS);
            // NOTE: Inlined CGameObject84D624::SetSource.
            pExplosion->SetSource(m_source);
            CRect rFx;
            CPoint ptReference;
            pSprite->GetAnimation()->CalculateFxRect(rFx, ptReference, nPosZ);
            SHORT nHeight = static_cast<SHORT>((rFx.bottom - rFx.top) * 5 / 10);
            pExplosion->m_nGravity = 100;
            pExplosion->AddToArea(pSprite->GetArea(), pSprite->GetPos(), nHeight, CGameObject::LIST_BACK);

            pExplosion = new CGameObject84D624(CGameObject84D624::COLORTYPE_ICE,
                CGameObject84D624::TYPE_EXPLOSION,
                CGameObject84D624::EXPLOSION_PUFFS);
            pExplosion->SetSource(CPoint(-m_source.x, -m_source.y));
            pExplosion->m_nGravity = 100;
            pExplosion->AddToArea(pSprite->GetArea(), pSprite->GetPos(), nHeight, CGameObject::LIST_BACK);
        }
        break;
    case 0x100:
        // Electrocuted.
        pSprite->m_baseStats.m_hitPoints = -10;
        pSprite->m_baseStats.m_generalState |= 0xA00;
        pSprite->m_derivedStats.m_generalState |= 0xA00;
        pSprite->m_persistantEffects.AddTail(new CPersistantEffectElectricDeath());
        break;
    case 0x200:
        bDestroy = TRUE;
        // fall through
    case 0x800:
        pSprite->m_baseStats.m_hitPoints = -10;
        pSprite->m_baseStats.m_generalState |= STATE_DEAD;
        pSprite->m_derivedStats.m_generalState |= STATE_DEAD;
        pSprite->SetSequence(CGameSprite::SEQ_DIE);
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 7596
        UTIL_ASSERT_MSG(FALSE, "Weird death type");
        break;
    }

    if (!bStatue) {
        if (pSprite->GetVertListType() == CGameObject::LIST_BACK) {
            pSprite->m_derivedStats.m_generalState |= STATE_SLEEPING;
        }

        pSprite->MoveToBack();

        if (nShatterType != 0xFF) {
            if (pSprite->m_baseStats.m_hitPoints <= -30
                && pGame->GetCharacterPortraitNum(pSprite->GetId()) != -1) {
                pSprite->Shatter(nShatterType);
            } else if (pSprite->m_baseStats.m_hitPoints <= -10) {
                pSprite->Shatter(nShatterType);
            }
        }

        if (bDestroy == TRUE) {
            message = new CMessageAddAction(CAIAction(21, CPoint(-1, -1), 0, -1), pSprite->GetId(), pSprite->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }

        CAIObjectType typeAI;
        typeAI.Set(pSprite->GetAIType());
        typeAI.m_nGeneral = CAIObjectType::G_DEAD;
        pSprite->SetAIType(typeAI, FALSE, FALSE);
        typeAI.Set(pSprite->m_startTypeAI);
        typeAI.m_nGeneral = CAIObjectType::G_DEAD;
        pSprite->m_startTypeAI.Set(typeAI);

        // NOTE: Inlined CGameObject::InControl.
        if (pSprite->InControl()) {
            pSprite->DropInventory(FALSE, CString(""));
        }

        if (pGame->GetCharacterPortraitNum(pSprite->GetId()) == -1) {
            if ((pSprite->m_baseStats.m_flags & 0x2) != 0) {
                pSprite->m_expirationTime = pGame->GetWorldTimer()->m_gameTime;

                pSprite->DropInventory(TRUE, CString(""));
            }
        } else if (pSprite->InControl()) {
            message = new CMessageVisibilityMapMove(FALSE, pSprite->GetId(), pSprite->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
    } else {
        CAIObjectType typeAI;
        typeAI.Set(pSprite->GetAIType());
        typeAI.m_nGeneral = CAIObjectType::G_FROZEN;
        pSprite->SetAIType(typeAI, FALSE, FALSE);
        typeAI.Set(pSprite->m_startTypeAI);
        typeAI.m_nGeneral = CAIObjectType::G_FROZEN;
        pSprite->m_startTypeAI.Set(typeAI);

        if (pGame->GetCharacterPortraitNum(pSprite->GetId()) == -1) {
            if ((pSprite->m_baseStats.m_flags & 0x2) != 0) {
                pSprite->m_expirationTime = pGame->GetWorldTimer()->m_gameTime;
            }
        } else if (pSprite->InControl()) {
            message = new CMessageVisibilityMapMove(FALSE, pSprite->GetId(), pSprite->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
    }

    pSprite->ClearAI(FALSE);
    pSprite->SetCurrAction(CAIAction::NULL_ACTION);
    pSprite->DropPath();
    pSprite->DropSearchRequest();
    pSprite->m_talkingCounter = 0;

    pGame->UpdatePortrait(pGame->GetCharacterPortraitNum(pSprite->GetId()), 1);

    // NOTE: Inlined CGameObject::InControl.
    if (pSprite->InControl()) {
        pSprite->m_bSendSpriteUpdate = TRUE;
    }

    pSprite->field_7110 = 0;

    // NOTE: The BYTE at 0x85BC7E (10).
    pSprite->m_baseStats.m_morale = 10;

    // 0x586DC0
    Icewind586B70::Instance()->sub_586DC0(pSprite);

    message = new CMessage89(pSprite->GetId(), pSprite->GetId(), pSprite->GetId());
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

    Icewind586B70::Instance()->Remove(pSprite);

    message = new CMessage96(pSprite->GetId(), pSprite->GetId(), pSprite->GetId());
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

    m_done = TRUE;

    // NOTE: The original returns 0 here (0x4AD202).
    return FALSE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectDefrost::CGameEffectDefrost(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x494190
CGameEffect* CGameEffectDefrost::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectDefrost* copy = new CGameEffectDefrost(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4ADCC0
BOOL CGameEffectDefrost::ApplyEffect(CGameSprite* pSprite)
{
    if ((pSprite->GetDerivedStats()->m_generalState & STATE_FROZEN_DEATH) != 0) {
        pSprite->GetDerivedStats()->m_generalState &= ~STATE_FROZEN_DEATH;
        pSprite->ResetAITypeLive();
        pSprite->GetBaseStats()->m_hitPoints = 1;
    }

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectDEX::CGameEffectDEX(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x494240
CGameEffect* CGameEffectDEX::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectDEX* copy = new CGameEffectDEX(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4ADD10
BOOL CGameEffectDEX::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_DEXBase = min(max(pSprite->GetBaseStats()->m_DEXBase + static_cast<SHORT>(m_effectAmount), 1), 40);
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            // NOTE: Uninline.
            AdjustDEX(pSprite, m_effectAmount);
            m_done = FALSE;
        }
        break;
    case 1:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_DEXBase = min(max(static_cast<SHORT>(m_effectAmount), 1), 40);
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->GetDerivedStats()->m_nDEX = min(max(static_cast<SHORT>(m_effectAmount), 1), 40);
            m_done = FALSE;
        }
        break;
    case 2:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_DEXBase = min(max(pSprite->GetBaseStats()->m_DEXBase * static_cast<SHORT>(m_effectAmount) / 100, 1), 40);
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->GetDerivedStats()->m_nDEX = min(max(pSprite->GetBaseStats()->m_DEXBase * static_cast<SHORT>(m_effectAmount) / 100, 1), 40);
            m_done = FALSE;
        }
        break;
    case 3:
        // Cat's Grace.
        if (pSprite->GetDerivedStats()->m_nDEX < 20) {
            if (m_effectAmount == 0) {
                m_effectAmount = rand() % 4 + 1;
            }

            pSprite->GetDerivedStats()->m_nDEX = min(pSprite->GetDerivedStats()->m_nDEX + static_cast<SHORT>(m_effectAmount), 20);

            if (m_secondaryType) {
                // NOTE: Uninline.
                DisplayStringRef(pSprite, 3798); // "Cat's Grace"
            }

            // NOTE: Uninline.
            AddPortraitIcon(pSprite, 75);

            pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_CATS_GRACE] = true;
        } else {
            m_done = FALSE;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 8038
        UTIL_ASSERT(FALSE);
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectHaste::CGameEffectHaste(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4942F0
CGameEffect* CGameEffectHaste::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectHaste* copy = new CGameEffectHaste(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4AE000
BOOL CGameEffectHaste::ApplyEffect(CGameSprite* pSprite)
{
    if ((pSprite->GetDerivedStats()->m_generalState & STATE_HASTED) == 0) {
        if (m_secondaryType != 0) {
            // NOTE: Uninline.
            DisplayStringRef(pSprite, 14023); // "Hasted"

            // NOTE: Probably should be outside of condition.
            if ((pSprite->GetDerivedStats()->m_generalState & STATE_SLOWED) != 0) {
                pSprite->GetEquipedEffectList()->RemoveAllOfType(pSprite,
                    CGAMEEFFECT_SLOW,
                    pSprite->GetEquipedEffectList()->GetPosCurrent(),
                    -1);
                pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
                    CGAMEEFFECT_SLOW,
                    pSprite->GetTimedEffectList()->GetPosCurrent(),
                    -1);
            }
        }

        pSprite->GetDerivedStats()->m_generalState |= STATE_HASTED;

        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 38);

        if (pSprite->GetDerivedStats()->m_nNumberOfAttacks < 5) {
            pSprite->GetDerivedStats()->m_nNumberOfAttacks += 1;
        }

        pSprite->m_derivedStats.field_C += 4;

        if (pSprite->GetAnimation()->GetMoveScale() != 0) {
            pSprite->GetAnimation()->SetMoveScale(pSprite->GetAnimation()->GetMoveScaleDefault() * 2);

            if (m_secondaryType != 0) {
                pSprite->DropPath();
            }
        }

        AddColorEffect(pSprite, 64, 64, 64, 15);
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectHeal::CGameEffectHeal()
{
    m_effectID = CGAMEEFFECT_HEAL;
}

// NOTE: Inlined.
CGameEffectHeal::CGameEffectHeal(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, TRUE)
{
}

// 0x4AE200
BOOL CGameEffectHeal::ApplyEffect(CGameSprite* pSprite)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // NOTE: The original computes the direction to the source and discards
    // it (0x4AE213).
    pSprite->GetDirection(m_source);

    DWORD dwFlags = m_dwFlags;
    WORD nMode = static_cast<WORD>(m_dwFlags);

    INT nRolled = 0;
    for (BYTE nDie = 0; nDie < m_numDice; nDie++) {
        nRolled += CUtil::UtilRandInt(m_diceSize, pSprite->m_tempStats.m_nLuck) + 1;
    }
    m_effectAmount += nRolled;

    if (pSprite->m_baseStats.m_hitPoints < 1) {
        if ((dwFlags & 0xFFFF0000) != 0x10000) {
            m_done = TRUE;
            return TRUE;
        }

        // A Raise Dead (effect 32) applied at once.
        ITEM_EFFECT* effect = new ITEM_EFFECT;
        memset(effect, 0, sizeof(ITEM_EFFECT));
        effect->effectID = 32;
        effect->dwFlags = 0x2B;
        effect->probabilityUpper = 100;
        CGameEffect* pRaise = CGameEffect::DecodeEffect(effect, CPoint(-1, -1), -1, CPoint(-1, -1));
        delete effect;
        pRaise->ApplyEffect(pSprite);
        if (pRaise != NULL) {
            delete pRaise;
        }
    }

    INT nAmount;
    CGameObject* pSource;
    BYTE rc;
    switch (nMode) {
    case 0:
        if (m_secondaryType) {
            nAmount = m_effectAmount;
            if (pSprite->m_baseStats.m_hitPoints + nAmount > pSprite->m_derivedStats.m_nMaxHitPoints) {
                nAmount = pSprite->m_derivedStats.m_nMaxHitPoints - pSprite->m_baseStats.m_hitPoints;
            }

            if (nAmount > 0 && pGame->GetCharacterPortraitNum(pSprite->GetId()) != -1) {
                sub_4A4D80(pSprite, nAmount);
            }
        }

        nAmount = pSprite->m_baseStats.m_hitPoints + m_effectAmount;
        if (static_cast<UINT>(nAmount) > 0x7FFF) {
            nAmount = 0x7FFF;
        }
        pSprite->m_baseStats.m_hitPoints = static_cast<SHORT>(nAmount);
        break;
    case 1:
        nAmount = pSprite->m_tempStats.m_nMaxHitPoints;
        if (m_effectAmount < nAmount) {
            nAmount = m_effectAmount;
        }
        if (nAmount < pSprite->m_baseStats.m_hitPoints) {
            nAmount = pSprite->m_baseStats.m_hitPoints;
        }
        nAmount -= pSprite->m_baseStats.m_hitPoints;

        if (m_secondaryType && nAmount > 0 && pGame->GetCharacterPortraitNum(pSprite->GetId()) != -1) {
            sub_4A4D80(pSprite, nAmount);
        }

        pSprite->m_baseStats.m_hitPoints += static_cast<SHORT>(nAmount);
        break;
    case 2:
        nAmount = pSprite->m_tempStats.m_nMaxHitPoints * m_effectAmount / 100;
        if (nAmount < pSprite->m_baseStats.m_hitPoints) {
            nAmount = pSprite->m_baseStats.m_hitPoints;
        }

        if (m_secondaryType
            && nAmount - pSprite->m_baseStats.m_hitPoints > 0
            && pGame->GetCharacterPortraitNum(pSprite->GetId()) != -1) {
            sub_4A4D80(pSprite, nAmount - pSprite->m_baseStats.m_hitPoints);
        }

        pSprite->m_baseStats.m_hitPoints += static_cast<SHORT>(pSprite->m_tempStats.m_nMaxHitPoints * m_effectAmount / 100);
        break;
    case 3:
    case 4:
    case 5:
        if (m_secondaryType) {
            do {
                rc = pGame->GetObjectArray()->GetShare(m_sourceID,
                    CGameObjectArray::THREAD_ASYNCH,
                    &pSource,
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

            nAmount = 1;
            if (rc == CGameObjectArray::SUCCESS) {
                CGameSprite* pSourceSprite = static_cast<CGameSprite*>(pSource);
                const CRuleTables& ruleTables = pGame->GetRuleTables();
                if (nMode == 3) {
                    nAmount = ruleTables.GetLayOnHandsAmount(pSourceSprite->GetAIType(), pSourceSprite->m_derivedStats);
                } else if (nMode == 4) {
                    nAmount = ruleTables.GetWholenessOfBodyAmount(pSourceSprite->GetAIType(), pSourceSprite->m_derivedStats);
                } else {
                    nAmount = ruleTables.GetLathanderRenewalAmount(pSourceSprite->GetAIType(), pSourceSprite->m_derivedStats);
                }
                pGame->GetObjectArray()->ReleaseShare(m_sourceID,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);

                if (pSprite->m_baseStats.m_hitPoints + m_effectAmount > pSprite->m_derivedStats.m_nMaxHitPoints) {
                    nAmount = pSprite->m_derivedStats.m_nMaxHitPoints - pSprite->m_baseStats.m_hitPoints;
                }
            }

            if (nAmount > 0) {
                m_effectAmount = nAmount;
                if (pGame->GetCharacterPortraitNum(pSprite->GetId()) != -1) {
                    sub_4A4D80(pSprite, nAmount);
                }
            }
        }

        pSprite->m_baseStats.m_hitPoints += static_cast<SHORT>(m_effectAmount);
        break;
    }

    if (pSprite->m_baseStats.m_hitPoints > pSprite->m_tempStats.m_nMaxHitPoints) {
        pSprite->m_baseStats.m_hitPoints = pSprite->m_tempStats.m_nMaxHitPoints;
    }

    pGame->UpdatePortrait(pGame->GetCharacterPortraitNum(pSprite->GetId()), 1);

    // NOTE: Inlined CGameObject::InControl.
    if (pSprite->InControl()) {
        pSprite->m_bSendSpriteUpdate = TRUE;
    }

    m_done = TRUE;
    return TRUE;
}

// 0x4943A0
CGameEffect* CGameEffectHeal::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectHeal* copy = new CGameEffectHeal(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectHitPoints::CGameEffectHitPoints(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, TRUE)
{
}

// 0x494450
CGameEffect* CGameEffectHitPoints::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectHitPoints* copy = new CGameEffectHitPoints(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4AE7C0
BOOL CGameEffectHitPoints::ApplyEffect(CGameSprite* pSprite)
{
    if (pSprite->m_baseStats.m_hitPoints > 0
        && (pSprite->m_tempStats.m_generalState & 0xF00) == 0
        && (pSprite->m_tempStats.m_generalState & 0xC0) == 0) {
        INT nRolled = 0;
        for (BYTE nDie = 0; nDie < m_numDice; nDie++) {
            nRolled += CUtil::UtilRandInt(m_diceSize, pSprite->m_tempStats.m_nLuck) + 1;
        }
        m_effectAmount += nRolled;

        WORD nMax;
        SHORT nValue;
        SHORT nOld;
        switch (m_dwFlags) {
        case 0:
            if (m_durationType == 1) {
                pSprite->m_baseStats.m_maxHitPointsBase += static_cast<SHORT>(m_effectAmount);
                pSprite->m_derivedStats.m_nMaxHitPoints += static_cast<SHORT>(m_effectAmount);
                pSprite->m_baseStats.m_hitPoints += static_cast<SHORT>(m_effectAmount);
                m_done = TRUE;
                m_forceRepass = TRUE;
                return TRUE;
            }
            pSprite->m_derivedStats.m_nMaxHitPoints += static_cast<SHORT>(m_effectAmount);
            if (m_secondaryType) {
                pSprite->m_baseStats.m_hitPoints += static_cast<SHORT>(m_effectAmount);
            }
            m_done = FALSE;
            return TRUE;
        case 1:
        case 4:
            // NOTE: The original reads the amount as an unsigned byte.
            if (m_durationType == 1) {
                pSprite->m_baseStats.m_maxHitPointsBase = static_cast<BYTE>(m_effectAmount);
                m_forceRepass = TRUE;
                m_done = TRUE;
                return TRUE;
            }
            pSprite->m_derivedStats.m_nMaxHitPoints = static_cast<BYTE>(m_effectAmount);
            m_done = FALSE;
            return TRUE;
        case 2:
            if (m_durationType == 1) {
                nMax = static_cast<WORD>(pSprite->m_baseStats.m_maxHitPointsBase * static_cast<SHORT>(m_effectAmount) / 100);
                pSprite->m_baseStats.m_maxHitPointsBase = nMax;
                if (nMax < 2) {
                    pSprite->m_baseStats.m_maxHitPointsBase = 1;
                }
                nOld = pSprite->m_tempStats.m_nMaxHitPoints;
                nMax = pSprite->m_baseStats.m_maxHitPointsBase;
                if (static_cast<INT>(nMax) - nOld > 0) {
                    pSprite->m_baseStats.m_hitPoints += nMax - nOld;
                }
                m_forceRepass = TRUE;
                m_done = TRUE;
                return TRUE;
            }
            nValue = static_cast<SHORT>(pSprite->m_baseStats.m_maxHitPointsBase * static_cast<SHORT>(m_effectAmount) / 100);
            pSprite->m_derivedStats.m_nMaxHitPoints = nValue;
            if (nValue < 2) {
                pSprite->m_derivedStats.m_nMaxHitPoints = 1;
            }
            nOld = pSprite->m_tempStats.m_nMaxHitPoints;
            nValue = pSprite->m_derivedStats.m_nMaxHitPoints;
            if (nValue - nOld > 0) {
                pSprite->m_baseStats.m_hitPoints += nValue - nOld;
            }
            m_done = FALSE;
            return TRUE;
        case 3:
            if (m_durationType == 1) {
                pSprite->m_baseStats.m_maxHitPointsBase += static_cast<SHORT>(m_effectAmount);
                pSprite->m_derivedStats.m_nMaxHitPoints += static_cast<SHORT>(m_effectAmount);
                m_done = TRUE;
                m_forceRepass = TRUE;
                return TRUE;
            }
            pSprite->m_derivedStats.m_nMaxHitPoints += static_cast<SHORT>(m_effectAmount);
            m_done = FALSE;
            return TRUE;
        case 5:
            if (m_durationType == 1) {
                nMax = static_cast<WORD>(pSprite->m_baseStats.m_maxHitPointsBase * static_cast<SHORT>(m_effectAmount) / 100);
                pSprite->m_baseStats.m_maxHitPointsBase = nMax;
                if (nMax < 2) {
                    pSprite->m_baseStats.m_maxHitPointsBase = 1;
                }
                m_forceRepass = TRUE;
                m_done = TRUE;
                return TRUE;
            }
            nValue = static_cast<SHORT>(pSprite->m_baseStats.m_maxHitPointsBase * static_cast<SHORT>(m_effectAmount) / 100);
            pSprite->m_derivedStats.m_nMaxHitPoints = nValue;
            if (nValue < 2) {
                pSprite->m_derivedStats.m_nMaxHitPoints = 1;
            }
            m_done = FALSE;
            return TRUE;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
            // __LINE__: 8586
            UTIL_ASSERT(FALSE);
            return TRUE;
        }
    }

    m_done = TRUE;
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectINT::CGameEffectINT(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x494500
CGameEffect* CGameEffectINT::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectINT* copy = new CGameEffectINT(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4AEB10
BOOL CGameEffectINT::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_INTBase = min(max(pSprite->GetBaseStats()->m_INTBase + static_cast<SHORT>(m_effectAmount), 1), 40);
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            // NOTE: Uninline.
            AdjustINT(pSprite, m_effectAmount);
            m_done = FALSE;
        }
        break;
    case 1:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_INTBase = min(max(static_cast<SHORT>(m_effectAmount), 1), 40);
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->GetDerivedStats()->m_nINT = min(max(static_cast<SHORT>(m_effectAmount), 1), 40);
            m_done = FALSE;
        }
        break;
    case 2:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_INTBase = min(max(pSprite->GetBaseStats()->m_INTBase * static_cast<SHORT>(m_effectAmount) / 100, 1), 40);
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->GetDerivedStats()->m_nINT = min(max(pSprite->GetBaseStats()->m_INTBase * static_cast<SHORT>(m_effectAmount) / 100, 1), 40);
            m_done = FALSE;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 8713
        UTIL_ASSERT(FALSE);
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectInvisible::CGameEffectInvisible(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4945B0
CGameEffect* CGameEffectInvisible::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectInvisible* copy = new CGameEffectInvisible(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4AED10
BOOL CGameEffectInvisible::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType) {
        if (m_dwFlags == 0) {
            // NOTE: Uninline.
            DisplayStringRef(pSprite, 14020);
        } else if (m_dwFlags == 1) {
            // NOTE: Uninline.
            DisplayStringRef(pSprite, 14783);
        }
    }

    pSprite->m_derivedStats.m_visualEffects.set(IWD_VFX_INVISIBILITY, true);

    switch (m_dwFlags) {
    case 0:
    case 2:
        if (m_durationType != 1) {
            // NOTE: Uninline.
            AddPortraitIcon(pSprite, 110);
            pSprite->m_derivedStats.m_generalState |= STATE_INVISIBLE;
            return TRUE;
        }
        pSprite->m_baseStats.m_generalState |= STATE_INVISIBLE;
        break;
    case 1:
        if (m_durationType != 1) {
            // NOTE: Uninline.
            AddPortraitIcon(pSprite, 144);
            pSprite->m_derivedStats.m_generalState |= STATE_INVISIBLE | STATE_IMPROVEDINVISIBILITY;
            if (!IcewindMisc::IsEnemyNearby(pSprite)) {
                pSprite->m_baseStats.field_2FC &= ~0x1;
            }
            return TRUE;
        }
        pSprite->m_baseStats.m_generalState |= STATE_INVISIBLE | STATE_IMPROVEDINVISIBILITY;
        break;
    default:
        return TRUE;
    }

    m_forceRepass = TRUE;
    m_done = TRUE;
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectLore::CGameEffectLore(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x494660
CGameEffect* CGameEffectLore::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectLore* copy = new CGameEffectLore(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4AEF50
BOOL CGameEffectLore::ApplyEffect(CGameSprite* pSprite)
{
    // NOTE: Opcode 21 works on Knowledge (Arcana), skill 8.
    signed char nValue;
    INT nProduct;

    switch (m_dwFlags) {
    case 0:
        if (m_durationType != 1) {
            pSprite->m_bonusStats.m_nSkills[CGAMESPRITE_SKILL_KNOWLEDGE_ARCANA] += static_cast<signed char>(m_effectAmount);
            m_done = FALSE;
            break;
        }
        nValue = static_cast<signed char>(pSprite->m_baseStats.m_skills[CGAMESPRITE_SKILL_KNOWLEDGE_ARCANA] + static_cast<signed char>(m_effectAmount));
        pSprite->m_baseStats.m_skills[CGAMESPRITE_SKILL_KNOWLEDGE_ARCANA] = nValue;
        if (nValue > 100) {
            pSprite->m_baseStats.m_skills[CGAMESPRITE_SKILL_KNOWLEDGE_ARCANA] = 100;
        }
        if (static_cast<signed char>(pSprite->m_baseStats.m_skills[CGAMESPRITE_SKILL_KNOWLEDGE_ARCANA]) < 0) {
            pSprite->m_baseStats.m_skills[CGAMESPRITE_SKILL_KNOWLEDGE_ARCANA] = 0;
        }
        m_forceRepass = TRUE;
        m_done = TRUE;
        break;
    case 1:
    case 2:
        if (m_dwFlags == 1) {
            nValue = static_cast<signed char>(m_effectAmount);
        } else if (m_durationType != 1) {
            nProduct = static_cast<signed char>(pSprite->m_baseStats.m_skills[CGAMESPRITE_SKILL_KNOWLEDGE_ARCANA]) * static_cast<SHORT>(m_effectAmount);
            nValue = static_cast<signed char>(nProduct / 100);
        } else {
            nProduct = static_cast<SHORT>(m_effectAmount) * static_cast<signed char>(pSprite->m_baseStats.m_skills[CGAMESPRITE_SKILL_KNOWLEDGE_ARCANA]);
            nValue = static_cast<signed char>(nProduct / 100);
        }

        if (m_durationType != 1) {
            pSprite->m_derivedStats.m_nSkills[CGAMESPRITE_SKILL_KNOWLEDGE_ARCANA] = nValue;
            if (nValue > 100) {
                pSprite->m_derivedStats.m_nSkills[CGAMESPRITE_SKILL_KNOWLEDGE_ARCANA] = 100;
            }
            if (pSprite->m_derivedStats.m_nSkills[CGAMESPRITE_SKILL_KNOWLEDGE_ARCANA] < 0) {
                pSprite->m_derivedStats.m_nSkills[CGAMESPRITE_SKILL_KNOWLEDGE_ARCANA] = 0;
            }
            m_done = FALSE;
            break;
        }

        pSprite->m_baseStats.m_skills[CGAMESPRITE_SKILL_KNOWLEDGE_ARCANA] = nValue;
        if (nValue > 100) {
            pSprite->m_baseStats.m_skills[CGAMESPRITE_SKILL_KNOWLEDGE_ARCANA] = 100;
        }
        if (static_cast<signed char>(pSprite->m_baseStats.m_skills[CGAMESPRITE_SKILL_KNOWLEDGE_ARCANA]) < 0) {
            pSprite->m_baseStats.m_skills[CGAMESPRITE_SKILL_KNOWLEDGE_ARCANA] = 0;
        }
        m_forceRepass = TRUE;
        m_done = TRUE;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 8947
        UTIL_ASSERT(FALSE);
        break;
    }

    // NOTE: Inlined CGameObject::InControl.
    if (pSprite->InControl()) {
        pSprite->m_bSendSpriteUpdate = TRUE;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectLuck::CGameEffectLuck(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4947C0
CGameEffect* CGameEffectLuck::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectLuck* copy = new CGameEffectLuck(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4AF210
BOOL CGameEffectLuck::ApplyEffect(CGameSprite* pSprite)
{
    signed char nBase;
    SHORT nValue;
    INT nProduct;

    switch (m_dwFlags) {
    case 0:
        if (m_durationType == 1) {
            nBase = static_cast<signed char>(pSprite->m_baseStats.m_luckBase + static_cast<signed char>(m_effectAmount));
            pSprite->m_baseStats.m_luckBase = nBase;
            if (nBase > 20) {
                pSprite->m_baseStats.m_luckBase = 20;
            }
            if (pSprite->m_baseStats.m_luckBase < -20) {
                pSprite->m_baseStats.m_luckBase = -20;
            }
            m_forceRepass = TRUE;
            m_done = TRUE;
            return TRUE;
        }
        pSprite->m_bonusStats.m_nLuck += static_cast<SHORT>(m_effectAmount);
        break;
    case 1:
        if (m_durationType == 1) {
            nBase = static_cast<signed char>(m_effectAmount);
            pSprite->m_baseStats.m_luckBase = nBase;
            if (nBase > 20) {
                pSprite->m_baseStats.m_luckBase = 20;
            }
            if (pSprite->m_baseStats.m_luckBase < -20) {
                pSprite->m_baseStats.m_luckBase = -20;
            }
            m_forceRepass = TRUE;
            m_done = TRUE;
            return TRUE;
        }
        // NOTE: The original reads the amount as an unsigned byte, so a
        // negative amount sets a large luck that the clamp brings to 20.
        pSprite->m_derivedStats.m_nLuck = static_cast<BYTE>(m_effectAmount);
        if (static_cast<BYTE>(m_effectAmount) > 20) {
            pSprite->m_derivedStats.m_nLuck = 20;
        }
        if (pSprite->m_derivedStats.m_nLuck < -20) {
            pSprite->m_derivedStats.m_nLuck = -20;
        }
        break;
    case 2:
        if (m_durationType == 1) {
            nProduct = static_cast<SHORT>(m_effectAmount) * pSprite->m_baseStats.m_luckBase;
            nBase = static_cast<signed char>(nProduct / 100);
            pSprite->m_baseStats.m_luckBase = nBase;
            if (nBase > 20) {
                pSprite->m_baseStats.m_luckBase = 20;
            }
            if (pSprite->m_baseStats.m_luckBase < -20) {
                pSprite->m_baseStats.m_luckBase = -20;
            }
            m_forceRepass = TRUE;
            m_done = TRUE;
            return TRUE;
        }
        nProduct = pSprite->m_baseStats.m_luckBase * static_cast<SHORT>(m_effectAmount);
        nValue = static_cast<SHORT>(nProduct / 100);
        pSprite->m_derivedStats.m_nLuck = nValue;
        if (nValue > 20) {
            pSprite->m_derivedStats.m_nLuck = 20;
        }
        if (pSprite->m_derivedStats.m_nLuck < -20) {
            pSprite->m_derivedStats.m_nLuck = -20;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 9178
        UTIL_ASSERT(FALSE);
        return TRUE;
    }

    m_done = FALSE;
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectMorale::CGameEffectMorale(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x494870
CGameEffect* CGameEffectMorale::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectMorale* copy = new CGameEffectMorale(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4AF420
BOOL CGameEffectMorale::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        pSprite->GetBaseStats()->m_morale += static_cast<BYTE>(m_effectAmount);
        break;
    case 1:
        pSprite->GetBaseStats()->m_morale = static_cast<BYTE>(m_effectAmount);
        break;
    case 2:
        pSprite->GetBaseStats()->m_morale = pSprite->GetBaseStats()->m_morale * static_cast<BYTE>(m_effectAmount) / 100;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 9248
        UTIL_ASSERT(FALSE);
    }

    if (pSprite->GetBaseStats()->m_morale > 20) {
        pSprite->GetBaseStats()->m_morale = 20;
    }

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectPanic::CGameEffectPanic(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x494960
CGameEffect* CGameEffectPanic::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectPanic* copy = new CGameEffectPanic(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4AF500
BOOL CGameEffectPanic::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType) {
        // NOTE: Uninline.
        DisplayStringRef(pSprite, 14007);
    }

    // NOTE: Uninline.
    AddPortraitIcon(pSprite, 36);

    if (m_dwFlags == 1) {
        pSprite->GetDerivedStats()->m_generalState |= STATE_HELPLESS | STATE_STUNNED;
        pSprite->field_9D15 = 1;
    }

    pSprite->sub_761650();

    if (m_durationType == 1) {
        pSprite->GetBaseStats()->m_generalState |= STATE_PANIC;
        m_forceRepass = TRUE;
        m_done = TRUE;
    } else {
        pSprite->GetDerivedStats()->m_generalState |= STATE_PANIC;
    }

    return TRUE;
}

// 0x4B5490
void CGameEffectPanic::OnRemove(CGameSprite* pSprite)
{
    PlaySound(CResRef("EFF_E05"), pSprite);
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectPoison::CGameEffectPoison(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x494A10
CGameEffect* CGameEffectPoison::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectPoison* copy = new CGameEffectPoison(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4AF610
BOOL CGameEffectPoison::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->m_derivedStats.m_generalState |= STATE_POISONED;

    if (m_secondaryType) {
        // NOTE: Uninline.
        DisplayStringRef(pSprite, m_dwFlags == 8 ? 37696 : 14662);
    }

    // NOTE: Uninline.
    AddPortraitIcon(pSprite, 6);

    if (m_secondaryType && m_dwFlags != 8) {
        m_special = g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime;
    }

    DWORD duration = m_durationType != 1 ? m_duration : 0;

    // NOTE: The CPersistantEffect84C420 constructor (0x4C2570) is inlined at
    // every site here.
    CPersistantEffect84C420* pEffect;
    switch (m_dwFlags) {
    case 2:
        pEffect = new CPersistantEffect84C420(0x200000,
            m_effectAmount,
            15,
            m_special,
            duration);
        if (pEffect == NULL) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
            // __LINE__: 9425
            UTIL_ASSERT_MSG(FALSE, "effectPtr != NULL");
            return TRUE;
        }
        break;
    case 3:
        pEffect = new CPersistantEffect84C420(0x200000,
            1,
            m_effectAmount * 15,
            m_special,
            duration);
        if (pEffect == NULL) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
            // __LINE__: 9431
            UTIL_ASSERT_MSG(FALSE, "effectPtr != NULL");
            return TRUE;
        }
        break;
    case 5:
        pEffect = new CPersistantEffect84C420(0x200000,
            m_effectAmount,
            735,
            m_special,
            duration);
        if (pEffect == NULL) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
            // __LINE__: 9452
            UTIL_ASSERT_MSG(FALSE, "effectPtr != NULL");
            return TRUE;
        }
        break;
    case 6:
        // A paralysing poison, then as 4.
        if (m_secondaryType) {
            // NOTE: Uninline.
            DisplayStringRef(pSprite, 14102);
        }

        pSprite->field_9D15 = 1;

        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 13);

        pSprite->m_derivedStats.m_spellStates.set(SPLSTATE_HELD, true);
        pSprite->m_derivedStats.m_generalState |= STATE_HELPLESS;
        // fall through
    case 4:
        pEffect = new CPersistantEffect84C420(0x200000,
            m_effectAmount,
            105,
            m_special,
            duration);
        if (pEffect == NULL) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
            // __LINE__: 9447
            UTIL_ASSERT_MSG(FALSE, "effectPtr != NULL");
            return TRUE;
        }
        break;
    case 7:
        pEffect = new CPersistantEffect84C420(0x200000,
            m_effectAmount,
            m_effectAmount2,
            m_special,
            duration);
        if (pEffect == NULL) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
            // __LINE__: 9457
            UTIL_ASSERT_MSG(FALSE, "effectPtr != NULL");
            return TRUE;
        }
        break;
    case 8:
        // Constitution poison: a CON change (effect 10) of the amount for 70
        // ticks.
        if (m_secondaryType) {
            ITEM_EFFECT effect;
            effect.effectID = CGAMEEFFECT_CON;
            effect.targetType = 2;
            effect.spellLevel = 10;
            effect.effectAmount = m_effectAmount;
            effect.dwFlags = 0;
            effect.durationType = 0;
            effect.duration = 70;
            effect.probabilityUpper = 100;
            effect.probabilityLower = 0;
            memset(effect.res, 0, sizeof(effect.res));
            effect.numDice = 0;
            effect.diceSize = 0;
            effect.savingThrow = 0;
            effect.saveMod = 0;
            effect.special = 0;

            CGameEffect* pCON = CGameEffect::DecodeEffect(&effect, CPoint(-1, -1), -1, CPoint(-1, -1));
            pCON->m_source = pSprite->GetPos();
            pCON->m_sourceID = pSprite->GetId();
            pCON->m_flags &= ~0x1;

            CMessage* message = new CMessageAddEffect(pCON, pSprite->GetId(), pSprite->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
        return TRUE;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 9490
        UTIL_ASSERT(FALSE);
        return TRUE;
    }

    pEffect->m_sourceID = m_sourceID;
    strcpy(reinterpret_cast<char*>(pEffect->field_29), m_sourceRes.GetResRefStr());
    pSprite->GetDerivedStats()->m_cRegeneratedPersistantEffectList.AddTail(pEffect);

    return TRUE;
}

// 0x4AFBF0
void CGameEffectPoison::OnRemove(CGameSprite* pSprite)
{
    if (m_dwFlags == 8) {
        ITEM_EFFECT effect;
        effect.effectID = CGAMEEFFECT_CON;
        effect.spellLevel = 10;
        effect.effectAmount = m_effectAmount + m_special;
        effect.probabilityUpper = 100;
        effect.probabilityLower = 0;
        effect.numDice = 0;
        effect.diceSize = 0;
        effect.saveMod = 0;
        effect.special = 0;
        effect.targetType = 2;
        effect.dwFlags = 0;
        effect.durationType = 0;
        effect.duration = 70;
        effect.savingThrow = 4;

        CGameEffect* pEffect = DecodeEffect(&effect,
            CPoint(-1, -1),
            -1,
            CPoint(-1, -1));
        pEffect->SetSource(pSprite->GetPos());
        pEffect->SetSourceId(pSprite->GetId());
        pEffect->m_flags &= ~1;

        CMessage* message = new CMessageAddEffect(pEffect,
            pSprite->GetId(),
            pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectRemoveCurse::CGameEffectRemoveCurse(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x494AC0
CGameEffect* CGameEffectRemoveCurse::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectRemoveCurse* copy = new CGameEffectRemoveCurse(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BB920
BOOL CGameEffectRemoveCurse::ApplyEffect(CGameSprite* pSprite)
{
    // Every cursed item (file flag 0x10) is unequipped and moved to the
    // pack, or dropped when the pack is full.
    SHORT nSelectedWeapon = pSprite->m_equipment.m_selectedWeapon;
    SHORT nLauncher = pSprite->GetLauncherSlot(nSelectedWeapon, pSprite->m_equipment.m_selectedWeaponAbility);

    BOOL bReequip = FALSE;
    for (INT nSlot = 0; nSlot <= 50; nSlot++) {
        CItem* pItem = pSprite->m_equipment.m_items[nSlot];
        if (pItem == NULL || (pItem->GetFlagsFile() & 0x10) == 0) {
            continue;
        }

        if (nSlot < 10) {
            pSprite->Unequip(static_cast<SHORT>(nSlot));
        } else if (nSlot == nSelectedWeapon) {
            pSprite->Unequip(static_cast<SHORT>(nSlot));
            bReequip = TRUE;

            CAbilityId abilityId;
            abilityId.m_itemType = 2;
            abilityId.m_itemNum = nSelectedWeapon;
            abilityId.m_abilityNum = pSprite->m_equipment.m_selectedWeaponAbility;

            pSprite->CheckQuickLists(&abilityId, 0, TRUE, FALSE);
        } else if (nSlot == nLauncher) {
            pSprite->Unequip(static_cast<SHORT>(nSlot));
            bReequip = TRUE;
        }

        BOOL bMoved = FALSE;
        INT nPack;
        if (g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(pSprite->GetId()) == -1) {
            // NOTE: For a creature that is not a sprite the original leaves
            // the item where it is.
            if (pSprite->GetObjectType() != CGameObject::TYPE_SPRITE) {
                continue;
            }

            for (nPack = 0; nPack < 28; nPack++) {
                if (pSprite->m_equipment.m_items[CGameSpriteEquipment::SLOT_MISC + nPack] == NULL) {
                    pSprite->m_equipment.m_items[CGameSpriteEquipment::SLOT_MISC + nPack] = pSprite->m_equipment.m_items[nSlot];
                    pSprite->m_equipment.m_items[nSlot] = NULL;
                    bMoved = TRUE;
                    break;
                }
            }
        } else {
            for (nPack = 0; nPack < 24; nPack++) {
                if (pSprite->m_equipment.m_items[18 + nPack] == NULL) {
                    pSprite->m_equipment.m_items[18 + nPack] = pSprite->m_equipment.m_items[nSlot];
                    pSprite->m_equipment.m_items[nSlot] = NULL;
                    bMoved = TRUE;
                    break;
                }
            }
        }

        if (!bMoved) {
            pSprite->PutItemGround(pSprite->m_equipment.m_items[nSlot]);
            pSprite->m_equipment.m_items[nSlot] = NULL;
        }
    }

    if (bReequip) {
        pSprite->EquipMostDamagingMelee();
    }

    CMessage* message = new CMessageSpriteEquipment(pSprite, pSprite->GetId(), pSprite->GetId());
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

    m_done = TRUE;
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectResistAcid::CGameEffectResistAcid(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x494B70
CGameEffect* CGameEffectResistAcid::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectResistAcid* copy = new CGameEffectResistAcid(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4AFCF0
BOOL CGameEffectResistAcid::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_resistAcidBase += static_cast<BYTE>(m_effectAmount);
            if (pSprite->GetBaseStats()->m_resistAcidBase > 100) {
                pSprite->GetBaseStats()->m_resistAcidBase = 100;
            }
            if (pSprite->GetBaseStats()->m_resistAcidBase < 0) {
                pSprite->GetBaseStats()->m_resistAcidBase = 0;
            }
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->m_bonusStats.m_nResistAcid += static_cast<BYTE>(m_effectAmount);
            m_done = FALSE;
        }
        break;
    case 1:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_resistAcidBase = static_cast<BYTE>(m_effectAmount);
            if (pSprite->GetBaseStats()->m_resistAcidBase > 100) {
                pSprite->GetBaseStats()->m_resistAcidBase = 100;
            }
            if (pSprite->GetBaseStats()->m_resistAcidBase < 0) {
                pSprite->GetBaseStats()->m_resistAcidBase = 0;
            }
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            if (pSprite->GetDerivedStats()->m_nResistAcid < m_effectAmount) {
                pSprite->GetDerivedStats()->m_nResistAcid = static_cast<BYTE>(m_effectAmount);
            }
            m_done = FALSE;
        }
        break;
    case 2:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_resistAcidBase = static_cast<BYTE>(m_effectAmount) * pSprite->GetBaseStats()->m_resistAcidBase / 100;
            if (pSprite->GetBaseStats()->m_resistAcidBase > 100) {
                pSprite->GetBaseStats()->m_resistAcidBase = 100;
            }
            if (pSprite->GetBaseStats()->m_resistAcidBase < 0) {
                pSprite->GetBaseStats()->m_resistAcidBase = 0;
            }
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->GetDerivedStats()->m_nResistAcid = static_cast<SHORT>(m_effectAmount) * pSprite->GetBaseStats()->m_resistAcidBase / 100;
            m_done = FALSE;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 9674
        UTIL_ASSERT(FALSE);
    }
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectResistCold::CGameEffectResistCold(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x494C20
CGameEffect* CGameEffectResistCold::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectResistCold* copy = new CGameEffectResistCold(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4AFEA0
BOOL CGameEffectResistCold::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_resistColdBase += static_cast<BYTE>(m_effectAmount);
            if (pSprite->GetBaseStats()->m_resistColdBase > 100) {
                pSprite->GetBaseStats()->m_resistColdBase = 100;
            }
            if (pSprite->GetBaseStats()->m_resistColdBase < 0) {
                pSprite->GetBaseStats()->m_resistColdBase = 0;
            }
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->m_bonusStats.m_nResistCold += static_cast<BYTE>(m_effectAmount);
            m_done = FALSE;
        }
        break;
    case 1:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_resistColdBase = static_cast<BYTE>(m_effectAmount);
            if (pSprite->GetBaseStats()->m_resistColdBase > 100) {
                pSprite->GetBaseStats()->m_resistColdBase = 100;
            }
            if (pSprite->GetBaseStats()->m_resistColdBase < 0) {
                pSprite->GetBaseStats()->m_resistColdBase = 0;
            }
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            if (pSprite->GetDerivedStats()->m_nResistCold < m_effectAmount) {
                pSprite->GetDerivedStats()->m_nResistCold = static_cast<BYTE>(m_effectAmount);
            }
            m_done = FALSE;
        }
        break;
    case 2:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_resistColdBase = static_cast<BYTE>(m_effectAmount) * pSprite->GetBaseStats()->m_resistColdBase / 100;
            if (pSprite->GetBaseStats()->m_resistColdBase > 100) {
                pSprite->GetBaseStats()->m_resistColdBase = 100;
            }
            if (pSprite->GetBaseStats()->m_resistColdBase < 0) {
                pSprite->GetBaseStats()->m_resistColdBase = 0;
            }
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->GetDerivedStats()->m_nResistCold = static_cast<SHORT>(m_effectAmount) * pSprite->GetBaseStats()->m_resistColdBase / 100;
            m_done = FALSE;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 9805
        UTIL_ASSERT(FALSE);
    }
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectResistElectricity::CGameEffectResistElectricity(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x494CD0
CGameEffect* CGameEffectResistElectricity::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectResistElectricity* copy = new CGameEffectResistElectricity(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B0050
BOOL CGameEffectResistElectricity::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_resistElectricityBase += static_cast<BYTE>(m_effectAmount);
            if (pSprite->GetBaseStats()->m_resistElectricityBase > 100) {
                pSprite->GetBaseStats()->m_resistElectricityBase = 100;
            }
            if (pSprite->GetBaseStats()->m_resistElectricityBase < 0) {
                pSprite->GetBaseStats()->m_resistElectricityBase = 0;
            }
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->m_bonusStats.m_nResistElectricity += static_cast<BYTE>(m_effectAmount);
            m_done = FALSE;
        }
        break;
    case 1:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_resistElectricityBase = static_cast<BYTE>(m_effectAmount);
            if (pSprite->GetBaseStats()->m_resistElectricityBase > 100) {
                pSprite->GetBaseStats()->m_resistElectricityBase = 100;
            }
            if (pSprite->GetBaseStats()->m_resistElectricityBase < 0) {
                pSprite->GetBaseStats()->m_resistElectricityBase = 0;
            }
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            if (pSprite->GetDerivedStats()->m_nResistElectricity < m_effectAmount) {
                pSprite->GetDerivedStats()->m_nResistElectricity = static_cast<BYTE>(m_effectAmount);
            }
            m_done = FALSE;
        }
        break;
    case 2:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_resistElectricityBase = static_cast<BYTE>(m_effectAmount) * pSprite->GetBaseStats()->m_resistElectricityBase / 100;
            if (pSprite->GetBaseStats()->m_resistElectricityBase > 100) {
                pSprite->GetBaseStats()->m_resistElectricityBase = 100;
            }
            if (pSprite->GetBaseStats()->m_resistElectricityBase < 0) {
                pSprite->GetBaseStats()->m_resistElectricityBase = 0;
            }
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->GetDerivedStats()->m_nResistElectricity = static_cast<SHORT>(m_effectAmount) * pSprite->GetBaseStats()->m_resistElectricityBase / 100;
            m_done = FALSE;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 9936
        UTIL_ASSERT(FALSE);
    }
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectResistFire::CGameEffectResistFire(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x494D80
CGameEffect* CGameEffectResistFire::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectResistFire* copy = new CGameEffectResistFire(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B0200
BOOL CGameEffectResistFire::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_resistFireBase += static_cast<BYTE>(m_effectAmount);
            if (pSprite->GetBaseStats()->m_resistFireBase > 100) {
                pSprite->GetBaseStats()->m_resistFireBase = 100;
            }
            if (pSprite->GetBaseStats()->m_resistFireBase < 0) {
                pSprite->GetBaseStats()->m_resistFireBase = 0;
            }
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->m_bonusStats.m_nResistFire += static_cast<SHORT>(m_effectAmount);
            m_done = FALSE;
        }
        break;
    case 1:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_resistFireBase = static_cast<BYTE>(m_effectAmount);
            if (pSprite->GetBaseStats()->m_resistFireBase > 100) {
                pSprite->GetBaseStats()->m_resistFireBase = 100;
            }
            if (pSprite->GetBaseStats()->m_resistFireBase < 0) {
                pSprite->GetBaseStats()->m_resistFireBase = 0;
            }
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            if (pSprite->GetDerivedStats()->m_nResistFire < m_effectAmount) {
                pSprite->GetDerivedStats()->m_nResistFire = static_cast<BYTE>(m_effectAmount);
            }
            m_done = FALSE;
        }
        break;
    case 2:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_resistFireBase = static_cast<SHORT>(m_effectAmount) * pSprite->GetBaseStats()->m_resistFireBase / 100;
            if (pSprite->GetBaseStats()->m_resistFireBase > 100) {
                pSprite->GetBaseStats()->m_resistFireBase = 100;
            }
            if (pSprite->GetBaseStats()->m_resistFireBase < 0) {
                pSprite->GetBaseStats()->m_resistFireBase = 0;
            }
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->GetDerivedStats()->m_nResistFire = static_cast<SHORT>(m_effectAmount) * pSprite->GetBaseStats()->m_resistFireBase / 100;
            m_done = FALSE;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 10067
        UTIL_ASSERT(FALSE);
    }
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectResistMagic::CGameEffectResistMagic(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x494E30
CGameEffect* CGameEffectResistMagic::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectResistMagic* copy = new CGameEffectResistMagic(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B03B0
BOOL CGameEffectResistMagic::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        if (m_effectAmount >= 0 || pSprite->m_tempStats.m_nMagicDamageResistance != 100) {
            pSprite->m_bonusStats.m_nMagicDamageResistance += static_cast<SHORT>(m_effectAmount);
            m_done = FALSE;
        }
        break;
    case 1:
        if (pSprite->GetDerivedStats()->m_nMagicDamageResistance < m_effectAmount) {
            pSprite->GetDerivedStats()->m_nMagicDamageResistance = static_cast<SHORT>(m_effectAmount);
        }
        m_done = FALSE;
        break;
    case 2:
        pSprite->GetDerivedStats()->m_nMagicDamageResistance = static_cast<SHORT>(m_effectAmount) * pSprite->GetDerivedStats()->m_nMagicDamageResistance / 100;
        m_done = FALSE;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 10154
        UTIL_ASSERT(FALSE);
    }
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectResurrect::CGameEffectResurrect(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x494EE0
CGameEffect* CGameEffectResurrect::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectResurrect* copy = new CGameEffectResurrect(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B0490
BOOL CGameEffectResurrect::ApplyEffect(CGameSprite* pSprite)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameObject* pObject;
    BYTE rc;

    // A statue comes back only through Stone to Flesh's flags 0x2B.
    if ((pSprite->m_tempStats.m_generalState & STATE_STONE_DEATH) != 0 && m_dwFlags != 0x2B) {
        m_done = TRUE;
        return TRUE;
    }

    // The body comes back as the commoner animation of the sprite's gender,
    // race and best class.
    WORD nGender;
    switch (pSprite->GetAIType().m_nGender) {
    case 1:
        nGender = 0;
        break;
    case 2:
        nGender = 0x10;
        break;
    default:
        m_done = TRUE;
        return TRUE;
    }

    WORD nRace;
    switch (pSprite->GetAIType().m_nRace) {
    case 1:
        nRace = 0;
        break;
    case 2:
    case 3:
        nRace = 1;
        break;
    case 4:
        nRace = 2;
        break;
    case 5:
        nRace = 3;
        break;
    case 6:
        nRace = 4;
        break;
    case 7:
        nRace = 5;
        break;
    default:
        m_done = TRUE;
        return TRUE;
    }

    WORD nClass;
    switch (pSprite->m_derivedStats.GetBestClass()) {
    case 1:
    case 5:
    case 6:
    case 7:
    case 8:
        nClass = 0x100;
        break;
    case 2:
    case 9:
        nClass = 0x300;
        break;
    case 3:
    case 4:
        nClass = 0;
        break;
    case 10:
    case 11:
        nClass = 0x200;
        break;
    default:
        m_done = TRUE;
        return TRUE;
    }

    WORD nAnimationType = nClass | nRace | nGender | 0x6000;

    if ((pSprite->m_tempStats.m_generalState & STATE_DEAD) != 0) {
        pSprite->m_baseStats.field_294 = 0;

        // Back at the source, or at the protagonist when the source is
        // gone or dead.
        LONG nSourceID = m_sourceID;
        do {
            rc = pGame->GetObjectArray()->GetShare(nSourceID,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            BOOLEAN bDead = IcewindMisc::IsDead(static_cast<CGameSprite*>(pObject));
            pGame->GetObjectArray()->ReleaseShare(nSourceID,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            if (bDead == TRUE) {
                nSourceID = pGame->GetProtagonist();
            }
        } else {
            nSourceID = pGame->GetProtagonist();
        }

        if ((pSprite->m_tempStats.m_generalState & STATE_STONE_DEATH) != 0) {
            pSprite->m_equipedEffectList.RemoveAllOfType(pSprite, CGAMEEFFECT_PORTRAITICON, pSprite->m_equipedEffectList.m_posCurrent, 71);
            pSprite->m_timedEffectList.RemoveAllOfType(pSprite, CGAMEEFFECT_PORTRAITICON, pSprite->m_timedEffectList.m_posCurrent, 71);
        }

        pSprite->m_baseStats.m_animationType = nAnimationType;

        pSprite->m_baseStats.m_generalState &= ~STATE_DEAD;
        pSprite->m_derivedStats.m_generalState &= ~STATE_DEAD;
        pSprite->m_baseStats.m_generalState &= ~0x400;
        pSprite->m_derivedStats.m_generalState &= ~0x400;
        pSprite->m_baseStats.m_generalState &= ~0x200;
        pSprite->m_derivedStats.m_generalState &= ~0x200;
        pSprite->m_baseStats.m_generalState &= ~0x100;
        pSprite->m_derivedStats.m_generalState &= ~0x100;
        pSprite->m_baseStats.m_generalState &= ~STATE_STONE_DEATH;
        pSprite->m_derivedStats.m_generalState &= ~STATE_STONE_DEATH;
        pSprite->m_baseStats.m_generalState &= ~STATE_FROZEN_DEATH;
        pSprite->m_derivedStats.m_generalState &= ~STATE_FROZEN_DEATH;

        pSprite->m_equipedEffectList.RemoveAllOfType(pSprite, 416, pSprite->m_equipedEffectList.m_posCurrent, -1);
        pSprite->m_timedEffectList.RemoveAllOfType(pSprite, 416, pSprite->m_timedEffectList.m_posCurrent, -1);

        do {
            rc = pGame->GetObjectArray()->GetShare(nSourceID,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc != CGameObjectArray::SUCCESS) {
            // NOTE: The original returns 0 here without ending the effect
            // (0x4B075A).
            return FALSE;
        }

        CGameSprite* pSource = static_cast<CGameSprite*>(pObject);
        CGameArea* pArea = pSource->GetArea();

        pSprite->ResetAITypeLive();

        CAIObjectType typeAI;
        if (pArea == pSprite->GetArea()
            && (pSprite->GetArea() != NULL || pGame->GetCharacterPortraitNum(pSprite->GetId()) == -1)) {
            // Where the body lies.
            pSprite->m_hasColorEffects = TRUE;

            // NOTE: Inlined CGameAnimation::SetColorRange (ObjAnimation.h
            // line 2111).
            for (INT nRange = 0; nRange < 7; nRange++) {
                pSprite->GetAnimation()->m_animation->SetColorRange(static_cast<BYTE>(nRange), pSprite->m_baseStats.m_colors[nRange]);
            }

            pSprite->m_baseStats.m_animationType = nAnimationType;

            // NOTE: Inlined CGameAnimation::SetAnimationType.
            pSprite->GetAnimation()->SetAnimationType(nAnimationType, pSprite->m_baseStats.m_colors, 0);

            pSprite->MoveToFront();
            pSprite->SetSequence(CGameSprite::SEQ_READY);
            pSprite->m_posZ = 0;

            typeAI.Set(pSprite->GetAIType());
            typeAI.m_nGeneral = CAIObjectType::G_HUMANOID;
            pSprite->SetAIType(typeAI, FALSE, FALSE);
            typeAI.Set(pSprite->m_startTypeAI);
            typeAI.m_nGeneral = CAIObjectType::G_HUMANOID;
            pSprite->m_startTypeAI.Set(typeAI);

            // NOTE: Inlined CGameObject::InControl.
            if (pSprite->InControl()) {
                CMessage* message = new CMessageVisibilityMapMove(TRUE, pSprite->GetId(), pSprite->GetId());
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }

            // NOTE: The original keeps the source's share on this path (it
            // jumps past the ReleaseShare at 0x4B0BC5).
        } else {
            // Next to the source, in its area.
            if (pSprite->GetArea() != NULL) {
                pSprite->RemoveFromArea();
            }

            CPoint ptDest(pSource->GetPos().x / CPathSearch::GRID_SQUARE_SIZEX,
                pSource->GetPos().y / CPathSearch::GRID_SQUARE_SIZEY);
            if (!pSprite->m_baseStats.field_294) {
                ptDest = pArea->m_search.GetNearestOpenSquare(ptDest,
                    pSprite->GetTerrainTable(),
                    pSprite->GetAnimation()->GetPersonalSpace(),
                    -1);
                ptDest.x = ptDest.x * CPathSearch::GRID_SQUARE_SIZEX + CPathSearch::GRID_SQUARE_SIZEX / 2;
                ptDest.y = ptDest.y * CPathSearch::GRID_SQUARE_SIZEY + CPathSearch::GRID_SQUARE_SIZEY / 2;
            }

            pGame->m_bInAreaTransition = TRUE;
            pSprite->AddToArea(pArea, ptDest, 0, CGameObject::LIST_FRONT);
            pGame->m_bInAreaTransition = FALSE;

            pSprite->m_hasColorEffects = TRUE;

            // NOTE: Inlined CGameAnimation::SetColorRange.
            for (INT nRange = 0; nRange < 7; nRange++) {
                pSprite->GetAnimation()->m_animation->SetColorRange(static_cast<BYTE>(nRange), pSprite->m_baseStats.m_colors[nRange]);
            }

            pSprite->m_baseStats.m_animationType = nAnimationType;

            // NOTE: Inlined CGameAnimation::SetAnimationType.
            pSprite->GetAnimation()->SetAnimationType(nAnimationType, pSprite->m_baseStats.m_colors, 0);

            pSprite->SetSequence(CGameSprite::SEQ_READY);
            pSprite->m_posZ = 0;
            pSprite->SetFacing(pSprite->GetDirection(pSource->GetPos()));

            typeAI.Set(pSprite->GetAIType());
            typeAI.m_nGeneral = CAIObjectType::G_HUMANOID;
            pSprite->SetAIType(typeAI, FALSE, FALSE);
            typeAI.Set(pSprite->m_startTypeAI);
            typeAI.m_nGeneral = CAIObjectType::G_HUMANOID;
            pSprite->m_startTypeAI.Set(typeAI);

            // NOTE: Inlined CGameObject::InControl.
            if (pSprite->InControl()) {
                CMessage* message = new CMessageVisibilityMapMove(TRUE, pSprite->GetId(), pSprite->GetId());
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }

            pGame->GetObjectArray()->ReleaseShare(nSourceID,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }

        pSprite->m_baseStats.m_hitPoints = 1;
    }

    pGame->UpdatePortrait(pGame->GetCharacterPortraitNum(pSprite->GetId()), 1);

    pSprite->UnequipAll(TRUE);
    sub_4B8730(pSprite, 42);
    pSprite->EquipAll(TRUE);

    if (g_pBaldurChitin->GetActiveEngine() != NULL) {
        CUIPanel* pPanel = g_pBaldurChitin->GetActiveEngine()->GetManager()->GetPanel(1);
        if (pPanel != NULL) {
            pPanel->InvalidateRect(NULL);
        }
    }

    // NOTE: Inlined CGameObject::InControl.
    if (pSprite->InControl()) {
        pSprite->m_cLastSpriteUpdate.m_bFullUpdateRequired = TRUE;
        pSprite->m_bSendSpriteUpdate = TRUE;
    }

    m_done = TRUE;
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectSaveVsFortitude::CGameEffectSaveVsFortitude(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x494F90
CGameEffect* CGameEffectSaveVsFortitude::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectSaveVsFortitude* copy = new CGameEffectSaveVsFortitude(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B0D10
BOOL CGameEffectSaveVsFortitude::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        switch (m_durationType) {
        case 1:
            pSprite->GetBaseStats()->m_saveVSFortitudeBase += static_cast<BYTE>(m_effectAmount);

            if (pSprite->GetBaseStats()->m_saveVSFortitudeBase > 40) {
                pSprite->GetBaseStats()->m_saveVSFortitudeBase = 40;
            }

            if (pSprite->GetBaseStats()->m_saveVSFortitudeBase < -40) {
                pSprite->GetBaseStats()->m_saveVSFortitudeBase = -40;
            }

            m_forceRepass = TRUE;
            m_done = TRUE;
            break;
        default:
            pSprite->m_bonusStats.m_nSaveVSFortitude += static_cast<SHORT>(m_effectAmount);
            m_done = FALSE;
            break;
        }
        break;
    case 1:
        switch (m_durationType) {
        case 1:
            pSprite->GetBaseStats()->m_saveVSFortitudeBase = static_cast<BYTE>(m_effectAmount);

            if (pSprite->GetBaseStats()->m_saveVSFortitudeBase > 40) {
                pSprite->GetBaseStats()->m_saveVSFortitudeBase = 40;
            }

            if (pSprite->GetBaseStats()->m_saveVSFortitudeBase < -40) {
                pSprite->GetBaseStats()->m_saveVSFortitudeBase = -40;
            }

            m_forceRepass = TRUE;
            m_done = TRUE;
            break;
        default:
            pSprite->GetDerivedStats()->m_nSaveVSFortitude = static_cast<BYTE>(m_effectAmount);

            if (pSprite->GetDerivedStats()->m_nSaveVSFortitude > 40) {
                pSprite->GetDerivedStats()->m_nSaveVSFortitude = 40;
            }

            if (pSprite->GetDerivedStats()->m_nSaveVSFortitude < -40) {
                pSprite->GetDerivedStats()->m_nSaveVSFortitude = -40;
            }

            m_done = FALSE;
            break;
        }
        break;
    case 2:
        switch (m_durationType) {
        case 1:
            pSprite->GetBaseStats()->m_saveVSFortitudeBase = pSprite->GetBaseStats()->m_saveVSFortitudeBase * static_cast<BYTE>(m_effectAmount) / 100;

            if (pSprite->GetBaseStats()->m_saveVSFortitudeBase > 40) {
                pSprite->GetBaseStats()->m_saveVSFortitudeBase = 40;
            }

            if (pSprite->GetBaseStats()->m_saveVSFortitudeBase < -40) {
                pSprite->GetBaseStats()->m_saveVSFortitudeBase = -40;
            }

            m_forceRepass = TRUE;
            m_done = TRUE;
            break;
        default:
            pSprite->GetDerivedStats()->m_nSaveVSFortitude = pSprite->GetBaseStats()->m_saveVSFortitudeBase * static_cast<BYTE>(m_effectAmount) / 100;

            if (pSprite->GetDerivedStats()->m_nSaveVSFortitude > 40) {
                pSprite->GetDerivedStats()->m_nSaveVSFortitude = 40;
            }

            if (pSprite->GetDerivedStats()->m_nSaveVSFortitude < -40) {
                pSprite->GetDerivedStats()->m_nSaveVSFortitude = -40;
            }

            m_done = FALSE;
            break;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 10621
        UTIL_ASSERT(FALSE);
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectSaveVsRelfex::CGameEffectSaveVsRelfex(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x495040
CGameEffect* CGameEffectSaveVsRelfex::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectSaveVsRelfex* copy = new CGameEffectSaveVsRelfex(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B0F20
BOOL CGameEffectSaveVsRelfex::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        switch (m_durationType) {
        case 1:
            pSprite->GetBaseStats()->m_saveVSReflexBase += static_cast<BYTE>(m_effectAmount);

            if (pSprite->GetBaseStats()->m_saveVSReflexBase > 40) {
                pSprite->GetBaseStats()->m_saveVSReflexBase = 40;
            }

            if (pSprite->GetBaseStats()->m_saveVSReflexBase < -40) {
                pSprite->GetBaseStats()->m_saveVSReflexBase = -40;
            }

            m_forceRepass = TRUE;
            m_done = TRUE;
            break;
        default:
            pSprite->m_bonusStats.m_nSaveVSReflex += static_cast<SHORT>(m_effectAmount);
            m_done = FALSE;
            break;
        }
        break;
    case 1:
        switch (m_durationType) {
        case 1:
            pSprite->GetBaseStats()->m_saveVSReflexBase = static_cast<BYTE>(m_effectAmount);

            if (pSprite->GetBaseStats()->m_saveVSReflexBase > 40) {
                pSprite->GetBaseStats()->m_saveVSReflexBase = 40;
            }

            if (pSprite->GetBaseStats()->m_saveVSReflexBase < -40) {
                pSprite->GetBaseStats()->m_saveVSReflexBase = -40;
            }

            m_forceRepass = TRUE;
            m_done = TRUE;
            break;
        default:
            pSprite->GetDerivedStats()->m_nSaveVSReflex = static_cast<BYTE>(m_effectAmount);

            if (pSprite->GetDerivedStats()->m_nSaveVSReflex > 40) {
                pSprite->GetDerivedStats()->m_nSaveVSReflex = 40;
            }

            if (pSprite->GetDerivedStats()->m_nSaveVSReflex < -40) {
                pSprite->GetDerivedStats()->m_nSaveVSReflex = -40;
            }

            m_done = FALSE;
            break;
        }
        break;
    case 2:
        switch (m_durationType) {
        case 1:
            pSprite->GetBaseStats()->m_saveVSReflexBase = pSprite->GetBaseStats()->m_saveVSReflexBase * static_cast<BYTE>(m_effectAmount) / 100;

            if (pSprite->GetBaseStats()->m_saveVSReflexBase > 40) {
                pSprite->GetBaseStats()->m_saveVSReflexBase = 40;
            }

            if (pSprite->GetBaseStats()->m_saveVSReflexBase < -40) {
                pSprite->GetBaseStats()->m_saveVSReflexBase = -40;
            }

            m_forceRepass = TRUE;
            m_done = TRUE;
            break;
        default:
            pSprite->GetDerivedStats()->m_nSaveVSReflex = pSprite->GetBaseStats()->m_saveVSReflexBase * static_cast<BYTE>(m_effectAmount) / 100;

            if (pSprite->GetDerivedStats()->m_nSaveVSReflex > 40) {
                pSprite->GetDerivedStats()->m_nSaveVSReflex = 40;
            }

            if (pSprite->GetDerivedStats()->m_nSaveVSReflex < -40) {
                pSprite->GetDerivedStats()->m_nSaveVSReflex = -40;
            }

            m_done = FALSE;
            break;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 10730
        UTIL_ASSERT(FALSE);
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectSaveVsWill::CGameEffectSaveVsWill(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4950F0
CGameEffect* CGameEffectSaveVsWill::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectSaveVsWill* copy = new CGameEffectSaveVsWill(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B1130
BOOL CGameEffectSaveVsWill::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        switch (m_durationType) {
        case 1:
            pSprite->GetBaseStats()->m_saveVSWillBase += static_cast<BYTE>(m_effectAmount);

            if (pSprite->GetBaseStats()->m_saveVSWillBase > 40) {
                pSprite->GetBaseStats()->m_saveVSWillBase = 40;
            }

            if (pSprite->GetBaseStats()->m_saveVSWillBase < -40) {
                pSprite->GetBaseStats()->m_saveVSWillBase = -40;
            }

            m_forceRepass = TRUE;
            m_done = TRUE;
            break;
        default:
            pSprite->m_bonusStats.m_nSaveVSWill += static_cast<SHORT>(m_effectAmount);
            m_done = FALSE;
            break;
        }
        break;
    case 1:
        switch (m_durationType) {
        case 1:
            pSprite->GetBaseStats()->m_saveVSWillBase = static_cast<BYTE>(m_effectAmount);

            if (pSprite->GetBaseStats()->m_saveVSWillBase > 40) {
                pSprite->GetBaseStats()->m_saveVSWillBase = 40;
            }

            if (pSprite->GetBaseStats()->m_saveVSWillBase < -40) {
                pSprite->GetBaseStats()->m_saveVSWillBase = -40;
            }

            m_forceRepass = TRUE;
            m_done = TRUE;
            break;
        default:
            pSprite->GetDerivedStats()->m_nSaveVSWill = static_cast<BYTE>(m_effectAmount);

            if (pSprite->GetDerivedStats()->m_nSaveVSWill > 40) {
                pSprite->GetDerivedStats()->m_nSaveVSWill = 40;
            }

            if (pSprite->GetDerivedStats()->m_nSaveVSWill < -40) {
                pSprite->GetDerivedStats()->m_nSaveVSWill = -40;
            }

            m_done = FALSE;
            break;
        }
        break;
    case 2:
        switch (m_durationType) {
        case 1:
            pSprite->GetBaseStats()->m_saveVSWillBase = pSprite->GetBaseStats()->m_saveVSWillBase * static_cast<BYTE>(m_effectAmount) / 100;

            if (pSprite->GetBaseStats()->m_saveVSWillBase > 40) {
                pSprite->GetBaseStats()->m_saveVSWillBase = 40;
            }

            if (pSprite->GetBaseStats()->m_saveVSWillBase < -40) {
                pSprite->GetBaseStats()->m_saveVSWillBase = -40;
            }

            m_forceRepass = TRUE;
            m_done = TRUE;
            break;
        default:
            pSprite->GetDerivedStats()->m_nSaveVSWill = pSprite->GetBaseStats()->m_saveVSWillBase * static_cast<BYTE>(m_effectAmount) / 100;

            if (pSprite->GetDerivedStats()->m_nSaveVSWill > 40) {
                pSprite->GetDerivedStats()->m_nSaveVSWill = 40;
            }

            if (pSprite->GetDerivedStats()->m_nSaveVSWill < -40) {
                pSprite->GetDerivedStats()->m_nSaveVSWill = -40;
            }

            m_done = FALSE;
            break;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 10839
        UTIL_ASSERT(FALSE);
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectSilence::CGameEffectSilence(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4951A0
CGameEffect* CGameEffectSilence::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectSilence* copy = new CGameEffectSilence(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B1340
BOOL CGameEffectSilence::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType != 0) {
        // NOTE: Uninline.
        DisplayStringRef(pSprite, 14676);
    }

    // NOTE: Uninline.
    AddPortraitIcon(pSprite, 34);

    if (m_durationType == 1) {
        pSprite->GetBaseStats()->m_generalState |= STATE_SILENCED;
        pSprite->GetDerivedStats()->m_generalState |= STATE_SILENCED;
        m_done = TRUE;
    } else {
        pSprite->GetDerivedStats()->m_generalState |= STATE_SILENCED;
    }

    return TRUE;
}

// 0x4B5490
void CGameEffectSilence::OnRemove(CGameSprite* pSprite)
{
    PlaySound(CResRef("EFF_E05"), pSprite);
}

// -----------------------------------------------------------------------------

// 0x495250
CGameEffectSleep::CGameEffectSleep(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x495290
CGameEffect* CGameEffectSleep::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectSleep* copy = new CGameEffectSleep(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B1430
BOOL CGameEffectSleep::ApplyEffect(CGameSprite* pSprite)
{
    if (m_durationType == 1) {
        pSprite->GetBaseStats()->m_generalState |= STATE_SLEEPING;
        pSprite->GetDerivedStats()->m_generalState |= STATE_SLEEPING;
        pSprite->GetBaseStats()->m_generalState |= STATE_HELPLESS;
        pSprite->GetDerivedStats()->m_generalState |= STATE_HELPLESS;
        m_done = TRUE;
    } else {
        pSprite->GetDerivedStats()->m_generalState |= STATE_SLEEPING | STATE_HELPLESS;

        if (m_dwFlags == 1) {
            pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_DOESNT_AWAKEN_ON_DAMAGE, true);
        }

        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 14);

        if (m_secondaryType) {
            // NOTE: Uninline.
            DisplayStringRef(pSprite, 14001);
        }

        ITEM_EFFECT* effect = new ITEM_EFFECT;

        // NOTE: Uninline.
        ClearItemEffect(effect, CGAMEEFFECT_SETSEQUENCE);

        effect->dwFlags = CGameSprite::SEQ_AWAKE;
        effect->durationType = 4;
        effect->duration = (m_duration - g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime) / 15;
        CGameEffect* pEffect = CGameEffect::DecodeEffect(effect,
            CPoint(-1, -1),
            -1,
            CPoint(-1, -1));
        delete effect;
        pSprite->AddEffect(pEffect, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);
    }

    if (pSprite->m_nSequence != CGameSprite::SEQ_DIE
        && pSprite->m_nSequence != CGameSprite::SEQ_SLEEP
        && pSprite->m_nSequence != CGameSprite::SEQ_TWITCH) {
        pSprite->SetSequence(CGameSprite::SEQ_SLEEP);
    }

    pSprite->field_9D15 = 1;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectSlow::CGameEffectSlow(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x495340
CGameEffect* CGameEffectSlow::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectSlow* copy = new CGameEffectSlow(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B1650
BOOL CGameEffectSlow::ApplyEffect(CGameSprite* pSprite)
{
    if (g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_cutScene != TRUE) {
        if (m_secondaryType != 0) {
            // NOTE: Uninline.
            DisplayStringRef(pSprite, 14668); // "Slowed"
        }

        AddSlowEffect(pSprite);
    } else {
        m_done = TRUE;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectSparkle::CGameEffectSparkle(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4953F0
CGameEffect* CGameEffectSparkle::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectSparkle* copy = new CGameEffectSparkle(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B1700
BOOL CGameEffectSparkle::ApplyEffect(CGameSprite* pSprite)
{
    WORD nType = static_cast<WORD>(m_dwFlags);

    CRect rFx;
    CPoint ptReference;
    pSprite->GetAnimation()->CalculateFxRect(rFx, ptReference, pSprite->m_posZ);
    LONG nHeight = (rFx.bottom - rFx.top) * 8 / 10;

    CPoint ptPos = pSprite->GetPos();
    if ((m_source.x != -1 || m_source.y != -1)
        && (pSprite->GetPos().x != m_source.x || pSprite->GetPos().y != m_source.y)) {
        // TODO: Incomplete. The original moves the position towards
        // m_source by CGameSprite 0x756770 (not in src/: the vector from the
        // sprite to m_source scaled to a length of 15, no rand); until then
        // the sparkles start at the sprite's position.
    } else {
        ptPos.y++;
    }

    CGameObject84D624* pSparkle;
    switch (nType) {
    case 1:
        pSparkle = new CGameObject84D624(static_cast<WORD>(m_effectAmount),
            CGameObject84D624::TYPE_EXPLOSION,
            CGameObject84D624::EXPLOSION_PUFFS);
        // NOTE: Inlined CGameObject84D624::SetSource.
        pSparkle->SetSource(m_source);
        pSparkle->AddToArea(pSprite->GetArea(), ptPos, static_cast<SHORT>(nHeight), CGameObject::LIST_FRONT);
        break;
    case 3:
        pSparkle = new CGameObject84D624(static_cast<WORD>(m_effectAmount),
            CGameObject84D624::TYPE_SHOWER,
            static_cast<WORD>(m_duration - g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime));
        pSparkle->AddToArea(pSprite->GetArea(), ptPos, static_cast<SHORT>(nHeight), CGameObject::LIST_FRONT);
        break;
    }

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectSpellMemorizationMage::CGameEffectSpellMemorizationMage(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4954A0
CGameEffect* CGameEffectSpellMemorizationMage::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectSpellMemorizationMage* copy = new CGameEffectSpellMemorizationMage(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B1960
BOOL CGameEffectSpellMemorizationMage::ApplyEffect(CGameSprite* pSprite)
{
    const CAIObjectType& typeAI = pSprite->GetAIType();

    // NOTE: The original calls CDerivedStats::sub_447B00 once, with the BARD
    // class, and reuses the returned list for every class walk below.
    CGameSpriteGroupedSpellList* pSpells = pSprite->m_derivedStats.sub_447B00(CAIObjectType::C_BARD);

    if (typeAI.IsClassValid(CAIObjectType::C_BARD)) {
        DWORD nMask = 1;

        for (UINT nLevel = 0; nLevel < pSpells->m_nHighestLevel; nLevel++, nMask <<= 1) {
            if ((m_dwFlags & nMask) == 0) {
                continue;
            }

            CGameSpriteSpellList* pList = pSpells->GetSpellsAtLevel(nLevel);
            if (pList->m_List.empty()) {
                continue;
            }

            for (UINT nIndex = 0; nIndex < pList->m_List.size(); nIndex++) {
                CGameSpriteSpellListEntry* pEntry = pList->Get(nIndex);
                if (pEntry->field_4 == 0) {
                    continue;
                }

                UINT nID = pEntry->m_nID;

                // NOTE: Inlined CSpellResRefList::Get.
                const CResRef& resRef = g_pBaldurChitin->GetObjectGame()->m_spells.Get(nID);

                pSprite->AddKnownSpell(CAIObjectType::C_BARD, nLevel, resRef, m_effectAmount, m_effectAmount, 0);
            }
        }
    }

    if (typeAI.IsClassValid(CAIObjectType::C_SORCERER)) {
        // NOTE: The original copies the grouped spell list through
        // CGameSprite::GetSpells (0x724730) and 0x444D10 and destroys the copy
        // (0x7E78DE); the result is unused, so it is not reproduced.
        DWORD nMask = 1;

        for (UINT nLevel = 0; nLevel < pSpells->m_nHighestLevel; nLevel++, nMask <<= 1) {
            if ((m_dwFlags & nMask) == 0) {
                continue;
            }

            CGameSpriteSpellList* pList = pSpells->GetSpellsAtLevel(nLevel);
            if (pList->m_List.empty()) {
                continue;
            }

            for (UINT nIndex = 0; nIndex < pList->m_List.size(); nIndex++) {
                CGameSpriteSpellListEntry* pEntry = pList->Get(nIndex);
                if (pEntry->field_4 == 0) {
                    continue;
                }

                UINT nID = pEntry->m_nID;

                // NOTE: Inlined CSpellResRefList::Get.
                const CResRef& resRef = g_pBaldurChitin->GetObjectGame()->m_spells.Get(nID);

                pSprite->AddKnownSpell(CAIObjectType::C_SORCERER, nLevel, resRef, m_effectAmount, m_effectAmount, 0);
            }
        }
    }

    if (typeAI.IsClassValid(CAIObjectType::C_WIZARD)) {
        // NOTE: The original copies the grouped spell list through
        // CGameSprite::GetSpells (0x724730) and 0x444D10 and destroys the copy
        // (0x7E78DE); the result is unused, so it is not reproduced.
        DWORD nMask = 1;

        for (UINT nLevel = 0; nLevel < pSpells->m_nHighestLevel; nLevel++, nMask <<= 1) {
            if ((m_dwFlags & nMask) == 0) {
                continue;
            }

            CGameSpriteSpellList* pList = pSpells->GetSpellsAtLevel(nLevel);
            if (pList->m_List.empty()) {
                continue;
            }

            for (UINT nIndex = 0; nIndex < pList->m_List.size(); nIndex++) {
                CGameSpriteSpellListEntry* pEntry = pList->Get(nIndex);
                if (pEntry->field_4 == 0) {
                    continue;
                }

                UINT nID = pEntry->m_nID;

                // NOTE: Inlined CSpellResRefList::Get.
                const CResRef& resRef = g_pBaldurChitin->GetObjectGame()->m_spells.Get(nID);

                pSprite->AddKnownSpell(CAIObjectType::C_WIZARD, nLevel, resRef, m_effectAmount, m_effectAmount, 0);
            }
        }
    }

    // NOTE: The original does not set m_done.

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectStoneToFlesh::CGameEffectStoneToFlesh()
{
    m_effectID = CGAMEEFFECT_STONETOFLESH;
}

// NOTE: Inlined.
CGameEffectStoneToFlesh::CGameEffectStoneToFlesh(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x495600
CGameEffect* CGameEffectStoneToFlesh::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectStoneToFlesh* copy = new CGameEffectStoneToFlesh(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B2070
BOOL CGameEffectStoneToFlesh::ApplyEffect(CGameSprite* pSprite)
{
    if ((pSprite->m_derivedStats.m_generalState & STATE_STONE_DEATH) == 0) {
        m_done = TRUE;
        return TRUE;
    }

    CMessage* message;

    // NOTE: Inlined CGameObject::InControl.
    if (pSprite->InControl()) {
        // A permanent Raise Dead (effect 32, flags 0x2B).
        ITEM_EFFECT* effect = new ITEM_EFFECT;
        memset(effect, 0, sizeof(ITEM_EFFECT));
        effect->effectID = CGAMEEFFECT_RESURRECT;
        effect->probabilityUpper = 100;
        effect->dwFlags = 0x2B;
        effect->durationType = 1;
        effect->duration = 0;

        CGameEffect* pEffect = CGameEffect::DecodeEffect(effect, pSprite->GetPos(), pSprite->GetId(), CPoint(-1, -1));
        message = new CMessageAddEffect(pEffect->Copy(), pSprite->GetId(), pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        if (pEffect != NULL) {
            delete pEffect;
        }
        delete effect;
    }

    pSprite->m_baseStats.m_hitPoints = 1;

    pSprite->m_equipedEffectList.RemoveAllOfType(pSprite, CGAMEEFFECT_PORTRAITICON, pSprite->m_equipedEffectList.m_posCurrent, 71);
    pSprite->m_timedEffectList.RemoveAllOfType(pSprite, CGAMEEFFECT_PORTRAITICON, pSprite->m_timedEffectList.m_posCurrent, 71);

    if ((pSprite->m_baseStats.m_flags & 0x800) != 0) {
        // NOTE: Inlined CGameObject::InControl.
        if (pSprite->InControl()) {
            message = new CMessageSetDialogResRef(CResRef("MULTIJ"), pSprite->GetId(), pSprite->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }

        if (g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(pSprite->GetId()) != -1) {
            message = new CMessageVisibilityMapMove(TRUE, pSprite->GetId(), pSprite->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
    }

    m_done = TRUE;
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectSTR::CGameEffectSTR(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4956B0
CGameEffect* CGameEffectSTR::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectSTR* copy = new CGameEffectSTR(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B2360
BOOL CGameEffectSTR::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_STRBase = min(max(pSprite->GetBaseStats()->m_STRBase + static_cast<SHORT>(m_effectAmount), 1), 40);
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            // NOTE: Uninline.
            AdjustSTR(pSprite, m_effectAmount);
            m_done = FALSE;
        }
        break;
    case 1:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_STRBase = min(max(static_cast<SHORT>(m_effectAmount), 1), 40);
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->GetDerivedStats()->m_nSTR = min(max(static_cast<SHORT>(m_effectAmount), 1), 40);
            m_done = FALSE;
        }
        break;
    case 2:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_STRBase = min(max(pSprite->GetBaseStats()->m_STRBase * static_cast<SHORT>(m_effectAmount) / 100, 1), 40);
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->GetDerivedStats()->m_nSTR = min(max(pSprite->GetBaseStats()->m_STRBase * static_cast<SHORT>(m_effectAmount) / 100, 1), 40);
            m_done = FALSE;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 11439
        UTIL_ASSERT(FALSE);
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectStun::CGameEffectStun(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x495760
CGameEffect* CGameEffectStun::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectStun* copy = new CGameEffectStun(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B2560
BOOL CGameEffectStun::ApplyEffect(CGameSprite* pSprite)
{
    if (m_dwFlags == 2) {
        return sub_4B2680(pSprite);
    }

    if (m_dwFlags == 1) {
        pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_UNSTUN_ON_DAMAGE, TRUE);
    }

    if (m_secondaryType != 0) {
        // NOTE: Uninline.
        DisplayStringRef(pSprite, 1280); // "Stunned"
    }

    // NOTE: Uninline.
    AddPortraitIcon(pSprite, 44);

    pSprite->sub_761650();

    if (m_durationType == 1) {
        pSprite->GetBaseStats()->m_generalState |= STATE_STUNNED;
    }

    pSprite->GetDerivedStats()->m_generalState |= STATE_STUNNED;
    pSprite->GetDerivedStats()->m_generalState |= STATE_HELPLESS;
    pSprite->field_9D15 = 1;

    return TRUE;
}

// 0x4B2680
BOOL CGameEffectStun::sub_4B2680(CGameSprite* pSprite)
{
    INT nHealth = pSprite->GetBaseStats()->m_hitPoints;
    int v1 = 0;

    if (nHealth <= 50) {
        for (int roll = 0; roll < 4; roll++) {
            v1 += CUtil::UtilRandInt(4, -pSprite->m_tempStats.m_nLuck) + 1;
        }
    } else if (nHealth <= 100) {
        for (int roll = 0; roll < 2; roll++) {
            v1 += CUtil::UtilRandInt(4, -pSprite->m_tempStats.m_nLuck) + 1;
        }
    } else if (nHealth <= 150) {
        v1 += CUtil::UtilRandInt(4, -pSprite->m_tempStats.m_nLuck) + 1;
    } else {
        FeedBackImmuneToResource(pSprite, m_sourceRes);
        m_done = TRUE;
        return TRUE;
    }

    CGameObject* pSource;

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_sourceID,
        CGameObjectArray::THREAD_ASYNCH,
        &pSource,
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        return FALSE;
    }

    if (v1 > 0) {
        CGameEffect* pEffect = IcewindMisc::CreateEffectStun(pSource,
            7 * v1,
            static_cast<BYTE>(m_spellLevel),
            0);
        pEffect->m_flags |= 0x2;
        pEffect->m_sourceRes = m_sourceRes;
        pEffect->m_casterLevel = m_casterLevel;
        pSprite->AddEffect(pEffect,
            CGameAIBase::EFFECT_LIST_TIMED,
            TRUE,
            TRUE);
    }

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceID,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectUnstun::CGameEffectUnstun(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x495810
CGameEffect* CGameEffectUnstun::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectUnstun* copy = new CGameEffectUnstun(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B2810
BOOL CGameEffectUnstun::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetBaseStats()->m_generalState &= ~STATE_STUNNED;
    pSprite->GetDerivedStats()->m_generalState &= ~STATE_STUNNED;

    pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_STUN,
        pSprite->GetTimedEffectList()->GetPosCurrent(),
        -1);
    pSprite->GetEquipedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_STUN,
        pSprite->GetEquipedEffectList()->GetPosCurrent(),
        -1);

    pSprite->GetEquipedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_PORTRAITICON,
        pSprite->GetEquipedEffectList()->GetPosCurrent(),
        44);
    pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_PORTRAITICON,
        pSprite->GetTimedEffectList()->GetPosCurrent(),
        44);

    pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_HOLDCREATURE,
        pSprite->GetTimedEffectList()->GetPosCurrent(),
        -1);
    pSprite->GetEquipedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_HOLDCREATURE,
        pSprite->GetEquipedEffectList()->GetPosCurrent(),
        -1);

    pSprite->GetEquipedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_PORTRAITICON,
        pSprite->GetEquipedEffectList()->GetPosCurrent(),
        13);
    pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_PORTRAITICON,
        pSprite->GetTimedEffectList()->GetPosCurrent(),
        13);

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectVisible::CGameEffectVisible(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4958C0
CGameEffect* CGameEffectVisible::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectVisible* copy = new CGameEffectVisible(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B2900
BOOL CGameEffectVisible::ApplyEffect(CGameSprite* pSprite)
{
    // NOTE: Folded with CGameEffectDispelInvisible::ApplyEffect (the same
    // address).
    if ((pSprite->m_tempStats.m_generalState & STATE_NONDETECTION) == 0) {
        pSprite->GetBaseStats()->m_generalState &= ~STATE_INVISIBLE;
        pSprite->GetDerivedStats()->m_generalState &= ~STATE_INVISIBLE;

        pSprite->GetBaseStats()->m_generalState &= ~STATE_IMPROVEDINVISIBILITY;
        pSprite->GetDerivedStats()->m_generalState &= ~STATE_IMPROVEDINVISIBILITY;

        pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
            CGAMEEFFECT_INVISIBLE,
            pSprite->GetTimedEffectList()->GetPosCurrent(),
            -1);
        pSprite->GetEquipedEffectList()->RemoveAllOfType(pSprite,
            CGAMEEFFECT_INVISIBLE,
            pSprite->GetEquipedEffectList()->GetPosCurrent(),
            -1);
    }

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectVocalize::CGameEffectVocalize(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x495970
CGameEffect* CGameEffectVocalize::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectVocalize* copy = new CGameEffectVocalize(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B2A40
BOOL CGameEffectVocalize::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType != 0) {
        // NOTE: Uninline.
        DisplayStringRef(pSprite, 14776); // "Vocalize"
    }

    pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_VOCALIZE);

    // NOTE: Uninline.
    AddPortraitIcon(pSprite, 139);

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectWIS::CGameEffectWIS(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x495A20
CGameEffect* CGameEffectWIS::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectWIS* copy = new CGameEffectWIS(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B2B20
BOOL CGameEffectWIS::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_WISBase = min(max(pSprite->GetBaseStats()->m_WISBase + static_cast<SHORT>(m_effectAmount), 1), 40);
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            // NOTE: Uninline.
            AdjustWIS(pSprite, m_effectAmount);
            m_done = FALSE;
        }
        break;
    case 1:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_WISBase = min(max(static_cast<SHORT>(m_effectAmount), 1), 40);
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->GetDerivedStats()->m_nWIS = min(max(static_cast<SHORT>(m_effectAmount), 1), 40);
            m_done = FALSE;
        }
        break;
    case 2:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_WISBase = min(max(pSprite->GetBaseStats()->m_WISBase * static_cast<SHORT>(m_effectAmount) / 100, 1), 40);
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->GetDerivedStats()->m_nWIS = min(max(pSprite->GetBaseStats()->m_WISBase * static_cast<SHORT>(m_effectAmount) / 100, 1), 40);
            m_done = FALSE;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 11877
        UTIL_ASSERT(FALSE);
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectSingleColorPulseAll::CGameEffectSingleColorPulseAll(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x493E80
CGameEffect* CGameEffectSingleColorPulseAll::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectSingleColorPulseAll* copy = new CGameEffectSingleColorPulseAll(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4A7360
BOOL CGameEffectSingleColorPulseAll::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->m_hasColorEffects = TRUE;

    CPersistantEffectColorEffect* pEffect = new CPersistantEffectColorEffect();
    pEffect->m_effectFinalRed = GetRValue(m_effectAmount >> 8);
    pEffect->m_effectFinalGreen = GetGValue(m_effectAmount >> 8);
    pEffect->m_effectFinalBlue = GetBValue(m_effectAmount >> 8);
    pEffect->m_effectDegreeRed = 0;
    pEffect->m_effectDegreeGreen = 0;
    pEffect->m_effectDegreeBlue = 0;
    pEffect->m_undo = TRUE;
    pEffect->m_colorEffect = 1;
    pEffect->m_period = (m_dwFlags >> 16) & 0xFFFF;
    pSprite->m_persistantEffects.AddTail(pEffect);

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectColorTintSolid::CGameEffectColorTintSolid(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x493D20
CGameEffect* CGameEffectColorTintSolid::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectColorTintSolid* copy = new CGameEffectColorTintSolid(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4A7000
BOOL CGameEffectColorTintSolid::ApplyEffect(CGameSprite* pSprite)
{
    if (m_slotNum == pSprite->sub_726800()
        && (m_dwFlags & 0xF0) == CGameAnimationType::RANGE_WEAPON) {
        m_dwFlags &= 0xF;
        m_dwFlags |= CGameAnimationType::RANGE_SHIELD;
    }

    pSprite->m_hasColorEffects = TRUE;

    COLORREF tintColor = m_effectAmount >> 8;

    if (m_dwFlags == 255) {
        // FIXME: Odd selection of type, should be `BYTE`.
        //
        // 0x8A8EE0
        static int ranges[] = {
            0x00,
            0x01,
            0x02,
            0x03,
            0x04,
            0x05,
            0x06,
            0x14,
            0x15,
            0x21,
            0x22,
            0x24,
            0x25,
            0x31,
            0x32,
            0x34,
            0x35,
            -1,
        };

        int* range = ranges;
        while (*range != -1) {
            // NOTE: Uninline.
            pSprite->GetAnimation()->SetColorEffect(0, static_cast<BYTE>(*range), tintColor, 1);

            CColorEffect* pColorEffect = new CColorEffect();
            pColorEffect->m_effectType = 0;
            pColorEffect->m_range = static_cast<BYTE>(*range);
            pColorEffect->m_tintColor = tintColor;
            pColorEffect->m_periodLength = 1;
            pSprite->GetDerivedStats()->m_appliedColorEffects.AddTail(pColorEffect);

            range++;
        }
    } else {
        // NOTE: Uninline.
        pSprite->GetAnimation()->SetColorEffect(0, static_cast<BYTE>(m_dwFlags), tintColor, 1);

        CColorEffect* pColorEffect = new CColorEffect();
        pColorEffect->m_effectType = 0;
        pColorEffect->m_range = static_cast<BYTE>(m_dwFlags);
        pColorEffect->m_tintColor = tintColor;
        pColorEffect->m_periodLength = 1;
        pSprite->GetDerivedStats()->m_appliedColorEffects.AddTail(pColorEffect);
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectColorLightSolid::CGameEffectColorLightSolid(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x493DD0
CGameEffect* CGameEffectColorLightSolid::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectColorLightSolid* copy = new CGameEffectColorLightSolid(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4A71B0
BOOL CGameEffectColorLightSolid::ApplyEffect(CGameSprite* pSprite)
{
    if (m_slotNum == pSprite->sub_726800()
        && (m_dwFlags & 0xF0) == CGameAnimationType::RANGE_WEAPON) {
        m_dwFlags &= 0xF;
        m_dwFlags |= CGameAnimationType::RANGE_SHIELD;
    }

    pSprite->m_hasColorEffects = TRUE;

    COLORREF tintColor = m_effectAmount >> 8;

    if (m_dwFlags == 255) {
        // FIXME: Odd selection of type, should be `BYTE`.
        //
        // 0x8A8F28
        static int ranges[] = {
            0x00,
            0x01,
            0x02,
            0x03,
            0x04,
            0x05,
            0x06,
            0x14,
            0x15,
            0x21,
            0x22,
            0x24,
            0x25,
            0x31,
            0x32,
            0x34,
            0x35,
            -1,
        };

        int* range = ranges;
        while (*range != -1) {
            // NOTE: Uninline.
            pSprite->GetAnimation()->SetColorEffect(2, static_cast<BYTE>(*range), tintColor, 1);

            CColorEffect* pColorEffect = new CColorEffect();
            pColorEffect->m_effectType = 2;
            pColorEffect->m_range = static_cast<BYTE>(*range);
            pColorEffect->m_tintColor = tintColor;
            pColorEffect->m_periodLength = 1;
            pSprite->GetDerivedStats()->m_appliedColorEffects.AddTail(pColorEffect);

            range++;
        }
    } else {
        // NOTE: Uninline.
        pSprite->GetAnimation()->SetColorEffect(2, static_cast<BYTE>(m_dwFlags), tintColor, 1);

        CColorEffect* pColorEffect = new CColorEffect();
        pColorEffect->m_effectType = 2;
        pColorEffect->m_range = static_cast<BYTE>(m_dwFlags);
        pColorEffect->m_tintColor = tintColor;
        pColorEffect->m_periodLength = 1;
        pSprite->GetDerivedStats()->m_appliedColorEffects.AddTail(pColorEffect);
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectAnimationChange::CGameEffectAnimationChange(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
    field_18C = -1;
}

// 0x495AD0
CGameEffect* CGameEffectAnimationChange::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectAnimationChange* copy = new CGameEffectAnimationChange(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    copy->field_18C = field_18C;
    return copy;
}

// 0x4B31D0
BOOL CGameEffectAnimationChange::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->field_5640 = field_188 == 0;

    if (m_durationType == 1) {
        pSprite->m_baseStats.m_animationType = static_cast<SHORT>(m_effectAmount);
        m_done = TRUE;
    } else {
        m_done = FALSE;
    }

    if (pSprite->m_pPath != NULL) {
        CMessage* message = new CMessageDropPath(pSprite->GetId(), pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    pSprite->UnequipAll(TRUE);

    // NOTE: Inlined CGameAnimation::GetListType (ObjAnimation.h line 2086)
    // and GetPersonalSpace (line 2093).
    if (pSprite->GetArea() != NULL) {
        if (pSprite->GetAnimation()->GetListType() != CGameObject::LIST_FLIGHT) {
            pSprite->GetArea()->m_search.RemoveObject(CPoint(pSprite->GetPos().x / CPathSearch::GRID_SQUARE_SIZEX,
                                                          pSprite->GetPos().y / CPathSearch::GRID_SQUARE_SIZEY),
                pSprite->GetAIType().m_nEnemyAlly,
                pSprite->GetAnimation()->GetPersonalSpace(),
                pSprite->field_54A8,
                pSprite->field_7430);
        }
    }

    // The animation to go back to.
    if (field_18C == -1) {
        field_18C = pSprite->GetAnimation()->m_animation->m_animationID;
    }

    // NOTE: Inlined CGameAnimation::SetAnimationType.
    pSprite->GetAnimation()->SetAnimationType(static_cast<USHORT>(m_effectAmount),
        pSprite->GetBaseStats()->m_colors,
        pSprite->GetDirection());

    pSprite->sub_7204C0();

    if (pSprite->GetArea() != NULL) {
        if (pSprite->GetAnimation()->GetListType() != CGameObject::LIST_FLIGHT) {
            pSprite->GetArea()->m_search.AddObject(CPoint(pSprite->GetPos().x / CPathSearch::GRID_SQUARE_SIZEX,
                                                       pSprite->GetPos().y / CPathSearch::GRID_SQUARE_SIZEY),
                pSprite->GetAIType().m_nEnemyAlly,
                pSprite->GetAnimation()->GetPersonalSpace(),
                pSprite->field_54A8,
                pSprite->field_7430);
        }
    }

    pSprite->EquipAll(TRUE);
    pSprite->JumpToPoint(pSprite->GetPos(), TRUE);
    pSprite->SetIdleSequence();

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectBaseAttackBonus::CGameEffectBaseAttackBonus(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x495BB0
CGameEffect* CGameEffectBaseAttackBonus::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectBaseAttackBonus* copy = new CGameEffectBaseAttackBonus(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B2D20
BOOL CGameEffectBaseAttackBonus::ApplyEffect(CGameSprite* pSprite)
{
    SHORT nValue;

    switch (m_dwFlags) {
    case 0:
        if (m_durationType != 1) {
            pSprite->m_derivedStats.m_nTHAC0 += static_cast<SHORT>(m_effectAmount);
            m_done = FALSE;
            return TRUE;
        }
        nValue = static_cast<SHORT>(m_effectAmount) + static_cast<signed char>(pSprite->m_baseStats.m_attackBase);
        if (nValue < 1) {
            nValue = 1;
        } else if (nValue > 40) {
            nValue = 40;
        }
        break;
    case 1:
        nValue = static_cast<SHORT>(m_effectAmount);
        if (m_durationType != 1) {
            if (nValue > 0) {
                if (nValue > 40) {
                    nValue = 40;
                }
                pSprite->m_derivedStats.m_nTHAC0 = nValue;
            } else {
                pSprite->m_derivedStats.m_nTHAC0 = 1;
            }
            m_done = FALSE;
            return TRUE;
        }
        if (nValue < 1) {
            nValue = 1;
        } else if (nValue > 40) {
            nValue = 40;
        }
        break;
    case 2:
        if (m_durationType != 1) {
            nValue = static_cast<SHORT>(static_cast<signed char>(pSprite->m_baseStats.m_attackBase) * static_cast<SHORT>(m_effectAmount) / 100);
            if (nValue > 0) {
                if (nValue > 40) {
                    nValue = 40;
                }
                pSprite->m_derivedStats.m_nTHAC0 = nValue;
            } else {
                pSprite->m_derivedStats.m_nTHAC0 = 1;
            }
            m_done = FALSE;
            return TRUE;
        }
        nValue = static_cast<SHORT>(static_cast<SHORT>(m_effectAmount) * static_cast<signed char>(pSprite->m_baseStats.m_attackBase) / 100);
        if (nValue < 1) {
            nValue = 1;
        } else if (nValue > 40) {
            nValue = 40;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 11968
        UTIL_ASSERT(FALSE);
        return TRUE;
    }

    pSprite->m_baseStats.m_attackBase = static_cast<BYTE>(nValue);
    m_forceRepass = TRUE;
    m_done = TRUE;
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectSlay::CGameEffectSlay(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x495C60
CGameEffect* CGameEffectSlay::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectSlay* copy = new CGameEffectSlay(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B2F10
BOOL CGameEffectSlay::ApplyEffect(CGameSprite* pSprite)
{
    if (g_pBaldurChitin->GetObjectGame()->m_gameSave.m_cutScene == TRUE) {
        m_done = TRUE;
        return TRUE;
    }

    if ((pSprite->m_baseStats.m_flags & 0x20000) != 0) {
        m_done = TRUE;
        return TRUE;
    }

    CAIObjectType typeAI;
    typeAI.Set(pSprite->GetAIType());

    // NOTE: Inlined CGameEffectDeath().
    CGameEffect* pDeath = new CGameEffectDeath();
    pDeath->m_dwFlags = 4;
    pDeath->m_source = m_source;
    pDeath->m_sourceID = m_sourceID;
    pDeath->m_casterLevel = m_casterLevel;

    // Kills when the sprite's object type has the amount in the field the
    // mode names (an amount of 0 matches any). NOTE: The original leaks the
    // death effect when nothing matches.
    BYTE nAlignment;
    switch (m_dwFlags) {
    case 2:
        if (typeAI.m_nEnemyAlly == static_cast<DWORD>(m_effectAmount) || m_effectAmount == 0) {
            pSprite->AddEffect(pDeath, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);
        }
        break;
    case 3:
        if (typeAI.m_nGeneral == static_cast<DWORD>(m_effectAmount) || m_effectAmount == 0) {
            pSprite->AddEffect(pDeath, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);
        }
        break;
    case 4:
        if (typeAI.m_nRace == static_cast<DWORD>(m_effectAmount) || m_effectAmount == 0) {
            pSprite->AddEffect(pDeath, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);
        }
        break;
    case 5:
        if ((typeAI.m_nClassMask & m_effectAmount) != 0 || m_effectAmount == 0) {
            pSprite->AddEffect(pDeath, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);
        }
        break;
    case 6:
        if (typeAI.m_nSpecific == static_cast<DWORD>(m_effectAmount) || m_effectAmount == 0) {
            pSprite->AddEffect(pDeath, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);
        }
        break;
    case 7:
        if (typeAI.m_nGender == static_cast<DWORD>(m_effectAmount) || m_effectAmount == 0) {
            pSprite->AddEffect(pDeath, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);
        }
        break;
    case 8:
        nAlignment = typeAI.m_nAlignment;
        if (nAlignment == static_cast<DWORD>(m_effectAmount) || m_effectAmount == 0) {
            pSprite->AddEffect(pDeath, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);
        }

        // An amount of 1, 2, 3 names the good-evil axis, 0x10, 0x20, 0x30
        // the law-chaos axis. NOTE: The original can add the same death a
        // second time here; for the alignments the game uses the exact match
        // above never meets one of these amounts.
        if ((m_effectAmount == 1 && (nAlignment & 0xF) == 1)
            || (m_effectAmount == 2 && (nAlignment & 0xF) == 2)
            || (m_effectAmount == 3 && (nAlignment & 0xF) == 3)
            || (m_effectAmount == 0x10 && (nAlignment & 0xF0) == 0x10)
            || (m_effectAmount == 0x20 && (nAlignment & 0xF0) == 0x20)
            || (m_effectAmount == 0x30 && (nAlignment & 0xF0) == 0x30)) {
            pSprite->AddEffect(pDeath, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);
        }
        break;
    }

    m_done = TRUE;
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectAlignmentReversal::CGameEffectAlignmentReversal(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x495D10
CGameEffect* CGameEffectAlignmentReversal::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectAlignmentReversal* copy = new CGameEffectAlignmentReversal(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B3840
BOOL CGameEffectAlignmentReversal::ApplyEffect(CGameSprite* pSprite)
{
    CAIObjectType typeAI(pSprite->GetAIType());
    CAIObjectType liveTypeAI(pSprite->GetLiveAIType());

    // FIXME: Probably wrong (using live as start).
    CAIObjectType startTypeAI(pSprite->GetLiveAIType());

    if (m_durationType == 1) {
        // NOTE: Uninline.
        startTypeAI.SetAlignment(Reverse(startTypeAI.GetAlignment()));
        pSprite->m_startTypeAI = startTypeAI;
        m_done = TRUE;
    } else {
        m_done = FALSE;
    }

    // NOTE: Uninline.
    typeAI.SetAlignment(Reverse(typeAI.GetAlignment()));
    pSprite->SetAIType(typeAI, FALSE, FALSE);

    // NOTE: Uninline.
    liveTypeAI.SetAlignment(Reverse(liveTypeAI.GetAlignment()));
    pSprite->m_liveTypeAI = liveTypeAI;

    return TRUE;
}

// NOTE: Inlined.
BYTE CGameEffectAlignmentReversal::Reverse(BYTE alignment)
{
    BYTE newAlign;

    switch (alignment) {
    case 0:
        newAlign = 0;
        break;
    case CAIOBJECTTYPE_LAWFUL_GOOD:
        newAlign = CAIOBJECTTYPE_CHAOTIC_EVIL;
        break;
    case CAIOBJECTTYPE_LAWFUL_NEUTRAL:
        newAlign = CAIOBJECTTYPE_CHAOTIC_NEUTRAL;
        break;
    case CAIOBJECTTYPE_LAWFUL_EVIL:
        newAlign = CAIOBJECTTYPE_CHAOTIC_GOOD;
        break;
    case CAIOBJECTTYPE_NEUTRAL_GOOD:
        newAlign = CAIOBJECTTYPE_NEUTRAL_EVIL;
        break;
    case CAIOBJECTTYPE_NEUTRAL:
        newAlign = CAIOBJECTTYPE_NEUTRAL;
        break;
    case CAIOBJECTTYPE_NEUTRAL_EVIL:
        newAlign = CAIOBJECTTYPE_NEUTRAL_GOOD;
        break;
    case CAIOBJECTTYPE_CHAOTIC_GOOD:
        newAlign = CAIOBJECTTYPE_LAWFUL_EVIL;
        break;
    case CAIOBJECTTYPE_CHAOTIC_NEUTRAL:
        newAlign = CAIOBJECTTYPE_LAWFUL_NEUTRAL;
        break;
    case CAIOBJECTTYPE_CHAOTIC_EVIL:
        newAlign = CAIOBJECTTYPE_LAWFUL_GOOD;
        break;
    }

    return newAlign;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectAlignmentChange::CGameEffectAlignmentChange(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x495DC0
CGameEffect* CGameEffectAlignmentChange::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectAlignmentChange* copy = new CGameEffectAlignmentChange(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B3BA0
BOOL CGameEffectAlignmentChange::ApplyEffect(CGameSprite* pSprite)
{
    CAIObjectType typeAI(pSprite->GetAIType());
    CAIObjectType liveTypeAI(pSprite->GetLiveAIType());

    // FIXME: Probably wrong (using live as start).
    CAIObjectType startTypeAI(pSprite->GetLiveAIType());

    if (m_durationType == 1) {
        startTypeAI.SetAlignment(static_cast<BYTE>(m_dwFlags));
        pSprite->m_startTypeAI = startTypeAI;
        m_done = TRUE;
    } else {
        m_done = FALSE;
    }

    typeAI.SetAlignment(static_cast<BYTE>(m_dwFlags));
    pSprite->SetAIType(typeAI, FALSE, FALSE);

    liveTypeAI.SetAlignment(static_cast<BYTE>(m_dwFlags));
    pSprite->m_liveTypeAI = liveTypeAI;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectDispelEffects::CGameEffectDispelEffects(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x495E70
CGameEffect* CGameEffectDispelEffects::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectDispelEffects* copy = new CGameEffectDispelEffects(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B3D00
BOOL CGameEffectDispelEffects::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->RemoveSpecialAbility(CResRef(CString(RESREF_EYE_OF_THE_MIND)));
    pSprite->RemoveSpecialAbility(CResRef(CString(RESREF_EYE_OF_THE_SWORD)));
    pSprite->RemoveSpecialAbility(CResRef(CString(RESREF_EYE_OF_THE_MAGE)));
    pSprite->RemoveSpecialAbility(CResRef(CString(RESREF_EYE_OF_VENOM)));
    pSprite->RemoveSpecialAbility(CResRef(CString(RESREF_EYE_OF_THE_SPIRIT)));
    pSprite->RemoveSpecialAbility(CResRef(CString(RESREF_EYE_OF_FORTITUDE)));
    pSprite->RemoveSpecialAbility(CResRef(CString(RESREF_EYE_OF_STONE)));

    if (m_dwFlags != 0) {
        pSprite->GetTimedEffectList()->RemoveAllEffectsIgnoreMoreThenPermanent(pSprite->GetTimedEffectList()->GetPosCurrent(),
            TRUE,
            TRUE,
            rand() % 100,
            static_cast<BYTE>(m_casterLevel));
    } else {
        pSprite->GetTimedEffectList()->RemoveAllEffectsIgnoreMoreThenPermanent(pSprite->GetTimedEffectList()->GetPosCurrent(),
            TRUE,
            FALSE,
            0,
            0);
    }

    pSprite->UnequipAll(TRUE);
    sub_4B8730(pSprite, 42);
    pSprite->EquipAll(TRUE);
    pSprite->m_hasColorEffects = TRUE;
    pSprite->m_hasColorRangeEffects = TRUE;
    pSprite->field_5640 = 1;

    m_forceRepass = TRUE;
    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectSkillStealth::CGameEffectSkillStealth(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x495F20
CGameEffect* CGameEffectSkillStealth::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectSkillStealth* copy = new CGameEffectSkillStealth(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B3F60
BOOL CGameEffectSkillStealth::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_MOVE_SILENTLY] += static_cast<BYTE>(m_effectAmount);
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else if (m_durationType == 9) {
            pSprite->m_bonusStats.m_nMoveSilentlyMTPBonus += m_effectAmount;
            m_done = FALSE;
        } else {
            pSprite->m_bonusStats.m_nSkills[CGAMESPRITE_SKILL_MOVE_SILENTLY] += static_cast<BYTE>(m_effectAmount);
            m_done = FALSE;
        }
        break;
    case 1:
        if (m_durationType == 1) {
            // FIXME: Why nothing changed?
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->GetDerivedStats()->m_nSkills[CGAMESPRITE_SKILL_MOVE_SILENTLY] = static_cast<BYTE>(m_effectAmount);
            m_done = FALSE;
        }
        break;
    case 2:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_MOVE_SILENTLY] = pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_MOVE_SILENTLY] * static_cast<SHORT>(m_effectAmount) / 100;
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->GetDerivedStats()->m_nSkills[CGAMESPRITE_SKILL_MOVE_SILENTLY] = pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_MOVE_SILENTLY] * static_cast<SHORT>(m_effectAmount) / 100;
            m_done = FALSE;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 12565
        UTIL_ASSERT(FALSE);
    }

    // NOTE: Uninline.
    if (pSprite->InControl()) {
        pSprite->m_bSendSpriteUpdate = TRUE;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined
CGameEffectHideInShadows::CGameEffectHideInShadows(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x495FD0
CGameEffect* CGameEffectHideInShadows::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectHideInShadows* copy = new CGameEffectHideInShadows(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B4080
BOOL CGameEffectHideInShadows::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        if (m_durationType == 9) {
            pSprite->m_bonusStats.m_nHideInShadowsMTPBonus += m_effectAmount;
        } else {
            pSprite->m_bonusStats.m_nSkills[CGAMESPRITE_SKILL_HIDE] += static_cast<BYTE>(m_effectAmount);
        }
        break;
    case 1:
        pSprite->GetDerivedStats()->m_nSkills[CGAMESPRITE_SKILL_HIDE] = static_cast<BYTE>(m_effectAmount);
        break;
    case 2:
        pSprite->GetDerivedStats()->m_nSkills[CGAMESPRITE_SKILL_HIDE] = pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_HIDE] * static_cast<SHORT>(m_effectAmount) / 100;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 12617
        UTIL_ASSERT(FALSE);
    }

    // NOTE: Uninline.
    if (pSprite->InControl()) {
        pSprite->m_bSendSpriteUpdate = TRUE;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectCastingFailure::CGameEffectCastingFailure(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x496080
CGameEffect* CGameEffectCastingFailure::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectCastingFailure* copy = new CGameEffectCastingFailure(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B4140
BOOL CGameEffectCastingFailure::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        pSprite->GetDerivedStats()->m_nSpellFailureArcane += static_cast<SHORT>(m_effectAmount);
        break;
    case 1:
        pSprite->GetDerivedStats()->m_nSpellFailureDivine += static_cast<SHORT>(m_effectAmount);
        break;
    case 2:
        pSprite->GetDerivedStats()->m_nSpellFailureArcane += static_cast<SHORT>(m_effectAmount);
        pSprite->GetDerivedStats()->m_nSpellFailureDivine += static_cast<SHORT>(m_effectAmount);
        break;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectSkillAlchemy::CGameEffectSkillAlchemy(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x494710
CGameEffect* CGameEffectSkillAlchemy::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectSkillAlchemy* copy = new CGameEffectSkillAlchemy(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4AF0B0
BOOL CGameEffectSkillAlchemy::ApplyEffect(CGameSprite* pSprite)
{
    // NOTE: Opcode 61 works on Alchemy, skill 0; the same code as Lore
    // (0x4AEF50) on another skill.
    signed char nValue;
    INT nProduct;

    switch (m_dwFlags) {
    case 0:
        if (m_durationType != 1) {
            pSprite->m_bonusStats.m_nSkills[CGAMESPRITE_SKILL_ALCHEMY] += static_cast<signed char>(m_effectAmount);
            m_done = FALSE;
            break;
        }
        nValue = static_cast<signed char>(pSprite->m_baseStats.m_skills[CGAMESPRITE_SKILL_ALCHEMY] + static_cast<signed char>(m_effectAmount));
        pSprite->m_baseStats.m_skills[CGAMESPRITE_SKILL_ALCHEMY] = nValue;
        if (nValue > 100) {
            pSprite->m_baseStats.m_skills[CGAMESPRITE_SKILL_ALCHEMY] = 100;
        }
        if (static_cast<signed char>(pSprite->m_baseStats.m_skills[CGAMESPRITE_SKILL_ALCHEMY]) < 0) {
            pSprite->m_baseStats.m_skills[CGAMESPRITE_SKILL_ALCHEMY] = 0;
        }
        m_forceRepass = TRUE;
        m_done = TRUE;
        break;
    case 1:
    case 2:
        if (m_dwFlags == 1) {
            nValue = static_cast<signed char>(m_effectAmount);
        } else if (m_durationType != 1) {
            nProduct = static_cast<signed char>(pSprite->m_baseStats.m_skills[CGAMESPRITE_SKILL_ALCHEMY]) * static_cast<SHORT>(m_effectAmount);
            nValue = static_cast<signed char>(nProduct / 100);
        } else {
            nProduct = static_cast<SHORT>(m_effectAmount) * static_cast<signed char>(pSprite->m_baseStats.m_skills[CGAMESPRITE_SKILL_ALCHEMY]);
            nValue = static_cast<signed char>(nProduct / 100);
        }

        if (m_durationType != 1) {
            pSprite->m_derivedStats.m_nSkills[CGAMESPRITE_SKILL_ALCHEMY] = nValue;
            if (nValue > 100) {
                pSprite->m_derivedStats.m_nSkills[CGAMESPRITE_SKILL_ALCHEMY] = 100;
            }
            if (pSprite->m_derivedStats.m_nSkills[CGAMESPRITE_SKILL_ALCHEMY] < 0) {
                pSprite->m_derivedStats.m_nSkills[CGAMESPRITE_SKILL_ALCHEMY] = 0;
            }
            m_done = FALSE;
            break;
        }

        pSprite->m_baseStats.m_skills[CGAMESPRITE_SKILL_ALCHEMY] = nValue;
        if (nValue > 100) {
            pSprite->m_baseStats.m_skills[CGAMESPRITE_SKILL_ALCHEMY] = 100;
        }
        if (static_cast<signed char>(pSprite->m_baseStats.m_skills[CGAMESPRITE_SKILL_ALCHEMY]) < 0) {
            pSprite->m_baseStats.m_skills[CGAMESPRITE_SKILL_ALCHEMY] = 0;
        }
        m_forceRepass = TRUE;
        m_done = TRUE;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 9062
        UTIL_ASSERT(FALSE);
        break;
    }

    // NOTE: Inlined CGameObject::InControl.
    if (pSprite->InControl()) {
        pSprite->m_bSendSpriteUpdate = TRUE;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectSpellMemoizationCleric::CGameEffectSpellMemoizationCleric(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x495550
CGameEffect* CGameEffectSpellMemoizationCleric::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectSpellMemoizationCleric* copy = new CGameEffectSpellMemoizationCleric(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B41A0
BOOL CGameEffectSpellMemoizationCleric::ApplyEffect(CGameSprite* pSprite)
{
    const CAIObjectType& typeAI = pSprite->GetAIType();

    // NOTE: The original calls CDerivedStats::sub_447B00 once, with the
    // CLERIC class, and reuses the returned list for every class walk below.
    CGameSpriteGroupedSpellList* pSpells = pSprite->m_derivedStats.sub_447B00(CAIObjectType::C_CLERIC);

    if (typeAI.IsClassValid(CAIObjectType::C_CLERIC)) {
        DWORD nMask = 1;

        for (UINT nLevel = 0; nLevel < pSpells->m_nHighestLevel; nLevel++, nMask <<= 1) {
            if ((m_dwFlags & nMask) == 0) {
                continue;
            }

            CGameSpriteSpellList* pList = pSpells->GetSpellsAtLevel(nLevel);
            if (pList->m_List.empty()) {
                continue;
            }

            for (UINT nIndex = 0; nIndex < pList->m_List.size(); nIndex++) {
                CGameSpriteSpellListEntry* pEntry = pList->Get(nIndex);
                if (pEntry->field_4 == 0) {
                    continue;
                }

                UINT nID = pEntry->m_nID;

                // NOTE: Inlined CSpellResRefList::Get.
                const CResRef& resRef = g_pBaldurChitin->GetObjectGame()->m_spells.Get(nID);

                pSprite->AddKnownSpell(CAIObjectType::C_CLERIC, nLevel, resRef, m_effectAmount, m_effectAmount, 0);
            }
        }
    }

    if (typeAI.IsClassValid(CAIObjectType::C_DRUID)) {
        // NOTE: The original copies the grouped spell list through
        // CGameSprite::GetSpells (0x724730) and 0x444D10 and destroys the copy
        // (0x7E78DE); the result is unused, so it is not reproduced.
        DWORD nMask = 1;

        for (UINT nLevel = 0; nLevel < pSpells->m_nHighestLevel; nLevel++, nMask <<= 1) {
            if ((m_dwFlags & nMask) == 0) {
                continue;
            }

            CGameSpriteSpellList* pList = pSpells->GetSpellsAtLevel(nLevel);
            if (pList->m_List.empty()) {
                continue;
            }

            for (UINT nIndex = 0; nIndex < pList->m_List.size(); nIndex++) {
                CGameSpriteSpellListEntry* pEntry = pList->Get(nIndex);
                if (pEntry->field_4 == 0) {
                    continue;
                }

                UINT nID = pEntry->m_nID;

                // NOTE: Inlined CSpellResRefList::Get.
                const CResRef& resRef = g_pBaldurChitin->GetObjectGame()->m_spells.Get(nID);

                pSprite->AddKnownSpell(CAIObjectType::C_DRUID, nLevel, resRef, m_effectAmount, m_effectAmount, 0);
            }
        }
    }

    if (typeAI.IsClassValid(CAIObjectType::C_PALADIN)) {
        // NOTE: The original copies the grouped spell list through
        // CGameSprite::GetSpells (0x724730) and 0x444D10 and destroys the copy
        // (0x7E78DE); the result is unused, so it is not reproduced.
        DWORD nMask = 1;

        for (UINT nLevel = 0; nLevel < pSpells->m_nHighestLevel; nLevel++, nMask <<= 1) {
            if ((m_dwFlags & nMask) == 0) {
                continue;
            }

            CGameSpriteSpellList* pList = pSpells->GetSpellsAtLevel(nLevel);
            if (pList->m_List.empty()) {
                continue;
            }

            for (UINT nIndex = 0; nIndex < pList->m_List.size(); nIndex++) {
                CGameSpriteSpellListEntry* pEntry = pList->Get(nIndex);
                if (pEntry->field_4 == 0) {
                    continue;
                }

                UINT nID = pEntry->m_nID;

                // NOTE: Inlined CSpellResRefList::Get.
                const CResRef& resRef = g_pBaldurChitin->GetObjectGame()->m_spells.Get(nID);

                pSprite->AddKnownSpell(CAIObjectType::C_PALADIN, nLevel, resRef, m_effectAmount, m_effectAmount, 0);
            }
        }
    }

    if (typeAI.IsClassValid(CAIObjectType::C_RANGER)) {
        // NOTE: The original copies the grouped spell list through
        // CGameSprite::GetSpells (0x724730) and 0x444D10 and destroys the copy
        // (0x7E78DE); the result is unused, so it is not reproduced.
        DWORD nMask = 1;

        for (UINT nLevel = 0; nLevel < pSpells->m_nHighestLevel; nLevel++, nMask <<= 1) {
            if ((m_dwFlags & nMask) == 0) {
                continue;
            }

            CGameSpriteSpellList* pList = pSpells->GetSpellsAtLevel(nLevel);
            if (pList->m_List.empty()) {
                continue;
            }

            for (UINT nIndex = 0; nIndex < pList->m_List.size(); nIndex++) {
                CGameSpriteSpellListEntry* pEntry = pList->Get(nIndex);
                if (pEntry->field_4 == 0) {
                    continue;
                }

                UINT nID = pEntry->m_nID;

                // NOTE: Inlined CSpellResRefList::Get.
                const CResRef& resRef = g_pBaldurChitin->GetObjectGame()->m_spells.Get(nID);

                pSprite->AddKnownSpell(CAIObjectType::C_RANGER, nLevel, resRef, m_effectAmount, m_effectAmount, 0);
            }
        }
    }

    // NOTE: The original does not set m_done.

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectInfravision::CGameEffectInfravision(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x496130
CGameEffect* CGameEffectInfravision::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectInfravision* copy = new CGameEffectInfravision(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B4B00
BOOL CGameEffectInfravision::ApplyEffect(CGameSprite* pSprite)
{
    if (m_durationType == 1) {
        pSprite->GetBaseStats()->m_generalState |= STATE_INFRAVISION;
        pSprite->GetDerivedStats()->m_generalState |= STATE_INFRAVISION;
        m_done = TRUE;
    } else {
        pSprite->GetDerivedStats()->m_generalState |= STATE_INFRAVISION;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectInfravisionOff::CGameEffectInfravisionOff(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4961E0
CGameEffect* CGameEffectInfravisionOff::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectInfravisionOff* copy = new CGameEffectInfravisionOff(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B4B40
BOOL CGameEffectInfravisionOff::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetBaseStats()->m_generalState &= ~STATE_INFRAVISION;
    pSprite->GetDerivedStats()->m_generalState &= ~STATE_INFRAVISION;

    pSprite->GetEquipedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_INFRAVISION,
        pSprite->GetEquipedEffectList()->GetPosCurrent(),
        -1);
    pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_INFRAVISION,
        pSprite->GetTimedEffectList()->GetPosCurrent(),
        -1);

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectBlur::CGameEffectBlur(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x496290
CGameEffect* CGameEffectBlur::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectBlur* copy = new CGameEffectBlur(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4A5DC0
BOOL CGameEffectBlur::ApplyEffect(CGameSprite* pSprite)
{
    if ((pSprite->GetDerivedStats()->m_generalState & STATE_BLUR) == 0) {
        pSprite->GetDerivedStats()->m_generalState |= STATE_BLUR;
        pSprite->GetDerivedStats()->m_visualEffects[IWD_VFX_BLUR] = true;

        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 109);
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectTranslucent::CGameEffectTranslucent(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x496600
CGameEffect* CGameEffectTranslucent::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectTranslucent* copy = new CGameEffectTranslucent(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BBB90
BOOL CGameEffectTranslucent::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        pSprite->GetDerivedStats()->m_nTranslucent = static_cast<BYTE>(m_effectAmount);
        break;
    case 1:
        pSprite->GetBaseStats()->m_generalState |= STATE_FADE_IN;
        pSprite->GetBaseStats()->m_fadeSpeed = static_cast<BYTE>(m_effectAmount);
        m_done = TRUE;
        break;
    case 2:
        pSprite->GetBaseStats()->m_generalState |= STATE_FADE_OUT;
        pSprite->GetBaseStats()->m_fadeSpeed = static_cast<BYTE>(m_effectAmount);
        m_done = TRUE;
        break;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectSkillUnsummon::CGameEffectSkillUnsummon(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4966B0
CGameEffect* CGameEffectSkillUnsummon::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectSkillUnsummon* copy = new CGameEffectSkillUnsummon(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B4BD0
BOOL CGameEffectSkillUnsummon::ApplyEffect(CGameSprite* pSprite)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (pGame->GetCharacterPortraitNum(pSprite->GetId()) == -1) {
        // 0x586DC0
        Icewind586B70::Instance()->sub_586DC0(pSprite);

        CMessage* message = new CMessage89(pSprite->GetId(), pSprite->GetId(), pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

        pSprite->m_removeFromArea = TRUE;
    }

    m_done = TRUE;
    return TRUE;
}

// 0x4B4BB0
void CGameEffectSkillUnsummon::DisplayString(CGameSprite* pSprite)
{
    if (m_effectAmount == 1) {
        CGameEffect::DisplayString(pSprite);
    }
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectNonDetection::CGameEffectNonDetection(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x496760
CGameEffect* CGameEffectNonDetection::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectNonDetection* copy = new CGameEffectNonDetection(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B4C60
BOOL CGameEffectNonDetection::ApplyEffect(CGameSprite* pSprite)
{
    if (m_durationType == 1) {
        pSprite->GetBaseStats()->m_generalState |= STATE_NONDETECTION;
        pSprite->GetDerivedStats()->m_generalState |= STATE_NONDETECTION;
        m_done = TRUE;
    } else {
        pSprite->GetDerivedStats()->m_generalState |= STATE_NONDETECTION;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectEndNonDetection::CGameEffectEndNonDetection(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x496810
CGameEffect* CGameEffectEndNonDetection::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectEndNonDetection* copy = new CGameEffectEndNonDetection(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B4CA0
BOOL CGameEffectEndNonDetection::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetBaseStats()->m_generalState &= ~STATE_NONDETECTION;
    pSprite->GetDerivedStats()->m_generalState &= ~STATE_NONDETECTION;

    pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_NONDETECTION,
        pSprite->GetTimedEffectList()->GetPosCurrent(),
        -1);
    pSprite->GetEquipedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_NONDETECTION,
        pSprite->GetEquipedEffectList()->GetPosCurrent(),
        -1);

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectSexChange::CGameEffectSexChange(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4968C0
CGameEffect* CGameEffectSexChange::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectSexChange* copy = new CGameEffectSexChange(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B4D00
BOOL CGameEffectSexChange::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->field_5640 = 1;

    // Bit 0x10 of the animation is the female one.
    WORD nAnimationType = pSprite->m_baseStats.m_animationType;

    CAIObjectType typeAI;
    typeAI.Set(pSprite->GetAIType());
    CAIObjectType liveTypeAI;
    liveTypeAI.Set(pSprite->m_liveTypeAI);

    // NOTE: The original copies the live object type for the start one as
    // well (0x4B4D98).
    CAIObjectType startTypeAI;
    startTypeAI.Set(pSprite->m_liveTypeAI);

    switch (m_dwFlags) {
    case 0:
        // Swap.
        nAnimationType ^= 0x10;

        if (typeAI.m_nGender == CAIObjectType::SEX_MALE) {
            typeAI.m_nGender = CAIObjectType::SEX_FEMALE;
        } else if (typeAI.m_nGender == CAIObjectType::SEX_FEMALE) {
            typeAI.m_nGender = CAIObjectType::SEX_MALE;
        }

        if (liveTypeAI.m_nGender == CAIObjectType::SEX_MALE) {
            liveTypeAI.m_nGender = CAIObjectType::SEX_FEMALE;
        } else if (liveTypeAI.m_nGender == CAIObjectType::SEX_FEMALE) {
            liveTypeAI.m_nGender = CAIObjectType::SEX_MALE;
        }

        if (m_durationType == 1) {
            // NOTE: The original's second test reads the swapped current
            // gender, not the start type's (0x4B4E8A).
            if (startTypeAI.m_nGender == CAIObjectType::SEX_MALE) {
                startTypeAI.m_nGender = CAIObjectType::SEX_FEMALE;
            } else if (typeAI.m_nGender == CAIObjectType::SEX_FEMALE) {
                startTypeAI.m_nGender = CAIObjectType::SEX_MALE;
            }

            pSprite->m_baseStats.m_animationType = nAnimationType;
            m_done = TRUE;
        }
        break;
    case 1:
        // Set.
        if (m_effectAmount == CAIObjectType::SEX_MALE) {
            nAnimationType &= ~0x10;
        } else if (m_effectAmount == CAIObjectType::SEX_FEMALE) {
            nAnimationType |= 0x10;
        }

        typeAI.m_nGender = static_cast<BYTE>(m_effectAmount);
        liveTypeAI.m_nGender = static_cast<BYTE>(m_effectAmount);

        if (m_durationType == 1) {
            startTypeAI.m_nGender = static_cast<BYTE>(m_effectAmount);
            pSprite->m_baseStats.m_animationType = nAnimationType;
            m_done = TRUE;
        }
        break;
    }

    pSprite->SetAIType(typeAI, FALSE, FALSE);
    pSprite->m_liveTypeAI.Set(liveTypeAI);
    pSprite->m_startTypeAI.Set(startTypeAI);

    pSprite->UnequipAll(TRUE);

    // NOTE: Inlined CGameAnimation::SetAnimationType.
    pSprite->GetAnimation()->SetAnimationType(nAnimationType,
        pSprite->GetBaseStats()->m_colors,
        pSprite->m_nDirection);

    if (pSprite->m_nSequence == CGameSprite::SEQ_DIE) {
        pSprite->SetSequence(CGameSprite::SEQ_READY);
        pSprite->SetSequence(CGameSprite::SEQ_DIE);
    }

    pSprite->EquipAll(TRUE);

    CMessage* message = new CMessageAnimationChange(nAnimationType, pSprite->GetId(), pSprite->GetId());
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectAIChange::CGameEffectAIChange(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x496970
CGameEffect* CGameEffectAIChange::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectAIChange* copy = new CGameEffectAIChange(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B5000
BOOL CGameEffectAIChange::ApplyEffect(CGameSprite* pSprite)
{
    CAIObjectType type(pSprite->GetAIType());
    CAIObjectType liveType(pSprite->m_liveTypeAI);
    CAIObjectType startType(pSprite->m_startTypeAI);

    switch (m_dwFlags) {
    case 0:
        type.SetEnemyAlly(static_cast<BYTE>(m_effectAmount));
        liveType.SetEnemyAlly(static_cast<BYTE>(m_effectAmount));
        if (m_durationType == 1) {
            startType.SetEnemyAlly(static_cast<BYTE>(m_effectAmount));
            m_done = TRUE;
        }
        break;
    case 1:
        type.SetGeneral(static_cast<BYTE>(m_effectAmount));
        liveType.SetGeneral(static_cast<BYTE>(m_effectAmount));
        if (m_durationType == 1) {
            startType.SetGeneral(static_cast<BYTE>(m_effectAmount));
            m_done = TRUE;
        }
        break;
    case 2:
        type.SetRace(static_cast<BYTE>(m_effectAmount));
        liveType.SetRace(static_cast<BYTE>(m_effectAmount));
        if (m_durationType == 1) {
            startType.SetRace(static_cast<BYTE>(m_effectAmount));
            m_done = TRUE;
        }
        break;
    case 3:
        type.SetClass(static_cast<BYTE>(m_effectAmount));
        liveType.SetClass(static_cast<BYTE>(m_effectAmount));
        if (m_durationType == 1) {
            startType.SetClass(static_cast<BYTE>(m_effectAmount));
            m_done = TRUE;
        }
        break;
    case 4:
        type.SetSpecific(static_cast<BYTE>(m_effectAmount));
        liveType.SetSpecific(static_cast<BYTE>(m_effectAmount));
        if (m_durationType == 1) {
            startType.SetSpecific(static_cast<BYTE>(m_effectAmount));
            m_done = TRUE;
        }
        break;
    case 5:
        type.SetGender(static_cast<BYTE>(m_effectAmount));
        liveType.SetGender(static_cast<BYTE>(m_effectAmount));
        if (m_durationType == 1) {
            startType.SetGender(static_cast<BYTE>(m_effectAmount));
            m_done = TRUE;
        }
        break;
    case 6:
        type.SetAlignment(static_cast<BYTE>(m_effectAmount));
        liveType.SetAlignment(static_cast<BYTE>(m_effectAmount));
        if (m_durationType == 1) {
            startType.SetAlignment(static_cast<BYTE>(m_effectAmount));
            m_done = TRUE;
        }
        break;
    }

    pSprite->SetAIType(type, FALSE, FALSE);
    pSprite->m_liveTypeAI.Set(liveType);
    pSprite->m_startTypeAI.Set(startType);

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectDamageMod::CGameEffectDamageMod(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x496A20
CGameEffect* CGameEffectDamageMod::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectDamageMod* copy = new CGameEffectDamageMod(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B5220
BOOL CGameEffectDamageMod::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        pSprite->GetDerivedStats()->m_nDamageBonus += static_cast<SHORT>(m_effectAmount);
        if (pSprite->GetDerivedStats()->m_nDamageBonus > 20) {
            pSprite->GetDerivedStats()->m_nDamageBonus = 20;
        }
        if (pSprite->GetDerivedStats()->m_nDamageBonus < -20) {
            pSprite->GetDerivedStats()->m_nDamageBonus = -20;
        }
        break;
    case 1:
        pSprite->GetDerivedStats()->m_nDamageModFire += static_cast<SHORT>(m_effectAmount);
        break;
    case 2:
        pSprite->GetDerivedStats()->m_nDamageModCold += static_cast<SHORT>(m_effectAmount);
        break;
    case 3:
        pSprite->GetDerivedStats()->m_nDamageModElectricity += static_cast<SHORT>(m_effectAmount);
        break;
    case 4:
        pSprite->GetDerivedStats()->m_nDamageModAcid += static_cast<SHORT>(m_effectAmount);
        break;
    case 5:
        pSprite->GetDerivedStats()->m_nDamageModMagic += static_cast<SHORT>(m_effectAmount);
        break;
    case 6:
        pSprite->GetDerivedStats()->m_nDamageModPoison += static_cast<SHORT>(m_effectAmount);
        break;
    case 7:
        pSprite->GetDerivedStats()->m_nDamageModSlashing += static_cast<SHORT>(m_effectAmount);
        break;
    case 8:
        pSprite->GetDerivedStats()->m_nDamageModPiercing += static_cast<SHORT>(m_effectAmount);
        break;
    case 9:
        pSprite->GetDerivedStats()->m_nDamageModCrushing += static_cast<SHORT>(m_effectAmount);
        break;
    case 10:
        pSprite->GetDerivedStats()->m_nDamageModMissile += static_cast<SHORT>(m_effectAmount);
        break;
    }

    m_done = FALSE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectBlindness::CGameEffectBlindness(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x496AD0
CGameEffect* CGameEffectBlindness::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectBlindness* copy = new CGameEffectBlindness(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B53A0
BOOL CGameEffectBlindness::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType != 0) {
        // NOTE: Uninline.
        DisplayStringRef(pSprite, 14674); // "Blinded"
    }

    if ((pSprite->GetDerivedStats()->m_generalState & STATE_BLIND) == 0) {
        pSprite->GetDerivedStats()->m_generalState |= STATE_BLIND;

        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 8);

        int v1 = -2;
        if (pSprite->sub_763150(CGAMESPRITE_FEAT_BLIND_FIGHT)) {
            v1 = 0;
        }

        pSprite->GetDerivedStats()->field_C += v1;
    }

    return TRUE;
}

// 0x4B5490
void CGameEffectBlindness::OnRemove(CGameSprite* pSprite)
{
    PlaySound(CResRef("EFF_E05"), pSprite);
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectDayBlindness::CGameEffectDayBlindness(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x496B80
CGameEffect* CGameEffectDayBlindness::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectDayBlindness* copy = new CGameEffectDayBlindness(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B54C0
BOOL CGameEffectDayBlindness::ApplyEffect(CGameSprite* pSprite)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
    // __LINE__: 13510
    UTIL_ASSERT(pSprite != NULL);

    if (pSprite->GetArea() != NULL) {
        if (!pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_DAY_BLINDNESS]) {
            pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_DAY_BLINDNESS] = true;
            SHORT nModifier = 0;
            if (pSprite->m_typeAI.m_nRace == CAIOBJECTTYPE_R_ELF
                && pSprite->m_typeAI.m_nSubRace == CAIOBJECTTYPE_SUBRACE_ELF_DROW) {
                nModifier = -1;
            } else if (pSprite->m_typeAI.m_nRace == CAIOBJECTTYPE_R_DWARF
                && pSprite->m_typeAI.m_nSubRace == CAIOBJECTTYPE_SUBRACE_DWARF_GRAY) {
                nModifier = -2;
            }

            pSprite->m_bonusStats.m_nSaveVSFortitude += nModifier;
            pSprite->m_bonusStats.m_nSaveVSReflex += nModifier;
            pSprite->m_bonusStats.m_nSaveVSWill += nModifier;

            for (int skill = 0; skill < CGAMESPRITE_SKILL_NUMSKILLS; skill++) {
                pSprite->m_bonusStats.m_nSkills[skill] += nModifier;
            }

            // NOTE: Uninline.
            if (pSprite->InControl()) {
                pSprite->m_bSendSpriteUpdate = TRUE;
            }
        } else {
            m_done = TRUE;
        }
    }

    return TRUE;
}

// 0x4B5490
void CGameEffectDayBlindness::OnRemove(CGameSprite* pSprite)
{
    PlaySound(CResRef("EFF_E05"), pSprite);
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectCureBlindness::CGameEffectCureBlindness(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x496C30
CGameEffect* CGameEffectCureBlindness::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectCureBlindness* copy = new CGameEffectCureBlindness(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B55B0
BOOL CGameEffectCureBlindness::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetBaseStats()->m_generalState &= ~STATE_BLIND;
    pSprite->GetDerivedStats()->m_generalState &= ~STATE_BLIND;

    pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_BLINDNESS,
        pSprite->GetTimedEffectList()->GetPosCurrent(),
        -1);
    pSprite->GetEquipedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_BLINDNESS,
        pSprite->GetEquipedEffectList()->GetPosCurrent(),
        -1);

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectFeebleMindedness::CGameEffectFeebleMindedness(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x496CE0
CGameEffect* CGameEffectFeebleMindedness::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectFeebleMindedness* copy = new CGameEffectFeebleMindedness(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B5610
BOOL CGameEffectFeebleMindedness::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType != 0) {
        // NOTE: Uninline.
        DisplayStringRef(pSprite, 14407); // "Feebleminded"
    }

    pSprite->GetDerivedStats()->m_generalState |= STATE_FEEBLEMINDED;

    // NOTE: Uninline.
    AddPortraitIcon(pSprite, 54);

    pSprite->GetDerivedStats()->m_nINT = 3;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectCureFeebleMindedness::CGameEffectCureFeebleMindedness(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x496D90
CGameEffect* CGameEffectCureFeebleMindedness::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectCureFeebleMindedness* copy = new CGameEffectCureFeebleMindedness(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B56F0
BOOL CGameEffectCureFeebleMindedness::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetBaseStats()->m_generalState &= ~STATE_FEEBLEMINDED;
    pSprite->GetDerivedStats()->m_generalState &= ~STATE_FEEBLEMINDED;

    pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_FEEBLEMINDEDNESS,
        pSprite->GetTimedEffectList()->GetPosCurrent(),
        -1);
    pSprite->GetEquipedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_FEEBLEMINDEDNESS,
        pSprite->GetEquipedEffectList()->GetPosCurrent(),
        -1);

    pSprite->GetEquipedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_PORTRAITICON,
        pSprite->GetEquipedEffectList()->GetPosCurrent(),
        54);
    pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_PORTRAITICON,
        pSprite->GetTimedEffectList()->GetPosCurrent(),
        54);

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectDisease::CGameEffectDisease(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x496E40
CGameEffect* CGameEffectDisease::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectDisease* copy = new CGameEffectDisease(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B5790
BOOL CGameEffectDisease::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetDerivedStats()->m_generalState |= STATE_DISEASED;

    if (m_secondaryType != 0) {
        // NOTE: Uninline.
        DisplayStringRef(pSprite, 4389);
    }

    // NOTE: Uninline.
    AddPortraitIcon(pSprite, 7);

    switch (m_dwFlags) {
    case 4:
        pSprite->m_bonusStats.m_nSTR -= static_cast<SHORT>(m_effectAmount);
        return TRUE;
    case 5:
        pSprite->m_bonusStats.m_nDEX -= static_cast<SHORT>(m_effectAmount);
        return TRUE;
    case 6:
        pSprite->m_bonusStats.m_nCON -= static_cast<SHORT>(m_effectAmount);
        return TRUE;
    case 7:
        pSprite->m_bonusStats.m_nINT -= static_cast<SHORT>(m_effectAmount);
        return TRUE;
    case 8:
        pSprite->m_bonusStats.m_nINT -= static_cast<SHORT>(m_effectAmount);
        return TRUE;
    case 9:
        pSprite->m_bonusStats.m_nCHR -= static_cast<SHORT>(m_effectAmount);
        return TRUE;
    case 10:
        AddSlowEffect(pSprite);
        return TRUE;
    case 11:
        sub_4B5E50(pSprite);
        return TRUE;
    case 12:
        sub_4B5FF0(pSprite);
        return TRUE;
    case 13:
        pSprite->m_bonusStats.m_nSTR -= 2;
        pSprite->m_bonusStats.m_nDEX -= 2;
        pSprite->m_bonusStats.m_nCHR -= 2;
        AddSlowEffect(pSprite);
        return TRUE;
    case 14:
        sub_4B5BF0(pSprite);
        return TRUE;
    case 15:
        sub_4B5D90(pSprite);
        return TRUE;
    }

    if (m_secondaryType != 0) {
        m_special = g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime;
    }

    DWORD duration = m_durationType != 1 ? m_duration : 0;

    CPersistantEffect84C420* pEffect;
    switch (m_dwFlags) {
    case 1:
        pEffect = new CPersistantEffect84C420(0x40000000,
            m_effectAmount,
            105,
            m_special,
            duration);
        break;
    case 2:
        pEffect = new CPersistantEffect84C420(0x40000000,
            m_effectAmount,
            15,
            m_special,
            duration);
        break;
    case 3:
        pEffect = new CPersistantEffect84C420(0x40000000,
            1,
            15 * m_effectAmount,
            m_special,
            duration);
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 13882
        UTIL_ASSERT(FALSE);
    }

    pEffect->m_sourceID = m_sourceID;
    strcpy(reinterpret_cast<char*>(pEffect->field_29), m_sourceRes.GetResRefStr());
    pSprite->GetDerivedStats()->m_cRegeneratedPersistantEffectList.AddTail(pEffect);

    return TRUE;
}

// 0x4B5BF0
void CGameEffectDisease::sub_4B5BF0(CGameSprite* pSprite)
{
    pSprite->m_bonusStats.m_nSTR -= 3;
    pSprite->m_bonusStats.m_nDEX -= 3;

    if (m_secondaryType != 0) {
        // NOTE: Uninline.
        DisplayStringRef(pSprite, 14674); // "Blinded"
    }

    pSprite->GetDerivedStats()->m_generalState |= STATE_BLIND;

    // NOTE: Uninline.
    AddPortraitIcon(pSprite, 8);

    pSprite->GetDerivedStats()->field_C -= 4;
    pSprite->GetDerivedStats()->m_nTHAC0 -= 4;

    if (m_secondaryType != 0) {
        CGameObject* pSource;

        BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_sourceID,
            CGameObjectArray::THREAD_ASYNCH,
            &pSource,
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            CGameEffect* pEffect = IcewindMisc::sub_585380(pSource,
                0,
                0,
                3,
                static_cast<BYTE>(m_spellLevel),
                0);

            CMessage* message = new CMessageAddEffect(pEffect,
                pSprite->GetId(),
                pSprite->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        } else {
            m_done = TRUE;
        }
    }
}

// 0x4B5D90
void CGameEffectDisease::sub_4B5D90(CGameSprite* pSprite)
{
    AddSlowEffect(pSprite);

    if (m_secondaryType != 0) {
        m_special = g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime;
    }

    DWORD duration = m_durationType != 1 ? m_duration : 0;

    CPersistantEffect84C420* pEffect = new CPersistantEffect84C420(0x40000000,
        1,
        15,
        m_special,
        duration);
    pSprite->GetDerivedStats()->m_cRegeneratedPersistantEffectList.AddTail(pEffect);
}

// 0x4B5E50
void CGameEffectDisease::sub_4B5E50(CGameSprite* pSprite)
{
    if (IcewindMisc::IsDead(pSprite) != TRUE
        && !pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_MOLD_TOUCH]) {
        if (m_secondaryType != 0) {
            // NOTE: Uninline.
            DisplayStringRef(pSprite, 11501); // "Afflicted with Mold Touch"
        }

        pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_MOLD_TOUCH] = true;

        pSprite->SetColorRange(2);

        CPersistantEffect84C484* pEffect = new CPersistantEffect84C484(m_effectAmount,
            "",
            100,
            105,
            m_special,
            m_duration);
        pEffect->m_sourceID = m_sourceID;
        strcpy(reinterpret_cast<char*>(pEffect->field_29), m_sourceRes.GetResRefStr());
        pSprite->GetDerivedStats()->m_cRegeneratedPersistantEffectList.AddTail(pEffect);
    } else {
        m_done = TRUE;
    }
}

// 0x4B5FF0
void CGameEffectDisease::sub_4B5FF0(CGameSprite* pSprite)
{
    if (IcewindMisc::IsDead(pSprite) != TRUE) {
        for (int index = 0; index < m_effectAmount; index += 7) {
            CGameEffect* copy = Copy();
            copy->m_dwFlags = 11;
            copy->m_durationType = 4;
            copy->m_duration = 7 * (index + 1);
            copy->m_effectAmount = m_effectAmount - index;
            copy->m_flags |= 0x2;
            copy->m_casterLevel = m_casterLevel;
            pSprite->AddEffect(copy,
                CGameAIBase::EFFECT_LIST_TIMED,
                TRUE,
                TRUE);
        }
    }
    m_done = TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectCureDisease::CGameEffectCureDisease(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x496EF0
CGameEffect* CGameEffectCureDisease::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectCureDisease* copy = new CGameEffectCureDisease(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B6090
BOOL CGameEffectCureDisease::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetEquipedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_DISEASE,
        pSprite->GetEquipedEffectList()->GetPosCurrent(),
        -1);
    pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_DISEASE,
        pSprite->GetTimedEffectList()->GetPosCurrent(),
        -1);

    pSprite->GetEquipedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_PORTRAITICON,
        pSprite->GetEquipedEffectList()->GetPosCurrent(),
        7);
    pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_PORTRAITICON,
        pSprite->GetTimedEffectList()->GetPosCurrent(),
        7);

    pSprite->GetBaseStats()->m_generalState &= ~STATE_DISEASED;
    pSprite->GetDerivedStats()->m_generalState &= ~STATE_DISEASED;

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectDeafness::CGameEffectDeafness(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x496FA0
CGameEffect* CGameEffectDeafness::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectDeafness* copy = new CGameEffectDeafness(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B6130
BOOL CGameEffectDeafness::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_DEAFENED, TRUE);

    if (m_secondaryType != 0) {
        // NOTE: Uninline.
        DisplayStringRef(pSprite, 14073); // "Deafness"
    }

    // NOTE: Uninline.
    AddPortraitIcon(pSprite, 83);

    m_done = FALSE;

    return TRUE;
}

// 0x4B5490
void CGameEffectDeafness::OnRemove(CGameSprite* pSprite)
{
    PlaySound(CResRef("EFF_E05"), pSprite);
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectCureDeafness::CGameEffectCureDeafness(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x497050
CGameEffect* CGameEffectCureDeafness::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectCureDeafness* copy = new CGameEffectCureDeafness(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B6200
BOOL CGameEffectCureDeafness::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_DEAFNESS,
        pSprite->GetTimedEffectList()->GetPosCurrent(),
        -1);
    pSprite->GetEquipedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_DEAFNESS,
        pSprite->GetEquipedEffectList()->GetPosCurrent(),
        -1);

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectSetAIScript::CGameEffectSetAIScript(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x497100
CGameEffect* CGameEffectSetAIScript::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectSetAIScript* copy = new CGameEffectSetAIScript(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B6250
BOOL CGameEffectSetAIScript::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType != 0
        && m_res != "") {
        CAIScript* script = new CAIScript(m_res);
        pSprite->SetScript(static_cast<SHORT>(m_dwFlags), script);
    }
    return TRUE;
}

// 0x4B6360
void CGameEffectSetAIScript::OnRemove(CGameSprite* pSprite)
{
    CAIScript* script = new CAIScript(CResRef(""));
    pSprite->SetScript(static_cast<SHORT>(m_dwFlags), script);
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectImmunityToProjectile::CGameEffectImmunityToProjectile(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4971B0
CGameEffect* CGameEffectImmunityToProjectile::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectImmunityToProjectile* copy = new CGameEffectImmunityToProjectile(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B63E0
BOOL CGameEffectImmunityToProjectile::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(m_dwFlags));

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectResistSlashing::CGameEffectResistSlashing(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x497260
CGameEffect* CGameEffectResistSlashing::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectResistSlashing* copy = new CGameEffectResistSlashing(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B6400
BOOL CGameEffectResistSlashing::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_resistSlashingBase += static_cast<BYTE>(m_effectAmount);
            if (pSprite->GetBaseStats()->m_resistSlashingBase > 100) {
                pSprite->GetBaseStats()->m_resistSlashingBase = 100;
            }
            if (pSprite->GetBaseStats()->m_resistSlashingBase < 0) {
                pSprite->GetBaseStats()->m_resistSlashingBase = 0;
            }
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->m_bonusStats.m_nResistSlashing += static_cast<BYTE>(m_effectAmount);
            m_done = FALSE;
        }
        break;
    case 1:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_resistSlashingBase = static_cast<BYTE>(m_effectAmount);
            if (pSprite->GetBaseStats()->m_resistSlashingBase > 100) {
                pSprite->GetBaseStats()->m_resistSlashingBase = 100;
            }
            if (pSprite->GetBaseStats()->m_resistSlashingBase < 0) {
                pSprite->GetBaseStats()->m_resistSlashingBase = 0;
            }
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            if (pSprite->GetDerivedStats()->m_nResistSlashing < m_effectAmount) {
                pSprite->GetDerivedStats()->m_nResistSlashing = static_cast<BYTE>(m_effectAmount);
            }
            m_done = FALSE;
        }
        break;
    case 2:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_resistSlashingBase = static_cast<BYTE>(m_effectAmount) * pSprite->GetBaseStats()->m_resistSlashingBase / 100;
            if (pSprite->GetBaseStats()->m_resistSlashingBase > 100) {
                pSprite->GetBaseStats()->m_resistSlashingBase = 100;
            }
            if (pSprite->GetBaseStats()->m_resistSlashingBase < 0) {
                pSprite->GetBaseStats()->m_resistSlashingBase = 0;
            }
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->GetDerivedStats()->m_nResistSlashing = static_cast<SHORT>(m_effectAmount) * pSprite->GetBaseStats()->m_resistSlashingBase / 100;
            m_done = FALSE;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 14586
        UTIL_ASSERT(FALSE);
    }
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectResistCrushing::CGameEffectResistCrushing(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x497310
CGameEffect* CGameEffectResistCrushing::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectResistCrushing* copy = new CGameEffectResistCrushing(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B68A0
BOOL CGameEffectResistCrushing::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_resistCrushingBase += static_cast<BYTE>(m_effectAmount);
            if (pSprite->GetBaseStats()->m_resistCrushingBase > 100) {
                pSprite->GetBaseStats()->m_resistCrushingBase = 100;
            }
            if (pSprite->GetBaseStats()->m_resistCrushingBase < 0) {
                pSprite->GetBaseStats()->m_resistCrushingBase = 0;
            }
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->m_bonusStats.m_nResistCrushing += static_cast<BYTE>(m_effectAmount);
            m_done = FALSE;
        }
        break;
    case 1:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_resistCrushingBase = static_cast<BYTE>(m_effectAmount);
            if (pSprite->GetBaseStats()->m_resistCrushingBase > 100) {
                pSprite->GetBaseStats()->m_resistCrushingBase = 100;
            }
            if (pSprite->GetBaseStats()->m_resistCrushingBase < 0) {
                pSprite->GetBaseStats()->m_resistCrushingBase = 0;
            }
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            if (pSprite->GetDerivedStats()->m_nResistCrushing < m_effectAmount) {
                pSprite->GetDerivedStats()->m_nResistCrushing = static_cast<BYTE>(m_effectAmount);
            }
            m_done = FALSE;
        }
        break;
    case 2:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_resistCrushingBase = static_cast<BYTE>(m_effectAmount) * pSprite->GetBaseStats()->m_resistCrushingBase / 100;
            if (pSprite->GetBaseStats()->m_resistCrushingBase > 100) {
                pSprite->GetBaseStats()->m_resistCrushingBase = 100;
            }
            if (pSprite->GetBaseStats()->m_resistCrushingBase < 0) {
                pSprite->GetBaseStats()->m_resistCrushingBase = 0;
            }
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->GetDerivedStats()->m_nResistCrushing = static_cast<SHORT>(m_effectAmount) * pSprite->GetBaseStats()->m_resistCrushingBase / 100;
            m_done = FALSE;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 14900
        UTIL_ASSERT(FALSE);
    }
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectResistPiercing::CGameEffectResistPiercing(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4973C0
CGameEffect* CGameEffectResistPiercing::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectResistPiercing* copy = new CGameEffectResistPiercing(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B6A50
BOOL CGameEffectResistPiercing::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_resistPiercingBase += static_cast<BYTE>(m_effectAmount);
            if (pSprite->GetBaseStats()->m_resistPiercingBase > 100) {
                pSprite->GetBaseStats()->m_resistPiercingBase = 100;
            }
            if (pSprite->GetBaseStats()->m_resistPiercingBase < 0) {
                pSprite->GetBaseStats()->m_resistPiercingBase = 0;
            }
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->m_bonusStats.m_nResistPiercing += static_cast<BYTE>(m_effectAmount);
            m_done = FALSE;
        }
        break;
    case 1:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_resistPiercingBase = static_cast<BYTE>(m_effectAmount);
            if (pSprite->GetBaseStats()->m_resistPiercingBase > 100) {
                pSprite->GetBaseStats()->m_resistPiercingBase = 100;
            }
            if (pSprite->GetBaseStats()->m_resistPiercingBase < 0) {
                pSprite->GetBaseStats()->m_resistPiercingBase = 0;
            }
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            if (pSprite->GetDerivedStats()->m_nResistPiercing < m_effectAmount) {
                pSprite->GetDerivedStats()->m_nResistPiercing = static_cast<BYTE>(m_effectAmount);
            }
            m_done = FALSE;
        }
        break;
    case 2:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_resistPiercingBase = static_cast<BYTE>(m_effectAmount) * pSprite->GetBaseStats()->m_resistPiercingBase / 100;
            if (pSprite->GetBaseStats()->m_resistPiercingBase > 100) {
                pSprite->GetBaseStats()->m_resistPiercingBase = 100;
            }
            if (pSprite->GetBaseStats()->m_resistPiercingBase < 0) {
                pSprite->GetBaseStats()->m_resistPiercingBase = 0;
            }
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->GetDerivedStats()->m_nResistPiercing = static_cast<SHORT>(m_effectAmount) * pSprite->GetBaseStats()->m_resistPiercingBase / 100;
            m_done = FALSE;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 15021
        UTIL_ASSERT(FALSE);
    }
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectResistMissile::CGameEffectResistMissile(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x497470
CGameEffect* CGameEffectResistMissile::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectResistMissile* copy = new CGameEffectResistMissile(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B6C00
BOOL CGameEffectResistMissile::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_resistMissileBase += static_cast<BYTE>(m_effectAmount);
            if (pSprite->GetBaseStats()->m_resistMissileBase > 100) {
                pSprite->GetBaseStats()->m_resistMissileBase = 100;
            }
            if (pSprite->GetBaseStats()->m_resistMissileBase < 0) {
                pSprite->GetBaseStats()->m_resistMissileBase = 0;
            }
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->m_bonusStats.m_nResistMissile += static_cast<BYTE>(m_effectAmount);
            m_done = FALSE;
        }
        break;
    case 1:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_resistMissileBase = static_cast<BYTE>(m_effectAmount);
            if (pSprite->GetBaseStats()->m_resistMissileBase > 100) {
                pSprite->GetBaseStats()->m_resistMissileBase = 100;
            }
            if (pSprite->GetBaseStats()->m_resistMissileBase < 0) {
                pSprite->GetBaseStats()->m_resistMissileBase = 0;
            }
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            if (pSprite->GetDerivedStats()->m_nResistMissile < m_effectAmount) {
                pSprite->GetDerivedStats()->m_nResistMissile = static_cast<BYTE>(m_effectAmount);
            }
            m_done = FALSE;
        }
        break;
    case 2:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_resistMissileBase = static_cast<BYTE>(m_effectAmount) * pSprite->GetBaseStats()->m_resistMissileBase / 100;
            if (pSprite->GetBaseStats()->m_resistMissileBase > 100) {
                pSprite->GetBaseStats()->m_resistMissileBase = 100;
            }
            if (pSprite->GetBaseStats()->m_resistMissileBase < 0) {
                pSprite->GetBaseStats()->m_resistMissileBase = 0;
            }
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->GetDerivedStats()->m_nResistMissile = static_cast<SHORT>(m_effectAmount) * pSprite->GetBaseStats()->m_resistMissileBase / 100;
            m_done = FALSE;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 15142
        UTIL_ASSERT(FALSE);
    }
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectSkillLockPicking::CGameEffectSkillLockPicking(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x497520
CGameEffect* CGameEffectSkillLockPicking::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectSkillLockPicking* copy = new CGameEffectSkillLockPicking(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B6DB0
BOOL CGameEffectSkillLockPicking::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        if (m_durationType == 1) {
            // NOTE: Signed compare.
            pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_OPEN_LOCK] = max(static_cast<signed char>(pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_OPEN_LOCK]) + static_cast<signed char>(m_effectAmount), 0);
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->m_bonusStats.m_nSkills[CGAMESPRITE_SKILL_OPEN_LOCK] += static_cast<BYTE>(m_effectAmount);
            m_done = FALSE;
        }
        break;
    case 1:
        if (m_durationType == 1) {
            // NOTE: Signed compare.
            pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_OPEN_LOCK] = max(static_cast<signed char>(m_effectAmount), 0);
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            // NOTE: Signed compare.
            pSprite->GetDerivedStats()->m_nSkills[CGAMESPRITE_SKILL_OPEN_LOCK] = max(static_cast<signed char>(m_effectAmount), 0);
            m_done = FALSE;
        }
        break;
    case 2:
        if (m_durationType == 1) {
            // NOTE: Signed compare.
            pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_OPEN_LOCK] = max(static_cast<signed char>(pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_OPEN_LOCK] * static_cast<SHORT>(m_effectAmount) / 100), 0);
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            // NOTE: Signed compare.
            pSprite->GetDerivedStats()->m_nSkills[CGAMESPRITE_SKILL_OPEN_LOCK] = max(static_cast<signed char>(pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_OPEN_LOCK] * static_cast<SHORT>(m_effectAmount) / 100), 0);
            m_done = FALSE;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 15232
        UTIL_ASSERT(FALSE);
    }

    // NOTE: Uninline.
    if (pSprite->InControl()) {
        pSprite->m_bSendSpriteUpdate = TRUE;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectSkillTraps::CGameEffectSkillTraps(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4975D0
CGameEffect* CGameEffectSkillTraps::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectSkillTraps* copy = new CGameEffectSkillTraps(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B6EE0
BOOL CGameEffectSkillTraps::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        if (m_durationType == 1) {
            // NOTE: Signed compare.
            pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_SEARCH] = max(static_cast<signed char>(pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_SEARCH]) + static_cast<signed char>(m_effectAmount), 0);
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->m_bonusStats.m_nSkills[CGAMESPRITE_SKILL_SEARCH] += static_cast<BYTE>(m_effectAmount);
            m_done = FALSE;
        }
        break;
    case 1:
        if (m_durationType == 1) {
            // NOTE: Signed compare.
            pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_SEARCH] = max(static_cast<signed char>(m_effectAmount), 0);
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            // NOTE: Signed compare.
            pSprite->GetDerivedStats()->m_nSkills[CGAMESPRITE_SKILL_SEARCH] = max(static_cast<signed char>(m_effectAmount), 0);
            m_done = FALSE;
        }
        break;
    case 2:
        if (m_durationType == 1) {
            // NOTE: Signed compare.
            pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_SEARCH] = max(static_cast<signed char>(pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_SEARCH] * static_cast<SHORT>(m_effectAmount) / 100), 0);
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            // NOTE: Signed compare.
            pSprite->GetDerivedStats()->m_nSkills[CGAMESPRITE_SKILL_SEARCH] = max(static_cast<signed char>(pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_SEARCH] * static_cast<SHORT>(m_effectAmount) / 100), 0);
            m_done = FALSE;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 15326
        UTIL_ASSERT(FALSE);
    }

    // NOTE: Uninline.
    if (pSprite->InControl()) {
        pSprite->m_bSendSpriteUpdate = TRUE;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectSkillPickPocket::CGameEffectSkillPickPocket(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x497680
CGameEffect* CGameEffectSkillPickPocket::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectSkillPickPocket* copy = new CGameEffectSkillPickPocket(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B7010
BOOL CGameEffectSkillPickPocket::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        if (m_durationType == 1) {
            // NOTE: Signed compare.
            pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_PICK_POCKET] = max(static_cast<signed char>(pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_PICK_POCKET]) + static_cast<signed char>(m_effectAmount), 0);
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->m_bonusStats.m_nSkills[CGAMESPRITE_SKILL_PICK_POCKET] += static_cast<BYTE>(m_effectAmount);
            m_done = FALSE;
        }
        break;
    case 1:
        if (m_durationType == 1) {
            // NOTE: Signed compare.
            pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_PICK_POCKET] = max(static_cast<signed char>(m_effectAmount), 0);
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            // NOTE: Signed compare.
            pSprite->GetDerivedStats()->m_nSkills[CGAMESPRITE_SKILL_PICK_POCKET] = max(static_cast<signed char>(m_effectAmount), 0);
            m_done = FALSE;
        }
        break;
    case 2:
        if (m_durationType == 1) {
            // NOTE: Signed compare.
            pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_PICK_POCKET] = max(static_cast<signed char>(pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_PICK_POCKET] * static_cast<SHORT>(m_effectAmount) / 100), 0);
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            // NOTE: Signed compare.
            pSprite->GetDerivedStats()->m_nSkills[CGAMESPRITE_SKILL_PICK_POCKET] = max(static_cast<signed char>(pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_PICK_POCKET] * static_cast<SHORT>(m_effectAmount) / 100), 0);
            m_done = FALSE;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 15420
        UTIL_ASSERT(FALSE);
    }

    // NOTE: Uninline.
    if (pSprite->InControl()) {
        pSprite->m_bSendSpriteUpdate = TRUE;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectSkillUseMagicDevice::CGameEffectSkillUseMagicDevice(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x497730
CGameEffect* CGameEffectSkillUseMagicDevice::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectSkillUseMagicDevice* copy = new CGameEffectSkillUseMagicDevice(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B7140
BOOL CGameEffectSkillUseMagicDevice::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        if (m_durationType == 1) {
            // NOTE: Signed compare.
            pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_USE_MAGIC_DEVICE] = max(static_cast<signed char>(pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_USE_MAGIC_DEVICE]) + static_cast<signed char>(m_effectAmount), 0);
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->m_bonusStats.m_nSkills[CGAMESPRITE_SKILL_USE_MAGIC_DEVICE] += static_cast<BYTE>(m_effectAmount);
            m_done = FALSE;
        }
        break;
    case 1:
        if (m_durationType == 1) {
            // NOTE: Signed compare.
            pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_USE_MAGIC_DEVICE] = max(static_cast<signed char>(m_effectAmount), 0);
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            // NOTE: Signed compare.
            pSprite->GetDerivedStats()->m_nSkills[CGAMESPRITE_SKILL_USE_MAGIC_DEVICE] = max(static_cast<signed char>(m_effectAmount), 0);
            m_done = FALSE;
        }
        break;
    case 2:
        if (m_durationType == 1) {
            // NOTE: Signed compare.
            pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_USE_MAGIC_DEVICE] = max(static_cast<signed char>(pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_USE_MAGIC_DEVICE] * static_cast<SHORT>(m_effectAmount) / 100), 0);
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            // NOTE: Signed compare.
            pSprite->GetDerivedStats()->m_nSkills[CGAMESPRITE_SKILL_USE_MAGIC_DEVICE] = max(static_cast<signed char>(pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_USE_MAGIC_DEVICE] * static_cast<SHORT>(m_effectAmount) / 100), 0);
            m_done = FALSE;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 15515
        UTIL_ASSERT(FALSE);
    }

    // NOTE: Uninline.
    if (pSprite->InControl()) {
        pSprite->m_bSendSpriteUpdate = TRUE;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectFatigue::CGameEffectFatigue(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4977E0
CGameEffect* CGameEffectFatigue::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectFatigue* copy = new CGameEffectFatigue(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B7270
BOOL CGameEffectFatigue::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        switch (m_durationType) {
        case 1:
            pSprite->GetBaseStats()->m_fatigue += static_cast<BYTE>(m_effectAmount);

            if (pSprite->GetBaseStats()->m_fatigue > 100) {
                pSprite->GetBaseStats()->m_fatigue = 100;
            }

            m_forceRepass = TRUE;
            m_done = TRUE;
            break;
        default:
            pSprite->m_bonusStats.m_nFatigue += static_cast<SHORT>(m_effectAmount);
            m_done = FALSE;
            break;
        }
        break;
    case 1:
        switch (m_durationType) {
        case 1:
            pSprite->GetBaseStats()->m_fatigue = static_cast<BYTE>(m_effectAmount);

            if (pSprite->GetBaseStats()->m_fatigue > 100) {
                pSprite->GetBaseStats()->m_fatigue = 100;
            }

            m_forceRepass = TRUE;
            m_done = TRUE;
            break;
        default:
            pSprite->GetDerivedStats()->m_nFatigue += static_cast<BYTE>(m_effectAmount);

            if (pSprite->GetDerivedStats()->m_nFatigue > 100) {
                pSprite->GetDerivedStats()->m_nFatigue = 100;
            }

            if (pSprite->GetDerivedStats()->m_nFatigue < 0) {
                pSprite->GetDerivedStats()->m_nFatigue = 0;
            }

            m_done = FALSE;
            break;
        }
        break;
    case 2:
        switch (m_durationType) {
        case 1:
            pSprite->GetBaseStats()->m_fatigue = pSprite->GetBaseStats()->m_fatigue * static_cast<BYTE>(m_effectAmount) / 100;

            if (pSprite->GetBaseStats()->m_fatigue > 100) {
                pSprite->GetBaseStats()->m_fatigue = 100;
            }

            m_forceRepass = TRUE;
            m_done = TRUE;
            break;
        default:
            pSprite->GetDerivedStats()->m_nFatigue = pSprite->GetBaseStats()->m_fatigue * static_cast<BYTE>(m_effectAmount) / 100;

            if (pSprite->GetDerivedStats()->m_nFatigue > 100) {
                pSprite->GetDerivedStats()->m_nFatigue = 100;
            }

            if (pSprite->GetDerivedStats()->m_nFatigue < 0) {
                pSprite->GetDerivedStats()->m_nFatigue = 0;
            }

            m_done = FALSE;
            break;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 15630
        UTIL_ASSERT(FALSE);
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectIntoxication::CGameEffectIntoxication(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x497890
CGameEffect* CGameEffectIntoxication::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectIntoxication* copy = new CGameEffectIntoxication(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B7440
BOOL CGameEffectIntoxication::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        switch (m_durationType) {
        case 1:
            pSprite->GetBaseStats()->m_intoxication += static_cast<BYTE>(m_effectAmount);

            if (pSprite->GetBaseStats()->m_intoxication > 100) {
                pSprite->GetBaseStats()->m_intoxication = 100;
            }

            m_forceRepass = TRUE;
            m_done = TRUE;
            break;
        default:
            pSprite->m_bonusStats.m_nIntoxication += static_cast<SHORT>(m_effectAmount);
            m_done = FALSE;
            break;
        }
        break;
    case 1:
        switch (m_durationType) {
        case 1:
            pSprite->GetBaseStats()->m_intoxication = static_cast<BYTE>(m_effectAmount);

            if (pSprite->GetBaseStats()->m_intoxication > 100) {
                pSprite->GetBaseStats()->m_intoxication = 100;
            }

            m_forceRepass = TRUE;
            m_done = TRUE;
            break;
        default:
            pSprite->GetDerivedStats()->m_nIntoxication += static_cast<BYTE>(m_effectAmount);

            if (pSprite->GetDerivedStats()->m_nIntoxication > 100) {
                pSprite->GetDerivedStats()->m_nIntoxication = 100;
            }

            if (pSprite->GetDerivedStats()->m_nIntoxication < 0) {
                pSprite->GetDerivedStats()->m_nIntoxication = 0;
            }

            m_done = FALSE;
            break;
        }
        break;
    case 2:
        switch (m_durationType) {
        case 1:
            pSprite->GetBaseStats()->m_intoxication = pSprite->GetBaseStats()->m_intoxication * static_cast<BYTE>(m_effectAmount) / 100;

            if (pSprite->GetBaseStats()->m_intoxication > 100) {
                pSprite->GetBaseStats()->m_intoxication = 100;
            }

            m_forceRepass = TRUE;
            m_done = TRUE;
            break;
        default:
            pSprite->GetDerivedStats()->m_nIntoxication = pSprite->GetBaseStats()->m_intoxication * static_cast<BYTE>(m_effectAmount) / 100;

            if (pSprite->GetDerivedStats()->m_nIntoxication > 100) {
                pSprite->GetDerivedStats()->m_nIntoxication = 100;
            }

            if (pSprite->GetDerivedStats()->m_nIntoxication < 0) {
                pSprite->GetDerivedStats()->m_nIntoxication = 0;
            }

            m_done = FALSE;
            break;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 15742
        UTIL_ASSERT(FALSE);
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectSkillTracking::CGameEffectSkillTracking(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x497940
CGameEffect* CGameEffectSkillTracking::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectSkillTracking* copy = new CGameEffectSkillTracking(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B7610
BOOL CGameEffectSkillTracking::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        if (m_durationType == 1) {
            // NOTE: Signed compare.
            pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_WILDERNESS_LORE] = max(static_cast<signed char>(pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_WILDERNESS_LORE]) + static_cast<signed char>(m_effectAmount), 0);
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->m_bonusStats.m_nSkills[CGAMESPRITE_SKILL_WILDERNESS_LORE] += static_cast<BYTE>(m_effectAmount);
            m_done = FALSE;
        }
        break;
    case 1:
        if (m_durationType == 1) {
            // NOTE: Signed compare.
            pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_WILDERNESS_LORE] = max(static_cast<signed char>(m_effectAmount), 0);
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            // NOTE: Signed compare.
            pSprite->GetDerivedStats()->m_nSkills[CGAMESPRITE_SKILL_WILDERNESS_LORE] = max(static_cast<signed char>(m_effectAmount), 0);
            m_done = FALSE;
        }
        break;
    case 2:
        if (m_durationType == 1) {
            // NOTE: Signed compare.
            pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_WILDERNESS_LORE] = max(static_cast<signed char>(pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_WILDERNESS_LORE] * static_cast<SHORT>(m_effectAmount) / 100), 0);
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            // NOTE: Signed compare.
            pSprite->GetDerivedStats()->m_nSkills[CGAMESPRITE_SKILL_WILDERNESS_LORE] = max(static_cast<signed char>(pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_WILDERNESS_LORE] * static_cast<SHORT>(m_effectAmount) / 100), 0);
            m_done = FALSE;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 15831
        UTIL_ASSERT(FALSE);
    }

    // NOTE: Uninline.
    if (pSprite->InControl()) {
        pSprite->m_bSendSpriteUpdate = TRUE;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectLevel::CGameEffectLevel(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4979F0
CGameEffect* CGameEffectLevel::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectLevel* copy = new CGameEffectLevel(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B7770
BOOL CGameEffectLevel::ApplyEffect(CGameSprite* pSprite)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
    // __LINE__: 15980
    UTIL_ASSERT_MSG(m_special <= 10, "m_special < MAX_CLASSES && m_special >= 0");

    // The change, kept so that the character level stays in 0..30.
    INT nAmount;
    INT nNewLevel;
    switch (m_dwFlags) {
    case 0:
        // Add to the class's level.
        nAmount = m_effectAmount;
        nNewLevel = pSprite->m_baseStats.m_characterLevel + nAmount;
        if (nNewLevel > 30) {
            nAmount += 30 - nNewLevel;
        }
        if (nNewLevel < 0) {
            nAmount -= nNewLevel;
        }

        if (m_durationType == 1) {
            pSprite->SetClassLevel(m_special, pSprite->GetClassLevel(m_special) + nAmount);
            m_forceRepass = TRUE;
            m_done = TRUE;
            return TRUE;
        }

        pSprite->m_bonusStats.SetClassLevel(m_special, pSprite->GetClassLevel(m_special) + nAmount);
        break;
    case 1:
        // Set the class's level.
        nAmount = m_effectAmount;
        if (m_durationType == 1) {
            nNewLevel = pSprite->m_baseStats.m_characterLevel - pSprite->m_baseStats.m_classLevels[m_special] + nAmount;
            if (nNewLevel > 30) {
                nAmount += 30 - nNewLevel;
            }
            if (nNewLevel < 0) {
                nAmount -= nNewLevel;
            }

            pSprite->SetClassLevel(m_special, nAmount);
            m_forceRepass = TRUE;
            m_done = TRUE;
            return TRUE;
        }

        nNewLevel = pSprite->m_derivedStats.m_nLevel - pSprite->m_derivedStats.m_nClassLevels[m_special] + nAmount;
        if (nNewLevel > 30) {
            nAmount += 30 - nNewLevel;
        }
        if (nNewLevel < 0) {
            nAmount -= nNewLevel;
        }

        pSprite->m_bonusStats.SetClassLevel(m_special, nAmount);
        break;
    case 2:
        if (m_durationType == 1) {
            m_done = TRUE;
            return TRUE;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 16083
        UTIL_ASSERT(FALSE);
        return TRUE;
    }

    m_done = FALSE;
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectSTRExtra::CGameEffectSTRExtra(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x497AA0
CGameEffect* CGameEffectSTRExtra::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectSTRExtra* copy = new CGameEffectSTRExtra(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B7740
BOOL CGameEffectSTRExtra::ApplyEffect(CGameSprite* pSprite)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
    // __LINE__: 15953
    UTIL_ASSERT(FALSE);
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectRegeneration::CGameEffectRegeneration(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x497B50
CGameEffect* CGameEffectRegeneration::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectRegeneration* copy = new CGameEffectRegeneration(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B65B0
void CGameEffectRegeneration::OnAddSpecific(CGameSprite* pSprite)
{
    // Caps the time left: mode 0 at m_effectAmount seconds, mode 1 at
    // m_effectAmount percent of the hit points (in ticks).
    ULONG nGameTime = g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime;
    LONG nLeft = m_duration - nGameTime;

    switch (static_cast<WORD>(m_dwFlags)) {
    case 0:
        if (nLeft >= m_effectAmount * 15) {
            nLeft = m_effectAmount * 15;
        }
        break;
    case 1:
        if (nLeft >= pSprite->m_baseStats.m_hitPoints * m_effectAmount / 100) {
            nLeft = pSprite->m_baseStats.m_hitPoints * m_effectAmount / 100;
        }
        break;
    }

    m_duration = nGameTime + nLeft;
}

// 0x4B6620
BOOL CGameEffectRegeneration::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType) {
        // NOTE: Uninline.
        DisplayStringRef(pSprite, 11006);
    }

    // NOTE: Uninline.
    AddPortraitIcon(pSprite, 42);

    if (m_secondaryType) {
        m_special = g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime;
    }

    DWORD duration = m_durationType != 1 ? m_duration : 0;

    CPersistantEffect84C494* pEffect;
    switch (m_dwFlags) {
    case 2:
        pEffect = new CPersistantEffect84C494(m_effectAmount,
            15,
            m_special,
            duration);
        break;
    case 3:
        pEffect = new CPersistantEffect84C494(1,
            m_effectAmount * 15,
            m_special,
            duration);
        break;
    case 4:
        pEffect = new CPersistantEffect84C494(m_effectAmount,
            105,
            m_special,
            duration);
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 14698
        UTIL_ASSERT(FALSE);

        // NOTE: The original goes on with the sprite as the new persistent
        // effect: it writes the source id and the source resref into the
        // sprite and adds the sprite to the list. Bounded here (ruling
        // P4-10).
        return TRUE;
    }

    pEffect->m_sourceID = m_sourceID;
    strcpy(reinterpret_cast<char*>(pEffect->field_29), m_sourceRes.GetResRefStr());
    pSprite->GetDerivedStats()->m_cRegeneratedPersistantEffectList.AddTail(pEffect);

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectDurationCasting::CGameEffectDurationCasting(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x497C00
CGameEffect* CGameEffectDurationCasting::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectDurationCasting* copy = new CGameEffectDurationCasting(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B7930
BOOL CGameEffectDurationCasting::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        pSprite->GetDerivedStats()->m_nSpellDurationModMage = static_cast<BYTE>(m_effectAmount);
        break;
    case 1:
        pSprite->GetDerivedStats()->m_nSpellDurationModPriest = static_cast<BYTE>(m_effectAmount);
        break;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectProtectionFromCreature::CGameEffectProtectionFromCreature(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x497CB0
CGameEffect* CGameEffectProtectionFromCreature::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectProtectionFromCreature* copy = new CGameEffectProtectionFromCreature(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B7970
BOOL CGameEffectProtectionFromCreature::ApplyEffect(CGameSprite* pSprite)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
    // __LINE__: 16143
    UTIL_ASSERT_MSG(FALSE, "CGameEffectProtectionCircle game effect not supported in Icewind II (-rjf)");
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectImmunityToEffect::CGameEffectImmunityToEffect(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x497D60
CGameEffect* CGameEffectImmunityToEffect::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectImmunityToEffect* copy = new CGameEffectImmunityToEffect(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B79A0
BOOL CGameEffectImmunityToEffect::ApplyEffect(CGameSprite* pSprite)
{
    ITEM_EFFECT* effect = new ITEM_EFFECT;
    effect->effectID = static_cast<WORD>(m_dwFlags);

    CPoint target(-1, -1);
    CPoint source(-1, -1);

    CGameEffect* pEffect = DecodeEffect(effect, source, -1, target);
    pEffect->m_compareIdOnly = TRUE;

    pSprite->GetDerivedStats()->m_cImmunitiesEffect.AddTail(pEffect);

    delete effect;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectImmunityToSpellLevel::CGameEffectImmunityToSpellLevel(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x497E10
CGameEffect* CGameEffectImmunityToSpellLevel::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectImmunityToSpellLevel* copy = new CGameEffectImmunityToSpellLevel(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B7AC0
BOOL CGameEffectImmunityToSpellLevel::ApplyEffect(CGameSprite* pSprite)
{
    if (m_effectAmount < 10) {
        pSprite->GetDerivedStats()->m_cImmunitiesSpellLevel.m_levels[m_effectAmount] = TRUE;
    }

    return true;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectName::CGameEffectName(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x497EC0
CGameEffect* CGameEffectName::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectName* copy = new CGameEffectName(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B7AE0
BOOL CGameEffectName::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetBaseStats()->m_name = m_effectAmount;

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectXP::CGameEffectXP(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x497F70
CGameEffect* CGameEffectXP::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectXP* copy = new CGameEffectXP(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B7B00
BOOL CGameEffectXP::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        switch (m_durationType) {
        case 1:
            pSprite->GetBaseStats()->m_xp += m_effectAmount;
            m_forceRepass = TRUE;
            m_done = TRUE;
            break;
        default:
            pSprite->m_bonusStats.m_nXP += m_effectAmount;
            m_done = FALSE;
            break;
        }
        break;
    case 1:
        switch (m_durationType) {
        case 1:
            pSprite->GetBaseStats()->m_xp = m_effectAmount;
            m_forceRepass = TRUE;
            m_done = TRUE;
            break;
        default:
            pSprite->GetDerivedStats()->m_nXP = m_effectAmount;
            m_done = FALSE;
            break;
        }
        break;
    case 2:
        switch (m_durationType) {
        case 1:
            pSprite->GetBaseStats()->m_xp = pSprite->GetBaseStats()->m_xp * m_effectAmount / 100;
            m_forceRepass = TRUE;
            m_done = TRUE;
            break;
        default:
            pSprite->GetDerivedStats()->m_nXP = pSprite->GetBaseStats()->m_xp * m_effectAmount / 100;
            m_done = FALSE;
            break;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 16415
        UTIL_ASSERT(FALSE);
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectGold::CGameEffectGold(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x498020
CGameEffect* CGameEffectGold::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectGold* copy = new CGameEffectGold(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B7C30
BOOL CGameEffectGold::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        switch (m_durationType) {
        case 1:
            pSprite->GetBaseStats()->m_gold += static_cast<SHORT>(m_effectAmount);
            m_forceRepass = TRUE;
            m_done = TRUE;
            break;
        default:
            pSprite->m_bonusStats.m_nXP += static_cast<SHORT>(m_effectAmount);
            m_done = FALSE;
            break;
        }
        break;
    case 1:
        switch (m_durationType) {
        case 1:
            pSprite->GetBaseStats()->m_gold = static_cast<BYTE>(m_effectAmount);
            m_forceRepass = TRUE;
            m_done = TRUE;
            break;
        default:
            pSprite->GetDerivedStats()->m_nXP = static_cast<BYTE>(m_effectAmount);
            m_done = FALSE;
            break;
        }
        break;
    case 2:
        switch (m_durationType) {
        case 1:
            pSprite->GetBaseStats()->m_gold = pSprite->GetBaseStats()->m_gold * static_cast<SHORT>(m_effectAmount) / 100;
            m_forceRepass = TRUE;
            m_done = TRUE;
            break;
        default:
            pSprite->GetDerivedStats()->m_nXP = pSprite->GetBaseStats()->m_gold * static_cast<SHORT>(m_effectAmount) / 100;
            m_done = FALSE;
            break;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 16504
        UTIL_ASSERT(FALSE);
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectMoralBreak::CGameEffectMoralBreak(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4980D0
CGameEffect* CGameEffectMoralBreak::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectMoralBreak* copy = new CGameEffectMoralBreak(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B7D70
BOOL CGameEffectMoralBreak::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        switch (m_durationType) {
        case 1:
            pSprite->GetBaseStats()->m_moraleBreak += static_cast<BYTE>(m_effectAmount);
            m_forceRepass = TRUE;
            m_done = TRUE;
            break;
        default:
            pSprite->m_bonusStats.m_nMoraleBreak += static_cast<SHORT>(m_effectAmount);
            m_done = FALSE;
            break;
        }
        break;
    case 1:
        switch (m_durationType) {
        case 1:
            pSprite->GetBaseStats()->m_moraleBreak = static_cast<BYTE>(m_effectAmount);
            m_forceRepass = TRUE;
            m_done = TRUE;
            break;
        default:
            pSprite->GetDerivedStats()->m_nMoraleBreak = static_cast<BYTE>(m_effectAmount);
            m_done = FALSE;
            break;
        }
        break;
    case 2:
        switch (m_durationType) {
        case 1:
            pSprite->GetBaseStats()->m_moraleBreak = pSprite->GetBaseStats()->m_moraleBreak * static_cast<BYTE>(m_effectAmount) / 100;
            m_forceRepass = TRUE;
            m_done = TRUE;
            break;
        default:
            pSprite->GetDerivedStats()->m_nMoraleBreak = pSprite->GetBaseStats()->m_moraleBreak * static_cast<BYTE>(m_effectAmount) / 100;
            m_done = FALSE;
            break;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 16593
        UTIL_ASSERT(FALSE);
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectPortrait::CGameEffectPortrait(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x498180
CGameEffect* CGameEffectPortrait::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectPortrait* copy = new CGameEffectPortrait(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B7ED0
BOOL CGameEffectPortrait::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        m_res.GetResRef(pSprite->GetBaseStats()->m_portraitSmall);
        break;
    case 1:
        m_res.GetResRef(pSprite->GetBaseStats()->m_portraitLarge);
        break;
    }

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectReputation::CGameEffectReputation(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x498230
CGameEffect* CGameEffectReputation::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectReputation* copy = new CGameEffectReputation(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B7F10
BOOL CGameEffectReputation::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        switch (m_durationType) {
        case 1:
            pSprite->GetBaseStats()->m_reputation += static_cast<BYTE>(m_effectAmount);
            m_forceRepass = TRUE;
            m_done = TRUE;
            break;
        default:
            pSprite->m_bonusStats.m_nReputation += static_cast<SHORT>(m_effectAmount);
            m_done = FALSE;
            break;
        }
        break;
    case 1:
        switch (m_durationType) {
        case 1:
            pSprite->GetBaseStats()->m_reputation = static_cast<BYTE>(m_effectAmount);
            m_forceRepass = TRUE;
            m_done = TRUE;
            break;
        default:
            pSprite->GetDerivedStats()->m_nReputation = static_cast<BYTE>(m_effectAmount);

            if (pSprite->GetDerivedStats()->m_nReputation < 0) {
                pSprite->GetDerivedStats()->m_nReputation = 0;
            }

            m_done = FALSE;
            break;
        }
        break;
    case 2:
        switch (m_durationType) {
        case 1:
            pSprite->GetBaseStats()->m_reputation = pSprite->GetBaseStats()->m_reputation * static_cast<BYTE>(m_effectAmount) / 100;
            m_forceRepass = TRUE;
            m_done = TRUE;
            break;
        default:
            pSprite->GetDerivedStats()->m_nReputation = pSprite->GetBaseStats()->m_reputation * static_cast<BYTE>(m_effectAmount) / 100;
            m_done = FALSE;
            break;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 16716
        UTIL_ASSERT(FALSE);
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4982E0
CGameEffectHoldCreature::CGameEffectHoldCreature(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x498320
CGameEffect* CGameEffectHoldCreature::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectHoldCreature* copy = new CGameEffectHoldCreature(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B8090
BOOL CGameEffectHoldCreature::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType != 0) {
        // NOTE: Uninline.
        DisplayStringRef(pSprite, 14102); // "Held"
    }

    pSprite->field_9D15 = 1;

    // NOTE: Uninline.
    AddPortraitIcon(pSprite, 13);

    pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_HELD);
    pSprite->GetDerivedStats()->m_generalState |= STATE_HELPLESS;

    m_done = FALSE;

    return TRUE;
}

// 0x4B5490
void CGameEffectHoldCreature::OnRemove(CGameSprite* pSprite)
{
    PlaySound(CResRef("EFF_E05"), pSprite);
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectRetreatFrom::CGameEffectRetreatFrom(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4983D0
CGameEffect* CGameEffectRetreatFrom::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectRetreatFrom* copy = new CGameEffectRetreatFrom(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B8180
BOOL CGameEffectRetreatFrom::ApplyEffect(CGameSprite* pSprite)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
    // __LINE__: 16918
    UTIL_ASSERT_MSG(FALSE, "CGameEffectRetreatFrom game effect not supported in Icewind II (-rjf)");
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectCreateWeapon::CGameEffectCreateWeapon(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x498480
CGameEffect* CGameEffectCreateWeapon::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectCreateWeapon* copy = new CGameEffectCreateWeapon(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B81B0
BOOL CGameEffectCreateWeapon::ApplyEffect(CGameSprite* pSprite)
{
    // A summoned weapon. The cleric's DECASTA needs a free hand: with the
    // off hand of the current weapon set taken it only tells so.
    BYTE nShieldSlot = static_cast<BYTE>((pSprite->m_nWeaponSet + 22) * 2);
    if (strcmp(m_res.GetResRefStr(), CString("DECASTA")) == 0
        && pSprite->m_equipment.m_items[nShieldSlot] != NULL) {
        COLORREF rgbNameColor = CVidPalette::RANGE_COLORS[pSprite->GetBaseStats()->m_colors[2]];

        STR_RES strRes;
        CString sText;
        g_pBaldurChitin->GetTlkTable().Fetch(14796, strRes);
        sText += strRes.szText + " : ";
        g_pBaldurChitin->GetTlkTable().Fetch(9380, strRes);
        sText += strRes.szText;

        g_pBaldurChitin->GetBaldurMessage()->DisplayText(pSprite->GetName(),
            sText,
            rgbNameColor,
            RGB(0xD7, 0xD7, 0xBE),
            -1,
            pSprite->GetId(),
            pSprite->GetId());

        m_done = TRUE;

        // NOTE: The original returns 0 here (0x4B830F).
        return FALSE;
    }

    // The weapon goes when the effect's time is up: a delayed Destroy
    // Weapon (effect 112) of the same item.
    ITEM_EFFECT* effect = new ITEM_EFFECT;
    effect->effectID = CGAMEEFFECT_DESTROYWEAPON;
    effect->targetType = 0;
    effect->spellLevel = 0;
    effect->effectAmount = 0;
    effect->dwFlags = 0;
    effect->probabilityUpper = 100;
    effect->probabilityLower = 0;
    effect->numDice = 0;
    effect->diceSize = 0;
    effect->savingThrow = 0;
    effect->saveMod = 0;
    effect->special = 0;
    // NOTE: The original leaves the resref uninitialised (it is replaced
    // below).
    memset(effect->res, 0, sizeof(effect->res));
    effect->durationType = 4;
    effect->duration = (m_duration - g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime) / 15;

    CGameEffect* pDestroy = CGameEffect::DecodeEffect(effect, CPoint(-1, -1), -1, CPoint(-1, -1));
    pDestroy->m_res = m_res;
    pDestroy->m_flags |= 0x1;

    if (m_effectAmount < 1) {
        m_effectAmount = 1;
    }

    CItem* pItem = new CItem(m_res,
        static_cast<WORD>(m_effectAmount),
        static_cast<WORD>(m_effectAmount),
        static_cast<WORD>(m_effectAmount),
        0,
        0);
    if (pItem == NULL) {
        delete effect;
        return TRUE;
    }

    // Slot 42 holds the magic weapon.
    if (pSprite->m_equipment.m_items[42] != NULL) {
        sub_4B8730(pSprite, 42);
    }

    pSprite->m_equipment.m_items[42] = pItem;
    pSprite->SelectWeaponAbility(42, 0, 1, 1);

    if (m_durationType != 1) {
        pDestroy->m_casterLevel = m_casterLevel;
        pSprite->AddEffect(pDestroy, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);
    }

    g_pBaldurChitin->GetObjectGame()->m_cButtonArray.ResetState();

    m_done = TRUE;
    delete effect;
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectDestroyWeapon::CGameEffectDestroyWeapon(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x498530
CGameEffect* CGameEffectDestroyWeapon::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectDestroyWeapon* copy = new CGameEffectDestroyWeapon(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B8580
BOOL CGameEffectDestroyWeapon::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->m_hasColorEffects = TRUE;
    pSprite->m_hasColorRangeEffects = TRUE;
    pSprite->field_5640 = 1;

    INT nSlot = 0;
    while (pSprite->m_equipment.m_items[nSlot] == NULL
        || pSprite->m_equipment.m_items[nSlot]->cResRef != m_res) {
        nSlot++;
        if (nSlot > 50) {
            pSprite->field_562C = 1;
            m_done = TRUE;
            return TRUE;
        }
    }

    // NOTE: The original moves the selection to slot 10 and then compares
    // the selection with the slot again below (0x4B8603, 0x4B866F).
    if (pSprite->m_equipment.m_selectedWeapon == nSlot) {
        pSprite->m_equipment.m_selectedWeapon = CGameSpriteEquipment::SLOT_FIST;
    }

    CItem* pItem = pSprite->m_equipment.m_items[nSlot];
    pSprite->m_equipment.m_items[nSlot] = NULL;
    pItem->Unequip(pSprite, nSlot, TRUE, FALSE);
    g_pBaldurChitin->GetObjectGame()->AddDisposableItem(pItem);

    // NOTE: Inlined CGameObject::InControl.
    if (pSprite->InControl() && g_pChitin->cNetwork.m_bConnectionEstablished == TRUE) {
        CMessage* message = new CMessageSpriteEquipment(pSprite, pSprite->GetId(), pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    if (pSprite->m_equipment.m_selectedWeapon == nSlot) {
        pSprite->EquipMostDamagingMelee();
    }

    if (nSlot + 1 > 42 && nSlot + 1 < 51) {
        pSprite->EquipMostDamagingMelee();
    }

    pSprite->field_562C = 1;

    PlaySound(CResRef("EFF_M02"), pSprite);

    m_done = TRUE;
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectEquipWeapon::CGameEffectEquipWeapon(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4985E0
CGameEffect* CGameEffectEquipWeapon::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectEquipWeapon* copy = new CGameEffectEquipWeapon(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x799C90
BOOL CGameEffectEquipWeapon::ApplyEffect(CGameSprite* pSprite)
{
    // NOTE: The vtable's slot +8 is the shared "return 1" (ret 4) that
    // identical code folding made of every such override; the effect works
    // through the code that looks for it, not here.
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectDither::CGameEffectDither(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x498690
CGameEffect* CGameEffectDither::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectDither* copy = new CGameEffectDither(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x799C90
BOOL CGameEffectDither::ApplyEffect(CGameSprite* pSprite)
{
    // NOTE: The vtable's slot +8 is the shared "return 1" (ret 4) that
    // identical code folding made of every such override; the effect works
    // through the code that looks for it, not here.
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectDispelInvisible::CGameEffectDispelInvisible(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x498740
CGameEffect* CGameEffectDispelInvisible::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectDispelInvisible* copy = new CGameEffectDispelInvisible(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B2900
BOOL CGameEffectDispelInvisible::ApplyEffect(CGameSprite* pSprite)
{
    if ((pSprite->m_tempStats.m_generalState & STATE_NONDETECTION) == 0) {
        pSprite->GetBaseStats()->m_generalState &= ~STATE_INVISIBLE;
        pSprite->GetDerivedStats()->m_generalState &= ~STATE_INVISIBLE;

        pSprite->GetBaseStats()->m_generalState &= ~STATE_IMPROVEDINVISIBILITY;
        pSprite->GetDerivedStats()->m_generalState &= ~STATE_IMPROVEDINVISIBILITY;

        pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
            CGAMEEFFECT_INVISIBLE,
            pSprite->GetTimedEffectList()->GetPosCurrent(),
            -1);
        pSprite->GetEquipedEffectList()->RemoveAllOfType(pSprite,
            CGAMEEFFECT_INVISIBLE,
            pSprite->GetEquipedEffectList()->GetPosCurrent(),
            -1);
    }

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectShowArea::CGameEffectShowArea(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4987F0
CGameEffect* CGameEffectShowArea::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectShowArea* copy = new CGameEffectShowArea(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B8820
BOOL CGameEffectShowArea::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetArea()->m_visibility.SetAreaExplored();

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectShowCreatures::CGameEffectShowCreatures(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4988A0
CGameEffect* CGameEffectShowCreatures::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectShowCreatures* copy = new CGameEffectShowCreatures(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x799C90
BOOL CGameEffectShowCreatures::ApplyEffect(CGameSprite* pSprite)
{
    // NOTE: The vtable's slot +8 is the shared "return 1" (ret 4) that
    // identical code folding made of every such override; the effect works
    // through the code that looks for it, not here.
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectMirrorImage::CGameEffectMirrorImage(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x498950
CGameEffect* CGameEffectMirrorImage::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectMirrorImage* copy = new CGameEffectMirrorImage(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B8850
BOOL CGameEffectMirrorImage::ApplyEffect(CGameSprite* pSprite)
{
    if (pSprite->GetDerivedStats()->m_nMirrorImages < 8) {
        if (m_secondaryType != 0) {
            // NOTE: Uninline.
            DisplayStringRef(pSprite, 14774);
        }

        switch (m_dwFlags) {
        case 0:
            pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_MIRROR_IMAGE);
            if (m_secondaryType != 0) {
                m_effectAmount += rand() % 4 + 1;
            }
            break;
        case 1:
            pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_REFLECTED_IMAGE);
            if (m_secondaryType != 0) {
                m_effectAmount = 1;
            }
            break;
        }

        pSprite->GetDerivedStats()->m_generalState |= STATE_MIRRORIMAGE;

        m_effectAmount = min(m_effectAmount, 8);

        if (pSprite->GetDerivedStats()->m_nMirrorImages < m_effectAmount) {
            pSprite->GetDerivedStats()->m_nMirrorImages = m_effectAmount;

            if (m_secondaryType != 0) {
                CMessage* message = new CMessageVisualEffect(3,
                    static_cast<BYTE>(pSprite->GetDerivedStats()->m_nMirrorImages),
                    pSprite->GetId(),
                    pSprite->GetId());
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }
        }

        if (m_effectAmount == 0) {
            m_done = TRUE;
        }
    } else {
        m_done = TRUE;
    }
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectImmuneToWeapon::CGameEffectImmuneToWeapon(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x498AB0
CGameEffect* CGameEffectImmuneToWeapon::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectImmuneToWeapon* copy = new CGameEffectImmuneToWeapon(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B8A90
BOOL CGameEffectImmuneToWeapon::ApplyEffect(CGameSprite* pSprite)
{
    CWeaponIdentification* pWeaponId = new CWeaponIdentification();

    switch (m_dwFlags) {
    case 0:
        pWeaponId->m_itemType = 0;
        pWeaponId->m_itemFlags = 0;
        pWeaponId->m_itemFlagMask = 0;
        pWeaponId->m_attributes = m_effectAmount;
        break;
    case 1:
        pWeaponId->m_itemType = 0;
        pWeaponId->m_itemFlags = 0x40;
        pWeaponId->m_itemFlagMask = 0x40;
        pWeaponId->m_attributes = -1;
        break;
    case 2:
        pWeaponId->m_itemType = 0;
        pWeaponId->m_itemFlags = 0;
        pWeaponId->m_itemFlagMask = 0x40;
        pWeaponId->m_attributes = -1;
        break;
    case 3:
        pWeaponId->m_itemType = 0;
        pWeaponId->m_itemFlags = 0x100;
        pWeaponId->m_itemFlagMask = 0x100;
        pWeaponId->m_attributes = -1;
        break;
    case 4:
        pWeaponId->m_itemType = 0;
        pWeaponId->m_itemFlags = 0;
        pWeaponId->m_itemFlagMask = 0x100;
        pWeaponId->m_attributes = -1;
        break;
    case 5:
        pWeaponId->m_itemType = 0;
        pWeaponId->m_itemFlags = 0;
        pWeaponId->m_itemFlagMask = 0x100;
        pWeaponId->m_attributes = 0;
        break;
    case 6:
        pWeaponId->m_itemType = 0;
        pWeaponId->m_itemFlags = 0x2;
        pWeaponId->m_itemFlagMask = 0x2;
        pWeaponId->m_attributes = -1;
        break;
    case 7:
        pWeaponId->m_itemType = 0;
        pWeaponId->m_itemFlags = 0;
        pWeaponId->m_itemFlagMask = 0x2;
        pWeaponId->m_attributes = -1;
        break;
    case 8:
        pWeaponId->m_itemType = 0;
        pWeaponId->m_itemFlags = 0x10;
        pWeaponId->m_itemFlagMask = 0x10;
        pWeaponId->m_attributes = -1;
        break;
    case 9:
        pWeaponId->m_itemType = 0;
        pWeaponId->m_itemFlags = 0;
        pWeaponId->m_itemFlagMask = 0x10;
        pWeaponId->m_attributes = -1;
        break;
    case 0xA:
        pWeaponId->m_itemType = 0;
        pWeaponId->m_itemFlags = 0x200;
        pWeaponId->m_itemFlagMask = 0x200;
        pWeaponId->m_attributes = -1;
        break;
    case 0xB:
        pWeaponId->m_itemType = 0;
        pWeaponId->m_itemFlags = 0;
        pWeaponId->m_itemFlagMask = 0x200;
        pWeaponId->m_attributes = -1;
        break;
    }

    pSprite->GetDerivedStats()->m_cImmunitiesWeapon.AddTail(pWeaponId);

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectVisualAnimationEffect::CGameEffectVisualAnimationEffect(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x498B60
CGameEffect* CGameEffectVisualAnimationEffect::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectVisualAnimationEffect* copy = new CGameEffectVisualAnimationEffect(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x799C90
BOOL CGameEffectVisualAnimationEffect::ApplyEffect(CGameSprite* pSprite)
{
    // NOTE: The vtable's slot +8 is the shared "return 1" (ret 4) that
    // identical code folding made of every such override; the effect works
    // through the code that looks for it, not here.
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectCreateItem::CGameEffectCreateItem(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x498C10
CGameEffect* CGameEffectCreateItem::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectCreateItem* copy = new CGameEffectCreateItem(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B8BE0
BOOL CGameEffectCreateItem::ApplyEffect(CGameSprite* pSprite)
{
    // The item goes when the effect's time is up: a delayed Destroy Item
    // (effect 123) of the same resource.
    ITEM_EFFECT* effect = new ITEM_EFFECT;
    effect->effectID = CGAMEEFFECT_DESTROYITEM;
    effect->targetType = 0;
    effect->spellLevel = 0;
    effect->effectAmount = 0;
    effect->dwFlags = 0;
    effect->probabilityUpper = 100;
    effect->probabilityLower = 0;
    effect->numDice = 0;
    effect->diceSize = 0;
    effect->savingThrow = 0;
    effect->saveMod = 0;
    effect->special = 0;
    // NOTE: The original leaves the resref uninitialised (it is replaced
    // below).
    memset(effect->res, 0, sizeof(effect->res));
    effect->durationType = 4;
    effect->duration = (m_duration - g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime) / 15;

    CGameEffect* pDestroy = CGameEffect::DecodeEffect(effect, CPoint(-1, -1), -1, CPoint(-1, -1));
    pDestroy->m_res = m_res;

    if (!g_pBaldurChitin->cDimm.m_cKeyTable.FindKey(m_res, 1005, TRUE)) {
        // NOTE: The original leaks the decoded effect and the record here.
        m_done = TRUE;
        return TRUE;
    }

    INT nSlot;
    if (g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(pSprite->GetId()) == -1) {
        // A creature: the first free slot from 15 (quick items and pack).
        if (pSprite->GetObjectType() == CGameObject::TYPE_SPRITE) {
            for (nSlot = 0; nSlot < 28; nSlot++) {
                if (pSprite->m_equipment.m_items[CGameSpriteEquipment::SLOT_MISC + nSlot] == NULL) {
                    pSprite->m_equipment.m_items[CGameSpriteEquipment::SLOT_MISC + nSlot] = new CItem(m_res,
                        static_cast<WORD>(m_effectAmount),
                        static_cast<WORD>(m_effectAmount),
                        static_cast<WORD>(m_effectAmount),
                        0,
                        0);
                    break;
                }
            }
        }
    } else {
        // A party member: the first free pack slot (18 to 41).
        for (nSlot = 0; nSlot < 24; nSlot++) {
            if (pSprite->m_equipment.m_items[18 + nSlot] == NULL) {
                pSprite->m_equipment.m_items[18 + nSlot] = new CItem(m_res,
                    static_cast<WORD>(m_effectAmount),
                    static_cast<WORD>(m_effectAmount),
                    static_cast<WORD>(m_effectAmount),
                    0,
                    0);
                break;
            }
        }
    }

    if (m_durationType != 1) {
        pDestroy->m_casterLevel = m_casterLevel;
        pSprite->AddEffect(pDestroy, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);
    }

    m_done = TRUE;
    delete effect;
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectDestroyItem::CGameEffectDestroyItem(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x498CC0
CGameEffect* CGameEffectDestroyItem::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectDestroyItem* copy = new CGameEffectDestroyItem(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B9010
BOOL CGameEffectDestroyItem::ApplyEffect(CGameSprite* pSprite)
{
    INT nSlot = 0;
    while (pSprite->m_equipment.m_items[nSlot] == NULL
        || pSprite->m_equipment.m_items[nSlot]->cResRef != m_res) {
        nSlot++;
        if (nSlot > 50) {
            pSprite->field_562C = 1;
            m_done = TRUE;
            return TRUE;
        }
    }

    CItem* pItem = pSprite->m_equipment.m_items[nSlot];
    pSprite->m_equipment.m_items[nSlot] = NULL;
    pItem->Unequip(pSprite, nSlot, TRUE, FALSE);
    g_pBaldurChitin->GetObjectGame()->AddDisposableItem(pItem);

    pSprite->field_562C = 1;

    // A quick item of the selected character: its button empties.
    if (nSlot == 15 || nSlot == 16 || nSlot == 17) {
        CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
        INT nPortrait = pGame->GetCharacterPortraitNum(pSprite->GetId());
        if (g_pBaldurChitin->GetActiveEngine()->GetSelectedCharacter() == nPortrait) {
            // NOTE: Inlined CGameSprite::SetQuickItem.
            pSprite->SetQuickItem(static_cast<BYTE>(nSlot - 15), CButtonData());

            pGame->m_cButtonArray.UpdateState();
        }
    }

    m_done = TRUE;
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectTeleport::CGameEffectTeleport(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x498D70
CGameEffect* CGameEffectTeleport::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectTeleport* copy = new CGameEffectTeleport(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B92E0
BOOL CGameEffectTeleport::ApplyEffect(CGameSprite* pSprite)
{
    CGameObject* pObject;
    CGameSprite* pSource;
    CMessage* message;

    switch (m_dwFlags) {
    case 0:
        // To the effect's target point.
        pSprite->JumpToPoint(m_target, TRUE);
        m_done = TRUE;
        return TRUE;
    case 1:
        // The source to this sprite.
        if (g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(m_sourceID,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE)
            == CGameObjectArray::SUCCESS) {
            pSource = static_cast<CGameSprite*>(pObject);
            pSource->JumpToPoint(pSprite->GetPos(), TRUE);
            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(m_sourceID,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            m_done = TRUE;
            return TRUE;
        }
        break;
    case 2:
        // Back to the creature file's saved point and facing.
        pSprite->JumpToPoint(CPoint(pSprite->m_baseStats.field_2E4, pSprite->m_baseStats.field_2E6), TRUE);
        pSprite->SetDirection(static_cast<SHORT>(static_cast<char>(pSprite->m_baseStats.field_2E8)));
        m_done = TRUE;
        return TRUE;
    case 3:
        // The source and this sprite change places.
        if (g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(m_sourceID,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE)
            == CGameObjectArray::SUCCESS) {
            pSource = static_cast<CGameSprite*>(pObject);
            CPoint ptSource = pSource->GetPos();
            CPoint ptTarget = pSprite->GetPos();

            pSource->sub_75F3D0(1);
            if (g_pChitin->cNetwork.m_nServiceProvider != 0) {
                message = new CMessage90(pSource->GetId(), pSource->GetId(), 1);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }

            pSprite->sub_75F3D0(1);
            if (g_pChitin->cNetwork.m_nServiceProvider != 0) {
                message = new CMessage90(pSprite->GetId(), pSprite->GetId(), 1);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }

            pSource->JumpToPoint(ptTarget, TRUE);
            pSprite->JumpToPoint(ptSource, TRUE);

            pSource->sub_75F3D0(0);
            if (g_pChitin->cNetwork.m_nServiceProvider != 0) {
                message = new CMessage90(pSource->GetId(), pSource->GetId(), 0);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }

            pSprite->sub_75F3D0(0);
            if (g_pChitin->cNetwork.m_nServiceProvider != 0) {
                message = new CMessage90(pSprite->GetId(), pSprite->GetId(), 0);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(m_sourceID,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            m_done = TRUE;
            return TRUE;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 17926
        UTIL_ASSERT(FALSE);
        break;
    }

    m_done = TRUE;
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectKnock::CGameEffectKnock(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x498E20
CGameEffect* CGameEffectKnock::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectKnock* copy = new CGameEffectKnock(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B9670
BOOL CGameEffectKnock::ApplyEffect(CGameSprite* pSprite)
{
    m_done = TRUE;
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectMovementRate::CGameEffectMovementRate(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x498ED0
CGameEffect* CGameEffectMovementRate::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectMovementRate* copy = new CGameEffectMovementRate(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B9680
BOOL CGameEffectMovementRate::ApplyEffect(CGameSprite* pSprite)
{
    if (pSprite->GetAnimation()->GetMoveScale() != 0) {
        switch (m_dwFlags) {
        case 0:
            pSprite->GetAnimation()->SetMoveScale(pSprite->GetAnimation()->GetMoveScaleDefault() + static_cast<BYTE>(m_effectAmount));
            break;
        case 1:
            pSprite->GetAnimation()->SetMoveScale(static_cast<BYTE>(m_effectAmount));
            break;
        case 2:
            pSprite->GetAnimation()->SetMoveScale(pSprite->GetAnimation()->GetMoveScaleDefault() * static_cast<BYTE>(m_effectAmount) / 100);
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
            // __LINE__: 17997
            UTIL_ASSERT(FALSE);
        }
    }

    if (m_secondaryType != 0) {
        CMessage* message = new CMessageDropPath(pSprite->GetId(), pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x498F80
CGameEffectMovementRateWithPortrait::CGameEffectMovementRateWithPortrait(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x498FC0
CGameEffect* CGameEffectMovementRateWithPortrait::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectMovementRateWithPortrait* copy = new CGameEffectMovementRateWithPortrait(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B9870
BOOL CGameEffectMovementRateWithPortrait::ApplyEffect(CGameSprite* pSprite)
{
    if (pSprite->GetAnimation()->GetMoveScale() != 0) {
        switch (m_dwFlags) {
        case 0:
            pSprite->GetAnimation()->SetMoveScale(pSprite->GetAnimation()->GetMoveScaleDefault() + static_cast<BYTE>(m_effectAmount));
            break;
        case 1:
            pSprite->GetAnimation()->SetMoveScale(static_cast<BYTE>(m_effectAmount));
            break;
        case 2:
            pSprite->GetAnimation()->SetMoveScale(pSprite->GetAnimation()->GetMoveScaleDefault() * static_cast<BYTE>(m_effectAmount) / 100);
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
            // __LINE__: 18041
            UTIL_ASSERT(FALSE);
        }
    }

    // NOTE: Uninline.
    AddPortraitIcon(pSprite, 41);

    if (m_secondaryType != 0) {
        CMessage* message = new CMessageDropPath(pSprite->GetId(), pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectConfusion::CGameEffectConfusion(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x499380
CGameEffect* CGameEffectConfusion::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectConfusion* copy = new CGameEffectConfusion(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BA920
BOOL CGameEffectConfusion::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType) {
        // NOTE: Uninline.
        DisplayStringRef(pSprite, 14782);
    }

    // NOTE: Uninline.
    AddPortraitIcon(pSprite, 3);

    pSprite->sub_761650();

    if (m_durationType == 1) {
        pSprite->GetBaseStats()->m_generalState |= STATE_CONFUSED;
        pSprite->GetDerivedStats()->m_generalState |= STATE_CONFUSED;
        pSprite->ClearActions(FALSE);
        m_done = TRUE;
    } else {
        pSprite->GetDerivedStats()->m_generalState |= STATE_CONFUSED;
    }

    return TRUE;
}

// 0x4B5490
void CGameEffectConfusion::OnRemove(CGameSprite* pSprite)
{
    PlaySound(CResRef("EFF_E05"), pSprite);
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectNon_CumulativeAid::CGameEffectNon_CumulativeAid(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x499430
CGameEffect* CGameEffectNon_CumulativeAid::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectNon_CumulativeAid* copy = new CGameEffectNon_CumulativeAid(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BAA40
BOOL CGameEffectNon_CumulativeAid::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType) {
        // NOTE: Uninline.
        DisplayStringRef(pSprite, 14122);
    }

    if (!pSprite->GetDerivedStats()->m_spellStates.test(SPLSTATE_AID)) {
        pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_AID, true);

        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 57);

        AddColorEffect(pSprite, 50, 50, 50, 30);

        pSprite->GetDerivedStats()->m_nDamageBonus++;
        pSprite->GetDerivedStats()->m_nTHAC0++;
        pSprite->GetDerivedStats()->m_nSaveVSFortitude++;
        pSprite->GetDerivedStats()->m_nSaveVSReflex++;
        pSprite->GetDerivedStats()->m_nSaveVSWill++;
        pSprite->GetDerivedStats()->m_nMaxHitPoints += 8;

        if (m_secondaryType) {
            pSprite->GetBaseStats()->m_hitPoints += 8;
        }
    }

    return TRUE;
}

// 0x55ECF0
void CGameEffectNon_CumulativeAid::OnRemove(CGameSprite* pSprite)
{
    PlaySound(CResRef("EFF_E03"), pSprite);
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectNon_CumulativeBless::CGameEffectNon_CumulativeBless(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4994E0
CGameEffect* CGameEffectNon_CumulativeBless::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectNon_CumulativeBless* copy = new CGameEffectNon_CumulativeBless(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BAB70
BOOL CGameEffectNon_CumulativeBless::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType) {
        // NOTE: Uninline.
        DisplayStringRef(pSprite, 14123);

        // NOTE: Uninline.
        RemoveAllOfType(pSprite, ICEWIND_CGAMEEFFECT_BANE, -1);
    }

    if (!pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_BLESS]) {
        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 17);

        AddColorEffect(pSprite, 192, 128, 0, 30);

        pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_BLESS] = true;
        pSprite->GetDerivedStats()->m_nTHAC0++;
        pSprite->GetBaseStats()->m_morale++;
    }

    return TRUE;
}

// 0x55ECF0
void CGameEffectNon_CumulativeBless::OnRemove(CGameSprite* pSprite)
{
    PlaySound(CResRef("EFF_E03"), pSprite);
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectNon_CumulativeChant::CGameEffectNon_CumulativeChant(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x499590
CGameEffect* CGameEffectNon_CumulativeChant::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectNon_CumulativeChant* copy = new CGameEffectNon_CumulativeChant(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BACA0
BOOL CGameEffectNon_CumulativeChant::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        // NOTE: Uses `test` (with bounds check).
        if (!pSprite->GetDerivedStats()->m_spellStates.test(SPLSTATE_BENEFICIAL_CHANT)) {
            pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_BENEFICIAL_CHANT, true);

            // NOTE: Uninline.
            AddPortraitIcon(pSprite, 18);

            AddColorEffect(pSprite, 192, 128, 0, 30);

            pSprite->GetDerivedStats()->m_nDamageBonus++;
            pSprite->GetDerivedStats()->m_nTHAC0++;
            pSprite->GetDerivedStats()->m_nSaveVSFortitude++;
            pSprite->GetDerivedStats()->m_nSaveVSReflex++;
            pSprite->GetDerivedStats()->m_nSaveVSWill++;
        }
        break;
    case 1:
        // NOTE: Using `operator[]` (without bounds check).
        if (!pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_DETRIMENTAL_CHANT]) {
            pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_DETRIMENTAL_CHANT, true);

            // NOTE: Uninline.
            AddPortraitIcon(pSprite, 35);

            AddColorEffect(pSprite, 128, 0, 192, 30);

            pSprite->GetDerivedStats()->m_nDamageBonus--;
            pSprite->GetDerivedStats()->m_nTHAC0--;
            pSprite->GetDerivedStats()->m_nSaveVSFortitude--;
            pSprite->GetDerivedStats()->m_nSaveVSReflex--;
            pSprite->GetDerivedStats()->m_nSaveVSWill--;
        }
        break;
    }

    return TRUE;
}

// 0x55ECF0
void CGameEffectNon_CumulativeChant::OnRemove(CGameSprite* pSprite)
{
    PlaySound(CResRef("EFF_E03"), pSprite);
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectNon_CumulativeDrawUponHolyMight::CGameEffectNon_CumulativeDrawUponHolyMight(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x499640
CGameEffect* CGameEffectNon_CumulativeDrawUponHolyMight::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectNon_CumulativeDrawUponHolyMight* copy = new CGameEffectNon_CumulativeDrawUponHolyMight(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BAE10
BOOL CGameEffectNon_CumulativeDrawUponHolyMight::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType) {
        // NOTE: Uninline.
        DisplayStringRef(pSprite, 14125);
    }

    if (!pSprite->GetDerivedStats()->m_spellStates.test(SPLSTATE_DRAW_UPON_HOLY_MIGHT)) {
        pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_DRAW_UPON_HOLY_MIGHT, true);

        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 59);

        if (m_effectAmount > pSprite->GetDerivedStats()->field_128) {
            pSprite->m_bonusStats.m_nSTR += static_cast<SHORT>(m_effectAmount) - static_cast<SHORT>(pSprite->GetDerivedStats()->field_128);
            pSprite->GetDerivedStats()->field_128 = m_effectAmount;
        }

        if (m_effectAmount > pSprite->GetDerivedStats()->field_130) {
            pSprite->m_bonusStats.m_nCON += static_cast<SHORT>(m_effectAmount) - static_cast<SHORT>(pSprite->GetDerivedStats()->field_130);
            pSprite->GetDerivedStats()->field_130 = m_effectAmount;
        }

        if (m_effectAmount > pSprite->GetDerivedStats()->field_12C) {
            pSprite->m_bonusStats.m_nDEX += static_cast<SHORT>(m_effectAmount) - static_cast<SHORT>(pSprite->GetDerivedStats()->field_12C);
            pSprite->GetDerivedStats()->field_12C = m_effectAmount;
        }

        AddColorEffect(pSprite, 128, 128, 128, 30);

        m_done = FALSE;
    }

    return TRUE;
}

// 0x4BAF90
void CGameEffectNon_CumulativeDrawUponHolyMight::OnRemove(CGameSprite* pSprite)
{
    PlaySound(CResRef("EFF_E04"), pSprite);
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectNon_CumulativeLuck::CGameEffectNon_CumulativeLuck(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4996F0
CGameEffect* CGameEffectNon_CumulativeLuck::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectNon_CumulativeLuck* copy = new CGameEffectNon_CumulativeLuck(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BAFC0
BOOL CGameEffectNon_CumulativeLuck::ApplyEffect(CGameSprite* pSprite)
{
    // The exe sign-extends the amount's low byte (movsx at 0x4BAFC0), so a
    // negative luck lowers the stat.
    pSprite->GetDerivedStats()->m_nLuck += static_cast<signed char>(m_effectAmount);

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectPetrification::CGameEffectPetrification()
{
    m_effectID = CGAMEEFFECT_PETRIFICATION;
}

// NOTE: Inlined.
CGameEffectPetrification::CGameEffectPetrification(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4997A0
CGameEffect* CGameEffectPetrification::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectPetrification* copy = new CGameEffectPetrification(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BAFE0
BOOL CGameEffectPetrification::ApplyEffect(CGameSprite* pSprite)
{
    if (g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_cutScene != TRUE) {
        // FIXME: Should be on stack.
        ITEM_EFFECT* effect = new ITEM_EFFECT();
        effect->durationType = 1;
        effect->effectID = CGAMEEFFECT_DEATH;
        effect->targetType = 0;
        effect->spellLevel = 0;
        effect->effectAmount = 0;
        effect->probabilityUpper = 100;
        effect->probabilityLower = 0;
        effect->numDice = 0;
        effect->diceSize = 0;
        effect->savingThrow = 0;
        effect->saveMod = 0;
        effect->special = 0;
        effect->dwFlags = 0x10;
        effect->duration = 0;

        CGameEffect* pEffect = DecodeEffect(effect,
            pSprite->GetPos(),
            pSprite->GetId(),
            CPoint(-1, -1));

        CMessage* message = new CMessageAddEffect(pEffect,
            pSprite->GetId(),
            pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

        delete effect;
    }

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectPolymorph::CGameEffectPolymorph(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x499850
CGameEffect* CGameEffectPolymorph::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectPolymorph* copy = new CGameEffectPolymorph(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BB110
BOOL CGameEffectPolymorph::ApplyEffect(CGameSprite* pSprite)
{
    if (m_res == "") {
        pSprite->m_timedEffectList.RemoveAllOfType(pSprite, CGAMEEFFECT_POLYMORPH, pSprite->m_timedEffectList.m_posCurrent, -1);
        pSprite->m_equipedEffectList.RemoveAllOfType(pSprite, CGAMEEFFECT_POLYMORPH, pSprite->m_equipedEffectList.m_posCurrent, -1);
        sub_4B8730(pSprite, 42);
        m_forceRepass = TRUE;
        m_done = TRUE;
        return TRUE;
    }

    pSprite->SetIdleSequence();

    if (pSprite->m_pPath != NULL) {
        CMessage* message = new CMessageDropPath(pSprite->GetId(), pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    // The creature to take the shape of, made from its file.
    CCreatureFile creatureData;
    creatureData.SetResRef(m_res, TRUE, TRUE);

    LONG nSize = creatureData.GetDataSize();
    CGameSprite* pShape = new CGameSprite(creatureData.GetData(),
        nSize,
        0,
        0,
        -1,
        0,
        0,
        0x7FFFFFFF,
        CPoint(-1, -1),
        0);
    pShape->SetResRef(creatureData.GetResRef());
    creatureData.ReleaseData();

    if (pShape != NULL) {
        LONG nShapeId = pShape->GetId();

        pSprite->m_derivedStats.m_disabledSpellTypes[0] = TRUE;
        pSprite->m_derivedStats.m_disabledSpellTypes[1] = TRUE;
        for (INT nButton = 2; nButton <= 11; nButton++) {
            pSprite->m_derivedStats.m_disabledButtons[nButton] = TRUE;
        }

        CString sResRef;
        m_res.CopyToString(sResRef);
        pShape->SetResRef(CResRef(sResRef));

        BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(nShapeId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pShape),
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            // NOTE: Inlined CGameAnimation::GetListType (ObjAnimation.h line
            // 2086) and GetPersonalSpace (line 2093).
            if (pSprite->GetArea() != NULL) {
                if (pSprite->GetAnimation()->GetListType() != CGameObject::LIST_FLIGHT) {
                    pSprite->GetArea()->m_search.RemoveObject(CPoint(pSprite->GetPos().x / CPathSearch::GRID_SQUARE_SIZEX,
                                                                  pSprite->GetPos().y / CPathSearch::GRID_SQUARE_SIZEY),
                        pSprite->GetAIType().m_nEnemyAlly,
                        pSprite->GetAnimation()->GetPersonalSpace(),
                        pSprite->field_54A8,
                        pSprite->field_7430);
                }
            }

            // NOTE: Inlined CGameAnimation::GetAnimationId (ObjAnimation.h
            // line 2115) and SetAnimationType.
            pSprite->GetAnimation()->SetAnimationType(pShape->GetAnimation()->GetAnimationId(),
                pShape->m_baseStats.m_colors,
                pSprite->m_nDirection);
            pSprite->GetAnimation()->m_animation->SetSequence(6);

            if (pSprite->GetArea() != NULL) {
                if (pSprite->GetAnimation()->GetListType() != CGameObject::LIST_FLIGHT) {
                    pSprite->GetArea()->m_search.AddObject(CPoint(pSprite->GetPos().x / CPathSearch::GRID_SQUARE_SIZEX,
                                                               pSprite->GetPos().y / CPathSearch::GRID_SQUARE_SIZEY),
                        pSprite->GetAIType().m_nEnemyAlly,
                        pSprite->GetAnimation()->GetPersonalSpace(),
                        pSprite->field_54A8,
                        pSprite->field_7430);
                }
            }

            CMessage* message = new CMessageAnimationChange(pShape->GetAnimation()->GetAnimationId(),
                pSprite->GetId(),
                pSprite->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

            // The shape's armor class, attacks, resistances and physical
            // scores.
            pSprite->m_derivedStats.m_bPolymorphed = TRUE;
            pSprite->m_derivedStats.field_6 = pShape->m_derivedStats.field_6;
            pSprite->m_derivedStats.field_8 = pShape->m_derivedStats.field_8;
            pSprite->m_derivedStats.field_A = pShape->m_derivedStats.field_A;
            pSprite->m_derivedStats.field_C = pShape->m_derivedStats.field_C;
            pSprite->m_derivedStats.m_nACCrushingMod = pShape->m_derivedStats.m_nACCrushingMod;
            pSprite->m_derivedStats.m_nACMissileMod = pShape->m_derivedStats.m_nACMissileMod;
            pSprite->m_derivedStats.m_nACPiercingMod = pShape->m_derivedStats.m_nACPiercingMod;
            pSprite->m_derivedStats.m_nACSlashingMod = pShape->m_derivedStats.m_nACSlashingMod;
            pSprite->m_derivedStats.m_nCON = pShape->m_derivedStats.m_nCON;
            pSprite->m_derivedStats.m_nDEX = pShape->m_derivedStats.m_nDEX;
            pSprite->m_derivedStats.m_nSTR = pShape->m_derivedStats.m_nSTR;
            pSprite->m_derivedStats.m_nNumberOfAttacks = pShape->m_derivedStats.m_nNumberOfAttacks;
            pSprite->m_derivedStats.m_nResistFire = pShape->m_derivedStats.m_nResistFire;
            pSprite->m_derivedStats.m_nResistCold = pShape->m_derivedStats.m_nResistCold;
            pSprite->m_derivedStats.m_nResistElectricity = pShape->m_derivedStats.m_nResistElectricity;
            pSprite->m_derivedStats.m_nResistAcid = pShape->m_derivedStats.m_nResistAcid;
            pSprite->m_derivedStats.m_nResistMagic = pShape->m_derivedStats.m_nResistMagic;
            pSprite->m_derivedStats.m_nResistMagicFire = pShape->m_derivedStats.m_nResistMagicFire;
            pSprite->m_derivedStats.m_nResistMagicCold = pShape->m_derivedStats.m_nResistMagicCold;
            pSprite->m_derivedStats.m_nResistSlashing = pShape->m_derivedStats.m_nResistSlashing;
            pSprite->m_derivedStats.m_nResistCrushing = pShape->m_derivedStats.m_nResistCrushing;
            pSprite->m_derivedStats.m_nResistPiercing = pShape->m_derivedStats.m_nResistPiercing;
            pSprite->m_derivedStats.m_nResistMissile = pShape->m_derivedStats.m_nResistMissile;
            pSprite->m_derivedStats.m_nMagicDamageResistance = pShape->m_derivedStats.m_nMagicDamageResistance;
            pSprite->m_derivedStats.m_nResistPoison = pShape->m_derivedStats.m_nResistPoison;

            if (m_secondaryType) {
                g_pBaldurChitin->GetObjectGame()->m_cButtonArray.ResetState();
            }

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(nShapeId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);

            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Delete(nShapeId,
                CGameObjectArray::THREAD_ASYNCH,
                NULL,
                INFINITE);
            if (rc == CGameObjectArray::SUCCESS && pShape != NULL) {
                delete pShape;
            }

            pSprite->JumpToPoint(pSprite->GetPos(), TRUE);
            pSprite->SetIdleSequence();
        }
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectForceVisible::CGameEffectForceVisible(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x499900
CGameEffect* CGameEffectForceVisible::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectForceVisible* copy = new CGameEffectForceVisible(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B2990
BOOL CGameEffectForceVisible::ApplyEffect(CGameSprite* pSprite)
{
    if ((pSprite->GetBaseStats()->m_generalState & STATE_IMPROVEDINVISIBILITY) != 0
        || (pSprite->GetDerivedStats()->m_generalState & STATE_IMPROVEDINVISIBILITY) != 0) {
        pSprite->GetBaseStats()->field_2FC |= 0x1;
    } else {
        pSprite->GetBaseStats()->m_generalState &= ~STATE_INVISIBLE;
        pSprite->GetDerivedStats()->m_generalState &= ~STATE_INVISIBLE;
    }

    pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_INVISIBLE,
        pSprite->GetTimedEffectList()->GetPosCurrent(),
        0);
    pSprite->GetEquipedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_INVISIBLE,
        pSprite->GetEquipedEffectList()->GetPosCurrent(),
        0);

    pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_INVISIBLE,
        pSprite->GetTimedEffectList()->GetPosCurrent(),
        2);
    pSprite->GetEquipedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_INVISIBLE,
        pSprite->GetEquipedEffectList()->GetPosCurrent(),
        2);

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectSetSequence::CGameEffectSetSequence(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4999B0
CGameEffect* CGameEffectSetSequence::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectSetSequence* copy = new CGameEffectSetSequence(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BBC00
BOOL CGameEffectSetSequence::ApplyEffect(CGameSprite* pSprite)
{
    if ((pSprite->m_tempStats.m_generalState & STATE_DEAD) == 0) {
        pSprite->SetSequence(static_cast<BYTE>(m_dwFlags));

        m_done = TRUE;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectDisplayString::CGameEffectDisplayString(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x499A60
CGameEffect* CGameEffectDisplayString::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectDisplayString* copy = new CGameEffectDisplayString(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BBC40
BOOL CGameEffectDisplayString::ApplyEffect(CGameSprite* pSprite)
{
    STRREF str = static_cast<STRREF>(m_effectAmount);
    if (str != -1) {
        // NOTE: Uninline.
        DisplayStringRef(pSprite, str);
    }

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectVisualSpellHit::CGameEffectVisualSpellHit(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x499B30
CGameEffect* CGameEffectVisualSpellHit::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectVisualSpellHit* copy = new CGameEffectVisualSpellHit(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BBCB0
BOOL CGameEffectVisualSpellHit::ApplyEffect(CGameSprite* pSprite)
{
    CString sPalette;
    CString sSound;
    CString sAnimation;

    if (pSprite->GetArea() == NULL) {
        m_done = TRUE;
        return TRUE;
    }

    // NOTE: The original keeps the projectile type in its sprite argument;
    // a mode with no entry below leaves the sprite's address there and
    // fires that as the type: an address-dependent value, inert in single
    // player (CMessageFireProjectile fires nothing without a service
    // provider). Ours sends no message for such a mode (its local hit
    // still plays).
    LONG nProjectile = -1;

    CProjectile84DFA4* pBurst = NULL;
    USHORT nColor = 0;

    switch (m_dwFlags) {
    case 0x20:
        pBurst = new CProjectile84DFA4();
        nColor = 0x43;
        break;
    case 0x21:
        pBurst = new CProjectile84DFA4();
        nColor = 0x41;
        break;
    case 0x22:
        pBurst = new CProjectile84DFA4();
        nColor = 0x45;
        break;
    case 0x23: {
        CProjectileShaft* pShaft = new CProjectileShaft(CString("FlmStrk"),
            CString(""),
            0xFF,
            FALSE,
            FALSE,
            CTimerWorld::TIMESCALE_MSEC_PER_SEC * 2);
        pShaft->Fire(pSprite->GetArea(),
            pSprite->GetId(),
            pSprite->GetId(),
            pSprite->GetPos(),
            pShaft->DetermineHeight(pSprite),
            0);
        m_done = TRUE;
        return TRUE;
    }
    case 0x24: {
        CProjectileShaft* pShaft = new CProjectileShaft(CString("HLYMITE"),
            CString(""),
            0xFF,
            TRUE,
            FALSE,
            CTimerWorld::TIMESCALE_MSEC_PER_SEC * 2);
        pShaft->Fire(pSprite->GetArea(),
            pSprite->GetId(),
            pSprite->GetId(),
            pSprite->GetPos(),
            pShaft->DetermineHeight(pSprite),
            0);
        m_done = TRUE;
        return TRUE;
    }
    case 0x25:
        nProjectile = 0x93;
        sAnimation = "SHAir";
        sPalette = "SHAir7";
        sSound = "";
        break;
    case 0x26: {
        CProjectile84E940* pDoor = new CProjectile84E940(CString("DDoorH"),
            CString(""),
            CString(""),
            0,
            0);
        pDoor->Fire(pSprite->GetArea(),
            pSprite->GetId(),
            pSprite->GetId(),
            pSprite->GetPos(),
            0,
            0);
        m_done = TRUE;
        return TRUE;
    }
    case 0x27:
        sAnimation = "SPFDEATH";
        nProjectile = 0xCF;
        sPalette = "";
        sSound = "";
        break;
    default:
        switch (m_dwFlags) {
        case 0x0:
            nProjectile = 0x76;
            break;
        case 0x1:
            nProjectile = 0x77;
            break;
        case 0x2:
            nProjectile = 0x78;
            break;
        case 0x4:
            nProjectile = 0x79;
            break;
        case 0x5:
            nProjectile = 0x7A;
            break;
        case 0x6:
            nProjectile = 0x7B;
            break;
        case 0x8:
            nProjectile = 0x7C;
            break;
        case 0x9:
            nProjectile = 0x7D;
            break;
        case 0xA:
            nProjectile = 0x7E;
            break;
        case 0xC:
            nProjectile = 0x7F;
            break;
        case 0xD:
            nProjectile = 0x80;
            break;
        case 0xE:
            nProjectile = 0x81;
            break;
        case 0x10:
            nProjectile = 0x82;
            break;
        case 0x11:
            nProjectile = 0x83;
            break;
        case 0x12:
            nProjectile = 0x84;
            break;
        case 0x14:
            nProjectile = 0x85;
            break;
        case 0x15:
            nProjectile = 0x86;
            break;
        case 0x16:
            nProjectile = 0x87;
            break;
        case 0x18:
            nProjectile = 0x88;
            break;
        case 0x19:
            nProjectile = 0x89;
            break;
        case 0x1A:
            nProjectile = 0x8A;
            break;
        case 0x1C:
            nProjectile = 0x8B;
            break;
        case 0x1D:
            nProjectile = 0x8C;
            break;
        case 0x1E:
            nProjectile = 0x8D;
            break;
        }

        // The low two bits pick air, earth or water, the rest the palette
        // (none for 0, else "SHAir1".."SHAir7" and so on).
        switch (m_dwFlags & 3) {
        case 0:
            sAnimation = "SHAir";
            if ((m_dwFlags & ~3) != 0) {
                sPalette = "SHAir0";
                sPalette.SetAt(sPalette.GetLength() - 1, static_cast<char>((m_dwFlags >> 2) + '0'));
            } else {
                sPalette = "";
            }
            sSound = "";
            break;
        case 1:
            sAnimation = "SHEarth";
            if ((m_dwFlags & ~3) != 0) {
                sPalette = "SHEarth0";
                sPalette.SetAt(sPalette.GetLength() - 1, static_cast<char>((m_dwFlags >> 2) + '0'));
            } else {
                sPalette = "";
            }
            sSound = "";
            break;
        case 2:
            sAnimation = "SHWater";
            if ((m_dwFlags & ~3) != 0) {
                sPalette = "SHWater0";
                sPalette.SetAt(sPalette.GetLength() - 1, static_cast<char>((m_dwFlags >> 2) + '0'));
            } else {
                sPalette = "";
            }
            sSound = "";
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
            // __LINE__: 19999
            UTIL_ASSERT(FALSE);
            break;
        }
        break;
    }

    if (pBurst != NULL) {
        // A burst of the colour at the sprite, two pixels up.
        pBurst->SetColorRange(0, nColor);
        CPoint pos = pSprite->GetPos();
        pBurst->PlayAt(pSprite->GetArea(), CPoint(pos.x, pos.y - 2));
        m_done = TRUE;
        return TRUE;
    }

    // The hit plays here (a local projectile) and, through the message, the
    // projectile of the type on the other machines.
    CProjectile84E9C0* pHit = new CProjectile84E9C0(sAnimation, sSound, sPalette, 8, 0);

    if (nProjectile != -1) {
        CMessage* message = new CMessageFireProjectile(static_cast<WORD>(nProjectile),
            pSprite->GetId(),
            pSprite->GetPos(),
            pHit->DetermineHeight(pSprite),
            pSprite->GetId(),
            pSprite->GetId(),
            0);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    pHit->Fire(pSprite->GetArea(),
        pSprite->GetId(),
        pSprite->GetId(),
        pSprite->GetPos(),
        pHit->DetermineHeight(pSprite),
        0);

    m_done = TRUE;
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectPortraitIcon::CGameEffectPortraitIcon(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x499C00
CGameEffect* CGameEffectPortraitIcon::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectPortraitIcon* copy = new CGameEffectPortraitIcon(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BC500
BOOL CGameEffectPortraitIcon::ApplyEffect(CGameSprite* pSprite)
{
    // NOTE: Uninline.
    AddPortraitIcon(pSprite, m_dwFlags);

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectReplaceItem::CGameEffectReplaceItem(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x499CB0
CGameEffect* CGameEffectReplaceItem::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectReplaceItem* copy = new CGameEffectReplaceItem(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B8E70
BOOL CGameEffectReplaceItem::ApplyEffect(CGameSprite* pSprite)
{
    ITEM_EFFECT* effect = new ITEM_EFFECT;
    effect->effectID = CGAMEEFFECT_DESTROYITEM;
    effect->targetType = 0;
    effect->spellLevel = 0;
    effect->effectAmount = 0;
    effect->dwFlags = 0;
    effect->probabilityUpper = 100;
    effect->probabilityLower = 0;
    effect->numDice = 0;
    effect->diceSize = 0;
    effect->savingThrow = 0;
    effect->saveMod = 0;
    effect->special = 0;
    // NOTE: The original never sets the resref of this Destroy Item, so it
    // names whatever the new block held and finds nothing; here it is
    // empty, which finds nothing either.
    memset(effect->res, 0, sizeof(effect->res));
    effect->durationType = 4;
    effect->duration = (m_duration - g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime) / 15;

    CGameEffect* pDestroy = CGameEffect::DecodeEffect(effect, CPoint(-1, -1), -1, CPoint(-1, -1));

    CItem* pItem = new CItem(m_res, 0, 0, 0, 0, 0);
    if (pItem == NULL) {
        return TRUE;
    }

    if (pSprite->m_equipment.m_items[m_effectAmount] != NULL) {
        pSprite->Unequip(static_cast<SHORT>(m_effectAmount));
        if (pSprite->m_equipment.m_items[m_effectAmount] != NULL) {
            delete pSprite->m_equipment.m_items[m_effectAmount];
        }
    }

    pSprite->m_equipment.m_items[m_effectAmount] = pItem;
    pSprite->Equip(static_cast<SHORT>(m_effectAmount));

    if (m_durationType != 1) {
        pDestroy->m_casterLevel = m_casterLevel;
        pSprite->AddEffect(pDestroy, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);
    }

    m_done = TRUE;
    delete effect;
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectDisableButton::CGameEffectDisableButton(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x499D60
CGameEffect* CGameEffectDisableButton::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectDisableButton* copy = new CGameEffectDisableButton(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BC560
BOOL CGameEffectDisableButton::ApplyEffect(CGameSprite* pSprite)
{
    unsigned int buttonIndex = static_cast<unsigned int>(m_dwFlags);
    if (buttonIndex < 40) {
        pSprite->GetDerivedStats()->m_disabledButtons[buttonIndex] = TRUE;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectDisableSpellType::CGameEffectDisableSpellType(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x499E10
CGameEffect* CGameEffectDisableSpellType::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectDisableSpellType* copy = new CGameEffectDisableSpellType(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BC580
BOOL CGameEffectDisableSpellType::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        pSprite->GetDerivedStats()->m_disabledSpellTypes[0] = TRUE;
        pSprite->GetDerivedStats()->m_disabledSpellTypes[1] = TRUE;
        pSprite->GetDerivedStats()->m_disabledSpellTypes[2] = TRUE;
        break;
    case 1:
        pSprite->GetDerivedStats()->m_disabledSpellTypes[0] = TRUE;
        pSprite->GetDerivedStats()->m_disabledSpellTypes[1] = TRUE;
        break;
    case 2:
        pSprite->GetDerivedStats()->m_disabledSpellTypes[0] = TRUE;
        break;
    case 3:
        pSprite->GetDerivedStats()->m_disabledSpellTypes[1] = TRUE;
        break;
    case 4:
        pSprite->GetDerivedStats()->m_disabledSpellTypes[2] = TRUE;
        break;
    }

    if (g_pBaldurChitin->GetActiveEngine()->GetSelectedCharacter() == g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(pSprite->GetId())) {
        g_pBaldurChitin->GetObjectGame()->m_cButtonArray.UpdateState();
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectCastSpell::CGameEffectCastSpell(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x499EC0
CGameEffect* CGameEffectCastSpell::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectCastSpell* copy = new CGameEffectCastSpell(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BC630
BOOL CGameEffectCastSpell::ApplyEffect(CGameSprite* pSprite)
{
    CAIObjectType target = pSprite->GetAIType();

    // NOTE: Uninline.
    CAIAction action(CAIAction::FORCESPELL, target, 0, 0, 0);

    CString str;
    m_res.CopyToString(str);
    action.SetString1(str);

    CMessageInsertAction* pMessage = new CMessageInsertAction(action,
        pSprite->GetId(),
        m_sourceID);
    g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectLearnSpell::CGameEffectLearnSpell(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x499F70
CGameEffect* CGameEffectLearnSpell::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectLearnSpell* copy = new CGameEffectLearnSpell(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BC8C0
BOOL CGameEffectLearnSpell::ApplyEffect(CGameSprite* pSprite)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // The chance from INTMOD.2DA: column 0 (the SHORT at 0x84EA6A), the row
    // of the sprite's intelligence.
    // NOTE: Inlined C2DArray::GetAt.
    CString sChance(pGame->GetRuleTables().m_tIntelligenceMod.GetAt(CPoint(0, pSprite->m_tempStats.m_nINT)));
    BYTE nChance = static_cast<BYTE>(atol(sChance));

    INT nRoll = CUtil::UtilRandInt(100, -pSprite->m_tempStats.m_nLuck);

    CSpell* pSpell = new CSpell(m_res);
    if (pSpell == NULL) {
        m_done = TRUE;
        return TRUE;
    }

    UINT nIndex = 0;
    UINT nID = 0;
    if (!pGame->m_spells.Find(m_res, nID)) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 20230
        UTIL_ASSERT_MSG(FALSE, "Invalid spell");

        delete pSpell;
        m_done = TRUE;
        return FALSE;
    }

    // The spell's level in the wizard list (class 11).
    BYTE nClass = 11;
    CGroupedSpellList* pSpells = &(pGame->m_spellsByClass[pGame->GetSpellcasterIndex(nClass)]);

    UINT nSpellLevel = 0;
    UINT nLevel;
    for (nLevel = 0; nLevel < 9; nLevel++) {
        // __FILE__: .\Include\InfGame.h
        // __LINE__: 772
        UTIL_ASSERT(nLevel < CSPELLLIST_MAX_LEVELS);

        if (pSpells->m_lists[nLevel].GetSpellLevel(nID, nIndex) == TRUE) {
            break;
        }
    }

    if (nLevel < 9) {
        // __FILE__: .\Include\InfGame.h
        // __LINE__: 772
        UTIL_ASSERT(nLevel < CSPELLLIST_MAX_LEVELS);

        // NOTE: An inlined accessor of the entry's level (its asserts are
        // CSpellList::Get's, lines 724 and 725).
        // __FILE__: .\Include\InfGame.h
        // __LINE__: 724
        UTIL_ASSERT(nIndex < pSpells->m_lists[nLevel].m_nCount);

        // __FILE__: .\Include\InfGame.h
        // __LINE__: 725
        UTIL_ASSERT(pSpells->m_lists[nLevel].m_pList != NULL);

        nSpellLevel = pSpells->m_lists[nLevel].m_pList[nIndex].m_nLevel;
    }

    if (nSpellLevel == 0) {
        delete pSpell;
        m_done = TRUE;
        return FALSE;
    }

    unsigned int nCasterType = pSpell->GetCasterType();

    // A specialist learns his school more easily and other schools less.
    if ((pSprite->m_baseStats.m_specialization & 0x4000) == 0) {
        if (pGame->GetRuleTables().MapCharacterSpecializationToSchool(static_cast<WORD>(pSprite->m_baseStats.m_specialization)) == pSpell->GetSchool()) {
            nChance += 15;
        } else {
            nChance -= 15;
        }
    }

    LONG nDifficulty;
    if (g_pChitin->cNetwork.m_bConnectionEstablished == TRUE) {
        nDifficulty = pGame->GetOptions()->m_nMPDifficultyMultiplier;
    } else {
        nDifficulty = pGame->GetOptions()->m_nDifficultyMultiplier;
    }

    if (nDifficulty < 0 || nRoll < nChance) {
        switch (pGame->GetSpellType(m_res)) {
        case 1:
            if (nCasterType > 0 && nCasterType <= 2) {
                BYTE nKnownClass = 11;
                UINT nKnownLevel = nSpellLevel - 1;
                unsigned int a4 = 0;
                unsigned int a5 = 0;
                unsigned int a6 = 0;
                pSprite->AddKnownSpell(nKnownClass, nKnownLevel, m_res, a4, a5, a6);
            }
            break;
        case 2:
            if (1) {
                unsigned int a2 = 0;
                unsigned int a3 = 0;
                unsigned int a4 = 0;
                pSprite->AddInnateSpell(m_res, a2, a3, a4);
            }
            break;
        case 3:
            if (1) {
                unsigned int a2 = 1;
                unsigned int a3 = 1;
                unsigned int a4 = 0;
                pSprite->AddSong(m_res, a2, a3, a4);
            }
            break;
        case 4:
            if (1) {
                unsigned int a2 = 0;
                pSprite->AddShapeshift(m_res, a2);
            }
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
            // __LINE__: 20320
            UTIL_ASSERT(FALSE);
            break;
        }
    }

    delete pSpell;
    m_done = TRUE;
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectCastSpellPoint::CGameEffectCastSpellPoint(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49A020
CGameEffect* CGameEffectCastSpellPoint::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectCastSpellPoint* copy = new CGameEffectCastSpellPoint(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BCDC0
BOOL CGameEffectCastSpellPoint::ApplyEffect(CGameSprite* pSprite)
{
    CAIObjectType target = pSprite->GetAIType();

    // NOTE: Uninline.
    CAIAction action(CAIAction::FORCESPELLPOINT, target, 0, 0, 0);

    CString str;
    m_res.CopyToString(str);
    action.SetString1(str);

    action.m_specificID = m_effectAmount;
    action.m_dest = m_target;

    CMessageInsertAction* pMessage = new CMessageInsertAction(action,
        pSprite->GetId(),
        m_sourceID);
    g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectIdentify::CGameEffectIdentify(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49A0D0
CGameEffect* CGameEffectIdentify::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectIdentify* copy = new CGameEffectIdentify(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B9670
BOOL CGameEffectIdentify::ApplyEffect(CGameSprite* pSprite)
{
    // NOTE: Folded with CGameEffectKnock::ApplyEffect (the same address).
    m_done = TRUE;
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectDetectTraps::CGameEffectDetectTraps(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49A180
CGameEffect* CGameEffectDetectTraps::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectDetectTraps* copy = new CGameEffectDetectTraps(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B9670
BOOL CGameEffectDetectTraps::ApplyEffect(CGameSprite* pSprite)
{
    // NOTE: Folded with CGameEffectKnock::ApplyEffect (the same address).
    m_done = TRUE;
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectReplaceSelf::CGameEffectReplaceSelf(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49A230
CGameEffect* CGameEffectReplaceSelf::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectReplaceSelf* copy = new CGameEffectReplaceSelf(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BD050
BOOL CGameEffectReplaceSelf::ApplyEffect(CGameSprite* pSprite)
{
    // The creature m_res names takes the sprite's place: the sprite dies (a
    // party member always, a creature only with gore and flags; otherwise it
    // just leaves the area) and the new creature is added where it stood,
    // under its script name in the area's named creatures.
    if (!g_pBaldurChitin->cDimm.m_cKeyTable.FindKey(m_res, 1009, TRUE)) {
        m_done = TRUE;
        return TRUE;
    }

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    CGameEffect* pDeath = NULL;
    if (pGame->GetCharacterPortraitNum(pSprite->GetId()) == -1) {
        if (m_dwFlags != 0 && pGame->GetOptions()->m_nGore != 0) {
            pDeath = new CGameEffectDeath();
            pDeath->m_source = m_source;
            pDeath->m_sourceID = m_sourceID;
            pDeath->m_dwFlags = 8;
            pDeath->m_effectAmount = 1;
            pDeath->m_casterLevel = m_casterLevel;
        } else {
            pSprite->m_removeFromArea = TRUE;
        }
    } else {
        pDeath = new CGameEffectDeath();
        pDeath->m_source = m_source;
        pDeath->m_sourceID = m_sourceID;
        pDeath->m_dwFlags = 8;
    }

    if (pDeath != NULL) {
        pSprite->AddEffect(pDeath, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);
    }

    CPoint pos = pSprite->GetPos();

    CCreatureFile creatureFile;

    // NOTE: Uninline.
    creatureFile.SetResRef(m_res, TRUE, TRUE);

    // NOTE: The original evaluates the size first.
    DWORD nSize = creatureFile.GetDataSize();
    BYTE* pData = creatureFile.GetData();

    CGameSprite* pNewSprite = new CGameSprite(pData,
        nSize,
        0,
        0,
        -1,
        0,
        0,
        0x7FFFFFFF,
        CPoint(-1, -1),
        0);

    pNewSprite->SetResRef(CResRef(creatureFile.GetResRef().GetResRefStr()));

    creatureFile.ReleaseData();

    if (pNewSprite != NULL) {
        LONG nNewId = pNewSprite->GetId();

        CString sResRef;
        m_res.CopyToString(sResRef);
        pNewSprite->SetResRef(CResRef(sResRef));

        CGameObject* pObject;
        if (pGame->GetObjectArray()->GetDeny(nNewId,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE)
            == CGameObjectArray::SUCCESS) {
            pNewSprite = static_cast<CGameSprite*>(pObject);

            // NOTE: Inlined CGameAnimation::GetListType (ObjAnimation.h line
            // 2086 asserts m_animation).
            BYTE nListType = pNewSprite->GetAnimation()->m_animation->GetListType();
            pNewSprite->AddToArea(pSprite->GetArea(), pos, 0, nListType);

            CVariable cVariable;
            cVariable.SetName(CString(reinterpret_cast<const char*>(pNewSprite->GetScriptName())));
            cVariable.m_intValue = nNewId;
            pSprite->GetArea()->m_namedCreatures.AddKey(cVariable);

            pGame->GetObjectArray()->ReleaseDeny(nNewId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);

            m_done = TRUE;
        }
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectPlayMovie::CGameEffectPlayMovie(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49A2E0
CGameEffect* CGameEffectPlayMovie::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectPlayMovie* copy = new CGameEffectPlayMovie(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BD540
BOOL CGameEffectPlayMovie::ApplyEffect(CGameSprite* pSprite)
{
    CString sFileName("");

    if (g_pBaldurChitin->m_pEngineProjector->ResolveMovieFileName(m_res, sFileName) == TRUE) {
        g_pBaldurChitin->m_pEngineWorld->ReadyMovie(m_res, FALSE);
    }

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectSanctuary::CGameEffectSanctuary(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x496340
CGameEffect* CGameEffectSanctuary::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectSanctuary* copy = new CGameEffectSanctuary(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BD5E0
BOOL CGameEffectSanctuary::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetDerivedStats()->m_visualEffects |= IWD_VFX_SANCTUARY;
    pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_SANCTUARY);

    // NOTE: Uninline.
    AddPortraitIcon(pSprite, 91);

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectEntangle::CGameEffectEntangle(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4963F0
CGameEffect* CGameEffectEntangle::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectEntangle* copy = new CGameEffectEntangle(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BD650
BOOL CGameEffectEntangle::ApplyEffect(CGameSprite* pSprite)
{
    if (g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_cutScene != TRUE) {
        pSprite->GetDerivedStats()->m_visualEffects[IWD_VFX_ENTANGLE] = true;

        if (m_secondaryType) {
            PlaySound(CResRef("CRE_P01"), pSprite);
        }

        if (!pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_ENTANGLE]) {
            pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_ENTANGLE] = true;

            // NOTE: Uninline.
            AddPortraitIcon(pSprite, 92);

            pSprite->GetDerivedStats()->m_nTHAC0 -= 2;
            AdjustDEX(pSprite, -4);
            Immobilize(pSprite);
        }
    } else {
        m_done = TRUE;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectWebHold::CGameEffectWebHold(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4964A0
CGameEffect* CGameEffectWebHold::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectWebHold* copy = new CGameEffectWebHold(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BD740
BOOL CGameEffectWebHold::ApplyEffect(CGameSprite* pSprite)
{
    if (g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_cutScene != TRUE) {
        if (!pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_WEB]) {
            pSprite->GetDerivedStats()->m_visualEffects[IWD_VFX_WEB] = true;

            if (m_secondaryType) {
                PlaySound(CResRef("CRE_M02"), pSprite);
            }

            pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_WEB] = true;

            AdjustDEX(pSprite, -4);

            // NOTE: Uninline
            Immobilize(pSprite);

            // NOTE: Uninline.
            AddPortraitIcon(pSprite, 95);

            // FIXME: Redundant, path already dropped in `Immobilize`.
            if (m_secondaryType) {
                CMessageDropPath* pMessage = new CMessageDropPath(pSprite->GetId(),
                    pSprite->GetId());
                g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
            }
        }
    } else {
        m_done = TRUE;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectGreasePool::CGameEffectGreasePool(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x496550
CGameEffect* CGameEffectGreasePool::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectGreasePool* copy = new CGameEffectGreasePool(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BD920
BOOL CGameEffectGreasePool::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetDerivedStats()->m_visualEffects.set(IWD_VFX_GREASE, true);

    if (m_secondaryType) {
        PlaySound(CResRef("CRE_M01"), pSprite);
    }

    pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_GREASE, true);

    // NOTE: Uninline.
    AddPortraitIcon(pSprite, 94);

    if (pSprite->GetAnimation()->GetMoveScale() != 0) {
        pSprite->GetAnimation()->SetMoveScale(0);
    }

    if (m_secondaryType) {
        CMessageDropPath* pMessage = new CMessageDropPath(pSprite->GetId(),
            pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectMirrorImageRun::CGameEffectMirrorImageRun(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x498A00
CGameEffect* CGameEffectMirrorImageRun::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectMirrorImageRun* copy = new CGameEffectMirrorImageRun(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B89E0
BOOL CGameEffectMirrorImageRun::ApplyEffect(CGameSprite* pSprite)
{
    if ((pSprite->GetBaseStats()->m_generalState & STATE_DEAD) == 0) {
        pSprite->GetDerivedStats()->m_generalState |= STATE_MIRRORIMAGE;
        if (pSprite->GetDerivedStats()->m_nMirrorImages != m_effectAmount
            || pSprite->m_bForceVisualEffects) {
            CMessage* message = new CMessageVisualEffect(3,
                static_cast<BYTE>(m_effectAmount),
                pSprite->GetId(),
                pSprite->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }

        if (m_effectAmount == 0) {
            m_done = TRUE;
        }
    } else {
        m_done = TRUE;
    }
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectDispelSanctuary::CGameEffectDispelSanctuary(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49A390
CGameEffect* CGameEffectDispelSanctuary::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectDispelSanctuary* copy = new CGameEffectDispelSanctuary(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BDA60
BOOL CGameEffectDispelSanctuary::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_SANCTUARY,
        pSprite->GetTimedEffectList()->GetPosCurrent(),
        -1);
    pSprite->GetEquipedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_SANCTUARY,
        pSprite->GetEquipedEffectList()->GetPosCurrent(),
        -1);

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x494920
CGameEffectRemovePanic::CGameEffectRemovePanic(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49A440
CGameEffect* CGameEffectRemovePanic::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectRemovePanic* copy = new CGameEffectRemovePanic(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BDAB0
BOOL CGameEffectRemovePanic::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetBaseStats()->m_generalState &= ~STATE_PANIC;
    pSprite->GetDerivedStats()->m_generalState &= ~STATE_PANIC;

    pSprite->GetEquipedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_PANIC,
        pSprite->GetEquipedEffectList()->GetPosCurrent(),
        -1);
    pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_PANIC,
        pSprite->GetTimedEffectList()->GetPosCurrent(),
        -1);

    pSprite->GetEquipedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_PORTRAITICON,
        pSprite->GetEquipedEffectList()->GetPosCurrent(),
        36);
    pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_PORTRAITICON,
        pSprite->GetTimedEffectList()->GetPosCurrent(),
        36);

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectRemoveHold::CGameEffectRemoveHold(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49A4F0
CGameEffect* CGameEffectRemoveHold::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectRemoveHold* copy = new CGameEffectRemoveHold(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BDB50
BOOL CGameEffectRemoveHold::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetEquipedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_HOLDCREATURE,
        pSprite->GetEquipedEffectList()->GetPosCurrent(),
        -1);
    pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_HOLDCREATURE,
        pSprite->GetTimedEffectList()->GetPosCurrent(),
        -1);

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectRestoreMovementRate::CGameEffectRestoreMovementRate(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49A5A0
CGameEffect* CGameEffectRestoreMovementRate::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectRestoreMovementRate* copy = new CGameEffectRestoreMovementRate(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BDBA0
BOOL CGameEffectRestoreMovementRate::ApplyEffect(CGameSprite* pSprite)
{
    // NOTE: Uninline.
    pSprite->GetAnimation()->ResetMoveScale();

    pSprite->GetEquipedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_MOVEMENTRATE_176,
        pSprite->GetEquipedEffectList()->GetPosCurrent(),
        -1);
    pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_MOVEMENTRATE_176,
        pSprite->GetTimedEffectList()->GetPosCurrent(),
        -1);

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectRemoveIntoxication::CGameEffectRemoveIntoxication(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49A650
CGameEffect* CGameEffectRemoveIntoxication::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectRemoveIntoxication* copy = new CGameEffectRemoveIntoxication(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BDC20
BOOL CGameEffectRemoveIntoxication::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetBaseStats()->m_intoxication = 0;
    pSprite->GetDerivedStats()->m_nIntoxication = 0;

    pSprite->GetEquipedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_INTOXICATION,
        pSprite->GetEquipedEffectList()->GetPosCurrent(),
        -1);
    pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_INTOXICATION,
        pSprite->GetTimedEffectList()->GetPosCurrent(),
        -1);

    pSprite->GetEquipedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_PORTRAITICON,
        pSprite->GetEquipedEffectList()->GetPosCurrent(),
        5);
    pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_PORTRAITICON,
        pSprite->GetTimedEffectList()->GetPosCurrent(),
        5);

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectPauseCaster::CGameEffectPauseCaster(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49A700
CGameEffect* CGameEffectPauseCaster::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectPauseCaster* copy = new CGameEffectPauseCaster(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BDCB0
BOOL CGameEffectPauseCaster::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetDerivedStats()->m_bCasterHold = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectResistanceToMagic::CGameEffectResistanceToMagic(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49A7B0
CGameEffect* CGameEffectResistanceToMagic::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectResistanceToMagic* copy = new CGameEffectResistanceToMagic(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x04BDCD0
BOOL CGameEffectResistanceToMagic::ApplyEffect(CGameSprite* pSprite)
{
    if (m_effectAmount > 0) {
        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 28);
    }

    switch (m_dwFlags) {
    case 0:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_resistMagicBase += static_cast<signed char>(m_effectAmount);
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->GetDerivedStats()->m_nResistMagic += static_cast<SHORT>(m_effectAmount);
        }
        break;
    case 1:
        if (m_durationType == 1) {
            pSprite->GetBaseStats()->m_resistMagicBase = static_cast<signed char>(m_effectAmount);
            m_forceRepass = TRUE;
            m_done = TRUE;
        } else {
            pSprite->GetDerivedStats()->m_nResistMagic = static_cast<SHORT>(m_effectAmount);
        }
        break;
    }

    if (pSprite->GetBaseStats()->m_resistMagicBase < 0) {
        pSprite->GetBaseStats()->m_resistMagicBase = 0;
    } else if (pSprite->GetBaseStats()->m_resistMagicBase > 50) {
        pSprite->GetBaseStats()->m_resistMagicBase = 50;
    }

    if (pSprite->GetDerivedStats()->m_nResistMagic < 0) {
        pSprite->GetDerivedStats()->m_nResistMagic = 0;
    } else if (pSprite->GetDerivedStats()->m_nResistMagic > 50) {
        pSprite->GetDerivedStats()->m_nResistMagic = 50;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectMissileTHAC0Bonus::CGameEffectMissileTHAC0Bonus(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49A860
CGameEffect* CGameEffectMissileTHAC0Bonus::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectMissileTHAC0Bonus* copy = new CGameEffectMissileTHAC0Bonus(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BDE00
BOOL CGameEffectMissileTHAC0Bonus::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetDerivedStats()->m_nMissileTHAC0Bonus = static_cast<BYTE>(m_effectAmount);

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectDestroySelf::CGameEffectDestroySelf(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49A910
CGameEffect* CGameEffectDestroySelf::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectDestroySelf* copy = new CGameEffectDestroySelf(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BDDE0
BOOL CGameEffectDestroySelf::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->m_removeFromArea = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectImmunityToPortraitIcon::CGameEffectImmunityToPortraitIcon(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49A9C0
CGameEffect* CGameEffectImmunityToPortraitIcon::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectImmunityToPortraitIcon* copy = new CGameEffectImmunityToPortraitIcon(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B7A10
BOOL CGameEffectImmunityToPortraitIcon::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetEquipedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_PORTRAITICON,
        pSprite->GetEquipedEffectList()->GetPosCurrent(),
        m_dwFlags);
    pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
        CGAMEEFFECT_PORTRAITICON,
        pSprite->GetTimedEffectList()->GetPosCurrent(),
        m_dwFlags);

    ITEM_EFFECT* effect = new ITEM_EFFECT;
    effect->effectID = CGAMEEFFECT_PORTRAITICON;
    effect->dwFlags = m_dwFlags;

    CPoint target(-1, -1);
    CPoint source(-1, -1);

    CGameEffect* pEffect = DecodeEffect(effect, source, -1, target);
    pEffect->m_compareIdAndFlagsOnly = TRUE;

    pSprite->GetDerivedStats()->m_cImmunitiesEffect.AddTail(pEffect);

    delete effect;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectDamageVisualEffect::CGameEffectDamageVisualEffect(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49AA70
CGameEffect* CGameEffectDamageVisualEffect::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectDamageVisualEffect* copy = new CGameEffectDamageVisualEffect(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BDE20
BOOL CGameEffectDamageVisualEffect::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 1:
        pSprite->StartSpriteEffect(0, 0, 128, TRUE);
        break;
    case 2:
        pSprite->StartSpriteEffect(0, 1, 128, TRUE);
        break;
    case 3:
        pSprite->StartSpriteEffect(0, 2, 128, TRUE);
        break;
    case 4:
    case 5:
    case 6:
        pSprite->StartSpriteEffect(1, 0, 128, TRUE);
        break;
    case 7:
        pSprite->StartSpriteEffect(2, 0, 128, TRUE);
        break;
    case 8:
        pSprite->StartSpriteEffect(2, 1, 128, TRUE);
        break;
    case 9:
        pSprite->StartSpriteEffect(2, 2, 128, TRUE);
        break;
    case 10:
        pSprite->StartSpriteEffect(4, 0, 128, TRUE);
        break;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectAddInnateAbility::CGameEffectAddInnateAbility(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49AB20
CGameEffect* CGameEffectAddInnateAbility::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectAddInnateAbility* copy = new CGameEffectAddInnateAbility(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BE000
BOOL CGameEffectAddInnateAbility::ApplyEffect(CGameSprite* pSprite)
{
    if (!g_pBaldurChitin->cDimm.m_cKeyTable.FindKey(m_res, 1006, TRUE)) {
        m_done = TRUE;
        return TRUE;
    }

    pSprite->AddSpecialAbility(m_res, FALSE);

    // A shapeshift: SPIN122 is added once more when its entry's field_4 is
    // 0; any other goes through 0x725D30.
    UINT nID = 0;
    if (g_pBaldurChitin->GetObjectGame()->m_shapeshifts.Find(m_res, nID) == TRUE) {
        if (m_res == CGameSprite::SPIN122) {
            UINT nIndex = 0;
            if (pSprite->m_shapeshifts.Find(nID, nIndex) == TRUE) {
                // NOTE: Inlined CGameSpriteSpellList::Get.
                if (pSprite->m_shapeshifts.Get(nIndex)->field_4 == 0) {
                    unsigned int a2 = 1;
                    unsigned int a3 = 0;
                    unsigned int a4 = 0;
                    pSprite->m_shapeshifts.Add(nID, a2, a3, a4);

                    BOOLEAN a5 = FALSE;
                    pSprite->m_shapeshifts.sub_725C50(nID, a2, a5);
                }
            }
        } else {
            unsigned int a1 = 1;
            BOOLEAN a2 = FALSE;
            pSprite->m_shapeshifts.sub_725D30(a1, a2);
        }
    }

    m_done = TRUE;
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectRemoveInnateAbility::CGameEffectRemoveInnateAbility(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49ABD0
CGameEffect* CGameEffectRemoveInnateAbility::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectRemoveInnateAbility* copy = new CGameEffectRemoveInnateAbility(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BE1B0
BOOL CGameEffectRemoveInnateAbility::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->RemoveSpecialAbility(m_res);

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectResistanceToPoison::CGameEffectResistanceToPoison(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49AC80
CGameEffect* CGameEffectResistanceToPoison::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectResistanceToPoison* copy = new CGameEffectResistanceToPoison(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BE1D0
BOOL CGameEffectResistanceToPoison::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        if (pSprite->GetDerivedStats()->m_nResistPoison < m_effectAmount) {
            pSprite->GetDerivedStats()->m_nResistPoison = static_cast<BYTE>(m_effectAmount);
        }
        break;
    case 1:
        pSprite->GetDerivedStats()->m_nResistPoison += static_cast<SHORT>(m_effectAmount);
        break;
    }

    if (pSprite->GetDerivedStats()->m_nResistPoison < 0) {
        pSprite->GetDerivedStats()->m_nResistPoison = 0;
    } else if (pSprite->GetDerivedStats()->m_nResistPoison > 100) {
        pSprite->GetDerivedStats()->m_nResistPoison = 100;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectPlaySound::CGameEffectPlaySound(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49AD30
CGameEffect* CGameEffectPlaySound::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectPlaySound* copy = new CGameEffectPlaySound(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BE240
BOOL CGameEffectPlaySound::ApplyEffect(CGameSprite* pSprite)
{
    PlaySound(m_res, pSprite);

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectHoldCreatureSpell::CGameEffectHoldCreatureSpell(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49ADE0
CGameEffect* CGameEffectHoldCreatureSpell::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectHoldCreatureSpell* copy = new CGameEffectHoldCreatureSpell(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BE270
BOOL CGameEffectHoldCreatureSpell::ApplyEffect(CGameSprite* pSprite)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
    // __LINE__: 21645
    UTIL_ASSERT_MSG(FALSE, "CGameEffectHoldCreatureSpell game effect not supported in Icewind II (-rjf)");
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectApplyEffect::CGameEffectApplyEffect(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49AE90
CGameEffect* CGameEffectApplyEffect::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectApplyEffect* copy = new CGameEffectApplyEffect(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BE2A0
BOOL CGameEffectApplyEffect::ApplyEffect(CGameSprite* pSprite)
{
    CAIObjectType typeAI;
    typeAI.Set(pSprite->GetAIType());

    CAIObjectType type(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);

    // The EFF file applies when the sprite's object type has the amount in
    // the field the mode names (an amount of 0 matches any).
    BOOL bAlignment = FALSE;
    BOOL bApply = FALSE;
    switch (m_dwFlags) {
    case 2:
        type.m_nEnemyAlly = static_cast<BYTE>(m_effectAmount);
        bApply = typeAI.OfType(type, FALSE, FALSE);
        break;
    case 3:
        bApply = typeAI.m_nGeneral == static_cast<DWORD>(m_effectAmount) || m_effectAmount == 0;
        break;
    case 4:
        bApply = typeAI.m_nRace == static_cast<DWORD>(m_effectAmount) || m_effectAmount == 0;
        break;
    case 5:
        bApply = (typeAI.m_nClassMask & m_effectAmount) != 0 || m_effectAmount == 0;
        break;
    case 6:
        bApply = typeAI.m_nSpecific == static_cast<DWORD>(m_effectAmount) || m_effectAmount == 0;
        break;
    case 7:
        bApply = typeAI.m_nGender == static_cast<DWORD>(m_effectAmount) || m_effectAmount == 0;
        break;
    case 8:
        // An amount of 1, 2, 3 names the good-evil axis, 0x10, 0x20, 0x30
        // the law-chaos axis.
        if (typeAI.m_nAlignment == static_cast<DWORD>(m_effectAmount) || m_effectAmount == 0) {
            bAlignment = TRUE;
        }

        bApply = (m_effectAmount == 1 && (typeAI.m_nAlignment & 0xF) == 1)
            || (m_effectAmount == 2 && (typeAI.m_nAlignment & 0xF) == 2)
            || (m_effectAmount == 3 && (typeAI.m_nAlignment & 0xF) == 3)
            || (m_effectAmount == 0x10 && (typeAI.m_nAlignment & 0xF0) == 0x10)
            || (m_effectAmount == 0x20 && (typeAI.m_nAlignment & 0xF0) == 0x20)
            || (m_effectAmount == 0x30 && (typeAI.m_nAlignment & 0xF0) == 0x30)
            || bAlignment;
        break;
    }

    if (!bApply) {
        return TRUE;
    }

    // The effect stored in the EFF file m_res, applied now; it ends this
    // one when it ends itself.
    CResHelper<CResEffect, 1016> cFile;
    cFile.SetResRef(m_res, TRUE, TRUE);
    if (cFile.pRes != NULL) {
        BYTE* pData = static_cast<BYTE*>(cFile.pRes->Demand());
        if (pData == NULL) {
            cFile.Release();
        } else {
            // NOTE: The original passes a record whose other fields are left
            // uninitialised; CopyFromBase replaces them all.
            ITEM_EFFECT effect;
            memset(&effect, 0, sizeof(effect));
            effect.effectID = *reinterpret_cast<WORD*>(pData + 0x10);

            CGameEffect* pEffect = CGameEffect::DecodeEffect(&effect, CPoint(-1, -1), -1, CPoint(-1, -1));
            pEffect->CopyFromBase(reinterpret_cast<CGameEffectBase*>(pData + 8));
            pEffect->m_res.MakeUpper();
            pEffect->m_res2.MakeUpper();
            pEffect->m_res3.MakeUpper();
            cFile.Release();

            if (pEffect != NULL) {
                pEffect->ApplyEffect(pSprite);
                if (pEffect->m_done) {
                    m_done = TRUE;
                }
                delete pEffect;
            }
        }
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectSelectiveToHitBonus::CGameEffectSelectiveToHitBonus(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49AF40
CGameEffect* CGameEffectSelectiveToHitBonus::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectSelectiveToHitBonus* copy = new CGameEffectSelectiveToHitBonus(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BE600
BOOL CGameEffectSelectiveToHitBonus::ApplyEffect(CGameSprite* pSprite)
{
    CAIObjectType type(CAIObjectType::ANYONE);

    CSelectiveBonus* pBonus = new CSelectiveBonus;

    switch (m_dwFlags) {
    case 2:
        type.m_nEnemyAlly = static_cast<BYTE>(m_effectAmount);
        break;
    case 3:
        type.m_nGeneral = static_cast<BYTE>(m_effectAmount);
        break;
    case 4:
        type.m_nRace = static_cast<BYTE>(m_effectAmount);
        break;
    case 5:
        type.m_nClass = static_cast<BYTE>(m_effectAmount);
        break;
    case 6:
        type.m_nSpecific = static_cast<BYTE>(m_effectAmount);
        break;
    case 7:
        type.m_nGender = static_cast<BYTE>(m_effectAmount);
        break;
    case 8:
        type.m_nAlignment = static_cast<BYTE>(m_effectAmount);
        break;
    }

    pBonus->m_type.Set(type);
    pBonus->m_bonus = m_effectAmount2;

    pSprite->GetDerivedStats()->m_cToHitBonusList.AddTail(pBonus);

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectSelectiveDamageBonus::CGameEffectSelectiveDamageBonus(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49AFF0
CGameEffect* CGameEffectSelectiveDamageBonus::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectSelectiveDamageBonus* copy = new CGameEffectSelectiveDamageBonus(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BE740
BOOL CGameEffectSelectiveDamageBonus::ApplyEffect(CGameSprite* pSprite)
{
    CAIObjectType type(CAIObjectType::ANYONE);

    CSelectiveBonus* pBonus = new CSelectiveBonus;

    switch (m_dwFlags) {
    case 2:
        type.m_nEnemyAlly = static_cast<BYTE>(m_effectAmount);
        break;
    case 3:
        type.m_nGeneral = static_cast<BYTE>(m_effectAmount);
        break;
    case 4:
        type.m_nRace = static_cast<BYTE>(m_effectAmount);
        break;
    case 5:
        type.m_nClass = static_cast<BYTE>(m_effectAmount);
        break;
    case 6:
        type.m_nSpecific = static_cast<BYTE>(m_effectAmount);
        break;
    case 7:
        type.m_nGender = static_cast<BYTE>(m_effectAmount);
        break;
    case 8:
        type.m_nAlignment = static_cast<BYTE>(m_effectAmount);
        break;
    }

    pBonus->m_type.Set(type);
    pBonus->m_bonus = m_effectAmount2;

    pSprite->GetDerivedStats()->m_cDamageBonusList.AddTail(pBonus);

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectRestrictEquipItem::CGameEffectRestrictEquipItem(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49B0A0
CGameEffect* CGameEffectRestrictEquipItem::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectRestrictEquipItem* copy = new CGameEffectRestrictEquipItem(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BE880
BOOL CGameEffectRestrictEquipItem::ApplyEffect(CGameSprite* pSprite)
{
    CImmunitiesItemEquip* pItemEquip = new CImmunitiesItemEquip;
    pItemEquip->m_error = m_effectAmount;
    pItemEquip->m_res = m_res;

    // FIXME: `pEffect` is not set, which will probably lead to crash, see
    // `CImmunitiesItemEquipList::OnList` and
    // `CImmunitiesItemEquipList::operator=` implementations.

    if (m_dwFlags != 0) {
        pSprite->GetDerivedStats()->m_cImmunitiesItemUse.AddTail(pItemEquip);
    } else {
        pSprite->GetDerivedStats()->m_cImmunitiesItemEquip.AddTail(pItemEquip);
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectRestrictEquipItemType::CGameEffectRestrictEquipItemType(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49B150
CGameEffect* CGameEffectRestrictEquipItemType::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectRestrictEquipItemType* copy = new CGameEffectRestrictEquipItemType(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BE930
BOOL CGameEffectRestrictEquipItemType::ApplyEffect(CGameSprite* pSprite)
{
    CImmunitiesItemTypeEquip* pItemTypeEquip = new CImmunitiesItemTypeEquip;
    pItemTypeEquip->m_error = m_effectAmount;
    pItemTypeEquip->m_type = m_dwFlags;

    // FIXME: `pEffect` is not set, which will probably lead to crash, see
    // `CGameEffectRestrictEquipItemType::OnList` and
    // `CGameEffectRestrictEquipItemType::operator=` implementations.

    if (m_dwFlags != 0) {
        pSprite->GetDerivedStats()->m_cImmunitiesItemTypeUse.AddTail(pItemTypeEquip);
    } else {
        pSprite->GetDerivedStats()->m_cImmunitiesItemTypeEquip.AddTail(pItemTypeEquip);
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectApplyEffectEquipItem::CGameEffectApplyEffectEquipItem(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49B200
CGameEffect* CGameEffectApplyEffectEquipItem::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectApplyEffectEquipItem* copy = new CGameEffectApplyEffectEquipItem(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BE9A0
BOOL CGameEffectApplyEffectEquipItem::ApplyEffect(CGameSprite* pSprite)
{
    // Equipping the item m_res applies the EFF file m_res2.
    CImmunitiesItemEquip* pEntry = new CImmunitiesItemEquip();
    pEntry->m_error = -1;
    pEntry->m_res = m_res;

    CGameEffect* pEffect;
    CResHelper<CResEffect, 1016> cFile;
    cFile.SetResRef(m_res2, TRUE, TRUE);
    if (cFile.pRes == NULL) {
        pEffect = NULL;
    } else {
        BYTE* pData = static_cast<BYTE*>(cFile.pRes->Demand());
        if (pData == NULL) {
            cFile.Release();
            pEffect = NULL;
        } else {
            // NOTE: The original passes a record whose other fields are left
            // uninitialised; CopyFromBase replaces them all.
            ITEM_EFFECT effect;
            memset(&effect, 0, sizeof(effect));
            effect.effectID = *reinterpret_cast<WORD*>(pData + 0x10);

            pEffect = CGameEffect::DecodeEffect(&effect, CPoint(-1, -1), -1, CPoint(-1, -1));
            pEffect->CopyFromBase(reinterpret_cast<CGameEffectBase*>(pData + 8));
            pEffect->m_res.MakeUpper();
            pEffect->m_res2.MakeUpper();
            pEffect->m_res3.MakeUpper();
            cFile.Release();
        }
    }

    pEntry->m_pEffect = pEffect;
    pSprite->m_derivedStats.field_290.AddTail(pEntry);

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectApplyEffectEquipItemType::CGameEffectApplyEffectEquipItemType(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49B2B0
CGameEffect* CGameEffectApplyEffectEquipItemType::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectApplyEffectEquipItemType* copy = new CGameEffectApplyEffectEquipItemType(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BEB60
BOOL CGameEffectApplyEffectEquipItemType::ApplyEffect(CGameSprite* pSprite)
{
    // Equipping an item of type m_dwFlags applies the EFF file m_res.
    CImmunitiesItemTypeEquip* pEntry = new CImmunitiesItemTypeEquip();
    pEntry->m_error = m_effectAmount;
    pEntry->m_type = m_dwFlags;

    CGameEffect* pEffect;
    CResHelper<CResEffect, 1016> cFile;
    cFile.SetResRef(m_res, TRUE, TRUE);
    if (cFile.pRes == NULL) {
        pEffect = NULL;
    } else {
        BYTE* pData = static_cast<BYTE*>(cFile.pRes->Demand());
        if (pData == NULL) {
            cFile.Release();
            pEffect = NULL;
        } else {
            // NOTE: The original passes a record whose other fields are left
            // uninitialised; CopyFromBase replaces them all.
            ITEM_EFFECT effect;
            memset(&effect, 0, sizeof(effect));
            effect.effectID = *reinterpret_cast<WORD*>(pData + 0x10);

            pEffect = CGameEffect::DecodeEffect(&effect, CPoint(-1, -1), -1, CPoint(-1, -1));
            pEffect->CopyFromBase(reinterpret_cast<CGameEffectBase*>(pData + 8));
            pEffect->m_res.MakeUpper();
            pEffect->m_res2.MakeUpper();
            pEffect->m_res3.MakeUpper();
            cFile.Release();
        }
    }

    pEntry->m_pEffect = pEffect;
    pSprite->m_derivedStats.field_2AC.AddTail(pEntry);

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectSetDoNoJump::CGameEffectSetDoNoJump(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49B360
CGameEffect* CGameEffectSetDoNoJump::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectSetDoNoJump* copy = new CGameEffectSetDoNoJump(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BECF0
BOOL CGameEffectSetDoNoJump::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetDerivedStats()->m_bDoNotJump = m_dwFlags;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectJumpToArea::CGameEffectJumpToArea(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49B410
CGameEffect* CGameEffectJumpToArea::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectJumpToArea* copy = new CGameEffectJumpToArea(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BED10
BOOL CGameEffectJumpToArea::ApplyEffect(CGameSprite* pSprite)
{
    CString tempStr;
    m_res.CopyToString(tempStr);
    pSprite->JumpToArea(tempStr,
        m_source,
        static_cast<SHORT>(m_dwFlags),
        static_cast<SHORT>(m_effectAmount));

    if (CString(pSprite->GetArea()->GetHeader()->m_areaName) == tempStr) {
        m_done = TRUE;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectSetLocalVariable::CGameEffectSetLocalVariable(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49B4C0
CGameEffect* CGameEffectSetLocalVariable::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectSetLocalVariable* copy = new CGameEffectSetLocalVariable(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BEDF0
BOOL CGameEffectSetLocalVariable::ApplyEffect(CGameSprite* pSprite)
{
    CVariable var;
    CString strRes1;
    CString strRes2;
    CString strRes3;
    CString work;

    m_res.CopyToString(strRes1);
    m_res2.CopyToString(strRes2);
    m_res3.CopyToString(strRes3);

    strncpy(var.m_name, m_scriptName, SCRIPTNAME_SIZE);
    var.m_type = static_cast<WORD>(m_flags);
    var.m_dwValue = static_cast<DWORD>(m_dwFlags);
    var.m_intValue = static_cast<LONG>(m_effectAmount);
    var.m_resRefType = static_cast<WORD>(m_effectAmount2);
    var.m_floatValue = static_cast<float>(m_effectAmount3);
    work = strRes1 + strRes2 + strRes3;
    strncpy(var.m_stringValue, work, SCRIPTNAME_SIZE);
    pSprite->GetLocalVariables()->AddKey(var);

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectAuraCleansing::CGameEffectAuraCleansing(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49B570
CGameEffect* CGameEffectAuraCleansing::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectAuraCleansing* copy = new CGameEffectAuraCleansing(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BEFC0
BOOL CGameEffectAuraCleansing::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetDerivedStats()->m_bAuraCleansing = m_dwFlags;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectMentalSpeed::CGameEffectMentalSpeed(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49B620
CGameEffect* CGameEffectMentalSpeed::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectMentalSpeed* copy = new CGameEffectMentalSpeed(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BEFE0
BOOL CGameEffectMentalSpeed::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetDerivedStats()->m_nMentalSpeed = static_cast<BYTE>(m_effectAmount);

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectPhysicalSpeed::CGameEffectPhysicalSpeed(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49B6D0
CGameEffect* CGameEffectPhysicalSpeed::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectPhysicalSpeed* copy = new CGameEffectPhysicalSpeed(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BF000
BOOL CGameEffectPhysicalSpeed::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetDerivedStats()->m_nPhysicalSpeed = static_cast<BYTE>(m_effectAmount);

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectCastingLevelBonus::CGameEffectCastingLevelBonus(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49B780
CGameEffect* CGameEffectCastingLevelBonus::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectCastingLevelBonus* copy = new CGameEffectCastingLevelBonus(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BF020
BOOL CGameEffectCastingLevelBonus::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        pSprite->GetDerivedStats()->m_nCastingLevelBonusMage = static_cast<BYTE>(m_effectAmount);
        break;
    case 1:
        pSprite->GetDerivedStats()->m_nCastingLevelBonusCleric = static_cast<BYTE>(m_effectAmount);
        break;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x499070
CGameEffectFeatCleave::CGameEffectFeatCleave(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4990B0
CGameEffect* CGameEffectFeatCleave::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectFindFamiliar* copy = new CGameEffectFindFamiliar(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B9AA0
BOOL CGameEffectFeatCleave::ApplyEffect(CGameSprite* pSprite)
{
    if (!m_secondaryType) {
        return TRUE;
    }

    CAIObjectType typeEnemy = pSprite->GetAIType().GetEnemyOf();

    // NOTE: Inlined CGameAnimation::GetPersonalSpace (ObjAnimation.h line
    // 2093). The product is a BYTE.
    SHORT nRange = static_cast<BYTE>(pSprite->GetAnimation()->GetPersonalSpace() * 5) + 50;

    // The nearest enemy in reach (the BYTEs 0x847D1D and 0x847D25, 29 and
    // 37: the n-th nearest from 0 to 8); the one in m_special is passed
    // over once.
    LONG nTarget = CGameObjectArray::INVALID_INDEX;
    LONG nLast = CGameObjectArray::INVALID_INDEX;
    for (BYTE nObject = 29; nObject <= 37; nObject++) {
        nTarget = pSprite->GetArea()->sub_46DDC0(pSprite->GetPos().x,
            pSprite->GetPos().y,
            typeEnemy,
            nRange,
            pSprite->GetVisibleTerrainTable(),
            FALSE,
            FALSE,
            nObject - 29,
            FALSE);
        if (nTarget == CGameObjectArray::INVALID_INDEX) {
            break;
        }

        if (nTarget != static_cast<LONG>(m_special)) {
            break;
        }

        nTarget = CGameObjectArray::INVALID_INDEX;
        if (nLast == static_cast<LONG>(m_special)) {
            break;
        }
        nLast = m_special;
    }

    if (nTarget == CGameObjectArray::INVALID_INDEX) {
        m_done = TRUE;
        return TRUE;
    }

    // Another swing at it.
    pSprite->FeedBack(CGameSprite::FEEDBACK_CLEAVE, pSprite->field_5636, 0, 0, -1, 0, 0);
    pSprite->m_attackFrame = -2;
    pSprite->field_5636 = static_cast<BYTE>(m_effectAmount);

    CMessage* message = new CMessageSetSequence(CGameSprite::SEQ_ATTACK, pSprite->GetId(), pSprite->GetId());
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

    CAIAction action(CAIAction::ATTACK, typeEnemy, nTarget, 0, 0);
    pSprite->ClearActions(FALSE);
    pSprite->DropPath();
    pSprite->m_interrupt = TRUE;
    pSprite->AddAction(action);

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectFindFamiliar::CGameEffectFindFamiliar(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x499160
CGameEffect* CGameEffectFindFamiliar::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectFindFamiliar* copy = new CGameEffectFindFamiliar(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B9EB0
BOOL CGameEffectFindFamiliar::ApplyEffect(CGameSprite* pSprite)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    if (pSprite->GetId() != pGame->GetProtagonist()) {
        pSprite->DisplayTextRef(-1, 0x215a, 0xff, 0xff);
        m_done = TRUE;
        return TRUE;
    }
    if (pGame->field_4204 != 0) {
        pSprite->DisplayTextRef(-1, 0x2159, 0xff, 0xff);
        m_done = TRUE;
        return TRUE;
    }

    // TODO: Incomplete (0x5AF540). The CInfGame helper that picks and pops the
    // familiar resref for the caster's class/level is not ported: it reads the
    // resref lists at +0x390c (the ported CInfGame layout has +0x3928) and
    // posts an unidentified message class (vtbl 0x84d1a0). Until then the name
    // stays empty. The original trips its UtilAssert(0x47fc, "FALSE") on an
    // empty resref and continues; asserts are not ported, so neither is the
    // call.
    CString sFamiliar;

    CCreatureFile cfile;
    CResRef tempResRef(sFamiliar);
    cfile.SetResRef(tempResRef, TRUE, TRUE);

    CGameSprite* pFamiliar = new CGameSprite(cfile.GetData(), cfile.GetDataSize(), 0, 0, (DWORD)-1, 0, 0, 0x7fffffff, CPoint(-1, -1), 0);
    if (pFamiliar == NULL) {
        // 0x480a "FALSE" assert, then the failure return.
        cfile.ReleaseData();
        return FALSE;
    }

    // NOTE: the exe reaches the resref through a call Ghidra labels
    // GetEllipseRect (0x4699e0); semantically it is &cfile.cResRef.
    pFamiliar->SetResRef(CResRef(cfile.cResRef.GetResRefStr()));
    cfile.ReleaseData();
    // 0x4ba16b: the original then SetResRefs a default (empty) CString.
    pFamiliar->SetResRef(CResRef(""));

    pGame->AddCharacterToFamiliars(pFamiliar->GetId());
    pFamiliar->MakeGlobal();

    // CAIOBJECTTYPE_EA_FAMILIAR (the rdata byte at 0x847c36): all three
    // familiar AI-type copies get the EA replaced.
    CAIObjectType typeFamiliarEA(pFamiliar->m_liveTypeAI);
    typeFamiliarEA.m_nEnemyAlly = CAIOBJECTTYPE_EA_FAMILIAR;
    pFamiliar->m_startTypeAI.Set(typeFamiliarEA);

    typeFamiliarEA.Set(pFamiliar->GetAIType());
    typeFamiliarEA.m_nEnemyAlly = CAIOBJECTTYPE_EA_FAMILIAR;
    pFamiliar->SetAIType(typeFamiliarEA, FALSE, FALSE);

    typeFamiliarEA.Set(pFamiliar->m_liveTypeAI);
    typeFamiliarEA.m_nEnemyAlly = CAIOBJECTTYPE_EA_FAMILIAR;
    pFamiliar->m_liveTypeAI.Set(typeFamiliarEA);

    // NOTE: the original asserts the caster's animation pointer inline (0x82d)
    // and the familiar's (0x826); the ported accessors handle a missing one.
    // NOTE: the exe divides the effect's target x by 16 and y by 12 ([+0x84]
    // and [+0x88] against the rdata bytes 0x84d6a0/0x84d6a1). Those fields are
    // m_target.x/m_target.y in the ported header (see evidence 4B9EB0).
    BYTE nPersonalSpace = pSprite->m_animation.GetPersonalSpace();
    CPoint ptOpen = pSprite->m_pArea->m_search.GetNearestOpenSquare(CPoint(m_target.x / 16, m_target.y / 12), pSprite->GetTerrainTable(), nPersonalSpace, -1);
    BYTE nListType = pFamiliar->m_animation.GetListType();
    CPoint ptPlaced(ptOpen.x * 16, ptOpen.y * 12);
    pFamiliar->AddToArea(pSprite->m_pArea, ptPlaced, 0, nListType);
    pFamiliar->SetFacing(rand() % 16);

    CProjectile* pProjectile = CProjectile::DecodeProjectile(0x90, pFamiliar, 0);
    // NOTE: the exe calls SetColorRange directly on the CProjectileBAM built
    // for projectile 0x90; the ported DecodeProjectile has no case 144 yet
    // (its default DEVIATION returns the instant projectile).
    static_cast<CProjectileBAM*>(pProjectile)->SetColorRange(0, 0x33);
    // TODO: the exe then calls the projectile's vtbl+0x88 virtual with
    // (pFamiliar->m_pArea, ptPlaced.x, ptPlaced.y + 1); CProjectileBAM
    // declares no virtual in that slot — unported pending identification.
    CMessageFireProjectile* pFire = new CMessageFireProjectile(pProjectile->m_projectileType,
        pFamiliar->GetId(),
        ptPlaced,
        0x1e,
        pFamiliar->GetId(),
        pFamiliar->GetId(),
        0);
    g_pBaldurChitin->GetMessageHandler()->AddMessage(pFire, FALSE);

    // Effect 1: HITPOINTS on the caster (the familiar's full hit points).
    ITEM_EFFECT* effect = new ITEM_EFFECT;
    memset(effect, 0, sizeof(ITEM_EFFECT));
    effect->effectID = CGAMEEFFECT_HITPOINTS;
    effect->effectAmount = pFamiliar->m_baseStats.m_hitPoints;
    effect->durationType = 1;
    effect->probabilityUpper = 100;
    CGameEffect* pEffect = CGameEffect::DecodeEffect(effect, pSprite->GetPos(), pSprite->GetId(), CPoint(-1, -1));
    CMessage* message = new CMessageAddEffect(pEffect, pSprite->GetId(), pSprite->GetId());
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    delete effect;

    // Effect 2: HITPOINTSONDEATH on the familiar (undoes effect 1, doubled).
    effect = new ITEM_EFFECT;
    memset(effect, 0, sizeof(ITEM_EFFECT));
    effect->effectID = CGAMEEFFECT_HITPOINTSONDEATH;
    effect->effectAmount = -(SHORT)pFamiliar->m_baseStats.m_hitPoints * 2;
    effect->dwFlags = 0x400000;
    effect->durationType = 1;
    effect->probabilityUpper = 100;
    pEffect = CGameEffect::DecodeEffect(effect, pFamiliar->GetPos(), pSprite->GetId(), CPoint(-1, -1));
    message = new CMessageAddEffect(pEffect, pFamiliar->GetId(), pSprite->GetId());
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    delete effect;

    // Effect 3: FAMILIARBLOCK on the familiar.
    effect = new ITEM_EFFECT;
    memset(effect, 0, sizeof(ITEM_EFFECT));
    effect->effectID = CGAMEEFFECT_FAMILIARBLOCK;
    effect->durationType = 1;
    effect->probabilityUpper = 100;
    pEffect = CGameEffect::DecodeEffect(effect, pFamiliar->GetPos(), pSprite->GetId(), CPoint(-1, -1));
    message = new CMessageAddEffect(pEffect, pFamiliar->GetId(), pSprite->GetId());
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    delete effect;

    m_done = TRUE;
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectSeeInvisible::CGameEffectSeeInvisible(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49B830
CGameEffect* CGameEffectSeeInvisible::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectSeeInvisible* copy = new CGameEffectSeeInvisible(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BF060
BOOL CGameEffectSeeInvisible::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetDerivedStats()->m_bSeeInvisible = m_dwFlags;

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectIgnoreDialogPause::CGameEffectIgnoreDialogPause(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49B8E0
CGameEffect* CGameEffectIgnoreDialogPause::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectIgnoreDialogPause* copy = new CGameEffectIgnoreDialogPause(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BF080
BOOL CGameEffectIgnoreDialogPause::ApplyEffect(CGameSprite* pSprite)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
    // __LINE__: 22409
    UTIL_ASSERT_MSG(FALSE, "CGameEffectIgnoreDialogPause game effect not supported in Icewind II (-rjf)");
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectHitPointsOnDeath::CGameEffectHitPointsOnDeath(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x499210
CGameEffect* CGameEffectHitPointsOnDeath::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectHitPointsOnDeath* copy = new CGameEffectHitPointsOnDeath(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BA6D0
BOOL CGameEffectHitPointsOnDeath::ApplyEffect(CGameSprite* pSprite)
{
    if (!pSprite->Animate()) {
        BYTE rc;
        CGameSprite* pProtagonist;
        ITEM_EFFECT* effect;
        CGameEffect* pEffect;
        CMessage* message;

        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(g_pBaldurChitin->GetObjectGame()->GetProtagonist(),
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pProtagonist),
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(pProtagonist->GetNameRef(),
                8536,
                RGB(215, 215, 190),
                RGB(215, 215, 190),
                -1,
                pProtagonist->GetId(),
                pProtagonist->GetId());

            // FIXME: Should be on stack.
            effect = new ITEM_EFFECT();
            effect->effectAmount = 0;
            effect->durationType = 0;
            effect->effectID = CGAMEEFFECT_HITPOINTS;
            effect->targetType = 0;
            effect->spellLevel = 0;
            effect->dwFlags = 0;
            effect->duration = 0;
            effect->probabilityUpper = 100;
            effect->probabilityLower = 0;
            effect->numDice = 0;
            effect->diceSize = 0;
            effect->savingThrow = 0;
            effect->saveMod = 0;
            effect->special = 0;
            effect->effectAmount = m_effectAmount;
            effect->durationType = static_cast<WORD>(m_durationType);

            pEffect = DecodeEffect(effect,
                pSprite->GetPos(),
                pSprite->GetId(),
                CPoint(-1, -1));

            message = new CMessageAddEffect(pEffect,
                pSprite->GetId(),
                pProtagonist->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

            delete effect;

            // FIXME: Should be on stack.
            effect = new ITEM_EFFECT();
            effect->effectAmount = 0;
            effect->durationType = 0;
            effect->effectID = CGAMEEFFECT_DAMAGE;
            effect->targetType = 0;
            effect->spellLevel = 0;
            effect->dwFlags = 0;
            effect->duration = 0;
            effect->probabilityUpper = 100;
            effect->probabilityLower = 0;
            effect->numDice = 0;
            effect->diceSize = 0;
            effect->savingThrow = 0;
            effect->saveMod = 0;
            effect->special = 0;
            effect->effectAmount = 0;
            effect->durationType = static_cast<WORD>(m_durationType);

            pEffect = DecodeEffect(effect,
                pSprite->GetPos(),
                pSprite->GetId(),
                CPoint(-1, -1));

            message = new CMessageAddEffect(pEffect,
                pSprite->GetId(),
                pProtagonist->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(g_pBaldurChitin->GetObjectGame()->GetProtagonist(),
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);

            delete effect;
        }
    } else {
        m_done = FALSE;
    }
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectFamiliarBlock::CGameEffectFamiliarBlock(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4992C0
CGameEffect* CGameEffectFamiliarBlock::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectFamiliarBlock* copy = new CGameEffectFamiliarBlock(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4B9E70
BOOL CGameEffectFamiliarBlock::ApplyEffect(CGameSprite* pSprite)
{
    g_pBaldurChitin->GetObjectGame()->field_4204 = pSprite->Animate();
    m_done = !pSprite->Animate();

    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectBounceProjectile::CGameEffectBounceProjectile(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49B990
CGameEffect* CGameEffectBounceProjectile::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectBounceProjectile* copy = new CGameEffectBounceProjectile(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x799C90
BOOL CGameEffectBounceProjectile::ApplyEffect(CGameSprite* pSprite)
{
    // NOTE: The vtable's slot +8 is the shared "return 1" (ret 4) that
    // identical code folding made of every such override; the effect works
    // through the code that looks for it, not here.
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectBounceEffect::CGameEffectBounceEffect(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49BA40
CGameEffect* CGameEffectBounceEffect::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectBounceEffect* copy = new CGameEffectBounceEffect(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x799C90
BOOL CGameEffectBounceEffect::ApplyEffect(CGameSprite* pSprite)
{
    // NOTE: The vtable's slot +8 is the shared "return 1" (ret 4) that
    // identical code folding made of every such override; the effect works
    // through the code that looks for it, not here.
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectBounceLevel::CGameEffectBounceLevel(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49BAF0
CGameEffect* CGameEffectBounceLevel::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectBounceLevel* copy = new CGameEffectBounceLevel(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x799C90
BOOL CGameEffectBounceLevel::ApplyEffect(CGameSprite* pSprite)
{
    // NOTE: The vtable's slot +8 is the shared "return 1" (ret 4) that
    // identical code folding made of every such override; the effect works
    // through the code that looks for it, not here.
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectBounceLevelDecrement::CGameEffectBounceLevelDecrement(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49BBA0
CGameEffect* CGameEffectBounceLevelDecrement::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectBounceLevelDecrement* copy = new CGameEffectBounceLevelDecrement(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x799C90
BOOL CGameEffectBounceLevelDecrement::ApplyEffect(CGameSprite* pSprite)
{
    // NOTE: The vtable's slot +8 is the shared "return 1" (ret 4) that
    // identical code folding made of every such override; the effect works
    // through the code that looks for it, not here.
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectImmunityLevelDecrement::CGameEffectImmunityLevelDecrement(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49BC50
CGameEffect* CGameEffectImmunityLevelDecrement::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectImmunityLevelDecrement* copy = new CGameEffectImmunityLevelDecrement(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BF0B0
BOOL CGameEffectImmunityLevelDecrement::ApplyEffect(CGameSprite* pSprite)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
    // __LINE__: 22530
    UTIL_ASSERT_MSG(FALSE, "CGameEffectImmunityLevelDecrement game effect not supported in Icewind II (-rjf)");
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectBounceSchool::CGameEffectBounceSchool(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49BD00
CGameEffect* CGameEffectBounceSchool::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectBounceSchool* copy = new CGameEffectBounceSchool(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x799C90
BOOL CGameEffectBounceSchool::ApplyEffect(CGameSprite* pSprite)
{
    // NOTE: The vtable's slot +8 is the shared "return 1" (ret 4) that
    // identical code folding made of every such override; the effect works
    // through the code that looks for it, not here.
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectBounceSecondaryType::CGameEffectBounceSecondaryType(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49BDB0
CGameEffect* CGameEffectBounceSecondaryType::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectBounceSecondaryType* copy = new CGameEffectBounceSecondaryType(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x799C90
BOOL CGameEffectBounceSecondaryType::ApplyEffect(CGameSprite* pSprite)
{
    // NOTE: The vtable's slot +8 is the shared "return 1" (ret 4) that
    // identical code folding made of every such override; the effect works
    // through the code that looks for it, not here.
    return TRUE;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CGameEffectImmunitySchool::CGameEffectImmunitySchool(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49BE60
CGameEffect* CGameEffectImmunitySchool::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectImmunitySchool* copy = new CGameEffectImmunitySchool(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BF0E0
BOOL CGameEffectImmunitySchool::ApplyEffect(CGameSprite* pSprite)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
    // __LINE__: 22603
    UTIL_ASSERT_MSG(FALSE, "CGameEffectImmunitySchool game effect not supported in Icewind II (-rjf)");
}

// -----------------------------------------------------------------------------

// 0x49BF10
CGameEffectImmunitySecondaryType::CGameEffectImmunitySecondaryType(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49BF50
CGameEffect* CGameEffectImmunitySecondaryType::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectImmunitySecondaryType* copy = new CGameEffectImmunitySecondaryType(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BF110
BOOL CGameEffectImmunitySecondaryType::ApplyEffect(CGameSprite* pSprite)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
    // __LINE__: 22629
    UTIL_ASSERT_MSG(FALSE, "CGameEffectImmunitySecondaryType game effect not supported in Icewind II (-rjf)");
}

// -----------------------------------------------------------------------------

// 0x49C000
CGameEffectImmunitySpell::CGameEffectImmunitySpell(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49C040
CGameEffect* CGameEffectImmunitySpell::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectImmunitySpell* copy = new CGameEffectImmunitySpell(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4BFE20
BOOL CGameEffectImmunitySpell::ApplyEffect(CGameSprite* pSprite)
{
    if (Evaluate(pSprite) == TRUE) {
        m_done = TRUE;
    } else {
        std::string sRes(static_cast<const char*>(m_res.GetResRefStr()));
        if (sRes.length() == 0) {
            m_done = TRUE;
        } else {
            pSprite->m_derivedStats.m_resourceImmunities.insert(sRes);

            if (m_secondaryType && m_effectID == ICEWIND_CGAMEEFFECT_IMMUNITYSPELL) {
                // NOTE: Inlined FeedBackImmuneToResource (0x4C4330), with
                // the key lookups not warning when missing.
                if (g_pBaldurChitin->cDimm.m_cKeyTable.FindKey(m_sourceRes, 1005, FALSE)) {
                    CItem* pItem = new CItem(m_sourceRes, 1, 0, 0, 0, 0);
                    pSprite->FeedBack(CGameSprite::FEEDBACK_IMMUNE_TO_RESOURCE,
                        0,
                        0,
                        0,
                        pItem->GetIdentifiedName(),
                        0,
                        0);
                    delete pItem;
                } else if (g_pBaldurChitin->cDimm.m_cKeyTable.FindKey(m_sourceRes, 1006, FALSE)) {
                    CSpell* pSpell = new CSpell(m_sourceRes);
                    pSprite->FeedBack(CGameSprite::FEEDBACK_IMMUNE_TO_RESOURCE,
                        0,
                        0,
                        0,
                        pSpell->GetGenericName(),
                        0,
                        0);
                    delete pSpell;
                }
            }
        }
    }

    return TRUE;
}

// 0x4BF140
BOOL CGameEffectImmunitySpell::Evaluate(CGameSprite* pSprite)
{
    CGameObject* pObject;
    BYTE rc;
    BOOL bResult;

    switch (m_dwFlags) {
    case 0:
        return FALSE;
    case 1:
        return !IcewindMisc::IsUndead(pSprite);
    case 2:
        return IcewindMisc::IsUndead(pSprite);
    case 3:
        return !IcewindMisc::IsImmuneToFire(pSprite);
    case 4:
        return IcewindMisc::IsImmuneToFire(pSprite);
    case 5:
        return !IcewindMisc::IsHumanoid(pSprite);
    case 6:
        return IcewindMisc::IsHumanoid(pSprite);
    case 7:
        return !IcewindMisc::IsAnimal(pSprite);
    case 8:
        return IcewindMisc::IsAnimal(pSprite);
    case 9:
        return !IcewindMisc::IsElemental(pSprite);
    case 10:
        return IcewindMisc::IsElemental(pSprite);
    case 11:
        return !IcewindMisc::IsFungus(pSprite);
    case 12:
        return IcewindMisc::IsFungus(pSprite);
    case 13:
        return !IcewindMisc::IsLarge(pSprite);
    case 14:
        return IcewindMisc::IsLarge(pSprite);
    case 15:
        return !IcewindMisc::IsElf(pSprite);
    case 16:
        return IcewindMisc::IsElf(pSprite);
    case 17:
        return !IcewindMisc::IsUmberhulk(pSprite);
    case 18:
        return IcewindMisc::IsUmberhulk(pSprite);
    case 19:
        return !IcewindMisc::IsHalfElf(pSprite);
    case 20:
        return IcewindMisc::IsHalfElf(pSprite);
    case 21:
        return !IcewindMisc::IsHumanoid(pSprite) && !IcewindMisc::IsAnimal(pSprite);
    case 22:
        return IcewindMisc::IsHumanoid(pSprite) || IcewindMisc::IsAnimal(pSprite);
    case 23:
        return !IcewindMisc::IsBlind(pSprite);
    case 24:
        return IcewindMisc::IsBlind(pSprite);
    case 25:
        return !IcewindMisc::IsImmuneToCold(pSprite);
    case 26:
        return IcewindMisc::IsImmuneToCold(pSprite);
    case 27:
        return !IcewindMisc::IsGolem(pSprite);
    case 28:
        return IcewindMisc::IsGolem(pSprite);
    case 29:
        return !IcewindMisc::IsMinotaur(pSprite);
    case 30:
        return IcewindMisc::IsMinotaur(pSprite);
    case 31:
        return !IcewindMisc::IsUndead(pSprite) && !IcewindMisc::IsFungus(pSprite);
    case 32:
        return IcewindMisc::IsUndead(pSprite) || IcewindMisc::IsFungus(pSprite);
    case 33:
        return !IcewindMisc::IsGood(pSprite);
    case 34:
        return IcewindMisc::IsGood(pSprite);
    case 35:
        return !IcewindMisc::IsNeutral(pSprite);
    case 36:
        return IcewindMisc::IsNeutral(pSprite);
    case 37:
        return !IcewindMisc::IsEvil(pSprite);
    case 38:
        return IcewindMisc::IsEvil(pSprite);
    case 39:
        return !IcewindMisc::IsPaladin(pSprite);
    case 40:
        return IcewindMisc::IsPaladin(pSprite);
    case 41:
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_sourceID,
            CGameObjectArray::THREAD_ASYNCH,
            &pObject,
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            bResult = !IcewindMisc::IsGoodEvilSame(pSprite, static_cast<CGameSprite*>(pObject));
            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceID,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        } else {
            bResult = FALSE;
        }
        return bResult;
    case 42:
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_sourceID,
            CGameObjectArray::THREAD_ASYNCH,
            &pObject,
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            bResult = IcewindMisc::IsGoodEvilSame(pSprite, static_cast<CGameSprite*>(pObject));
            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceID,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        } else {
            bResult = FALSE;
        }
        return bResult;
    case 43:
        return pSprite->GetId() != m_sourceID;
    case 44:
        return pSprite->GetId() == m_sourceID;
    case 45:
        return !IcewindMisc::IsAcquatic(pSprite);
    case 46:
        return IcewindMisc::IsAcquatic(pSprite);
    case 47:
        return !IcewindMisc::IsLiving(pSprite);
    case 48:
        return IcewindMisc::IsLiving(pSprite);
    case 49:
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_sourceID,
            CGameObjectArray::THREAD_ASYNCH,
            &pObject,
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            bResult = !IcewindMisc::sub_585230(pSprite, static_cast<CGameSprite*>(pObject));
            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceID,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        } else {
            bResult = FALSE;
        }
        return bResult;
    case 50:
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_sourceID,
            CGameObjectArray::THREAD_ASYNCH,
            &pObject,
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            bResult = IcewindMisc::sub_585230(pSprite, static_cast<CGameSprite*>(pObject));
            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceID,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        } else {
            bResult = FALSE;
        }
        return bResult;
    case 51:
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_sourceID,
            CGameObjectArray::THREAD_ASYNCH,
            &pObject,
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            bResult = !IcewindMisc::sub_5852A0(pSprite, static_cast<CGameSprite*>(pObject));
            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceID,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        } else {
            bResult = FALSE;
        }
        return bResult;
    case 52:
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_sourceID,
            CGameObjectArray::THREAD_ASYNCH,
            &pObject,
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            bResult = IcewindMisc::sub_5852A0(pSprite, static_cast<CGameSprite*>(pObject));
            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceID,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        } else {
            bResult = FALSE;
        }
        return bResult;
    case 53:
        return !IcewindMisc::IsImmuneToFire(pSprite) && !IcewindMisc::IsImmuneToCold(pSprite);
    case 54:
        return IcewindMisc::IsImmuneToFire(pSprite) || IcewindMisc::IsImmuneToCold(pSprite);
    case 55:
        return !IcewindMisc::sud_585070(pSprite);
    case 56:
        return IcewindMisc::sud_585070(pSprite);
    case 57:
        return !IcewindMisc::IsMale(pSprite);
    case 58:
        return IcewindMisc::IsMale(pSprite);
    case 59:
        return !IcewindMisc::IsLawful(pSprite);
    case 60:
        return IcewindMisc::IsLawful(pSprite);
    case 61:
        return !IcewindMisc::IsChaotic(pSprite);
    case 62:
        return IcewindMisc::IsChaotic(pSprite);
    case 63:
        return TRUE;
    case 64:
    case 65:
        return pSprite->GetAIType().GetRace() == CAIOBJECTTYPE_R_ORC;
    case 66:
        return !pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_DEAFENED];
    case 67:
        return pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_DEAFENED];
    case 68:
        return (pSprite->GetDerivedStats()->m_generalState & STATE_SUMMONED_CREATURE) == 0;
    case 69:
        return (pSprite->GetDerivedStats()->m_generalState & STATE_SUMMONED_CREATURE) != 0;
    case 70:
        return pSprite->GetAIType().GetRace() != CAIOBJECTTYPE_R_184;
    case 71:
        return pSprite->GetAIType().GetRace() == CAIOBJECTTYPE_R_184;
    case 72:
        return TRUE;
    case 73:
        return TRUE;
    case 74:
        return pSprite->GetDerivedStats()->m_nINT >= m_effectAmount;
    case 75:
        return pSprite->GetDerivedStats()->m_nINT <= m_effectAmount;
    case 76:
        return pSprite->GetDerivedStats()->m_nINT > m_effectAmount;
    case 77:
        return pSprite->GetDerivedStats()->m_nINT < m_effectAmount;
    case 78:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 23134
        UTIL_ASSERT_MSG(FALSE, "CGameEffectImmunitySpell::ApplyEffect - Test for IF_BARD_SKALD is obsolete");
    case 79:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 23137
        UTIL_ASSERT_MSG(FALSE, "CGameEffectImmunitySpell::ApplyEffect - Test for IF_BARD_SKALD is obsolete");
    case 80:
        return !IcewindMisc::IsEnemyNearby(pSprite);
    case 81:
        return IcewindMisc::IsEnemyNearby(pSprite);
    case 82:
        return !(pSprite->GetAIType().GetRace() == CAIOBJECTTYPE_R_ELF && pSprite->GetAIType().GetSubRace() == CAIOBJECTTYPE_SUBRACE_ELF_DROW);
    case 83:
        return pSprite->GetAIType().GetRace() == CAIOBJECTTYPE_R_ELF && pSprite->GetAIType().GetSubRace() == CAIOBJECTTYPE_SUBRACE_ELF_DROW;
    case 84:
        return !(pSprite->GetAIType().GetRace() == CAIOBJECTTYPE_R_DWARF && pSprite->GetAIType().GetSubRace() == CAIOBJECTTYPE_SUBRACE_DWARF_GRAY);
    case 85:
        return pSprite->GetAIType().GetRace() == CAIOBJECTTYPE_R_DWARF && pSprite->GetAIType().GetSubRace() == CAIOBJECTTYPE_SUBRACE_DWARF_GRAY;
    case 86:
        return !g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->IsDay() && !g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->IsDawn();
    case 87:
        return g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->IsDay() || g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->IsDawn();
    case 88:
        return !(pSprite->GetArea() != NULL && (pSprite->GetArea()->GetHeader()->m_areaType & 0x3) != 0);
    case 89:
        return pSprite->GetArea() != NULL && (pSprite->GetArea()->GetHeader()->m_areaType & 0x3) != 0;
    case 90:
        return pSprite->GetAIType().GetRace() != CAIOBJECTTYPE_R_190;
    case 91:
        return pSprite->GetAIType().GetRace() == CAIOBJECTTYPE_R_190;
    case 92:
        return !IcewindMisc::IsOutsider(pSprite);
    case 93:
        return IcewindMisc::IsOutsider(pSprite);
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
        // __LINE__: 23214
        UTIL_ASSERT(FALSE);
    }
}

// -----------------------------------------------------------------------------

// 0x49C0F0
CGameEffectBounceSpell::CGameEffectBounceSpell(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49C130
CGameEffect* CGameEffectBounceSpell::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectBounceSpell* copy = new CGameEffectBounceSpell(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x799C90
BOOL CGameEffectBounceSpell::ApplyEffect(CGameSprite* pSprite)
{
    // NOTE: The vtable's slot +8 is the shared "return 1" (ret 4) that
    // identical code folding made of every such override; the effect works
    // through the code that looks for it, not here.
    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49C1E0
CGameEffectMinHitPoints::CGameEffectMinHitPoints(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49C220
CGameEffect* CGameEffectMinHitPoints::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectMinHitPoints* copy = new CGameEffectMinHitPoints(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4C0100
BOOL CGameEffectMinHitPoints::ApplyEffect(CGameSprite* pSprite)
{
    if (pSprite->GetDerivedStats()->m_nMinHitPoints > 0) {
        m_done = TRUE;
    } else {
        pSprite->GetDerivedStats()->m_nMinHitPoints = m_effectAmount;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49C2D0
CGameEffectPowerWordKill::CGameEffectPowerWordKill(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49C310
CGameEffect* CGameEffectPowerWordKill::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectPowerWordKill* copy = new CGameEffectPowerWordKill(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4C0130
BOOL CGameEffectPowerWordKill::ApplyEffect(CGameSprite* pSprite)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
    // __LINE__: 24032
    UTIL_ASSERT_MSG(FALSE, "CGameEffectPowerWordKill game effect not supported in Icewind II (-rjf)");
}

// -----------------------------------------------------------------------------

// 0x49C3C0
CGameEffectPowerWordStun::CGameEffectPowerWordStun(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49C400
CGameEffect* CGameEffectPowerWordStun::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectPowerWordStun* copy = new CGameEffectPowerWordStun(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4C0160
BOOL CGameEffectPowerWordStun::ApplyEffect(CGameSprite* pSprite)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
    // __LINE__: 24057
    UTIL_ASSERT_MSG(FALSE, "CGameEffectPowerWordStun game effect not supported in Icewind II (-rjf)");
}

// -----------------------------------------------------------------------------

// 0x49C4B0
CGameEffectImprisonment::CGameEffectImprisonment(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49C4F0
CGameEffect* CGameEffectImprisonment::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectImprisonment* copy = new CGameEffectImprisonment(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4C0190
BOOL CGameEffectImprisonment::ApplyEffect(CGameSprite* pSprite)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
    // __LINE__: 24141
    UTIL_ASSERT_MSG(FALSE, "CGameEffectImprisonment game effect not supported in Icewind II (-rjf)");
}

// -----------------------------------------------------------------------------

// 0x49C5A0
CGameEffectFreedom::CGameEffectFreedom(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49C5E0
CGameEffect* CGameEffectFreedom::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectFreedom* copy = new CGameEffectFreedom(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4C01C0
BOOL CGameEffectFreedom::ApplyEffect(CGameSprite* pSprite)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
    // __LINE__: 24166
    UTIL_ASSERT_MSG(FALSE, "CGameEffectFreedom game effect not supported in Icewind II (-rjf)");
}

// -----------------------------------------------------------------------------

// 0x49C690
CGameEffectMaze::CGameEffectMaze(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49C6D0
CGameEffect* CGameEffectMaze::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectMaze* copy = new CGameEffectMaze(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4C01F0
BOOL CGameEffectMaze::ApplyEffect(CGameSprite* pSprite)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
    // __LINE__: 24214
    UTIL_ASSERT_MSG(FALSE, "CGameEffectMaze game effect not supported in Icewind II (-rjf)");
}

// -----------------------------------------------------------------------------

// 0x49C780
CGameEffectSecondaryCastList::CGameEffectSecondaryCastList(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49C7C0
CGameEffect* CGameEffectSecondaryCastList::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectSecondaryCastList* copy = new CGameEffectSecondaryCastList(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x799C90
BOOL CGameEffectSecondaryCastList::ApplyEffect(CGameSprite* pSprite)
{
    // NOTE: The vtable's slot +8 is the shared "return 1" (ret 4) that
    // identical code folding made of every such override; the effect works
    // through the code that looks for it, not here.
    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49C870
CGameEffectVisualEffect::CGameEffectVisualEffect(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49C8B0
CGameEffect* CGameEffectVisualEffect::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectVisualEffect* copy = new CGameEffectVisualEffect(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x799C90
BOOL CGameEffectVisualEffect::ApplyEffect(CGameSprite* pSprite)
{
    // NOTE: The vtable's slot +8 is the shared "return 1" (ret 4) that
    // identical code folding made of every such override; the effect works
    // through the code that looks for it, not here.
    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49C960
CGameEffectLevelDrain::CGameEffectLevelDrain(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49C9A0
CGameEffect* CGameEffectLevelDrain::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectLevelDrain* copy = new CGameEffectLevelDrain(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x799C90
BOOL CGameEffectLevelDrain::ApplyEffect(CGameSprite* pSprite)
{
    // NOTE: The vtable's slot +8 is the shared "return 1" (ret 4) that
    // identical code folding made of every such override; the effect works
    // through the code that looks for it, not here.
    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49CA50
CGameEffectPowerWordSleep::CGameEffectPowerWordSleep(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49CA90
CGameEffect* CGameEffectPowerWordSleep::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectPowerWordSleep* copy = new CGameEffectPowerWordSleep(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4C0220
BOOL CGameEffectPowerWordSleep::ApplyEffect(CGameSprite* pSprite)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
    // __LINE__: 24337
    UTIL_ASSERT_MSG(FALSE, "CGameEffectPowerWordSleep game effect not supported in Icewind II (-rjf)");
}

// -----------------------------------------------------------------------------

// 0x49CB40
CGameEffectStoneSkins::CGameEffectStoneSkins(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, TRUE)
{
}

// 0x49CB80
CGameEffect* CGameEffectStoneSkins::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectStoneSkins* copy = new CGameEffectStoneSkins(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4C0250
BOOL CGameEffectStoneSkins::ApplyEffect(CGameSprite* pSprite)
{
    if (m_dwFlags == 0) {
        // Stoneskin.
        if (m_secondaryType) {
            // NOTE: Uninline.
            DisplayStringRef(pSprite, 803);
        }

        pSprite->m_derivedStats.m_spellStates.set(SPLSTATE_STONESKIN, true);

        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 46);

        if (m_secondaryType) {
            DWORD nAmount = m_casterLevel * 10;
            if (nAmount > 149) {
                nAmount = 150;
            }
            m_effectAmount2 = nAmount;
        }

        // NOTE: The entry keeps pointers to m_effectAmount2 and this effect,
        // which dangle once the effect is removed (see 447C20.md). Kept.
        CDamageReduction reduction(10, 5);
        reduction.AddDamageType(0);
        reduction.AddDamageType(0x100000);
        reduction.AddDamageType(0x1000000);
        reduction.AddDamageType(0x2000000);
        reduction.AddDamageType(0x4000000);
        reduction.AddDamageType(0x8000000);
        reduction.SetSource(&m_effectAmount2, pSprite, this);
        pSprite->m_derivedStats.m_damageReductions.push_back(reduction);
    } else if (m_dwFlags == 1) {
        // Iron skins.
        if (m_secondaryType) {
            // NOTE: Uninline.
            DisplayStringRef(pSprite, 804);
        }

        pSprite->m_derivedStats.m_spellStates.set(SPLSTATE_IRON_SKINS, true);

        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 89);

        pSprite->m_derivedStats.m_nResistCrushing += 10;
        pSprite->m_derivedStats.m_nResistPiercing += 10;
        pSprite->m_derivedStats.m_nResistSlashing += 10;

        LONG nSkins = pSprite->m_derivedStats.m_nStoneSkins;
        if (nSkins <= m_effectAmount) {
            nSkins = m_effectAmount;
        }
        pSprite->m_derivedStats.m_nStoneSkins = nSkins;

        CMessage* message = new CMessageVisualEffect(23,
            static_cast<BYTE>(pSprite->m_derivedStats.m_nStoneSkins),
            pSprite->GetId(),
            pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    pSprite->SetColorRange(14);

    pSprite->m_derivedStats.m_naturalImmunities.insert(CGAMEEFFECT_STONESKINS);

    m_done = FALSE;
    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49CC30
CGameEffectProtectionCircle::CGameEffectProtectionCircle(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49CC70
CGameEffect* CGameEffectProtectionCircle::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectProtectionCircle* copy = new CGameEffectProtectionCircle(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4C0650
BOOL CGameEffectProtectionCircle::ApplyEffect(CGameSprite* pSprite)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
    // __LINE__: 24445
    UTIL_ASSERT_MSG(FALSE, "CGameEffectProtectionCircle game effect not supported in Icewind II (-rjf)");
}

// -----------------------------------------------------------------------------

// 0x49CD20
CGameEffectDispelSchool::CGameEffectDispelSchool(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49CD60
CGameEffect* CGameEffectDispelSchool::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectDispelSchool* copy = new CGameEffectDispelSchool(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x799C90
BOOL CGameEffectDispelSchool::ApplyEffect(CGameSprite* pSprite)
{
    // NOTE: The vtable's slot +8 is the shared "return 1" (ret 4) that
    // identical code folding made of every such override; the effect works
    // through the code that looks for it, not here.
    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49CE10
CGameEffectDispelSecondaryType::CGameEffectDispelSecondaryType(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49CE50
CGameEffect* CGameEffectDispelSecondaryType::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectDispelSecondaryType* copy = new CGameEffectDispelSecondaryType(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x799C90
BOOL CGameEffectDispelSecondaryType::ApplyEffect(CGameSprite* pSprite)
{
    // NOTE: The vtable's slot +8 is the shared "return 1" (ret 4) that
    // identical code folding made of every such override; the effect works
    // through the code that looks for it, not here.
    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49CF00
CGameEffectRandomTeleport::CGameEffectRandomTeleport(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49CF40
CGameEffect* CGameEffectRandomTeleport::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectRandomTeleport* copy = new CGameEffectRandomTeleport(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x799C90
BOOL CGameEffectRandomTeleport::ApplyEffect(CGameSprite* pSprite)
{
    // NOTE: The vtable's slot +8 is the shared "return 1" (ret 4) that
    // identical code folding made of every such override; the effect works
    // through the code that looks for it, not here.
    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49CFF0
CGameEffectImmunitySchoolDecrement::CGameEffectImmunitySchoolDecrement(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49D030
CGameEffect* CGameEffectImmunitySchoolDecrement::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectImmunitySchoolDecrement* copy = new CGameEffectImmunitySchoolDecrement(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4C0680
BOOL CGameEffectImmunitySchoolDecrement::ApplyEffect(CGameSprite* pSprite)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
    // __LINE__: 24543
    UTIL_ASSERT_MSG(FALSE, "CGameEffectImmunitySchoolDecrement game effect not supported in Icewind II (-rjf)");
}

// -----------------------------------------------------------------------------

// 0x49D0E0
CGameEffectLevelRestoration::CGameEffectLevelRestoration(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49D120
CGameEffect* CGameEffectLevelRestoration::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectLevelRestoration* copy = new CGameEffectLevelRestoration(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x799C90
BOOL CGameEffectLevelRestoration::ApplyEffect(CGameSprite* pSprite)
{
    // NOTE: The vtable's slot +8 is the shared "return 1" (ret 4) that
    // identical code folding made of every such override; the effect works
    // through the code that looks for it, not here.
    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49D1D0
CGameEffectRevealMagic::CGameEffectRevealMagic(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49D210
CGameEffect* CGameEffectRevealMagic::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectRevealMagic* copy = new CGameEffectRevealMagic(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4C06B0
BOOL CGameEffectRevealMagic::ApplyEffect(CGameSprite* pSprite)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
    // __LINE__: 24592
    UTIL_ASSERT_MSG(FALSE, "CGameEffectRevealMagic game effect not supported in Icewind II (-rjf)");
}

// -----------------------------------------------------------------------------

// 0x49D2C0
CGameEffectSecondaryTypeDecrement::CGameEffectSecondaryTypeDecrement(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49D300
CGameEffect* CGameEffectSecondaryTypeDecrement::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectSecondaryTypeDecrement* copy = new CGameEffectSecondaryTypeDecrement(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4C06E0
BOOL CGameEffectSecondaryTypeDecrement::ApplyEffect(CGameSprite* pSprite)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
    // __LINE__: 24620
    UTIL_ASSERT_MSG(FALSE, "CGameEffectSecondaryTypeDecrement game effect not supported in Icewind II (-rjf)");
}

// -----------------------------------------------------------------------------

// 0x49D3B0
CGameEffectBounceSchoolDecrement::CGameEffectBounceSchoolDecrement(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49D3F0
CGameEffect* CGameEffectBounceSchoolDecrement::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectBounceSchoolDecrement* copy = new CGameEffectBounceSchoolDecrement(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4C0710
BOOL CGameEffectBounceSchoolDecrement::ApplyEffect(CGameSprite* pSprite)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
    // __LINE__: 24645
    UTIL_ASSERT_MSG(FALSE, "CGameEffectBounceSchoolDecrement game effect not supported in Icewind II (-rjf)");
}

// -----------------------------------------------------------------------------

// 0x49D4A0
CGameEffectBounceSecondaryTypeDecrement::CGameEffectBounceSecondaryTypeDecrement(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49D4E0
CGameEffect* CGameEffectBounceSecondaryTypeDecrement::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectBounceSecondaryTypeDecrement* copy = new CGameEffectBounceSecondaryTypeDecrement(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4C0740
BOOL CGameEffectBounceSecondaryTypeDecrement::ApplyEffect(CGameSprite* pSprite)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
    // __LINE__: 24671
    UTIL_ASSERT_MSG(FALSE, "CGameEffectBounceSecondaryTypeDecrement game effect not supported in Icewind II (-rjf)");
}

// -----------------------------------------------------------------------------

// 0x49D590
CGameEffectDispelSchoolOne::CGameEffectDispelSchoolOne(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49D5D0
CGameEffect* CGameEffectDispelSchoolOne::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectDispelSchoolOne* copy = new CGameEffectDispelSchoolOne(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4C0770
BOOL CGameEffectDispelSchoolOne::ApplyEffect(CGameSprite* pSprite)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
    // __LINE__: 24696
    UTIL_ASSERT_MSG(FALSE, "CGameEffectDispelSchoolOne game effect not supported in Icewind II (-rjf)");
}

// -----------------------------------------------------------------------------

// 0x49D680
CGameEffectDispelSecondaryTypeOne::CGameEffectDispelSecondaryTypeOne(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49D6C0
CGameEffect* CGameEffectDispelSecondaryTypeOne::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectDispelSecondaryTypeOne* copy = new CGameEffectDispelSecondaryTypeOne(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4C07A0
BOOL CGameEffectDispelSecondaryTypeOne::ApplyEffect(CGameSprite* pSprite)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
    // __LINE__: 24696
    UTIL_ASSERT_MSG(FALSE, "CGameEffectDispelSecondaryTypeOne game effect not supported in Icewind II (-rjf)");
}

// -----------------------------------------------------------------------------

// 0x49D770
CGameEffectTimeStop::CGameEffectTimeStop(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49D7B0
CGameEffect* CGameEffectTimeStop::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectTimeStop* copy = new CGameEffectTimeStop(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4C07D0
BOOL CGameEffectTimeStop::ApplyEffect(CGameSprite* pSprite)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
    // __LINE__: 24747
    UTIL_ASSERT_MSG(FALSE, "CGameEffectTimeStop game effect not supported in Icewind II (-rjf)");
}

// -----------------------------------------------------------------------------

// 0x49D860
CGameEffectDisguise::CGameEffectDisguise(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
    field_18C = -1;
}

// 0x49D8A0
CGameEffect* CGameEffectDisguise::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectDisguise* copy = new CGameEffectDisguise(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    copy->field_18C = field_18C;
    return copy;
}

// 0x4C36C0
BOOL CGameEffectDisguise::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->field_5640 = field_188 == 0;

    // The new animation: the amount, or with 0 a commoner of the sprite's
    // gender and race.
    USHORT nAnimationID = static_cast<USHORT>(m_effectAmount);
    if (m_effectAmount == 0) {
        if (pSprite->GetAIType().m_nGender == 1) {
            switch (pSprite->GetAIType().m_nRace) {
            case 2:
            case 3:
                nAnimationID = 0x6001;
                break;
            case 4:
            case 6:
                nAnimationID = 0x6002;
                break;
            case 5:
                nAnimationID = 0x6003;
                break;
            case 7:
                nAnimationID = 0x6005;
                break;
            default:
                nAnimationID = 0x6000;
                break;
            }
        } else if (pSprite->GetAIType().m_nGender == 2) {
            switch (pSprite->GetAIType().m_nRace) {
            case 2:
            case 3:
                nAnimationID = 0x6011;
                break;
            case 4:
            case 6:
                nAnimationID = 0x6012;
                break;
            case 5:
                nAnimationID = 0x6013;
                break;
            case 7:
                nAnimationID = 0x6015;
                break;
            default:
                nAnimationID = 0x6010;
                break;
            }
        } else {
            nAnimationID = 0x6000;
        }
    }

    if (m_durationType == 1) {
        pSprite->m_baseStats.m_animationType = nAnimationID;
        m_done = TRUE;
    } else {
        m_done = FALSE;
    }

    if (pSprite->m_pPath != NULL) {
        CMessage* message = new CMessageDropPath(pSprite->GetId(), pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    if (!pSprite->m_bInUnmarshal) {
        pSprite->UnequipAll(TRUE);
    }

    // NOTE: Inlined CGameAnimation::GetListType (ObjAnimation.h line 2086)
    // and GetPersonalSpace (line 2093).
    if (pSprite->GetArea() != NULL) {
        if (pSprite->GetAnimation()->GetListType() != CGameObject::LIST_FLIGHT) {
            pSprite->GetArea()->m_search.RemoveObject(CPoint(pSprite->GetPos().x / CPathSearch::GRID_SQUARE_SIZEX,
                                                          pSprite->GetPos().y / CPathSearch::GRID_SQUARE_SIZEY),
                pSprite->GetAIType().m_nEnemyAlly,
                pSprite->GetAnimation()->GetPersonalSpace(),
                pSprite->field_54A8,
                pSprite->field_7430);
        }
    }

    // The animation to go back to.
    if (field_18C == -1) {
        field_18C = pSprite->GetAnimation()->m_animation->m_animationID;
    }

    // NOTE: Inlined CGameAnimation::SetAnimationType.
    pSprite->GetAnimation()->SetAnimationType(nAnimationID,
        pSprite->GetBaseStats()->m_colors,
        pSprite->GetDirection());

    if (pSprite->GetArea() != NULL) {
        if (pSprite->GetAnimation()->GetListType() != CGameObject::LIST_FLIGHT) {
            pSprite->GetArea()->m_search.AddObject(CPoint(pSprite->GetPos().x / CPathSearch::GRID_SQUARE_SIZEX,
                                                       pSprite->GetPos().y / CPathSearch::GRID_SQUARE_SIZEY),
                pSprite->GetAIType().m_nEnemyAlly,
                pSprite->GetAnimation()->GetPersonalSpace(),
                pSprite->field_54A8,
                pSprite->field_7430);
        }
    }

    if (!pSprite->m_bInUnmarshal) {
        pSprite->EquipAll(TRUE);
    }

    pSprite->JumpToPoint(pSprite->GetPos(), TRUE);
    pSprite->SetIdleSequence();

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49D980
CGameEffectHeroicInspiration::CGameEffectHeroicInspiration(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49D9C0
CGameEffect* CGameEffectHeroicInspiration::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectHeroicInspiration* copy = new CGameEffectHeroicInspiration(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4C3E20
BOOL CGameEffectHeroicInspiration::ApplyEffect(CGameSprite* pSprite)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameEffect.cpp
    // __LINE__: 27295
    UTIL_ASSERT(pSprite != NULL);

    if (pSprite->GetBaseStats()->m_hitPoints > pSprite->GetBaseStats()->m_maxHitPointsBase / 2) {
        m_done = TRUE;
        PlaySound(CResRef("EFF_E05"), pSprite);
    } else {
        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 138);

        pSprite->GetDerivedStats()->m_nTHAC0++;
        pSprite->GetDerivedStats()->m_nDamageBonus++;
        pSprite->GetDerivedStats()->m_nSaveVSFortitude++;
        pSprite->GetDerivedStats()->m_nSaveVSReflex++;
        pSprite->GetDerivedStats()->m_nSaveVSWill++;
        m_done = FALSE;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49DA70
CGameEffectPreventAISlowdown::CGameEffectPreventAISlowdown(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49DAB0
CGameEffect* CGameEffectPreventAISlowdown::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectPreventAISlowdown* copy = new CGameEffectPreventAISlowdown(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x4C3F10
BOOL CGameEffectPreventAISlowdown::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetDerivedStats()->m_bPreventAISlowDown = m_dwFlags;
    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4AD250
CPersistantEffect::CPersistantEffect()
{
    m_done = FALSE;
    m_deleted = FALSE;
    m_sourceID = -1;
    m_numDamage = 0;
    m_persistantType = 0;
    m_counter = 0;
}

// NOTE: Inlined.
CPersistantEffect::CPersistantEffect(const CPersistantEffect& other)
{
    m_done = other.m_done;
    m_deleted = other.m_deleted;
    m_sourceID = other.m_sourceID;
    m_numDamage = other.m_numDamage;
    m_persistantType = other.m_persistantType;
    m_counter = other.m_counter;
}

// 0x4A7460
CPersistantEffect::~CPersistantEffect()
{
}

// 0x4A7470
void CPersistantEffect::AIUpdate(CGameSprite* pSprite, LONG deltaT)
{
    // __FILE__: .\Include\CGameEffect.h
    // __LINE__: 3656
    UTIL_ASSERT(FALSE);
}

// 0x4A7440
CPersistantEffect* CPersistantEffect::Copy()
{
    // __FILE__: .\Include\CGameEffect.h
    // __LINE__: 3665
    UTIL_ASSERT_MSG(FALSE, "COPY missing");
}

// 0x4C1080
void CPersistantEffect::PlaySound(const CResRef& res, CGameSprite* pSprite)
{
    CSound cSound;

    CMessage* message = new CMessagePlaySoundRef(res, pSprite->GetId(), pSprite->GetId());
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

    cSound.SetResRef(res, TRUE, TRUE);

    if (cSound.m_nLooping == 0) {
        cSound.SetFireForget(TRUE);
    }

    cSound.SetChannel(14, reinterpret_cast<DWORD_PTR>(pSprite->GetArea()));
    cSound.Play(pSprite->GetPos().x, pSprite->GetPos().y, 0, FALSE);
}

// -----------------------------------------------------------------------------

// 0x4AD280
CPersistantEffectBurningDeath::CPersistantEffectBurningDeath()
{
    m_persistantType = 1;
    m_counter = 0;
    m_charredDegree = CVidPalette::NO_TINT;
    m_done = FALSE;
    m_charredIncrement = 0;
}

// 0x4AD2C0
void CPersistantEffectBurningDeath::AIUpdate(CGameSprite* pSprite, LONG deltaT)
{
    // __FILE__: .\Include\CGameEffect.h
    // __LINE__: 3681
    UTIL_ASSERT(FALSE);
}

// 0x4AD2E0
CPersistantEffect* CPersistantEffectBurningDeath::Copy()
{
    CPersistantEffectBurningDeath* copy = new CPersistantEffectBurningDeath();
    copy->m_charredDegree = m_charredDegree;
    copy->m_charredIncrement = m_charredIncrement;
    copy->m_counter = m_counter;
    copy->m_deleted = m_deleted;
    copy->m_done = m_done;
    copy->m_numDamage = m_numDamage;
    copy->m_persistantType = m_persistantType;
    copy->m_sourceID = m_sourceID;
    return copy;
}

// -----------------------------------------------------------------------------

// 0x4C1270
void CPersistantEffectFireDeath::AIUpdate(CGameSprite* pSprite, LONG deltaT)
{
    if (m_counter++ == 0) {
        pSprite->SetSequence(CGameSprite::SEQ_DIE);
        m_numDamage = rand() % 6 + 4;
        PlaySound(CResRef("FIRE"), pSprite);
        pSprite->StartSpriteEffect(1, 0, static_cast<BYTE>(8 * m_numDamage), TRUE);
        pSprite->GetAnimation()->SetColorEffectAll(0,
            RGB(m_charredDegree, m_charredDegree, m_charredDegree),
            1);
        m_done = FALSE;
        m_charredIncrement = (CVidPalette::NO_TINT - 75) / (2 * m_numDamage);
        m_charredDegree -= m_charredIncrement;
    } else {
        if (m_numDamage != 0) {
            pSprite->GetAnimation()->SetColorEffectAll(0,
                RGB(m_charredDegree, m_charredDegree, m_charredDegree),
                1);
            m_charredDegree -= m_charredIncrement;
            if ((m_counter & 1) == 0) {
                m_numDamage--;
            }
        } else {
            pSprite->GetAnimation()->SetColorEffectAll(0, RGB(75, 75, 75), 1);
            m_done = TRUE;
        }
    }
}

// -----------------------------------------------------------------------------

// 0x4C1430
void CPersistantEffectElectricDeath::AIUpdate(CGameSprite* pSprite, LONG deltaT)
{
    if (m_counter++ == 0) {
        pSprite->SetSequence(CGameSprite::SEQ_DIE);
        m_numDamage = rand() % 6 + 4;
        PlaySound(CResRef("ELEC2"), pSprite);
        pSprite->StartSpriteEffect(2, 0, static_cast<BYTE>(8 * m_numDamage), TRUE);
        pSprite->GetAnimation()->SetColorEffectAll(0,
            RGB(m_charredDegree, m_charredDegree, m_charredDegree),
            1);
        m_done = FALSE;
        m_charredIncrement = (CVidPalette::NO_TINT - 75) / (2 * m_numDamage);
        m_charredDegree -= m_charredIncrement;
    } else {
        if (m_numDamage != 0) {
            pSprite->GetAnimation()->SetColorEffectAll(0,
                RGB(m_charredDegree, m_charredDegree, m_charredDegree),
                1);
            m_charredDegree -= m_charredIncrement;
            if ((m_counter & 1) == 0) {
                m_numDamage--;
            }
        } else {
            pSprite->GetAnimation()->SetColorEffectAll(0, RGB(75, 75, 75), 1);
            m_done = TRUE;
        }
    }
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CPersistantEffectColorEffect::CPersistantEffectColorEffect()
{
    m_persistantType = 2;
    m_undo = FALSE;
    m_counter = 0;
    m_period = 0;
    m_effectDegreeBlue = CVidPalette::NO_TINT;
    m_effectDegreeGreen = CVidPalette::NO_TINT;
    m_effectDegreeRed = CVidPalette::NO_TINT;
    m_colorEffect = 0;
    m_done = FALSE;
    m_effectFinalBlue = 0;
    m_effectFinalGreen = 0;
    m_effectFinalRed = 0;
    m_effectIncrementBlue = 0;
    m_effectIncrementGreen = 0;
    m_effectIncrementRed = 0;
}

// 0x4C15F0
void CPersistantEffectColorEffect::AIUpdate(CGameSprite* pSprite, LONG deltaT)
{
    if (m_period == 0) {
        m_period = 1;
    }

    if (m_counter++ == 0) {
        pSprite->GetAnimation()->SetColorEffectAll(m_colorEffect,
            RGB(m_effectDegreeRed, m_effectDegreeGreen, m_effectDegreeBlue),
            1);
        m_done = FALSE;

        switch (m_colorEffect) {
        case 0:
            m_effectIncrementRed = (CVidPalette::NO_TINT - m_effectFinalRed) / m_period;
            m_effectDegreeRed -= m_effectIncrementRed;
            m_effectIncrementGreen = (CVidPalette::NO_TINT - m_effectFinalGreen) / m_period;
            m_effectDegreeGreen -= m_effectIncrementGreen;
            m_effectIncrementBlue = (CVidPalette::NO_TINT - m_effectFinalBlue) / m_period;
            m_effectDegreeBlue -= m_effectIncrementBlue;
            break;
        case 1:
        case 2:
            m_effectIncrementRed = m_effectFinalRed / m_period;
            m_effectDegreeRed += m_effectIncrementRed;
            m_effectIncrementGreen = m_effectFinalGreen / m_period;
            m_effectDegreeGreen += m_effectIncrementGreen;
            m_effectIncrementBlue = m_effectFinalBlue / m_period;
            m_effectDegreeBlue += m_effectIncrementBlue;
            break;
        case 3:
            m_effectIncrementRed = m_effectFinalRed / m_period;
            m_effectDegreeRed -= m_effectIncrementRed;
            m_effectIncrementGreen = m_effectFinalGreen / m_period;
            m_effectDegreeGreen -= m_effectIncrementGreen;
            m_effectIncrementBlue = m_effectFinalBlue / m_period;
            m_effectDegreeBlue -= m_effectIncrementBlue;
            break;
        }
    } else {
        // NOTE: The counter as incremented above (0x4C1608, compared at
        // 0x4C1777).
        if (m_counter < m_period) {
            pSprite->GetAnimation()->SetColorEffectAll(m_colorEffect,
                RGB(m_effectDegreeRed, m_effectDegreeGreen, m_effectDegreeBlue),
                1);

            switch (m_colorEffect) {
            case 0:
            case 3:
                m_effectDegreeRed -= m_effectIncrementRed;
                m_effectDegreeGreen -= m_effectIncrementGreen;
                m_effectDegreeBlue -= m_effectIncrementBlue;
                break;
            default:
                m_effectDegreeRed += m_effectIncrementRed;
                m_effectDegreeGreen += m_effectIncrementGreen;
                m_effectDegreeBlue += m_effectIncrementBlue;
                break;
            }

        } else {
            m_done = TRUE;

            if (m_undo) {
                pSprite->GetAnimation()->ClearColorEffectsAll();
                pSprite->field_562C = 1;
            }
        }
    }
}

// -----------------------------------------------------------------------------

// 0x4C18A0
CPersistantEffect84C4A4::CPersistantEffect84C4A4(int a1, int a2, int a3)
{
    field_18 = a2;
    field_1C = a3;
    field_24 = a1;
    field_28 = 1;

    ULONG delta = g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime - a2;
    if (delta == 0 || delta == 1) {
        field_20 = a2;
    } else {
        field_20 = a2 + a1 * (delta / a1 + 1);
    }
}

// NOTE: Inlined.
CPersistantEffect84C4A4::CPersistantEffect84C4A4(const CPersistantEffect84C4A4& other)
    : CPersistantEffect(other)
{
    field_18 = other.field_18;
    field_1C = other.field_1C;
    field_20 = other.field_20;
    field_24 = other.field_24;
    field_28 = other.field_28;
    memcpy(field_29, other.field_29, sizeof(field_29));
}

// 0x4C1920
void CPersistantEffect84C4A4::AIUpdate(CGameSprite* pSprite, LONG deltaT)
{
    int gameTime = g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime;
    if (gameTime > field_20
        && (field_1C == 0 || gameTime <= field_1C)) {
        int time = gameTime - deltaT;
        int index = 0;
        while (time < gameTime) {
            time += field_24;
            index++;
        }
        field_20 += index * field_24;
        ApplyEffect(pSprite, index);
    }
}

// -----------------------------------------------------------------------------

// 0x4C1980
CPersistantEffect84C4B4::CPersistantEffect84C4B4(const char* a1, int a2, int a3, int a4, int a5)
    : CPersistantEffect84C4A4(a3, a4, a5)
{
    field_34 = a1;
    field_54 = a2;
}

// NOTE: Inlined.
CPersistantEffect84C4B4::CPersistantEffect84C4B4(const CPersistantEffect84C4B4& other)
    : CPersistantEffect84C4A4(other)
{
    field_34 = other.field_34;
    field_44 = other.field_44;
    field_54 = other.field_54;
    field_58 = other.field_58;
}

// 0x4C1B00
CPersistantEffect84C4B4::~CPersistantEffect84C4B4()
{
}

// 0x4C1C00
void CPersistantEffect84C4B4::ApplyEffect(CGameSprite* pSprite, int index)
{
    CGameObject* pSource;
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    BYTE rc = pGame->GetObjectArray()->GetShare(m_sourceID,
        CGameObjectArray::THREAD_ASYNCH,
        &pSource,
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        return;
    }

    CProjectileArea* pProjectile = new CProjectileArea(static_cast<WORD>(field_54));

    // NOTE: Inlined `std::set::insert` (0x4C45A0) for each id.
    for (std::set<void*>::iterator it = field_58.begin(); it != field_58.end(); ++it) {
        pProjectile->m_excludedTargets.insert(static_cast<LONG>(reinterpret_cast<INT_PTR>(*it)));
    }

    if (index < 100) {
        for (int n = index; n > 0; n--) {
            const char* szRes = field_34.c_str() != NULL ? field_34.c_str() : "";
            std::list<CGameEffect*> effects = IcewindMisc::GetSpellResrefEffects(pSource,
                reinterpret_cast<const BYTE*>(szRes),
                1);
            for (std::list<CGameEffect*>::iterator it = effects.begin(); it != effects.end(); it++) {
                (*it)->m_sourceRes = CResRef(reinterpret_cast<const char*>(field_29));
                (*it)->m_target = pSprite->GetPos();
                pProjectile->AddEffect(*it);
            }
        }
    } else if (field_44.length() != 0) {
        const char* szRes = field_44.c_str() != NULL ? field_44.c_str() : "";
        std::list<CGameEffect*> effects = IcewindMisc::GetSpellResrefEffects(pSource,
            reinterpret_cast<const BYTE*>(szRes),
            1);
        for (std::list<CGameEffect*>::iterator it = effects.begin(); it != effects.end(); it++) {
            (*it)->m_sourceRes = CResRef(reinterpret_cast<const char*>(field_29));
            (*it)->m_target = pSprite->GetPos();
            pProjectile->AddEffect(*it);
        }
    }

    // NOTE: Fired even when it carries nothing.
    pProjectile->Fire(pSprite->GetArea(),
        pSource->GetId(),
        pSprite->GetId(),
        pSprite->GetPos(),
        0,
        0);

    pGame->GetObjectArray()->ReleaseShare(m_sourceID,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);
}

// 0x4C1F10
CPersistantEffect* CPersistantEffect84C4B4::Copy()
{
    return new CPersistantEffect84C4B4(*this);
}

// -----------------------------------------------------------------------------

// 0x4C2070
CPersistantEffect84C4C4::CPersistantEffect84C4C4(const char* a1, int a2, int a3, int a4)
    : CPersistantEffect84C4A4(a2, a3, a4)
{
    field_34 = a1;
}

// NOTE: Inlined.
CPersistantEffect84C4C4::CPersistantEffect84C4C4(const CPersistantEffect84C4C4& other)
    : CPersistantEffect84C4A4(other)
{
    field_34 = other.field_34;
    field_44 = other.field_44;
}

// 0x4C2160
CPersistantEffect84C4C4::~CPersistantEffect84C4C4()
{
}

// 0x4C21E0
void CPersistantEffect84C4C4::ApplyEffect(CGameSprite* pSprite, int index)
{
    CGameObject* pSource;
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    BYTE rc = pGame->GetObjectArray()->GetShare(m_sourceID,
        CGameObjectArray::THREAD_ASYNCH,
        &pSource,
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        return;
    }

    if (index < 100) {
        for (int n = index; n > 0; n--) {
            const char* szRes = field_34.c_str() != NULL ? field_34.c_str() : "";
            std::list<CGameEffect*> effects = IcewindMisc::GetSpellResrefEffects(pSource,
                reinterpret_cast<const BYTE*>(szRes),
                1);
            for (std::list<CGameEffect*>::iterator it = effects.begin(); it != effects.end(); it++) {
                (*it)->m_sourceRes = CResRef(reinterpret_cast<const char*>(field_29));
                (*it)->m_target = pSprite->GetPos();
                pSprite->AddEffect(*it, CGameAIBase::EFFECT_LIST_TIMED, FALSE, TRUE);
            }
        }
    } else if (field_44.length() != 0) {
        const char* szRes = field_44.c_str() != NULL ? field_44.c_str() : "";
        std::list<CGameEffect*> effects = IcewindMisc::GetSpellResrefEffects(pSource,
            reinterpret_cast<const BYTE*>(szRes),
            1);
        for (std::list<CGameEffect*>::iterator it = effects.begin(); it != effects.end(); it++) {
            (*it)->m_sourceRes = CResRef(reinterpret_cast<const char*>(field_29));
            (*it)->m_target = pSprite->GetPos();
            pSprite->AddEffect(*it, CGameAIBase::EFFECT_LIST_TIMED, FALSE, TRUE);
        }
    }

    pGame->GetObjectArray()->ReleaseShare(m_sourceID,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);
}

// 0x4C2440
CPersistantEffect* CPersistantEffect84C4C4::Copy()
{
    return new CPersistantEffect84C4C4(*this);
}

// -----------------------------------------------------------------------------

// 0x4C2570
CPersistantEffect84C420::CPersistantEffect84C420(int a1, int a2, int a3, int a4, int a5)
    : CPersistantEffect84C4A4(a3, a4, a5)
{
    field_34 = a1;
    field_38 = a2;
}

// NOTE: Inlined.
CPersistantEffect84C420::CPersistantEffect84C420(const CPersistantEffect84C420& other)
    : CPersistantEffect84C4A4(other)
{
    field_34 = other.field_34;
    field_38 = other.field_38;
}

// 0x4C25B0
void CPersistantEffect84C420::ApplyEffect(CGameSprite* pSprite, int index)
{
    CGameEffect* pEffect = new CGameEffectDamage();
    pEffect->m_dwFlags = field_34;
    pEffect->m_sourceID = m_sourceID;
    pEffect->m_effectAmount = index * field_38;
    pSprite->AddEffect(pEffect,
        CGameAIBase::EFFECT_LIST_TIMED,
        TRUE,
        TRUE);
}

// 0x4C27F0
CPersistantEffect* CPersistantEffect84C420::Copy()
{
    return new CPersistantEffect84C420(*this);
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CPersistantEffect84C494::CPersistantEffect84C494(int a1, int a2, int a3, int a4)
    : CPersistantEffect84C4A4(a2, a3, a4)
{
    field_34 = a1;

    // NOTE: Seen inlined in CGameEffectRegeneration::ApplyEffect (0x4B6620):
    // a first tick due at the start time moves one period on.
    if (field_20 == field_18) {
        field_20 = field_24 + field_18;
    }
}

// NOTE: Inlined.
CPersistantEffect84C494::CPersistantEffect84C494(const CPersistantEffect84C494& other)
    : CPersistantEffect84C4A4(other)
{
    field_34 = other.field_34;
}

// 0x4C2890
void CPersistantEffect84C494::ApplyEffect(CGameSprite* pSprite, int index)
{
    if (pSprite->GetBaseStats()->m_hitPoints < pSprite->GetDerivedStats()->m_nMaxHitPoints) {
        CGameEffectHeal* pEffect = new CGameEffectHeal();
        pEffect->m_sourceID = m_sourceID;
        pEffect->m_effectAmount = index * field_34;
        pSprite->AddEffect(pEffect,
            CGameAIBase::EFFECT_LIST_TIMED,
            TRUE,
            TRUE);
    }
}

// 0x4C2990
CPersistantEffect* CPersistantEffect84C494::Copy()
{
    return new CPersistantEffect84C494(*this);
}

// -----------------------------------------------------------------------------

// 0x4C2A20
CPersistantEffect84C4D4::CPersistantEffect84C4D4(int a1, int a2, int a3, int a4)
    : CPersistantEffect84C4A4(a2, a3, a4)
{
    field_34 = "";
    field_54 = a1;
}

// NOTE: Inlined.
CPersistantEffect84C4D4::CPersistantEffect84C4D4(const CPersistantEffect84C4D4& other)
    : CPersistantEffect84C4A4(other)
{
    field_34 = other.field_34;
    field_44 = other.field_44;
    field_54 = other.field_54;
    field_58 = other.field_58;
}

// 0x4C2BB0
CPersistantEffect84C4D4::~CPersistantEffect84C4D4()
{
}

// 0x4C2CB0
void CPersistantEffect84C4D4::ApplyEffect(CGameSprite* pSprite, int index)
{
    CGameObject* pSource;
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // Not in the first two ticks after the start.
    LONG nGameTime = pGame->GetWorldTimer()->m_gameTime;
    if (nGameTime == field_18 || nGameTime - field_18 == 1) {
        return;
    }

    BYTE rc = pGame->GetObjectArray()->GetShare(m_sourceID,
        CGameObjectArray::THREAD_ASYNCH,
        &pSource,
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        return;
    }

    for (int n = index; n > 0; n--) {
        // NOTE: The string at 0x8AB108.
        std::list<CGameEffect*> effects = IcewindMisc::GetSpellResrefEffects(pSource,
            reinterpret_cast<const BYTE*>("EffSoF1"),
            1);
        for (std::list<CGameEffect*>::iterator it = effects.begin(); it != effects.end(); it++) {
            (*it)->m_sourceRes = CResRef(reinterpret_cast<const char*>(field_29));
            pSprite->AddEffect(*it, CGameAIBase::EFFECT_LIST_TIMED, FALSE, TRUE);
        }
    }

    CProjectileArea850690* pProjectile = new CProjectileArea850690(static_cast<WORD>(field_54));

    // NOTE: Inlined `std::set::insert` (0x4C45A0, then 0x4C4690 for each id
    // of field_58).
    pProjectile->m_excludedTargets.insert(pSprite->GetId());
    for (std::set<void*>::iterator it = field_58.begin(); it != field_58.end(); ++it) {
        pProjectile->m_excludedTargets.insert(static_cast<LONG>(reinterpret_cast<INT_PTR>(*it)));
    }

    for (int n = index; n > 0; n--) {
        // NOTE: The string at 0x8AB100.
        std::list<CGameEffect*> effects = IcewindMisc::GetSpellResrefEffects(pSource,
            reinterpret_cast<const BYTE*>("EffSoF2"),
            1);
        for (std::list<CGameEffect*>::iterator it = effects.begin(); it != effects.end(); it++) {
            (*it)->m_sourceRes = CResRef(reinterpret_cast<const char*>(field_29));
            // NOTE: An unsigned division (0x4C2EED).
            (*it)->m_duration = static_cast<DWORD>(field_1C - pGame->GetWorldTimer()->m_gameTime) / 15;
            pProjectile->AddEffect(*it);
        }
    }

    pProjectile->Fire(pSprite->GetArea(),
        pSource->GetId(),
        pSprite->GetId(),
        pSprite->GetPos(),
        0,
        0);

    pGame->GetObjectArray()->ReleaseShare(m_sourceID,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);
}

// 0x4C3000
CPersistantEffect* CPersistantEffect84C4D4::Copy()
{
    return new CPersistantEffect84C4D4(*this);
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CPersistantEffect84C484::CPersistantEffect84C484(int a1, const char* a2, int a3, int a4, int a5, int a6)
    : CPersistantEffect84C4B4(a2, a3, a4, a5, a6)
{
    field_68 = a1;
}

// NOTE: Inlined.
CPersistantEffect84C484::CPersistantEffect84C484(const CPersistantEffect84C484& other)
    : CPersistantEffect84C4B4(other)
{
    field_68 = other.field_68;
}

// 0x4C3180
CPersistantEffect84C484::~CPersistantEffect84C484()
{
}

// 0x4C3280
void CPersistantEffect84C484::ApplyEffect(CGameSprite* pSprite, int index)
{
    CGameObject* pSource;
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    BYTE rc = pGame->GetObjectArray()->GetShare(m_sourceID,
        CGameObjectArray::THREAD_ASYNCH,
        &pSource,
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        return;
    }

    // The damage dice fall by one each period since the start: field_68
    // less the periods elapsed, d6 magic damage.
    LONG nElapsed = pGame->GetWorldTimer()->m_gameTime - field_18;
    LONG nDice = field_68 - nElapsed / field_24;
    if (nDice > 0) {
        CGameEffect* pDamage = IcewindMisc::CreateEffectDamage(pSource, 0x40000000, nDice, 6, 0, 3, 0);
        pDamage->m_sourceRes = CResRef(reinterpret_cast<const char*>(field_29));
        pSprite->AddEffect(pDamage, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);
    }

    if (nElapsed != 0 && nElapsed != 1) {
        CProjectileArea8505F4* pProjectile = new CProjectileArea8505F4(static_cast<WORD>(field_54));

        // NOTE: Inlined `std::set::insert` (0x4C4690).
        pProjectile->m_excludedTargets.insert(pSprite->GetId());
        for (std::set<void*>::iterator it = field_58.begin(); it != field_58.end(); ++it) {
            pProjectile->m_excludedTargets.insert(static_cast<LONG>(reinterpret_cast<INT_PTR>(*it)));
        }

        for (int n = index; n > 0; n--) {
            // NOTE: The string at 0x8AB110.
            std::list<CGameEffect*> effects = IcewindMisc::GetSpellResrefEffects(pSource,
                reinterpret_cast<const BYTE*>("EffMT1"),
                1);
            for (std::list<CGameEffect*>::iterator it = effects.begin(); it != effects.end(); it++) {
                (*it)->m_sourceRes = CResRef(reinterpret_cast<const char*>(field_29));
                pProjectile->AddEffect(*it);
            }
        }

        pProjectile->Fire(pSprite->GetArea(),
            pSource->GetId(),
            pSprite->GetId(),
            pSprite->GetPos(),
            0,
            0);
    }

    pGame->GetObjectArray()->ReleaseShare(m_sourceID,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);
}

// 0x4C3570
CPersistantEffect* CPersistantEffect84C484::Copy()
{
    return new CPersistantEffect84C484(*this);
}

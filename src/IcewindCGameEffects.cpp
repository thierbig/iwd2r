#include "IcewindCGameEffects.h"

#include "CAIObjectType.h"
#include "CBaldurChitin.h"
#include "CCreatureFile.h"
#include "CGameAnimationType.h"
#include "CGameArea.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CPathSearch.h"
#include "CProjectile.h"
#include "CProjectile850F9C.h"
#include "CProjectileArea.h"
#include "CProjectileSingleTarget.h"
#include "CScreenWorld.h"
#include "CUtil.h"
#include "Icewind586B70.h"
#include "IcewindCVisualEffect.h"
#include "IcewindMisc.h"

#include <algorithm>
#include <cctype>
#include <list>
#include <string>

// NOTE: I'm not sure where these constants belong.

// 0x8AEBCC
const char RESREF_EYE_OF_THE_MIND[] = "SpIn126";

// 0x8AEBD0
const char RESREF_EYE_OF_THE_SWORD[] = "SpIn127";

// 0x8AEBD4
const char RESREF_EYE_OF_THE_MAGE[] = "SpIn128";

// 0x8AEBD8
const char RESREF_EYE_OF_VENOM[] = "SpIn129";

// 0x8AEBDC
const char RESREF_EYE_OF_THE_SPIRIT[] = "SpIn130";

// 0x8AEBE0
const char RESREF_EYE_OF_FORTITUDE[] = "SpIn131";

// 0x8AEBE4
const char RESREF_EYE_OF_STONE[] = "SpIn132";

// 0x8AEBE8
const char RESREF_BEHOLDER_DISPEL_MAGIC[] = "SpIn164";

// 0x8AEBEC
const char RESREF_HARPY_WAIL[] = "SpIn166";

// 0x8AEBF0
const char RESREF_JACKALWERE_GAZE[] = "SpIn179";

// 0x8AF154
const char RESREF_UMBER_HULK_GAZE[] = "SpIn994";

// NOTE: Its `this` is the game, and the table it reads, at +0x1300, is
// CRuleTables::m_tTracking (the rule tables start the CInfGame object): a
// CRuleTables member by what it reads. Kept here, file-static, until
// CRuleTables.cpp's owner takes it; only the ranger tracking effect (opcode
// 286) calls it. The area's row in TRACKING.2DA: when the sprite has
// wilderness lore, a d20 plus the skill against the row's difficulty / 5 +
// 10 gives the row's string; otherwise 19534.
// NOTE: Ruling M3-12: on the fold list; it moves into CRuleTables.
// 0x5466F0
static STRREF RuleTables_GetTrackingString5466F0(const CRuleTables& ruleTables, CGameSprite* pSprite, CString& sAreaName)
{
    STRREF strTracks = 19534;

    // NOTE: The original leaves the point unset and reads its y when the
    // search fails (0x546719); ours starts it at -1, the found-nothing case.
    CPoint pt(-1, -1);
    ruleTables.m_tTracking.Find(sAreaName, pt, TRUE);
    if (pt.y == -1) {
        return 19534;
    }

    INT nSkill = pSprite->m_derivedStats.m_nSkills[CGAMESPRITE_SKILL_WILDERNESS_LORE];
    if (nSkill > 0) {
        // NOTE: The original calls it and drops the result (0x546746).
        pSprite->sub_763E40(CGAMESPRITE_SKILL_WILDERNESS_LORE);

        pt.x = 1;
        INT nDifficulty = ruleTables.m_tTracking.GetAtLong(pt) / 5;

        INT nRoll = rand() % 20 + 1;
        pSprite->FeedBack(CGameSprite::FEEDBACK_ROLL,
            nRoll + nSkill,
            nDifficulty + 10,
            nSkill,
            40285,
            0,
            0);

        if (nRoll + nSkill >= nDifficulty + 10) {
            pt.x = 0;
            strTracks = ruleTables.m_tTracking.GetAtLong(pt);
        }
    }

    return strTracks;
}

// 0x49DB70
IcewindCGameEffectBeltynsBurningBlood::IcewindCGameEffectBeltynsBurningBlood(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, TRUE)
{
}

// 0x49DBB0
CGameEffect* IcewindCGameEffectBeltynsBurningBlood::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectBeltynsBurningBlood* copy = new IcewindCGameEffectBeltynsBurningBlood(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x55DEB0
BOOL IcewindCGameEffectBeltynsBurningBlood::ApplyEffect(CGameSprite* pSprite)
{
    if (IcewindMisc::IsDead(pSprite) == TRUE) {
        m_done = TRUE;
        return TRUE;
    }

    if (m_effectAmount > 0) {
        LONG nDelay = 7;
        for (LONG nRound = 1; nRound < m_effectAmount; nRound++) {
            CGameEffect* copy = Copy();
            copy->m_duration = nDelay;
            copy->m_effectAmount = 0;
            copy->m_flags |= 0x2;
            // NOTE: The duration type is the WORD constant at 0x8491B0 (4).
            copy->m_durationType = 4;
            copy->m_casterLevel = m_casterLevel;
            pSprite->AddEffect(copy, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);
            nDelay += 7;
        }
    }

    CGameObject* pSource;
    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_sourceID,
        CGameObjectArray::THREAD_ASYNCH,
        &pSource,
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        m_done = TRUE;
        return TRUE;
    }

    CGameEffect* pEffect;

    pEffect = IcewindMisc::sub_5853B0(pSource, m_numDice, m_diceSize, 0, static_cast<BYTE>(m_spellLevel), 4);
    pEffect->m_flags |= 0x2;
    pEffect->m_sourceRes = m_sourceRes;
    pEffect->m_casterLevel = m_casterLevel;
    pSprite->AddEffect(pEffect, CGameAIBase::EFFECT_LIST_TIMED, FALSE, TRUE);

    pEffect = IcewindMisc::CreateEffectBerserk(pSource, 7, static_cast<BYTE>(m_spellLevel), 4);
    pEffect->m_flags |= 0x2;
    pEffect->m_durationType = 1;
    pEffect->m_sourceRes = m_sourceRes;
    pEffect->m_casterLevel = m_casterLevel;
    pSprite->AddEffect(pEffect, CGameAIBase::EFFECT_LIST_TIMED, FALSE, TRUE);

    pEffect = IcewindMisc::CreateEffectPortraitIcon(pSource, 4, 7, static_cast<BYTE>(m_spellLevel), 4);
    pEffect->m_flags |= 0x2;
    pEffect->m_sourceRes = m_sourceRes;
    pEffect->m_casterLevel = m_casterLevel;
    pSprite->AddEffect(pEffect, CGameAIBase::EFFECT_LIST_TIMED, FALSE, TRUE);

    pEffect = IcewindMisc::CreateEffectDisplayString(pSource, 14036, static_cast<BYTE>(m_spellLevel), 4);
    pEffect->m_flags |= 0x2;
    pEffect->m_sourceRes = m_sourceRes;
    pEffect->m_casterLevel = m_casterLevel;
    pSprite->AddEffect(pEffect, CGameAIBase::EFFECT_LIST_TIMED, FALSE, TRUE);

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceID,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49DC60
IcewindCGameEffectCastingGlow::IcewindCGameEffectCastingGlow(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49DCA0
CGameEffect* IcewindCGameEffectCastingGlow::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectCastingGlow* copy = new IcewindCGameEffectCastingGlow(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x55E130
BOOL IcewindCGameEffectCastingGlow::ApplyEffect(CGameSprite* pSprite)
{
    // The glow lasts what is left of the effect, in ticks, as a byte.
    BYTE nDuration = static_cast<BYTE>(m_duration - g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime);

    IcewindCVisualEffect visualEffect;
    visualEffect.sub_586A90(TRUE);

    WORD nProjectileType;
    CProjectile850F9C* pProjectile;
    switch (m_dwFlags) {
    case 1:
        nProjectileType = 113;
        pProjectile = new CProjectile850F9C(CString("AbjurCG"), CString(""), nDuration, 0, visualEffect);
        break;
    case 2:
        nProjectileType = 115;
        pProjectile = new CProjectile850F9C(CString("ConjuCG"), CString(""), nDuration, 0, visualEffect);
        break;
    case 3:
        nProjectileType = 117;
        pProjectile = new CProjectile850F9C(CString("DivinCG"), CString(""), nDuration, 0, visualEffect);
        break;
    case 4:
        nProjectileType = 112;
        pProjectile = new CProjectile850F9C(CString("EnchaCG"), CString(""), nDuration, 0, visualEffect);
        break;
    case 5:
        nProjectileType = 114;
        pProjectile = new CProjectile850F9C(CString("IllusCG"), CString(""), nDuration, 0, visualEffect);
        break;
    case 6:
        nProjectileType = 116;
        pProjectile = new CProjectile850F9C(CString("InvocCG"), CString(""), nDuration, 0, visualEffect);
        break;
    case 7:
        nProjectileType = 110;
        pProjectile = new CProjectile850F9C(CString("NecroCG"), CString(""), nDuration, 0, visualEffect);
        break;
    case 8:
        nProjectileType = 111;
        pProjectile = new CProjectile850F9C(CString("AlterCG"), CString(""), nDuration, 0, visualEffect);
        break;
    default:
        // DEVIATION: for another type the original keeps the sprite in its
        // projectile register (0x55E76A). It calls DetermineHeight with the
        // sprite as the projectile, queues a message whose type is the
        // uninitialised stack slot holding the duration byte, and calls the
        // sprite's own vtable slot 0x6C with Fire's seven arguments, which
        // unbalances the stack. Bounded (ruling P4-10): ours queues and fires
        // nothing.
        m_done = TRUE;
        return TRUE;
    }

    CMessage* message = new CMessageFireProjectile(nProjectileType,
        pSprite->GetId(),
        pSprite->GetPos(),
        pProjectile->DetermineHeight(pSprite),
        pSprite->GetId(),
        pSprite->GetId(),
        nDuration);
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

    pProjectile->Fire(pSprite->GetArea(),
        pSprite->GetId(),
        pSprite->GetId(),
        pSprite->GetPos(),
        pProjectile->DetermineHeight(pSprite),
        0);

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49DD50
IcewindCGameEffectPrayer::IcewindCGameEffectPrayer(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49DD90
CGameEffect* IcewindCGameEffectPrayer::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectPrayer* copy = new IcewindCGameEffectPrayer(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x55E850
BOOL IcewindCGameEffectPrayer::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        if (!pSprite->GetDerivedStats()->m_spellStates.test(SPLSTATE_BENEFICIAL_PRAYER)) {
            pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_BENEFICIAL_PRAYER, true);

            // NOTE: Uninline.
            AddPortraitIcon(pSprite, 62);

            AddColorEffect(pSprite, 215, 182, 0, 30);

            pSprite->GetDerivedStats()->m_nDamageBonus++;
            pSprite->GetDerivedStats()->m_nTHAC0++;
            pSprite->GetDerivedStats()->m_nSaveVSFortitude++;
            pSprite->GetDerivedStats()->m_nSaveVSReflex++;
            pSprite->GetDerivedStats()->m_nSaveVSWill++;
        }
        break;
    case 1:
        if (!pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_DETRIMENTAL_PRAYER]) {
            pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_DETRIMENTAL_PRAYER, true);

            // NOTE: Uninline.
            AddPortraitIcon(pSprite, 35);

            AddColorEffect(pSprite, 128, 0, 92, 30);

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
void IcewindCGameEffectPrayer::OnRemove(CGameSprite* pSprite)
{
    PlaySound(CResRef("EFF_E03"), pSprite);
}

// -----------------------------------------------------------------------------

// 0x49DF30
IcewindCGameEffectRighteousWrathOfFaithful::IcewindCGameEffectRighteousWrathOfFaithful(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49DF70
CGameEffect* IcewindCGameEffectRighteousWrathOfFaithful::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectRighteousWrathOfFaithful* copy = new IcewindCGameEffectRighteousWrathOfFaithful(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x55E9C0
BOOL IcewindCGameEffectRighteousWrathOfFaithful::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        if (!pSprite->GetDerivedStats()->m_spellStates.test(SPLSTATE_ALLIED_RIGHTEOUS_WRATH_OF_THE_FAITHFUL)) {
            pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_ALLIED_RIGHTEOUS_WRATH_OF_THE_FAITHFUL, true);

            // NOTE: Uninline.
            AddPortraitIcon(pSprite, 67);

            AddColorEffect(pSprite, 215, 182, 0, 30);

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
        break;
    case 1:
        if (!pSprite->GetDerivedStats()->m_spellStates.test(SPLSTATE_ALLIED_AND_SAME_ALIGNMENT_RIGHTEOUS_WRATH_OF_THE_FAITHFUL)) {
            pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_ALLIED_AND_SAME_ALIGNMENT_RIGHTEOUS_WRATH_OF_THE_FAITHFUL, true);

            // NOTE: Uninline.
            AddPortraitIcon(pSprite, 67);

            AddColorEffect(pSprite, 215, 182, 0, 30);

            pSprite->GetDerivedStats()->m_nDamageBonus += 2;
            pSprite->GetDerivedStats()->m_nTHAC0 += 2;
            pSprite->GetDerivedStats()->m_nSaveVSFortitude += 2;
            pSprite->GetDerivedStats()->m_nSaveVSReflex += 2;
            pSprite->GetDerivedStats()->m_nSaveVSWill += 2;

            if (m_secondaryType) {
                pSprite->GetBaseStats()->m_hitPoints += 8;
            }

            if (pSprite->GetDerivedStats()->m_nNumberOfAttacks < 5) {
                pSprite->GetDerivedStats()->m_nNumberOfAttacks++;
            }
        }
        break;
    }
    return TRUE;
}

// 0x55EB80
void IcewindCGameEffectRighteousWrathOfFaithful::OnRemove(CGameSprite* pSprite)
{
    pSprite->m_bonusStats.m_nFatigue += 8;
    PlaySound(CResRef("EFF_E01"), pSprite);
}

// -----------------------------------------------------------------------------

// 0x49DE40
IcewindCGameEffectRecitation::IcewindCGameEffectRecitation(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49DE80
CGameEffect* IcewindCGameEffectRecitation::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectRecitation* copy = new IcewindCGameEffectRecitation(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x55EBB0
BOOL IcewindCGameEffectRecitation::ApplyEffect(CGameSprite* pSprite)
{
    SHORT nModifier;

    switch (m_dwFlags) {
    case 0:
        if (!pSprite->GetDerivedStats()->m_spellStates.test(SPLSTATE_BENEFICIAL_RECITATION)) {
            pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_BENEFICIAL_RECITATION, true);

            // NOTE: Uninline.
            AddPortraitIcon(pSprite, 64);

            AddColorEffect(pSprite, 160, 128, 96, 30);

            nModifier = 2;
            pSprite->GetDerivedStats()->m_nDamageBonus += nModifier;
            pSprite->GetDerivedStats()->m_nTHAC0 += nModifier;
            pSprite->GetDerivedStats()->m_nSaveVSFortitude += nModifier;
            pSprite->GetDerivedStats()->m_nSaveVSReflex += nModifier;
            pSprite->GetDerivedStats()->m_nSaveVSWill += nModifier;
        }
        break;
    case 1:
        if (!pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_DETRIMENTAL_RECITATION]) {
            pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_DETRIMENTAL_RECITATION, true);

            // NOTE: Uninline.
            AddPortraitIcon(pSprite, 35);

            AddColorEffect(pSprite, 128, 0, 192, 30);

            nModifier = -2;
            pSprite->GetDerivedStats()->m_nDamageBonus += nModifier;
            pSprite->GetDerivedStats()->m_nTHAC0 += nModifier;
            pSprite->GetDerivedStats()->m_nSaveVSFortitude += nModifier;
            pSprite->GetDerivedStats()->m_nSaveVSReflex += nModifier;
            pSprite->GetDerivedStats()->m_nSaveVSWill += nModifier;
        }
        break;
    }
    return TRUE;
}

// 0x55ECF0
void IcewindCGameEffectRecitation::OnRemove(CGameSprite* pSprite)
{
    PlaySound(CResRef("EFF_E03"), pSprite);
}

// -----------------------------------------------------------------------------

// 0x49E020
IcewindCGameEffectSavingThrowChange::IcewindCGameEffectSavingThrowChange(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49E060
CGameEffect* IcewindCGameEffectSavingThrowChange::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectSavingThrowChange* copy = new IcewindCGameEffectSavingThrowChange(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x55ED20
BOOL IcewindCGameEffectSavingThrowChange::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        sub_55ED90(pSprite);
        break;
    case 1:
        sub_55EE70(pSprite);
        break;
    case 2:
        sub_55EFB0(pSprite);
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\IcewindCGameEffects.cpp
        // __LINE__: 809
        UTIL_ASSERT(FALSE);
    }

    return TRUE;
}

// 0x55ED90
void IcewindCGameEffectSavingThrowChange::sub_55ED90(CGameSprite* pSprite)
{
    if (m_durationType == 1) {
        CCreatureFileHeader* pBStats = pSprite->GetBaseStats();
        pBStats->m_saveVSFortitudeBase += static_cast<BYTE>(m_effectAmount);
        if (pBStats->m_saveVSFortitudeBase > 40) {
            pBStats->m_saveVSFortitudeBase = 40;
        }
        if (pBStats->m_saveVSFortitudeBase < -40) {
            pBStats->m_saveVSFortitudeBase = -40;
        }

        pBStats->m_saveVSReflexBase += static_cast<BYTE>(m_effectAmount);
        if (pBStats->m_saveVSReflexBase > 40) {
            pBStats->m_saveVSReflexBase = 40;
        }
        if (pBStats->m_saveVSReflexBase < -40) {
            pBStats->m_saveVSReflexBase = -40;
        }

        pBStats->m_saveVSWillBase += static_cast<BYTE>(m_effectAmount);
        if (pBStats->m_saveVSWillBase > 40) {
            pBStats->m_saveVSWillBase = 40;
        }
        if (pBStats->m_saveVSWillBase < -40) {
            pBStats->m_saveVSWillBase = -40;
        }
        m_forceRepass = TRUE;
        m_done = TRUE;
    } else {
        pSprite->m_bonusStats.m_nSaveVSFortitude += static_cast<SHORT>(m_effectAmount);
        pSprite->m_bonusStats.m_nSaveVSReflex += static_cast<SHORT>(m_effectAmount);
        pSprite->m_bonusStats.m_nSaveVSWill += static_cast<SHORT>(m_effectAmount);
        m_done = FALSE;
    }
}

// 0x55EE70
void IcewindCGameEffectSavingThrowChange::sub_55EE70(CGameSprite* pSprite)
{
    if (m_durationType == 1) {
        CCreatureFileHeader* pBStats = pSprite->GetBaseStats();
        pBStats->m_saveVSFortitudeBase = static_cast<BYTE>(m_effectAmount);
        if (pBStats->m_saveVSFortitudeBase > 40) {
            pBStats->m_saveVSFortitudeBase = 40;
        }
        if (pBStats->m_saveVSFortitudeBase < -40) {
            pBStats->m_saveVSFortitudeBase = -40;
        }

        pBStats->m_saveVSReflexBase = static_cast<BYTE>(m_effectAmount);
        if (pBStats->m_saveVSReflexBase > 40) {
            pBStats->m_saveVSReflexBase = 40;
        }
        if (pBStats->m_saveVSReflexBase < -40) {
            pBStats->m_saveVSReflexBase = -40;
        }

        pBStats->m_saveVSWillBase = static_cast<BYTE>(m_effectAmount);
        if (pBStats->m_saveVSWillBase > 40) {
            pBStats->m_saveVSWillBase = 40;
        }
        if (pBStats->m_saveVSWillBase < -40) {
            pBStats->m_saveVSWillBase = -40;
        }
        m_forceRepass = TRUE;
        m_done = TRUE;
    } else {
        CDerivedStats* pDStats = pSprite->GetDerivedStats();
        pDStats->m_nSaveVSFortitude = static_cast<BYTE>(m_effectAmount);
        if (pDStats->m_nSaveVSFortitude > 40) {
            pDStats->m_nSaveVSFortitude = 40;
        }
        if (pDStats->m_nSaveVSFortitude < -40) {
            pDStats->m_nSaveVSFortitude = -40;
        }

        pDStats->m_nSaveVSReflex = static_cast<BYTE>(m_effectAmount);
        if (pDStats->m_nSaveVSReflex > 40) {
            pDStats->m_nSaveVSReflex = 40;
        }
        if (pDStats->m_nSaveVSReflex < -40) {
            pDStats->m_nSaveVSReflex = -40;
        }

        pDStats->m_nSaveVSWill = static_cast<BYTE>(m_effectAmount);
        if (pDStats->m_nSaveVSWill > 40) {
            pDStats->m_nSaveVSWill = 40;
        }
        if (pDStats->m_nSaveVSWill < -40) {
            pDStats->m_nSaveVSWill = -40;
        }
        m_done = FALSE;
    }
}

// 0x55EFB0
void IcewindCGameEffectSavingThrowChange::sub_55EFB0(CGameSprite* pSprite)
{
    if (m_durationType == 1) {
        CCreatureFileHeader* pBStats = pSprite->GetBaseStats();
        pBStats->m_saveVSFortitudeBase = static_cast<SHORT>(m_effectAmount) * pBStats->m_saveVSFortitudeBase / 100;
        if (pBStats->m_saveVSFortitudeBase > 40) {
            pBStats->m_saveVSFortitudeBase = 40;
        }
        if (pBStats->m_saveVSFortitudeBase < -40) {
            pBStats->m_saveVSFortitudeBase = -40;
        }

        pBStats->m_saveVSReflexBase = static_cast<SHORT>(m_effectAmount) * pBStats->m_saveVSReflexBase / 100;
        if (pBStats->m_saveVSReflexBase > 40) {
            pBStats->m_saveVSReflexBase = 40;
        }
        if (pBStats->m_saveVSReflexBase < -40) {
            pBStats->m_saveVSReflexBase = -40;
        }

        pBStats->m_saveVSWillBase = static_cast<SHORT>(m_effectAmount) * pBStats->m_saveVSWillBase / 100;
        if (pBStats->m_saveVSWillBase > 40) {
            pBStats->m_saveVSWillBase = 40;
        }
        if (pBStats->m_saveVSWillBase < -40) {
            pBStats->m_saveVSWillBase = -40;
        }
        m_forceRepass = TRUE;
        m_done = TRUE;
    } else {
        CCreatureFileHeader* pBStats = pSprite->GetBaseStats();
        CDerivedStats* pDStats = pSprite->GetDerivedStats();
        pDStats->m_nSaveVSFortitude = static_cast<SHORT>(m_effectAmount) * pBStats->m_saveVSFortitudeBase / 100;
        if (pDStats->m_nSaveVSFortitude > 40) {
            pDStats->m_nSaveVSFortitude = 40;
        }
        if (pDStats->m_nSaveVSFortitude < -40) {
            pDStats->m_nSaveVSFortitude = -40;
        }

        pDStats->m_nSaveVSReflex = static_cast<SHORT>(m_effectAmount) * pBStats->m_saveVSReflexBase / 100;
        if (pDStats->m_nSaveVSReflex > 40) {
            pDStats->m_nSaveVSReflex = 40;
        }
        if (pDStats->m_nSaveVSReflex < -40) {
            pDStats->m_nSaveVSReflex = -40;
        }

        pDStats->m_nSaveVSWill = static_cast<SHORT>(m_effectAmount) * pBStats->m_saveVSWillBase / 100;
        if (pDStats->m_nSaveVSWill > 40) {
            pDStats->m_nSaveVSWill = 40;
        }
        if (pDStats->m_nSaveVSWill < -40) {
            pDStats->m_nSaveVSWill = -40;
        }
        m_done = FALSE;
    }
}

// -----------------------------------------------------------------------------

// 0x49E110
IcewindCGameEffectSlowPoison::IcewindCGameEffectSlowPoison(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49E150
CGameEffect* IcewindCGameEffectSlowPoison::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectSlowPoison* copy = new IcewindCGameEffectSlowPoison(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x55F180
BOOL IcewindCGameEffectSlowPoison::ApplyEffect(CGameSprite* pSprite)
{
    POSITION pos = pSprite->GetTimedEffectList()->GetHeadPosition();
    while (pos != NULL) {
        CGameEffect* pEffect = pSprite->GetTimedEffectList()->GetNext(pos);
        if (pEffect->m_effectID == CGAMEEFFECT_POISON) {
            switch (m_dwFlags) {
            case 2:
                pEffect->m_dwFlags = 4;
                pEffect->m_duration = 8 * pEffect->m_duration
                    - 7 * g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime;
                break;
            case 3:
                pEffect->m_effectAmount *= 7;
                pEffect->m_duration = 8 * pEffect->m_duration
                    - 7 * g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime;
                break;
            case 4:
                m_dwFlags = 5;
                pEffect->m_duration = 8 * pEffect->m_duration
                    - 7 * g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime;
                break;
            }
        }
    }
    m_forceRepass = TRUE;
    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49E200
IcewindCGameEffectVampiricTouch::IcewindCGameEffectVampiricTouch(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, TRUE)
{
}

// 0x49E240
CGameEffect* IcewindCGameEffectVampiricTouch::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectVampiricTouch* copy = new IcewindCGameEffectVampiricTouch(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x55F260
BOOL IcewindCGameEffectVampiricTouch::ApplyEffect(CGameSprite* pSprite)
{
    LONG nAmount = m_effectAmount;
    for (DWORD nDie = 0; nDie < m_numDice; nDie++) {
        nAmount += (m_diceSize != 0 ? rand() % static_cast<INT>(m_diceSize) : 0) + 1;
    }

    if (pSprite->GetBaseStats()->m_hitPoints <= nAmount) {
        nAmount = pSprite->GetBaseStats()->m_hitPoints;
    }

    CGameSprite* pSource;
    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(m_sourceID,
        CGameObjectArray::THREAD_ASYNCH,
        reinterpret_cast<CGameObject**>(&pSource),
        INFINITE);
    if (rc == CGameObjectArray::SUCCESS) {
        CGameEffect* pEffect;
        CMessage* message;
        switch (m_dwFlags) {
        case 0:
            pEffect = IcewindMisc::sub_585380(pSprite, 0, 0, nAmount, static_cast<BYTE>(m_spellLevel), 0);
            pEffect->m_casterLevel = m_casterLevel;
            pSprite->AddEffect(pEffect, CGameAIBase::EFFECT_LIST_TIMED, FALSE, TRUE);
            IcewindMisc::sub_584880(pSprite, 4392, 0);

            message = new CMessageAddEffect(IcewindMisc::CreateEffectHitPoints(pSource, nAmount, 350, static_cast<BYTE>(m_spellLevel), 1),
                pSprite->GetId(),
                m_sourceID);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

            IcewindMisc::sub_584880(pSource, 4391, nAmount);
            break;
        case 1:
            pEffect = IcewindMisc::CreateEffectHitPoints(pSprite, nAmount, 350, static_cast<BYTE>(m_spellLevel), 1);
            pEffect->m_casterLevel = m_casterLevel;
            pSprite->AddEffect(pEffect, CGameAIBase::EFFECT_LIST_TIMED, FALSE, TRUE);
            IcewindMisc::sub_584880(pSprite, 4391, nAmount);

            message = new CMessageAddEffect(IcewindMisc::sub_585380(pSource, 0, 0, nAmount, static_cast<BYTE>(m_spellLevel), 0),
                pSprite->GetId(),
                m_sourceID);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

            IcewindMisc::sub_584880(pSource, 4392, 0);
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\IcewindCGameEffects.cpp
            // __LINE__: 1580
            UTIL_ASSERT(FALSE);
        }

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(m_sourceID,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49E2F0
IcewindCGameEffectColorGlowDissipate::IcewindCGameEffectColorGlowDissipate(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49E330
CGameEffect* IcewindCGameEffectColorGlowDissipate::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectColorGlowDissipate* copy = new IcewindCGameEffectColorGlowDissipate(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x55F500
BOOL IcewindCGameEffectColorGlowDissipate::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->m_hasColorEffects = TRUE;

    CPersistantEffectColorEffect* pEffect = new CPersistantEffectColorEffect();
    pEffect->m_effectFinalRed = GetRValue(m_effectAmount >> 8);
    pEffect->m_effectFinalGreen = GetGValue(m_effectAmount >> 8);
    pEffect->m_effectFinalBlue = GetBValue(m_effectAmount >> 8);
    pEffect->m_effectDegreeRed = pEffect->m_effectFinalRed;
    pEffect->m_effectDegreeGreen = pEffect->m_effectFinalGreen;
    pEffect->m_effectDegreeBlue = pEffect->m_effectFinalBlue;
    pEffect->m_undo = TRUE;
    pEffect->m_colorEffect = 3;
    pEffect->m_period = (m_dwFlags >> 16) & 0xFFFF;
    pSprite->m_persistantEffects.AddTail(pEffect);

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x55F5E0
IcewindCGameEffectSummon::IcewindCGameEffectSummon(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
    field_18C = 0;
    field_190 = 0;
}

// 0x55F620
CGameEffect* IcewindCGameEffectSummon::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectSummon* copy = new IcewindCGameEffectSummon(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x49DB60
void IcewindCGameEffectSummon::sub_49DB60(int a1)
{
    field_190 = a1;
}

// 0x55F930
void IcewindCGameEffectSummon::PlayGroupVFX(int vfxId)
{
    if (vfxId == 0) {
        return;
    }

    CProjectile* pProjectile = CProjectile::DecodeProjectile(static_cast<WORD>(vfxId + 1), NULL, 0);

    CMessageFireProjectile* pMessage = new CMessageFireProjectile(pProjectile->m_projectileType,
        CGameObjectArray::INVALID_INDEX,
        m_target,
        0,
        field_18C->m_id,
        field_18C->m_id,
        0);
    g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);

    pProjectile->Fire(field_18C->GetArea(),
        field_18C->m_id,
        field_18C->m_id,
        m_target,
        0,
        0);
}

// 0x55FAD0
CGameSprite* IcewindCGameEffectSummon::SpawnFromResRef(const CString& resRef, const CPoint* pTarget)
{
    // The target point converts to search-grid coordinates first; the open
    // square search runs on the grid, the answer converts back afterwards.
    POINT spawnGrid;
    spawnGrid.y = pTarget->y / CPathSearch::GRID_SQUARE_SIZEY;
    spawnGrid.x = pTarget->x / CPathSearch::GRID_SQUARE_SIZEX;

    CGameArea* pArea = field_18C->GetArea();
    if (pArea == NULL) {
        return NULL;
    }

    // __FILE__: .\Include\ObjAnimation.h
    // __LINE__: 2093
    UTIL_ASSERT(field_18C->m_animation.m_animation != NULL);

    // The exe's 0x548B80 takes (POINT* out, int gridX, int gridY,
    // const BYTE* terrainTable, BYTE personalSpace, SHORT nDirection) and
    // writes the answer through the out slot (ret 0x18); our tree spells it
    // as the CPoint-returning CSearchBitmap::GetNearestOpenSquare.
    CPoint ptSpawn = pArea->m_search.GetNearestOpenSquare(CPoint(spawnGrid.x, spawnGrid.y),
        field_18C->GetTerrainTable(),
        field_18C->m_animation.m_animation->GetPersonalSpace(),
        -1);

    CCreatureFile creature;
    creature.SetResRef(CResRef(resRef), TRUE, TRUE);

    CGameSprite* pSpawned = new CGameSprite(creature.GetData(),
        creature.GetDataSize(),
        0,
        IcewindMisc::sub_585310(field_18C) ? 2 : 0, // GOOD 2 / EVIL 0
        static_cast<DWORD>(-1),
        0,
        0,
        0x7fffffff,
        CPoint(-1, -1),
        0);
    pSpawned->SetResRef(CResRef(resRef));
    creature.ReleaseData();

    // The exe's plain-new codegen (new 0x9D16 + je) lands here with a null
    // sprite when the allocation fails.
    if (pSpawned == NULL) {
        return NULL;
    }

    pSpawned->m_baseStats.m_generalState |= STATE_SUMMONED_CREATURE; // sprite+0x5BC

    if (field_190 != 0) {
        CAIObjectType type(pSpawned->GetAIType());
        BOOL ally = FALSE;

        if (field_190 == 1) {
            ally = (IcewindMisc::sub_585310(field_18C) == 1);
        } else if (field_190 == 2) {
            ally = (IcewindMisc::sub_585310(field_18C) != 1);
        }

        if (ally) {
            g_pBaldurChitin->GetObjectGame()->AddCharacterToAllies(pSpawned->m_id);
            type.SetEnemyAlly(CAIOBJECTTYPE_EA_ALLY);
        } else {
            type.SetEnemyAlly(CAIOBJECTTYPE_EA_ENEMY);
        }

        pSpawned->SetAIType(type, FALSE, FALSE);
        pSpawned->m_liveTypeAI.Set(type); // sprite+0x3DFA
    }

    // The deny is taken but neither the result nor the out object is read:
    // the code already holds pSpawned.
    CGameObject* pObject;
    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(pSpawned->m_id,
        CGameObjectArray::THREAD_ASYNCH,
        &pObject,
        INFINITE);

    // __FILE__: .\Include\ObjAnimation.h
    // __LINE__: 2086
    UTIL_ASSERT(pSpawned->m_animation.m_animation != NULL);

    pSpawned->AddToArea(pArea,
        CPoint(ptSpawn.x * CPathSearch::GRID_SQUARE_SIZEX,
            ptSpawn.y * CPathSearch::GRID_SQUARE_SIZEY),
        0,
        pSpawned->m_animation.m_animation->GetListType());
    pSpawned->SetFacing(field_18C->GetDirection(field_18C->GetPos()));
    pSpawned->m_posStart = pSpawned->m_pos;

    Icewind586B70::Instance()->sub_586D60(field_18C, pSpawned);

    CMessage* pMessage = new CMessageAddSummonLink(pSpawned->m_id,
        pSpawned->m_id,
        pSpawned->m_id,
        field_18C->m_id);
    g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(pSpawned->m_id,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    return pSpawned;
}

// 0x55FA10
void IcewindCGameEffectSummon::AddSummonVisualHit(CGameSprite* pSpawned, int mode)
{
    if (mode == 0 || pSpawned == NULL) {
        return;
    }

    CGameObject* pSource;
    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_sourceID,
        CGameObjectArray::THREAD_ASYNCH,
        &pSource,
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        return;
    }

    CGameEffect* pEffect = IcewindMisc::CreateEffectVisualSpellHit(pSource, mode, 0, 0);
    pEffect->m_casterLevel = m_casterLevel;
    pSpawned->AddEffect(pEffect, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceID,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);
}

// -----------------------------------------------------------------------------

// 0x49E3E0
IcewindCGameEffectVisualSpellHit::IcewindCGameEffectVisualSpellHit(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49E420
CGameEffect* IcewindCGameEffectVisualSpellHit::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectVisualSpellHit* copy = new IcewindCGameEffectVisualSpellHit(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x5601E0
BOOL IcewindCGameEffectVisualSpellHit::ApplyEffect(CGameSprite* pSprite)
{
    if (pSprite->GetArea() == NULL) {
        m_done = TRUE;
        return FALSE;
    }

    BOOLEAN bPriest = FALSE;
    const BYTE* sourceRes = m_sourceRes.GetResRef();
    if (sourceRes[2] == 'P' || sourceRes[2] == 'p') {
        bPriest = TRUE;
    }

    CProjectile* pProjectile = CProjectile::DecodeSpellHitProjectile(m_dwFlags, pSprite, bPriest);

    CMessage* message = new CMessageFireProjectile(static_cast<WORD>(m_dwFlags + 0x1000),
        pSprite->GetId(),
        pSprite->GetPos(),
        0,
        pSprite->GetId(),
        pSprite->GetId(),
        0);
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

    pProjectile->Fire(pSprite->GetArea(),
        pSprite->GetId(),
        pSprite->GetId(),
        pSprite->GetPos(),
        0,
        0);

    m_done = TRUE;

    return FALSE;
}

// -----------------------------------------------------------------------------

// 0x49E4D0
IcewindCGameEffectRemoveEffectOfResource::IcewindCGameEffectRemoveEffectOfResource(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49E510
CGameEffect* IcewindCGameEffectRemoveEffectOfResource::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectRemoveEffectOfResource* copy = new IcewindCGameEffectRemoveEffectOfResource(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x563A20
BOOL IcewindCGameEffectRemoveEffectOfResource::ApplyEffect(CGameSprite* pSprite)
{
    // NOTE: The resrefs are compared as lowercase std::strings (tolower over
    // the string's iterators, FUN_0056BDD0 the iterator).
    std::string sRes(m_res.GetResRefStr());
    std::transform(sRes.begin(), sRes.end(), sRes.begin(), ::tolower);

    for (int nList = 0; nList < 2; nList++) {
        CGameEffectList* pList;
        switch (nList) {
        case 0:
            if (m_dwFlags != 0 && m_dwFlags != 1) {
                continue;
            }
            pList = &(pSprite->m_equipedEffectList);
            break;
        case 1:
            if (m_dwFlags != 0 && m_dwFlags != 2) {
                continue;
            }
            pList = &(pSprite->m_timedEffectList);
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\IcewindCGameEffects.cpp
            // __LINE__: 3035
            UTIL_ASSERT(FALSE);
        }

        POSITION pos = pList->GetHeadPosition();
        while (pos != NULL) {
            POSITION posCurrent = pos;
            CGameEffect* pEffect = pList->GetNext(pos);

            std::string sSourceRes(pEffect->m_sourceRes.GetResRefStr());
            std::transform(sSourceRes.begin(), sSourceRes.end(), sSourceRes.begin(), ::tolower);

            if (sRes.compare(sSourceRes) == 0) {
                pList->RemoveAt(posCurrent);
                if (pEffect != NULL) {
                    delete pEffect;
                }
            }
        }
    }

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49E5C0
IcewindCGameEffectSalamanderAura::IcewindCGameEffectSalamanderAura(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, TRUE)
{
}

// 0x49E600
CGameEffect* IcewindCGameEffectSalamanderAura::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectSalamanderAura* copy = new IcewindCGameEffectSalamanderAura(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x563D30
BOOL IcewindCGameEffectSalamanderAura::ApplyEffect(CGameSprite* pSprite)
{
    if (IcewindMisc::IsDead(pSprite) == TRUE) {
        m_done = TRUE;
        return TRUE;
    }

    CGameEffect* copy = Copy();
    copy->m_duration = 7;
    // NOTE: The duration type is the WORD constant at 0x8491B0 (4).
    copy->m_durationType = 4;
    pSprite->AddEffect(copy, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);

    CProjectileArea* pProjectile = new CProjectileArea(100);

    CGameEffect* pEffect;
    switch (m_dwFlags) {
    case 0:
        pEffect = IcewindMisc::CreateEffectImmunitySpell(pSprite, m_sourceRes.GetResRef(), 3, 10, 0, 2);
        pEffect->m_sourceRes = m_sourceRes;
        pProjectile->AddEffect(pEffect);

        pEffect = IcewindMisc::sub_5853B0(pSprite, m_numDice, m_diceSize, m_effectAmount, static_cast<BYTE>(m_spellLevel), 0);
        pEffect->m_sourceRes = m_sourceRes;
        pProjectile->AddEffect(pEffect);
        break;
    case 1:
        pEffect = IcewindMisc::CreateEffectImmunitySpell(pSprite, m_sourceRes.GetResRef(), 25, 10, 0, 2);
        pEffect->m_sourceRes = m_sourceRes;
        pEffect->m_casterLevel = m_casterLevel;
        pProjectile->AddEffect(pEffect);

        pEffect = IcewindMisc::sub_5853E0(pSprite, m_numDice, m_diceSize, m_effectAmount, static_cast<BYTE>(m_spellLevel), 0);
        pEffect->m_sourceRes = m_sourceRes;
        pEffect->m_casterLevel = m_casterLevel;
        pProjectile->AddEffect(pEffect);
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\IcewindCGameEffects.cpp
        // __LINE__: 3123
        UTIL_ASSERT(FALSE);
        break;
    }

    pProjectile->Fire(pSprite->GetArea(),
        pSprite->GetId(),
        pSprite->GetId(),
        pSprite->GetPos(),
        0,
        0);

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49E6B0
IcewindCGameEffectUmberHulkGaze::IcewindCGameEffectUmberHulkGaze(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, TRUE)
{
}

// 0x49E6F0
CGameEffect* IcewindCGameEffectUmberHulkGaze::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectUmberHulkGaze* copy = new IcewindCGameEffectUmberHulkGaze(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x563F70
BOOL IcewindCGameEffectUmberHulkGaze::ApplyEffect(CGameSprite* pSprite)
{
    if (IcewindMisc::IsDead(pSprite) == TRUE) {
        m_done = TRUE;
        return TRUE;
    }

    CGameEffect* copy = Copy();
    copy->m_duration = 7;
    // NOTE: The duration type is the WORD constant at 0x8491B0 (4).
    copy->m_durationType = 4;
    pSprite->AddEffect(copy, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);

    CProjectileArea* pProjectile = new CProjectileArea(275);
    pProjectile->TargetEnemy(pSprite);

    CGameEffect* pEffect;

    pEffect = IcewindMisc::CreateEffectImmunitySpell(pSprite, reinterpret_cast<const BYTE*>(RESREF_UMBER_HULK_GAZE), 17, 10, 0, 2);
    pEffect->m_sourceRes = CResRef(RESREF_UMBER_HULK_GAZE);
    pProjectile->AddEffect(pEffect);

    pEffect = IcewindMisc::CreateEffectImmunitySpell(pSprite, reinterpret_cast<const BYTE*>(RESREF_UMBER_HULK_GAZE), 27, 10, 0, 2);
    pEffect->m_sourceRes = CResRef(RESREF_UMBER_HULK_GAZE);
    pProjectile->AddEffect(pEffect);

    pEffect = IcewindMisc::CreateEffectImmunitySpell(pSprite, reinterpret_cast<const BYTE*>(RESREF_UMBER_HULK_GAZE), 29, 10, 0, 2);
    pEffect->m_sourceRes = CResRef(RESREF_UMBER_HULK_GAZE);
    pProjectile->AddEffect(pEffect);

    pEffect = IcewindMisc::CreateEffectImmunitySpell(pSprite, reinterpret_cast<const BYTE*>(RESREF_UMBER_HULK_GAZE), 23, 10, 0, 2);
    pEffect->m_sourceRes = CResRef(RESREF_UMBER_HULK_GAZE);
    pProjectile->AddEffect(pEffect);

    pEffect = IcewindMisc::CreateEffectConfusion(pSprite, m_effectAmount, static_cast<BYTE>(m_spellLevel), 16);
    pEffect->m_sourceRes = CResRef(RESREF_UMBER_HULK_GAZE);
    pProjectile->AddEffect(pEffect);

    pEffect = IcewindMisc::CreateEffectImmunitySpell(pSprite, reinterpret_cast<const BYTE*>(RESREF_UMBER_HULK_GAZE), 0, m_effectAmount, 0, 2);
    pEffect->m_sourceRes = CResRef(RESREF_UMBER_HULK_GAZE);
    pProjectile->AddEffect(pEffect);

    pProjectile->Fire(pSprite->GetArea(),
        pSprite->GetId(),
        pSprite->GetId(),
        pSprite->GetPos(),
        0,
        0);

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49E7A0
IcewindCGameEffectImmunityToResource::IcewindCGameEffectImmunityToResource(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49E7E0
CGameEffect* IcewindCGameEffectImmunityToResource::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectImmunityToResource* copy = new IcewindCGameEffectImmunityToResource(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x564200
BOOL IcewindCGameEffectImmunityToResource::ApplyEffect(CGameSprite* pSprite)
{
    if (m_dwFlags != 0) {
        if (m_dwFlags == 1) {
            CVariable* pVariable = g_pBaldurChitin->GetObjectGame()->GetVariables()->FindKey(m_res.GetResRefStr());
            if (pVariable != NULL) {
                if (pVariable->m_intValue == 0) {
                    m_done = TRUE;
                    return TRUE;
                }

                pVariable->m_intValue = 0;
            }
        } else {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\IcewindCGameEffects.cpp
            // __LINE__: 3327
            UTIL_ASSERT(FALSE);
        }
    }

    std::string sRes(m_res.GetResRefStr());
    if (sRes.length() == 0) {
        m_done = TRUE;
        return TRUE;
    }

    pSprite->GetDerivedStats()->m_resourceImmunities.insert(sRes);

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49E890
IcewindCGameEffectAnimationRemove::IcewindCGameEffectAnimationRemove(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49E8D0
CGameEffect* IcewindCGameEffectAnimationRemove::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectAnimationRemove* copy = new IcewindCGameEffectAnimationRemove(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x564460
BOOL IcewindCGameEffectAnimationRemove::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->sub_75F3D0(1);

    if (g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL) {
        CMessage* message = new CMessage90(pSprite->GetId(), pSprite->GetId(), 1);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49E980
IcewindCGameEffectImmunityToEffectAndResource::IcewindCGameEffectImmunityToEffectAndResource(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49E9C0
CGameEffect* IcewindCGameEffectImmunityToEffectAndResource::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectImmunityToEffectAndResource* copy = new IcewindCGameEffectImmunityToEffectAndResource(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x5644D0
BOOL IcewindCGameEffectImmunityToEffectAndResource::ApplyEffect(CGameSprite* pSprite)
{
    // NOTE: Inlined `std::set::insert` (0x44AB10 is the tree's node
    // insertion). The original's set compares unsigned.
    pSprite->GetDerivedStats()->m_naturalImmunities.insert(m_dwFlags);

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49EA70
IcewindCGameEffectDominate::IcewindCGameEffectDominate(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49EAB0
CGameEffect* IcewindCGameEffectDominate::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectDominate* copy = new IcewindCGameEffectDominate(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x5645A0
BOOL IcewindCGameEffectDominate::ApplyEffect(CGameSprite* pSprite)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    CAIObjectType typeAI;
    typeAI.Set(pSprite->GetAIType());
    CAIObjectType liveTypeAI;
    liveTypeAI.Set(pSprite->m_liveTypeAI);
    CAIObjectType startTypeAI;
    startTypeAI.Set(pSprite->m_startTypeAI);
    CAIObjectType enemyTypeAI(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);

    // m_effectAmount at +0x18 gates the target's general type: a non-zero
    // amount only dominates that general type.
    if (m_effectAmount != 0
        && static_cast<DWORD>(pSprite->GetAIType().m_nGeneral) != static_cast<DWORD>(m_effectAmount)) {
        m_done = TRUE;
        return TRUE;
    }

    pSprite->sub_761650();

    // The allegiance ApplyEffect keeps in m_numDice after the first pass,
    // and the bare type the original uses for GetEnemyOf and the action.
    CAIObjectType type(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
    BYTE nEnemyAlly = 0;
    BOOL bPartyCharm = FALSE;

    if (m_numDice != static_cast<DWORD>(-1)) {
        nEnemyAlly = static_cast<BYTE>(m_numDice);
        bPartyCharm = m_diceSize;

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
                CGameSprite* pSourceSprite = static_cast<CGameSprite*>(pSource);

                if (pSourceSprite->GetAIType().m_nEnemyAlly > CAIObjectType::EA_GOODCUTOFF) {
                    nEnemyAlly = CAIObjectType::EA_ENEMY;
                }

                enemyTypeAI.Set(pSourceSprite->GetAIType().GetEnemyOf());

                if (pGame->GetCharacterPortraitNum(pSprite->GetId()) != -1
                    && pGame->GetCharacterPortraitNum(pSourceSprite->GetId()) != -1) {
                    bPartyCharm = TRUE;
                }

                if (!Icewind586B70::Instance()->sub_586CF0(pSourceSprite, pSprite)) {
                    pGame->GetObjectArray()->ReleaseShare(m_sourceID,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);

                    m_done = TRUE;
                    return TRUE;
                }

                Icewind586B70::Instance()->sub_586F20(pSourceSprite, pSprite);

                // 0x56485D-0x56489D: the original's inline construction,
                // vtable 0x84D37C, posts the target/source pair separately
                // from the local registry update above. CMessage95 is the
                // linked message PR's address/subtype-derived class; do not
                // substitute the one-id CMessageRemoveSummonLink.
                CMessage* pMessage = new CMessage95(pSprite->GetId(),
                    pSourceSprite->GetId(),
                    pSprite->GetId(),
                    pSprite->GetId());
                g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);

                type.m_nEnemyAlly = nEnemyAlly;

                // The original builds one function-static ATTACKREEVALUATE
                // action (0x8E2CF0) and overwrites its actee, specifics,
                // strings, destination and flags on every first pass.
                static CAIAction action;
                action.m_actionID = CAIAction::ATTACKREEVALUATE;
                action.m_acteeID.Set(type);
                action.m_specificID = 45;
                action.m_specificID2 = -1;
                action.m_specificID3 = 0;
                action.m_string1 = "";
                action.m_string2 = "";
                action.m_dest = CPoint(-1, -1);
                action.m_internalFlags = 0;

                pSprite->ClearActions(FALSE);
                pSprite->DropPath();
                pSprite->m_interrupt = TRUE;
                pSprite->AddAction(action);

                pGame->GetObjectArray()->ReleaseShare(m_sourceID,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            }
        }
    }

    // The jump table at 0x564D1C on m_dwFlags (exe +0x1c). Every arm then
    // tests m_durationType (exe +0x20) against the WORD constant 0x8491AA
    // (1): permanent effects rewrite the start type. Cases 3 and 4 share a
    // body; flags above 4 skip the switch and the AI-type update.
    BOOL updateAIType = TRUE;
    switch (m_dwFlags) {
    case 0:
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
        } else {
            m_done = FALSE;
        }
        break;
    case 1:
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
        break;
    case 2:
        if (nEnemyAlly == 0) {
            nEnemyAlly = CAIObjectType::EA_CONTROLLED;
        }

        if (m_durationType == 1) {
            startTypeAI.m_nEnemyAlly = nEnemyAlly;
            pSprite->m_startTypeAI.Set(startTypeAI);
            m_done = TRUE;
        } else {
            m_done = FALSE;
        }
        break;
    case 3:
    case 4:
        if (nEnemyAlly == 0) {
            nEnemyAlly = CAIObjectType::EA_CONTROLLED;
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
        break;
    default:
        updateAIType = FALSE;
        break;
    }

    if (updateAIType) {
        typeAI.m_nEnemyAlly = nEnemyAlly;
        pSprite->SetAIType(typeAI, FALSE, FALSE);

        liveTypeAI.m_nEnemyAlly = nEnemyAlly;
        pSprite->m_liveTypeAI.Set(liveTypeAI);
    }

    pSprite->m_derivedStats.m_generalState |= STATE_CHARMED;

    if (m_durationType == 1) {
        pSprite->m_baseStats.m_generalState |= STATE_CHARMED;
    }

    m_numDice = nEnemyAlly;
    m_diceSize = bPartyCharm;

    return TRUE;
}

// 0x4A6220
void IcewindCGameEffectDominate::OnAddSpecific(CGameSprite* pSprite)
{
    // NOTE: Identical code folding shares CGameEffectCharm's (vtable
    // 0x84B7F0 +0x10).
    m_numDice = -1;
    m_diceSize = -1;
}

// -----------------------------------------------------------------------------

// 0x49EB60
IcewindCGameEffectStaticCharge::IcewindCGameEffectStaticCharge(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, TRUE)
{
}

// 0x49EBA0
CGameEffect* IcewindCGameEffectStaticCharge::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectStaticCharge* copy = new IcewindCGameEffectStaticCharge(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x564D40
BOOL IcewindCGameEffectStaticCharge::ApplyEffect(CGameSprite* pSprite)
{
    if (IcewindMisc::IsDead(pSprite) == TRUE) {
        m_done = TRUE;
    } else {
        if (m_effectAmount > 0) {
            LONG nDelay = 70;
            for (LONG nRound = 1; nRound < m_effectAmount; nRound++) {
                CGameEffect* copy = Copy();
                copy->m_duration = nDelay;
                copy->m_effectAmount = 0;
                // NOTE: The duration type is the WORD constant at 0x8491B0
                // (4).
                copy->m_durationType = 4;
                copy->m_casterLevel = m_casterLevel;
                pSprite->AddEffect(copy, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);
                nDelay += 70;
            }
        }

        CGameObject* pSource;
        BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_sourceID,
            CGameObjectArray::THREAD_ASYNCH,
            &pSource,
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            CProjectileArea850078* pProjectile = new CProjectileArea850078();
            pProjectile->TargetEnemy(pSprite);

            std::list<CGameEffect*> effects = IcewindMisc::GetSpellResrefEffects(pSprite,
                m_res.GetResRef(),
                static_cast<BYTE>(m_casterLevel));
            for (std::list<CGameEffect*>::iterator it = effects.begin(); it != effects.end(); it++) {
                (*it)->m_sourceRes = m_sourceRes;
                pProjectile->AddEffect(*it);
            }

            pProjectile->Fire(pSprite->GetArea(),
                pSprite->GetId(),
                pSprite->GetId(),
                pSprite->GetPos(),
                0,
                0);

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceID,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            m_done = TRUE;
        } else {
            m_done = TRUE;
        }
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49EC50
IcewindCGameEffectCallLightning::IcewindCGameEffectCallLightning(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, TRUE)
{
}

// 0x49EC90
CGameEffect* IcewindCGameEffectCallLightning::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectCallLightning* copy = new IcewindCGameEffectCallLightning(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x564F80
BOOL IcewindCGameEffectCallLightning::ApplyEffect(CGameSprite* pSprite)
{
    if (m_effectAmount > 0) {
        LONG nDelay = 70;
        for (LONG nRound = 1; nRound < m_effectAmount; nRound++) {
            CGameEffect* copy = Copy();
            copy->m_duration = nDelay;
            copy->m_effectAmount = 0;
            // NOTE: The duration type is the WORD constant at 0x8491B0 (4).
            copy->m_durationType = 4;
            copy->m_casterLevel = m_casterLevel;
            pSprite->AddEffect(copy, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);
            nDelay += 70;
        }
    }

    CGameObject* pSource;
    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_sourceID,
        CGameObjectArray::THREAD_ASYNCH,
        &pSource,
        INFINITE);
    if (rc == CGameObjectArray::SUCCESS) {
        CProjectileArea850114* pProjectile = new CProjectileArea850114();
        pProjectile->TargetEnemy(pSprite);

        std::list<CGameEffect*> effects = IcewindMisc::GetSpellResrefEffects(pSprite,
            m_res.GetResRef(),
            static_cast<BYTE>(m_casterLevel));
        for (std::list<CGameEffect*>::iterator it = effects.begin(); it != effects.end(); it++) {
            (*it)->m_sourceRes = m_sourceRes;
            pProjectile->AddEffect(*it);
        }

        pProjectile->Fire(pSprite->GetArea(),
            pSprite->GetId(),
            pSprite->GetId(),
            pSprite->GetPos(),
            0,
            0);

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceID,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        m_done = TRUE;
    } else {
        m_done = TRUE;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49ED40
IcewindCGameEffectCloakOfFear::IcewindCGameEffectCloakOfFear(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, TRUE)
{
}

// 0x49ED80
CGameEffect* IcewindCGameEffectCloakOfFear::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectCloakOfFear* copy = new IcewindCGameEffectCloakOfFear(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x5651A0
BOOL IcewindCGameEffectCloakOfFear::ApplyEffect(CGameSprite* pSprite)
{
    if (IcewindMisc::IsDead(pSprite) == TRUE) {
        m_done = TRUE;
    } else {
        if (m_effectAmount > 0) {
            LONG nDelay = 3;
            for (LONG nRound = 1; nRound < m_effectAmount; nRound++) {
                CGameEffect* copy = Copy();
                copy->m_duration = nDelay;
                copy->m_effectAmount = 0;
                // NOTE: The duration type is the WORD constant at 0x8491B0
                // (4).
                copy->m_durationType = 4;
                pSprite->AddEffect(copy, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);
                nDelay += 3;
            }
        }

        CGameObject* pSource;
        BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_sourceID,
            CGameObjectArray::THREAD_ASYNCH,
            &pSource,
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            // NOTE: The radius argument is the DWORD at 0x84F1BC (0x100).
            CProjectileArea8504BC* pProjectile = new CProjectileArea8504BC(0x100);
            pProjectile->m_nRadius = 75;

            // NOTE: Level 1, not the caster level.
            std::list<CGameEffect*> effects = IcewindMisc::GetSpellResrefEffects(pSprite,
                m_res.GetResRef(),
                1);
            for (std::list<CGameEffect*>::iterator it = effects.begin(); it != effects.end(); it++) {
                (*it)->m_sourceRes = m_sourceRes;
                pProjectile->AddEffect(*it);
            }

            pProjectile->Fire(pSprite->GetArea(),
                pSprite->GetId(),
                pSprite->GetId(),
                pSprite->GetPos(),
                0,
                0);

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceID,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            m_done = TRUE;
        } else {
            m_done = TRUE;
        }
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49EE30
IcewindCGameEffectForceMovementRate::IcewindCGameEffectForceMovementRate(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49EE70
CGameEffect* IcewindCGameEffectForceMovementRate::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectForceMovementRate* copy = new IcewindCGameEffectForceMovementRate(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x5653D0
BOOL IcewindCGameEffectForceMovementRate::ApplyEffect(CGameSprite* pSprite)
{
    // NOTE: Uninline.
    if (pSprite->GetAnimation()->GetMoveScaleDefault() != 0) {
        switch (m_dwFlags) {
        case 0:
            // NOTE: Uninline.
            pSprite->GetAnimation()->SetMoveScale(pSprite->GetAnimation()->GetMoveScaleDefault() + static_cast<BYTE>(m_effectAmount));
            break;
        case 1:
            // NOTE: Uninline.
            pSprite->GetAnimation()->SetMoveScale(static_cast<BYTE>(m_effectAmount));
            break;
        case 2:
            pSprite->GetAnimation()->SetMoveScale(pSprite->GetAnimation()->GetMoveScaleDefault() * static_cast<BYTE>(m_effectAmount) / 100);
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\IcewindCGameEffects.cpp
            // __LINE__: 4099
            UTIL_ASSERT(FALSE);
        }
    }

    if (m_secondaryType) {
        pSprite->DropPath();
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49EF20
IcewindCGameEffectRemoveConfusion::IcewindCGameEffectRemoveConfusion(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49EF60
CGameEffect* IcewindCGameEffectRemoveConfusion::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectRemoveConfusion* copy = new IcewindCGameEffectRemoveConfusion(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x565590
BOOL IcewindCGameEffectRemoveConfusion::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetBaseStats()->m_generalState &= ~STATE_CONFUSED;
    pSprite->GetDerivedStats()->m_generalState &= ~STATE_CONFUSED;

    // NOTE: Uninline.
    RemoveAllOfType(pSprite, CGAMEEFFECT_CONFUSION, -1);

    // NOTE: Uninline.
    RemoveAllOfType(pSprite, CGAMEEFFECT_PORTRAITICON, 3);

    m_done = TRUE;
    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49F010
IcewindCGameEffectEyeOfMind::IcewindCGameEffectEyeOfMind(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49F050
CGameEffect* IcewindCGameEffectEyeOfMind::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectEyeOfMind* copy = new IcewindCGameEffectEyeOfMind(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x565640
BOOL IcewindCGameEffectEyeOfMind::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType) {
        pSprite->AddSpecialAbility(CResRef(CString(RESREF_EYE_OF_THE_MIND)), FALSE);
    }
    pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_EYE_OF_THE_MIND] = true;
    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49F100
IcewindCGameEffectEyeOfSword::IcewindCGameEffectEyeOfSword(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49F140
CGameEffect* IcewindCGameEffectEyeOfSword::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectEyeOfSword* copy = new IcewindCGameEffectEyeOfSword(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x5656D0
BOOL IcewindCGameEffectEyeOfSword::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType) {
        pSprite->AddSpecialAbility(CResRef(CString(RESREF_EYE_OF_THE_SWORD)), FALSE);
    }
    pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_EYE_OF_THE_SWORD] = true;
    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49F1F0
IcewindCGameEffectEyeOfMage::IcewindCGameEffectEyeOfMage(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49F230
CGameEffect* IcewindCGameEffectEyeOfMage::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectEyeOfMage* copy = new IcewindCGameEffectEyeOfMage(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x565760
BOOL IcewindCGameEffectEyeOfMage::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType) {
        pSprite->AddSpecialAbility(CResRef(CString(RESREF_EYE_OF_THE_MAGE)), FALSE);
    }
    pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_EYE_OF_THE_MAGE] = true;
    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49F2E0
IcewindCGameEffectEyeOfVenom::IcewindCGameEffectEyeOfVenom(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49F320
CGameEffect* IcewindCGameEffectEyeOfVenom::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectEyeOfVenom* copy = new IcewindCGameEffectEyeOfVenom(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x5657F0
BOOL IcewindCGameEffectEyeOfVenom::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType) {
        pSprite->AddSpecialAbility(CResRef(CString(RESREF_EYE_OF_VENOM)), FALSE);
    }
    pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_EYE_OF_VENOM] = true;
    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49F3D0
IcewindCGameEffectEyeOfSpirit::IcewindCGameEffectEyeOfSpirit(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49F410
CGameEffect* IcewindCGameEffectEyeOfSpirit::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectEyeOfSpirit* copy = new IcewindCGameEffectEyeOfSpirit(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x565880
BOOL IcewindCGameEffectEyeOfSpirit::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType) {
        pSprite->AddSpecialAbility(CResRef(CString(RESREF_EYE_OF_THE_SPIRIT)), FALSE);
    }
    pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_EYE_OF_THE_SPIRIT] = true;
    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49F4C0
IcewindCGameEffectEyeOfFortitude::IcewindCGameEffectEyeOfFortitude(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49F500
CGameEffect* IcewindCGameEffectEyeOfFortitude::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectEyeOfFortitude* copy = new IcewindCGameEffectEyeOfFortitude(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x565910
BOOL IcewindCGameEffectEyeOfFortitude::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType) {
        pSprite->AddSpecialAbility(CResRef(CString(RESREF_EYE_OF_FORTITUDE)), FALSE);
    }
    pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_EYE_OF_FORTITUDE] = true;
    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49F5B0
IcewindCGameEffectEyeOfStone::IcewindCGameEffectEyeOfStone(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49F5F0
CGameEffect* IcewindCGameEffectEyeOfStone::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectEyeOfStone* copy = new IcewindCGameEffectEyeOfStone(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x5659A0
BOOL IcewindCGameEffectEyeOfStone::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType) {
        pSprite->AddSpecialAbility(CResRef(CString(RESREF_EYE_OF_STONE)), FALSE);
    }
    pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_EYE_OF_STONE] = true;
    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49F6A0
IcewindCGameEffectRemoveSevenEyes::IcewindCGameEffectRemoveSevenEyes(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49F6E0
CGameEffect* IcewindCGameEffectRemoveSevenEyes::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectRemoveSevenEyes* copy = new IcewindCGameEffectRemoveSevenEyes(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x565A30
BOOL IcewindCGameEffectRemoveSevenEyes::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->RemoveSpecialAbility(CResRef(CString(RESREF_EYE_OF_THE_MIND)));
    pSprite->RemoveSpecialAbility(CResRef(CString(RESREF_EYE_OF_THE_SWORD)));
    pSprite->RemoveSpecialAbility(CResRef(CString(RESREF_EYE_OF_THE_MAGE)));
    pSprite->RemoveSpecialAbility(CResRef(CString(RESREF_EYE_OF_VENOM)));
    pSprite->RemoveSpecialAbility(CResRef(CString(RESREF_EYE_OF_THE_SPIRIT)));
    pSprite->RemoveSpecialAbility(CResRef(CString(RESREF_EYE_OF_FORTITUDE)));
    pSprite->RemoveSpecialAbility(CResRef(CString(RESREF_EYE_OF_STONE)));

    m_done = TRUE;
    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49F790
IcewindCGameEffectRemoveAllOfType::IcewindCGameEffectRemoveAllOfType(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49F7D0
CGameEffect* IcewindCGameEffectRemoveAllOfType::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectRemoveAllOfType* copy = new IcewindCGameEffectRemoveAllOfType(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x565C20
BOOL IcewindCGameEffectRemoveAllOfType::ApplyEffect(CGameSprite* pSprite)
{
    // NOTE: Uninline.
    RemoveAllOfType(pSprite, static_cast<WORD>(m_dwFlags), -1);

    if (m_dwFlags == CGAMEEFFECT_POISON) {
        // NOTE: Uninline.
        RemoveAllOfType(pSprite, CGAMEEFFECT_PORTRAITICON, 6);
    }

    m_forceRepass = TRUE;
    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49F880
IcewindCGameEffectShroudOfFlame::IcewindCGameEffectShroudOfFlame(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49F8C0
CGameEffect* IcewindCGameEffectShroudOfFlame::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectShroudOfFlame* copy = new IcewindCGameEffectShroudOfFlame(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x565CB0
BOOL IcewindCGameEffectShroudOfFlame::ApplyEffect(CGameSprite* pSprite)
{
    if (IcewindMisc::IsDead(pSprite) == TRUE) {
        m_done = TRUE;
    } else if (pSprite->GetDerivedStats()->m_spellStates.test(SPLSTATE_SHROUD_OF_FLAME) == true) {
        m_done = TRUE;
    } else {
        if (m_secondaryType != 0) {
            m_special = g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime;
            DisplayStringRef(pSprite, 7879);
        }

        pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_SHROUD_OF_FLAME, true);
        pSprite->GetDerivedStats()->m_visualEffects[IWD_VFX_SHROUD_OF_FLAME] = true;

        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 98);

        AddColorEffect(pSprite, 160, 0, 0, 1);

        CPersistantEffect84C4D4* pEffect = new CPersistantEffect84C4D4(100,
            105,
            m_special,
            m_duration);
        pEffect->m_sourceID = m_sourceID;
        strcpy(reinterpret_cast<char*>(pEffect->field_29), m_sourceRes.GetResRefStr());
        pSprite->GetDerivedStats()->m_cRegeneratedPersistantEffectList.AddTail(pEffect);
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49F970
IcewindCGameEffectAnimalRage::IcewindCGameEffectAnimalRage(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49F9B0
CGameEffect* IcewindCGameEffectAnimalRage::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectAnimalRage* copy = new IcewindCGameEffectAnimalRage(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x565E50
BOOL IcewindCGameEffectAnimalRage::ApplyEffect(CGameSprite* pSprite)
{
    if (m_dwFlags == 1) {
        pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_ANIMAL_RAGE, true);
    } else if (IcewindMisc::IsDead(pSprite) == TRUE) {
        m_done = TRUE;
    } else if (!pSprite->GetTimedEffectList()->IsTypeOnList(CGAMEEFFECT_BERSERK)) {
        pSprite->field_562C = 1;
        if (pSprite->GetArea() != NULL && pSprite->GetVertListPos() != NULL) {
            CTypedPtrList<CPtrList, LONG*> targets(10);
            pSprite->GetArea()->GetAllInRange(pSprite->GetVertListPos(),
                pSprite->GetPos(),
                CAIObjectType::ANYONE,
                275,
                pSprite->GetTerrainTable(),
                targets,
                TRUE,
                FALSE);
            pSprite->GetArea()->GetAllInRangeBack(pSprite->GetPos(),
                CAIObjectType::ANYONE,
                275,
                pSprite->GetTerrainTable(),
                targets,
                TRUE,
                FALSE,
                FALSE);

            POSITION pos = targets.GetHeadPosition();
            while (pos != NULL) {
                LONG nTargetId = static_cast<LONG>(reinterpret_cast<INT_PTR>(targets.GetNext(pos)));

                CGameSprite* pTarget;
                BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(nTargetId,
                    CGameObjectArray::THREAD_ASYNCH,
                    reinterpret_cast<CGameObject**>(&pTarget),
                    INFINITE);
                if (rc == CGameObjectArray::SUCCESS) {
                    if (IcewindMisc::sub_5852A0(pSprite, pTarget) == TRUE) {
                        pSprite->EquipMostDamagingMelee();

                        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nTargetId,
                            CGameObjectArray::THREAD_ASYNCH,
                            INFINITE);

                        if (rand() % 100 < 5) {
                            CGameEffect* pBerserk = IcewindMisc::CreateEffectBerserk(pSprite, 21, 0, 0);
                            pSprite->AddEffect(pBerserk, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);
                        }
                        break;
                    }

                    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nTargetId,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);
                }
            }
        }
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49FA60
IcewindCGameEffectTurnUndead::IcewindCGameEffectTurnUndead(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49FAA0
CGameEffect* IcewindCGameEffectTurnUndead::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectTurnUndead* copy = new IcewindCGameEffectTurnUndead(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x576D40
static INT CalculateTurnUndeadFinalSourceRoll(CGameSprite* pSprite)
{
    INT nModifier = g_pBaldurChitin->GetObjectGame()->GetRuleTables().GetAbilityScoreModifier(pSprite->GetDerivedStats()->m_nCHR);
    INT nRoll = nModifier + 1 + CUtil::UtilRandInt(20, pSprite->GetDerivedStats()->m_nLuck);
    SHORT nTurnUndeadLevel = pSprite->GetTurnUndeadLevel();
    if (nRoll - 10 >= 0) {
        return (nRoll - 10) / 3 + nTurnUndeadLevel;
    }
    return (nRoll - 9) / 3 - 1 + nTurnUndeadLevel;
}

// 0x566080
BOOL IcewindCGameEffectTurnUndead::ApplyEffect(CGameSprite* pSprite)
{
    // NOTE: The turner's id travels in m_numDice.
    LONG nTurnerId = m_numDice;

    CGameSprite* pTurner;
    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(nTurnerId,
        CGameObjectArray::THREAD_ASYNCH,
        reinterpret_cast<CGameObject**>(&pTurner),
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        return FALSE;
    }

    if (m_dwFlags == 4) {
        LONG nLevel = pSprite->GetLevel();
        if (nLevel > CalculateTurnUndeadFinalSourceRoll(pTurner)) {
            FeedBackImmuneToResource(pSprite, m_sourceRes);
            m_done = TRUE;
            // NOTE: The original returns here without releasing its share on
            // the turner (original bug, kept). GetDeny only waits on other
            // threads' shares, so this cannot stall a message; the hazards
            // are the turner's share count staying above 0, which asserts in
            // CGameObjectArray::Delete (CGameObjectArray.cpp:533-537) when the
            // turner is deleted, and GetShare asserting once 255 shares pile
            // up.
            return TRUE;
        }

        if (IcewindMisc::IsEvil(pTurner) == TRUE) {
            m_dwFlags = static_cast<BYTE>(m_casterLevel) <= nLevel * 2 ? 1 : 0;
        } else {
            m_dwFlags = (static_cast<BYTE>(m_casterLevel) <= nLevel * 2 ? 1 : 0) + 2;
        }
    }

    if (m_secondaryType != 0) {
        PlaySound(CResRef("ACT_06"), pSprite);
    }

    pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_TURNED, true);

    CGameEffect* pEffect;
    CMessage* message;
    switch (m_dwFlags) {
    case 0:
        if (m_secondaryType != 0) {
            DisplayStringRef(pSprite, 41023);

            ITEM_EFFECT effect;
            CGameEffect::ClearItemEffect(&effect, ICEWIND_CGAMEEFFECT_DOMINATE);
            effect.dwFlags = 4;
            effect.durationType = 0x1000;
            effect.duration = m_duration;
            pEffect = CGameEffect::DecodeEffect(&effect,
                pTurner->GetPos(),
                pTurner->GetId(),
                CPoint(-1, -1));
            message = new CMessageAddEffect(pEffect, pSprite->GetId(), pSprite->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
        break;
    case 1:
        if (m_secondaryType != 0) {
            DisplayStringRef(pSprite, 20827);
        }
        SetSpellState(pSprite, SPLSTATE_179);
        pSprite->GetDerivedStats()->field_C -= 2;
        break;
    case 2:
        // NOTE: The inlined CGameEffectDeath default constructor (vtable
        // 0x8487E8, whose slot 1 is CGameEffectDeath::Copy 0x452E70): opcode
        // 13, flags 8, from the turner. The constructor sets +0x18C to 0 to
        // match the original (the effects-core stream's fix to
        // CGameEffectDeath()); the explicit store below is harmless and
        // covers the branch until that fix merges. Its probability range is
        // 100/0 (CGameEffectBase), so the death applies.
        pEffect = new CGameEffectDeath();
        static_cast<CGameEffectDeath*>(pEffect)->field_18C = 0;
        pEffect->m_effectAmount = 0;
        pEffect->m_dwFlags = 8;
        pEffect->m_sourceID = pTurner->GetId();
        message = new CMessageAddEffect(pEffect, pSprite->GetId(), pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        m_done = TRUE;
        break;
    case 3:
        if (m_secondaryType != 0) {
            DisplayStringRef(pSprite, 20828);
        }
        AddPanicEffect(pSprite);
        break;
    }

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nTurnerId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    return TRUE;
}

// 0x566070
void IcewindCGameEffectTurnUndead::OnRemove(CGameSprite* pSprite)
{
    pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_TURNED] = false;
}

// -----------------------------------------------------------------------------

// 0x49FB50
IcewindCGameEffectVitriolicSphere::IcewindCGameEffectVitriolicSphere(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, TRUE)
{
}

// 0x49FB90
CGameEffect* IcewindCGameEffectVitriolicSphere::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectVitriolicSphere* copy = new IcewindCGameEffectVitriolicSphere(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x5663F0
BOOL IcewindCGameEffectVitriolicSphere::ApplyEffect(CGameSprite* pSprite)
{
    CGameObject* pSource;
    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_sourceID,
        CGameObjectArray::THREAD_ASYNCH,
        &pSource,
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        m_done = TRUE;
        return TRUE;
    }

    if (m_numDice > 1) {
        CGameEffect* copy = Copy();
        copy->m_duration = 7;
        // NOTE: The duration type is the WORD constant at 0x8491B0 (4).
        copy->m_durationType = 4;
        copy->m_numDice = m_numDice - 2;
        copy->m_savingThrow = 8;
        pSprite->AddEffect(copy, CGameAIBase::EFFECT_LIST_TIMED, FALSE, FALSE);
    }

    CGameEffect* pDamage = IcewindMisc::CreateEffectDamage(pSource,
        0x10003,
        m_numDice,
        m_diceSize,
        0,
        static_cast<BYTE>(m_spellLevel),
        8);
    pSprite->AddEffect(pDamage, CGameAIBase::EFFECT_LIST_TIMED, FALSE, TRUE);

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceID,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49FC50
IcewindCGameEffectSuppressHPDisplay::IcewindCGameEffectSuppressHPDisplay(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49FC90
CGameEffect* IcewindCGameEffectSuppressHPDisplay::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectSuppressHPDisplay* copy = new IcewindCGameEffectSuppressHPDisplay(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x566500
BOOL IcewindCGameEffectSuppressHPDisplay::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_SUPPRESS_HP_INFO] = true;

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49FD40
IcewindCGameEffectFloatText::IcewindCGameEffectFloatText(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49FD80
CGameEffect* IcewindCGameEffectFloatText::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectFloatText* copy = new IcewindCGameEffectFloatText(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x566520
BOOL IcewindCGameEffectFloatText::ApplyEffect(CGameSprite* pSprite)
{
    if (m_dwFlags == 0) {
        CMessage* message = new CMessageFloatText(pSprite->GetId(),
            pSprite->GetId(),
            m_effectAmount,
            FALSE);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49FE30
IcewindCGameEffectMaceOfDisruption::IcewindCGameEffectMaceOfDisruption(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49FE70
CGameEffect* IcewindCGameEffectMaceOfDisruption::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectMaceOfDisruption* copy = new IcewindCGameEffectMaceOfDisruption(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x566580
BOOL IcewindCGameEffectMaceOfDisruption::ApplyEffect(CGameSprite* pSprite)
{
    INT nChance = 0;

    if (pSprite->GetAIType().GetRace() == CAIOBJECTTYPE_R_GHOUL
        || pSprite->GetAIType().GetRace() == CAIOBJECTTYPE_R_SKELETON
        || pSprite->GetAIType().GetRace() == CAIOBJECTTYPE_R_UNDEAD) {
        BYTE nLevel = pSprite->GetBaseStats()->m_characterLevel;
        // NOTE: Level 4 has no case: a level 4 undead is never destroyed
        // (original bug, kept).
        if (nLevel < 4) {
            nChance = 100;
        } else if (nLevel == 5) {
            nChance = 95;
        } else if (nLevel == 6) {
            nChance = 80;
        } else if (nLevel == 7) {
            nChance = 65;
        } else if (nLevel == 8 || nLevel == 9) {
            nChance = 50;
        } else if (nLevel == 10) {
            nChance = 35;
        } else if (nLevel >= 11) {
            nChance = 20;
        }
    }

    if (pSprite->GetAIType().GetRace() == CAIOBJECTTYPE_R_TANARI) {
        nChance = 5;
    }

    if (rand() % 100 < nChance) {
        CGameObject* pSource;
        BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_sourceID,
            CGameObjectArray::THREAD_ASYNCH,
            &pSource,
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            CGameEffect* pHit = IcewindMisc::CreateEffectVisualSpellHit(pSource, 84, static_cast<BYTE>(m_spellLevel), 0);
            CMessage* message = new CMessageAddEffect(pHit, pSprite->GetId(), pSprite->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

            ITEM_EFFECT* effect = new ITEM_EFFECT;
            CGameEffect::ClearItemEffect(effect, CGAMEEFFECT_DEATH);
            effect->dwFlags = 8;
            // NOTE: The duration type is the WORD constant at 0x8491AA (1).
            effect->durationType = 1;
            effect->duration = 0;

            CGameEffect* pDeath = CGameEffect::DecodeEffect(effect,
                pSprite->GetPos(),
                pSprite->GetId(),
                CPoint(-1, -1));
            CGameEffect* copy = pDeath->Copy();
            copy->m_sourceID = m_sourceID;
            message = new CMessageAddEffect(copy, pSprite->GetId(), pSprite->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

            if (pDeath != NULL) {
                delete pDeath;
            }

            delete effect;

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceID,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x49FF20
IcewindCGameEffectRangerTracking::IcewindCGameEffectRangerTracking(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x49FF60
CGameEffect* IcewindCGameEffectRangerTracking::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectRangerTracking* copy = new IcewindCGameEffectRangerTracking(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x5667C0
BOOL IcewindCGameEffectRangerTracking::ApplyEffect(CGameSprite* pSprite)
{
    CString sAreaName;
    STR_RES strRes;

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    pGame->m_gameAreas[pGame->m_visibleArea]->m_resRef.CopyToString(sAreaName);

    STRREF strTracks = RuleTables_GetTrackingString5466F0(pGame->GetRuleTables(), pSprite, sAreaName);
    g_pBaldurChitin->m_cTlkTable.Fetch(strTracks, strRes);
    g_pBaldurChitin->m_pEngineWorld->DisplayText(pSprite->GetName(),
        strRes.szText,
        -1,
        FALSE);

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4A0010
IcewindCGameEffectImmunityToBackstab::IcewindCGameEffectImmunityToBackstab(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A0050
CGameEffect* IcewindCGameEffectImmunityToBackstab::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectImmunityToBackstab* copy = new IcewindCGameEffectImmunityToBackstab(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x5668F0
BOOL IcewindCGameEffectImmunityToBackstab::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_SNEAK_ATTACK_IMMUNITY] = true;

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4A0100
IcewindCGameEffectSetSpellState::IcewindCGameEffectSetSpellState(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A0140
CGameEffect* IcewindCGameEffectSetSpellState::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectSetSpellState* copy = new IcewindCGameEffectSetSpellState(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x566910
BOOL IcewindCGameEffectSetSpellState::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetDerivedStats()->m_spellStates.set(m_dwFlags, true);

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4A01F0
IcewindCGameEffectRodOfSmiting::IcewindCGameEffectRodOfSmiting(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A0230
CGameEffect* IcewindCGameEffectRodOfSmiting::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectRodOfSmiting* copy = new IcewindCGameEffectRodOfSmiting(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x566960
BOOL IcewindCGameEffectRodOfSmiting::ApplyEffect(CGameSprite* pSprite)
{
    CGameEffect* pEffect;
    CMessage* message;

    if (IcewindMisc::IsGolem(pSprite) == TRUE) {
        if (rand() % 100 < 5) {
            ITEM_EFFECT* effect = new ITEM_EFFECT;
            CGameEffect::ClearItemEffect(effect, CGAMEEFFECT_DEATH);
            effect->dwFlags = 8;
            // NOTE: The duration type is the WORD constant at 0x8491AA (1).
            effect->durationType = 1;
            effect->duration = 0;

            CGameEffect* pDeath = CGameEffect::DecodeEffect(effect,
                pSprite->GetPos(),
                pSprite->GetId(),
                CPoint(-1, -1));
            pDeath->m_flags |= 0x2;
            CGameEffect* copy = pDeath->Copy();
            copy->m_sourceID = m_sourceID;
            message = new CMessageAddEffect(copy, pSprite->GetId(), pSprite->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

            if (pDeath != NULL) {
                delete pDeath;
            }

            delete effect;
        } else {
            pEffect = IcewindMisc::sub_585410(pSprite, 1, 8, 3, 0, 0);
            pEffect->m_flags |= 0x2;
            pEffect->m_sourceRes = m_sourceRes;
            message = new CMessageAddEffect(pEffect, pSprite->GetId(), pSprite->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
    } else if (pSprite->GetAIType().GetRace() == CAIOBJECTTYPE_R_TANARI && rand() % 100 < 5) {
        pEffect = IcewindMisc::sub_585410(pSprite, 8, 3, 0, 0, 0);
        pEffect->m_flags |= 0x2;
        pEffect->m_sourceRes = m_sourceRes;
        message = new CMessageAddEffect(pEffect, pSprite->GetId(), pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4A02E0
IcewindCGameEffectMagicalRest::IcewindCGameEffectMagicalRest(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x566BA0
BOOL IcewindCGameEffectMagicalRest::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetBaseStats()->m_fatigue = 0;

    CGameEffect* pEffect = IcewindMisc::CreateEffectRemoveAllOfType(pSprite, 93, 0);
    CMessage* message = new CMessageAddEffect(pEffect, pSprite->GetId(), pSprite->GetId());
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

    pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
        142,
        pSprite->GetTimedEffectList()->GetPosCurrent(),
        39);

    m_done = TRUE;

    return TRUE;
}

// 0x4A0320
CGameEffect* IcewindCGameEffectMagicalRest::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectMagicalRest* copy = new IcewindCGameEffectMagicalRest(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// -----------------------------------------------------------------------------

// 0x4A03D0
IcewindCGameEffectBeholderDispelMagic::IcewindCGameEffectBeholderDispelMagic(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, TRUE)
{
}

// 0x4A0410
CGameEffect* IcewindCGameEffectBeholderDispelMagic::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectBeholderDispelMagic* copy = new IcewindCGameEffectBeholderDispelMagic(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x566C20
BOOL IcewindCGameEffectBeholderDispelMagic::ApplyEffect(CGameSprite* pSprite)
{
    if (IcewindMisc::IsDead(pSprite) == TRUE) {
        m_done = TRUE;
        return TRUE;
    }

    // NOTE: The two strings are the empty string at 0x8C5CF8.
    CProjectile850DDC* pProjectile = new CProjectile850DDC("", "");
    pProjectile->m_speed *= 5;
    pProjectile->m_nWidth = 600;
    pProjectile->m_nLength = 300;

    CPoint ptDirection = IcewindMisc::sub_584610(pSprite->m_nDirection);
    CPoint ptTarget = IcewindMisc::sub_5847B0(pSprite->GetPos(), ptDirection.x, ptDirection.y, 300);

    std::list<CGameEffect*> effects = IcewindMisc::GetSpellResrefEffects(pSprite,
        reinterpret_cast<const BYTE*>(RESREF_BEHOLDER_DISPEL_MAGIC),
        1);
    for (std::list<CGameEffect*>::iterator it = effects.begin(); it != effects.end(); it++) {
        (*it)->m_sourceRes = CResRef(CString(RESREF_BEHOLDER_DISPEL_MAGIC));
        (*it)->m_casterLevel = m_casterLevel;
        pProjectile->AddEffect(*it);
    }

    pProjectile->Fire(pSprite->GetArea(),
        pSprite->GetId(),
        pSprite->GetId(),
        ptTarget,
        0,
        0);

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4A04C0
IcewindCGameEffectHarpyWail::IcewindCGameEffectHarpyWail(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, TRUE)
{
}

// 0x4A0500
CGameEffect* IcewindCGameEffectHarpyWail::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectHarpyWail* copy = new IcewindCGameEffectHarpyWail(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x566E10
BOOL IcewindCGameEffectHarpyWail::ApplyEffect(CGameSprite* pSprite)
{
    if (IcewindMisc::IsDead(pSprite) == TRUE) {
        m_done = TRUE;
        return TRUE;
    }

    CGameEffect* copy = Copy();
    copy->m_duration = 7;
    // NOTE: The duration type is the WORD constant at 0x8491B0 (4).
    copy->m_durationType = 4;
    pSprite->AddEffect(copy, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);

    CProjectileArea* pProjectile = new CProjectileArea(300);
    // NOTE: The string at 0x8ACB70.
    pProjectile->m_explosion1.m_sound = "EFF_P111";

    std::list<CGameEffect*> effects = IcewindMisc::GetSpellResrefEffects(pSprite,
        reinterpret_cast<const BYTE*>(RESREF_HARPY_WAIL),
        1);
    for (std::list<CGameEffect*>::iterator it = effects.begin(); it != effects.end(); it++) {
        (*it)->m_sourceRes = CResRef(CString(RESREF_HARPY_WAIL));
        (*it)->m_casterLevel = m_casterLevel;
        pProjectile->AddEffect(*it);
    }

    pProjectile->Fire(pSprite->GetArea(),
        pSprite->GetId(),
        pSprite->GetId(),
        pSprite->GetPos(),
        0,
        0);

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4A05B0
IcewindCGameEffectJackalwereGaze::IcewindCGameEffectJackalwereGaze(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, TRUE)
{
}

// 0x4A05F0
CGameEffect* IcewindCGameEffectJackalwereGaze::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectJackalwereGaze* copy = new IcewindCGameEffectJackalwereGaze(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x567040
BOOL IcewindCGameEffectJackalwereGaze::ApplyEffect(CGameSprite* pSprite)
{
    if (IcewindMisc::IsDead(pSprite) == TRUE) {
        m_done = TRUE;
        return TRUE;
    }

    CGameEffect* copy = Copy();
    copy->m_duration = 7;
    // NOTE: The duration type is the WORD constant at 0x8491B0 (4).
    copy->m_durationType = 4;
    pSprite->AddEffect(copy, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);

    CGameEffect* pString = IcewindMisc::CreateEffectDisplayString(pSprite, 26939, 0, 0);
    pSprite->AddEffect(pString, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);

    CProjectileArea* pProjectile = new CProjectileArea(300);

    std::list<CGameEffect*> effects = IcewindMisc::GetSpellResrefEffects(pSprite,
        reinterpret_cast<const BYTE*>(RESREF_JACKALWERE_GAZE),
        1);
    for (std::list<CGameEffect*>::iterator it = effects.begin(); it != effects.end(); it++) {
        (*it)->m_sourceRes = CResRef(CString(RESREF_JACKALWERE_GAZE));
        pProjectile->AddEffect(*it);
    }

    pProjectile->Fire(pSprite->GetArea(),
        pSprite->GetId(),
        pSprite->GetId(),
        pSprite->GetPos(),
        0,
        0);

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4A06A0
IcewindCGameEffectSetGlobal::IcewindCGameEffectSetGlobal(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A06E0
CGameEffect* IcewindCGameEffectSetGlobal::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectSetGlobal* copy = new IcewindCGameEffectSetGlobal(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x567230
BOOL IcewindCGameEffectSetGlobal::ApplyEffect(CGameSprite* pSprite)
{
    CString sName;

    if (m_dwFlags == 0) {
        sName = "RETURN_TO_LONELYWOOD";
    } else {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\IcewindCGameEffects.cpp
        // __LINE__: 5535
        UTIL_ASSERT(FALSE);
    }

    CVariable var;

    CVariable* pVariable = g_pBaldurChitin->GetObjectGame()->GetVariables()->FindKey(sName);
    if (pVariable != NULL) {
        pVariable->m_intValue = m_effectAmount;
    } else {
        var.SetName(sName);
        var.m_intValue = m_effectAmount;
        g_pBaldurChitin->GetObjectGame()->m_variables.AddKey(var);
    }

    CMessage* message = new CMessageSetVariable(CString("GLOBAL"),
        sName,
        m_effectAmount,
        pSprite->GetId(),
        pSprite->GetId());
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4A0790
IcewindCGameEffectHopelessness::IcewindCGameEffectHopelessness(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A07D0
CGameEffect* IcewindCGameEffectHopelessness::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectHopelessness* copy = new IcewindCGameEffectHopelessness(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x567450
BOOL IcewindCGameEffectHopelessness::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType) {
        DisplayStringRef(pSprite, 1280);
        m_secondaryType = 0;
    }

    if (!pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_HOPELESSNESS]) {
        pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_HOPELESSNESS] = true;
        pSprite->GetDerivedStats()->m_generalState |= STATE_HELPLESS;
        pSprite->field_9D15 = 1;

        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 47);

        m_done = FALSE;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4A0880
IcewindCGameEffectProtectionFromEvil::IcewindCGameEffectProtectionFromEvil(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A08C0
CGameEffect* IcewindCGameEffectProtectionFromEvil::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectProtectionFromEvil* copy = new IcewindCGameEffectProtectionFromEvil(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x567500
BOOL IcewindCGameEffectProtectionFromEvil::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType) {
        DisplayStringRef(pSprite, 14769);
    }

    if (!pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_PROTECTION_FROM_EVIL]) {
        pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_PROTECTION_FROM_EVIL, true);

        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 9);

        pSprite->GetDerivedStats()->m_naturalImmunities.insert(ICEWIND_CGAMEEFFECT_CHARM);
        sub_4C3F30(pSprite, 2);
        pSprite->GetDerivedStats()->m_nSaveVSFortitude += 2;
        pSprite->GetDerivedStats()->m_nSaveVSReflex += 2;
        pSprite->GetDerivedStats()->m_nSaveVSWill += 2;

        m_done = FALSE;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4A0970
IcewindCGameEffectApplyEffectsList::IcewindCGameEffectApplyEffectsList(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A09B0
CGameEffect* IcewindCGameEffectApplyEffectsList::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectApplyEffectsList* copy = new IcewindCGameEffectApplyEffectsList(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x5675E0
BOOL IcewindCGameEffectApplyEffectsList::ApplyEffect(CGameSprite* pSprite)
{
    CGameObject* pObject;
    BYTE rc;

    m_done = TRUE;

    switch (m_dwFlags) {
    case 0:
        break;
    case 1:
        if (IcewindMisc::IsUndead(pSprite) == FALSE) {
            return TRUE;
        }
        break;
    case 2:
        if (IcewindMisc::IsUndead(pSprite) == TRUE) {
            return TRUE;
        }
        break;
    case 3:
        if (IcewindMisc::IsImmuneToFire(pSprite) == FALSE) {
            return TRUE;
        }
        break;
    case 4:
        if (IcewindMisc::IsImmuneToFire(pSprite) == TRUE) {
            return TRUE;
        }
        break;
    case 5:
        if (IcewindMisc::IsHumanoid(pSprite) == FALSE) {
            return TRUE;
        }
        break;
    case 6:
        if (IcewindMisc::IsHumanoid(pSprite) == TRUE) {
            return TRUE;
        }
        break;
    case 21:
        if (IcewindMisc::IsHumanoid(pSprite) != FALSE) {
            break;
        }
        // FALLTHROUGH
    case 7:
        if (IcewindMisc::IsAnimal(pSprite) == FALSE) {
            return TRUE;
        }
        break;
    case 22:
        if (IcewindMisc::IsHumanoid(pSprite) == TRUE) {
            return TRUE;
        }
        // FALLTHROUGH
    case 8:
        if (IcewindMisc::IsAnimal(pSprite) == TRUE) {
            return TRUE;
        }
        break;
    case 9:
        if (IcewindMisc::IsElemental(pSprite) == FALSE) {
            return TRUE;
        }
        break;
    case 10:
        if (IcewindMisc::IsElemental(pSprite) == TRUE) {
            return TRUE;
        }
        break;
    case 31:
        if (IcewindMisc::IsUndead(pSprite) != FALSE) {
            break;
        }
        // FALLTHROUGH
    case 11:
        if (IcewindMisc::IsFungus(pSprite) == FALSE) {
            return TRUE;
        }
        break;
    case 32:
        if (IcewindMisc::IsUndead(pSprite) == TRUE) {
            return TRUE;
        }
        // FALLTHROUGH
    case 12:
        if (IcewindMisc::IsFungus(pSprite) == TRUE) {
            return TRUE;
        }
        break;
    case 13:
        if (IcewindMisc::IsLarge(pSprite) == FALSE) {
            return TRUE;
        }
        break;
    case 14:
        if (IcewindMisc::IsLarge(pSprite) == TRUE) {
            return TRUE;
        }
        break;
    case 15:
        if (IcewindMisc::IsElf(pSprite) == FALSE) {
            return TRUE;
        }
        break;
    case 16:
        if (IcewindMisc::IsElf(pSprite) == TRUE) {
            return TRUE;
        }
        break;
    case 17:
        if (IcewindMisc::IsUmberhulk(pSprite) == FALSE) {
            return TRUE;
        }
        break;
    case 18:
        if (IcewindMisc::IsUmberhulk(pSprite) == TRUE) {
            return TRUE;
        }
        break;
    case 19:
        if (IcewindMisc::IsHalfElf(pSprite) == FALSE) {
            return TRUE;
        }
        break;
    case 20:
        if (IcewindMisc::IsHalfElf(pSprite) == TRUE) {
            return TRUE;
        }
        break;
    case 23:
        if (IcewindMisc::IsBlind(pSprite) == FALSE) {
            return TRUE;
        }
        break;
    case 24:
        if (IcewindMisc::IsBlind(pSprite) == TRUE) {
            return TRUE;
        }
        break;
    case 53:
        if (IcewindMisc::IsImmuneToFire(pSprite) != FALSE) {
            break;
        }
        // FALLTHROUGH
    case 25:
        if (IcewindMisc::IsImmuneToCold(pSprite) == FALSE) {
            return TRUE;
        }
        break;
    case 54:
        if (IcewindMisc::IsImmuneToFire(pSprite) == TRUE) {
            return TRUE;
        }
        // FALLTHROUGH
    case 26:
        if (IcewindMisc::IsImmuneToCold(pSprite) == TRUE) {
            return TRUE;
        }
        break;
    case 27:
        if (IcewindMisc::IsGolem(pSprite) == FALSE) {
            return TRUE;
        }
        break;
    case 28:
        if (IcewindMisc::IsGolem(pSprite) == TRUE) {
            return TRUE;
        }
        break;
    case 29:
        if (IcewindMisc::IsMinotaur(pSprite) == FALSE) {
            return TRUE;
        }
        break;
    case 30:
        if (IcewindMisc::IsMinotaur(pSprite) == TRUE) {
            return TRUE;
        }
        break;
    case 33:
        if (IcewindMisc::IsGood(pSprite) == FALSE) {
            return TRUE;
        }
        break;
    case 34:
        if (IcewindMisc::IsGood(pSprite) == TRUE) {
            return TRUE;
        }
        break;
    case 35:
        if (IcewindMisc::IsNeutral(pSprite) == FALSE) {
            return TRUE;
        }
        break;
    case 36:
        if (IcewindMisc::IsNeutral(pSprite) == TRUE) {
            return TRUE;
        }
        break;
    case 37:
        if (IcewindMisc::IsEvil(pSprite) == FALSE) {
            return TRUE;
        }
        break;
    case 38:
        if (IcewindMisc::IsEvil(pSprite) == TRUE) {
            return TRUE;
        }
        break;
    case 39:
        if (IcewindMisc::IsPaladin(pSprite) == FALSE) {
            return TRUE;
        }
        break;
    case 40:
        if (IcewindMisc::IsPaladin(pSprite) == TRUE) {
            return TRUE;
        }
        break;
    case 43:
        if (pSprite->GetId() != m_sourceID) {
            return TRUE;
        }
        break;
    case 44:
        if (pSprite->GetId() == m_sourceID) {
            return TRUE;
        }
        break;
    case 45:
        if (IcewindMisc::IsAcquatic(pSprite) == FALSE) {
            return TRUE;
        }
        break;
    case 46:
        if (IcewindMisc::IsAcquatic(pSprite) == TRUE) {
            return TRUE;
        }
        break;
    case 47:
        if (IcewindMisc::IsLiving(pSprite) == FALSE) {
            return TRUE;
        }
        break;
    case 48:
        if (IcewindMisc::IsLiving(pSprite) == TRUE) {
            return TRUE;
        }
        break;
    case 55:
        if (IcewindMisc::sud_585070(pSprite) == FALSE) {
            return TRUE;
        }
        break;
    case 56:
        if (IcewindMisc::sud_585070(pSprite) == TRUE) {
            return TRUE;
        }
        break;
    case 57:
        if (IcewindMisc::IsMale(pSprite) == FALSE) {
            return TRUE;
        }
        break;
    case 58:
        if (IcewindMisc::IsMale(pSprite) == TRUE) {
            return TRUE;
        }
        break;
    case 59:
        if (IcewindMisc::IsLawful(pSprite) == FALSE) {
            return TRUE;
        }
        break;
    case 60:
        if (IcewindMisc::IsLawful(pSprite) == TRUE) {
            return TRUE;
        }
        break;
    case 61:
        if (IcewindMisc::IsChaotic(pSprite) == FALSE) {
            return TRUE;
        }
        break;
    case 62:
        if (IcewindMisc::IsChaotic(pSprite) == TRUE) {
            return TRUE;
        }
        break;
    case 63:
        if (pSprite->GetAIType().GetRace() != CAIOBJECTTYPE_R_ORC) {
            return TRUE;
        }
        break;
    case 64:
        if (pSprite->GetAIType().GetRace() == CAIOBJECTTYPE_R_ORC) {
            return TRUE;
        }
        break;
    case 65:
        if (!pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_DEAFENED]) {
            return TRUE;
        }
        break;
    case 66:
        if (pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_DEAFENED]) {
            return TRUE;
        }
        break;
    case 71:
        if (pSprite->GetBaseStats()->m_hitPoints <= m_effectAmount) {
            return TRUE;
        }
        break;
    case 72:
        if (m_effectAmount <= pSprite->GetBaseStats()->m_hitPoints) {
            return TRUE;
        }
        break;
    case 73:
        if (pSprite->GetAIType().GetRace() != CAIOBJECTTYPE_R_ELF) {
            return TRUE;
        }
        if (pSprite->GetAIType().GetSubRace() != CAIOBJECTTYPE_SUBRACE_ELF_DROW) {
            return TRUE;
        }
        break;
    case 74:
        if (pSprite->GetAIType().GetRace() == CAIOBJECTTYPE_R_ELF
            && pSprite->GetAIType().GetSubRace() == CAIOBJECTTYPE_SUBRACE_ELF_DROW) {
            return TRUE;
        }
        break;
    case 75:
        if (pSprite->GetAIType().GetRace() != CAIOBJECTTYPE_R_DWARF) {
            return TRUE;
        }
        if (pSprite->GetAIType().GetSubRace() != CAIOBJECTTYPE_SUBRACE_DWARF_GRAY) {
            return TRUE;
        }
        break;
    case 76:
        if (pSprite->GetAIType().GetRace() == CAIOBJECTTYPE_R_DWARF
            && pSprite->GetAIType().GetSubRace() == CAIOBJECTTYPE_SUBRACE_DWARF_GRAY) {
            return TRUE;
        }
        break;
    case 77:
        if ((pSprite->GetBaseStats()->m_generalState & STATE_SUMMONED_CREATURE) == 0) {
            return TRUE;
        }
        break;
    case 78:
        if ((pSprite->GetBaseStats()->m_generalState & STATE_SUMMONED_CREATURE) != 0) {
            return TRUE;
        }
        break;
    case 79:
        if (pSprite->GetAIType().GetRace() != CAIOBJECTTYPE_R_GOBLIN) {
            return TRUE;
        }
        break;
    case 80:
        if (pSprite->GetAIType().GetRace() == CAIOBJECTTYPE_R_GOBLIN) {
            return TRUE;
        }
        break;
    case 81:
        if (pSprite->GetAIType().GetRace() != CAIOBJECTTYPE_R_GIANT) {
            return TRUE;
        }
        break;
    case 82:
        if (pSprite->GetAIType().GetRace() == CAIOBJECTTYPE_R_GIANT) {
            return TRUE;
        }
        break;
    case 83:
        if (pSprite->GetAIType().GetRace() != CAIOBJECTTYPE_R_TROLL) {
            return TRUE;
        }
        break;
    case 84:
        if (pSprite->GetAIType().GetRace() == CAIOBJECTTYPE_R_TROLL) {
            return TRUE;
        }
        break;
    case 85:
        if (pSprite->GetAIType().GetRace() != CAIOBJECTTYPE_R_190) {
            return TRUE;
        }
        break;
    case 86:
        if (pSprite->GetAIType().GetRace() == CAIOBJECTTYPE_R_190) {
            return TRUE;
        }
        break;
    case 87:
        if (pSprite->GetAIType().GetRace() != CAIOBJECTTYPE_R_HUMAN) {
            return TRUE;
        }
        break;
    case 88:
        if (pSprite->GetAIType().GetRace() == CAIOBJECTTYPE_R_HUMAN) {
            return TRUE;
        }
        break;
    case 89:
        if (pSprite->GetAIType().GetRace() != CAIOBJECTTYPE_R_YUANTI) {
            return TRUE;
        }
        break;
    case 90:
        if (pSprite->GetAIType().GetRace() == CAIOBJECTTYPE_R_YUANTI) {
            return TRUE;
        }
        break;
    case 91:
        if (IcewindMisc::IsOutsider(pSprite) == FALSE) {
            return TRUE;
        }
        break;
    case 92:
        if (IcewindMisc::IsOutsider(pSprite) == TRUE) {
            return TRUE;
        }
        break;
    default:
        // NOTE: 41, 42, 49 to 52, 67 to 70 and every value above 92 compare
        // with the source.
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_sourceID,
            CGameObjectArray::THREAD_ASYNCH,
            &pObject,
            INFINITE);
        if (rc != CGameObjectArray::SUCCESS) {
            return FALSE;
        }

        switch (m_dwFlags) {
        case 41:
            if (IcewindMisc::IsGoodEvilSame(pSprite, static_cast<CGameSprite*>(pObject)) == FALSE) {
                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceID,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
                return TRUE;
            }
            break;
        case 42:
            if (IcewindMisc::IsGoodEvilSame(pSprite, static_cast<CGameSprite*>(pObject)) == TRUE) {
                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceID,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
                return TRUE;
            }
            break;
        case 49:
            if (IcewindMisc::sub_585230(pSprite, static_cast<CGameSprite*>(pObject)) == FALSE) {
                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceID,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
                return TRUE;
            }
            break;
        case 50:
            if (IcewindMisc::sub_585230(pSprite, static_cast<CGameSprite*>(pObject)) == TRUE) {
                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceID,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
                return TRUE;
            }
            break;
        case 51:
            if (IcewindMisc::sub_5852A0(pSprite, static_cast<CGameSprite*>(pObject)) == FALSE) {
                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceID,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
                return TRUE;
            }
            break;
        case 52:
            if (IcewindMisc::sub_5852A0(pSprite, static_cast<CGameSprite*>(pObject)) == TRUE) {
                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceID,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
                return TRUE;
            }
            break;
        case 67:
            if (IcewindMisc::IsAlignmentSame(pSprite, static_cast<CGameSprite*>(pObject)) == FALSE) {
                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceID,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
                return TRUE;
            }
            break;
        case 68:
            if (IcewindMisc::IsAlignmentSame(pSprite, static_cast<CGameSprite*>(pObject)) == TRUE) {
                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceID,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
                return TRUE;
            }
            break;
        case 69:
            if (IcewindMisc::IsAlignmentSame(pSprite, static_cast<CGameSprite*>(pObject)) != TRUE
                || IcewindMisc::sub_585230(pSprite, static_cast<CGameSprite*>(pObject)) != TRUE) {
                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceID,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
                return TRUE;
            }
            break;
        case 70:
            if (IcewindMisc::IsAlignmentSame(pSprite, static_cast<CGameSprite*>(pObject)) == TRUE
                && IcewindMisc::sub_585230(pSprite, static_cast<CGameSprite*>(pObject)) == TRUE) {
                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceID,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
                return TRUE;
            }
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\IcewindCGameEffects.cpp
            // __LINE__: 5802
            UTIL_ASSERT_MSG(FALSE, "Out-of-range value detected in IcewindCGameEffectApplyEffectsList (-rjf)");
            break;
        }

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceID,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        break;
    }

    CGameObject* pSource;
    rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_sourceID,
        CGameObjectArray::THREAD_ASYNCH,
        &pSource,
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        m_done = TRUE;
        return TRUE;
    }

    std::list<CGameEffect*> effects = IcewindMisc::GetSpellResrefEffects(pSource,
        m_res.GetResRef(),
        static_cast<BYTE>(m_casterLevel));

    for (std::list<CGameEffect*>::iterator it = effects.begin(); it != effects.end(); it++) {
        CGameEffect* pEffect = *it;
        pEffect->m_sourceRes = m_sourceRes;
        pEffect->m_target = m_target;
        pEffect->m_casterLevel = m_casterLevel;
        if (pEffect->m_targetType == 1) {
            if (pSource->GetObjectType() == CGameObject::TYPE_SPRITE) {
                static_cast<CGameSprite*>(pSource)->AddEffect(pEffect, CGameAIBase::EFFECT_LIST_TIMED, FALSE, TRUE);
            }
        } else {
            pSprite->AddEffect(pEffect, CGameAIBase::EFFECT_LIST_TIMED, FALSE, TRUE);
        }
    }

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceID,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4A0A60
IcewindCGameEffectArmorOfFaith::IcewindCGameEffectArmorOfFaith(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A0AA0
CGameEffect* IcewindCGameEffectArmorOfFaith::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectArmorOfFaith* copy = new IcewindCGameEffectArmorOfFaith(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x568930
BOOL IcewindCGameEffectArmorOfFaith::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType) {
        DisplayStringRef(pSprite, 509);
    }

    if (!pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_ARMOR_OF_FAITH]) {
        // NOTE: Using `set` (with bounds check).
        pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_ARMOR_OF_FAITH, true);

        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 84);

        pSprite->GetDerivedStats()->m_nResistCrushing++;
        pSprite->GetDerivedStats()->m_nResistPiercing++;
        pSprite->GetDerivedStats()->m_nResistSlashing++;
        pSprite->GetDerivedStats()->m_nResistFire++;
        pSprite->GetDerivedStats()->m_nResistCold++;
        pSprite->GetDerivedStats()->m_nResistAcid++;
        pSprite->GetDerivedStats()->m_nResistElectricity++;
        pSprite->GetDerivedStats()->m_nResistMagic++;

        m_done = FALSE;
    }
    return TRUE;
}

// 0x55ECF0
void IcewindCGameEffectArmorOfFaith::OnRemove(CGameSprite* pSprite)
{
    PlaySound(CResRef("EFF_E03"), pSprite);
}

// -----------------------------------------------------------------------------

// 0x4A0B50
IcewindCGameEffectNausea::IcewindCGameEffectNausea(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A0B90
CGameEffect* IcewindCGameEffectNausea::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectNausea* copy = new IcewindCGameEffectNausea(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x568A00
BOOL IcewindCGameEffectNausea::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        pSprite->GetDerivedStats()->m_generalState |= STATE_HELPLESS | STATE_SLEEPING;

        // NOTE: Using `set` (with bounds check).
        pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_DOESNT_AWAKEN_ON_DAMAGE, true);

        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 43);

        if (m_secondaryType) {
            DisplayStringRef(pSprite, 4390); // "Nauseous"

            ITEM_EFFECT* effect = new ITEM_EFFECT;
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
        break;
    case 1:
        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 43);

        if (m_secondaryType) {
            DisplayStringRef(pSprite, 4390); // "Nauseous"
        }

        pSprite->GetDerivedStats()->m_nDamageBonus -= 2;
        pSprite->GetDerivedStats()->m_nTHAC0 -= 2;
        pSprite->GetDerivedStats()->m_nSaveVSFortitude -= 2;
        pSprite->GetDerivedStats()->m_nSaveVSReflex -= 2;
        pSprite->GetDerivedStats()->m_nSaveVSWill -= 2;
        break;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4A0C40
IcewindCGameEffectEnfeeblement::IcewindCGameEffectEnfeeblement(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A0C80
CGameEffect* IcewindCGameEffectEnfeeblement::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectEnfeeblement* copy = new IcewindCGameEffectEnfeeblement(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x568C10
BOOL IcewindCGameEffectEnfeeblement::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType) {
        DisplayStringRef(pSprite, 7924); // "Enfeebled"

        SHORT v1 = rand() % 6 + 1;
        SHORT v2 = static_cast<BYTE>(m_casterLevel) - 1 > 10
            ? 5
            : static_cast<BYTE>(m_casterLevel) / 2;
        m_effectAmount = v1 + v2;
    }

    if (!pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_ENFEEBLED]) {
        pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_ENFEEBLED] = true;

        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 90);

        pSprite->GetDerivedStats()->m_nSTR -= 15;
        if (pSprite->GetDerivedStats()->m_nSTR < 1) {
            pSprite->GetDerivedStats()->m_nSTR = 1;
        }

        m_done = FALSE;
    }
    return TRUE;
}

// 0x4BAF90
void IcewindCGameEffectEnfeeblement::OnRemove(CGameSprite* pSprite)
{
    PlaySound(CResRef("EFF_E04"), pSprite);
}

// -----------------------------------------------------------------------------

// 0x4A0D30
IcewindCGameEffectFireshield::IcewindCGameEffectFireshield(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A0D70
CGameEffect* IcewindCGameEffectFireshield::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectFireshield* copy = new IcewindCGameEffectFireshield(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x568D00
BOOL IcewindCGameEffectFireshield::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType != 0) {
        DisplayStringRef(pSprite, 736);
    }

    if (m_dwFlags == 0) {
        if (pSprite->GetDerivedStats()->m_spellStates.test(SPLSTATE_RED_FIRESHIELD) == true) {
            return TRUE;
        }

        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 121);

        pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_RED_FIRESHIELD, true);
        pSprite->GetDerivedStats()->m_visualEffects.set(IWD_VFX_FIRE_SHIELD_RED, true);
        pSprite->GetDerivedStats()->m_visualEffects.set(IWD_VFX_28, true);

        CWhenStruckEntry entry;
        entry.m_sourceRes = m_sourceRes;
        entry.m_res = m_res;
        entry.m_casterLevel = static_cast<BYTE>(m_casterLevel);
        entry.m_target = m_target;
        pSprite->GetDerivedStats()->m_whenStruckList.push_back(entry);

        // NOTE: The entry is sent to the other players here (vtable
        // 0x84D45C). Omitted: single player only
        // (docs/adr/0002-single-player-only.md).
    } else if (m_dwFlags == 1) {
        if (pSprite->GetDerivedStats()->m_spellStates.test(SPLSTATE_BLUE_FIRESHIELD) == true) {
            return TRUE;
        }

        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 122);

        pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_BLUE_FIRESHIELD, true);
        pSprite->GetDerivedStats()->m_visualEffects.set(IWD_VFX_FIRE_SHIELD_BLUE, true);
        pSprite->GetDerivedStats()->m_visualEffects.set(IWD_VFX_29, true);

        CWhenStruckEntry entry;
        entry.m_sourceRes = m_sourceRes;
        entry.m_res = m_res;
        entry.m_casterLevel = static_cast<BYTE>(m_casterLevel);
        entry.m_target = m_target;
        pSprite->GetDerivedStats()->m_whenStruckList.push_back(entry);

        // NOTE: The entry is sent to the other players here (vtable
        // 0x84D45C). Omitted: single player only
        // (docs/adr/0002-single-player-only.md).
    }

    m_done = FALSE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4A0E20
IcewindCGameEffectDeathWard::IcewindCGameEffectDeathWard(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A0E60
CGameEffect* IcewindCGameEffectDeathWard::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectDeathWard* copy = new IcewindCGameEffectDeathWard(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x569090
BOOL IcewindCGameEffectDeathWard::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType != 0) {
        DisplayStringRef(pSprite, 1309);
    }

    if (pSprite->GetDerivedStats()->m_spellStates.test(SPLSTATE_DEATH_WARD)) {
        return TRUE;
    }

    pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_DEATH_WARD, true);

    // NOTE: Uninline.
    AddPortraitIcon(pSprite, 87);

    // NOTE: Inlined `std::set::insert` (0x44AB10).
    pSprite->GetDerivedStats()->m_naturalImmunities.insert(ICEWIND_CGAMEEFFECT_MAGIC_DEATH);

    m_done = FALSE;

    return TRUE;
}

// 0x4BAF90
void IcewindCGameEffectDeathWard::OnRemove(CGameSprite* pSprite)
{
    PlaySound(CResRef("EFF_E04"), pSprite);
}

// -----------------------------------------------------------------------------

// 0x4A0F10
IcewindCGameEffectHolyPower::IcewindCGameEffectHolyPower(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A0F50
CGameEffect* IcewindCGameEffectHolyPower::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectHolyPower* copy = new IcewindCGameEffectHolyPower(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x569210
BOOL IcewindCGameEffectHolyPower::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType) {
        DisplayStringRef(pSprite, 1315); // "Holy Power"
    }

    if (!pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_HOLY_POWER]) {
        pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_HOLY_POWER] = true;

        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 86);

        AddColorEffect(pSprite, 128, 128, 128, 20);

        pSprite->m_bonusStats.m_nDamageBonus += 4;
        m_done = FALSE;
    }
    return TRUE;
}

// 0x4BAF90
void IcewindCGameEffectHolyPower::OnRemove(CGameSprite* pSprite)
{
    PlaySound(CResRef("EFF_E04"), pSprite);
}

// -----------------------------------------------------------------------------

// 0x4A1000
IcewindCGameEffectVisualEffect::IcewindCGameEffectVisualEffect(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A1040
CGameEffect* IcewindCGameEffectVisualEffect::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectVisualEffect* copy = new IcewindCGameEffectVisualEffect(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x5692D0
BOOL IcewindCGameEffectVisualEffect::ApplyEffect(CGameSprite* pSprite)
{
    // NOTE: Using `set` (with bounds check).
    pSprite->GetDerivedStats()->m_visualEffects.set(m_dwFlags, true);

    if (m_dwFlags == IWD_VFX_SHIELD_OF_LATHANDER) {
        pSprite->GetDerivedStats()->m_visualEffects[IWD_VFX_SHIELD_OF_LATHANDER_2] = true;
    }

    if (m_dwFlags == IWD_VFX_GREATER_SHIELD_OF_LATHANDER) {
        pSprite->GetDerivedStats()->m_visualEffects[IWD_VFX_GREATER_SHIELD_OF_LATHANDER_2] = true;
    }

    if (m_dwFlags == IWD_VFX_SEVEN_EYES) {
        pSprite->GetDerivedStats()->m_visualEffects[IWD_VFX_SEVEN_EYES_2] = true;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4A10F0
IcewindCGameEffectOtilukesResilientSphere::IcewindCGameEffectOtilukesResilientSphere(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A1130
CGameEffect* IcewindCGameEffectOtilukesResilientSphere::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectOtilukesResilientSphere* copy = new IcewindCGameEffectOtilukesResilientSphere(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x569320
BOOL IcewindCGameEffectOtilukesResilientSphere::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType) {
        DisplayStringRef(pSprite, 4732); // "Encased in Otiluke's Resilient Sphere"
    }

    pSprite->field_9D15 = 1;

    // NOTE: Uninline.
    AddPortraitIcon(pSprite, 100);

    pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_HELD] = true;
    pSprite->GetDerivedStats()->m_generalState |= STATE_HELPLESS;
    pSprite->GetDerivedStats()->m_visualEffects[IWD_VFX_OTILUKES_RESILIENT_SPHERE] = true;
    pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_OTILUKES_RESILIENT_SPHERE] = true;

    return TRUE;
}

// 0x4BAF90
void IcewindCGameEffectOtilukesResilientSphere::OnRemove(CGameSprite* pSprite)
{
    PlaySound(CResRef("EFF_E04"), pSprite);
}

// -----------------------------------------------------------------------------

// 0x4A11E0
IcewindCGameEffectBarkskin::IcewindCGameEffectBarkskin(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A1220
CGameEffect* IcewindCGameEffectBarkskin::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectBarkskin* copy = new IcewindCGameEffectBarkskin(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x5693B0
BOOL IcewindCGameEffectBarkskin::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType) {
        DisplayStringRef(pSprite, 14785); // "Barkskin"
    }

    if (!pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_BARKSKIN]) {
        pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_BARKSKIN] = true;

        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 20);

        if (m_casterLevel < 6) {
            pSprite->GetDerivedStats()->field_C += 3;
        } else if (m_casterLevel < 12) {
            pSprite->GetDerivedStats()->field_C += 4;
        } else {
            pSprite->GetDerivedStats()->field_C += 5;
        }

        pSprite->SetColorRange(2);
    }
    return TRUE;
}

// 0x55ECF0
void IcewindCGameEffectBarkskin::OnRemove(CGameSprite* pSprite)
{
    PlaySound(CResRef("EFF_E03"), pSprite);
}

// -----------------------------------------------------------------------------

// 0x4A12D0
IcewindCGameEffectBleedingWounds::IcewindCGameEffectBleedingWounds(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A1310
CGameEffect* IcewindCGameEffectBleedingWounds::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectBleedingWounds* copy = new IcewindCGameEffectBleedingWounds(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x569A30
BOOL IcewindCGameEffectBleedingWounds::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType != 0) {
        DisplayStringRef(pSprite, 26877);
    }

    // NOTE: Uninline.
    AddPortraitIcon(pSprite, 85);

    if (m_secondaryType != 0) {
        m_special = g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime;
    }

    DWORD duration = m_durationType != 1 ? m_duration : 0;

    CPersistantEffect84C420* pEffect;
    switch (m_dwFlags) {
    case 0:
        pEffect = new CPersistantEffect84C420(0x20000000,
            m_effectAmount,
            105,
            m_special,
            duration);
        break;
    case 1:
        pEffect = new CPersistantEffect84C420(0x20000000,
            m_effectAmount,
            15,
            m_special,
            duration);
        break;
    case 2:
        pEffect = new CPersistantEffect84C420(0x20000000,
            1,
            15 * m_effectAmount,
            m_special,
            duration);
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\IcewindCGameEffects.cpp
        // __LINE__: 6408
        UTIL_ASSERT(FALSE);

        // DEVIATION: past the assertion the original adds its stale register
        // (the sprite pointer) to the list as a persistant effect, which
        // would corrupt memory when the list runs. Bounded (ruling P4-10):
        // ours adds nothing.
        return TRUE;
    }

    pSprite->GetDerivedStats()->m_cRegeneratedPersistantEffectList.AddTail(pEffect);

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4A13C0
IcewindCGameEffectAreaOfEffect::IcewindCGameEffectAreaOfEffect(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A1400
CGameEffect* IcewindCGameEffectAreaOfEffect::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectAreaOfEffect* copy = new IcewindCGameEffectAreaOfEffect(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x569470
BOOL IcewindCGameEffectAreaOfEffect::ApplyEffect(CGameSprite* pSprite)
{
    if (IcewindMisc::IsDead(pSprite) == TRUE) {
        m_done = TRUE;
        return TRUE;
    }

    switch (m_dwFlags) {
    case 0:
    case 2: {
            CGameObject* pSource;
            BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_sourceID,
                CGameObjectArray::THREAD_ASYNCH,
                &pSource,
                INFINITE);
            if (rc == CGameObjectArray::SUCCESS) {
                CProjectileArea* pProjectile = new CProjectileArea(static_cast<WORD>(m_effectAmount));
                if (m_dwFlags == 2) {
                    // NOTE: Inlined `std::set::insert` (FUN_004C45A0).
                    pProjectile->m_excludedTargets.insert(pSprite->GetId());
                }

                std::list<CGameEffect*> effects = IcewindMisc::GetSpellResrefEffects(pSource,
                    m_res.GetResRef(),
                    1);
                for (std::list<CGameEffect*>::iterator it = effects.begin(); it != effects.end(); it++) {
                    (*it)->m_sourceRes = m_res;
                    pProjectile->AddEffect(*it);
                }

                pProjectile->Fire(pSprite->GetArea(),
                    pSource->GetId(),
                    pSprite->GetId(),
                    pSprite->GetPos(),
                    0,
                    0);

                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceID,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
                m_done = TRUE;
            } else {
                m_done = TRUE;
            }
        break;
    }
    case 1:
    case 3: {
            if (m_secondaryType != 0) {
                m_special = g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime;
            }

            CPersistantEffect84C4B4* pEffect;
            // NOTE: The duration type is the WORD constant at 0x8491AA (1).
            if (m_durationType == 1) {
                pEffect = new CPersistantEffect84C4B4(m_res.GetResRefStr(),
                    m_effectAmount,
                    105,
                    m_special,
                    0);
            } else {
                pEffect = new CPersistantEffect84C4B4(m_res.GetResRefStr(),
                    m_effectAmount,
                    105,
                    m_special,
                    m_duration);
            }

            pEffect->m_sourceID = m_sourceID;
            strcpy(reinterpret_cast<char*>(pEffect->field_29), m_sourceRes.GetResRefStr());

            if (m_dwFlags == 3) {
                // NOTE: Inlined `std::set::insert` (FUN_004C54E0); the set
                // holds the ids of the sprites it leaves out.
                pEffect->field_58.insert(reinterpret_cast<void*>(static_cast<INT_PTR>(pSprite->GetId())));
            }

            pSprite->GetDerivedStats()->m_cRegeneratedPersistantEffectList.AddTail(pEffect);
        break;
    }
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4A14B0
IcewindCGameEffectFreeAction::IcewindCGameEffectFreeAction(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A14F0
CGameEffect* IcewindCGameEffectFreeAction::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectFreeAction* copy = new IcewindCGameEffectFreeAction(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x5698E0
BOOL IcewindCGameEffectFreeAction::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType) {
        DisplayStringRef(pSprite, 14794);
    }

    if (!pSprite->GetDerivedStats()->m_spellStates.test(SPLSTATE_FREE_ACTION)) {
        pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_FREE_ACTION, true);

        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 19);

        AddColorEffect(pSprite, 128, 96, 96, 30);

        pSprite->GetDerivedStats()->m_naturalImmunities.insert(CGAMEEFFECT_ENTANGLE);
        pSprite->GetDerivedStats()->m_naturalImmunities.insert(CGAMEEFFECT_GREASEPOOL);
        pSprite->GetDerivedStats()->m_naturalImmunities.insert(CGAMEEFFECT_WEBHOLD);
        pSprite->GetDerivedStats()->m_naturalImmunities.insert(CGAMEEFFECT_HOLDCREATURE);
        pSprite->GetDerivedStats()->m_naturalImmunities.insert(CGAMEEFFECT_SLOW);
        pSprite->GetDerivedStats()->m_naturalImmunities.insert(CGAMEEFFECT_MOVEMENTRATE_176);

        m_done = FALSE;
    }

    return TRUE;
}

// 0x55ECF0
void IcewindCGameEffectFreeAction::OnRemove(CGameSprite* pSprite)
{
    PlaySound(CResRef("EFF_E03"), pSprite);
}

// -----------------------------------------------------------------------------

// 0x4A15A0
IcewindCGameEffectUnconsciousness::IcewindCGameEffectUnconsciousness(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A15E0
CGameEffect* IcewindCGameEffectUnconsciousness::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectUnconsciousness* copy = new IcewindCGameEffectUnconsciousness(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x569BD0
BOOL IcewindCGameEffectUnconsciousness::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetDerivedStats()->m_generalState |= STATE_HELPLESS | STATE_SLEEPING;

    if (m_dwFlags == 1) {
        // NOTE: Using `set` (with bounds check).
        pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_DOESNT_AWAKEN_ON_DAMAGE, true);
    }

    // NOTE: Uninline.
    AddPortraitIcon(pSprite, 88);

    if (m_secondaryType) {
        DisplayStringRef(pSprite, 20438); // "Knocked Unconscious"

        ITEM_EFFECT* effect = new ITEM_EFFECT;
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

// 0x4A1690
IcewindCGameEffectEntropyShield::IcewindCGameEffectEntropyShield(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A16D0
CGameEffect* IcewindCGameEffectEntropyShield::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectEntropyShield* copy = new IcewindCGameEffectEntropyShield(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x569D30
BOOL IcewindCGameEffectEntropyShield::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType) {
        DisplayStringRef(pSprite, 8104);
    }

    if (!pSprite->GetDerivedStats()->m_spellStates.test(SPLSTATE_ENTROPY_SHIELD)) {
        pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_ENTROPY_SHIELD, true);

        AddPortraitIcon(pSprite, 103);

        pSprite->GetDerivedStats()->m_visualEffects.set(IWD_VFX_ENTROPY_SHIELD, true);
        AddColorEffect(pSprite, 64, 192, 64, 30);
        sub_4C3F30(pSprite, 6);

        pSprite->m_bonusStats.m_nResistFire += 10;
        pSprite->m_bonusStats.m_nResistCold += 10;
        pSprite->m_bonusStats.m_nResistElectricity += 10;
        pSprite->m_bonusStats.m_nResistAcid += 10;
        pSprite->m_bonusStats.m_nSaveVSFortitude += 10;
        pSprite->m_bonusStats.m_nSaveVSReflex += 10;
        pSprite->m_bonusStats.m_nSaveVSWill += 10;

        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(1));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(3));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(4));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(5));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(6));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(8));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(9));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(10));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(11));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(13));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(14));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(15));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(16));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(18));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(19));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(20));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(23));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(26));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(28));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(29));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(30));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(31));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(33));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(34));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(35));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(36));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(55));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(57));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(58));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(59));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(79));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(101));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(102));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(105));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(107));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(187));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(191));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(192));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(193));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(217));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(226));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(227));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(228));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(229));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(230));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(231));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(232));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(233));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(250));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(267));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(268));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(270));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(284));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(297));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(302));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(312));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(313));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(315));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(345));
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(350));
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4A1780
IcewindCGameEffectStormShell::IcewindCGameEffectStormShell(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A17C0
CGameEffect* IcewindCGameEffectStormShell::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectStormShell* copy = new IcewindCGameEffectStormShell(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x56A090
BOOL IcewindCGameEffectStormShell::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType) {
        DisplayStringRef(pSprite, 8830); // "Protected by Storm Shell"
    }

    if (!pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_STORM_SHELL]) {
        // NOTE: Using `set` (with bounds check).
        pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_STORM_SHELL, true);

        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 105);

        pSprite->GetDerivedStats()->m_visualEffects[IWD_VFX_STORM_SHELL] = true;
        pSprite->m_bonusStats.m_nResistFire += 15;
        pSprite->m_bonusStats.m_nResistCold += 15;
        pSprite->m_bonusStats.m_nResistElectricity += 15;
    }
    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4A1870
IcewindCGameEffectProtectionFromElements::IcewindCGameEffectProtectionFromElements(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A18B0
CGameEffect* IcewindCGameEffectProtectionFromElements::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectProtectionFromElements* copy = new IcewindCGameEffectProtectionFromElements(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x56A140
BOOL IcewindCGameEffectProtectionFromElements::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType) {
        DisplayStringRef(pSprite, 8842); // "Protected by Protection from the Elements"
    }

    if (!pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_PROTECTION_FROM_THE_ELEMENTS]) {
        pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_PROTECTION_FROM_THE_ELEMENTS] = true;

        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 93);

        AddColorEffect(pSprite, 192, 0, 0, 61);
        AddColorEffect(pSprite, 0, 192, 0, 43);
        AddColorEffect(pSprite, 0, 0, 192, 79);

        pSprite->m_bonusStats.m_nResistFire += 15;
        pSprite->m_bonusStats.m_nResistCold += 15;
        pSprite->m_bonusStats.m_nResistElectricity += 15;
        pSprite->m_bonusStats.m_nResistAcid += 15;
    }
    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4A1960
IcewindCGameEffectAegis::IcewindCGameEffectAegis(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A19A0
CGameEffect* IcewindCGameEffectAegis::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectAegis* copy = new IcewindCGameEffectAegis(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x56A230
BOOL IcewindCGameEffectAegis::ApplyEffect(CGameSprite* pSprite)
{
    CDerivedStats* pStats = pSprite->GetDerivedStats();

    if (pStats->m_spellStates.test(SPLSTATE_AEGIS) == true) {
        return TRUE;
    }

    pStats->m_spellStates.set(SPLSTATE_AEGIS, true);

    // NOTE: Uninline.
    AddPortraitIcon(pSprite, 119);

    pStats->m_nResistFire += 15;
    pStats->m_nResistCold += 15;
    pStats->m_nResistElectricity += 15;
    pStats->m_nResistAcid += 15;
    sub_4C3F30(pSprite, 5);
    pStats->m_nResistMagic += 3;
    pStats->m_nSaveVSFortitude += 2;
    pStats->m_nSaveVSReflex += 2;
    pStats->m_nSaveVSWill += 2;

    pStats->m_naturalImmunities.insert(218);

    if (m_secondaryType != 0) {
        m_effectAmount = 8;
    }

    if (pStats->m_nStoneSkins <= m_effectAmount) {
        pStats->m_nStoneSkins = m_effectAmount;
    }
    if (pStats->m_nStoneSkins > 0) {
        pSprite->SetColorRange(14);
    }

    pStats->m_nResistCrushing += 10;
    pStats->m_nResistPiercing += 10;
    pStats->m_nResistSlashing += 10;

    // NOTE: Using `set` (with bounds check). The original sets bit 6, the
    // minor globe's.
    pStats->m_visualEffects.set(IWD_VFX_MINOR_GLOBE_OF_INVULNERABILITY, true);

    pStats->field_1E0 = 1;
    pStats->field_1E4 = 1;
    pStats->field_1E8 = 1;

    AddColorEffect(pSprite, 128, 96, 96, 30);

    pStats->m_naturalImmunities.insert(154);
    pStats->m_naturalImmunities.insert(158);
    pStats->m_naturalImmunities.insert(157);
    pStats->m_naturalImmunities.insert(109);
    pStats->m_naturalImmunities.insert(40);
    // NOTE: Inlined `std::set::insert` (0x44AB10).
    pStats->m_naturalImmunities.insert(176);

    return TRUE;
}

// 0x56B6E0
void IcewindCGameEffectAegis::OnRemove(CGameSprite* pSprite)
{
    PlaySound(CResRef("EFF_E01"), pSprite);
}

// -----------------------------------------------------------------------------

// 0x4A1A50
IcewindCGameEffectExecutionersEyes::IcewindCGameEffectExecutionersEyes(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A1A90
CGameEffect* IcewindCGameEffectExecutionersEyes::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectExecutionersEyes* copy = new IcewindCGameEffectExecutionersEyes(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x56A500
BOOL IcewindCGameEffectExecutionersEyes::ApplyEffect(CGameSprite* pSprite)
{
    if (!pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_EXECUTIONERS_EYES]) {
        pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_EXECUTIONERS_EYES] = true;

        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 120);

        pSprite->SetColorRange(8);
        pSprite->GetDerivedStats()->m_nTHAC0 += 4;
        pSprite->GetDerivedStats()->m_nCriticalHitBonus += 4;
    }
    return TRUE;
}

// 0x56B6E0
void IcewindCGameEffectExecutionersEyes::OnRemove(CGameSprite* pSprite)
{
    PlaySound(CResRef("EFF_E01"), pSprite);
}

// -----------------------------------------------------------------------------

// 0x4A1B40
IcewindCGameEffectWhenStruck::IcewindCGameEffectWhenStruck(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A1B80
CGameEffect* IcewindCGameEffectWhenStruck::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectWhenStruck* copy = new IcewindCGameEffectWhenStruck(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x56A5A0
BOOL IcewindCGameEffectWhenStruck::ApplyEffect(CGameSprite* pSprite)
{
    CWhenStruckEntry entry;
    entry.m_sourceRes = m_sourceRes;
    entry.m_res = m_res;
    entry.m_casterLevel = static_cast<BYTE>(m_casterLevel);
    entry.m_target = m_target;
    pSprite->GetDerivedStats()->m_whenStruckList.push_back(entry);

    // NOTE: The entry is sent to the other players here (vtable 0x84D45C).
    // Omitted: single player only (docs/adr/0002-single-player-only.md).

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4A1C30
IcewindCGameEffectProjectile::IcewindCGameEffectProjectile(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A1C70
CGameEffect* IcewindCGameEffectProjectile::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectProjectile* copy = new IcewindCGameEffectProjectile(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x56A730
BOOL IcewindCGameEffectProjectile::ApplyEffect(CGameSprite* pSprite)
{
    CGameObject* pSource;
    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_sourceID,
        CGameObjectArray::THREAD_ASYNCH,
        &pSource,
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        m_done = TRUE;
        return TRUE;
    }

    // NOTE: Most types still decode to the instant fallback (see
    // DecodeProjectile 0x51EAF0).
    CProjectile* pProjectile = CProjectile::DecodeProjectile(static_cast<WORD>(m_dwFlags + 1), pSprite, 0);

    std::list<CGameEffect*> effects = IcewindMisc::GetSpellResrefEffects(pSource,
        m_res.GetResRef(),
        1);
    for (std::list<CGameEffect*>::iterator it = effects.begin(); it != effects.end(); it++) {
        (*it)->m_sourceRes = m_res;
        (*it)->m_target = m_target;
        (*it)->m_casterLevel = m_casterLevel;
        pProjectile->AddEffect(*it);
    }

    pProjectile->Fire(pSprite->GetArea(),
        pSource->GetId(),
        pSprite->GetId(),
        pSprite->GetPos(),
        0,
        0);

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceID,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    m_done = TRUE;

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4A1D20
IcewindCGameEffectEnergyDrain::IcewindCGameEffectEnergyDrain(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A1D60
CGameEffect* IcewindCGameEffectEnergyDrain::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectEnergyDrain* copy = new IcewindCGameEffectEnergyDrain(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x56A910
BOOL IcewindCGameEffectEnergyDrain::ApplyEffect(CGameSprite* pSprite)
{
    if (pSprite != NULL && pSprite->m_bInUnmarshal) {
        return FALSE;
    }

    if (m_secondaryType) {
        DisplayStringRef(pSprite, 16286);
    }

    // NOTE: Using `set` (with bounds check).
    pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_ENERGY_DRAINED, true);

    // NOTE: Uninline.
    AddPortraitIcon(pSprite, 123);

    pSprite->GetDerivedStats()->m_nTHAC0 -= static_cast<SHORT>(m_effectAmount);
    pSprite->GetDerivedStats()->m_nSaveVSFortitude -= static_cast<SHORT>(m_effectAmount);
    pSprite->GetDerivedStats()->m_nSaveVSReflex -= static_cast<SHORT>(m_effectAmount);
    pSprite->GetDerivedStats()->m_nSaveVSWill -= static_cast<SHORT>(m_effectAmount);
    pSprite->GetDerivedStats()->m_nMaxHitPoints += -5 * static_cast<SHORT>(m_effectAmount);

    if (m_secondaryType) {
        pSprite->GetBaseStats()->m_hitPoints += -5 * static_cast<SHORT>(m_effectAmount);
    }

    if (pSprite->GetBaseStats()->m_hitPoints <= 0 || pSprite->GetDerivedStats()->m_nMaxHitPoints <= 0) {
        CGameEffectDeath* pDeath = new CGameEffectDeath();
        pDeath->m_source = m_source;
        pDeath->m_sourceID = m_sourceID;
        pDeath->m_dwFlags = 0x4;
        pSprite->AddEffect(pDeath, CGameAIBase::EFFECT_LIST_TIMED, TRUE, TRUE);

        pSprite->GetDerivedStats()->m_nMaxHitPoints = 1;
        pSprite->GetBaseStats()->m_hitPoints = 0;
        m_done = TRUE;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4A1E10
IcewindCGameEffectTortoiseShell::IcewindCGameEffectTortoiseShell(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A1E50
CGameEffect* IcewindCGameEffectTortoiseShell::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectTortoiseShell* copy = new IcewindCGameEffectTortoiseShell(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x56AAE0
BOOL IcewindCGameEffectTortoiseShell::ApplyEffect(CGameSprite* pSprite)
{
    if (!pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_TORTOISE_SHELL]) {
        if (m_secondaryType) {
            DisplayStringRef(pSprite, 17063); // "Encased in Tortoise Shell"
        }

        // NOTE: Using `set` (with bounds check).
        pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_TORTOISE_SHELL, true);
        pSprite->GetDerivedStats()->m_visualEffects.set(IWD_VFX_TORTOISE_SHELL, true);

        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 125);

        pSprite->field_9D15 = 1;
        pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_HELD] = true;
        pSprite->GetDerivedStats()->m_generalState |= STATE_HELPLESS;
        pSprite->GetDerivedStats()->field_124 = m_effectAmount;
    } else {
        m_done = TRUE;
    }
    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4A1F00
IcewindCGameEffectBlink::IcewindCGameEffectBlink(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A1F40
CGameEffect* IcewindCGameEffectBlink::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectBlink* copy = new IcewindCGameEffectBlink(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x56ABB0
BOOL IcewindCGameEffectBlink::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_BLINK] = true;
    pSprite->GetDerivedStats()->m_visualEffects[IWD_VFX_INVISIBILITY] = true;

    switch (m_dwFlags) {
    case 0:
        AddPortraitIcon(pSprite, 130);
        break;
    case 1:
        AddPortraitIcon(pSprite, 145);
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\IcewindCGameEffects.cpp
        // __LINE__: 6894
        UTIL_ASSERT(FALSE);
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4A1FF0
IcewindCGameEffectPersistantEffect::IcewindCGameEffectPersistantEffect(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A2030
CGameEffect* IcewindCGameEffectPersistantEffect::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectPersistantEffect* copy = new IcewindCGameEffectPersistantEffect(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x56AC20
BOOL IcewindCGameEffectPersistantEffect::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType != 0) {
        m_special = g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime;
    }

    CPersistantEffect84C4C4* pEffect;
    // NOTE: The duration type is the WORD constant at 0x8491AA (1).
    if (m_durationType == 1) {
        pEffect = new CPersistantEffect84C4C4(m_res.GetResRefStr(),
            15 * m_effectAmount,
            m_special,
            0);
    } else {
        pEffect = new CPersistantEffect84C4C4(m_res.GetResRefStr(),
            15 * m_effectAmount,
            m_special,
            m_duration);
    }

    pEffect->m_sourceID = m_sourceID;
    strcpy(reinterpret_cast<char*>(pEffect->field_29), m_sourceRes.GetResRefStr());
    pSprite->GetDerivedStats()->m_cRegeneratedPersistantEffectList.AddTail(pEffect);

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4A20E0
IcewindCGameEffectDamageReduction::IcewindCGameEffectDamageReduction(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A2120
CGameEffect* IcewindCGameEffectDamageReduction::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectDamageReduction* copy = new IcewindCGameEffectDamageReduction(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x56ADC0
BOOL IcewindCGameEffectDamageReduction::ApplyEffect(CGameSprite* pSprite)
{
    LONG nAmount;
    if (m_effectAmount == 0) {
        if (m_dwFlags == 6) {
            m_dwFlags = 3;
        }
        nAmount = 5 * m_dwFlags;
    } else {
        // DEVIATION (ruling M3-8): the original takes the amount from its
        // stack argument slot, the sprite pointer (0x56ADF8). That heap
        // address absorbs everything in practice and changes from run to
        // run; a constant does the same deterministically.
        nAmount = 0x7FFFFFFF;
    }

    CDamageReduction reduction(nAmount, m_dwFlags);
    reduction.AddDamageType(0);
    reduction.AddDamageType(0x100000);
    reduction.AddDamageType(0x1000000);
    reduction.AddDamageType(0x2000000);
    reduction.AddDamageType(0x4000000);
    reduction.AddDamageType(0x8000000);
    pSprite->GetDerivedStats()->m_damageReductions.push_back(reduction);

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4A21D0
IcewindCGameEffectProtectionFromArrows::IcewindCGameEffectProtectionFromArrows(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A2210
CGameEffect* IcewindCGameEffectProtectionFromArrows::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectProtectionFromArrows* copy = new IcewindCGameEffectProtectionFromArrows(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x56AF80
BOOL IcewindCGameEffectProtectionFromArrows::ApplyEffect(CGameSprite* pSprite)
{
    CDamageReduction reduction(10, m_dwFlags);
    reduction.AddDamageType(0x4000000);
    reduction.AddDamageType(0x2000000);
    pSprite->GetDerivedStats()->m_damageReductions.push_back(reduction);

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4A22C0
IcewindCGameEffectTensersTransformation::IcewindCGameEffectTensersTransformation(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A2300
CGameEffect* IcewindCGameEffectTensersTransformation::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectTensersTransformation* copy = new IcewindCGameEffectTensersTransformation(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x56B0F0
BOOL IcewindCGameEffectTensersTransformation::ApplyEffect(CGameSprite* pSprite)
{
    if (!pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_TENSERS_TRANSFORMATION]) {
        pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_TENSERS_TRANSFORMATION, true);

        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 55);

        pSprite->SetColorRange(62);

        if (m_secondaryType) {
            m_effectAmount2 = IcewindMisc::Roll(m_casterLevel, 6);
            m_effectAmount3 = IcewindMisc::Roll(2, 4);
            m_effectAmount4 = IcewindMisc::Roll(2, 4);
        }

        pSprite->GetDerivedStats()->m_nMaxHitPoints += static_cast<SHORT>(m_effectAmount2);
        if (m_secondaryType) {
            pSprite->GetBaseStats()->m_hitPoints += static_cast<SHORT>(m_effectAmount2);
        }
        pSprite->GetDerivedStats()->m_nSTR += static_cast<SHORT>(m_effectAmount3);
        pSprite->GetDerivedStats()->m_nDEX += static_cast<SHORT>(m_effectAmount4);
        pSprite->GetDerivedStats()->m_nTHAC0 += static_cast<BYTE>(m_casterLevel) / 2;
        pSprite->GetDerivedStats()->m_nSaveVSFortitude += 5;
        pSprite->GetDerivedStats()->field_C += 4;
        pSprite->GetDerivedStats()->m_disabledSpellTypes[0] = TRUE;
        pSprite->GetDerivedStats()->m_disabledSpellTypes[1] = TRUE;
    }
    return TRUE;
}

// 0x56B6E0
void IcewindCGameEffectTensersTransformation::OnRemove(CGameSprite* pSprite)
{
    PlaySound(CResRef("EFF_E01"), pSprite);
}

// -----------------------------------------------------------------------------

// 0x4A23B0
IcewindCGameEffectSmiteEvil::IcewindCGameEffectSmiteEvil(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A23F0
CGameEffect* IcewindCGameEffectSmiteEvil::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectSmiteEvil* copy = new IcewindCGameEffectSmiteEvil(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x56B220
BOOL IcewindCGameEffectSmiteEvil::ApplyEffect(CGameSprite* pSprite)
{
    pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_SMITE_EVIL] = true;
    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4A24A0
IcewindCGameEffectRestoration::IcewindCGameEffectRestoration(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A24E0
CGameEffect* IcewindCGameEffectRestoration::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectRestoration* copy = new IcewindCGameEffectRestoration(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x56B240
BOOL IcewindCGameEffectRestoration::ApplyEffect(CGameSprite* pSprite)
{
    if ((pSprite->GetDerivedStats()->m_generalState & STATE_DISEASED) != 0) {
        RemoveAllOfType(pSprite, CGAMEEFFECT_DISEASE, 13);
        RemoveAllOfType(pSprite, CGAMEEFFECT_DISEASE, 14);
        RemoveAllOfType(pSprite, CGAMEEFFECT_DISEASE, 15);
        RemoveAllOfType(pSprite, CGAMEEFFECT_DISEASE, 4);
        RemoveAllOfType(pSprite, CGAMEEFFECT_DISEASE, 5);
        RemoveAllOfType(pSprite, CGAMEEFFECT_DISEASE, 6);
        RemoveAllOfType(pSprite, CGAMEEFFECT_DISEASE, 7);
        RemoveAllOfType(pSprite, CGAMEEFFECT_DISEASE, 8);
        RemoveAllOfType(pSprite, CGAMEEFFECT_DISEASE, 9);
    }

    // __FILE__: C:\Projects\Icewind2\src\Baldur\IcewindCGameEffects.cpp
    // __LINE__: 7075
    UTIL_ASSERT(pSprite != NULL);

    POSITION pos = pSprite->GetTimedEffectList()->GetHeadPosition();
    while (pos != NULL) {
        CGameEffect* pEffect = pSprite->GetTimedEffectList()->GetNext(pos);
        if (sub_56B340(pEffect, pSprite) == TRUE) {
            pSprite->GetTimedEffectList()->sub_4C0830(pEffect);
        }
    }

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    pGame->UpdatePortrait(pGame->GetCharacterPortraitNum(pSprite->GetId()), 1);

    m_done = TRUE;

    return TRUE;
}

// 0x56B340
BOOLEAN IcewindCGameEffectRestoration::sub_56B340(CGameEffect* pEffect, CGameSprite* pSprite)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\IcewindCGameEffects.cpp
    // __LINE__: 7096
    UTIL_ASSERT(pEffect != NULL);

    if (pEffect->m_effectID != CGAMEEFFECT_STR
        && pEffect->m_effectID != CGAMEEFFECT_DEX
        && pEffect->m_effectID != CGAMEEFFECT_CON
        && pEffect->m_effectID != CGAMEEFFECT_INT
        && pEffect->m_effectID != CGAMEEFFECT_WIS
        && pEffect->m_effectID != CGAMEEFFECT_CHR) {
        return FALSE;
    }

    LONG nStat;
    switch (pEffect->m_dwFlags) {
    case 0:
        if (pEffect->m_effectAmount < 0) {
            return TRUE;
        }
        // FALLTHROUGH
    case 1:
        switch (pEffect->m_effectID) {
        case CGAMEEFFECT_STR:
            nStat = pSprite->m_tempStats.m_nSTR;
            break;
        case CGAMEEFFECT_DEX:
            nStat = pSprite->m_tempStats.m_nDEX;
            break;
        case CGAMEEFFECT_CON:
            nStat = pSprite->m_tempStats.m_nCON;
            break;
        case CGAMEEFFECT_INT:
            nStat = pSprite->m_tempStats.m_nINT;
            break;
        case CGAMEEFFECT_WIS:
            nStat = pSprite->m_tempStats.m_nWIS;
            break;
        case CGAMEEFFECT_CHR:
            nStat = pSprite->m_tempStats.m_nCHR;
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\IcewindCGameEffects.cpp
            // __LINE__: 7123
            UTIL_ASSERT(FALSE);

            // NOTE: Unreachable (the opcode was checked above). The original
            // compares with its stale argument, the sprite pointer.
            nStat = 0;
            break;
        }

        if (pEffect->m_effectAmount < nStat) {
            return TRUE;
        }
        // FALLTHROUGH
    case 2:
        if (pEffect->m_effectAmount < 100) {
            return TRUE;
        }
        break;
    }

    // __FILE__: C:\Projects\Icewind2\src\Baldur\IcewindCGameEffects.cpp
    // __LINE__: 7133
    UTIL_ASSERT(FALSE);

    return FALSE;
}

// -----------------------------------------------------------------------------

// 0x4A2590
IcewindCGameEffectAlicornLanceGlow::IcewindCGameEffectAlicornLanceGlow(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A25D0
CGameEffect* IcewindCGameEffectAlicornLanceGlow::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectAlicornLanceGlow* copy = new IcewindCGameEffectAlicornLanceGlow(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x56B4B0
BOOL IcewindCGameEffectAlicornLanceGlow::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType) {
        DisplayStringRef(pSprite, 21646);
    }

    if (!pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_ALICORN_LANCE]) {
        pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_ALICORN_LANCE] = true;
        pSprite->GetDerivedStats()->field_C -= 2;
        AddColorEffect(pSprite, 185, 185, 185);
        m_done = FALSE;
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4A2680
IcewindCGameEffectGlobeOfInvulnerability::IcewindCGameEffectGlobeOfInvulnerability(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A26C0
CGameEffect* IcewindCGameEffectGlobeOfInvulnerability::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectGlobeOfInvulnerability* copy = new IcewindCGameEffectGlobeOfInvulnerability(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x56B520
BOOL IcewindCGameEffectGlobeOfInvulnerability::ApplyEffect(CGameSprite* pSprite)
{
    switch (m_dwFlags) {
    case 0:
        if (m_secondaryType) {
            DisplayStringRef(pSprite, 14771);
        }

        pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_MINOR_GLOBE_OF_INVULNERABILITY, true);

        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 96);

        pSprite->GetDerivedStats()->m_visualEffects.set(IWD_VFX_MINOR_GLOBE_OF_INVULNERABILITY);
        pSprite->GetDerivedStats()->field_1E0 = 1;
        pSprite->GetDerivedStats()->field_1E4 = 1;
        pSprite->GetDerivedStats()->field_1E8 = 1;
        break;
    case 1:
        if (m_secondaryType) {
            DisplayStringRef(pSprite, 14771);
        }

        pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_GLOBE_OF_INVULNERABILITY, true);

        // NOTE: Uninline.
        AddPortraitIcon(pSprite, 97);

        pSprite->GetDerivedStats()->m_visualEffects.set(IWD_VFX_GLOBE_OF_INVULNERABILITY);
        pSprite->GetDerivedStats()->field_1E0 = 1;
        pSprite->GetDerivedStats()->field_1E4 = 1;
        pSprite->GetDerivedStats()->field_1E8 = 1;
        pSprite->GetDerivedStats()->field_1EC = 1;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\IcewindCGameEffects.cpp
        // __LINE__: 7204
        UTIL_ASSERT(FALSE);
    }
    return TRUE;
}

// 0x56B6E0
void IcewindCGameEffectGlobeOfInvulnerability::OnRemove(CGameSprite* pSprite)
{
    PlaySound(CResRef("EFF_E01"), pSprite);
}

// -----------------------------------------------------------------------------

// 0x4A2770
IcewindCGameEffectLowerResistance::IcewindCGameEffectLowerResistance(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A27B0
CGameEffect* IcewindCGameEffectLowerResistance::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectLowerResistance* copy = new IcewindCGameEffectLowerResistance(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x56B680
BOOL IcewindCGameEffectLowerResistance::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType) {
        DisplayStringRef(pSprite, 16967); // "Spell Resistance Lowered"
    }

    if (!pSprite->GetDerivedStats()->m_spellStates[CGAMEEFFECT_CASTINGGLOW]) {
        pSprite->GetDerivedStats()->m_spellStates[CGAMEEFFECT_CASTINGGLOW] = true;
        AdjustResistMagic(pSprite, -static_cast<BYTE>(m_casterLevel) * 2);
    }

    return TRUE;
}

// 0x56B6E0
void IcewindCGameEffectLowerResistance::OnRemove(CGameSprite* pSprite)
{
    PlaySound(CResRef("EFF_E01"), pSprite);
}

// -----------------------------------------------------------------------------

// 0x4A2860
IcewindCGameEffectBane::IcewindCGameEffectBane(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A28A0
CGameEffect* IcewindCGameEffectBane::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectBane* copy = new IcewindCGameEffectBane(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x56B710
BOOL IcewindCGameEffectBane::ApplyEffect(CGameSprite* pSprite)
{
    if (m_secondaryType) {
        DisplayStringRef(pSprite, 14406); // "Afflicted by Bane"
        RemoveAllOfType(pSprite, CGAMEEFFECT_NON_CUMULATIVEAID, -1);
    }

    if (!pSprite->GetDerivedStats()->m_spellStates[CGAMEEFFECT_DURATIONCASTING]) {
        AddPortraitIcon(pSprite, 35);
        AddColorEffect(pSprite, 0, 0, 128, 30);
        SetSpellState(pSprite, CGAMEEFFECT_DURATIONCASTING);
        AdjustTHAC0(pSprite, -1);
        pSprite->GetBaseStats()->m_morale--;
    }

    return TRUE;
}

// 0x4BAF90
void IcewindCGameEffectBane::OnRemove(CGameSprite* pSprite)
{
    PlaySound(CResRef("EFF_E04"), pSprite);
}

// -----------------------------------------------------------------------------

// 0x4A2950
IcewindCGameEffectFeatPowerAttack::IcewindCGameEffectFeatPowerAttack(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A2990
CGameEffect* IcewindCGameEffectFeatPowerAttack::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectFeatPowerAttack* copy = new IcewindCGameEffectFeatPowerAttack(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x56B790
BOOL IcewindCGameEffectFeatPowerAttack::ApplyEffect(CGameSprite* pSprite)
{
    if (!pSprite->sub_763150(CGAMESPRITE_FEAT_POWER_ATTACK)) {
        return FALSE;
    }

    INT nRank = pSprite->sub_726270(CGAMESPRITE_FEAT_POWER_ATTACK);

    // NOTE: Using `set` (with bounds check).
    pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_FEAT_POWER_ATTACK_1, nRank == 1);
    pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_FEAT_POWER_ATTACK_2, nRank == 2);
    pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_FEAT_POWER_ATTACK_3, nRank == 3);
    pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_FEAT_POWER_ATTACK_4, nRank == 4);
    pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_FEAT_POWER_ATTACK_5, nRank == 5);

    if (nRank != 0
        && (pSprite->GetDerivedStats()->m_spellStates.test(SPLSTATE_FEAT_POWER_ATTACK_1)
            || pSprite->GetDerivedStats()->m_spellStates.test(SPLSTATE_FEAT_POWER_ATTACK_2)
            || pSprite->GetDerivedStats()->m_spellStates.test(SPLSTATE_FEAT_POWER_ATTACK_3)
            || pSprite->GetDerivedStats()->m_spellStates.test(SPLSTATE_FEAT_POWER_ATTACK_4)
            || pSprite->GetDerivedStats()->m_spellStates.test(SPLSTATE_FEAT_POWER_ATTACK_5))) {
        if (m_secondaryType) {
            // NOTE: Odd mixing of `test` (with bounds check) and `operator[]`.
            if (pSprite->GetDerivedStats()->m_spellStates.test(SPLSTATE_FEAT_EXPERTISE_1)
                || pSprite->GetDerivedStats()->m_spellStates.test(SPLSTATE_FEAT_EXPERTISE_2)
                || pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_FEAT_EXPERTISE_3]
                || pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_FEAT_EXPERTISE_4]
                || pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_FEAT_EXPERTISE_5]) {
                pSprite->FeedBack(CGameSprite::FEEDBACK_TOGGLEFEAT,
                    1,
                    0,
                    0,
                    35785, // // "Expertise"
                    0,
                    0);
            }
            pSprite->FeedBack(CGameSprite::FEEDBACK_TOGGLEFEAT,
                0,
                0,
                0,
                35794, // "Power Attack"
                0,
                0);
        }

        // NOTE: Odd mixing of `set` (with bounds check) and `operator[]`.
        pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_FEAT_EXPERTISE_1, false);
        pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_FEAT_EXPERTISE_2] = false;
        pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_FEAT_EXPERTISE_3] = false;
        pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_FEAT_EXPERTISE_4] = false;
        pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_FEAT_EXPERTISE_5] = false;
    } else {
        m_done = TRUE;
        if (m_secondaryType) {
            pSprite->FeedBack(CGameSprite::FEEDBACK_TOGGLEFEAT,
                1,
                nRank,
                0,
                35794, // "Power Attack"
                0,
                0);
        }
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4A2A40
IcewindCGameEffectFeatExpertise::IcewindCGameEffectFeatExpertise(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A2A80
CGameEffect* IcewindCGameEffectFeatExpertise::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectFeatExpertise* copy = new IcewindCGameEffectFeatExpertise(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x56B960
BOOL IcewindCGameEffectFeatExpertise::ApplyEffect(CGameSprite* pSprite)
{
    if (!pSprite->sub_763150(CGAMESPRITE_FEAT_EXPERTISE)) {
        return FALSE;
    }

    INT nRank = pSprite->sub_726270(CGAMESPRITE_FEAT_EXPERTISE);

    // NOTE: Using `set` (with bounds check).
    pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_FEAT_EXPERTISE_1, nRank == 1);
    pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_FEAT_EXPERTISE_2, nRank == 2);
    pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_FEAT_EXPERTISE_3, nRank == 3);
    pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_FEAT_EXPERTISE_4, nRank == 4);
    pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_FEAT_EXPERTISE_5, nRank == 5);

    if (nRank != 0
        && (pSprite->GetDerivedStats()->m_spellStates.test(SPLSTATE_FEAT_EXPERTISE_1)
            || pSprite->GetDerivedStats()->m_spellStates.test(SPLSTATE_FEAT_EXPERTISE_2)
            || pSprite->GetDerivedStats()->m_spellStates.test(SPLSTATE_FEAT_EXPERTISE_3)
            || pSprite->GetDerivedStats()->m_spellStates.test(SPLSTATE_FEAT_EXPERTISE_4)
            || pSprite->GetDerivedStats()->m_spellStates.test(SPLSTATE_FEAT_EXPERTISE_5))) {
        if (m_secondaryType) {
            // NOTE: Odd mixing of `test` (with bounds check) and `operator[]`.
            if (pSprite->GetDerivedStats()->m_spellStates.test(SPLSTATE_FEAT_POWER_ATTACK_1)
                || pSprite->GetDerivedStats()->m_spellStates.test(SPLSTATE_FEAT_POWER_ATTACK_2)
                || pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_FEAT_POWER_ATTACK_3]
                || pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_FEAT_POWER_ATTACK_4]
                || pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_FEAT_POWER_ATTACK_5]) {
                pSprite->FeedBack(CGameSprite::FEEDBACK_TOGGLEFEAT,
                    1,
                    0,
                    0,
                    35794, // "Power Attack"
                    0,
                    0);
            }
            pSprite->FeedBack(CGameSprite::FEEDBACK_TOGGLEFEAT,
                0,
                0,
                0,
                35785, // "Expertise"
                0,
                0);
        }

        // NOTE: Odd mixing of `set` (with bounds check) and `operator[]`.
        pSprite->GetDerivedStats()->m_spellStates.set(SPLSTATE_FEAT_POWER_ATTACK_1, false);
        pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_FEAT_POWER_ATTACK_2] = false;
        pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_FEAT_POWER_ATTACK_3] = false;
        pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_FEAT_POWER_ATTACK_4] = false;
        pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_FEAT_POWER_ATTACK_5] = false;
    } else {
        m_done = TRUE;
        if (m_secondaryType) {
            pSprite->FeedBack(CGameSprite::FEEDBACK_TOGGLEFEAT,
                1,
                nRank,
                0,
                35785, // "Expertise"
                0,
                0);
        }
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4A2B30
IcewindCGameEffectFeatArterialStrike::IcewindCGameEffectFeatArterialStrike(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A2B70
CGameEffect* IcewindCGameEffectFeatArterialStrike::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectFeatArterialStrike* copy = new IcewindCGameEffectFeatArterialStrike(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x56BB30
BOOL IcewindCGameEffectFeatArterialStrike::ApplyEffect(CGameSprite* pSprite)
{
    if (!pSprite->sub_763150(CGAMESPRITE_FEAT_ARTERIAL_STRIKE)) {
        return FALSE;
    }

    INT nRank = pSprite->sub_726270(CGAMESPRITE_FEAT_ARTERIAL_STRIKE);
    if (nRank > 0) {
        pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_FEAT_ARTERIAL_STRIKE] = true;
        if (m_secondaryType != 0) {
            if (pSprite->GetDerivedStats()->m_spellStates[CGAMESPRITE_FEAT_HAMSTRING]) {
                pSprite->FeedBack(CGameSprite::FEEDBACK_TOGGLEFEAT,
                    1,
                    0,
                    0,
                    35787, // "Hamstring"
                    0,
                    0);
            }
            pSprite->FeedBack(CGameSprite::FEEDBACK_TOGGLEFEAT,
                0,
                0,
                0,
                35774, // "Arterial Strike"
                0,
                0);
        }
        pSprite->GetDerivedStats()->m_spellStates[CGAMESPRITE_FEAT_HAMSTRING] = false;
    } else {
        pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_FEAT_ARTERIAL_STRIKE] = false;
        m_done = TRUE;
        if (m_secondaryType != 0) {
            pSprite->FeedBack(CGameSprite::FEEDBACK_TOGGLEFEAT,
                1,
                0,
                0,
                35774, // "Arterial Strike"
                0,
                0);
        }
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4A2C20
IcewindCGameEffectFeatHamstring::IcewindCGameEffectFeatHamstring(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A2C60
CGameEffect* IcewindCGameEffectFeatHamstring::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    IcewindCGameEffectFeatHamstring* copy = new IcewindCGameEffectFeatHamstring(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x56BC20
BOOL IcewindCGameEffectFeatHamstring::ApplyEffect(CGameSprite* pSprite)
{
    if (!pSprite->sub_763150(CGAMESPRITE_FEAT_HAMSTRING)) {
        return FALSE;
    }

    INT nRank = pSprite->sub_726270(CGAMESPRITE_FEAT_HAMSTRING);
    if (nRank > 0) {
        pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_FEAT_HAMSTRING] = true;
        if (m_secondaryType != 0) {
            if (pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_FEAT_ARTERIAL_STRIKE]) {
                pSprite->FeedBack(CGameSprite::FEEDBACK_TOGGLEFEAT,
                    1,
                    0,
                    0,
                    35774, // "Arterial Strike"
                    0,
                    0);
            }
            pSprite->FeedBack(CGameSprite::FEEDBACK_TOGGLEFEAT,
                0,
                0,
                0,
                35787, // "Hamstring"
                0,
                0);
        }
        pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_FEAT_ARTERIAL_STRIKE] = false;
    } else {
        pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_FEAT_HAMSTRING] = false;
        m_done = TRUE;
        if (m_secondaryType != 0) {
            pSprite->FeedBack(CGameSprite::FEEDBACK_TOGGLEFEAT,
                1,
                0,
                0,
                35787, // "Hamstring"
                0,
                0);
        }
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x4A2D10
IcewindCGameEffectFeatRapidShot::IcewindCGameEffectFeatRapidShot(ITEM_EFFECT* effect, const CPoint& source, LONG sourceID, CPoint target)
    : CGameEffect(effect, source, sourceID, target, FALSE)
{
}

// 0x4A2D50
CGameEffect* IcewindCGameEffectFeatRapidShot::Copy()
{
    ITEM_EFFECT* effect = GetItemEffect();
    CGameEffectStoneSkins* copy = new CGameEffectStoneSkins(effect, m_source, m_sourceID, m_target);
    delete effect;
    copy->CopyFromBase(this);
    return copy;
}

// 0x56BD10
BOOL IcewindCGameEffectFeatRapidShot::ApplyEffect(CGameSprite* pSprite)
{
    if (!pSprite->sub_763150(CGAMESPRITE_FEAT_RAPID_SHOT)) {
        return FALSE;
    }

    INT nRank = pSprite->sub_726270(CGAMESPRITE_FEAT_RAPID_SHOT);
    if (nRank > 0) {
        pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_FEAT_RAPID_SHOT] = true;
        if (m_secondaryType != 0) {
            pSprite->FeedBack(CGameSprite::FEEDBACK_TOGGLEFEAT,
                0,
                0,
                0,
                35796, // "Rapid Shot"
                0,
                0);
        }
    } else {
        pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_FEAT_RAPID_SHOT] = false;
        m_done = TRUE;
        if (m_secondaryType != 0) {
            pSprite->FeedBack(CGameSprite::FEEDBACK_TOGGLEFEAT,
                1,
                0,
                0,
                35796, // "Rapid Shot"
                0,
                0);
        }
    }

    return TRUE;
}

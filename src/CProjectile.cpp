#include "CProjectile.h"

#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameChunk.h"
#include "CGameContainer.h"
#include "CGameDoor.h"
#include "CGameEffect.h"
#include "CGameSprite.h"
#include "CGameTrigger.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CProjectileArea.h"
#include "CProjectileBAM.h"
#include "CProjectileSingleTarget.h"
#include "CTimerWorld.h"
#include "CUtil.h"
#include "IcewindCVisualEffect.h"
#include "IcewindMisc.h"

#include <list>
#include <math.h>

// 0x529490
CProjectile::CProjectile()
{
    m_nSpellLevel = 0;
    m_projectileType = 0;
    m_speed = 0;
    m_sourceId = 0;
    m_targetId = 0;
    m_nSparkleColour = 0;
    m_driftDecay = 0;
    field_E6 = 0;
    m_pArea = NULL;
    m_nStartHeight = 0;
    m_nCurrentHeight = 0;
    m_nStartDistance = 0;
    field_17C = 0;
    m_callBackProjectile = CGameObjectArray::INVALID_INDEX;
    m_bHasHeight = FALSE;
    m_bTrackTargetHeight = FALSE;
    m_bSparkle = FALSE;
    m_fireSoundRef = "";
    m_loopFireSound = FALSE;
    m_arrivalSoundRef = "";
    m_loopArrivalSound = FALSE;
    field_182 = CGameObjectArray::INVALID_INDEX;
}

// 0x51E9B0
CProjectile::~CProjectile()
{
    POSITION pos = m_effectList.GetHeadPosition();
    while (pos != NULL) {
        CGameEffect* node = m_effectList.GetNext(pos);
        if (node != NULL) {
            delete node;
        }
    }
    m_effectList.RemoveAll();
}

// 0x6A3130
BOOLEAN CProjectile::IsProjectile()
{
    return TRUE;
}

// 0x5551B0
void CProjectile::RemoveSelf()
{
    RemoveFromArea();

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Delete(m_id,
        CGameObjectArray::THREAD_ASYNCH,
        NULL,
        INFINITE);
    if (rc == CGameObjectArray::SUCCESS) {
        delete this;
    }
}

// 0x51EA90
void CProjectile::AddEffect(CGameEffect* pEffect)
{
    pEffect->m_projectileType = m_projectileType;
    m_effectList.AddTail(pEffect);
}

// 0x51EAB0
void CProjectile::AddEffect2(CGameEffect* pEffect)
{
    // The original adds only when no session is up, or when this machine
    // hosts it (chitin +0x1032/+0x1033: cNetwork.m_bConnectionEstablished
    // and m_bIsHost).
    if (g_pChitin->cNetwork.m_bConnectionEstablished == FALSE
        || (g_pChitin->cNetwork.m_bConnectionEstablished == TRUE
            && g_pChitin->cNetwork.m_bIsHost == TRUE)) {
        pEffect->m_projectileType = m_projectileType;
        m_effectList.AddTail(pEffect);
    }
}

// 0x51EAF0
CProjectile* CProjectile::DecodeProjectile(WORD nProjectileType, CGameAIBase* pSprite, BYTE nUnknown)
{
    CProjectile* pProjectile = NULL;
    CProjectileArea* pArea;
    CString sResRef;
    CString sTemp1;
    CString sTemp2;
    IcewindCVisualEffect visualEffect;

    if (nProjectileType > 0x1000) {
        pProjectile = DecodeSpellHitProjectile(nProjectileType - 0x1001, NULL, FALSE);
        if (pProjectile == NULL) {
            // NOTE: The original writes the type through the NULL that
            // DecodeSpellHitProjectile returns for a type it does not know.
            // Bounded (ruling P4-10): the instant projectile takes its place.
            pProjectile = new CProjectile84D8B8();
        }
        pProjectile->m_projectileType = nProjectileType - 1;
        return pProjectile;
    }

    switch (nProjectileType) {
    case 1:
        pProjectile = new CProjectile84D8B8();
        break;
    case 68:
    case 69:
    case 70:
    case 71:
    case 72:
        // Magic Missile, 1 to 5 missiles. NOTE: The WORD at 0x85E84A is 1.
        pProjectile = new CProjectile84E0C4(nProjectileType - 67, 1);
        pProjectile->m_fireSoundRef = "TRA_22";
        pProjectile->m_arrivalSoundRef = "";
        pProjectile->m_bHasHeight = TRUE;
        break;
    case 38:
        // Fireball.
        pProjectile = new CProjectileArea84F580();
        break;
    case 42:
        // Sleep: those not evil, or not those, by the caster's side.
        pArea = new CProjectileArea84FEA4();
        if (pSprite != NULL) {
            CAIObjectType type(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
            if (pSprite->GetAIType().m_nEnemyAlly <= CAIObjectType::EA_NOTEVIL) {
                type.m_nEnemyAlly = CAIObjectType::EA_EVILCUTOFF;
            } else {
                type.m_nEnemyAlly = CAIObjectType::EA_NOTEVIL;
            }
            pArea->m_targetType.Set(type);
        }
        pArea->m_fireSoundRef = "TRA_04C";
        pArea->m_arrivalSoundRef = "EFF_M05";
        pProjectile = pArea;
        break;
    case 64:
        // Web.
        pProjectile = new CProjectileArea8502E8();
        break;
    case 67:
        // Flame Strike.
        pProjectile = new CProjectileArea84F754();
        break;
    case 79:
        pProjectile = new CProjectile84DF1C();
        break;
    case 92:
        // Fire Storm, Lightning Bolt.
        pProjectile = new CProjectileArea84F6B8();
        break;
    case 95:
        // Stinking Cloud.
        pProjectile = new CProjectileArea8501B0();
        break;
    case 101:
        // Grease.
        pProjectile = new CProjectileArea84F7F0();
        break;
    case 187:
        // Cloudkill.
        pProjectile = new CProjectileArea84F310();
        break;
    case 190:
        // Hold Person.
        pArea = new CProjectileArea84F9C4();
        pArea->TargetEnemy(pSprite);
        pArea->m_fireSoundRef = CResRef("TRA_04C");
        pArea->m_arrivalSoundRef = CResRef("EFF_M05");
        pProjectile = pArea;
        break;
    case 212:
        // Acid Fog.
        pProjectile = new CProjectileArea84F3AC();
        break;
    case 213:
        // Spike Stones.
        pProjectile = new CProjectileArea84FFDC();
        break;
    case 218:
        // One magic missile. NOTE: The WORD at 0x85E848 is 0.
        pProjectile = new CProjectile8510A4(0);
        pProjectile->m_bHasHeight = TRUE;
        pProjectile->m_fireSoundRef = "TRA_02";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 235:
        // Entangle.
        pProjectile = new CProjectileArea84F4E4();
        break;
    case 236:
        // Bless.
        pArea = new CProjectileArea(250);
        pArea->TargetFriend(pSprite);
        pArea->m_explosion1.m_animation = "Area1X";
        pArea->m_explosion1.m_sound = "EFF_P99";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE);
        pProjectile = pArea;
        break;
    case 237:
        // Bane.
        pArea = new CProjectileArea(250);
        pArea->TargetEnemy(pSprite);
        pArea->m_explosion1.m_animation = "Area1X";
        pArea->m_explosion1.m_sound = "EFF_P99";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE);
        pProjectile = pArea;
        break;
    case 238:
        // Remove Fear.
        pArea = new CProjectileArea(300);
        pArea->TargetFriend(pSprite);
        pArea->m_explosion1.m_animation = "AbjuraX";
        pArea->m_explosion1.m_sound = "ARE_M20";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE);
        pProjectile = pArea;
        break;
    case 240:
        // See Invisibility, Invisibility Purge.
        pArea = new CProjectileArea(300);
        pArea->TargetEnemy(pSprite);
        pArea->m_explosion1.m_animation = "Area2X";
        pArea->m_explosion1.m_sound = "EFF_M99";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE);
        pProjectile = pArea;
        break;
    case 241:
        // Horror.
        pArea = new CProjectileArea(150);
        pArea->TargetEnemy(pSprite);
        pArea->m_explosion1.m_animation = "Area4X";
        pArea->m_explosion1.m_sound = "EFF_M99";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE);
        pProjectile = pArea;
        break;
    case 244:
        // Find Traps.
        pArea = new CProjectileArea(250);
        pArea->m_bCheckForNonSprites = TRUE;
        pProjectile = pArea;
        break;
    case 245:
        // Silence.
        pArea = new CProjectileArea(150);
        pArea->m_explosion1.m_animation = "AlteraX";
        pArea->m_explosion1.m_sound = "ARE_M19";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE);
        pProjectile = pArea;
        break;
    case 246:
        // Dispel Magic.
        pProjectile = new CProjectileArea850C0C();
        break;
    case 250:
        // Remove Paralysis.
        pArea = new CProjectileArea(200);
        pArea->TargetFriend(pSprite);
        pArea->m_explosion1.m_animation = "AbjuraX";
        pArea->m_explosion1.m_sound = "ARE_M20";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE);
        pProjectile = pArea;
        break;
    case 253:
        // Prayer.
        pArea = new CProjectileArea(600);
        pArea->m_explosion1.m_animation = "Area1X";
        pArea->m_explosion1.m_sound = "EFF_P99";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE);
        pProjectile = pArea;
        break;
    case 255:
        // Emotion.
        pArea = new CProjectileArea(150);
        pArea->m_explosion1.m_animation = "EnchanX";
        pArea->m_explosion1.m_sound = "ARE_M21";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE);
        pProjectile = pArea;
        break;
    case 258:
        // Magic Circle Against Evil, Holy Aura.
        pArea = new CProjectileArea(200);
        pArea->TargetFriend(pSprite);
        pArea->m_explosion1.m_animation = "AbjuraX";
        pArea->m_explosion1.m_sound = "ARE_M20";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE);
        pProjectile = pArea;
        break;
    case 260:
        // Recitation.
        pArea = new CProjectileArea(600);
        pArea->m_explosion1.m_animation = "Area1X";
        pArea->m_explosion1.m_sound = "EFF_P99";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE);
        pProjectile = pArea;
        break;
    case 266:
        // Righteous Wrath of the Faithful.
        pArea = new CProjectileArea(350);
        pArea->m_explosion1.m_animation = "AbjuraX";
        pArea->m_explosion1.m_sound = "ARE_M20";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE);
        pArea->TargetFriend(pSprite);
        pProjectile = pArea;
        break;
    case 275:
        pArea = new CProjectileArea(200);
        pArea->TargetEnemy(pSprite);
        pArea->m_explosion1.m_animation = "EnchanX";
        pArea->m_explosion1.m_sound = "ARE_M21";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE);
        pProjectile = pArea;
        break;
    case 270:
        // Fire Seeds, Flaming Oil.
        pProjectile = new CProjectileArea84F61C();
        break;
    case 276:
        // Symbol of Pain, Symbol of Stunning.
        pArea = new CProjectileArea(300);
        pArea->TargetEnemy(pSprite);
        pArea->m_explosion1.m_animation = "SoPainX";
        pArea->m_explosion1.m_sound = "EFF_P49";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE);
        pProjectile = pArea;
        break;
    case 277:
        // Symbol of Hopelessness, Symbol of Fear.
        pArea = new CProjectileArea(300);
        pArea->TargetEnemy(pSprite);
        pArea->m_explosion1.m_animation = "SoHopeX";
        pArea->m_explosion1.m_sound = "EFF_P50";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE);
        pProjectile = pArea;
        break;
    case 284:
        // SPIN991: the same cloud, after a floating line over the caster.
        IcewindMisc::sub_584880(static_cast<CGameSprite*>(pSprite), 4387, 0);
        pProjectile = new CProjectileArea84F310();
        break;
    case 287:
        // An incendiary cloud.
        pArea = new CProjectileArea(150);
        pArea->m_explosion3.m_animation = "ICloudA";
        pArea->m_explosion3.m_visualEffect.sub_586A90(TRUE);
        pArea->m_explosion3.field_35 = TRUE;
        pArea->m_explosion3.field_34 = TRUE;
        pArea->m_explosion3.field_38 = 500;
        pArea->m_nDuration = 200;
        pProjectile = pArea;
        break;
    case 299:
        // Soul Eater.
        pArea = new CProjectileArea(120);
        pArea->m_explosion3.m_animation = "SEaterA";
        pArea->m_explosion3.m_visualEffect.sub_586A90(TRUE);
        pArea->m_explosion3.field_35 = TRUE;
        pArea->m_explosion3.field_34 = TRUE;
        pArea->m_explosion3.m_sound = "ARE_M18";
        pArea->m_explosion3.field_38 = 200;
        pProjectile = pArea;
        break;
    case 300:
        // Spike Growth.
        pArea = new CProjectileArea(150);
        pArea->m_explosion1.m_animation = "SGrowtX";
        pArea->m_explosion1.m_sound = "EFF_P111";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE);
        pArea->m_explosion3.m_animation = "SGrowtA";
        pArea->m_explosion3.m_visualEffect.sub_586A90(TRUE);
        pArea->m_explosion3.field_35 = TRUE;
        pArea->m_explosion3.field_34 = TRUE;
        pArea->m_explosion3.m_sound = "AFT_P24";
        pArea->m_explosion3.field_38 = 250;
        pArea->m_nTriggerInterval = 10;
        pArea->m_nTriggerCountdown = 0;
        pArea->m_nHitModulus = 10;
        pArea->m_nDuration = 2000;
        pProjectile = pArea;
        break;
    case 337:
        // Holy Smite.
        pArea = new CProjectileArea(200);
        pArea->m_explosion3.m_animation = "HSmiteA";
        pArea->m_explosion3.m_visualEffect.sub_586AC0(TRUE, 0x40);
        pArea->m_explosion3.field_34 = TRUE;
        pArea->m_explosion3.field_35 = TRUE;
        pArea->m_explosion3.field_38 = 300;
        pArea->m_nTriggerInterval = 10000;
        pArea->m_nTriggerCountdown = 0;
        pArea->m_nHitModulus = 10;
        pArea->m_nDuration = 45;
        pProjectile = pArea;
        break;
    case 341:
        // Holy Word, Blasphemy.
        pArea = new CProjectileArea(300);
        pArea->m_explosion1.m_animation = "SoHopeX";
        pArea->m_explosion1.m_sound = "EFF_P50";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE);
        pProjectile = pArea;
        break;
    case 349:
        // A breath.
        pProjectile = new CProjectileArea85072C();
        break;
    case 359:
        // Healing Circle, Mass Heal.
        pArea = new CProjectileArea(300);
        pArea->TargetFriend(pSprite);
        pArea->m_explosion1.m_animation = "Area1X";
        pArea->m_explosion1.m_sound = "EFF_P99";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE);
        pProjectile = pArea;
        break;
    case 373:
        // Control Undead, Mass Dominate.
        pArea = new CProjectileArea(200);
        pArea->m_explosion1.m_animation = "EnchanX";
        pArea->m_explosion1.m_sound = "ARE_M21";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE);
        pProjectile = pArea;
        break;
    case 376:
        // Turn Undead.
        pProjectile = new CProjectileArea850A38();
        break;
    default:
        if (static_cast<UINT>(nProjectileType - 1) > 0x181) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CProjectile.cpp
            // __LINE__: 3127
            UTIL_ASSERT(FALSE);
        }

        // TODO: Incomplete (0x51EB93-0x528DEF). The jump table at 0x528E7C
        // builds one class per type, 2 to 386; only types 1, 38, 68 to 72,
        // 79, 218 and the area types used by the game's spells and items
        // are ported.
        // DEVIATION (temporary): unported type -> instant projectile. The
        // original reaches the fallback below only for types out of range
        // and for the table entries 106, 107, 219-226, 259 and 265. Until
        // the other cases are ported, a replay differs in two ways: the
        // effects land on the Fire frame instead of after the projectile's
        // flight, and the instant projectile never calls
        // GetObjectArray()->Add where the real classes do (some also add a
        // VEF and sparkle objects), so every later object id shifts.
        (void)pSprite;
        (void)nUnknown;
        break;
    }

    if (pProjectile == NULL) {
        pProjectile = new CProjectile84D8B8();
    }

    pProjectile->m_projectileType = nProjectileType - 1;

    return pProjectile;
}

// 0x529F10
void CProjectile::ClearEffects()
{
    POSITION pos = m_effectList.GetHeadPosition();
    while (pos != NULL) {
        CGameEffect* node = m_effectList.GetNext(pos);
        delete node;
    }
    m_effectList.RemoveAll();
}

// 0x529F40
LONG CProjectile::DetermineHeight(CGameSprite* pSprite)
{
    if (!m_bHasHeight) {
        return 0;
    }

    if (pSprite->GetObjectType() != TYPE_SPRITE) {
        return 32;
    }

    return pSprite->GetAnimation()->GetCastHeight();
}

// 0x78E740
void CProjectile::Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType)
{
}

// 0x529FB0
void CProjectile::OnArrival()
{
    CProjectile* pProjectile;
    BYTE rc;

    if (m_callBackProjectile != CGameObjectArray::INVALID_INDEX) {
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(m_callBackProjectile,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pProjectile),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc != CGameObjectArray::SUCCESS) {
            return;
        }

        pProjectile->CallBack();

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(m_callBackProjectile,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    // NOTE: Uninline.
    PlaySound(m_arrivalSoundRef, m_loopArrivalSound, TRUE);

    if (field_182 != CGameObjectArray::INVALID_INDEX) {
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(field_182,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pProjectile),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            // Direct base call in the binary at 0x52A118.
            pProjectile->CProjectile::RemoveSelf();

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(field_182,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }

    DeliverEffects();
    RemoveFromArea();

    rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Delete(m_id,
        CGameObjectArray::THREAD_ASYNCH,
        NULL,
        INFINITE);
    if (rc == CGameObjectArray::SUCCESS) {
        delete this;
    }
}

// 0x52A1A0
void CProjectile::DeliverEffects()
{
    CGameObject* pObject;

    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_targetId,
            CGameObjectArray::THREAD_ASYNCH,
            &pObject,
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        return;
    }

    if ((pObject->GetObjectType() & CGameObject::TYPE_AIBASE) == 0) {
        POSITION pos = m_effectList.GetHeadPosition();
        while (pos != NULL) {
            CGameEffect* node = m_effectList.GetNext(pos);
            if (node != NULL) {
                delete node;
            }
        }
        m_effectList.RemoveAll();

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_targetId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        return;
    }

    if (pObject->GetObjectType() == CGameObject::TYPE_SPRITE) {
        CGameSprite* pSprite = static_cast<CGameSprite*>(pObject);
        if (pSprite->GetDerivedStats()->m_cImmunitiesProjectile.OnList(m_projectileType)
            || (static_cast<BYTE>(m_nSpellLevel) < 9
                && pSprite->GetDerivedStats()->m_cImmunitiesSpellLevel.m_levels[static_cast<BYTE>(m_nSpellLevel)] != FALSE)) {
            POSITION pos = m_effectList.GetHeadPosition();
            while (pos != NULL) {
                CGameEffect* node = m_effectList.GetNext(pos);
                if (node != NULL) {
                    delete node;
                }
            }
            m_effectList.RemoveAll();

            CGameEffect::FeedBackImmuneToResource(pSprite, m_sourceRes);

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_targetId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            return;
        }
    }

    std::list<CMessage*> messages;

    POSITION pos = m_effectList.GetHeadPosition();
    while (pos != NULL) {
        CGameEffect* node = m_effectList.GetNext(pos);
        CMessage* message = new CMessageAddEffect(node, m_sourceId, m_targetId);
        messages.push_back(message);
    }
    m_effectList.RemoveAll();

    CMessage* message = new CMessageGroup(messages, m_sourceId, m_targetId);
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_targetId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);
}

// 0x52A480
SHORT CProjectile::GetDirection(CPoint target)
{
    CPoint ptStart;
    ptStart.x = m_pos.x;
    ptStart.y = 4 * m_pos.y / 3;

    CPoint ptTarget;
    ptTarget.x = target.x;
    ptTarget.y = 4 * target.y / 3;

    return CGameSprite::GetDirection(ptStart, ptTarget);
}

// CompressTime (vtable slot 0x2C): the game is fast-forwarding time past
// this projectile and nothing here survives a skip, so it removes itself
// from the object array and deletes itself. It first asserts the skip is
// more than one minute (the exe multiplies the two timer bytes 15 and 60).
// NOTE: The exe masks the low byte off its return in every path, so the
// BOOLEAN answer is always FALSE.
// 0x536F40
BOOLEAN CProjectile::CompressTime(DWORD deltaTime)
{
    UTIL_ASSERT(deltaTime > CTimerWorld::TIMESCALE_MSEC_PER_SEC * CTimerWorld::TIMESCALE_SEC_PER_MIN);

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Delete(m_id,
        CGameObjectArray::THREAD_ASYNCH,
        NULL,
        INFINITE);
    if (rc == CGameObjectArray::SUCCESS) {
        delete this;
    }

    return FALSE;
}

// 0x536FC0
// TRUE when the target is immune to this projectile: the projectile type is
// on the target's projectile-immunity list, or m_nSpellLevel (when it is 8
// or less) indexes a set slot of the spell-level immunity table (the table
// CGameEffectImmunityToSpellLevel writes).
BOOL CProjectile::IsTargetImmune(CGameSprite* pSprite)
{
    if (pSprite->GetDerivedStats()->m_cImmunitiesProjectile.OnList(m_projectileType) == 0
        && ((m_nSpellLevel & 0xFF) > 8
            || pSprite->GetDerivedStats()->m_cImmunitiesSpellLevel.m_levels[m_nSpellLevel & 0xFF] == 0)) {
        return FALSE;
    }

    return TRUE;
}

// 0x52A640
void CProjectile::GetSourcePosition(LONG nObjectId, CPoint& pt, BOOLEAN bLaunchOffset)
{
    CGameObject* pObject;

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(nObjectId,
        CGameObjectArray::THREAD_ASYNCH,
        &pObject,
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        return;
    }

    switch (pObject->GetObjectType()) {
    case 0x00:
    case 0x01:
    case 0x10:
    case 0x20:
    case 0x30:
    case 0x31:
    case 0x40:
    case 0x50:
        pt = pObject->GetPos();
        break;
    case 0x11:
        pt = static_cast<CGameContainer*>(pObject)->m_posTrapOrigin;
        break;
    case 0x21:
        pt.x = static_cast<CGameDoor*>(pObject)->m_posXTrapOrigin;
        pt.y = static_cast<CGameDoor*>(pObject)->m_posYTrapOrigin;
        break;
    case 0x41:
        pt = static_cast<CGameTrigger*>(pObject)->m_posTrapOrigin;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CProjectile.cpp
        // __LINE__: 3590
        UTIL_ASSERT(FALSE);
        break;
    }

    if (bLaunchOffset && pObject->GetObjectType() == CGameObject::TYPE_SPRITE) {
        CGameSprite* pSprite = static_cast<CGameSprite*>(pObject);
        SHORT nOffset;

        switch (pSprite->m_nDirection) {
        case 0:
        case 1:
            pt.y += pSprite->GetAnimation()->GetEllipseRect().bottom >> 1;
            break;
        case 2:
        case 3:
            nOffset = GetLaunchOffset(pSprite);
            pt.x += nOffset * -2;
            pt.y += nOffset * 2;
            break;
        case 4:
        case 5:
            pt.x += pSprite->GetAnimation()->GetEllipseRect().right * -2;
            pt.y += 1;
            break;
        case 6:
        case 7:
            nOffset = GetLaunchOffset(pSprite);
            pt.x += nOffset * -2;
            pt.y += nOffset * -2;
            break;
        case 8:
        case 9:
            pt.y += pSprite->GetAnimation()->GetEllipseRect().bottom * -2;
            break;
        case 10:
        case 11:
            nOffset = GetLaunchOffset(pSprite);
            pt.x += nOffset * 2;
            pt.y += nOffset * -2;
            break;
        case 12:
        case 13:
            pt.x += pSprite->GetAnimation()->GetEllipseRect().right * 2;
            pt.y += 1;
            break;
        case 14:
        case 15:
            nOffset = GetLaunchOffset(pSprite);
            pt.x += nOffset * 2;
            pt.y += nOffset * 2;
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CProjectile.cpp
            // __LINE__: 3664
            UTIL_ASSERT(FALSE);
            break;
        }
    }

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nObjectId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);
}

// NOTE: Inlined in GetSourcePosition (0x52A79A-0x52A889): the distance to
// the sprite's ellipse along a diagonal, from its two radii. The products
// are 16-bit, as in the original.
SHORT CProjectile::GetLaunchOffset(CGameSprite* pSprite)
{
    const CRect& rEllipse = pSprite->GetAnimation()->GetEllipseRect();
    SHORT a = static_cast<SHORT>(static_cast<SHORT>(rEllipse.right) * static_cast<SHORT>(rEllipse.right));
    SHORT b = static_cast<SHORT>(static_cast<SHORT>(rEllipse.bottom) * static_cast<SHORT>(rEllipse.bottom));
    LONG nProduct = (a * b) / (b + a);
    return static_cast<SHORT>(static_cast<LONG>(sqrt(static_cast<double>(nProduct))));
}

// 0x52A4E0
void CProjectile::PlaySound(CResRef resRef, BOOL loop, BOOL fireAndForget)
{
    m_sound.Stop();
    if (resRef != "") {
        m_sound.SetResRef(resRef, TRUE, TRUE);
        if (loop) {
            m_sound.SetLoopingFlag(TRUE);
        }
        if (fireAndForget) {
            m_sound.SetFireForget(TRUE);
        }
        m_sound.SetChannel(15, reinterpret_cast<DWORD_PTR>(m_pArea));
        m_sound.Play(m_pos.x, m_pos.y, 0, FALSE);
    }
}

// 0x78E730
void CProjectile::CallBack()
{
}

// -----------------------------------------------------------------------------

// 0x530790
CProjectile84D8B8::CProjectile84D8B8()
{
}

// 0x5295D0
void CProjectileBAM::SetVidCell(CResRef resRef)
{
    if (m_animation != NULL) {
        delete m_animation;
    }
    m_animation = new CVidCell(resRef, FALSE);
}

// 0x5297B0
CProjectileSparkleExplosion::CProjectileSparkleExplosion(CResRef resRef)
    : CProjectileBAM(resRef)
{
}

// 0x529950
void CProjectile84D8B8::Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType)
{
    m_targetId = target;
    m_sourceId = source;
    m_pArea = pArea;
    m_callBackProjectile = CGameObjectArray::INVALID_INDEX;

    OnArrival();

    // NOTE: OnArrival's Delete fails (the projectile was never added to the
    // object array), so the projectile deletes itself here.
    delete this;
}

// 0x52CA10
CProjectileSparkle::CProjectileSparkle(SHORT nPaletteType)
    : CProjectileBAM(CResRef("SPMAGMIS"))
{
    m_shadow = NULL;
    field_1D8 = 0;
    field_1DA = 0;
    m_nDirection = 0;
    field_1DE = FALSE;
    field_29C = 0;
    m_bLimitedLifetime = FALSE;
    m_nLifetime = 0;

    m_animation->SequenceSet(0);

    m_speed = static_cast<SHORT>(m_speed << 1);
    field_1CA = 0xF;
    field_1CE = 0xB;
    field_1BE = FALSE;
    field_1C2 = FALSE;
    field_1C6 = TRUE;
    field_1D2 = 0x80;
    field_1D4 = FALSE;
    m_fireSoundRef = "";
    field_E6 |= 8;

    if (nPaletteType != 0) {
        if (nPaletteType == CVidPalette::TYPE_RANGE) {
            m_palette.SetRange(0, 0x21, *g_pBaldurChitin->GetObjectGame()->GetMasterBitmap());
            m_animation->SetPalette(m_palette);
        } else {
            UTIL_ASSERT(FALSE);
        }
    }

    m_callBackProjectile = CGameObjectArray::INVALID_INDEX;
    m_sVisualEffect = "";
    field_182 = CGameObjectArray::INVALID_INDEX;
}

// 0x52CCE0
CProjectileExploding::CProjectileExploding(CResRef resRef)
    : CProjectileBAM(resRef)
    , m_targetType(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0)
{
    m_callBackProjectile = CGameObjectArray::INVALID_INDEX;
    m_sVisualEffect = "";
    field_182 = CGameObjectArray::INVALID_INDEX;
    m_childProjectileType = 0x4E;
    m_strikesLeft = 1;
    m_nState = 0;
    m_lingerPeriod = 100;
    m_lingerCountdown = 0;
    m_targetType.Set(CAIObjectType::ANYONE);
    m_strikeRange = 0x100;
    m_preCheckRange = 0x100;
    m_bPreScan = 0;
    m_bBurstPending = 0;
    field_1BE = 0;
    m_bExplodeCell1Active = 0;
    m_bExplodeCell2Active = 0;
    m_bCheckNonSprites = 0;
    m_burstType = 0xFF;
}

// 0x52E940
//
// The weapon missile's burst: the fireball (CGameObject85BFE8) with its
// colour ranges set from the leaf's own explosion colour range — the strike
// range around, expanding at half the flight speed, destroyed by the first
// wall it meets, no duration of its own.
void CProjectileExplodingWeapon::Explode()
{
    BYTE colorRangeValues[7];
    memset(colorRangeValues, m_explodeColorRange, sizeof(colorRangeValues));
    new CGameObject85BFE8(0, colorRangeValues, m_pArea, m_pos, m_strikeRange,
        static_cast<BYTE>(m_speed) >> 1, 1, 0);
}

// 0x52F1C0
//
// The flame missile's burst: the same fireball with the colour ranges
// hardwired to 0x43.
void CProjectileExplodingFlame::Explode()
{
    BYTE colorRangeValues[7];
    memset(colorRangeValues, 0x43, sizeof(colorRangeValues));
    new CGameObject85BFE8(0, colorRangeValues, m_pArea, m_pos, m_strikeRange,
        static_cast<BYTE>(m_speed) >> 1, 1, 0);
}

// 0x52F760
//
// The skull trap's burst: the ShSkull sound (the resref the parked ctor
// stored at +0x4B4) fire-and-forget on channel 14, then the bone chunk
// spray the shatter effect throws — one CGameChunk per count, sized by the
// trap's own child type, thrown sideways at up to 5 either way and upwards
// at one of three speeds, the chunk lifetime the shatter effect uses.
void CProjectileSkullTrap::Explode()
{
    CSound sound;
    CString sSound(""); // re-lint: disable RE002 (char-based CString in this tree; no _T in the tree)
    // NOTE: the exe passes a stack buffer it never writes here.
    BYTE colorRangeValues[7];

    sound.SetResRef(m_explodeSound, TRUE, TRUE);
    sound.SetFireForget(TRUE);
    sound.SetChannel(14, reinterpret_cast<DWORD_PTR>(m_pArea));
    sound.Play(m_pos.x, m_pos.y, 0, FALSE);

    BYTE nChunks = static_cast<BYTE>(rand() % 16 + 10);
    while (nChunks != 0) {
        LONG nPosZDelta = rand() % 3 * 3 + 3; // the exe draws this one first
        CPoint ptVelocity;
        ptVelocity.y = rand() % 11 - 5; // …then y…
        ptVelocity.x = rand() % 11 - 5; // …then x
        new CGameChunk(m_nType, colorRangeValues, sSound, m_pArea, m_pos,
            ptVelocity, -m_posZ, nPosZDelta, (CHUNK_DURATION + 1) << 2,
            CHUNK_DURATION + 1);
        nChunks--;
    }
}

// 0x531360
CProjectileBolt::CProjectileBolt()
    : CProjectileBAM(CResRef("BOLT"))
{
    m_animation->SequenceSet(0);
    field_1BE = TRUE;
    field_1C2 = FALSE;
    field_1C6 = FALSE;
    field_1D4 = FALSE;
    field_1D8 = 16;
    m_fireSoundRef = "";
    m_speed = static_cast<SHORT>(m_speed * 5);
    m_callBackProjectile = CGameObjectArray::INVALID_INDEX;
    m_sVisualEffect = "";
    field_182 = CGameObjectArray::INVALID_INDEX;
}

// 0x531570
CProjectileBoulder::CProjectileBoulder()
    : CProjectileBAM(CResRef("BOULDER"))
{
    m_animation->SequenceSet(0);
    field_1BE = TRUE;
    field_1C2 = FALSE;
    field_1C6 = FALSE;
    field_1D4 = FALSE;
    field_1D8 = 16;
    m_fireSoundRef = "";
    m_speed = static_cast<SHORT>(m_speed * 3 / 2);
    m_callBackProjectile = CGameObjectArray::INVALID_INDEX;
    m_sVisualEffect = "";
    field_182 = CGameObjectArray::INVALID_INDEX;
}

// 0x531790
CProjectileDart::CProjectileDart()
    : CProjectileBAM(CResRef("DART"))
{
    m_animation->SequenceSet(0);
    field_1BE = TRUE;
    field_1C2 = FALSE;
    field_1C6 = FALSE;
    field_1D4 = FALSE;
    field_1D8 = 16;
    m_fireSoundRef = "";
    m_speed = static_cast<SHORT>(m_speed * 3);
    m_callBackProjectile = CGameObjectArray::INVALID_INDEX;
    m_sVisualEffect = "";
    field_182 = CGameObjectArray::INVALID_INDEX;
}

// 0x5319A0
CProjectileSlingStone::CProjectileSlingStone()
    : CProjectileBAM(CResRef("MAGICSTN"))
{
    m_animation->SequenceSet(0);
    m_palette.SetRange(0, 27, *g_pBaldurChitin->GetObjectGame()->GetMasterBitmap());
    m_animation->SetPalette(m_palette);
    field_1BE = TRUE;
    field_1C2 = FALSE;
    field_1C6 = FALSE;
    field_1D4 = FALSE;
    field_1D8 = 1;
    m_fireSoundRef = "";
    m_speed = static_cast<SHORT>(m_speed * 5);
    m_callBackProjectile = CGameObjectArray::INVALID_INDEX;
    m_sVisualEffect = "";
    field_182 = CGameObjectArray::INVALID_INDEX;
}

// 0x531BE0
CProjectileAxe::CProjectileAxe()
    : CProjectileBAM(CResRef("AXE"))
{
    m_animation->SequenceSet(0);
    field_1BE = TRUE;
    field_1C2 = FALSE;
    field_1C6 = FALSE;
    field_1D4 = FALSE;
    field_1D8 = 16;
    m_fireSoundRef = "";
    m_speed = static_cast<SHORT>(m_speed * 2);
    m_callBackProjectile = CGameObjectArray::INVALID_INDEX;
    m_sVisualEffect = "";
    field_182 = CGameObjectArray::INVALID_INDEX;
}

// 0x531DF0
CProjectileSpear::CProjectileSpear()
    : CProjectileBAM(CResRef("SPEAR"))
{
    m_animation->SequenceSet(0);
    field_1BE = TRUE;
    field_1C2 = FALSE;
    field_1C6 = FALSE;
    field_1D4 = FALSE;
    field_1D8 = 16;
    m_fireSoundRef = "";
    m_speed = static_cast<SHORT>(m_speed * 2);
    m_callBackProjectile = CGameObjectArray::INVALID_INDEX;
    m_sVisualEffect = "";
    field_182 = CGameObjectArray::INVALID_INDEX;
}

// 0x532260
CProjectileAcidBlob::CProjectileAcidBlob()
    : CProjectileBAM(CResRef("ACIDBLOB"))
{
    m_animation->SequenceSet(0);
    m_palette.SetRange(0, 73, *g_pBaldurChitin->GetObjectGame()->GetMasterBitmap());
    m_animation->SetPalette(m_palette);
    field_1BE = TRUE;
    field_1C2 = FALSE;
    field_1C6 = FALSE;
    field_1D4 = FALSE;
    field_1D8 = 16;
    m_fireSoundRef = "";
    m_callBackProjectile = CGameObjectArray::INVALID_INDEX;
    m_sVisualEffect = "";
    field_182 = CGameObjectArray::INVALID_INDEX;
}

// 0x5324A0
CProjectileArrow::CProjectileArrow()
    : CProjectileBAM(CResRef("ARARROW"))
{
    m_animation->SequenceSet(0);
    m_palette.SetRange(5, 0x1B, *g_pBaldurChitin->GetObjectGame()->GetMasterBitmap());
    m_palette.SetRange(4, 0x17, *g_pBaldurChitin->GetObjectGame()->GetMasterBitmap());
    m_palette.SetRange(2, 0x2E, *g_pBaldurChitin->GetObjectGame()->GetMasterBitmap());
    m_animation->SetPalette(m_palette);
    field_1BE = TRUE;
    field_1C2 = FALSE;
    field_1C6 = FALSE;
    field_1D4 = FALSE;
    field_1D8 = 16;
    m_fireSoundRef = "";
    m_speed = static_cast<SHORT>(m_speed * 5);
    m_callBackProjectile = CGameObjectArray::INVALID_INDEX;
    m_sVisualEffect = "";
    field_182 = CGameObjectArray::INVALID_INDEX;
}

// 0x532720
CProjectileSPFLMARR::CProjectileSPFLMARR()
    : CProjectileBAM(CResRef("SPFLMARR"))
{
    m_animation->SequenceSet(0);
    m_palette.SetRange(0, 0x43, *g_pBaldurChitin->GetObjectGame()->GetMasterBitmap());
    m_palette.SetRange(1, 0x2E, *g_pBaldurChitin->GetObjectGame()->GetMasterBitmap());
    m_animation->SetPalette(m_palette);
    field_1BE = FALSE;
    field_1C2 = FALSE;
    field_1C6 = FALSE;
    field_1D4 = FALSE;
    field_1D8 = 16;
    m_fireSoundRef = "";
    m_speed = static_cast<SHORT>(m_speed * 5);
    m_trailTick = 0;
    memset(m_trailColorRanges, 0x41, sizeof(m_trailColorRanges));
    m_callBackProjectile = CGameObjectArray::INVALID_INDEX;
    m_sVisualEffect = "";
    field_182 = CGameObjectArray::INVALID_INDEX;
}

// 0x533DE0
void CProjectileFollowUp::CallBack()
{
    m_ready = TRUE;
}

// 0x534600
CProjectileMAGMISS::CProjectileMAGMISS(SHORT count, USHORT /*unused*/)
    : CProjectileFollowUp(0x16, CResRef("MAGMISS"), count, 300, TRUE, CAIObjectType::ANYONE)
{
    field_1CA = 10;
    field_1CE = 7;
    field_1BE = FALSE;
    field_1C2 = FALSE;
    field_1C6 = TRUE;
    field_1D2 = 0x80;
    field_1D4 = FALSE;
    field_1D8 = 1;
    m_fireSoundRef = "";
    m_callBackProjectile = CGameObjectArray::INVALID_INDEX;
    m_sVisualEffect = "";
    field_182 = CGameObjectArray::INVALID_INDEX;

    CAIObjectType type;
    type.m_nEnemyAlly = CAIObjectType::EA_NOTGOOD;
    m_targetType.Set(type);
    m_checkLOS = TRUE;
}

// 0x56F1F0
//
// The exe's body is the compiler-generated member destruction — the set,
// the two CSounds, the three explosion strings (third to first), the
// target-hits map, the two embedded CVidCells, the target filter's CString,
// then the CProjectileSingleTarget dtor (0x5783E0). Ours is the empty
// braces the compiler emits the same destruction from.
CProjectileArea::~CProjectileArea()
{
}

// 0x56FDC0
//
// The second explosion's description: the base (two empty strings and the
// visual effect), then the animation-mode byte clear and the field_2E
// sentinel INT_MAX (the fork names them m_animMode and m_maxMovingSpawn —
// the cap on moving pieces the emission may still spawn).
CProjectileAreaExplosion2::CProjectileAreaExplosion2()
{
    field_2C = 0;
    field_2E = 0x7FFFFFFF;
}

// 0x56FE30
//
// The third explosion description's ctor: no burned points yet, the two
// chance bytes and the trailing flags clear, a 250 density base, a 6-tick
// emission period and a 30 density-ramp divisor. (The exe also stores
// uninitialized stack bytes into the two string flags at +0x00/+0x10 — a
// VC6 artifact our default-constructed string members cover; our
// CProjectileArea ctor at 0x56EDD0 stores these same values again after the
// member ctor has run, so the net construction is unchanged.)
CProjectileAreaExplosion3::CProjectileAreaExplosion3()
{
    m_pPoints = NULL;
    field_30 = 0;
    field_34 = 0;
    field_35 = 0;
    field_36 = 0;
    field_38 = 250;
    field_3C = 6;
    field_40 = 30;
    field_44 = 0;
}

// 0x571170
CProjectileAcidStorm::CProjectileAcidStorm()
    : CProjectileArea(0x100)
{
    m_explosion1.m_animation = "AStormX";
    m_explosion1.m_visualEffect.sub_586A90(TRUE);
    m_explosion3.m_animation = "AStormA";
    m_explosion3.m_sound = "ARE_M04";
    m_explosion3.m_visualEffect.sub_586A90(TRUE);
    m_explosion3.field_35 = TRUE;
    m_explosion3.field_36 = TRUE;
    m_nTriggerInterval = 10;
    m_nTriggerCountdown = 0;
    m_nHitModulus = 10000;
    m_nDuration = 45;
    field_1D8 = 1;
    m_nRadius = 200;
}

// 0x571930
CProjectileCircleOfDeath::CProjectileCircleOfDeath()
    : CProjectileArea(0x100)
{
    m_explosion1.m_animation = "DSpellX";
    m_explosion1.m_sound = "EFF_M42";
    m_explosion1.m_visualEffect.sub_586A90(TRUE);
    m_nTriggerInterval = 10000;
    m_nTriggerCountdown = 0;
    m_nHitModulus = 10;
    m_nDuration = 45;
    m_nRadius = 300;
}

// 0x572830
CProjectileHoldAnimal::CProjectileHoldAnimal()
    : CProjectileArea(0x100)
{
    m_explosion1.m_animation = "EnchanX";
    m_explosion1.m_sound = "ARE_M21";
    m_explosion1.m_visualEffect.sub_586A90(TRUE);
    m_nTriggerInterval = 10000;
    m_nTriggerCountdown = 0;
    m_nHitModulus = 10;
    m_nDuration = 45;
    m_nRadius = 200;
}

// 0x572B30
CProjectileHoldMonster::CProjectileHoldMonster()
    : CProjectileArea(0x100)
{
    m_explosion1.m_animation = "EnchanX";
    m_explosion1.m_sound = "ARE_M21";
    m_explosion1.m_visualEffect.sub_586A90(TRUE);
    m_nTriggerInterval = 10000;
    m_nTriggerCountdown = 0;
    m_nHitModulus = 10;
    m_nDuration = 45;
    m_nRadius = 200;
}

// 0x573160
CProjectileHaltUndead::CProjectileHaltUndead()
    : CProjectileArea(0x100)
{
    m_explosion1.m_animation = "EnchanX";
    m_explosion1.m_sound = "ARE_M21";
    m_explosion1.m_visualEffect.sub_586A90(TRUE);
    m_nTriggerInterval = 10000;
    m_nTriggerCountdown = 0;
    m_nHitModulus = 10;
    m_nDuration = 45;
    m_nRadius = 150;
}

// 0x573460
CProjectileIceStorm::CProjectileIceStorm()
    : CProjectileArea(0x100)
{
    m_explosion1.m_animation = "IStormX";
    m_explosion1.m_visualEffect.sub_586A90(TRUE);
    m_explosion3.m_animation = "IStormA";
    m_explosion3.m_sound = "ARE_M04";
    m_explosion3.m_visualEffect.sub_586A90(TRUE);
    m_explosion3.field_34 = TRUE;
    m_explosion3.field_35 = TRUE;
    m_nTriggerInterval = 10;
    m_nTriggerCountdown = 0;
    m_nHitModulus = 10000;
    m_nDuration = 100;
    field_1D8 = 1;
    m_nRadius = 200;
}

// 0x573600
CProjectileFieryCloud::CProjectileFieryCloud()
    : CProjectileArea(0x100)
{
    field_65E = 0;
    m_explosion1.m_animation = "ICloudX";
    m_explosion1.m_sound = "EFF_M40";
    m_explosion1.m_visualEffect.sub_586A90(TRUE);
    m_explosion2.m_animation = "ICloudR";
    m_explosion2.m_visualEffect.sub_586A90(TRUE);
    m_explosion2.field_2C = TRUE;
    m_explosion2.field_2E = 13;
    m_explosion3.m_animation = "ICloudA";
    m_explosion3.m_sound = "ARE_M05";
    m_explosion3.m_visualEffect.sub_586A90(TRUE);
    m_explosion3.field_34 = TRUE;
    m_explosion3.field_35 = TRUE;
    m_explosion3.field_38 = 1500;
    m_explosion3.field_44 = TRUE;
    m_nDuration = 1050;
    field_1D8 = 1;
    m_nRadius = 100;
}

// 0x573AF0
CProjectileInsectPlague::CProjectileInsectPlague()
    : CProjectileArea(0x100)
{
    m_explosion1.m_animation = "IPlaguX";
    m_explosion1.m_sound = "RNG_P01";
    m_explosion1.m_visualEffect.sub_586A90(TRUE);
    m_explosion2.m_animation = "IPlaguR";
    m_explosion2.m_visualEffect.sub_586A90(TRUE);
    m_explosion2.field_2C = TRUE;
    m_explosion2.field_2E = 13;
    m_explosion3.m_animation = "IPlaguA";
    m_explosion3.m_sound = "ARE_P02";
    m_explosion3.m_visualEffect.sub_586A90(TRUE);
    m_nTriggerInterval = 10;
    m_nHitModulus = 10;
    m_explosion3.field_34 = TRUE;
    m_explosion3.field_35 = TRUE;
    m_nTriggerCountdown = 0;
    m_nDuration = 1500;
    field_1D8 = 1;
    m_nRadius = 250;
}

// 0x573E90
CProjectilePowerWordKill::CProjectilePowerWordKill()
    : CProjectileArea(0x100)
{
    m_explosion1.m_animation = "PWKillX";
    m_explosion1.m_sound = "EFF_M39";
    m_explosion1.m_visualEffect.sub_586A90(TRUE);
    m_nTriggerInterval = 10000;
    m_nTriggerCountdown = 0;
    m_nHitModulus = 10;
    m_nDuration = 45;
    m_nRadius = 150;
}

// 0x574140
CProjectileProduceFire::CProjectileProduceFire()
    : CProjectileArea(0x100)
{
    m_explosion1.m_animation = "PFireX";
    m_explosion1.m_sound = "EFF_P45";
    m_explosion1.m_visualEffect.sub_586A90(TRUE);
    m_explosion3.m_animation = "PFireA";
    m_explosion3.m_sound = "ARE_P03";
    m_explosion3.m_visualEffect.sub_586A90(TRUE);
    m_explosion3.field_34 = TRUE;
    m_explosion3.field_35 = TRUE;
    m_explosion3.field_38 = 750;
    m_nTriggerInterval = 10;
    m_nTriggerCountdown = 0;
    m_nHitModulus = 10000;
    m_nDuration = 100;
    field_1D8 = 1;
    m_nRadius = 60;
}

// 0x576BA0
void CProjectileDBFireball::AIUpdate()
{
    if (m_bExploded == 0) {
        CProjectileArea::AIUpdate();
        return;
    }
    if (m_bBlasted == 1) {
        CProjectileArea::AIUpdate();
        return;
    }

    if (++m_scanTimer == 5) {
        m_scanTimer = 0;

        CTypedPtrList<CPtrList, LONG*> targets;
        m_pArea->GetAllInRange(m_posVertList, m_pos, CAIObjectType::ANYONE, 100, m_terrainTable, targets, TRUE, FALSE);
        m_pArea->GetAllInRangeBack(m_pos, CAIObjectType::ANYONE, 100, m_terrainTable, targets, TRUE, FALSE, FALSE);

        if (targets.GetCount() != 0) {
            CProjectileArea::OnArrival();
            m_bBlasted = TRUE;
        }
    }

    m_animation->FrameAdvance();
}

// The exe keeps these in the zero-initialized virtual tail of .data — the
// raw range ends at 0x8C6000, and the elements sit at 0x8E52D4/0x8E52D8/
// 0x8E52DC with the flag at 0x8E52E8 — so the loaded image starts them all
// at 0. Default construction matches; the flag is set FALSE explicitly for
// definedness.
std::vector<CPoint> IcewindCProjectileScorcher::m_offsets;
// 0x8E52E8
BYTE IcewindCProjectileScorcher::m_offsetsInitialized = FALSE;

// 0x577590
CProjectileBigBoulder::CProjectileBigBoulder()
    : CProjectileArea(100)
{
    const char* szTravel = "BIGBOLDR";
    if (strlen(szTravel) != 0) {
        field_1DE = TRUE;
        SetTravelAnimation(CResRef(szTravel));
        m_bTravels = TRUE;
    } else {
        field_1DE = FALSE;
        m_bTravels = FALSE;
    }

    m_explosion3.field_34 = TRUE;
    m_explosion3.field_35 = FALSE;
    m_nTriggerInterval = 10;
    m_nTriggerCountdown = 0;
    m_nHitModulus = 10;
    field_1D8 = 1;
    m_nRadius = 100;
    m_speed = 7;
    m_fireSoundRef = CResRef("AM6103e");
    m_arrivalSoundRef = CResRef("AM5101e");
}

// 0x5778A0
//
// Everything the spell is lives in the first explosion: the SoPainX burst
// on the EFF_P49 sound with the copy-from-back mode set on the visual
// effect — the same first-explosion shape the spell-hit cases of
// DecodeProjectile build inline. The base is handed 256, then the radius
// is widened to 300.
CProjectileSymbolOfDeath::CProjectileSymbolOfDeath()
    : CProjectileArea(0x100)
{
    m_explosion1.m_animation = "SoPainX";
    m_explosion1.m_sound = "EFF_P49";
    m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
    m_nRadius = 300;
}

// 0x579DE0
//
// The exe's body is the compiler-generated member destruction: the vector
// buffer freed through the triple at +0x2F6/+0x2FA/+0x2FE, then the
// refcounted puff name released at +0x2E2 (count byte at ptr−1, the block
// freed when the count reaches 0 or was the 0xFF sentinel), then the
// CProjectileSingleTarget dtor (0x5783E0). Ours is the empty braces the
// compiler emits the same destruction from — m_puffPoints is declared
// after m_sPuff, so it dies first. Neither this body nor the deleting
// wrapper 0x579DC0 stores the vtable: the cone is a leaf, and no virtual
// dispatch happens during the teardown.
CProjectile850DDC::~CProjectile850DDC()
{
}

// 0x57AEB0
IcewindCProjectileScorcher::IcewindCProjectileScorcher()
    : CProjectileSingleTarget(CResRef("AScorcT"))
{
    m_tickCount = 0;
    m_strikePeriod = 3;
    m_clearPeriod = 60;
    m_deltaZ = 0;
    m_bGlow = FALSE;
    m_strikeTarget = 0;
    InitializeOffsets();
    m_animation->FrameSet(0);
    field_1C2 = 0;
    field_1D4 = 0;
    field_1D8 = 16;
    m_fireSoundRef = "TRA_M04";
    CSize size;
    m_animation->GetFrameSize(0, 0, size, FALSE);
    m_speed = static_cast<SHORT>(static_cast<LONG>(static_cast<DWORD>(size.cy) * 3) / 4);
    m_callBackProjectile = -1;
    m_sVisualEffect = "";
    field_182 = -1;
    m_nSegments = 0;
    m_lifeSpan = 110;
    m_bHasHeight = TRUE;
    field_1C2 = 1;
}

// 0x57B1D0
void IcewindCProjectileScorcher::Fire(CGameArea* pArea, LONG source, LONG target,
    CPoint targetPos, LONG nHeight, SHORT nType)
{
    CProjectileSingleTarget::Fire(pArea, source, target, targetPos, nHeight, nType);
    m_strikeTarget = target;
    DeliverEffects();
}

// 0x57B780
void IcewindCProjectileScorcher::Render(CGameArea* pArea, CVidMode* pVidMode, int nSurface)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\IcewindCProjectiles.cpp
    // __LINE__: 207
    if (pVidMode == NULL) {
        CUtil::UtilAssert(207, "C:\\Projects\\Icewind2\\src\\Baldur\\IcewindCProjectiles.cpp", "pVidMode != NULL", NULL);
    }

    CInfinity* pInfinity = m_pArea->GetInfinity();
    DWORD flags = 0x200;
    if (CGameSprite::DIR_N < field_1DA) {
        flags |= CInfinity::MIRROR_FX;
    }
    if (CGameSprite::DIR_W < field_1DA && field_1DA < CGameSprite::DIR_E) {
        flags |= CInfinity::MIRROR_FX_UPDOWN;
    }
    CRect viewport(pInfinity->nCurrentX, pInfinity->nCurrentY,
        pInfinity->nCurrentX + pInfinity->rViewPort.Width(),
        pInfinity->nCurrentY + pInfinity->rViewPort.Height());
    CPoint point(m_pos.x, m_pos.y - m_posZ + m_pArea->GetHeightOffset(m_pos, m_listType));
    CPoint reference;
    CSize size;
    m_animation->GetCurrentCenterPoint(reference, FALSE);
    m_animation->GetCurrentFrameSize(size, FALSE);
    CRect bounds;
    bounds.SetRect(0, 0, size.cx, size.cy);
    for (BYTE index = 0; index < m_nSegments; ++index) {
        SHORT tile = static_cast<SHORT>(static_cast<SHORT>(point.y / CVisibilityMap::SQUARE_SIZEY)
                * m_pArea->m_visibility.m_nWidth
            + point.x / CVisibilityMap::SQUARE_SIZEX);
        if (!m_pArea->m_visibility.IsTileVisible(tile)) {
            continue;
        }
        CRect gcBounds;
        gcBounds.left = CGameSprite::DIR_N < field_1DA
            ? reference.x - bounds.right + point.x + bounds.left
            : point.x - reference.x;
        gcBounds.top = CGameSprite::DIR_W < field_1DA && field_1DA < CGameSprite::DIR_E
            ? bounds.top - bounds.bottom + reference.y + point.y
            : point.y - reference.y;
        gcBounds.right = gcBounds.left + bounds.Width();
        gcBounds.bottom = gcBounds.top + bounds.Height();
        CRect intersection;
        intersection.IntersectRect(gcBounds, viewport);
        if (intersection.IsRectEmpty()) {
            continue;
        }
        flags |= CInfinity::FXPREP_COPYFROMBACK;
        pInfinity->FXPrep(bounds, flags, nSurface, point, reference);
        if (pInfinity->FXLock(bounds, flags)) {
            pInfinity->FXRender(m_animation, reference.x, reference.y, flags, 0);
            pInfinity->FXRenderClippingPolys(point.x, point.y, 0, reference, gcBounds, FALSE, flags);
            pInfinity->FXUnlock(flags, NULL, CPoint(0, 0));
            pInfinity->FXBltFrom(nSurface, bounds, point.x, point.y, reference.x, reference.y, flags);
        }
        INT step = index + 1;
        point.x = static_cast<LONG>(static_cast<DWORD>(m_deltaX) * step + static_cast<DWORD>(m_posExactX))
            >> CGameSprite::EXACT_SCALE;
        LONG scaledY = static_cast<LONG>((static_cast<DWORD>(m_deltaY) * step + static_cast<DWORD>(m_posExactY)) * 3);
        point.y = ((scaledY / 4) >> CGameSprite::EXACT_SCALE) - m_posZ
            + m_pArea->GetHeightOffset(m_pos, m_listType);
        if (m_deltaZ > 0) {
            point.y -= step < m_deltaZ ? step : m_deltaZ;
        } else if (m_deltaZ < 0) {
            LONG delta = static_cast<LONG>(0u - static_cast<DWORD>(m_deltaZ));
            point.y = static_cast<LONG>(static_cast<DWORD>(point.y)
                + static_cast<DWORD>(step < delta ? step : delta));
        }
    }
}

// 0x57BBC0
void IcewindCProjectileScorcher::InitializeOffsets()
{
    if (m_offsetsInitialized != TRUE) {
        // count: the exe loads it from the byte at 0x85BB48 (0x10).
        const size_t count = 16;
        if (m_offsets.size() < count) {
            CPoint point;
            m_offsets.insert(m_offsets.end(), count - m_offsets.size(), point);
        } else if (count < m_offsets.size()) {
            m_offsets.erase(m_offsets.begin() + count, m_offsets.end());
        }
        // The exe reads each store's element index from the .rdata byte
        // table at 0x85BB38 (00 02 04 06 08 0a 0c 0e 01 03 05 07 09 0b 0d
        // 0f) and writes (x, y) as two DWORDs — the same sixteen pairs, in
        // the same order, as these assignments.
        m_offsets[12] = CPoint(0, -20);
        m_offsets[11] = CPoint(-10, -18);
        m_offsets[10] = CPoint(-19, -14);
        m_offsets[9] = CPoint(-25, -8);
        m_offsets[8] = CPoint(-27, 0);
        m_offsets[7] = CPoint(-25, 8);
        m_offsets[6] = CPoint(-19, 14);
        m_offsets[5] = CPoint(-10, 18);
        m_offsets[4] = CPoint(0, 20);
        m_offsets[3] = CPoint(10, 18);
        m_offsets[2] = CPoint(19, 14);
        m_offsets[1] = CPoint(25, 8);
        m_offsets[0] = CPoint(-20, 0);
        m_offsets[15] = CPoint(-24, 8);
        m_offsets[14] = CPoint(-19, 14);
        m_offsets[13] = CPoint(-10, 18);
        // movb $0x1,0x8e52e8
        m_offsetsInitialized = TRUE;
    }
}

// 0x57BE80
void IcewindCProjectileScorcher::GatherTargets()
{
    const CPoint& offset = m_offsets[m_nDirection];
    CPoint polygon[4] = {
        CPoint(m_pos.x + offset.x, m_pos.y + offset.y),
        CPoint(m_pos.x - offset.x, m_pos.y - offset.y),
        CPoint(m_targetPos.x - offset.x, m_targetPos.y - offset.y),
        CPoint(m_targetPos.x + offset.x, m_targetPos.y + offset.y)
    };
    CRect bounds(polygon[0], polygon[0]);
    for (INT i = 1; i < 4; ++i) {
        bounds.left = min(bounds.left, polygon[i].x);
        bounds.top = min(bounds.top, polygon[i].y);
        bounds.right = max(bounds.right, polygon[i].x);
        bounds.bottom = max(bounds.bottom, polygon[i].y);
    }
    CTypedPtrList<CPtrList, LONG*> targets;
    m_pArea->GetAllInPoly(bounds, polygon, 4, CAIObjectType::ANYONE,
        m_pArea->m_visibleTerrainTable, targets, TRUE);
    POSITION position = targets.GetHeadPosition();
    while (position != NULL) {
        LONG target = static_cast<LONG>(reinterpret_cast<INT_PTR>(targets.GetNext(position)));
        if (m_struckTargets.find(static_cast<DWORD>(target)) == m_struckTargets.end()
            && target != m_sourceId) {
            Strike(target, FALSE);
            m_struckTargets.insert(static_cast<DWORD>(target));
        }
    }
}

// 0x57C1D0
void IcewindCProjectileScorcher::Strike(LONG target, BYTE initial)
{
    CGameObjectArray* objects = g_pBaldurChitin->GetObjectGame()->GetObjectArray();
    CGameObject* victim;
    if (objects->GetShare(target, CGameObjectArray::THREAD_ASYNCH, &victim, INFINITE)
        != CGameObjectArray::SUCCESS) {
        return;
    }

    if ((victim->GetObjectType() & CGameObject::TYPE_AIBASE) == 0) {
        objects->ReleaseShare(target, CGameObjectArray::THREAD_ASYNCH, INFINITE);
        return;
    }

    if (victim->GetObjectType() == CGameObject::TYPE_SPRITE
        && IsTargetImmune(static_cast<CGameSprite*>(victim)) == TRUE) {
        CGameEffect::FeedBackImmuneToResource(static_cast<CGameSprite*>(victim), m_sourceRes);
        objects->ReleaseShare(target, CGameObjectArray::THREAD_ASYNCH, INFINITE);
        return;
    }

    CGameObject* source;
    if (objects->GetShare(m_sourceId, CGameObjectArray::THREAD_ASYNCH, &source, INFINITE)
        != CGameObjectArray::SUCCESS) {
        objects->ReleaseShare(target, CGameObjectArray::THREAD_ASYNCH, INFINITE);
        return;
    }

    CPoint origin;
    GetSourcePosition(m_sourceId, origin, TRUE);
    if (m_pArea->CheckLOS(origin, victim->GetPos(), m_terrainTable, FALSE)) {
        std::list<CGameEffect*> effects = IcewindMisc::GetSpellResrefEffects(source,
            reinterpret_cast<const BYTE*>(initial == TRUE ? "EffAS1" : "EffAS2"),
            1);
        for (std::list<CGameEffect*>::iterator it = effects.begin(); it != effects.end(); it++) {
            CGameEffect* effect = *it;
            effect->m_sourceRes = CResRef("SpWi217");
            CMessage* message = new CMessageAddEffect(effect, m_sourceId, target);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
    }

    objects->ReleaseShare(target, CGameObjectArray::THREAD_ASYNCH, INFINITE);
    objects->ReleaseShare(m_sourceId, CGameObjectArray::THREAD_ASYNCH, INFINITE);
}

// NOTE: The original's slot holds 0x799CA0, a bare `ret`: MSVC ICF-folded it with
// CProjectileArea85072C::OnArrival (also empty), and the Scorcher's
// vtable slot 0x0078 points here.
void IcewindCProjectileScorcher::DeliverEffects()
{
}

// 0x57C510
IcewindCProjectileChainLightning::IcewindCProjectileChainLightning(const std::set<LONG>& struckTargets)
    : CProjectileSingleTarget(CResRef("CLightT")), m_struckTargets(struckTargets)
{
    m_animation->SequenceSet(0);
    field_1D8 = 16;
    m_fireSoundRef = "TRA_09";
    m_speed = static_cast<SHORT>(static_cast<USHORT>(m_speed) << 1);
    m_visualEffect.sub_586A90(TRUE);
    m_bHasHeight = FALSE;
    field_1C2 = FALSE;
}

// 0x57FF80
// The tornado's own launch: it starts the ARE_P23 loop (fire-and-forget on
// channel 15 of the area, at the tornado's position) and goes through the
// base with INVALID_INDEX for the target — the tornado never homes.
void CProjectileWhirlwind::Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType)
{
    (void)target;

    m_loopSound.SetResRef(CResRef("ARE_P23"), TRUE, TRUE);
    m_loopSound.SetFireForget(TRUE);
    m_loopSound.SetChannel(15, reinterpret_cast<DWORD_PTR>(m_pArea));
    m_loopSound.Play(m_pos.x, m_pos.y, 0, FALSE);

    CProjectileSingleTarget::Fire(pArea, source, CGameObjectArray::INVALID_INDEX, targetPos, nHeight, nType);
}

// 0x580B30
// Push the picked target point out to m_beamRange past it along the caster->
// target heading, so the bolt overshoots its mark and rakes the whole line,
// then launch through the base.
void CProjectileLightningBolt::Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType)
{
    CGameObject* pSource;
    if (g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(
            source, CGameObjectArray::THREAD_ASYNCH, &pSource, INFINITE)
        == CGameObjectArray::SUCCESS) {
        const CPoint& casterPos = pSource->GetPos();
        CPoint endPoint = IcewindMisc::sub_5847B0(targetPos,
            targetPos.x - casterPos.x, targetPos.y - casterPos.y, m_beamRange);
        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(
            source, CGameObjectArray::THREAD_ASYNCH, INFINITE);
        CProjectileSingleTarget::Fire(pArea, source, target, endPoint, nHeight, nType);
    }
}

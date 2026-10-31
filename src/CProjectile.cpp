#include "CProjectile.h"

#include "CBaldurChitin.h"
#include "CGameContainer.h"
#include "CGameDoor.h"
#include "CGameEffect.h"
#include "CGameSprite.h"
#include "CGameTrigger.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CProjectileArea.h"
#include "CProjectileBAM.h"
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

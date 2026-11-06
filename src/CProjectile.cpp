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
#include "CProjectile84DFA4.h"
#include "CProjectile84E940.h"
#include "CProjectile84E9C0.h"
#include "CProjectileArea.h"
#include "CProjectileBAM.h"
#include "CProjectileShaft.h"
#include "CProjectileSingleTarget.h"
#include "CProjectileSpellHit.h"
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
    CProjectileMAGMISS* pMAGMISS;
    CProjectile850DDC* pPuff;
    CProjectileSpellHit* pSpellHit;
    CProjectileSingleTarget* pSingle;
    CProjectileSparkle* pSparkle;
    CProjectileSparkleExplosion* pSparkleExplosion;
    CProjectileExploding* pExploding;
    CProjectileExploding* pPick;
    CProjectileBAM* pBam;
    CProjectileSPFLMARR* pFlame;
    CProjectileShaft* pShaft;
    CProjectile84E940* pDoor;
    CProjectile84DFA4* pBurst;
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
    case 2:
    case 6:
        // The plain arrow: height flag and empty sound tails.
        pProjectile = new CProjectileArrow();
        pProjectile->m_bHasHeight = TRUE;
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 5:
        // The arrow with the third palette range recoloured. The original
        // calls SetPalette before SetRange (the palette pointer is shared,
        // so the later SetRange lands in the cell's copy too).
        pBam = new CProjectileArrow();
        pBam->m_bHasHeight = TRUE;
        pBam->m_animation->SetPalette(pBam->m_palette);
        pBam->m_palette.SetRange(2, 0x39, *g_pBaldurChitin->GetObjectGame()->GetMasterBitmap());
        pBam->m_fireSoundRef = "";
        pBam->m_arrivalSoundRef = "";
        pProjectile = pBam;
        break;
    case 7:
    case 10:
    case 11:
        // The throwing axe: height flag and empty sound tails.
        pProjectile = new CProjectileAxe();
        pProjectile->m_bHasHeight = TRUE;
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 12:
    case 15:
    case 16:
        // The bolt: height flag and empty sound tails.
        pProjectile = new CProjectileBolt();
        pProjectile->m_bHasHeight = TRUE;
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 17:
    case 20:
    case 21:
        // The sling stone: height flag and empty sound tails.
        pProjectile = new CProjectileSlingStone();
        pProjectile->m_bHasHeight = TRUE;
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 41:
        // The sling stone with the base palette range recoloured. The
        // original points the cell's palette back at the projectile's own
        // before recolouring range 0.
        pBam = new CProjectileSlingStone();
        pBam->m_animation->SetPalette(pBam->m_palette);
        pBam->m_palette.SetRange(0, 0x1C, *g_pBaldurChitin->GetObjectGame()->GetMasterBitmap());
        pBam->m_bHasHeight = TRUE;
        pBam->m_fireSoundRef = "";
        pBam->m_arrivalSoundRef = "";
        pProjectile = pBam;
        break;
    case 27:
    case 30:
    case 31:
        // The dagger: height flag and empty sound tails.
        pProjectile = new CProjectileDagger();
        pProjectile->m_bHasHeight = TRUE;
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 32:
    case 35:
    case 36:
        // The dart: height flag and empty sound tails.
        pProjectile = new CProjectileDart();
        pProjectile->m_bHasHeight = TRUE;
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 56:
    case 59:
    case 60:
        // The spear: height flag and empty sound tails.
        pProjectile = new CProjectileSpear();
        pProjectile->m_bHasHeight = TRUE;
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 26:
        // The burning-hands stream: the height flag first, then the cell's
        // palette rebound to the projectile's own and range 0 recoloured to
        // 0x43 over the ctor's 71.
        pBam = new CProjectileSPBRNHND();
        pBam->m_bHasHeight = TRUE;
        pBam->m_animation->SetPalette(pBam->m_palette);
        pBam->m_palette.SetRange(0, 0x43, *g_pBaldurChitin->GetObjectGame()->GetMasterBitmap());
        pBam->m_fireSoundRef = "EFF_M20";
        pBam->m_arrivalSoundRef = "";
        pProjectile = pBam;
        break;
    case 4:
        // The plain flame arrow, playing the arrow-launch tail.
        pBam = new CProjectileSPFLMARR();
        pBam->m_bHasHeight = TRUE;
        pBam->m_fireSoundRef = "TRA_24";
        pBam->m_arrivalSoundRef = "";
        pProjectile = pBam;
        break;
    case 9:
        // The flame arrow over the throwing-axe cell.
        pBam = new CProjectileSPFLMARR();
        pBam->SetVidCell(CResRef("AXE"));
        pBam->m_bHasHeight = TRUE;
        pBam->m_fireSoundRef = "TRA_10";
        pBam->m_arrivalSoundRef = "";
        pProjectile = pBam;
        break;
    case 14:
        // The flame arrow over the bolt cell.
        pBam = new CProjectileSPFLMARR();
        pBam->SetVidCell(CResRef("BOLT"));
        pBam->m_bHasHeight = TRUE;
        pBam->m_fireSoundRef = "TRA_10";
        pBam->m_arrivalSoundRef = "";
        pProjectile = pBam;
        break;
    case 29:
        // The flame arrow over the dagger cell.
        pBam = new CProjectileSPFLMARR();
        pBam->SetVidCell(CResRef("DAGGER"));
        pBam->m_bHasHeight = TRUE;
        pBam->m_fireSoundRef = "TRA_10";
        pBam->m_arrivalSoundRef = "";
        pProjectile = pBam;
        break;
    case 34:
        // The flame arrow over the dart cell.
        pBam = new CProjectileSPFLMARR();
        pBam->SetVidCell(CResRef("DART"));
        pBam->m_bHasHeight = TRUE;
        pBam->m_fireSoundRef = "TRA_10";
        pBam->m_arrivalSoundRef = "";
        pProjectile = pBam;
        break;
    case 102:
        // The hottest flame-arrow tint: the cell's palette is rebound to
        // the projectile's own, range 0 recoloured to 0x45 over the ctor's
        // 0x43, and all seven trail colours follow it.
        pFlame = new CProjectileSPFLMARR();
        pFlame->m_animation->SetPalette(pFlame->m_palette);
        pFlame->m_palette.SetRange(0, 0x45, *g_pBaldurChitin->GetObjectGame()->GetMasterBitmap());
        memset(pFlame->m_trailColorRanges, 0x45, sizeof(pFlame->m_trailColorRanges));
        pFlame->m_bHasHeight = TRUE;
        pFlame->m_fireSoundRef = "TRA_10";
        pFlame->m_arrivalSoundRef = "";
        pProjectile = pFlame;
        break;
    case 103:
        // The mid flame-arrow tint: range 0 and the trail colours to 0x44.
        pFlame = new CProjectileSPFLMARR();
        pFlame->m_animation->SetPalette(pFlame->m_palette);
        pFlame->m_palette.SetRange(0, 0x44, *g_pBaldurChitin->GetObjectGame()->GetMasterBitmap());
        memset(pFlame->m_trailColorRanges, 0x44, sizeof(pFlame->m_trailColorRanges));
        pFlame->m_bHasHeight = TRUE;
        pFlame->m_fireSoundRef = "TRA_10";
        pFlame->m_arrivalSoundRef = "";
        pProjectile = pFlame;
        break;
    case 188:
        // The coolest flame-arrow tint: range 0 and the trail colours to
        // 0x47.
        pFlame = new CProjectileSPFLMARR();
        pFlame->m_animation->SetPalette(pFlame->m_palette);
        pFlame->m_palette.SetRange(0, 0x47, *g_pBaldurChitin->GetObjectGame()->GetMasterBitmap());
        memset(pFlame->m_trailColorRanges, 0x47, sizeof(pFlame->m_trailColorRanges));
        pFlame->m_bHasHeight = TRUE;
        pFlame->m_fireSoundRef = "TRA_10";
        pFlame->m_arrivalSoundRef = "";
        pProjectile = pFlame;
        break;
    case 108:
        // The acid blob: height flag and empty sound tails.
        pProjectile = new CProjectileAcidBlob();
        pProjectile->m_bHasHeight = TRUE;
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 192:
        // The acid blob tinted through SetColorRange: the cell's palette is
        // rebound to the projectile's own and range 0 recoloured to 0x56
        // over the ctor's 73, after the height flag.
        pBam = new CProjectileAcidBlob();
        pBam->m_bHasHeight = TRUE;
        pBam->SetColorRange(0, 0x56);
        pBam->m_fireSoundRef = "";
        pBam->m_arrivalSoundRef = "";
        pProjectile = pBam;
        break;
    case 193:
        // The acid blob tinted to 0x6e.
        pBam = new CProjectileAcidBlob();
        pBam->m_bHasHeight = TRUE;
        pBam->SetColorRange(0, 0x6e);
        pBam->m_fireSoundRef = "";
        pBam->m_arrivalSoundRef = "";
        pProjectile = pBam;
        break;
    case 194:
        // The acid blob tinted to 0x1a.
        pBam = new CProjectileAcidBlob();
        pBam->m_bHasHeight = TRUE;
        pBam->SetColorRange(0, 0x1a);
        pBam->m_fireSoundRef = "";
        pBam->m_arrivalSoundRef = "";
        pProjectile = pBam;
        break;
    case 23:
        // Call Lightning's standing bolt, the only shaft with a fixed
        // 20-frame duration instead of the timescale-scaled one.
        pShaft = new CProjectileShaft("CallLiH", "", 0xFF, TRUE, FALSE, 20);
        pShaft->m_fireSoundRef = "EFF_P19";
        pShaft->m_arrivalSoundRef = "";
        pProjectile = pShaft;
        break;
    case 66:
        pShaft = new CProjectileShaft("HMightH", "", 0xFF, TRUE, FALSE,
            CTimerWorld::TIMESCALE_MSEC_PER_SEC * 2);
        pShaft->m_fireSoundRef = "";
        pShaft->m_arrivalSoundRef = "";
        pProjectile = pShaft;
        break;
    case 146:
        // Flame Strike's shaft: the one opaque, non-random shaft.
        pShaft = new CProjectileShaft("FlmStrk", "", 0xFF, FALSE, FALSE,
            CTimerWorld::TIMESCALE_MSEC_PER_SEC * 2);
        pShaft->m_fireSoundRef = "";
        pShaft->m_arrivalSoundRef = "";
        pProjectile = pShaft;
        break;
    case 147:
        pShaft = new CProjectileShaft("HLYMITE", "", 0xFF, TRUE, FALSE,
            CTimerWorld::TIMESCALE_MSEC_PER_SEC * 2);
        pShaft->m_fireSoundRef = "";
        pShaft->m_arrivalSoundRef = "";
        pProjectile = pShaft;
        break;
    case 195:
        // The tinted holy-mite shaft: same construction as 147 with the
        // animation fully tinted before the sound tails are set.
        pShaft = new CProjectileShaft("HLYMITE", "", 0xFF, TRUE, FALSE,
            CTimerWorld::TIMESCALE_MSEC_PER_SEC * 2);
        pShaft->m_animation.SetTintColor(0xFF);
        pShaft->m_fireSoundRef = "EFF_M10";
        pShaft->m_arrivalSoundRef = "";
        pProjectile = pShaft;
        break;
    case 110:
        // The dimension door: the ctor's empty-palette arm runs (no palette
        // BAM), the door animation provides the visuals.
        pDoor = new CProjectile84E940("SPDimDr", "", "", 0, 0);
        pDoor->m_fireSoundRef = "EFF_M09";
        pDoor->m_arrivalSoundRef = "";
        pProjectile = pDoor;
        break;
    case 143:
        // The spell-hit burst with the gold colour range: same construction
        // the Visual Spell Hit modes 0x20..0x22 use.
        pBurst = new CProjectile84DFA4();
        pBurst->field_1D8 = 1;
        pBurst->SetColorRange(0, 0x43);
        pBurst->m_fireSoundRef = "";
        pBurst->m_arrivalSoundRef = "";
        pProjectile = pBurst;
        break;
    case 144:
        // The spell-hit burst with the blue-green colour range.
        pBurst = new CProjectile84DFA4();
        pBurst->field_1D8 = 1;
        pBurst->SetColorRange(0, 0x41);
        pBurst->m_fireSoundRef = "";
        pBurst->m_arrivalSoundRef = "";
        pProjectile = pBurst;
        break;
    case 145:
        // The spell-hit burst with the violet colour range.
        pBurst = new CProjectile84DFA4();
        pBurst->field_1D8 = 1;
        pBurst->SetColorRange(0, 0x45);
        pBurst->m_fireSoundRef = "";
        pBurst->m_arrivalSoundRef = "";
        pProjectile = pBurst;
        break;
    case 98:
        // The ice-storm cloud: the ctor carries the whole description.
        pProjectile = new CProjectileIceStorm();
        break;
    case 211:
        // The acid-storm cloud.
        pProjectile = new CProjectileAcidStorm();
        break;
    case 214:
        // The fiery cloud.
        pProjectile = new CProjectileFieryCloud();
        break;
    case 215:
        // The produce-fire circle.
        pProjectile = new CProjectileProduceFire();
        break;
    case 216:
        // The insect plague.
        pProjectile = new CProjectileInsectPlague();
        break;
    case 267:
        // The circle of death.
        pProjectile = new CProjectileCircleOfDeath();
        break;
    case 279:
        // The corrosive fog.
        pProjectile = new CProjectileCorrosiveFog();
        break;
    case 306:
        // The tremor.
        pProjectile = new CProjectileTremor();
        break;
    case 383:
        // The big boulder.
        pProjectile = new CProjectileBigBoulder();
        break;
    case 249:
        // Hold Animal.
        pArea = new CProjectileHoldAnimal();
        pArea->TargetEnemy(pSprite);
        pProjectile = pArea;
        break;
    case 262:
        // The static-charge area.
        pArea = new CProjectileArea850078();
        pArea->TargetEnemy(pSprite);
        pProjectile = pArea;
        break;
    case 263:
        // Hold Monster.
        pArea = new CProjectileHoldMonster();
        pArea->TargetEnemy(pSprite);
        pProjectile = pArea;
        break;
    case 278:
        // Power Word, Kill.
        pArea = new CProjectilePowerWordKill();
        pArea->TargetEnemy(pSprite);
        pProjectile = pArea;
        break;
    case 357:
        // Halt Undead.
        pArea = new CProjectileHaltUndead();
        pArea->TargetEnemy(pSprite);
        pArea->m_fireSoundRef = CResRef("TRA_04C");
        pArea->m_arrivalSoundRef = CResRef("EFF_M05");
        pProjectile = pArea;
        break;
    case 294:
        // The portal opening.
        pArea = new CProjectilePortalAnimFlip();
        pArea->m_explosion1.m_sound = "PRTL_OP";
        pProjectile = pArea;
        break;
    case 297:
        // The portal closing.
        pArea = new CProjectilePortalAnimFlip();
        pArea->m_explosion1.m_sound = "PRTL_CL";
        pProjectile = pArea;
        break;
    case 209:
        // The rage burst area.
        pArea = new CProjectileArea8504BC(150);
        pArea->m_explosion1.m_animation = "MRageX";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 308:
        // The slow pulse area.
        pArea = new CProjectileArea8504BC(70);
        pArea->m_explosion3.m_sound = "EFF_P101";
        pArea->m_nTriggerInterval = 5;
        pArea->m_nTriggerCountdown = 0;
        pArea->m_nHitModulus = 0x14;
        pArea->m_nDuration = 0x12C;
        pProjectile = pArea;
        break;
    case 311:
        // The fast pulse area.
        pArea = new CProjectileArea8504BC(70);
        pArea->m_nTriggerInterval = 5;
        pArea->m_nTriggerCountdown = 0;
        pArea->m_nHitModulus = 0x14;
        pArea->m_explosion3.m_sound = "EFF_P100";
        pArea->m_nDuration = 0xC8;
        pProjectile = pArea;
        break;
    case 335:
        // The fire-burst area.
        pArea = new CProjectileArea8504BC(250);
        pArea->m_explosion1.m_animation = "FirebaX";
        pArea->m_explosion1.m_sound = "RNG_M03";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pArea->m_explosion2.m_animation = "FirebaR";
        pArea->m_explosion2.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pArea->m_explosion2.field_2C = TRUE;
        pArea->m_explosion2.field_2E = 0x14;
        pArea->m_explosion3.m_animation = "FirebaA";
        pArea->m_explosion3.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pArea->m_explosion3.field_34 = TRUE;
        pArea->m_explosion3.field_35 = TRUE;
        pArea->m_explosion3.field_36 = TRUE;
        pArea->m_nTriggerInterval = 0x2710;
        pArea->m_nTriggerCountdown = 0;
        pArea->m_nHitModulus = 0xA;
        pArea->m_nDuration = 0x2D;
        pProjectile = pArea;
        break;
    case 369:
        // The plain pulse area.
        pArea = new CProjectileArea8504BC(70);
        pProjectile = pArea;
        break;
    case 375:
        // The boulder.
        pProjectile = new CProjectileBoulder();
        pProjectile->m_bHasHeight = TRUE;
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 363:
    case 364:
        // Symbol of death (363/364 share the jump-table entry 0x52831D).
        // The vanilla decoder builds and fully dresses a CProjectileArea(300)
        // here — enemy target, SoHopeX burst on EFF_P50, the copy-from-back
        // flag — and then drops it on the floor when the symbol object is
        // allocated (the mov esi,eax overwrites its only pointer). Ported
        // literally, abandoned dressing included.
        pArea = new CProjectileArea(300);
        pArea->TargetEnemy(pSprite);
        pArea->m_explosion1.m_animation = "SoHopeX";
        pArea->m_explosion1.m_sound = "EFF_P50";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pArea = new CProjectileSymbolOfDeath();
        pArea->TargetEnemy(pSprite);
        pProjectile = pArea;
        break;
    case 365:
        // Symbol of death.
        pArea = new CProjectileSymbolOfDeath();
        pArea->TargetEnemy(pSprite);
        pProjectile = pArea;
        break;
    case 68:
    case 69:
    case 70:
    case 71:
    case 72:
    case 73:
    case 74:
    case 75:
    case 76:
    case 77:
    case 78:
        // Magic Missile, 1 to 11 missiles. NOTE: The WORD at 0x85E84A is 1.
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
    // The follow-up magic-missile barrage (0x534600, CProjectileMAGMISS),
    // one body for types 81 to 91: 1 to 11 shots of type 22 (MAGMISS), and
    // the target type the caster's side picks. NOTE: Unlike case 42 the
    // original dereferences pSprite here with no NULL check.
    case 81:
    case 82:
    case 83:
    case 84:
    case 85:
    case 86:
    case 87:
    case 88:
    case 89:
    case 90:
    case 91:
        pMAGMISS = new CProjectileMAGMISS(nProjectileType - 80, 1);
        {
            CAIObjectType type(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
            if (pSprite->GetAIType().m_nEnemyAlly <= CAIObjectType::EA_NOTEVIL) {
                type.m_nEnemyAlly = CAIObjectType::EA_EVILCUTOFF;
            } else {
                type.m_nEnemyAlly = CAIObjectType::EA_NOTEVIL;
            }
            pMAGMISS->m_targetType.Set(type);
        }
        pProjectile = pMAGMISS;
        break;
    case 92:
        // Fire Storm, Lightning Bolt.
        pProjectile = new CProjectileArea84F6B8();
        break;
    // The wave-puff cones (0x579B40, CProjectile850DDC), one entry per type:
    // each builds its cone on the invisible MMissiT cell (the empty
    // animation argument) with its own puff animation in m_sPuff, arms the
    // visual effect, and sets a fire sound - except the two silent types.
    // The ctor leaves the speed word at 20; some types triple it, double it,
    // or multiply it by 3/2.
    case 22:
        // Burning Hands.
        pPuff = new CProjectile850DDC("", "BHandsT");
        pPuff->m_nWidth = 200;
        pPuff->m_nLength = 80;
        pPuff->field_317 = 0;
        pPuff->m_bRandomPuffFrame = 1;
        pPuff->field_2EE = 16;
        pPuff->m_nPuffSpacing = 50;
        pPuff->m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pPuff->m_fireSoundRef = "TRA_15";
        pProjectile = pPuff;
        break;
    case 25:
        // Cone of Cold.
        pPuff = new CProjectile850DDC("", "CoColdT");
        pPuff->m_nWidth = 200;
        pPuff->m_nLength = 350;
        pPuff->m_bRandomPuffFrame = 1;
        pPuff->m_speed = static_cast<SHORT>(pPuff->m_speed * 3);
        pPuff->m_nPuffSpacing = 50;
        pPuff->m_nFrames = 8;
        pPuff->m_nWaveInterval = 4;
        pPuff->m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pPuff->m_fireSoundRef = "TRA_08";
        pProjectile = pPuff;
        break;
    case 97:
        // Color Spray.
        pPuff = new CProjectile850DDC("", "CSprayT");
        pPuff->m_nWidth = 200;
        pPuff->m_nLength = 200;
        pPuff->m_bRandomPuffFrame = 1;
        pPuff->m_nPuffSpacing = 30;
        pPuff->m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pPuff->m_fireSoundRef = "TRA_16";
        pProjectile = pPuff;
        break;
    case 272:
        // Prismatic Spray.
        pPuff = new CProjectile850DDC("", "PSprayT");
        pPuff->m_nWidth = 175;
        pPuff->m_nLength = 400;
        pPuff->m_bRandomPuffFrame = 1;
        pPuff->m_nPuffSpacing = 30;
        pPuff->m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pPuff->m_fireSoundRef = "TRA_21";
        pProjectile = pPuff;
        break;
    case 296:
        // Cone of Cold (the short cone).
        pPuff = new CProjectile850DDC("", "CoColdT");
        pPuff->m_nWidth = 200;
        pPuff->m_nLength = 120;
        pPuff->m_bRandomPuffFrame = 1;
        pPuff->m_speed = static_cast<SHORT>(pPuff->m_speed * 2);
        pPuff->m_nPuffSpacing = 50;
        pPuff->m_nFrames = 8;
        pPuff->m_nWaveInterval = 4;
        pPuff->m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pPuff->m_fireSoundRef = "TRA_08";
        pProjectile = pPuff;
        break;
    case 303:
        // The TSprayT cone; no fire sound.
        pPuff = new CProjectile850DDC("", "TSprayT");
        pPuff->m_nWidth = 600;
        pPuff->m_nLength = 300;
        pPuff->m_bRandomPuffFrame = 1;
        pPuff->field_317 = 0;
        pPuff->field_2EE = 16;
        pPuff->m_speed = static_cast<SHORT>(pPuff->m_speed * 3);
        pPuff->m_nPuffSpacing = 35;
        pPuff->m_nFrames = 8;
        pPuff->m_nWaveInterval = 4;
        pPuff->m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pPuff;
        break;
    case 315:
        // Shout.
        pPuff = new CProjectile850DDC("", "ShoutT");
        pPuff->m_nWidth = 100;
        pPuff->m_nLength = 300;
        pPuff->field_2EE = 16;
        pPuff->m_speed = static_cast<SHORT>(pPuff->m_speed * 3 / 2);
        pPuff->m_nPuffSpacing = 1000;
        pPuff->m_nPuffSpeed = 75;
        pPuff->m_nPuffSpeedRandom = 0;
        pPuff->m_nFrames = 12;
        pPuff->m_nWaveInterval = 2;
        pPuff->m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pPuff->m_fireSoundRef = "TRA_08";
        pProjectile = pPuff;
        break;
    case 319:
        // The GShoutT cone.
        pPuff = new CProjectile850DDC("", "GShoutT");
        pPuff->m_nWidth = 100;
        pPuff->m_nLength = 300;
        pPuff->field_2EE = 16;
        pPuff->m_speed = static_cast<SHORT>(pPuff->m_speed * 3 / 2);
        pPuff->m_nPuffSpacing = 1000;
        pPuff->m_nPuffSpeed = 75;
        pPuff->m_nPuffSpeedRandom = 0;
        pPuff->m_nFrames = 12;
        pPuff->m_nWaveInterval = 2;
        pPuff->m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pPuff->m_fireSoundRef = "TRA_08";
        pProjectile = pPuff;
        break;
    case 343:
        // The WOWispT cone; no fire sound.
        pPuff = new CProjectile850DDC("", "WOWispT");
        pPuff->m_nWidth = 200;
        pPuff->m_nLength = 200;
        pPuff->m_bRandomPuffFrame = 1;
        pPuff->m_nPuffSpacing = 30;
        pPuff->m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pPuff;
        break;
    case 368:
        // The FFingeT cone.
        pPuff = new CProjectile850DDC("", "FFingeT");
        pPuff->m_nWidth = 200;
        pPuff->m_nLength = 80;
        pPuff->field_317 = 0;
        pPuff->m_bRandomPuffFrame = 1;
        pPuff->field_2EE = 16;
        pPuff->m_nPuffSpacing = 50;
        pPuff->m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pPuff->m_fireSoundRef = "TRA_15";
        pProjectile = pPuff;
        break;
    case 374:
        // Shout (the narrow cone).
        pPuff = new CProjectile850DDC("", "ShoutT");
        pPuff->m_nWidth = 120;
        pPuff->m_nLength = 300;
        pPuff->field_2EE = 16;
        pPuff->m_speed = static_cast<SHORT>(pPuff->m_speed * 3 / 2);
        pPuff->m_nPuffSpacing = 1000;
        pPuff->m_nPuffSpeed = 75;
        pPuff->m_nPuffSpeedRandom = 0;
        pPuff->m_nFrames = 12;
        pPuff->m_nWaveInterval = 2;
        pPuff->m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pPuff->m_fireSoundRef = "TRA_08";
        pProjectile = pPuff;
        break;
    case 379:
        // The HDFBreT cone.
        pPuff = new CProjectile850DDC("", "HDFBreT");
        pPuff->m_nWidth = 300;
        pPuff->m_nLength = 300;
        pPuff->m_bRandomPuffFrame = 1;
        pPuff->m_speed = static_cast<SHORT>(pPuff->m_speed * 3);
        pPuff->m_nPuffSpacing = 50;
        pPuff->m_nFrames = 8;
        pPuff->m_nWaveInterval = 4;
        pPuff->m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pPuff->m_fireSoundRef = "TRA_08";
        pProjectile = pPuff;
        break;
    case 382:
        // The GShoutT cone (the narrow cone).
        pPuff = new CProjectile850DDC("", "GShoutT");
        pPuff->m_nWidth = 120;
        pPuff->m_nLength = 300;
        pPuff->field_2EE = 16;
        pPuff->m_speed = static_cast<SHORT>(pPuff->m_speed * 3 / 2);
        pPuff->m_nPuffSpacing = 1000;
        pPuff->m_nPuffSpeed = 75;
        pPuff->m_nPuffSpeedRandom = 0;
        pPuff->m_nFrames = 12;
        pPuff->m_nWaveInterval = 2;
        pPuff->m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pPuff->m_fireSoundRef = "TRA_08";
        pProjectile = pPuff;
        break;
    case 386:
        // The HDFBreT cone (the short cone).
        pPuff = new CProjectile850DDC("", "HDFBreT");
        pPuff->m_nWidth = 150;
        pPuff->m_nLength = 150;
        pPuff->m_bRandomPuffFrame = 1;
        pPuff->m_speed = static_cast<SHORT>(pPuff->m_speed * 3);
        pPuff->m_nPuffSpacing = 50;
        pPuff->m_nFrames = 8;
        pPuff->m_nWaveInterval = 4;
        pPuff->m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pPuff->m_fireSoundRef = "TRA_08";
        pProjectile = pPuff;
        break;
    // The MFMissT barrages (0x5309C0, CProjectile84E030): one body for types
    // 324 to 334 - 1 to 11 missiles of child type 314 - and type 345's seven
    // of child type 344, all on the MFMissT cell. The palette type is the
    // WORD 1 at 0x85E84A. The children are decoded with pSprite NULL; type
    // 314 falls to the default path's fallback until its family is wired
    // (344 wires to the MFMissT single-target below).
    case 324:
    case 325:
    case 326:
    case 327:
    case 328:
    case 329:
    case 330:
    case 331:
    case 332:
    case 333:
    case 334:
        pProjectile = new CProjectile84E030(313, "MFMissT", nProjectileType - 323, 1);
        pProjectile->m_fireSoundRef = "TRA_55";
        pProjectile->m_arrivalSoundRef = "";
        pProjectile->m_bHasHeight = TRUE;
        break;
    case 345:
        pProjectile = new CProjectile84E030(343, "MFMissT", 7, 1);
        pProjectile->m_fireSoundRef = "TRA_55";
        pProjectile->m_arrivalSoundRef = "";
        pProjectile->m_bHasHeight = TRUE;
        break;
    // The spell-hit cells (0x57E490, CProjectileSpellHit): one animation
    // played where the spell lands, its effects delivered the moment it
    // fires. DecodeProjectile's prologue default-constructs the shared
    // visual effect (0x586A40); every entry but type 361 arms it
    // (the 0x586A90 arm call, m_dwFlags 0x20200) before the ctor copies it, so
    // GateX keeps the bare default (0x20000). The five summoning and
    // elemental hits also take the EFF_M13 arrival sound and draw behind
    // sprites.
    case 289:
        // The MSumm1X hit.
        visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pSpellHit = new CProjectileSpellHit("MSumm1X", visualEffect);
        pSpellHit->m_arrivalSoundRef = "EFF_M13";
        pSpellHit->m_bDrawBehind = TRUE;
        pProjectile = pSpellHit;
        break;
    case 290:
        // The ASumm1X hit.
        visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pSpellHit = new CProjectileSpellHit("ASumm1X", visualEffect);
        pSpellHit->m_arrivalSoundRef = "EFF_M13";
        pSpellHit->m_bDrawBehind = TRUE;
        pProjectile = pSpellHit;
        break;
    case 291:
        // The CEElemX hit.
        visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pSpellHit = new CProjectileSpellHit("CEElemX", visualEffect);
        pSpellHit->m_arrivalSoundRef = "EFF_M13";
        pSpellHit->m_bDrawBehind = TRUE;
        pProjectile = pSpellHit;
        break;
    case 292:
        // The CFElemX hit.
        visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pSpellHit = new CProjectileSpellHit("CFElemX", visualEffect);
        pSpellHit->m_arrivalSoundRef = "EFF_M13";
        pSpellHit->m_bDrawBehind = TRUE;
        pProjectile = pSpellHit;
        break;
    case 293:
        // The CWElemX hit.
        visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pSpellHit = new CProjectileSpellHit("CWElemX", visualEffect);
        pSpellHit->m_arrivalSoundRef = "EFF_M13";
        pSpellHit->m_bDrawBehind = TRUE;
        pProjectile = pSpellHit;
        break;
    case 347:
        // The PortalH hit.
        visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = new CProjectileSpellHit("PortalH", visualEffect);
        break;
    case 353:
        // The IllusH hit.
        visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = new CProjectileSpellHit("IllusH", visualEffect);
        break;
    case 354:
        // The CCDamaH hit.
        visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = new CProjectileSpellHit("CCDamaH", visualEffect);
        break;
    case 361:
        // The GateX hit: bare default visual effect, no 0x586A90 arm call.
        pProjectile = new CProjectileSpellHit("GateX", visualEffect);
        break;
    // The single-target missiles (0x578110, CProjectileSingleTarget): one
    // animation flying to one target. Every entry builds a CResRef for its
    // cell and hands it to the ctor by value; the object's own visual
    // effect at 0x2A2 is armed afterwards (the 0x586A90 call) except for
    // MSporeT, which instead displays effect feedback 4386 off pSprite
    // before constructing and leaves the effect bare. Types 269 and 271
    // share their OFSpheT tail block byte for byte.
    case 24:
        // The COrbT missile.
        pSingle = new CProjectileSingleTarget(CResRef("COrbT"));
        pSingle->m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pSingle->m_bHasHeight = TRUE;
        pSingle->m_fireSoundRef = "TRA_23";
        pSingle->m_arrivalSoundRef = "";
        pProjectile = pSingle;
        break;
    case 251:
        // The IcelanT missile.
        pSingle = new CProjectileSingleTarget(CResRef("IcelanT"));
        pSingle->m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pSingle->field_1D8 = 0x10;
        pSingle->m_fireSoundRef = "TRA_19";
        pProjectile = pSingle;
        break;
    case 268:
        // The DisintT missile.
        pSingle = new CProjectileSingleTarget(CResRef("DisintT"));
        pSingle->m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pSingle->field_1D8 = 0x10;
        pSingle->m_fireSoundRef = "TRA_20";
        pProjectile = pSingle;
        break;
    case 269:
    case 271:
        // The OFSpheT missiles: one shared body for both types.
        pSingle = new CProjectileSingleTarget(CResRef("OFSpheT"));
        pSingle->m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pSingle->field_1D8 = 0x10;
        pProjectile = pSingle;
        break;
    case 285:
        // The MSporeT missile: feedback text first, bare visual effect.
        IcewindMisc::sub_584880(static_cast<CGameSprite*>(pSprite), 4386, 0); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = new CProjectileSingleTarget(CResRef("MSporeT"));
        break;
    case 298:
        // The ALanceT missile.
        pSingle = new CProjectileSingleTarget(CResRef("ALanceT"));
        pSingle->m_speed = static_cast<SHORT>(pSingle->m_speed * 2);
        pSingle->m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pSingle->field_1D8 = 0x10;
        pSingle->field_2A0 = 0;
        pSingle->m_fireSoundRef = "TRA_57";
        pProjectile = pSingle;
        break;
    case 316:
        // The VSpherT missile.
        pSingle = new CProjectileSingleTarget(CResRef("VSpherT"));
        pSingle->m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pSingle->m_speed = static_cast<SHORT>(pSingle->m_speed * 2);
        pSingle->field_1D8 = 1;
        pSingle->m_bHasHeight = TRUE;
        pSingle->field_1C2 = TRUE;
        pSingle->m_fireSoundRef = "TRA_60";
        pSingle->m_arrivalSoundRef = "";
        pProjectile = pSingle;
        break;
    case 344:
        // The MFMissT missile (type 345's barrage children decode here).
        pSingle = new CProjectileSingleTarget(CResRef("MFMissT"));
        pSingle->m_speed = static_cast<SHORT>(pSingle->m_speed * 2);
        pSingle->m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pSingle->m_bHasHeight = TRUE;
        pSingle->field_1C2 = TRUE;
        pProjectile = pSingle;
        break;
    // The sparkle-stream missiles (0x52CA10, CProjectileSparkle): the
    // travelling spell bolt. Every entry constructs with the palette type
    // read from the WORD at 0x85E848, which is 0 — no recolour — then,
    // except type 37, swaps the cell to TRAVEL, and, except types 51 to 53,
    // arms the alternate palette bitmap (0x529660) before the sparkle tail
    // (m_bSparkle, m_nSparkleColour, m_bHasHeight) and the two sound refs.
    // Types 47 to 55 and 184/185 are the eight STTRAVL colours plus the
    // three plain (colour-only) bolts; 65 doubles the flight speed once
    // more, on top of the ctor's own doubling.
    case 37:
        // The bare bolt: no TRAVEL swap, no sparkle tail.
        pSparkle = new CProjectileSparkle(0);
        pSparkle->m_bHasHeight = TRUE;
        pSparkle->m_fireSoundRef = "TRA_02";
        pSparkle->m_arrivalSoundRef = "";
        pProjectile = pSparkle;
        break;
    case 47:
        // Colour 2.
        pSparkle = new CProjectileSparkle(0);
        pSparkle->SetVidCell(CResRef("TRAVEL"));
        pSparkle->SetAltPalette("STTRAVL1");
        pSparkle->m_bSparkle = TRUE;
        pSparkle->m_nSparkleColour = 2;
        pSparkle->m_bHasHeight = TRUE;
        pSparkle->m_fireSoundRef = "TRA_04A";
        pSparkle->m_arrivalSoundRef = "";
        pProjectile = pSparkle;
        break;
    case 48:
        // Colour 4.
        pSparkle = new CProjectileSparkle(0);
        pSparkle->SetVidCell(CResRef("TRAVEL"));
        pSparkle->SetAltPalette("STTRAVL2");
        pSparkle->m_bSparkle = TRUE;
        pSparkle->m_nSparkleColour = 4;
        pSparkle->m_bHasHeight = TRUE;
        pSparkle->m_fireSoundRef = "TRA_04C";
        pSparkle->m_arrivalSoundRef = "";
        pProjectile = pSparkle;
        break;
    case 49:
        // Colour 6.
        pSparkle = new CProjectileSparkle(0);
        pSparkle->SetVidCell(CResRef("TRAVEL"));
        pSparkle->SetAltPalette("STTRAVL5");
        pSparkle->m_bSparkle = TRUE;
        pSparkle->m_nSparkleColour = 6;
        pSparkle->m_bHasHeight = TRUE;
        pSparkle->m_fireSoundRef = "";
        pSparkle->m_arrivalSoundRef = "";
        pProjectile = pSparkle;
        break;
    case 50:
        // Colour 9.
        pSparkle = new CProjectileSparkle(0);
        pSparkle->SetVidCell(CResRef("TRAVEL"));
        pSparkle->SetAltPalette("STTRAVL7");
        pSparkle->m_bSparkle = TRUE;
        pSparkle->m_nSparkleColour = 9;
        pSparkle->m_bHasHeight = TRUE;
        pSparkle->m_fireSoundRef = "TRA_01";
        pSparkle->m_arrivalSoundRef = "";
        pProjectile = pSparkle;
        break;
    case 51:
        // Colour 10, no alternate palette.
        pSparkle = new CProjectileSparkle(0);
        pSparkle->SetVidCell(CResRef("TRAVEL"));
        pSparkle->m_bSparkle = TRUE;
        pSparkle->m_nSparkleColour = 0xA;
        pSparkle->m_bHasHeight = TRUE;
        pSparkle->m_fireSoundRef = "";
        pSparkle->m_arrivalSoundRef = "";
        pProjectile = pSparkle;
        break;
    case 52:
        // Colour 1, no alternate palette.
        pSparkle = new CProjectileSparkle(0);
        pSparkle->SetVidCell(CResRef("TRAVEL"));
        pSparkle->m_bSparkle = TRUE;
        pSparkle->m_nSparkleColour = 1;
        pSparkle->m_bHasHeight = TRUE;
        pSparkle->m_fireSoundRef = "";
        pSparkle->m_arrivalSoundRef = "";
        pProjectile = pSparkle;
        break;
    case 53:
        // Colour 3, no alternate palette.
        pSparkle = new CProjectileSparkle(0);
        pSparkle->SetVidCell(CResRef("TRAVEL"));
        pSparkle->m_bSparkle = TRUE;
        pSparkle->m_nSparkleColour = 3;
        pSparkle->m_bHasHeight = TRUE;
        pSparkle->m_fireSoundRef = "";
        pSparkle->m_arrivalSoundRef = "";
        pProjectile = pSparkle;
        break;
    case 54:
        // Colour 7.
        pSparkle = new CProjectileSparkle(0);
        pSparkle->SetVidCell(CResRef("TRAVEL"));
        pSparkle->SetAltPalette("STTRAVL6");
        pSparkle->m_bSparkle = TRUE;
        pSparkle->m_nSparkleColour = 7;
        pSparkle->m_bHasHeight = TRUE;
        pSparkle->m_fireSoundRef = "";
        pSparkle->m_arrivalSoundRef = "";
        pProjectile = pSparkle;
        break;
    case 55:
        // Colour 5.
        pSparkle = new CProjectileSparkle(0);
        pSparkle->SetVidCell(CResRef("TRAVEL"));
        pSparkle->SetAltPalette("STTRAVL3");
        pSparkle->m_bSparkle = TRUE;
        pSparkle->m_nSparkleColour = 5;
        pSparkle->m_bHasHeight = TRUE;
        pSparkle->m_fireSoundRef = "TRA_04D";
        pSparkle->m_arrivalSoundRef = "";
        pProjectile = pSparkle;
        break;
    case 65:
        // Colour 10: field_1DE re-cleared after the ctor already cleared
        // it, and the flight speed doubled once more.
        pSparkle = new CProjectileSparkle(0);
        pSparkle->SetVidCell(CResRef("TRAVEL"));
        pSparkle->SetAltPalette("STTRAVL2");
        pSparkle->field_1DE = FALSE;
        pSparkle->m_bSparkle = TRUE;
        pSparkle->m_nSparkleColour = 0xA;
        pSparkle->m_bHasHeight = TRUE;
        pSparkle->m_speed = static_cast<SHORT>(pSparkle->m_speed * 2);
        pSparkle->m_fireSoundRef = "TRA_04C";
        pSparkle->m_arrivalSoundRef = "";
        pProjectile = pSparkle;
        break;
    case 184:
        // Colour 11.
        pSparkle = new CProjectileSparkle(0);
        pSparkle->SetVidCell(CResRef("TRAVEL"));
        pSparkle->SetAltPalette("STTRAVL4");
        pSparkle->m_bSparkle = TRUE;
        pSparkle->m_nSparkleColour = 0xB;
        pSparkle->m_bHasHeight = TRUE;
        pSparkle->m_fireSoundRef = "TRA_04A";
        pSparkle->m_arrivalSoundRef = "";
        pProjectile = pSparkle;
        break;
    case 185:
        // Colour 12.
        pSparkle = new CProjectileSparkle(0);
        pSparkle->SetVidCell(CResRef("TRAVEL"));
        pSparkle->SetAltPalette("STTRAVL8");
        pSparkle->m_bSparkle = TRUE;
        pSparkle->m_nSparkleColour = 0xC;
        pSparkle->m_bHasHeight = TRUE;
        pSparkle->m_fireSoundRef = "TRA_03";
        pSparkle->m_arrivalSoundRef = "";
        pProjectile = pSparkle;
        break;
    // The sparkle-burst missiles (0x5297B0, CProjectileSparkleExplosion):
    // the delayed-burst carrier built on the exploding intermediate, with
    // TRAVEL handed to the ctor as the cell (no post-construct swap, unlike
    // the sparkle-stream family). Every entry overwrites the ctor's child
    // projectile ([+0x2A4], 78) with the burst's own, then arms the sparkle
    // tail (m_bSparkle, m_nSparkleColour, m_bHasHeight), sets the trails
    // flag (field_E6 |= 8 — load-bearing here, since unlike the
    // sparkle-stream ctor the exploding ctor chain leaves field_E6 at
    // 0x20000), and the two sound refs — the arrival ref is a real sound
    // here, not the empty string. Types 153 to 155 skip the alternate
    // palette; 182 reuses 157's child (54) with colour 11.
    case 149:
        // Colour 2, child 46.
        pSparkleExplosion = new CProjectileSparkleExplosion(CResRef("TRAVEL"));
        pSparkleExplosion->SetAltPalette("STTRAVL1");
        pSparkleExplosion->m_bSparkle = TRUE;
        pSparkleExplosion->m_nSparkleColour = 2;
        pSparkleExplosion->m_childProjectileType = 0x2E;
        pSparkleExplosion->m_bHasHeight = TRUE;
        pSparkleExplosion->field_E6 |= 8;
        pSparkleExplosion->m_fireSoundRef = "TRA_04A";
        pSparkleExplosion->m_arrivalSoundRef = "CAS_M08";
        pProjectile = pSparkleExplosion;
        break;
    case 150:
        // Colour 4, child 47.
        pSparkleExplosion = new CProjectileSparkleExplosion(CResRef("TRAVEL"));
        pSparkleExplosion->SetAltPalette("STTRAVL2");
        pSparkleExplosion->m_bSparkle = TRUE;
        pSparkleExplosion->m_nSparkleColour = 4;
        pSparkleExplosion->m_childProjectileType = 0x2F;
        pSparkleExplosion->m_bHasHeight = TRUE;
        pSparkleExplosion->field_E6 |= 8;
        pSparkleExplosion->m_fireSoundRef = "TRA_04C";
        pSparkleExplosion->m_arrivalSoundRef = "EFF_M05";
        pProjectile = pSparkleExplosion;
        break;
    case 151:
        // Colour 6, child 48.
        pSparkleExplosion = new CProjectileSparkleExplosion(CResRef("TRAVEL"));
        pSparkleExplosion->SetAltPalette("STTRAVL5");
        pSparkleExplosion->m_bSparkle = TRUE;
        pSparkleExplosion->m_nSparkleColour = 6;
        pSparkleExplosion->m_childProjectileType = 0x30;
        pSparkleExplosion->m_bHasHeight = TRUE;
        pSparkleExplosion->field_E6 |= 8;
        pSparkleExplosion->m_fireSoundRef = "TRA_04B";
        pSparkleExplosion->m_arrivalSoundRef = "CAS_P03";
        pProjectile = pSparkleExplosion;
        break;
    case 152:
        // Colour 9, child 49. Both sound refs are the empty string.
        pSparkleExplosion = new CProjectileSparkleExplosion(CResRef("TRAVEL"));
        pSparkleExplosion->SetAltPalette("STTRAVL7");
        pSparkleExplosion->m_bSparkle = TRUE;
        pSparkleExplosion->m_nSparkleColour = 9;
        pSparkleExplosion->m_childProjectileType = 0x31;
        pSparkleExplosion->m_bHasHeight = TRUE;
        pSparkleExplosion->field_E6 |= 8;
        pSparkleExplosion->m_fireSoundRef = "";
        pSparkleExplosion->m_arrivalSoundRef = "";
        pProjectile = pSparkleExplosion;
        break;
    case 153:
        // Colour 10, child 50; no alternate palette.
        pSparkleExplosion = new CProjectileSparkleExplosion(CResRef("TRAVEL"));
        pSparkleExplosion->m_bSparkle = TRUE;
        pSparkleExplosion->m_nSparkleColour = 0xA;
        pSparkleExplosion->m_childProjectileType = 0x32;
        pSparkleExplosion->m_bHasHeight = TRUE;
        pSparkleExplosion->field_E6 |= 8;
        pSparkleExplosion->m_fireSoundRef = "";
        pSparkleExplosion->m_arrivalSoundRef = "";
        pProjectile = pSparkleExplosion;
        break;
    case 154:
        // Colour 1, child 51; no alternate palette. The exe holds one 1 in
        // eax and feeds it to all three stores at once (sparkle, colour,
        // has-height); ours spells the literals out.
        pSparkleExplosion = new CProjectileSparkleExplosion(CResRef("TRAVEL"));
        pSparkleExplosion->m_bSparkle = TRUE;
        pSparkleExplosion->m_nSparkleColour = 1;
        pSparkleExplosion->m_childProjectileType = 0x33;
        pSparkleExplosion->m_bHasHeight = TRUE;
        pSparkleExplosion->field_E6 |= 8;
        pSparkleExplosion->m_fireSoundRef = "";
        pSparkleExplosion->m_arrivalSoundRef = "";
        pProjectile = pSparkleExplosion;
        break;
    case 155:
        // Colour 3, child 52; no alternate palette.
        pSparkleExplosion = new CProjectileSparkleExplosion(CResRef("TRAVEL"));
        pSparkleExplosion->m_bSparkle = TRUE;
        pSparkleExplosion->m_nSparkleColour = 3;
        pSparkleExplosion->m_childProjectileType = 0x34;
        pSparkleExplosion->m_bHasHeight = TRUE;
        pSparkleExplosion->field_E6 |= 8;
        pSparkleExplosion->m_fireSoundRef = "";
        pSparkleExplosion->m_arrivalSoundRef = "";
        pProjectile = pSparkleExplosion;
        break;
    case 156:
        // Colour 7, child 53.
        pSparkleExplosion = new CProjectileSparkleExplosion(CResRef("TRAVEL"));
        pSparkleExplosion->SetAltPalette("STTRAVL6");
        pSparkleExplosion->m_bSparkle = TRUE;
        pSparkleExplosion->m_nSparkleColour = 7;
        pSparkleExplosion->m_childProjectileType = 0x35;
        pSparkleExplosion->m_bHasHeight = TRUE;
        pSparkleExplosion->field_E6 |= 8;
        pSparkleExplosion->m_fireSoundRef = "";
        pSparkleExplosion->m_arrivalSoundRef = "";
        pProjectile = pSparkleExplosion;
        break;
    case 157:
        // Colour 5, child 54.
        pSparkleExplosion = new CProjectileSparkleExplosion(CResRef("TRAVEL"));
        pSparkleExplosion->SetAltPalette("STTRAVL3");
        pSparkleExplosion->m_bSparkle = TRUE;
        pSparkleExplosion->m_nSparkleColour = 5;
        pSparkleExplosion->m_childProjectileType = 0x36;
        pSparkleExplosion->m_bHasHeight = TRUE;
        pSparkleExplosion->field_E6 |= 8;
        pSparkleExplosion->m_fireSoundRef = "TRA_04A";
        pSparkleExplosion->m_arrivalSoundRef = "CAS_P02";
        pProjectile = pSparkleExplosion;
        break;
    case 182:
        // Colour 11, child 54 — the same child as 157.
        pSparkleExplosion = new CProjectileSparkleExplosion(CResRef("TRAVEL"));
        pSparkleExplosion->SetAltPalette("STTRAVL4");
        pSparkleExplosion->m_bSparkle = TRUE;
        pSparkleExplosion->m_nSparkleColour = 0xB;
        pSparkleExplosion->m_childProjectileType = 0x36;
        pSparkleExplosion->m_bHasHeight = TRUE;
        pSparkleExplosion->field_E6 |= 8;
        pSparkleExplosion->m_fireSoundRef = "";
        pSparkleExplosion->m_arrivalSoundRef = "";
        pProjectile = pSparkleExplosion;
        break;
    case 183:
        // Colour 12, child 184.
        pSparkleExplosion = new CProjectileSparkleExplosion(CResRef("TRAVEL"));
        pSparkleExplosion->SetAltPalette("STTRAVL8");
        pSparkleExplosion->m_bSparkle = TRUE;
        pSparkleExplosion->m_nSparkleColour = 0xC;
        pSparkleExplosion->m_childProjectileType = 0xB8;
        pSparkleExplosion->m_bHasHeight = TRUE;
        pSparkleExplosion->field_E6 |= 8;
        pSparkleExplosion->m_fireSoundRef = "TRA_03";
        pSparkleExplosion->m_arrivalSoundRef = "EFF_M04";
        pProjectile = pSparkleExplosion;
        break;
    // The bare exploding missiles (0x52CCE0, CProjectileExploding, one
    // entry per type): every type allocates 0x4B2 bytes and runs the
    // exploding ctor on the "MAGMISS" cell, then only tweaks what it needs.
    // The child re-stores of 0x4E in 94/158/159/205 and the empty-string
    // sound stores in 93 re-write what the ctor already wrote — ported
    // exactly. Types 158 and 159 narrow the exploding target type by the
    // caster's EA against the GOODCUTOFF global 0x847C3B (30): both arms
    // of the branch store one — 158 picks EA_GOODCUTOFF at-or-below and
    // EA_NOTGOOD above (159 the reverse, its other arm reading 0x847C3D,
    // 31). A NULL sprite skips the pick (the ctor's ANYONE target type
    // stays).
    case 93:
        // Child 22 — the only type here whose field_1DE stays TRUE.
        pExploding = new CProjectileExploding(CResRef("MAGMISS"));
        pExploding->m_childProjectileType = 0x16;
        pExploding->m_fireSoundRef = "";
        pExploding->m_arrivalSoundRef = "";
        pProjectile = pExploding;
        break;
    case 94:
        pExploding = new CProjectileExploding(CResRef("MAGMISS"));
        pExploding->m_childProjectileType = 0x4E;
        pExploding->field_1DE = FALSE;
        pProjectile = pExploding;
        break;
    case 158:
        pExploding = new CProjectileExploding(CResRef("MAGMISS"));
        pExploding->m_childProjectileType = 0x4E;
        pExploding->field_1DE = FALSE;
        if (pSprite != NULL) {
            CAIObjectType type(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
            if (pSprite->GetAIType().m_nEnemyAlly <= CAIObjectType::EA_GOODCUTOFF) {
                type.m_nEnemyAlly = CAIObjectType::EA_GOODCUTOFF;
            } else {
                type.m_nEnemyAlly = CAIObjectType::EA_NOTGOOD;
            }
            pExploding->m_targetType.Set(type);
        }
        pProjectile = pExploding;
        break;
    case 159:
        // The guard still reads EA_GOODCUTOFF; the arms are 159's reverse
        // of 158's: NOTGOOD at-or-below, GOODCUTOFF above.
        pExploding = new CProjectileExploding(CResRef("MAGMISS"));
        pExploding->m_childProjectileType = 0x4E;
        pExploding->field_1DE = FALSE;
        if (pSprite != NULL) {
            CAIObjectType type(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
            if (pSprite->GetAIType().m_nEnemyAlly <= CAIObjectType::EA_GOODCUTOFF) {
                type.m_nEnemyAlly = CAIObjectType::EA_NOTGOOD;
            } else {
                type.m_nEnemyAlly = CAIObjectType::EA_GOODCUTOFF;
            }
            pExploding->m_targetType.Set(type);
        }
        pProjectile = pExploding;
        break;
    case 205:
        // Strike ranges tightened from the ctor's 0x100 to 0x50.
        pExploding = new CProjectileExploding(CResRef("MAGMISS"));
        pExploding->m_childProjectileType = 0x4E;
        pExploding->field_1DE = FALSE;
        pExploding->m_strikeRange = 0x50;
        pExploding->m_preCheckRange = 0x50;
        pProjectile = pExploding;
        break;
    // The recursing bolt wrappers (six entries in the exe, one body each):
    // every type re-decodes an already-wired bolt as its carrier and most
    // then retarget it — the same two-armed pick as 158/159 (the guard
    // always reads the GOODCUTOFF global 0x847C3B; both arms store one,
    // only the stored EA differs), applied to the RESULT's exploding
    // target type (+0x2B4). 160-168 and 169-177 offset their own type
    // (-11 / -20, both landing in the sparkle bursts 149-157); 178/180
    // re-decode the fixed bolt 182, 179/181 the bolt 183; 186 re-decodes
    // 94 with no pick but sets m_bCheckNonSprites (+0x2F2, an int) on the
    // result.
    //
    // NOTE: the original writes r+0x2B4 and r+0x2F2 with no type check and
    // no NULL check on the decoded result — every carrier these re-decode
    // (149-157 and 182/183 = CProjectileSparkleExplosion, 0x5297B0; 94 =
    // CProjectileExploding, 0x52CCE0) IS-A CProjectileExploding, so the
    // downcast here is static. The pSprite NULL check stays (the original
    // tests the sprite before reading its EA).
    case 160:
    case 161:
    case 162:
    case 163:
    case 164:
    case 165:
    case 166:
    case 167:
    case 168:
        pProjectile = DecodeProjectile(nProjectileType - 11, pSprite, 0);
        pPick = static_cast<CProjectileExploding*>(pProjectile);
        if (pSprite != NULL) {
            CAIObjectType type(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
            if (pSprite->GetAIType().m_nEnemyAlly <= CAIObjectType::EA_GOODCUTOFF) {
                type.m_nEnemyAlly = CAIObjectType::EA_GOODCUTOFF;
            } else {
                type.m_nEnemyAlly = CAIObjectType::EA_NOTGOOD;
            }
            pPick->m_targetType.Set(type);
        }
        break;
    case 169:
    case 170:
    case 171:
    case 172:
    case 173:
    case 174:
    case 175:
    case 176:
    case 177:
        pProjectile = DecodeProjectile(nProjectileType - 20, pSprite, 0);
        pPick = static_cast<CProjectileExploding*>(pProjectile);
        if (pSprite != NULL) {
            CAIObjectType type(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
            if (pSprite->GetAIType().m_nEnemyAlly <= CAIObjectType::EA_GOODCUTOFF) {
                type.m_nEnemyAlly = CAIObjectType::EA_NOTGOOD;
            } else {
                type.m_nEnemyAlly = CAIObjectType::EA_GOODCUTOFF;
            }
            pPick->m_targetType.Set(type);
        }
        break;
    case 178:
    case 179:
        // Entries 0x524260/0x524342: fixed bolts 182/183, NOTGOOD below /
        // GOODCUTOFF above.
        pProjectile = DecodeProjectile(nProjectileType == 178 ? 182 : 183, pSprite, 0);
        pPick = static_cast<CProjectileExploding*>(pProjectile);
        if (pSprite != NULL) {
            CAIObjectType type(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
            if (pSprite->GetAIType().m_nEnemyAlly <= CAIObjectType::EA_GOODCUTOFF) {
                type.m_nEnemyAlly = CAIObjectType::EA_NOTGOOD;
            } else {
                type.m_nEnemyAlly = CAIObjectType::EA_GOODCUTOFF;
            }
            pPick->m_targetType.Set(type);
        }
        break;
    case 180:
    case 181:
        // Entries 0x524424/0x524506: fixed bolts 182/183, GOODCUTOFF below /
        // NOTGOOD above.
        pProjectile = DecodeProjectile(nProjectileType == 180 ? 182 : 183, pSprite, 0);
        pPick = static_cast<CProjectileExploding*>(pProjectile);
        if (pSprite != NULL) {
            CAIObjectType type(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
            if (pSprite->GetAIType().m_nEnemyAlly <= CAIObjectType::EA_GOODCUTOFF) {
                type.m_nEnemyAlly = CAIObjectType::EA_GOODCUTOFF;
            } else {
                type.m_nEnemyAlly = CAIObjectType::EA_NOTGOOD;
            }
            pPick->m_targetType.Set(type);
        }
        break;
    case 186:
        // Entry 0x5245E8: re-decode 94, then set m_bCheckNonSprites only
        // (the +0x2F2 write above — no type or NULL check in the original).
        pProjectile = DecodeProjectile(94, pSprite, 0);
        pPick = static_cast<CProjectileExploding*>(pProjectile);
        pPick->m_bCheckNonSprites = 1;
        break;
    case 95:
        // Stinking Cloud.
        pProjectile = new CProjectileArea8501B0();
        break;
    case 101:
        // Grease.
        pProjectile = new CProjectileArea84F7F0();
        break;
    // The elemental spell-hit visuals (0x535F20, CProjectile84E9C0): an
    // animation, an empty sound, an optional palette tint, flags 8, trans 0;
    // both sound refs are then cleared.
    case 119:
        // Air.
        pProjectile = new CProjectile84E9C0("SHAir", "", "", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 120:
        // Earth.
        pProjectile = new CProjectile84E9C0("SHEarth", "", "", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 121:
        // Water.
        pProjectile = new CProjectile84E9C0("SHWater", "", "", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 122:
        // Air, palette 1.
        pProjectile = new CProjectile84E9C0("SHAir", "", "SHAir1", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 123:
        // Earth, palette 1.
        pProjectile = new CProjectile84E9C0("SHEarth", "", "SHEarth1", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 124:
        // Water, palette 1.
        pProjectile = new CProjectile84E9C0("SHWater", "", "SHWater1", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 125:
        // Air, palette 2.
        pProjectile = new CProjectile84E9C0("SHAir", "", "SHAir2", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 126:
        // Earth, palette 2.
        pProjectile = new CProjectile84E9C0("SHEarth", "", "SHEarth2", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 127:
        // Water, palette 2.
        pProjectile = new CProjectile84E9C0("SHWater", "", "SHWater2", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 128:
        // Air, palette 3.
        pProjectile = new CProjectile84E9C0("SHAir", "", "SHAir3", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 129:
        // Earth, palette 3.
        pProjectile = new CProjectile84E9C0("SHEarth", "", "SHEarth3", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 130:
        // Water, palette 3.
        pProjectile = new CProjectile84E9C0("SHWater", "", "SHWater3", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 131:
        // Air, palette 4.
        pProjectile = new CProjectile84E9C0("SHAir", "", "SHAir4", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 132:
        // Earth, palette 4.
        pProjectile = new CProjectile84E9C0("SHEarth", "", "SHEarth4", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 133:
        // Water, palette 4.
        pProjectile = new CProjectile84E9C0("SHWater", "", "SHWater4", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 134:
        // Air, palette 5.
        pProjectile = new CProjectile84E9C0("SHAir", "", "SHAir5", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 135:
        // Earth, palette 5.
        pProjectile = new CProjectile84E9C0("SHEarth", "", "SHEarth5", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 136:
        // Water, palette 5.
        pProjectile = new CProjectile84E9C0("SHWater", "", "SHWater5", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 137:
        // Air, palette 6.
        pProjectile = new CProjectile84E9C0("SHAir", "", "SHAir6", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 138:
        // Earth, palette 6.
        pProjectile = new CProjectile84E9C0("SHEarth", "", "SHEarth6", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 139:
        // Water, palette 6.
        pProjectile = new CProjectile84E9C0("SHWater", "", "SHWater6", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 140:
        // Air, palette 7.
        pProjectile = new CProjectile84E9C0("SHAir", "", "SHAir7", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 141:
        // Earth, palette 7.
        pProjectile = new CProjectile84E9C0("SHEarth", "", "SHEarth7", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 142:
        // Water, palette 7.
        pProjectile = new CProjectile84E9C0("SHWater", "", "SHWater7", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 148:
        // Air, palette 7 again.
        pProjectile = new CProjectile84E9C0("SHAir", "", "SHAir7", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
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
    // The area spell-hit visuals (0x535F20, CProjectile84E9C0), built like
    // the elemental ones above.
    case 196:
        // Area.
        pProjectile = new CProjectile84E9C0("SHAREA", "", "", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 197:
        // Area, palette 1.
        pProjectile = new CProjectile84E9C0("SHAREA", "", "SHAREA1", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 198:
        // Area.
        pProjectile = new CProjectile84E9C0("SHAREA", "", "", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 199:
        // Area, palette 3.
        pProjectile = new CProjectile84E9C0("SHAREA", "", "SHAREA3", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 200:
        // Area, palette 4.
        pProjectile = new CProjectile84E9C0("SHAREA", "", "SHAREA4", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 201:
        // Area, palette 5.
        pProjectile = new CProjectile84E9C0("SHAREA", "", "SHAREA5", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 202:
        // Area, palette 6.
        pProjectile = new CProjectile84E9C0("SHAREA", "", "SHAREA6", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 203:
        // Area, palette 2.
        pProjectile = new CProjectile84E9C0("SHAREA", "", "SHAREA2", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
        break;
    case 208:
        // Death.
        pProjectile = new CProjectile84E9C0("SPFDEATH", "", "", 8, 0);
        pProjectile->m_fireSoundRef = "";
        pProjectile->m_arrivalSoundRef = "";
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
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 237:
        // Bane.
        pArea = new CProjectileArea(250);
        pArea->TargetEnemy(pSprite);
        pArea->m_explosion1.m_animation = "Area1X";
        pArea->m_explosion1.m_sound = "EFF_P99";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 238:
        // Remove Fear.
        pArea = new CProjectileArea(300);
        pArea->TargetFriend(pSprite);
        pArea->m_explosion1.m_animation = "AbjuraX";
        pArea->m_explosion1.m_sound = "ARE_M20";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 239:
        // An area burst.
        pArea = new CProjectileArea(300);
        pArea->m_explosion1.m_animation = "Area2X";
        pArea->m_explosion1.m_sound = "EFF_M99";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 240:
        // See Invisibility, Invisibility Purge.
        pArea = new CProjectileArea(300);
        pArea->TargetEnemy(pSprite);
        pArea->m_explosion1.m_animation = "Area2X";
        pArea->m_explosion1.m_sound = "EFF_M99";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 241:
        // Horror.
        pArea = new CProjectileArea(150);
        pArea->TargetEnemy(pSprite);
        pArea->m_explosion1.m_animation = "Area4X";
        pArea->m_explosion1.m_sound = "EFF_M99";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 242:
        // An abjuration burst over friends.
        pArea = new CProjectileArea(300);
        pArea->TargetFriend(pSprite);
        pArea->m_explosion1.m_animation = "AbjuraX";
        pArea->m_explosion1.m_sound = "ARE_M20";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 243:
        // An area burst.
        pArea = new CProjectileArea(350);
        pArea->m_explosion1.m_animation = "Area1X";
        pArea->m_explosion1.m_sound = "EFF_P99";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
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
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 246:
        // Dispel Magic.
        pProjectile = new CProjectileArea850C0C();
        break;
    case 247:
        // An alteration burst over friends.
        pArea = new CProjectileArea(200);
        pArea->TargetFriend(pSprite);
        pArea->m_explosion1.m_animation = "AlteraX";
        pArea->m_explosion1.m_sound = "ARE_M19";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 248:
        // An alteration burst over enemies.
        pArea = new CProjectileArea(200);
        pArea->TargetEnemy(pSprite);
        pArea->m_explosion1.m_animation = "AlteraX";
        pArea->m_explosion1.m_sound = "ARE_M19";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 250:
        // Remove Paralysis.
        pArea = new CProjectileArea(200);
        pArea->TargetFriend(pSprite);
        pArea->m_explosion1.m_animation = "AbjuraX";
        pArea->m_explosion1.m_sound = "ARE_M20";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 252:
        // An alteration burst over friends.
        pArea = new CProjectileArea(250);
        pArea->TargetFriend(pSprite);
        pArea->m_explosion1.m_animation = "AlteraX";
        pArea->m_explosion1.m_sound = "ARE_M19";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 253:
        // Prayer.
        pArea = new CProjectileArea(600);
        pArea->m_explosion1.m_animation = "Area1X";
        pArea->m_explosion1.m_sound = "EFF_P99";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 254:
        // An enchantment burst over enemies.
        pArea = new CProjectileArea(300);
        pArea->TargetEnemy(pSprite);
        pArea->m_explosion1.m_animation = "EnchanX";
        pArea->m_explosion1.m_sound = "ARE_M21";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 255:
        // Emotion.
        pArea = new CProjectileArea(150);
        pArea->m_explosion1.m_animation = "EnchanX";
        pArea->m_explosion1.m_sound = "ARE_M21";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 256:
        // An enchantment burst over enemies.
        pArea = new CProjectileArea(300);
        pArea->TargetEnemy(pSprite);
        pArea->m_explosion1.m_animation = "EnchanX";
        pArea->m_explosion1.m_sound = "ARE_M21";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 257:
        // An enchantment burst over friends.
        pArea = new CProjectileArea(100);
        pArea->TargetFriend(pSprite);
        pArea->m_explosion1.m_animation = "EnchanX";
        pArea->m_explosion1.m_sound = "ARE_M21";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 258:
        // Magic Circle Against Evil, Holy Aura.
        pArea = new CProjectileArea(200);
        pArea->TargetFriend(pSprite);
        pArea->m_explosion1.m_animation = "AbjuraX";
        pArea->m_explosion1.m_sound = "ARE_M20";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 260:
        // Recitation.
        pArea = new CProjectileArea(600);
        pArea->m_explosion1.m_animation = "Area1X";
        pArea->m_explosion1.m_sound = "EFF_P99";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 264:
        // An enchantment burst over enemies.
        pArea = new CProjectileArea(200);
        pArea->TargetEnemy(pSprite);
        pArea->m_explosion1.m_animation = "EnchanX";
        pArea->m_explosion1.m_sound = "ARE_M21";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 266:
        // Righteous Wrath of the Faithful.
        pArea = new CProjectileArea(350);
        pArea->m_explosion1.m_animation = "AbjuraX";
        pArea->m_explosion1.m_sound = "ARE_M20";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pArea->TargetFriend(pSprite);
        pProjectile = pArea;
        break;
    case 275:
    case 339:
        pArea = new CProjectileArea(200);
        pArea->TargetEnemy(pSprite);
        pArea->m_explosion1.m_animation = "EnchanX";
        pArea->m_explosion1.m_sound = "ARE_M21";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 270:
        // Fire Seeds, Flaming Oil.
        pProjectile = new CProjectileArea84F61C();
        break;
    case 273:
        // An area burst over friends.
        pArea = new CProjectileArea(300);
        pArea->TargetFriend(pSprite);
        pArea->m_explosion1.m_animation = "Area3X";
        pArea->m_explosion1.m_sound = "EFF_M99";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 276:
        // Symbol of Pain, Symbol of Stunning.
        pArea = new CProjectileArea(300);
        pArea->TargetEnemy(pSprite);
        pArea->m_explosion1.m_animation = "SoPainX";
        pArea->m_explosion1.m_sound = "EFF_P49";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 277:
        // Symbol of Hopelessness, Symbol of Fear.
        pArea = new CProjectileArea(300);
        pArea->TargetEnemy(pSprite);
        pArea->m_explosion1.m_animation = "SoHopeX";
        pArea->m_explosion1.m_sound = "EFF_P50";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 280:
        // A plain area. The original reads the radius from the read-only
        // dword at 0x84F06C, which is 100.
        pArea = new CProjectileArea(100);
        pProjectile = pArea;
        break;
    case 281:
        // A plain area.
        pArea = new CProjectileArea(250);
        pProjectile = pArea;
        break;
    case 282:
        // A stinking cloud, after a floating line over the caster.
        IcewindMisc::sub_584880(static_cast<CGameSprite*>(pSprite), 4388, 0); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pArea = new CProjectileArea(100);
        pArea->m_explosion2.m_animation = "SCloudR";
        pArea->m_explosion2.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pArea->m_explosion2.field_2C = TRUE;
        pArea->m_explosion2.field_2E = 7;
        pProjectile = pArea;
        break;
    case 284:
        // SPIN991: the same cloud, after a floating line over the caster.
        IcewindMisc::sub_584880(static_cast<CGameSprite*>(pSprite), 4387, 0); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = new CProjectileArea84F310();
        break;
    case 286:
        // An incendiary cloud.
        pArea = new CProjectileArea(150);
        pArea->m_explosion3.m_animation = "ICloudB";
        pArea->m_explosion3.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pArea->m_explosion3.field_35 = TRUE;
        pArea->m_explosion3.field_36 = TRUE;
        pArea->m_explosion3.field_38 = 500;
        pProjectile = pArea;
        break;
    case 287:
        // An incendiary cloud.
        pArea = new CProjectileArea(150);
        pArea->m_explosion3.m_animation = "ICloudA";
        pArea->m_explosion3.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pArea->m_explosion3.field_35 = TRUE;
        pArea->m_explosion3.field_34 = TRUE;
        pArea->m_explosion3.field_38 = 500;
        pArea->m_nDuration = 200;
        pProjectile = pArea;
        break;
    case 288:
        // A plain area over enemies.
        pArea = new CProjectileArea(300);
        pArea->TargetEnemy(pSprite);
        pProjectile = pArea;
        break;
    case 295:
        // A plain area over enemies.
        pArea = new CProjectileArea(250);
        pArea->TargetEnemy(pSprite);
        pProjectile = pArea;
        break;
    case 299:
        // Soul Eater.
        pArea = new CProjectileArea(120);
        pArea->m_explosion3.m_animation = "SEaterA";
        pArea->m_explosion3.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
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
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pArea->m_explosion3.m_animation = "SGrowtA";
        pArea->m_explosion3.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
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
    case 301:
        // A lingering cloud.
        pArea = new CProjectileArea(300);
        pArea->m_explosion3.m_animation = "CloudBA";
        pArea->m_explosion3.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pArea->m_explosion3.field_35 = TRUE;
        pArea->m_explosion3.field_34 = TRUE;
        pArea->m_explosion3.m_sound = "ARE_P24";
        pArea->m_explosion3.field_38 = 100;
        pArea->m_nTriggerInterval = 10;
        pArea->m_nTriggerCountdown = 0;
        pArea->m_nHitModulus = 10;
        pArea->m_nDuration = 200;
        pProjectile = pArea;
        break;
    case 307:
        // A miasma.
        pArea = new CProjectileArea(120);
        pArea->m_explosion1.m_sound = "EFF_P104";
        pArea->m_explosion3.m_animation = "MoEldaA";
        pArea->m_explosion3.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pArea->m_explosion3.field_35 = TRUE;
        pArea->m_explosion3.field_34 = TRUE;
        pArea->m_explosion3.field_38 = 1200;
        pProjectile = pArea;
        break;
    case 309:
        // A pestilence cloud.
        pArea = new CProjectileArea(200);
        pArea->m_explosion3.m_animation = "CoPestA";
        pArea->m_explosion3.m_visualEffect.sub_586AC0(TRUE, 0x80); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pArea->m_explosion3.field_35 = TRUE;
        pArea->m_explosion3.field_34 = TRUE;
        pArea->m_explosion3.m_sound = "ARE_P25";
        pArea->m_explosion3.field_38 = 175;
        pArea->m_nTriggerInterval = 10;
        pArea->m_nTriggerCountdown = 0;
        pArea->m_nHitModulus = 10;
        pArea->m_nDuration = 400;
        pProjectile = pArea;
        break;
    case 310:
        // A lingering cloud without an animation.
        pArea = new CProjectileArea(200);
        pArea->m_explosion1.m_sound = "ARE_P28";
        pArea->m_explosion3.m_sound = "AFT_P27";
        pArea->m_nTriggerInterval = 10;
        pArea->m_nTriggerCountdown = 0;
        pArea->m_nHitModulus = 10000;
        pArea->m_nDuration = 1000;
        pProjectile = pArea;
        break;
    case 317:
        // A suffocation cloud.
        pArea = new CProjectileArea(120);
        pArea->m_explosion3.m_animation = "SuffocA";
        pArea->m_explosion3.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pArea->m_explosion3.field_35 = TRUE;
        pArea->m_explosion3.field_34 = TRUE;
        pArea->m_explosion3.m_sound = "AFT_M18";
        pArea->m_explosion3.field_38 = 1000;
        pArea->m_nTriggerInterval = 10;
        pArea->m_nTriggerCountdown = 0;
        pArea->m_nHitModulus = 10;
        pArea->m_nDuration = 400;
        pProjectile = pArea;
        break;
    case 318:
        // Horrid Wilting.
        pArea = new CProjectileArea(300);
        pArea->m_explosion1.m_animation = "ADHWilX";
        pArea->m_explosion1.m_sound = "ARE_M17";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 320:
        // A plain area.
        pArea = new CProjectileArea(225);
        pProjectile = pArea;
        break;
    case 321:
        // A plain area.
        pArea = new CProjectileArea(250);
        pProjectile = pArea;
        break;
    case 322:
        // A plain area.
        pArea = new CProjectileArea(150);
        pProjectile = pArea;
        break;
    case 323:
        // A plain area.
        pArea = new CProjectileArea(200);
        pProjectile = pArea;
        break;
    case 336:
        // Power Word: Stun.
        pArea = new CProjectileArea(150);
        pArea->TargetEnemy(pSprite);
        pArea->m_explosion1.m_animation = "PWStunH";
        pArea->m_explosion1.m_sound = "EFF_M38";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 337:
        // Holy Smite.
        pArea = new CProjectileArea(200);
        pArea->m_explosion3.m_animation = "HSmiteA";
        pArea->m_explosion3.m_visualEffect.sub_586AC0(TRUE, 0x40); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pArea->m_explosion3.field_34 = TRUE;
        pArea->m_explosion3.field_35 = TRUE;
        pArea->m_explosion3.field_38 = 300;
        pArea->m_nTriggerInterval = 10000;
        pArea->m_nTriggerCountdown = 0;
        pArea->m_nHitModulus = 10;
        pArea->m_nDuration = 45;
        pProjectile = pArea;
        break;
    case 338:
        // Unholy Blight.
        pArea = new CProjectileArea(200);
        pArea->m_explosion3.m_animation = "UBlighA";
        pArea->m_explosion3.m_visualEffect.sub_586AC0(TRUE, 0x40); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pArea->m_explosion3.field_34 = TRUE;
        pArea->m_explosion3.field_35 = TRUE;
        pArea->m_explosion3.field_38 = 300;
        pArea->m_nTriggerInterval = 10000;
        pArea->m_nTriggerCountdown = 0;
        pArea->m_nHitModulus = 10;
        pArea->m_nDuration = 45;
        pProjectile = pArea;
        break;
    case 340:
        // A twin of 341 (Holy Word, Blasphemy).
        pArea = new CProjectileArea(300);
        pArea->m_explosion1.m_animation = "SoHopeX";
        pArea->m_explosion1.m_sound = "EFF_P50";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 341:
        // Holy Word, Blasphemy.
        pArea = new CProjectileArea(300);
        pArea->m_explosion1.m_animation = "SoHopeX";
        pArea->m_explosion1.m_sound = "EFF_P50";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 342:
        // A plain area over enemies.
        pArea = new CProjectileArea(500);
        pArea->TargetEnemy(pSprite);
        pProjectile = pArea;
        break;
    case 349:
        // A breath.
        pProjectile = new CProjectileArea85072C();
        break;
    case 351:
        // A cloudkill, after a floating line over the caster.
        IcewindMisc::sub_584880(static_cast<CGameSprite*>(pSprite), 27030, 0); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pArea = new CProjectileArea(125);
        pArea->m_explosion2.m_animation = "CloudKR";
        pArea->m_explosion2.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pArea->m_explosion2.field_2C = TRUE;
        pArea->m_explosion2.field_2E = 7;
        pProjectile = pArea;
        break;
    case 358:
        // A healing circle.
        pArea = new CProjectileArea(125);
        pArea->TargetFriend(pSprite);
        pArea->m_explosion1.m_animation = "Area1X";
        pArea->m_explosion1.m_sound = "EFF_M99";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 359:
        // Healing Circle, Mass Heal.
        pArea = new CProjectileArea(300);
        pArea->TargetFriend(pSprite);
        pArea->m_explosion1.m_animation = "Area1X";
        pArea->m_explosion1.m_sound = "EFF_P99";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 362:
        // An area burst over enemies.
        pArea = new CProjectileArea(300);
        pArea->TargetEnemy(pSprite);
        pArea->m_explosion1.m_animation = "Area1X";
        pArea->m_explosion1.m_sound = "EFF_W99";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 366:
        // A swarm. The original also re-stores the radius (a no-op here).
        pArea = new CProjectileArea(300);
        pArea->m_explosion3.m_animation = "MSwarmA";
        pArea->m_explosion3.m_sound = "ARE_P03";
        pArea->m_explosion3.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pArea->m_explosion3.field_34 = FALSE;
        pArea->m_explosion3.field_35 = FALSE;
        pArea->m_explosion3.field_38 = 250;
        pArea->m_nTriggerInterval = 10;
        pArea->m_nTriggerCountdown = 0;
        pArea->m_nHitModulus = 10000;
        pArea->m_nDuration = 45;
        pArea->field_1D8 = 1;
        pProjectile = pArea;
        break;
    case 367:
        // An alteration burst over friends.
        pArea = new CProjectileArea(200);
        pArea->TargetFriend(pSprite);
        pArea->m_explosion1.m_animation = "AlteraX";
        pArea->m_explosion1.m_sound = "ARE_M19";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 371:
        // An alteration burst over friends.
        pArea = new CProjectileArea(70);
        pArea->TargetFriend(pSprite);
        pArea->m_explosion1.m_animation = "AlteraX";
        pArea->m_explosion1.m_sound = "ARE_M19";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 372:
        // An abjuration burst.
        pArea = new CProjectileArea(160);
        pArea->m_explosion1.m_animation = "AbjuraX";
        pArea->m_explosion1.m_sound = "ARE_M20";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 373:
        // Control Undead, Mass Dominate.
        pArea = new CProjectileArea(200);
        pArea->m_explosion1.m_animation = "EnchanX";
        pArea->m_explosion1.m_sound = "ARE_M21";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pProjectile = pArea;
        break;
    case 376:
        // Turn Undead.
        pProjectile = new CProjectileArea850A38();
        break;
    case 377:
        // A pestilence cloud.
        pArea = new CProjectileArea(200);
        pArea->m_explosion3.m_animation = "CoPestA";
        pArea->m_explosion3.m_visualEffect.sub_586AC0(TRUE, 0x80); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pArea->m_explosion3.field_35 = TRUE;
        pArea->m_explosion3.field_34 = TRUE;
        pArea->m_explosion3.m_sound = "ARE_P25";
        pArea->m_explosion3.field_38 = 175;
        pArea->m_nTriggerInterval = 10;
        pArea->m_nTriggerCountdown = 0;
        pArea->m_nHitModulus = 10;
        pArea->m_nDuration = 300;
        pProjectile = pArea;
        break;
    case 381:
        // A breath of gas. The original also re-stores the radius (a
        // no-op here).
        pArea = new CProjectileArea(200);
        pArea->m_explosion1.m_animation = "GABreaX";
        pArea->m_explosion1.m_sound = "ARE_M21";
        pArea->m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pArea->m_explosion3.m_animation = "GABreaA";
        pArea->m_explosion3.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
        pArea->m_explosion3.field_35 = TRUE;
        pArea->m_explosion3.field_36 = TRUE;
        pArea->m_explosion3.field_38 = 500;
        pArea->m_nTriggerInterval = 10000;
        pArea->m_nTriggerCountdown = 0;
        pArea->m_nHitModulus = 10;
        pArea->m_nDuration = 45;
        pProjectile = pArea;
        break;
    case 384:
    case 385:
        // A plain area.
        pArea = new CProjectileArea(65);
        pProjectile = pArea;
        break;
    default:
        if (static_cast<UINT>(nProjectileType - 1) > 0x181) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CProjectile.cpp
            // __LINE__: 3127
            UTIL_ASSERT(FALSE);
        }

        // TODO (0x51EB93-0x528DEF): the jump table at 0x528E7C
        // builds one class per type, 2 to 386. Beyond the cases above:
        // - types 19, 39, 43, 44, 46, 58, 61, 62, 63, 99, 106, 107, 191,
        //   219-226, 259, 265 and 348 call UtilAssert and fall through to
        //   the fallback below; the default path already reproduces that,
        //   so they stay unwired;
        // - TODO: 0x5201C0 (type 45): builds the CProjectile84E9C0 (ctor
        //   0x535F20, like types 119-142) and then overwrites its vtable
        //   with 0x84D838 - a CProjectile84E9C0-derived class whose
        //   constructor is fully inlined at the call site; the class is not
        //   ported;
        // - types whose constructor is not ported yet:
        //   TODO: type 189: ctor 0x529A90 not ported
        //   TODO: type 352: ctor 0x529CE0 not ported
        //   TODO: types 8, 13, 18, 28, 33, 57, 104, 204: ctor 0x52E230 not ported
        //   TODO: type 3: ctor 0x52E9F0 not ported
        //   TODO: types 96, 100: ctor 0x52F260 not ported
        //   TODO: types 80, 105: ctor 0x52FC20 not ported
        //   TODO: types 206, 380: ctor 0x52FE90 not ported
        //   TODO: type 210: ctor 0x537010 not ported
        //   TODO: type 217: ctor 0x5745E0 not ported
        //   TODO: type 274: ctor 0x574D50 not ported
        //   TODO: type 283: ctor 0x575090 not ported
        //   TODO: type 350: ctor 0x576080 not ported
        //   TODO: type 355: ctor 0x576770 not ported
        //   TODO: type 356: ctor 0x576870 not ported
        //   TODO: type 360: ctor 0x576990 not ported
        //   TODO: type 207: ctor 0x57D390 not ported
        //   TODO: type 261: ctor 0x57E1B0 not ported
        //   TODO: types 227-234: ctor 0x57E370 not ported
        //   TODO: type 312: ctor 0x57F210 not ported
        //   TODO: type 305: ctor 0x57F640 not ported
        //   TODO: type 304: ctor 0x5802B0 not ported
        //   TODO: types 40, 302, 313, 378: ctor 0x5806C0 not ported
        //   TODO: type 314: ctor 0x580C00 not ported
        //   TODO: type 346: ctor 0x581060 not ported
        //   TODO: type 370: ctor 0x581CA0 not ported
        // DEVIATION (temporary): unported type -> instant projectile. The
        // original reaches the fallback below only for types out of range
        // and for the assert entries listed above. Until the other cases
        // are ported, a replay differs in two ways: the effects land on
        // the Fire frame instead of after the projectile's flight, and the
        // instant projectile never calls GetObjectArray()->Add where the
        // real classes do (some also add a VEF and sparkle objects), so
        // every later object id shifts.
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
    // NOTE: The exe has the base CProjectile constructor (0x529490) inlined
    // here whole, then stamps vtable 0x84D8B8; the implicit base construction
    // already performs these stores, so the re-statement below mirrors the
    // exe's instruction stream rather than adding state. It is the fork's
    // constructor body completed with the four stores its class model puts on
    // CProjectileTravelling instead (m_driftDecay, field_E6,
    // m_bTrackTargetHeight, m_bSparkle).
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

// 0x5295D0
void CProjectileBAM::SetVidCell(CResRef resRef)
{
    if (m_animation != NULL) {
        delete m_animation;
    }
    m_animation = new CVidCell(resRef, FALSE);
}

// 0x529660: point the alternate palette bitmap (field_1E2) at the named
// resource, clear its double-size flag, and arm it (field_29C) as the
// palette Render swaps in for the cell.
void CProjectileBAM::SetAltPalette(const char* sName)
{
    field_1E2.SetResRef(CResRef(sName), TRUE, TRUE);
    field_1E2.m_bDoubleSize = FALSE;
    field_29C = 1;
}

// 0x5297B0
CProjectileSparkleExplosion::CProjectileSparkleExplosion(CResRef resRef)
    : CProjectileExploding(resRef)
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
    m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
    m_explosion3.m_animation = "AStormA";
    m_explosion3.m_sound = "ARE_M04";
    m_explosion3.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
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
    m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
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
    m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
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
    m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
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
    m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
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
    m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
    m_explosion3.m_animation = "IStormA";
    m_explosion3.m_sound = "ARE_M04";
    m_explosion3.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
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
    m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
    m_explosion2.m_animation = "ICloudR";
    m_explosion2.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
    m_explosion2.field_2C = TRUE;
    m_explosion2.field_2E = 13;
    m_explosion3.m_animation = "ICloudA";
    m_explosion3.m_sound = "ARE_M05";
    m_explosion3.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
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
    m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
    m_explosion2.m_animation = "IPlaguR";
    m_explosion2.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
    m_explosion2.field_2C = TRUE;
    m_explosion2.field_2E = 13;
    m_explosion3.m_animation = "IPlaguA";
    m_explosion3.m_sound = "ARE_P02";
    m_explosion3.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
    m_nTriggerInterval = 10;
    m_nHitModulus = 10;
    m_explosion3.field_34 = TRUE;
    m_explosion3.field_35 = TRUE;
    m_nTriggerCountdown = 0;
    m_nDuration = 1500;
    field_1D8 = 1;
    m_nRadius = 250;
}

// 0x573CC0
CProjectileCorrosiveFog::CProjectileCorrosiveFog()
    : CProjectileArea(0x100)
{
    m_explosion1.m_animation = "DFogX";
    m_explosion1.m_sound = "RNG_M01";
    m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
    m_explosion2.m_animation = "DFogR";
    m_explosion2.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
    m_explosion2.field_2C = TRUE;
    m_explosion2.field_2E = 26;
    m_explosion3.m_animation = "DFogA";
    m_explosion3.m_sound = "ARE_M02";
    m_explosion3.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
    m_nTriggerInterval = 10;
    m_nHitModulus = 10;
    m_explosion3.field_34 = TRUE;
    m_explosion3.field_35 = TRUE;
    m_explosion3.field_38 = 500;
    m_nTriggerCountdown = 0;
    m_nDuration = 2000;
    field_1D8 = 1;
    m_nRadius = 250;
}

// 0x573E90
CProjectilePowerWordKill::CProjectilePowerWordKill()
    : CProjectileArea(0x100)
{
    m_explosion1.m_animation = "PWKillX";
    m_explosion1.m_sound = "EFF_M39";
    m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
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
    m_explosion1.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
    m_explosion3.m_animation = "PFireA";
    m_explosion3.m_sound = "ARE_P03";
    m_explosion3.m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
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

// 0x5756C0
CProjectilePortalAnimFlip::CProjectilePortalAnimFlip()
    : CProjectileArea(0x7D0)
{
    m_targetType.Set(CAIObjectType::NOT_SPRITE);
}

// 0x575B90
CProjectileTremor::CProjectileTremor()
    : CProjectileArea(0x100)
{
    m_explosion1.m_sound = "ARE_P27";
    m_nRadius = 0x15E;
}

// 0x5768A0
// Acid Fog's slot-0 destructor. The body at 0x5768A0 is the deleting
// thunk MSVC emits beside every destructor definition; the exe
// ICF-folds every empty spell-hit leaf onto it — Tremor's 0x850558,
// PortalAnimFlip's 0x850420 and DBFireball's 0x85099C all carry 0x5768A0
// as slot 0, and the scalar half 0x575B20 is a bare jump to the base
// dtor 0x56F1F0. The fog adds no members of its own, so ours is the
// empty braces that chain to CProjectileArea::~CProjectileArea.
CProjectileArea84F3AC::~CProjectileArea84F3AC()
{
}

// The DBFireball half of the same fold (0x85099C slot 0): both members are
// trivial (a BYTE flag and a LONG timer), so the empty braces are the whole
// body. The exe ICF-folds this copy onto the Acid Fog row above, so the
// definition carries no address marker of its own; it exists so the class
// declares every slot its vtable overrides.
CProjectileDBFireball::~CProjectileDBFireball()
{
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

// 0x576CE0
// Vtable slot 0x70 of 0x85099C: the landing does not detonate — the bead
// only latches the exploded flag so the AIUpdate scan above (and the base
// teardown) sees the arrival; the actual burst comes later through
// CProjectileArea::OnArrival.
void CProjectileDBFireball::OnArrival()
{
    m_bExploded = TRUE;
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
    : CProjectileSingleTarget(CResRef("CLightT"))
    , m_struckTargets(struckTargets)
{
    m_animation->SequenceSet(0);
    field_1D8 = 16;
    m_fireSoundRef = "TRA_09";
    m_speed = static_cast<SHORT>(static_cast<USHORT>(m_speed) << 1);
    m_visualEffect.sub_586A90(TRUE); // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
    m_bHasHeight = FALSE;
    field_1C2 = FALSE;
}

// 0x57F760
// The tornado's slot-0 destructor: the body at 0x57F760 is the deleting
// thunk (call the scalar body 0x57F780, free this when the flag's bit 0
// is set), and the scalar body is the compiler's member teardown — the
// loop sound's CSound dtor first (+0x2F0), then the strike tracker map
// at +0x2C2 (each node's value through 0x582670, the node freed, the
// tree orphaned), then the CProjectileSingleTarget dtor. Our tracker is
// parked as raw bytes (+0x2BA), so the empty braces spell the source
// truth: the members go down implicitly, in reverse declaration order,
// exactly what the exe runs.
CProjectileWhirlwind::~CProjectileWhirlwind()
{
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

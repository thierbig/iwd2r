#include "CGameAIBase.h"

#include "C2DArray.h"
#include "CAIConditionResponse.h"
#include "CAIResponse.h"
#include "CAIScript.h"
#include "CAITrigger.h"
#include "CAIUtil.h"
#include "CBaldurChitin.h"
#include "CBaldurProjector.h"
#include "CButtonData.h"
#include "CCreatureFile.h"
#include "CGameArea.h"
#include "CGameContainer.h"
#include "CGameDoor.h"
#include "CGameEffect.h"
#include "CGameSound.h"
#include "CGameSpawning.h"
#include "CGameSprite.h"
#include "CGameStatic.h"
#include "CGameTiledObject.h"
#include "CGameTimer.h"
#include "CGameTrigger.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CPathSearch.h"
#include "CProjectile.h"
#include "CProjectileSingleTarget.h"
#include "CScreenChapter.h"
#include "CScreenCharacter.h"
#include "CScreenInventory.h"
#include "CScreenWorld.h"
#include "CSound.h"
#include "CSpell.h"
#include "CTimerWorld.h"
#include "CUtil.h"
#include "CVariableHash.h"
#include "CWeather.h"
#include "Icewind586B70.h"
#include "IcewindCGameEffects.h"

// 0x8485C4
const SHORT CGameAIBase::ACTION_DONE = -1;

// 0x8485C6
const SHORT CGameAIBase::ACTION_INTERRUPTABLE = 1;

// 0x8485C8
const SHORT CGameAIBase::ACTION_NORMAL = 0;

// 0x8485CA
const SHORT CGameAIBase::ACTION_ERROR = -2;

// 0x8485CC
const SHORT CGameAIBase::ACTION_NO_ACTION = 2;

// 0x8485CE
const SHORT CGameAIBase::ACTION_STOPPED = -3;

// 0x8485E8
const BYTE CGameAIBase::EFFECT_LIST_TIMED = 1;

// 0x8485E9
const BYTE CGameAIBase::EFFECT_LIST_EQUIPED = 2;

// 0x8D1408
const CString CGameAIBase::DEAD_GLOBAL_PREFIX("_DEAD");

// 0x8D1810
CAIAction CGameAIBase::m_aiAction;

// 0x44C4B0
CGameAIBase::CGameAIBase()
{
    m_nLastActionReturn = 0;
    m_objectType = CGameObject::TYPE_AIBASE;
    m_lAttacker.Set(CAIObjectType::NOONE);
    m_lOrderedBy.Set(CAIObjectType::NOONE);
    field_EA.Set(CAIObjectType::NOONE);
    field_126.Set(CAIObjectType::NOONE);
    field_162.Set(CAIObjectType::NOONE);
    m_lHitter.Set(CAIObjectType::NOONE);
    m_lHelp.Set(CAIObjectType::NOONE);
    m_lTrigger.Set(CAIObjectType::NOONE);
    m_lSeen.Set(CAIObjectType::NOONE);
    m_lTalkedTo.Set(CAIObjectType::NOONE);
    m_lHeard.Set(CAIObjectType::NOONE);
    field_306.Set(CAIObjectType::NOONE);
    field_342.Set(CAIObjectType::NOONE);
    field_37E.Set(CAIObjectType::NOONE);
    field_3BA.Set(CAIObjectType::NOONE);
    m_curResponseNum = -1;
    m_curResponseSetNum = -1;
    m_curScriptNum = -1;
    m_curAction = CAIAction::NULL_ACTION;
    m_interrupt = FALSE;
    m_actionCount = 0;
    field_54C = 0;
    field_550 = 0;
    field_552 = 0;
    field_44A = 0;
    m_overrideScript = NULL;
    m_special1Script = NULL;
    m_teamScript = NULL;
    m_special2Script = NULL;
    m_combatScript = NULL;
    m_special3Script = NULL;
    m_movementScript = NULL;
    m_inCutScene = FALSE;
    m_reactionRoll = 10;
    field_550 = rand() % 120;

    CAITrigger trigger(CAITrigger::ONCREATION, 0);
    m_pendingTriggers.AddTail(new CAITrigger(trigger));

    m_firstCall = TRUE;
    m_forceActionPick = FALSE;
    field_580 = 0;
    m_nLastActionReturn = -1;
    field_588 = 0;
    field_58C = 0;
    field_594 = 0;
    field_595 = 1;
    field_596 = 0;
    m_randValue = rand() & 0x7FFF;
}

// 0x44D160
CGameAIBase::~CGameAIBase()
{
    POSITION pos;

    pos = m_pendingTriggers.GetHeadPosition();
    while (pos != NULL) {
        CAITrigger* pTrigger = m_pendingTriggers.GetNext(pos);
        if (pTrigger != NULL) {
            delete pTrigger;
        }
    }

    pos = m_queuedActions.GetHeadPosition();
    while (pos != NULL) {
        CAIAction* pAction = m_queuedActions.GetNext(pos);
        if (pAction != NULL) {
            delete pAction;
        }
    }

    pos = m_timers.GetHeadPosition();
    while (pos != NULL) {
        CGameTimer* pTimer = m_timers.GetNext(pos);
        if (pTimer != NULL) {
            delete pTimer;
        }
    }

    if (m_overrideScript != NULL) {
        delete m_overrideScript;
        m_overrideScript = NULL;
    }

    if (m_special1Script != NULL) {
        delete m_special1Script;
        m_special1Script = NULL;
    }

    if (m_teamScript != NULL) {
        delete m_teamScript;
        m_teamScript = NULL;
    }

    if (m_special2Script != NULL) {
        delete m_special2Script;
        m_special2Script = NULL;
    }

    if (m_combatScript != NULL) {
        delete m_combatScript;
        m_combatScript = NULL;
    }

    if (m_special3Script != NULL) {
        delete m_special3Script;
        m_special3Script = NULL;
    }

    if (m_movementScript != NULL) {
        delete m_movementScript;
        m_movementScript = NULL;
    }
}

// 0x44D100
const BYTE* CGameAIBase::GetVisibleTerrainTable()
{
    return CGameObject::DEFAULT_VISIBLE_TERRAIN_TABLE;
}

// 0x44D110
const BYTE* CGameAIBase::GetTerrainTable()
{
    return CGameObject::DEFAULT_TERRAIN_TABLE;
}

// 0x44D120
BOOLEAN CGameAIBase::CompressTime(DWORD deltaTime)
{
    CheckTimers(deltaTime / CTimerWorld::TIMESCALE_MSEC_PER_SEC);
    return TRUE;
}

// 0x44D4B0
void CGameAIBase::CheckTimers(LONG cycles)
{
    POSITION pos = m_timers.GetHeadPosition();
    while (pos != NULL) {
        POSITION oldPos = pos;
        CGameTimer* pTimer = m_timers.GetNext(pos);
        pTimer->m_time -= cycles;
        if (pTimer->m_time <= 0) {
            CAITrigger trigger(CAITrigger::TIMEREXPIRED, pTimer->m_id);

            // NOTE: Uninline.
            SetTrigger(trigger);

            m_timers.RemoveAt(oldPos);
            delete pTimer;
        }
    }
}

// 0x44D640
void CGameAIBase::ClearActions(BOOL a1)
{
    if (a1) {
        POSITION pos = m_queuedActions.GetHeadPosition();
        while (pos != NULL) {
            POSITION curr = pos;
            CAIAction* pAction = m_queuedActions.GetNext(pos);
            if (pAction != NULL) {
                if (pAction->m_actionID != CAIAction::SPELL
                    && pAction->m_actionID != CAIAction::SPELLPOINT
                    && pAction->m_actionID != CAIAction::FORCESPELL
                    && pAction->m_actionID != CAIAction::FORCESPELLPOINT
                    && pAction->m_actionID != CAIAction::APPLYSPELL
                    && pAction->m_actionID != CAIAction::REALLYFORCESPELL
                    && pAction->m_actionID != CAIAction::SPELLNODEC
                    && pAction->m_actionID != CAIAction::SPELLPOINTNODEC) {
                    m_queuedActions.RemoveAt(curr);
                    delete pAction;
                }
            }
        }
    } else {
        POSITION pos = m_queuedActions.GetHeadPosition();
        while (pos != NULL) {
            CAIAction* pAction = m_queuedActions.GetNext(pos);
            if (pAction != NULL) {
                delete pAction;
            }
        }
        m_queuedActions.RemoveAll();
    }

    if (m_queuedActions.GetCount() == 0) {
        m_curResponseNum = -1;
        m_curResponseSetNum = -1;
        m_curScriptNum = -1;
    }
}

// 0x44D730
void CGameAIBase::ClearTriggers()
{
    ApplyTriggers();

    POSITION pos = m_pendingTriggers.GetHeadPosition();
    while (pos != NULL) {
        CAITrigger* pTrigger = m_pendingTriggers.GetNext(pos);
        if (pTrigger != NULL) {
            delete pTrigger;
        }
    }
    m_pendingTriggers.RemoveAll();
}

// 0x44D780
void CGameAIBase::DoAction()
{
    SHORT actionReturn = ExecuteAction();

    if (actionReturn == ACTION_DONE
        || actionReturn == ACTION_ERROR
        || actionReturn == ACTION_STOPPED) {
        CAIAction action;
        SetCurrAction(GetNextAction(action));
        ResetCurrResponse();
    } else if (m_interrupt && actionReturn == ACTION_INTERRUPTABLE) {
        CAIAction action;
        m_actionCount++;
        SetCurrAction(GetNextAction(action));
        m_interrupt = FALSE;
    } else {
        m_actionCount++;
    }
}

// NOTE: Inlined in `ExecuteAction` for the actions whose target is the
// action's actee of any type: the actee if it is not immune to the caller,
// shared, and handed to the caller's target hook (vtable + 0x70).
static CGameObject* ExecuteActionGetActee(CGameAIBase* pCaller)
{
    CGameObject* pTarget = pCaller->m_curAction.m_acteeID.sub_40CCA0(pCaller, FALSE);
    if (pTarget != NULL
        && pTarget->GetObjectType() == CGameObject::TYPE_SPRITE
        && static_cast<CGameSprite*>(pTarget)->m_derivedStats.m_cImmunitiesAIType.OnList(pCaller->m_typeAI)) {
        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(pTarget->m_id,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        pTarget = NULL;
    }

    pCaller->SetTargetObject(pTarget);

    return pTarget;
}

// 0x44DC10
SHORT CGameAIBase::ExecuteAction()
{
    CGameObject* pTarget = NULL;
    SHORT returnValue = ACTION_ERROR;
    BOOL bShare = FALSE;
    CString sError;
    CMessage* message;

    if (m_curAction.m_actionID == 178) {
        // BreakInstants.
        SetCurrAction(GetNextAction(m_aiAction));
    }

    switch (m_curAction.m_actionID) {
    case 0:
        // NoAction
        returnValue = ACTION_NO_ACTION;
        goto done;
    case 1:
        // ActionOverride: the jump table's entry is the epilogue.
        goto done;
    case 7:
    case 228:
        // CreateCreature
        returnValue = CreateCreature();
        goto done;
    case 16:
        // GiveOrder
        pTarget = GetTargetShareType(CGameObject::TYPE_AIBASE);
        bShare = TRUE;
        returnValue = GiveOrder(static_cast<CGameAIBase*>(pTarget));
        goto release;
    case 26:
        // PlaySound
        returnValue = PlaySound(0, 0, FALSE);
        goto done;
    case 320:
        // PlaySoundPoint
        returnValue = PlaySound(m_curAction.m_dest.x, m_curAction.m_dest.y, TRUE);
        goto done;
    case 30:
    case 306:
        // SetGlobal, SetGlobalRandom
        returnValue = SetGlobal();
        goto done;
    case 36:
    case 178:
    case 234:
    case 246:
        // Continue, BreakInstants, Debug, Log
        returnValue = ACTION_DONE;
        goto done;
    case 49:
    case 264:
        // MoveViewPoint, MoveViewPointUntilDone
        returnValue = MoveViewPoint();
        goto done;
    case 50:
        // MoveViewObject
        pTarget = ExecuteActionGetActee(this);
        if (pTarget == NULL) {
            returnValue = ACTION_ERROR;
            goto release;
        }
        bShare = TRUE;
        returnValue = MoveViewObject(pTarget);
        goto release;
    case 51:
        returnValue = ClickLButtonPoint();
        goto done;
    case 52:
        pTarget = ExecuteActionGetActee(this);
        bShare = TRUE;
        returnValue = ClickLButtonObject(pTarget);
        goto release;
    case 53:
        returnValue = ClickRButtonPoint();
        goto done;
    case 54:
        pTarget = ExecuteActionGetActee(this);
        bShare = TRUE;
        returnValue = ClickRButtonObject(pTarget);
        goto release;
    case 55:
        returnValue = DoubleClickLButtonPoint();
        goto done;
    case 56:
        pTarget = ExecuteActionGetActee(this);
        bShare = TRUE;
        returnValue = DoubleClickLButtonObject(pTarget);
        goto release;
    case 57:
        returnValue = DoubleClickRButtonPoint();
        goto done;
    case 58:
        pTarget = ExecuteActionGetActee(this);
        bShare = TRUE;
        returnValue = DoubleClickRButtonObject(pTarget);
        goto release;
    case 59:
        returnValue = MoveCursorPoint();
        goto done;
    case 252:
        // ChangeCurrentScript: ChangeAIScript at the current script level.
        m_curAction.m_specificID = m_curScriptNum;
        // FALLTHROUGH
    case 60:
        // ChangeAIScript
        returnValue = ChangeAIScript();
        goto done;
    case 232:
        // StartRandomTimer
        {
            // NOTE: The SHORT at 0x8477EC (61, StartTimer).
            m_curAction.m_actionID = 61;
            LONG nMin = m_curAction.m_specificID2 < m_curAction.m_specificID3
                ? m_curAction.m_specificID2
                : m_curAction.m_specificID3;
            LONG nMax = m_curAction.m_specificID2 > m_curAction.m_specificID3
                ? m_curAction.m_specificID2
                : m_curAction.m_specificID3;
            LONG nRange = nMax - nMin + 1;
            if (nRange == 0) {
                m_curAction.m_specificID2 = nMin;
            } else {
                m_curAction.m_specificID2 = rand() % nRange + nMin;
            }
        }
        // FALLTHROUGH
    case 61:
        // StartTimer
        returnValue = StartTimer();
        goto done;
    case 62:
        // SendTrigger
        pTarget = ExecuteActionGetActee(this);
        bShare = TRUE;
        returnValue = SendTrigger(static_cast<CGameAIBase*>(pTarget));
        goto release;
    case 63:
    case 205:
        // Wait
        returnValue = Wait();
        goto done;
    case 64:
        // UndoExplore
        message = new CMessageSetAreaExplored(FALSE, m_id, m_id);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        returnValue = ACTION_DONE;
        goto done;
    case 65:
        // Explore
        message = new CMessageSetAreaExplored(TRUE, m_id, m_id);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        returnValue = ACTION_DONE;
        goto done;
    case 66:
        // DayNight
        g_pBaldurChitin->GetObjectGame()->m_worldTime.AdvanceCurrentTime(CTimerWorld::TIMESCALE_MIN_PER_HOUR
            * CTimerWorld::TIMESCALE_SEC_PER_MIN
            * CTimerWorld::TIMESCALE_MSEC_PER_SEC
            * m_curAction.m_specificID);
        returnValue = ACTION_DONE;
        goto done;
    case 67:
        // Weather
        g_pBaldurChitin->m_pEngineWorld->m_weather.SetCurrentWeather(m_pArea,
            g_pBaldurChitin->GetObjectGame()->m_worldTime.m_gameTime,
            static_cast<BYTE>(m_curAction.m_specificID));
        returnValue = ACTION_DONE;
        goto done;
    case 68:
        // CallLightning (0x44E7A1-0x44E885): strikes the target sprite's
        // position with lightning in this object's area infinity, then
        // queues a death effect (id 13, amount 0, flags 0x100) on the
        // target by message. A null target skips both; either way the arm
        // lands in the tail that forces ACTION_DONE (0x44E1DB).
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        bShare = TRUE;
        if (pTarget != NULL) {
            const CPoint& pos = pTarget->GetPos();
            m_pArea->GetInfinity()->CallLightning(pos.x, pos.y);
            // NOTE: both allocation-failure branches are dead in tree
            // (throwing new): the effect new's failure jump (0x44E808)
            // zeroes the pointer and the next store (0x44E832)
            // dereferences it, and a failed message new posts NULL
            // (0x44E870). The buffer is owned by the message. The default
            // CGameEffectDeath construction covers the inlined writes
            // (vtable 0x8487E8, m_effectID 13 at +0xC, field_18C 0 at
            // +0x18C).
            CGameEffectDeath* pEffect = new CGameEffectDeath();
            pEffect->m_effectAmount = 0;
            pEffect->m_dwFlags = 0x100;
            message = new CMessageAddEffect(pEffect, GetId(), pTarget->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
        // The AddMessage call site (0x44E1D6) falls into the tail that
        // forces ACTION_DONE (0x44E1DB) before the release block.
        returnValue = ACTION_DONE;
        goto release;
    case 82:
        // CreateItem
        returnValue = CreateItem();
        goto done;
    case 285:
        // SmallWaitRandom
        {
            LONG nMin = m_curAction.m_specificID;
            LONG nMax = m_curAction.m_specificID2;
            if (nMin > nMax) {
                nMin = m_curAction.m_specificID2;
                nMax = m_curAction.m_specificID;
            }
            LONG nRange = nMax - nMin;
            LONG nRandom;
            if (nRange == 0) {
                nRandom = 0;
            } else {
                nRandom = rand() % nRange;
            }
            m_curAction.m_specificID = nRandom + nMin;
        }
        // FALLTHROUGH
    case 83:
        // SmallWait
        returnValue = SmallWait();
        goto done;
    case 106:
    case 212:
        // Shout
        returnValue = Shout();
        goto done;
    case 109:
        // IncrementGlobal
        returnValue = IncrementGlobal();
        goto done;
    case 113:
    case 181:
        // ForceSpell, ReallyForceSpell (0x44E885-0x44E8AE)
        pTarget = GetTargetShareType(CGameObject::TYPE_AIBASE);
        bShare = TRUE;
        returnValue = ForceSpell(static_cast<CGameAIBase*>(pTarget));
        goto release;
    case 114:
        // ForceSpellPoint (0x44E8AE-0x44E8BC)
        returnValue = ForceSpellPoint();
        goto done;
    case 115:
    case 307:
    case 308:
        // SetGlobalTimer, SetGlobalTimerRandom, SetGlobalTimerOnce
        returnValue = SetGlobalTimer();
        goto done;
    case 116:
    case 188:
    case 193:
    case 204:
        // TakePartyItem, TakePartyItemAll, TakePartyItemRange,
        // TakePartyItemNum
        returnValue = TakePartyItem();
        goto done;
    case 117:
        returnValue = TakePartyGold();
        goto done;
    case 118:
    case 171:
        // GivePartyGold, GiveGoldForce
        returnValue = GivePartyGold();
        goto done;
    case 120:
        returnValue = StartCutScene();
        goto done;
    case 121:
        // StartCutSceneMode
        message = new CMessageCutSceneModeStatus(TRUE, m_id, m_id);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        returnValue = ACTION_DONE;
        goto done;
    case 122:
        // EndCutSceneMode
        message = new CMessageCutSceneModeStatus(FALSE, m_id, m_id);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        returnValue = ACTION_DONE;
        goto done;
    case 123:
        returnValue = ClearAllActions();
        goto done;
    case 125:
        // Deactivate
        pTarget = ExecuteActionGetActee(this);
        bShare = TRUE;
        if (pTarget != NULL
            && g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(pTarget->m_id) == -1) {
            message = new CMessageSetActive(FALSE, m_id, pTarget->m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
        returnValue = ACTION_DONE;
        goto release;
    case 126:
        // Activate
        pTarget = ExecuteActionGetActee(this);
        bShare = TRUE;
        if (pTarget != NULL) {
            message = new CMessageSetActive(TRUE, m_id, pTarget->m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
        returnValue = ACTION_DONE;
        goto release;
    case 131:
        // Kill (0x44EA6E-0x44EB32): a null target or one already dead
        // (STATE_DEAD in the derived m_generalState, CGameSprite+0x920) is
        // left alone; both jumps go to the tail that forces ACTION_DONE
        // (0x44E1DB). Otherwise CGameSprite::sub_761650 (0x761650) runs on
        // the target and a death effect (id 13, amount 0, flags 4,
        // field_18C 1, sourced to the target's id) is queued on it by
        // message.
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        bShare = TRUE;
        if (pTarget != NULL
            && (static_cast<CGameSprite*>(pTarget)->m_derivedStats.m_generalState
                   & STATE_DEAD)
                == 0) {
            static_cast<CGameSprite*>(pTarget)->sub_761650();
            // NOTE: both allocation-failure branches are dead in tree
            // (throwing new): the effect new's failure jump (0x44EAC4)
            // zeroes the pointer and the next store (0x44EAE4)
            // dereferences it, and a failed message new posts NULL
            // (0x44EB2B). The buffer is owned by the message. The default
            // CGameEffectDeath construction covers the inlined writes
            // (vtable 0x8487E8, m_effectID 13 at +0xC); field_18C is
            // then set to 1 (+0x18C).
            CGameEffectDeath* pEffect = new CGameEffectDeath();
            pEffect->m_effectAmount = 0;
            pEffect->m_dwFlags = 4;
            pEffect->m_sourceID = pTarget->GetId();
            pEffect->field_18C = 1;
            message = new CMessageAddEffect(pEffect, GetId(), pTarget->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
        // The AddMessage call site (0x44E1D6, reached through 0x44E1C7)
        // falls into the tail that forces ACTION_DONE (0x44E1DB) before the
        // release block.
        returnValue = ACTION_DONE;
        goto release;
    case 132:
        // VerbalConstant
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        bShare = TRUE;
        if (pTarget != NULL) {
            message = new CMessageVerbalConstant(m_curAction.m_specificID, m_id, pTarget->m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
        returnValue = ACTION_DONE;
        goto release;
    case 133:
        // ClearActions (0x44EC0A-0x44EE8B): clear the actee's action queue,
        // then queue a NO_ACTION on it so it stops whatever it was doing.
        pTarget = ExecuteActionGetActee(this);
        bShare = TRUE;
        if (pTarget == NULL) {
            // 0x451451: ACTION_ERROR straight to release (entry default).
            returnValue = ACTION_ERROR;
            goto release;
        }
        returnValue = ClearActions(pTarget);
        message = new CMessageAddAction(CAIAction(CAIAction::NO_ACTION, CPoint(-1, -1), 0, -1),
            m_id,
            pTarget->m_id);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        goto release;
    case 136:
        // UnlockScroll
        g_pBaldurChitin->m_pEngineWorld->m_scrollLockId = CGameObjectArray::INVALID_INDEX;
        returnValue = ACTION_DONE;
        goto done;
    case 140:
        // GiveItemCreate
        pTarget = ExecuteActionGetActee(this);
        bShare = TRUE;
        returnValue = GiveItemCreate(pTarget);
        goto release;
    case 141:
        // GivePartyGoldGlobal
        returnValue = GivePartyGoldGlobal();
        goto done;
    case 143:
        // OpenDoor
        pTarget = GetTargetShareType(CGameObject::TYPE_DOOR);
        bShare = TRUE;
        returnValue = OpenDoor(static_cast<CGameDoor*>(pTarget));
        goto release;
    case 144:
        // CloseDoor
        pTarget = GetTargetShareType(CGameObject::TYPE_DOOR);
        bShare = TRUE;
        returnValue = CloseDoor(static_cast<CGameDoor*>(pTarget));
        goto release;
    case 151:
        // DisplayString
        pTarget = GetTargetShareType(CGameObject::TYPE_AIBASE);
        bShare = TRUE;
        returnValue = DisplayString(static_cast<CGameAIBase*>(pTarget));
        goto release;
    case 153:
        // ChangeEnemyAlly
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        bShare = TRUE;
        if (pTarget != NULL) {
            CGameSprite* pSprite = static_cast<CGameSprite*>(pTarget);
            CAIObjectType typeAI;
            CAIObjectType typeAILive;
            CAIObjectType typeAIStart;
            typeAI.Set(pSprite->GetAIType());
            typeAILive.Set(pSprite->m_liveTypeAI);
            typeAIStart.Set(pSprite->m_startTypeAI);
            typeAI.SetEnemyAlly(static_cast<BYTE>(m_curAction.m_specificID));
            typeAILive.SetEnemyAlly(static_cast<BYTE>(m_curAction.m_specificID));
            typeAIStart.SetEnemyAlly(static_cast<BYTE>(m_curAction.m_specificID));
            pSprite->SetAIType(typeAI, TRUE, FALSE);
            pSprite->m_liveTypeAI.Set(typeAILive);
            pSprite->m_startTypeAI.Set(typeAIStart);
            if (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL
                || g_pChitin->cNetwork.m_idLocalPlayer == pSprite->m_remotePlayerID) {
                message = new CMessageSpriteUpdate(pSprite, pSprite->m_id, pSprite->m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            } else if (g_pChitin->cNetwork.m_bConnectionEstablished == TRUE
                && g_pBaldurChitin->GetObjectGame()->m_gameSave.m_cutScene == TRUE) {
                message = new CMessageSpriteUpdate(pSprite, pSprite->m_id, pSprite->m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE,
                    CMessage::BROADCAST_FORCED_OTHERS);
            }
        }
        returnValue = ACTION_DONE;
        goto release;
    case 154:
        // ChangeGeneral
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        bShare = TRUE;
        if (pTarget != NULL) {
            CGameSprite* pSprite = static_cast<CGameSprite*>(pTarget);
            CAIObjectType typeAI;
            CAIObjectType typeAILive;
            CAIObjectType typeAIStart;
            typeAI.Set(pSprite->GetAIType());
            typeAILive.Set(pSprite->m_liveTypeAI);
            typeAIStart.Set(pSprite->m_liveTypeAI);
            typeAI.SetGeneral(static_cast<BYTE>(m_curAction.m_specificID));
            typeAILive.SetGeneral(static_cast<BYTE>(m_curAction.m_specificID));
            typeAIStart.SetGeneral(static_cast<BYTE>(m_curAction.m_specificID));
            pSprite->SetAIType(typeAI, TRUE, FALSE);
            pSprite->m_liveTypeAI.Set(typeAILive);
            pSprite->m_startTypeAI.Set(typeAIStart);
            if (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL
                || g_pChitin->cNetwork.m_idLocalPlayer == pSprite->m_remotePlayerID) {
                message = new CMessageSpriteUpdate(pSprite, pSprite->m_id, pSprite->m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }
        }
        returnValue = ACTION_DONE;
        goto release;
    case 155:
        // ChangeRace
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        bShare = TRUE;
        if (pTarget != NULL) {
            CGameSprite* pSprite = static_cast<CGameSprite*>(pTarget);
            CAIObjectType typeAI;
            CAIObjectType typeAILive;
            CAIObjectType typeAIStart;
            typeAI.Set(pSprite->GetAIType());
            typeAILive.Set(pSprite->m_liveTypeAI);
            typeAIStart.Set(pSprite->m_liveTypeAI);
            typeAI.SetRace(static_cast<BYTE>(m_curAction.m_specificID));
            typeAILive.SetRace(static_cast<BYTE>(m_curAction.m_specificID));
            typeAIStart.SetRace(static_cast<BYTE>(m_curAction.m_specificID));
            pSprite->SetAIType(typeAI, TRUE, FALSE);
            pSprite->m_liveTypeAI.Set(typeAILive);
            pSprite->m_startTypeAI.Set(typeAIStart);
            if (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL
                || g_pChitin->cNetwork.m_idLocalPlayer == pSprite->m_remotePlayerID) {
                message = new CMessageSpriteUpdate(pSprite, pSprite->m_id, pSprite->m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }
        }
        returnValue = ACTION_DONE;
        goto release;
    case 156:
        // ChangeClass: the original (0x44F55E-0x44F621) constructs three
        // default CAIObjectType locals and destroys them again without
        // storing a field or calling SetAIType - the action changes
        // nothing but the target share.
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        bShare = TRUE;
        if (pTarget != NULL) {
            CAIObjectType typeAI;
            CAIObjectType typeAILive;
            CAIObjectType typeAIStart;
        }
        returnValue = ACTION_DONE;
        goto release;
    case 157:
        // ChangeSpecifics
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        bShare = TRUE;
        if (pTarget != NULL) {
            CGameSprite* pSprite = static_cast<CGameSprite*>(pTarget);
            CAIObjectType typeAI;
            CAIObjectType typeAILive;
            CAIObjectType typeAIStart;
            typeAI.Set(pSprite->GetAIType());
            typeAILive.Set(pSprite->m_liveTypeAI);
            typeAIStart.Set(pSprite->m_liveTypeAI);
            typeAI.SetSpecific(static_cast<BYTE>(m_curAction.m_specificID));
            typeAILive.SetSpecific(static_cast<BYTE>(m_curAction.m_specificID));
            typeAIStart.SetSpecific(static_cast<BYTE>(m_curAction.m_specificID));
            pSprite->SetAIType(typeAI, TRUE, FALSE);
            pSprite->m_liveTypeAI.Set(typeAILive);
            pSprite->m_startTypeAI.Set(typeAIStart);
            if (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL
                || g_pChitin->cNetwork.m_idLocalPlayer == pSprite->m_remotePlayerID) {
                message = new CMessageSpriteUpdate(pSprite, pSprite->m_id, pSprite->m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }
        }
        returnValue = ACTION_DONE;
        goto release;
    case 158:
        // ChangeGender
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        bShare = TRUE;
        if (pTarget != NULL) {
            CGameSprite* pSprite = static_cast<CGameSprite*>(pTarget);
            CAIObjectType typeAI;
            CAIObjectType typeAILive;
            CAIObjectType typeAIStart;
            typeAI.Set(pSprite->GetAIType());
            typeAILive.Set(pSprite->m_liveTypeAI);
            typeAIStart.Set(pSprite->m_liveTypeAI);
            typeAI.SetGender(static_cast<BYTE>(m_curAction.m_specificID));
            typeAILive.SetGender(static_cast<BYTE>(m_curAction.m_specificID));
            typeAIStart.SetGender(static_cast<BYTE>(m_curAction.m_specificID));
            pSprite->SetAIType(typeAI, TRUE, FALSE);
            pSprite->m_liveTypeAI.Set(typeAILive);
            pSprite->m_startTypeAI.Set(typeAIStart);
            if (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL
                || g_pChitin->cNetwork.m_idLocalPlayer == pSprite->m_remotePlayerID) {
                message = new CMessageSpriteUpdate(pSprite, pSprite->m_id, pSprite->m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }
        }
        returnValue = ACTION_DONE;
        goto release;
    case 159:
        // ChangeAlignment
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        bShare = TRUE;
        if (pTarget != NULL) {
            CGameSprite* pSprite = static_cast<CGameSprite*>(pTarget);
            CAIObjectType typeAI;
            CAIObjectType typeAILive;
            CAIObjectType typeAIStart;
            typeAI.Set(pSprite->GetAIType());
            typeAILive.Set(pSprite->m_liveTypeAI);
            typeAIStart.Set(pSprite->m_liveTypeAI);
            typeAI.SetAlignment(static_cast<BYTE>(m_curAction.m_specificID));
            typeAILive.SetAlignment(static_cast<BYTE>(m_curAction.m_specificID));
            typeAIStart.SetAlignment(static_cast<BYTE>(m_curAction.m_specificID));
            pSprite->SetAIType(typeAI, TRUE, FALSE);
            pSprite->m_liveTypeAI.Set(typeAILive);
            pSprite->m_startTypeAI.Set(typeAIStart);
            if (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL
                || g_pChitin->cNetwork.m_idLocalPlayer == pSprite->m_remotePlayerID) {
                message = new CMessageSpriteUpdate(pSprite, pSprite->m_id, pSprite->m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }
        }
        returnValue = ACTION_DONE;
        goto release;
    case 160:
        // ApplySpell
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        if (pTarget == NULL && m_curAction.m_acteeID.m_nInstance != CGameObjectArray::INVALID_INDEX) {
            CAIObjectType typeAI;
            typeAI.m_nInstance = m_curAction.m_acteeID.m_nInstance;
            pTarget = GetTargetShareAIType(typeAI, CGameObject::TYPE_SPRITE);
        }
        bShare = TRUE;
        returnValue = ApplySpell(pTarget);
        goto release;
    case 161:
        // IncrementChapter
        if (m_curAction.m_specificID2 == 0) {
            m_curAction.m_specificID = -1;
        }
        returnValue = IncrementChapter();
        goto done;
    case 162:
        // ReputationSet
        {
            CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
            SHORT nAdjustment = static_cast<SHORT>((m_curAction.m_specificID - pGame->m_nReputation / CInfGame::REPUTATION_MULTIPLIER)
                * CInfGame::REPUTATION_MULTIPLIER);
            pGame->ReputationAdjustment(nAdjustment, FALSE);
        }
        returnValue = ACTION_DONE;
        goto done;
    case 163:
        // ReputationInc
        g_pBaldurChitin->GetObjectGame()->ReputationAdjustment(static_cast<SHORT>(m_curAction.m_specificID * CInfGame::REPUTATION_MULTIPLIER),
            FALSE);
        returnValue = ACTION_DONE;
        goto done;
    case 164:
        // AddexperienceParty
        g_pBaldurChitin->GetObjectGame()->AddExperienceParty(m_curAction.m_specificID, FALSE, FALSE);
        returnValue = ACTION_DONE;
        goto done;
    case 165:
        // AddexperiencePartyGlobal
        returnValue = AddexperiencePartyGlobal();
        goto done;
    case 167:
        returnValue = StartMovie();
        goto done;
    case 169:
        returnValue = DestroyItem();
        goto done;
    case 170:
        returnValue = RevealAreaOnMap();
        goto done;
    case 172:
        // ChangeTileState
        pTarget = GetTargetShareType(CGameObject::TYPE_TILED_OBJECT);
        bShare = TRUE;
        returnValue = ChangeTileState(static_cast<CGameTiledObject*>(pTarget));
        goto release;
    case 173:
        // AddJournalEntry
        g_pBaldurChitin->GetObjectGame()->m_cJournal.AddEntry(m_curAction.m_specificID, 0);
        returnValue = ACTION_DONE;
        goto done;
    case 177:
        // TriggerActivation
        pTarget = GetTargetShareType(CGameObject::TYPE_TRIGGER);
        bShare = TRUE;
        returnValue = TriggerActivation(static_cast<CGameTrigger*>(pTarget));
        goto release;
    case 183:
        returnValue = MultiPlayerSync();
        goto done;
    case 185: {
        // SetMasterArea (0x44FDCF-0x44FED7): the action string must be
        // found in the masterArea 2DA (m_ruleTables.m_tMasterArea, label
        // search, CInfGame+0xA60); only then is the named loaded area
        // looked up once via CInfGame::GetArea and stored into
        // m_pGameAreaMaster (CInfGame+0x3812) with ACTION_DONE. A name
        // missing from the 2DA concatenates the original's literal
        // (0x8A7F90) into the never-read sError; an unloaded area errors
        // silently; both return ACTION_ERROR without touching pTarget.
        CString sArea = m_curAction.GetString1();
        CPoint ptLocation(-1, -1);
        if (g_pBaldurChitin->GetObjectGame()->m_ruleTables.m_tMasterArea.Find(sArea, ptLocation, TRUE) == FALSE) {
            sError = m_curAction.GetString1() + ": That's no master area, that's my wife!";
            returnValue = ACTION_ERROR;
        } else {
            CGameArea* pArea = g_pBaldurChitin->GetObjectGame()->GetArea(m_curAction.GetString1());
            if (pArea == NULL) {
                returnValue = ACTION_ERROR;
            } else {
                g_pBaldurChitin->GetObjectGame()->m_pGameAreaMaster = pArea;
                returnValue = ACTION_DONE;
            }
        }
        goto done;
    }
    case 265:
        // ExpansionEndCredits
        CInfGame::byte_8E7528 = TRUE;
        // FALLTHROUGH
    case 186:
        // EndCredits
        g_pBaldurChitin->m_pEngineWorld->ReadyEndCredits(FALSE);
        returnValue = ACTION_DONE;
        goto done;
    case 187:
        returnValue = StartMusic();
        goto done;
    case 189:
        // LeaveAreaLUAPanic
        returnValue = LeaveAreaLUAPanic();
        goto done;
    case 190:
        returnValue = FinalSave();
        goto done;
    case 195:
        // Lock
        pTarget = GetTargetShareType(CGameObject::TYPE_AIBASE);
        bShare = TRUE;
        returnValue = Lock(static_cast<CGameAIBase*>(pTarget));
        goto release;
    case 196:
        // Unlock
        pTarget = GetTargetShareType(CGameObject::TYPE_AIBASE);
        bShare = TRUE;
        returnValue = Unlock(static_cast<CGameAIBase*>(pTarget));
        goto release;
    case 197:
        // MoveGlobal
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        bShare = TRUE;
        returnValue = MoveGlobal(static_cast<CGameSprite*>(pTarget));
        goto release;
    case 199:
        // TextScreen (0x44FFB9-0x450093): the arm opens with
        // UTIL_ASSERT(FALSE) (original CGameAIBase.cpp:1439); UtilAssert
        // never returns, so the message post below is dead code in the
        // original too -- ported faithfully. Would-be behavior: build
        // CMessageStartTextScreen from GetString1() with this sprite's id
        // as both source and target, post it, and return ACTION_DONE.
        UTIL_ASSERT(FALSE);
        message = new CMessageStartTextScreen(CResRef(m_curAction.GetString1()), m_id, m_id);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        returnValue = ACTION_DONE;
        goto done;
    case 201:
        // DetectSecretDoor
        pTarget = GetTargetShareType(CGameObject::TYPE_DOOR);
        bShare = TRUE;
        returnValue = DetectSecretDoor(static_cast<CGameDoor*>(pTarget));
        goto release;
    case 202:
        returnValue = FadeToColor();
        goto done;
    case 203:
        returnValue = FadeFromColor();
        goto done;
    case 253:
        // FadeToAndFromColor
        returnValue = sub_467970();
        goto done;
    case 209:
        // SpawnPtActivate
        pTarget = GetTargetShareType(CGameObject::TYPE_SPAWNING);
        bShare = TRUE;
        returnValue = SpawnPtActivate(static_cast<CGameSpawning*>(pTarget));
        goto release;
    case 210:
        // SpawnPtDeactivate
        pTarget = GetTargetShareType(CGameObject::TYPE_SPAWNING);
        bShare = TRUE;
        returnValue = SpawnPtDeactivate(static_cast<CGameSpawning*>(pTarget));
        goto release;
    case 211:
        // SpawnPtSpawn
        pTarget = GetTargetShareType(CGameObject::TYPE_SPAWNING);
        bShare = TRUE;
        returnValue = SpawnPtSpawn(static_cast<CGameSpawning*>(pTarget));
        goto release;
    case 213:
        // StaticStart
        pTarget = GetTargetShareType(CGameObject::TYPE_STATIC);
        bShare = TRUE;
        returnValue = StaticStart(static_cast<CGameStatic*>(pTarget), TRUE);
        goto release;
    case 214:
        // StaticStop
        pTarget = GetTargetShareType(CGameObject::TYPE_STATIC);
        bShare = TRUE;
        returnValue = StaticStart(static_cast<CGameStatic*>(pTarget), FALSE);
        goto release;
    case 216:
        // The familiar's join (CMessageFamiliarAdd TRUE).
        message = new CMessageFamiliarAdd(TRUE, m_id, m_id);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        returnValue = ACTION_DONE;
        goto done;
    case 217:
        // The familiar's leave (CMessageFamiliarAdd FALSE).
        message = new CMessageFamiliarAdd(FALSE, m_id, m_id);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        returnValue = ACTION_DONE;
        goto done;
    case 218:
        if (GetObjectType() != CGameObject::TYPE_SPRITE) {
            returnValue = ACTION_ERROR;
            goto done;
        }
        AutoPause(0x100);
        returnValue = ACTION_DONE;
        goto done;
    case 220:
    case 226:
        // TakeItemListParty, TakeItemListPartyNum
        returnValue = TakeItemListParty();
        goto done;
    case 227:
        // CreateCreatureObject
        pTarget = GetTargetShareType(CGameObject::TYPE_AIBASE);
        bShare = TRUE;
        returnValue = CreateCreatureObject(pTarget);
        goto release;
    case 230:
        // RestParty
        g_pBaldurChitin->GetObjectGame()->RestParty(1, 1);
        returnValue = ACTION_DONE;
        goto done;
    case 233:
        // HideCreature (0x452035-0x4520ED)
        {
            BYTE bHide = static_cast<BYTE>(m_curAction.GetSpecifics());
            pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
            if (pTarget == NULL) {
                // 0x452054 jumps straight to the epilogue, keeping the
                // entry default (ACTION_ERROR).
                returnValue = ACTION_ERROR;
                goto done;
            }
            bShare = TRUE;
            returnValue = static_cast<CGameSprite*>(pTarget)->sub_75F3D0(bHide);
            if (g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL) {
                message = new CMessage90(pTarget->GetId(), pTarget->GetId(), bHide);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }
        }
        goto release;
    case 235:
        // PlaySequence (0x450B52-0x450BD9)
        pTarget = sub_45C290();
        if (pTarget == NULL) {
            // 0x450B5D jumps to the forced-done tail (0x44E1DB) with no
            // message to send.
            returnValue = ACTION_DONE;
            goto release;
        }
        {
            // The bounds test reads two bytes of the SEQ_* value table at
            // 0x85BBB0: index 0 (0, SEQ_ATTACK) and index 11 (0x11,
            // SEQ_SLEEP).
            LONG nSequence = m_curAction.GetSpecifics();
            if (nSequence < CGAMESPRITE_SEQ_ATTACK || nSequence > CGAMESPRITE_SEQ_SLEEP) {
                // Out of range also takes the forced-done tail (0x44E1DB).
                returnValue = ACTION_DONE;
                goto release;
            }
            bShare = TRUE;
            message = new CMessageSetSequence(static_cast<BYTE>(nSequence), m_id, pTarget->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            // The AddMessage call site (0x44E1D6) falls into the tail that
            // forces ACTION_DONE (0x44E1DB) before the release block.
            returnValue = ACTION_DONE;
        }
        goto release;
    case 238:
        // AddXPVar (0x4508CE-0x450B52)
        {
            C2DArray tExperience;
            tExperience.Load(CResRef("exptable"));

            CString sLabel = m_curAction.GetString1();
            CPoint ptLocation;
            if (!tExperience.Find(sLabel, ptLocation, TRUE)) {
                returnValue = ACTION_ERROR;
                goto done;
            }

            // The row label is found with x = -1; the value is in column 0
            // (`mov [esp+0x70], ebp` at 0x4509FE).
            ptLocation.x = 0;

            if (m_curAction.m_specificID != 0) {
                STR_RES strRes;
                if (g_pBaldurChitin->GetTlkTable().Fetch(m_curAction.m_specificID, strRes)) {
                    g_pBaldurChitin->GetBaldurMessage()->DisplayText(CString(""),
                        strRes.szText,
                        RGB(200, 255, 200),
                        RGB(200, 255, 200),
                        -1,
                        m_id,
                        m_id);
                }
            }

            g_pBaldurChitin->GetObjectGame()->AddExperienceParty(tExperience.GetAtLong(ptLocation), FALSE, FALSE);
            returnValue = ACTION_DONE;
        }
        goto done;
    case 241:
        // FloatMessage (0x4507E1-0x4508CE)
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        if (pTarget == NULL) {
            pTarget = GetTargetShareType(CGameObject::TYPE_DOOR);
            if (pTarget == NULL) {
                returnValue = ACTION_DONE;
                goto done;
            }
        }
        bShare = TRUE;
        if (g_pChitin->cNetwork.m_nServiceProvider == CNetwork::SERV_PROV_NULL) {
            pTarget->FloatText(m_curAction.m_specificID, 10, 5);
        } else {
            message = new CMessageFloatText(m_id, pTarget->GetId(), m_curAction.m_specificID, FALSE);
            if (g_pChitin->cNetwork.m_nServiceProvider == 0
                || g_pChitin->cNetwork.m_idLocalPlayer == pTarget->m_remotePlayerID) {
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            } else {
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE, CMessage::BROADCAST_FORCED);
            }
        }
        returnValue = ACTION_DONE;
        goto release;
    case 242:
        // ChangeStat (0x4523BA-0x452487): specifics1 is the stat,
        // specifics2 the value and the low byte of specifics3 the modify
        // flag, applied to the target sprite. A NULL target jumps straight
        // to the done tail, so the entry default ACTION_ERROR is returned
        // and the share is never released.
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        if (pTarget == NULL) {
            goto done;
        }
        {
            CGameSprite* pSprite = static_cast<CGameSprite*>(pTarget);
            DWORD nStat = static_cast<DWORD>(m_curAction.GetSpecifics());
            DWORD nValue = static_cast<DWORD>(m_curAction.GetSpecifics2());
            BOOL bModify = static_cast<BYTE>(m_curAction.GetSpecifics3());
            pSprite->SetAtOffset(nStat, nValue, bModify);
            returnValue = ACTION_DONE;
            if (g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL) {
                // The caller and target are both the target's own id.
                message = new CMessageChangeStat(static_cast<WORD>(nStat), nValue,
                    static_cast<BOOLEAN>(bModify), pSprite->GetId(), pSprite->GetId());
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }
            // 0x452487 jumps to the done tail (0x4526F7); no release.
            goto done;
        }
    case 243: {
        // IncrementGlobalOnce
        // Original: 0x450299-0x450648. Both action strings are split on ':'
        // and trimmed. String1 names the once-guard variable (its scope and
        // name), string2 the variable incremented when the guard fires.
        CString sString1 = m_curAction.GetString1();
        sString1.MakeUpper();
        int nColon1 = sString1.Find(':');
        CString sScope1 = sString1.Left(nColon1);
        sScope1.TrimLeft();
        sScope1.TrimRight();
        CString sName1 = sString1.Right(sString1.GetLength() - nColon1 - 1);
        sName1.TrimLeft();
        sName1.TrimRight();

        CString sString2 = m_curAction.GetString2();
        sString2.MakeUpper();
        int nColon2 = sString2.Find(':');
        CString sScope2 = sString2.Left(nColon2);
        sScope2.TrimLeft();
        sScope2.TrimRight();
        CString sName2 = sString2.Right(sString2.GetLength() - nColon2 - 1);
        sName2.TrimLeft();
        sName2.TrimRight();

        // The guard: look string1's variable up -- in the game variables
        // for GLOBAL, else in GetArea(sScope1)'s. Unlike SetGlobal this
        // lookup has no LOCALS or MYAREA handling, and the original calls
        // GetVariables on the area pointer without a null check.
        CVariable* pVariable;
        if (strcmp(sScope1, "GLOBAL") == 0) {
            pVariable = g_pBaldurChitin->GetObjectGame()->GetVariables()->FindKey(sName1);
        } else {
            pVariable = g_pBaldurChitin->GetObjectGame()->GetArea(sScope1)->GetVariables()->FindKey(sName1);
        }

        // An existing guard variable holding 1 means the action already ran:
        // the original jumps straight to the case tail, changing nothing.
        if (pVariable == NULL || pVariable->m_intValue != 1) {
            // Set the guard: string1 is rejoined as scope1+name1, m_specificID
            // is forced to 1 and SetGlobal runs. (The original passes SetGlobal
            // a by-value CString copy of the name that the callee never reads
            // -- dead argument, not even set up on the area path -- so the
            // in-tree SetGlobal() without it is the same function.)
            LONG nSpecificID = m_curAction.m_specificID;
            m_curAction.SetString1(sScope1 + sName1);
            m_curAction.m_specificID = 1;
            SetGlobal();

            // Increment string2's variable by the saved m_specificID.
            m_curAction.SetString1(sScope2 + sName2);
            m_curAction.m_specificID = nSpecificID;
            IncrementGlobal();
        }

        // Both helpers' return values are discarded; the tail forces
        // ACTION_DONE (0x4505EB: or esi, -1 before the destructors).
        returnValue = ACTION_DONE;
        goto done;
    }
    case 244:
        // DeleteJournalEntry
        // Original: 0x450648-0x45066E: the game's embedded journal
        // (chitin+0x1C54 game, journal at game+0x44C8) gets
        // DeleteEntry(m_specificID); the tail forces ACTION_DONE.
        g_pBaldurChitin->GetObjectGame()->GetJournal()->DeleteEntry(m_curAction.m_specificID);
        returnValue = ACTION_DONE;
        goto done;
    case 245:
        // JournalEntryDone
        // Original: 0x45066E-0x450694: SetQuestDone(m_specificID) on the
        // same embedded journal; the tail forces ACTION_DONE.
        g_pBaldurChitin->GetObjectGame()->GetJournal()->SetQuestDone(m_curAction.m_specificID);
        returnValue = ACTION_DONE;
        goto done;
    case 247:
        // BitGlobal
        returnValue = BitGlobal();
        goto done;
    case 248:
        // GlobalBitGlobal
        returnValue = GlobalBitGlobal();
        goto done;
    case 251: {
        // DropInventoryEX (0x45248C-0x452570): drop the target's whole
        // inventory (non-critical items, no exceptions) and report the
        // dropper's own return. The equipment message is broadcast only
        // when a session is open, the target is not locally controlled and
        // a cutscene is running.
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        if (pTarget == NULL) {
            // 0x451451: ACTION_ERROR straight to release (entry default).
            returnValue = ACTION_ERROR;
            goto release;
        }
        bShare = TRUE;
        CGameSprite* pSprite = static_cast<CGameSprite*>(pTarget);
        returnValue = pSprite->DropInventory(FALSE, CString(""));
        if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
            && pTarget->InControl() == FALSE
            && g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_cutScene == TRUE) {
            message = new CMessageSpriteEquipment(pSprite, pSprite->GetId(), pSprite->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE,
                CMessage::BROADCAST_FORCED_OTHERS);
        }
        goto release;
    }
    case 254:
        // SetHP
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        if (pTarget != NULL) {
            CGameSprite* pSprite = static_cast<CGameSprite*>(pTarget);
            bShare = TRUE;
            if ((pSprite->m_derivedStats.m_generalState & STATE_DEAD) == 0) {
                if (m_curAction.m_specificID > pSprite->m_derivedStats.GetAtOffset(STAT_MAXHITPOINTS)) {
                    pSprite->m_baseStats.m_hitPoints = static_cast<SHORT>(pSprite->m_derivedStats.GetAtOffset(STAT_MAXHITPOINTS));
                } else {
                    pSprite->m_baseStats.m_hitPoints = static_cast<SHORT>(m_curAction.m_specificID);
                }
            }
        }
        returnValue = ACTION_DONE;
        goto release;
    case 258:
        // RestUntilHealed
        {
            STRREF strError;
            if (g_pBaldurChitin->GetObjectGame()->CanRestParty(strError, 0, 1,
                    (m_curAction.GetFlags() & 2) != 0)
                == 1) {
                CInfGame::m_bHealPartyOnRest = TRUE;
                g_pBaldurChitin->GetObjectGame()->RestParty(1, TRUE);
                CInfGame::m_bHealPartyOnRest = FALSE;
                returnValue = ACTION_DONE;
                goto done;
            }

            // The rest was refused: show why (Fetch only fills strRes when the
            // strref resolves; otherwise no feedback is displayed).
            STR_RES strRes;
            if (g_pBaldurChitin->GetTlkTable().Fetch(strError, strRes)) {
                g_pBaldurChitin->GetBaldurMessage()->DisplayText(CString(""),
                    strRes.szText,
                    RGB(0, 255, 0),
                    RGB(0, 255, 0),
                    -1,
                    GetId(),
                    GetId());
            }
        }
        returnValue = ACTION_DONE;
        goto done;
    case 259:
        // SetVisualRange
        message = new CMessage92(m_id, m_id, m_curAction.GetSpecifics());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        returnValue = ACTION_DONE;
        goto done;
    case 260:
        // JumpToPointInstant
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        if (pTarget == NULL) {
            returnValue = ACTION_ERROR;
            goto release;
        }
        bShare = TRUE;
        static_cast<CGameSprite*>(pTarget)->JumpToPoint(m_curAction.GetDest(), FALSE);
        // The success tail jumps to 0x44E1DB, which ors the return value to
        // ACTION_DONE before the release block runs.
        returnValue = ACTION_DONE;
        goto release;
    case 266:
        // DisplayMessage
        {
            STR_RES strRes;
            if (g_pBaldurChitin->GetTlkTable().Fetch(m_curAction.GetSpecifics(), strRes)) {
                g_pBaldurChitin->GetBaldurMessage()->DisplayText(CString(""),
                    strRes.szText,
                    RGB(160, 200, 215),
                    RGB(160, 200, 215),
                    -1,
                    GetId(),
                    GetId());
            }
        }
        returnValue = ACTION_DONE;
        goto done;
    case 267:
        // ExportParty
        {
            CString sFileName;
            int nExportIndex = 1;

            g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(-1, 26827, RGB(0, 0, 0), RGB(255, 255, 0), -1, 0, 0);

            for (int nCharacter = 0;
                 nCharacter < g_pBaldurChitin->GetObjectGame()->GetNumCharacters();
                 nCharacter++) {
                CGameObject* pObject;
                LONG nCharacterId = g_pBaldurChitin->GetObjectGame()->GetCharacterId(nCharacter);
                BYTE rc;

                do {
                    rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(nCharacterId,
                        CGameObjectArray::THREAD_ASYNCH,
                        &pObject,
                        INFINITE);
                } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

                if (rc == CGameObjectArray::SUCCESS) {
                    // The export flag is m_flags bit 0x800 of the base stats.
                    if ((static_cast<CGameSprite*>(pObject)->GetBaseStats()->m_flags & 0x800) != 0) {
                        sFileName.Format("%s%d", (LPCSTR)m_curAction.GetString1(), nExportIndex);
                        g_pBaldurChitin->GetObjectGame()->CharacterExport(nCharacterId, sFileName);
                        nExportIndex++;
                    }

                    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nCharacterId,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);
                }
            }
        }
        returnValue = ACTION_DONE;
        goto done;
    case 268:
        // ClearPartyEffects
        {
            for (int nCharacter = 0;
                 nCharacter < g_pBaldurChitin->GetObjectGame()->GetNumCharacters();
                 nCharacter++) {
                CGameObject* pObject;
                LONG nCharacterId = g_pBaldurChitin->GetObjectGame()->GetCharacterId(nCharacter);
                BYTE rc;

                do {
                    rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(nCharacterId,
                        CGameObjectArray::THREAD_ASYNCH,
                        &pObject,
                        INFINITE);
                } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

                if (rc == CGameObjectArray::SUCCESS) {
                    // The flag test is m_flags bit 0x800 of the base stats.
                    if ((static_cast<CGameSprite*>(pObject)->GetBaseStats()->m_flags & 0x800) != 0) {
                        static_cast<CGameSprite*>(pObject)->sub_761990();
                    }

                    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nCharacterId,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);
                }
            }
        }
        returnValue = ACTION_DONE;
        goto done;
    case 270:
        // ClearSpriteEffects
        // Both paths jump to 0x44E1DB, which ors the return value to ACTION_DONE
        // before the release block runs.
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        if (pTarget != NULL) {
            bShare = TRUE;
            static_cast<CGameSprite*>(pTarget)->sub_761990();
        }
        returnValue = ACTION_DONE;
        goto release;
    case 271:
        // StopJoinRequests
        if (field_596 == 0) {
            field_595 = g_pBaldurChitin->GetObjectGame()->GetMultiplayerSettings()->GetListenToJoinOption();
            g_pBaldurChitin->GetObjectGame()->GetMultiplayerSettings()->SetListenToJoinOption(FALSE, TRUE);
            field_596++;
        }
        returnValue = ACTION_DONE;
        goto done;
    case 272:
        // ResetJoinRequests
        g_pBaldurChitin->GetObjectGame()->GetMultiplayerSettings()->SetListenToJoinOption(field_595, TRUE);
        field_596 = 0;
        returnValue = ACTION_DONE;
        goto done;
    case 273:
        // HideGUI (0x45114E-0x4511D5)
        if (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL) {
            if (!g_pBaldurChitin->GetScreenWorld()->GetManager()->m_bHidden) {
                g_pBaldurChitin->GetScreenWorld()->HideInterface();
                g_pBaldurChitin->GetScreenWorld()->sub_453790(TRUE);
            }
        } else {
            message = new CMessageToggleInterface(TRUE, m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
        returnValue = ACTION_DONE;
        goto done;
    case 274:
        // UnhideGUI (0x4511D5-0x451282)
        if (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL) {
            if (g_pBaldurChitin->GetScreenWorld()->sub_453780()) {
                g_pBaldurChitin->GetScreenWorld()->UnhideInterface();
                g_pBaldurChitin->GetScreenWorld()->sub_453790(FALSE);
            }
        } else {
            message = new CMessageToggleInterface(FALSE, m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
        returnValue = ACTION_DONE;
        goto done;
    case 275:
        // SaveGame (0x451282-0x4512E9)
        if (!g_pChitin->cNetwork.GetSessionOpen() || g_pChitin->cNetwork.GetSessionHosting() == TRUE) {
            message = new CMessageSaveGame(m_curAction.GetSpecifics(), m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
        returnValue = ACTION_DONE;
        goto done;
    case 278:
        // DropInventoryEXExclude (0x4512E9-0x451362)
        pTarget = sub_45C290();
        if (pTarget == NULL) {
            // 0x451451 sets ACTION_ERROR and jumps straight to the release.
            returnValue = ACTION_ERROR;
            goto release;
        }
        bShare = TRUE;
        if (pTarget->GetObjectType() == CGameObject::TYPE_SPRITE) {
            // The exception list rides as a by-value CString built from the
            // action's second string (0x451312-0x45131C).
            returnValue = static_cast<CGameSprite*>(pTarget)->DropInventory(FALSE,
                m_curAction.GetString1());
            goto release;
        }
        if (pTarget->GetObjectType() == CGameObject::TYPE_CONTAINER) {
            // 0x45134C: the container drops its inventory except the items
            // named by the action's second string; the drop's result is the
            // action return.
            returnValue = static_cast<CGameContainer*>(pTarget)->DropInventoryEXExclude(
                m_curAction.GetString1());
            goto release;
        }
        // Any other object type takes the 0x451451 ACTION_ERROR exit.
        returnValue = ACTION_ERROR;
        goto release;
    case 279:
        // AdvanceTime (0x4513D8-0x451427)
        g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->AdvanceCurrentTime(m_curAction.GetSpecifics());
        g_pBaldurChitin->GetObjectGame()->FeedBack(CInfGame::FEEDBACK_TIMEPASSEDREST,
            m_curAction.GetSpecifics(),
            TRUE);
        returnValue = ACTION_DONE;
        goto done;
    case 280:
        // AdvanceTime (0x451427-0x4514FB): with a resolved sprite target and
        // empty specifics, decodes a cleared item effect (id 0x5D, amount
        // GetSpecifics2(), flags 1, duration type 1) at this sprite's
        // position and broadcasts it as CMessageAddEffect.
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        if (pTarget == NULL) {
            returnValue = ACTION_ERROR;
            goto release;
        }
        bShare = TRUE;
        if (m_curAction.GetSpecifics() != 0) {
            returnValue = ACTION_ERROR;
            goto release;
        }
        {
            ITEM_EFFECT effect;
            CGameEffect::ClearItemEffect(&effect, 0x5D);
            effect.effectAmount = m_curAction.GetSpecifics2();
            effect.dwFlags = 1;
            effect.durationType = 1;
            CGameEffect* pEffect = CGameEffect::DecodeEffect(&effect, m_pos, GetId(), CPoint(-1, -1));
            message = new CMessageAddEffect(pEffect, GetId(), pTarget->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            // The AddMessage call site (0x44E1D6) falls into the tail that
            // forces ACTION_DONE (0x44E1DB) before the release block.
            returnValue = ACTION_DONE;
        }
        goto release;
    case 281:
        // MarkObject (0x451362-0x45139D)
        pTarget = sub_45C290();
        if (pTarget != NULL) {
            bShare = TRUE;
            sub_4530F0(pTarget->GetAIType());
        } else {
            sub_4530F0(CAIObjectType::NOONE);
        }
        returnValue = ACTION_DONE;
        goto release;
    case 282:
        // OverrideAreaDifficulty (0x44E042-0x44E12E)
        {
            CString sArea = m_curAction.GetString1();
            if (!sArea.IsEmpty() && m_curAction.m_specificID != 0) {
                LONG nLevel = m_curAction.m_specificID;
                if (nLevel > 1) {
                    nLevel = 1;
                } else if (nLevel < -1) {
                    nLevel = -1;
                }

                CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
                // 0x452F80: find the entry by area name (case-insensitive).
                CInfGameAreaLevel* pAreaLevel = NULL;
                POSITION pos = pGame->field_4BDC.GetHeadPosition();
                while (pos != NULL) {
                    CInfGameAreaLevel* pCurrent = pGame->field_4BDC.GetNext(pos);
                    if (pCurrent != NULL && pCurrent->m_sArea.CompareNoCase(sArea) == 0) {
                        pAreaLevel = pCurrent;
                        break;
                    }
                }
                if (pAreaLevel == NULL) {
                    pAreaLevel = new CInfGameAreaLevel(sArea);
                    pGame->field_4BDC.AddTail(pAreaLevel);
                }
                // Clear the absolute bit and set the additive bit; the
                // consumer (CGameArea) reads both.
                pAreaLevel->m_dwFlags = (pAreaLevel->m_dwFlags & ~0x1) | 0x2;
                pAreaLevel->m_nLevel = static_cast<BYTE>(nLevel);
            }
            returnValue = ACTION_DONE;
        }
        goto done;
    case 283:
        // SetCriticalPathObject (0x44E12E-0x44E223)
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        if (pTarget == NULL) {
            // 0x44E1DB forces ACTION_DONE before the release block.
            returnValue = ACTION_DONE;
            goto release;
        }
        bShare = TRUE;
        if (m_curAction.m_specificID != 0) {
            static_cast<CGameSprite*>(pTarget)->GetBaseStats()->m_flags |= 0x2000;
        } else {
            static_cast<CGameSprite*>(pTarget)->GetBaseStats()->m_flags &= ~0x2000;
        }
        if (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL
            || g_pChitin->cNetwork.m_idLocalPlayer == pTarget->m_remotePlayerID) {
            message = new CMessageSpriteUpdate(static_cast<CGameSprite*>(pTarget),
                pTarget->GetId(),
                pTarget->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
        // The AddMessage call site (0x44E1D6) falls into the tail that
        // forces ACTION_DONE (0x44E1DB) before the release block.
        returnValue = ACTION_DONE;
        goto release;
    case 284: {
        // SetScriptName (0x44DCDA-0x44E042): the old name is the target's
        // m_scriptName char[32] (0x44DCF4-0x44DD01, CString ctor with length
        // 32), the new name is GetString1 with an empty string turned into
        // "none" (0x44DD38). Nothing happens while the names match
        // (CompareStrings 0x7E8182 is lstrcmpi; 0x44DD5A jumps to the tail).
        // Each of the area and the game named-creature hashes drops the old
        // name when it maps to the target's id (RemoveKey(old, TRUE, id) ==
        // 1), each success queues part of a CMessageSetVariable named by the
        // old name so remote machines do the same removal (m_dwFlags 2 = area
        // scope, 4 = game scope) and registers the new name in the game hash
        // (0x44DE37-0x44DE8B, 0x44DFAC-0x44DFF8). 0x44E03D jumps to the
        // 0x4526F7 done tail: the original never sets bShare and never
        // releases the shared target.
        pTarget = GetTargetShareType(CGameObject::TYPE_AIBASE);
        if (pTarget == NULL) {
            // 0x45127A: ACTION_DONE without the release block.
            returnValue = ACTION_DONE;
            goto done;
        }

        CGameAIBase* pAIBase = static_cast<CGameAIBase*>(pTarget);
        CString sOldName(pAIBase->m_scriptName, SCRIPTNAME_SIZE);
        CString sNewName = m_curAction.GetString1();
        if (strcmp(sNewName, "") == 0) {
            sNewName = "none";
        }

        CMessageSetVariable* pSetVariable = NULL;
        if (_stricmp(sOldName, sNewName) != 0) {
            if (pTarget->m_pArea != NULL
                && pTarget->m_pArea->GetNamedCreatures()->RemoveKey(sOldName, TRUE, pTarget->GetId()) == TRUE) {
                CString sAreaRes;
                pTarget->m_pArea->m_resRef.CopyToString(sAreaRes);
                pSetVariable = new CMessageSetVariable(sAreaRes, sOldName,
                    pTarget->GetId(), pTarget->GetId(), pTarget->GetId());
                pSetVariable->m_dwFlags = 2;

                CVariable varNewName;
                varNewName.SetName(sNewName);
                varNewName.m_intValue = pTarget->GetId();
                g_pBaldurChitin->GetObjectGame()->m_namedCreatures.AddKey(varNewName);
            }

            if (g_pBaldurChitin->GetObjectGame()->GetNamedCreatures()->RemoveKey(sOldName, TRUE, pTarget->GetId()) == TRUE) {
                if (pSetVariable == NULL) {
                    pSetVariable = new CMessageSetVariable("GLOBAL", sOldName,
                        pTarget->GetId(), pTarget->GetId(), pTarget->GetId());
                    pSetVariable->m_dwFlags = 4;
                } else {
                    // 0x44DFA8: the game removal joins the area message.
                    pSetVariable->m_dwFlags |= 4;
                }

                CVariable varNewName;
                varNewName.SetName(sNewName);
                varNewName.m_intValue = pTarget->GetId();
                g_pBaldurChitin->GetObjectGame()->m_namedCreatures.AddKey(varNewName);
            }

            if (pSetVariable != NULL) {
                message = pSetVariable;
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }
        }
        returnValue = ACTION_DONE;
        goto done;
    }
    case 286:
        // AllowAreaResting
        if (m_pArea == NULL) {
            goto done;
        }
        if (m_curAction.m_specificID != 0) {
            m_pArea->m_header.m_flags &= ~0x2;
        } else {
            m_pArea->m_header.m_flags |= 0x2;
        }
        returnValue = ACTION_DONE;
        goto done;
    case 287: {
        // SetHPPercent (0x4514FB-0x451638): share TYPE_SPRITE. A dead target
        // is left alone - the derived m_generalState (0x451523) and the base
        // m_generalState (0x451535) each test bit 0x800 = STATE_DEAD.
        // specifics is clamped to [0,100] (0x451548-0x451580) and scaled to a
        // fraction. With specifics2 == 0 the value is the derived
        // m_nMaxHitPoints, but only stored when that times the fraction is
        // above the base m_hitPoints (0x4515D7-0x4515E0 otherwise jumps to
        // the release tail without a store); with specifics2 != 0 the value
        // is the current base m_hitPoints itself. The store truncates through
        // __ftol into the base m_hitPoints word (0x45161C-0x45162F).
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        if (pTarget == NULL) {
            // 0x45150E: the 0x44E1DB tail forces ACTION_DONE before the
            // release block.
            returnValue = ACTION_DONE;
            goto release;
        }
        bShare = TRUE;

        CGameSprite* pSprite = static_cast<CGameSprite*>(pTarget);
        if ((pSprite->GetDerivedStats()->m_generalState & STATE_DEAD) != 0
            || (pSprite->GetBaseStats()->m_generalState & STATE_DEAD) != 0) {
            returnValue = ACTION_DONE;
            goto release;
        }

        float fPercent = static_cast<float>(m_curAction.GetSpecifics());
        if (fPercent > 100.0f) {
            fPercent = 100.0f;
        } else if (fPercent < 0.0f) {
            fPercent = 0.0f;
        }
        fPercent /= 100.0f;

        float fValue;
        if (m_curAction.GetSpecifics2() == 0) {
            float fBaseHitPoints = static_cast<float>(pSprite->GetBaseStats()->m_hitPoints);
            float fMaxHitPoints = static_cast<float>(pSprite->GetDerivedStats()->m_nMaxHitPoints);
            if (fMaxHitPoints * fPercent <= fBaseHitPoints) {
                // 0x4515E0: done without storing.
                returnValue = ACTION_DONE;
                goto release;
            }
            fValue = fMaxHitPoints;
        } else {
            fValue = static_cast<float>(pSprite->GetBaseStats()->m_hitPoints);
        }

        pSprite->GetBaseStats()->m_hitPoints = static_cast<SHORT>(fValue * fPercent);
        returnValue = ACTION_DONE;
        goto release;
    }
    case 288: {
        // SetDoorFlag (0x451638-0x4516BE and the shared tail at
        // 0x4514DC-0x44E1DB): specifics1 is the flag mask on the target
        // door; a non-zero specifics2 sets the bits, a zero clears them.
        // The door status is always announced to the message handler.
        pTarget = GetTargetShareType(CGameObject::TYPE_DOOR);
        if (pTarget == NULL) {
            // 0x45164C jumps to the ACTION_DONE tail, not the error one.
            returnValue = ACTION_DONE;
            goto release;
        }
        bShare = TRUE;
        CGameDoor* pDoor = static_cast<CGameDoor*>(pTarget);
        DWORD dwFlags = pDoor->GetFlags();
        if (m_curAction.GetSpecifics2() != 0) {
            dwFlags |= static_cast<DWORD>(m_curAction.GetSpecifics());
        } else {
            dwFlags &= ~static_cast<DWORD>(m_curAction.GetSpecifics());
        }
        pDoor->SetFlags(dwFlags);
        // The caller is this actor's own id, the target the door's.
        message = new CMessageDoorStatus(pDoor, m_id, pDoor->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        returnValue = ACTION_DONE;
        goto release;
    }
    case 289:
        // SpellCastEffect
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        bShare = TRUE;
        if (pTarget == NULL) {
            returnValue = ACTION_ERROR;
        } else {
            returnValue = static_cast<CGameSprite*>(pTarget)->SpellCastEffect(m_curAction.GetString1(),
                m_curAction.GetString2(),
                m_curAction.GetSpecifics(),
                m_curAction.GetSpecifics2(),
                m_curAction.GetSpecifics3());
        }
        goto release;
    case 290:
        // SpellHitEffectPoint
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        if (pTarget != NULL) {
            bShare = TRUE;
            // 0x4517A8: the target sprite takes the spell-hit effect at the
            // action's destination (source id 0 in the fifth argument slot).
            returnValue = static_cast<CGameSprite*>(pTarget)->SpellHitEffect(
                static_cast<CGameSprite*>(pTarget),
                m_curAction.GetDest(),
                0,
                m_curAction.GetSpecifics(),
                m_curAction.GetSpecifics2());
        } else {
            pTarget = GetTargetShareType(CGameObject::TYPE_CONTAINER);
            if (pTarget != NULL) {
                bShare = TRUE;
                // 0x4517F9: the container takes its own spell-hit effect,
                // passing itself as the hit object.
                returnValue = static_cast<CGameContainer*>(pTarget)->SpellHitEffect(
                    static_cast<CGameContainer*>(pTarget),
                    pTarget->GetPos(),
                    pTarget->GetId(),
                    m_curAction.GetSpecifics(),
                    m_curAction.GetSpecifics2());
            } else {
                returnValue = ACTION_ERROR;
            }
        }
        goto release;
    case 291:
        // SpellHitEffectSprite
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        bShare = TRUE;
        if (pTarget == NULL) {
            returnValue = ACTION_ERROR;
        } else {
            CGameObject* pActee = m_curAction.GetActee2().sub_40CB20(this, CGameObject::TYPE_SPRITE, FALSE);
            if (pActee == NULL) {
                returnValue = ACTION_ERROR;
            } else if (pActee->GetArea() == NULL) {
                // NOTE: the original jumps to the ACTION_ERROR tail here
                // without releasing the share sub_40CB20 acquired.
                returnValue = ACTION_ERROR;
            } else {
                // 0x451882: the target sprite takes the spell-hit effect at
                // the actee's position and id.
                returnValue = static_cast<CGameSprite*>(pTarget)->SpellHitEffect(
                    static_cast<CGameSprite*>(pTarget),
                    pActee->GetPos(),
                    pActee->GetId(),
                    m_curAction.GetSpecifics(),
                    m_curAction.GetSpecifics2());
                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(pActee->GetId(),
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            }
        }
        goto release;
    case 292:
        // ChunkCreature
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        bShare = TRUE;
        if (pTarget == NULL) {
            returnValue = ACTION_ERROR;
        } else {
            static_cast<CGameSprite*>(pTarget)->sub_761650();
            // NOTE: the original news the 0x30-byte buffer (0x4518C5) and
            // asserts on failure (0x45195E, "Memory Allocation problem"); it
            // never frees the buffer.
            ITEM_EFFECT* effect = new ITEM_EFFECT;
            CGameEffect::ClearItemEffect(effect, 13);
            effect->dwFlags = 8;
            effect->numDice = 0;
            effect->effectAmount = 0x31;
            CGameEffect* pEffect = CGameEffect::DecodeEffect(effect, CPoint(-1, -1), -1, CPoint(-1, -1));
            message = new CMessageAddEffect(pEffect, GetId(), pTarget->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            returnValue = ACTION_DONE;
        }
        goto release;
    case 293:
        // UnHide
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        if (pTarget == NULL) {
            // The original jumps to the done tail (0x4526F7) before bShare is
            // set, keeping the ACTION_ERROR entry default.
            goto done;
        }
        bShare = TRUE;
        if (static_cast<CGameSprite*>(pTarget)->sub_453170() != 1) {
            // A target that is not hiding jumps to the tail that forces
            // ACTION_DONE (0x44E1DB) before the release block.
            returnValue = ACTION_DONE;
            goto release;
        }
        {
            CGameSprite* pSprite = static_cast<CGameSprite*>(pTarget);
            pSprite->sub_453160(0);
            pSprite->SetModalState(0, TRUE);
            pSprite->SetStealthGreyOut(90);
            // A party member's (portrait number != -1) toolbar is rebuilt:
            // the selected button is reset to 100 (0x64) first, and the state
            // stack is popped when the target is also the active engine's
            // selected character and the state is none of the 16 below.
            if (g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(pSprite->GetId()) != -1) {
                g_pBaldurChitin->GetObjectGame()->GetButtonArray()->SetSelectedButton(100);
                if (g_pBaldurChitin->GetActiveEngine()->GetSelectedCharacter()
                    == g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(pSprite->GetId())) {
                    INT nState = g_pBaldurChitin->GetObjectGame()->GetButtonArray()->m_nState;
                    if (nState != 102
                        && nState != 101
                        && nState != 104
                        && nState != 103
                        && nState != 105
                        && nState != 112
                        && nState != 115
                        && nState != 116
                        && nState != 117
                        && nState != 118
                        && nState != 119
                        && nState != 120
                        && nState != 121
                        && nState != 122
                        && nState != 123
                        && nState != 106) {
                        g_pBaldurChitin->GetObjectGame()->GetButtonArray()->ResetState();
                    }
                }
            }
            // The invisible effect comes off the target's timed list, with
            // the equipped list's current position left in place; the
            // remove-all-of-type effect that follows is reported to self.
            pSprite->GetTimedEffectList()->RemoveAllOfType(pSprite,
                CGAMEEFFECT_INVISIBLE,
                pSprite->GetEquipedEffectList()->GetPosCurrent(),
                -1);
            ITEM_EFFECT effect;
            CGameEffect::ClearItemEffect(&effect, ICEWIND_CGAMEEFFECT_REMOVEALLOFTYPE);
            effect.dwFlags = CGAMEEFFECT_INVISIBLE;
            effect.effectAmount = 0;
            // NOTE: The source position is the target's id packed into a
            // CPoint (0x452B00); the message's caller and target are both
            // this object's id ([ebx+0x5C] pushed twice at 0x451D1E).
            CGameEffect* pEffect = CGameEffect::DecodeEffect(&effect,
                CPoint(pSprite->GetId()),
                pSprite->GetId(),
                CPoint(-1, -1));
            message = new CMessageAddEffect(pEffect, GetId(), GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            returnValue = ACTION_DONE;
            goto release;
        }
    case 294:
        // XEquipItem
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        if (pTarget == NULL) {
            // The original jumps to the tail that forces ACTION_DONE
            // (0x44E1DB) before setting bShare.
            returnValue = ACTION_DONE;
            goto release;
        }
        bShare = TRUE;
        // TODO: 0x451D73 calls 0x7625F0 (not ported): equips the item named
        // by m_curAction.GetString1() (a CString temp the listing builds at
        // 0x451D6C) on the target sprite, with m_curAction.GetSpecifics()
        // and GetSpecifics2() as the remaining arguments; its result is the
        // action return, which keeps the ACTION_ERROR entry default here.
        goto release;
    case 295:
        // TransferInventory
        // 0x451D81: the receiving sprite; a NULL result takes 0x451451
        // (ACTION_ERROR straight to release), as the entry default does here.
        pTarget = sub_45C290();
        if (pTarget != NULL) {
            bShare = TRUE;
            CGameObject* pActee = m_curAction.GetActee2().sub_40CCA0(this, FALSE);
            if (pActee != NULL) {
                // TODO: 0x451DB6 calls 0x4C83F0 (not ported; annotated
                // CGameObject::FloatText): the receiving sprite runs the
                // transfer against the actee; its return is the action
                // result, which keeps the ACTION_ERROR entry default here.
                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(pActee->GetId(),
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            }
        }
        goto release;
    case 296:
        // ChangeStoreMarkup
        g_pBaldurChitin->GetObjectGame()->ChangeStoreMarkup(CResRef(m_curAction.GetString1()),
            m_curAction.GetSpecifics(),
            m_curAction.GetSpecifics2(),
            m_curAction.GetSpecifics3());
        returnValue = ACTION_DONE;
        goto done;
    case 297:
        // ScreenShake
        message = new CMessageScreenShake(m_curAction.GetSpecifics(),
            m_curAction.GetSpecifics2(),
            m_curAction.GetSpecifics3(),
            1,
            GetId(),
            GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        returnValue = ACTION_DONE;
        goto done;
    case 298:
        // FaceSavedLocation: the sprite turns to its saved orientation
        // (0x451FA3).
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        bShare = TRUE;
        if (pTarget != NULL) {
            CGameSprite* pSprite = static_cast<CGameSprite*>(pTarget);
            pSprite->SetDirection(static_cast<SHORT>(pSprite->m_baseStats.field_2E8));
            if (pSprite->GetPos() != CPoint(-1, -1) || pSprite->GetArea() != NULL) {
                message = new CMessageSpriteUpdate(pSprite, pSprite->GetId(), pSprite->GetId());
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }
        }
        returnValue = ACTION_DONE;
        goto release;
    case 301:
        // SetExtendedNight (0x4520ED-0x45216A): set or clear bit 0x40 of
        // the area header's area type, then mirror it into the infinity.
        {
            WORD areaType = m_pArea->GetHeader()->m_areaType;
            if (m_curAction.GetSpecifics() != 0) {
                areaType |= 0x40;
            } else {
                areaType &= 0xFFBF;
            }
            m_pArea->GetHeader()->m_areaType = areaType;
            m_pArea->GetInfinity()->SetAreaType(areaType);
            // 0x45212D: only the player-controlled side posts the update.
            if (InControl()) {
                message = new CMessageSetAreaType(areaType, GetId(), GetId());
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }
        }
        returnValue = ACTION_DONE;
        goto done;
    case 303:
        // SetRestEncounterChance (0x45216A-0x452253)
        {
            CAreaFileRestEncounter* pRestEncounter = m_pArea->GetHeaderRestEncounter();
            if (pRestEncounter != NULL) {
                // 0x45EA80 is the exe's min(const int&, const int&)
                // instantiation; both chances clamp to 100.
                LONG nDay = m_curAction.GetSpecifics();
                LONG nNight = m_curAction.GetSpecifics2();
                pRestEncounter->m_probDay = static_cast<WORD>(nDay < 100 ? nDay : 100);
                pRestEncounter->m_probNight = static_cast<WORD>(nNight < 100 ? nNight : 100);
            }
            // 0x4521E7: only the player-controlled side posts the update,
            // and the message re-reads the stored words (0x452211/0x45221A).
            if (InControl()) {
                message = new CMessageSetAreaRestEncounter(
                    pRestEncounter->m_probDay, pRestEncounter->m_probNight,
                    GetId(), GetId());
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }
        }
        returnValue = ACTION_DONE;
        goto done;
    case 304: {
        // AddHP (0x45074B-0x4507DC): m_curAction.m_specificID is added to
        // the target's base hit points, clamped at the derived maximum.
        // A NULL or dead target still reports ACTION_DONE, doing nothing.
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        if (pTarget == NULL) {
            returnValue = ACTION_DONE;
            goto release;
        }
        CGameSprite* pSprite = static_cast<CGameSprite*>(pTarget);
        bShare = TRUE;
        if ((pSprite->m_derivedStats.m_generalState & STATE_DEAD) == 0) {
            if (pSprite->m_baseStats.m_hitPoints + m_curAction.m_specificID
                >= pSprite->m_derivedStats.GetAtOffset(STAT_MAXHITPOINTS)) {
                pSprite->m_baseStats.m_hitPoints
                    = static_cast<SHORT>(pSprite->m_derivedStats.GetAtOffset(STAT_MAXHITPOINTS));
            } else {
                pSprite->m_baseStats.m_hitPoints
                    = static_cast<SHORT>(m_curAction.m_specificID + pSprite->m_baseStats.m_hitPoints);
            }
        }
        returnValue = ACTION_DONE;
        goto release;
    }
    case 305:
        // SetMyTarget (0x45139D-0x4513D8)
        pTarget = sub_45C290();
        if (pTarget != NULL) {
            bShare = TRUE;
            sub_453110(pTarget->GetAIType());
        } else {
            sub_453110(CAIObjectType::NOONE);
        }
        returnValue = ACTION_DONE;
        goto release;
    case 312:
        // SetApparentNameSTRREF (0x451EAA-0x451F37)
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        if (pTarget == NULL) {
            // 0x451EBD: a miss takes the tail that forces ACTION_DONE
            // (0x44E1DB) before bShare is set.
            returnValue = ACTION_DONE;
            goto release;
        }
        bShare = TRUE;
        {
            CGameSprite* pSprite = static_cast<CGameSprite*>(pTarget);
            // 0x451EDB: an unchanged name skips the update message.
            if (pSprite->GetBaseStats()->m_apparentName
                == static_cast<DWORD>(m_curAction.GetSpecifics())) {
                returnValue = ACTION_DONE;
                goto release;
            }
            pSprite->GetBaseStats()->m_apparentName = m_curAction.GetSpecifics();
            // 0x451F1A: the message's caller and target are both the
            // target's id (GetId at 0x4530B0 read twice).
            message = new CMessageSpriteUpdate(pSprite, pSprite->GetId(), pSprite->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
        returnValue = ACTION_DONE;
        goto release;
    case 313:
        // SetRegularNameSTRREF (0x451F37-0x451FA3): the same arm against
        // m_name (base stats offset 0); both arms share the tail at
        // 0x451F12.
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        if (pTarget == NULL) {
            // 0x451F4B: forced ACTION_DONE (0x44E1DB) before bShare.
            returnValue = ACTION_DONE;
            goto release;
        }
        bShare = TRUE;
        {
            CGameSprite* pSprite = static_cast<CGameSprite*>(pTarget);
            // 0x451F69: an unchanged name skips the update message.
            if (pSprite->GetBaseStats()->m_name
                == static_cast<DWORD>(m_curAction.GetSpecifics())) {
                returnValue = ACTION_DONE;
                goto release;
            }
            pSprite->GetBaseStats()->m_name = m_curAction.GetSpecifics();
            message = new CMessageSpriteUpdate(pSprite, pSprite->GetId(), pSprite->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
        returnValue = ACTION_DONE;
        goto release;
    case 314:
        // EndGame (0x452253-0x4522F2)
        g_pBaldurChitin->GetObjectGame()->ReadyCharacterTerminationSequence(
            3, m_curAction.GetSpecifics());
        {
            // 0x452283: the message's caller and target are both the
            // VISIBLE AREA's id (a byte), not this object's.
            LONG nAreaId = g_pBaldurChitin->GetObjectGame()->GetVisibleArea()->GetId();
            message = new CMessageEndGame(3, m_curAction.GetSpecifics(), nAreaId, nAreaId);
            // 0x4522C5 loads the comm-type table word at 0x84820C (3 =
            // BROADCAST_OTHERS) inline rather than calling GetCommType,
            // whose in-tree base is still the assert stub.
            g_pBaldurChitin->GetMessageHandler()->AddMessage(
                message, FALSE, CMessage::BROADCAST_OTHERS);
        }
        returnValue = ACTION_DONE;
        goto done;
    case 315:
        // AddExperiencePartyCR (0x4522F2-0x452310): the CR flag is TRUE,
        // unlike case 164's plain AddExperienceParty.
        g_pBaldurChitin->GetObjectGame()->AddExperienceParty(
            m_curAction.GetSpecifics(), FALSE, TRUE);
        returnValue = ACTION_DONE;
        goto done;
    case 316: {
        // AddFeat (0x452318-0x4523B5): specifics1 names the feat. When
        // sub_763A40 refuses the upgrade the arm still reports ACTION_DONE,
        // without granting anything.
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        if (pTarget == NULL) {
            // 0x451451: ACTION_ERROR straight to release (entry default).
            returnValue = ACTION_ERROR;
            goto release;
        }
        bShare = TRUE;
        CGameSprite* pSprite = static_cast<CGameSprite*>(pTarget);
        UINT nFeat = static_cast<UINT>(m_curAction.GetSpecifics());
        if (pSprite->sub_763A40(nFeat, 0) == 0) {
            returnValue = ACTION_DONE;
            goto release;
        }
        // A copy of the current base stats is taken first; the upgraded feat
        // is then diffed against it by sub_71E240 to apply stat side-effects.
        CCreatureFileHeader oldBaseStats = *pSprite->GetBaseStats();
        pSprite->SetFeatValue(nFeat, pSprite->GetFeatValue(nFeat) + 1);
        pSprite->sub_71E240(oldBaseStats, *pSprite->GetDerivedStats());
        pSprite->sub_4531B0();
        pSprite->ProcessEffectList();
        returnValue = ACTION_DONE;
        goto release;
    }
    case 318: {
        // SetCreatureAreaFlag (0x452575-0x4525E5): specifics1 is the flag
        // mask on the target's base-stats m_flags; a non-zero specifics2
        // sets the bits, a zero clears them.
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        if (pTarget == NULL) {
            // 0x451451: ACTION_ERROR straight to release (entry default).
            returnValue = ACTION_ERROR;
            goto release;
        }
        bShare = TRUE;
        returnValue = ACTION_DONE;
        CGameSprite* pSprite = static_cast<CGameSprite*>(pTarget);
        if (m_curAction.GetSpecifics2() != 0) {
            pSprite->GetBaseStats()->m_flags |= static_cast<DWORD>(m_curAction.GetSpecifics());
        } else {
            pSprite->GetBaseStats()->m_flags &= ~static_cast<DWORD>(m_curAction.GetSpecifics());
        }
        goto release;
    }
    case 323:
        // WaitAnimation
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        bShare = TRUE;
        returnValue = WaitAnimation(static_cast<CGameSprite*>(pTarget));
        goto release;
    case 324:
        returnValue = SetMusic();
        goto done;
    case 325:
        // SetAreaFlags (0x45262C-0x4526AA): specifics2 picks the operation
        // on the area header flags — 1 and, 2 or, 3 xor — with specifics as
        // the mask. Every path keeps the entry default: the three write
        // arms jump straight to the epilogue without touching the result
        // register, so the action reports ACTION_ERROR even when it wrote
        // the flags (mirrored from the listing).
        {
            LONG nOp = m_curAction.GetSpecifics2();
            if (nOp == 1 || nOp == 2 || nOp == 3) {
                DWORD dwMask = static_cast<DWORD>(m_curAction.GetSpecifics());
                DWORD* pdwFlags = &GetArea()->GetHeader()->m_flags;
                if (nOp == 1) {
                    *pdwFlags &= dwMask;
                } else if (nOp == 2) {
                    *pdwFlags |= dwMask;
                } else {
                    *pdwFlags ^= dwMask;
                }
            }
            returnValue = ACTION_ERROR;
        }
        goto done;
    default:
        sError.Format("Error: Action number: %d not implemented", m_curAction.GetActionID());
        returnValue = ACTION_ERROR;
        goto done;
    }

release:
    if (pTarget != NULL) {
        if (bShare) {
            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(pTarget->GetId(),
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        } else {
            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(pTarget->GetId(),
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }

done:
    SetLastActionReturn(returnValue);
    return returnValue;
}

// 0x4525CC
//
// ACTION 323 WaitAnimation; the exe body is the ExecuteAction (0x44DC10)
// case at 0x4525EA-0x45262C. An unresolved target returns ACTION_ERROR;
// otherwise the target sprite's current animation sequence (0x453180) is
// compared against the action's specifics (0x41E180) -- still matching
// keeps the action interruptable, a finished sequence reports ACTION_DONE.
SHORT CGameAIBase::WaitAnimation(CGameSprite* pTarget)
{
    if (pTarget == NULL) {
        return ACTION_ERROR;
    }

    return pTarget->GetSequence() == m_curAction.GetSpecifics()
        ? ACTION_INTERRUPTABLE
        : ACTION_DONE;
}

// 0x45C300
void CGameAIBase::InsertResponse(CAIResponse& response, BOOL checkCurrentResponse, BOOL clearActions)
{
    CAIAction action;
    CAIObjectType actorType;

    if (checkCurrentResponse
        && m_curResponseSetNum >= 0
        && m_curScriptNum >= 0
        && m_curResponseSetNum == response.m_responseSetNum
        && m_curScriptNum == response.m_scriptNum) {
        return;
    }

    if (clearActions) {
        ClearActions(FALSE);
    }

    m_curResponseNum = response.m_responseNum;
    m_curResponseSetNum = response.m_responseSetNum;
    m_curScriptNum = response.m_scriptNum;
    m_interrupt = TRUE;

    POSITION pos = response.m_actionList.GetHeadPosition();
    while (pos != NULL) {
        CAIAction* node = response.m_actionList.GetNext(pos);
        action = *node;

        CAIAction* newNode = new CAIAction();
        *newNode = action;
        m_queuedActions.AddTail(newNode);
    }
}

// 0x45C730
void CGameAIBase::ProcessAI()
{
    if (g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL
        && g_pChitin->cNetwork.m_idLocalPlayer != m_remotePlayerID) {
        return;
    }

    if (m_curAction.m_actionID == CAIAction::NO_ACTION) {
        if (m_queuedActions.GetCount() != 0) {
            SetCurrAction(GetNextAction(m_aiAction));
            m_interrupt = FALSE;
            if (m_curAction.m_actionID != CAIAction::NO_ACTION) {
                goto rand_value;
            }
        }
        m_inCutScene = FALSE;
    }

rand_value:
    m_randValue = rand() % 0x7FFF;

    if (!g_pBaldurChitin->GetObjectGame()->m_worldTime.m_active && !m_firstCall) {
        // The game is paused: only instant actions run.
        if (m_curAction.m_actionID == CAIAction::NO_ACTION) {
            SetCurrAction(GetNextAction(m_aiAction));
            m_interrupt = FALSE;
        }

        BOOL bInstant = g_pBaldurChitin->GetObjectGame()->GetRuleTables().m_lInstantActions.Find(m_curAction.m_actionID) != NULL;
        if (g_pChitin->cNetwork.m_bConnectionEstablished == TRUE
            && m_curAction.m_actionID == 110) {
            return;
        }

        while (bInstant) {
            DoAction();

            bInstant = g_pBaldurChitin->GetObjectGame()->GetRuleTables().m_lInstantActions.Find(m_curAction.m_actionID) != NULL;
            if (g_pChitin->cNetwork.m_bConnectionEstablished == TRUE
                && m_curAction.m_actionID == 110) {
                return;
            }
        }
        return;
    }

    field_44A++;
    if (((m_id ^ field_44A) & 0xF) == 0) {
        CheckTimers(1);
    }

    if (((field_44A ^ m_id) & 0xF) != 0 && !m_firstCall && !m_forceActionPick) {
        goto do_actions;
    }

    field_54C++;
    m_firstCall = FALSE;
    m_forceActionPick = FALSE;

    if (m_curAction.m_actionID != CAIAction::NO_ACTION && m_pendingTriggers.GetCount() == 0) {
        if (field_550 < 120 && field_552 <= 0) {
            field_550++;
            goto do_actions;
        }
    }

    if (m_pendingTriggers.GetCount() != 0) {
        field_552 = 5;
    }

    ProcessPendingTriggers();

    if (field_552 > 0) {
        field_552--;
    }
    field_550 = 0;

do_actions:
    if (m_curAction.m_actionID == CAIAction::NO_ACTION) {
        SetCurrAction(GetNextAction(m_aiAction));
        m_interrupt = FALSE;
    }

    if (m_posVertList == NULL) {
        return;
    }

    BOOL bInstant;
    do {
        DoAction();

        bInstant = g_pBaldurChitin->GetObjectGame()->GetRuleTables().m_lInstantActions.Find(m_curAction.m_actionID) != NULL;
        if (g_pChitin->cNetwork.m_bConnectionEstablished == TRUE
            && m_curAction.m_actionID == 110) {
            break;
        }
    } while (bInstant);
}

// 0x45CA10
void CGameAIBase::ProcessPendingTriggers()
{
    BOOL bContinue = FALSE;

    if (m_inCutScene) {
        return;
    }

    if (m_nLastActionReturn == ACTION_NORMAL) {
        return;
    }

    CAIResponse* pResponse = NULL;
    CAIResponse response;
    SHORT nScriptLevel = 0;

    // NOTE: Unused.
    CTypedPtrList<CPtrList, CAITrigger*> unused;
    CAITrigger unusedTrigger(CAITRIGGER_SEE, 0);

    ApplyTriggers();

    if (m_overrideScript != NULL) {
        pResponse = m_overrideScript->Find(m_pendingTriggers, this);
    }

    if ((!g_pBaldurChitin->GetObjectGame()->m_bPartyAI
            && g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id) != -1)
        || m_curAction.m_actionID == 27
        || m_curAction.m_actionID == 87
        || (m_curAction.m_actionID == CAIAction::MOVETOPOINT && m_curAction.m_specificID3 == 1)) {
        // A party member without party AI, or one following the player's
        // order (ProtectPoint 27, ProtectObject 87, MoveToPoint with
        // specifics3 1): only the override script.
        if (pResponse != NULL) {
            if (!pResponse->m_actionList.IsEmpty()) {
                pResponse->m_scriptNum = 0;
                InsertResponse(*pResponse, TRUE, TRUE);

                POSITION pos = m_pendingTriggers.GetHeadPosition();
                while (pos != NULL) {
                    CAITrigger* pTrigger = m_pendingTriggers.GetNext(pos);
                    if (pTrigger != NULL) {
                        delete pTrigger;
                    }
                }
                m_pendingTriggers.RemoveAll();
            }

            delete pResponse;
        }
        return;
    }

    BOOL bEndsInContinue;

    if (pResponse != NULL) {
        response.Add(*pResponse);
        bEndsInContinue = pResponse->InListEnd(CAIAction::CONTINUE);
    } else {
        bEndsInContinue = FALSE;
    }

    if (pResponse == NULL || pResponse->m_actionList.IsEmpty() || bEndsInContinue) {
        bContinue = TRUE;
    }

    BOOL bNotCharmed = TRUE;
    if (GetObjectType() == CGameObject::TYPE_SPRITE) {
        CGameSprite* pSprite = static_cast<CGameSprite*>(this);
        if ((pSprite->m_derivedStats.m_generalState & STATE_CHARMED) != 0
            && pSprite->m_startTypeAI.m_nEnemyAlly != pSprite->m_liveTypeAI.m_nEnemyAlly) {
            bNotCharmed = FALSE;
        }
    }

    if (bContinue && bNotCharmed) {
        CAIScript* scripts[5] = {
            m_special1Script,
            m_teamScript,
            m_special2Script,
            m_combatScript,
            m_special3Script,
        };

        for (int index = 0; index < 5; index++) {
            if (pResponse != NULL) {
                delete pResponse;
                pResponse = NULL;
            }

            nScriptLevel = static_cast<SHORT>(index + 1);

            CAIScript* pScript = scripts[index];
            if (pScript != NULL && pScript->m_caList.GetCount() != 0) {
                pResponse = pScript->Find(m_pendingTriggers, this);
                if (pResponse != NULL) {
                    response.Add(*pResponse);
                    bEndsInContinue = pResponse->InListEnd(CAIAction::CONTINUE);
                } else {
                    bEndsInContinue = FALSE;
                }

                if (pResponse != NULL && !pResponse->m_actionList.IsEmpty() && !bEndsInContinue) {
                    goto insert;
                }

                bContinue = TRUE;
            }
        }

        if (pResponse != NULL) {
            delete pResponse;
            pResponse = NULL;
        }

        nScriptLevel = 6;
        if (m_movementScript != NULL && m_movementScript->m_caList.GetCount() != 0) {
            pResponse = m_movementScript->Find(m_pendingTriggers, this);
            if (pResponse != NULL) {
                response.Add(*pResponse);
                pResponse->InListEnd(CAIAction::CONTINUE);
            }
        }
    }

insert:
    if (!response.m_actionList.IsEmpty()) {
        response.m_scriptNum = nScriptLevel;
        InsertResponse(response, TRUE, TRUE);

        POSITION pos = m_pendingTriggers.GetHeadPosition();
        while (pos != NULL) {
            CAITrigger* pTrigger = m_pendingTriggers.GetNext(pos);
            if (pTrigger != NULL) {
                delete pTrigger;
            }
        }
        m_pendingTriggers.RemoveAll();
    }

    if (pResponse != NULL) {
        delete pResponse;
    }
}

// 0x45D130
void CGameAIBase::RemoveFromArea()
{
    CGameObject::RemoveFromArea();

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Delete(m_id,
        CGameObjectArray::THREAD_ASYNCH,
        NULL,
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameAIBase.cpp
        // __LINE__: 6886
        UTIL_ASSERT(FALSE);
    }

    delete this;
}

// 0x44CC70
void CGameAIBase::AddAction(const CAIAction& action)
{
    CAIAction* copy = new CAIAction();
    copy->m_actionID = action.m_actionID;
    copy->m_specificID = action.m_specificID;
    copy->m_actorID.Set(action.m_actorID);
    copy->m_acteeID.Set(action.m_acteeID);
    copy->m_acteeID2.Set(action.m_acteeID2);
    copy->m_dest = action.m_dest;
    copy->m_specificID2 = action.m_specificID2;
    copy->m_specificID3 = action.m_specificID3;
    copy->m_string1 = action.m_string1;
    copy->m_string2 = action.m_string2;
    copy->m_internalFlags = action.m_internalFlags;
    m_queuedActions.AddHead(copy);
}

// 0x44CE20
void CGameAIBase::AddEffect(CGameEffect* pEffect, BYTE list, BOOL noSave, BOOL immediateApply)
{
    if (pEffect != NULL) {
        delete pEffect;
    }
}

// 0x44CE40
void CGameAIBase::ClearAI(BOOLEAN a1)
{
    ClearActions(FALSE);
    ApplyTriggers();

    POSITION pos = m_pendingTriggers.GetHeadPosition();
    while (pos != NULL) {
        CAITrigger* pTrigger = m_pendingTriggers.GetNext(pos);
        if (pTrigger != NULL) {
            delete pTrigger;
        }
    }
    m_pendingTriggers.RemoveAll();

    SetCurrAction(CAIAction::NULL_ACTION);

    m_curAction.m_actionID = CAIAction::NULL_ACTION.m_actionID;
    m_curAction.m_specificID = CAIAction::NULL_ACTION.m_specificID;
    m_curAction.m_actorID.Set(CAIAction::NULL_ACTION.m_actorID);
    m_curAction.m_acteeID.Set(CAIAction::NULL_ACTION.m_acteeID);
    m_curAction.m_acteeID2.Set(CAIAction::NULL_ACTION.m_acteeID2);
    m_curAction.m_dest = CAIAction::NULL_ACTION.m_dest;
    m_curAction.m_specificID2 = CAIAction::NULL_ACTION.m_specificID2;
    m_curAction.m_specificID3 = CAIAction::NULL_ACTION.m_specificID3;
    m_curAction.m_string1 = CAIAction::NULL_ACTION.m_string1;
    m_curAction.m_string2 = CAIAction::NULL_ACTION.m_string2;
    m_curAction.m_internalFlags = CAIAction::NULL_ACTION.m_internalFlags;
}

// 0x44CF50
void CGameAIBase::InsertAction(const CAIAction& action)
{
    CAIAction* copy = new CAIAction();
    copy->m_actionID = action.m_actionID;
    copy->m_specificID = action.m_specificID;
    copy->m_actorID.Set(action.m_actorID);
    copy->m_acteeID.Set(action.m_acteeID);
    copy->m_acteeID2.Set(action.m_acteeID2);
    copy->m_dest = action.m_dest;
    copy->m_specificID2 = action.m_specificID2;
    copy->m_specificID3 = action.m_specificID3;
    copy->m_string1 = action.m_string1;
    copy->m_string2 = action.m_string2;
    copy->m_internalFlags = action.m_internalFlags;
    m_queuedActions.AddTail(copy);
}

// 0x45D190
void CGameAIBase::SetCurrAction(const CAIAction& action)
{
    m_actionCount = 0;
    m_interrupt = FALSE;
    m_curAction.m_actionID = action.m_actionID;
    m_curAction.m_specificID = action.m_specificID;
    m_curAction.m_actorID.Set(action.m_actorID);
    m_curAction.m_acteeID.Set(action.m_acteeID);
    m_curAction.m_acteeID2.Set(action.m_acteeID2);
    m_curAction.m_dest = action.m_dest;
    m_curAction.m_specificID2 = action.m_specificID2;
    m_curAction.m_specificID3 = action.m_specificID3;
    m_curAction.m_string1 = action.m_string1;
    m_curAction.m_string2 = action.m_string2;
    m_curAction.m_internalFlags = action.m_internalFlags;
    if (action.m_actionID != CAIAction::NO_ACTION
        && g_pBaldurChitin->GetObjectGame()->GetRuleTables().m_lNoDecodeList.Find(action.m_actionID) == NULL) {
        m_curAction.Decode(this);
    }
}

// 0x45D280
void CGameAIBase::SetScript(SHORT level, CAIScript* script)
{
    switch (level) {
    case 0:
        if (m_overrideScript != NULL) {
            delete m_overrideScript;
        }
        m_overrideScript = script;
        break;
    case 1:
        if (m_special1Script != NULL) {
            delete m_special1Script;
        }
        m_special1Script = script;
        break;
    case 2:
        if (m_teamScript != NULL) {
            delete m_teamScript;
        }
        m_teamScript = script;
        break;
    case 3:
        if (m_special2Script != NULL) {
            delete m_special2Script;
        }
        m_special2Script = script;
        break;
    case 4:
        if (m_combatScript != NULL) {
            delete m_combatScript;
        }
        m_combatScript = script;
        break;
    case 5:
        if (m_special3Script != NULL) {
            delete m_special3Script;
        }
        m_special3Script = script;
        break;
    case 6:
        if (m_movementScript != NULL) {
            delete m_movementScript;
        }
        m_movementScript = script;
        break;
    }
}

// 0x45D3E0
void CGameAIBase::DecodeSpell(LONG nSpellId, CString& sResRef)
{
    if (nSpellId == 0) {
        nSpellId = field_588;
        if (nSpellId == 0) {
            sResRef = "";
            return;
        }
    }

    CString sSpell("");
    sSpell += "SP";

    if (nSpellId / 1000 == 1) {
        sSpell += "PR";
    } else if (nSpellId / 1000 == 2) {
        sSpell += "WI";
    } else if (nSpellId / 1000 == 3) {
        sSpell += "IN";
    } else {
        sSpell = "MARW";

        // NOTE: The original formats this message and drops it (no assert,
        // no log), 0x45D4AD-0x45D4D8.
        CString sError;
        sError.Format("Invalid or missing spell ID (check SPELL.IDS): %d", nSpellId);
    }

    CString sNumber;
    sNumber.Format("%03d", nSpellId % 1000);
    sSpell += sNumber;
    sResRef = sSpell;
}

// 0x45D6A0
void CGameAIBase::ApplyTriggers()
{
    CMessage* message;

    if ((g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime / 4) % 900 == 0) {
        message = new CMessageUpdateReaction(11, m_id, m_id);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    POSITION pos = m_pendingTriggers.GetHeadPosition();
    while (pos != NULL) {
        CAITrigger* pTrigger = m_pendingTriggers.GetNext(pos);
        if ((pTrigger->m_flags & 0x4) == 0) {
            pTrigger->m_flags |= 0x4;

            switch (pTrigger->m_triggerID) {
            case CAITRIGGER_ATTACKEDBY:
                AutoPause(2);
                if (!m_lAttacker.Equal(pTrigger->GetCause())
                    && (g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id) != -1
                        || g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(pTrigger->GetCause().GetInstance()) != -1)) {
                    m_lAttacker.Set(pTrigger->GetCause());
                    message = new CMessageSetLastObject(pTrigger->GetCause(),
                        CAITRIGGER_ATTACKEDBY,
                        m_id,
                        m_id);
                    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
                }
                break;
            case CAITRIGGER_HELP:
                if (!m_lHelp.Equal(pTrigger->GetCause())) {
                    m_lHelp.Set(pTrigger->GetCause());
                    message = new CMessageSetLastObject(pTrigger->GetCause(),
                        CAITRIGGER_HELP,
                        m_id,
                        m_id);
                    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
                }
                break;
            case CAITRIGGER_RECEIVEDORDER:
                if (!m_lOrderedBy.Equal(pTrigger->GetCause())) {
                    m_lOrderedBy.Set(pTrigger->GetCause());
                    message = new CMessageSetLastObject(pTrigger->GetCause(), 6, m_id, m_id);
                    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
                }
                break;
            case CAITRIGGER_SAID:
                if (!m_lTalkedTo.Equal(pTrigger->GetCause())) {
                    m_lTalkedTo.Set(pTrigger->GetCause());
                    message = new CMessageSetLastObject(pTrigger->GetCause(),
                        CAITRIGGER_SAID,
                        m_id,
                        m_id);
                    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
                }
                break;
            case CAITRIGGER_HITBY:
                m_lAttackStyle = pTrigger->m_specificID;
                if (!m_lHitter.Equal(pTrigger->GetCause())) {
                    m_lHitter.Set(pTrigger->GetCause());
                    message = new CMessageSetLastObject(pTrigger->GetCause(),
                        CAITRIGGER_HITBY,
                        m_id,
                        m_id);
                    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
                }
                break;
            case CAITRIGGER_HEARD:
                if (!m_lHeard.Equal(pTrigger->GetCause())) {
                    m_lHeard.Set(pTrigger->GetCause());
                    message = new CMessageSetLastObject(pTrigger->GetCause(),
                        CAITRIGGER_HEARD,
                        m_id,
                        m_id);
                    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
                }
                break;
            }

            if (g_pBaldurChitin->GetObjectGame()->m_saveObjectList.Find(pTrigger->m_triggerID) != NULL
                && !m_lTrigger.Equal(pTrigger->GetCause())) {
                m_lTrigger.Set(pTrigger->GetCause());
                message = new CMessageSetLastObject(pTrigger->GetCause(), 0, m_id, m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }
        }
    }
}

// 0x45DED0
BOOL CGameAIBase::CheckAppropriateTarget(CSpell* pSpell, CGameAIBase* pTarget)
{

    pSpell->Demand();
    if (pTarget->GetObjectType() != TYPE_SPRITE) {
        pSpell->Release();
        return FALSE;
    }

    SPELL_ABILITY* ability = pSpell->GetAbility(0);
    if (ability != NULL
        && (ability->actionType != 1
            || (static_cast<CGameSprite*>(pTarget)->GetDerivedStats()->m_generalState & 0x800) == 0
            || pSpell->GetGenericName() == 12117
            || pSpell->GetGenericName() == 25765
            || pSpell->GetGenericName() == 32393)) {
        pSpell->Release();
        return TRUE;
    }

    pSpell->Release();
    return FALSE;
}

// 0x45DF70
BOOL CGameAIBase::PartyHasItem(const CResRef& resRef)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    for (SHORT nPortrait = 0; nPortrait < pGame->GetNumCharacters(); nPortrait++) {
        LONG nCharacterId = pGame->GetCharacterId(nPortrait);

        CGameSprite* pSprite;
        BYTE rc = pGame->GetObjectArray()->GetShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
        if (rc != CGameObjectArray::SUCCESS) {
            break;
        }

        CString sName;
        resRef.CopyToString(sName);

        if (pSprite->FindItemPersonal(sName, 0, FALSE) != -1) {
            pGame->GetObjectArray()->ReleaseShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            return TRUE;
        }

        if (pSprite->FindItemBags(sName, 0, FALSE) != -1) {
            pGame->GetObjectArray()->ReleaseShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            return TRUE;
        }

        pGame->GetObjectArray()->ReleaseShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }
    return FALSE;
}

// 0x45E100
void CGameAIBase::ApplyEffectToParty(CGameEffect* pEffect)
{
    CMessage* message;
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    for (SHORT nPortrait = 0; nPortrait < pGame->GetNumCharacters(); nPortrait++) {
        LONG nCharacterId = pGame->GetCharacterId(nPortrait);
        if (pGame->GetGameSave()->field_1AC) {
            CGameSprite* pSprite;
            BYTE rc = pGame->GetObjectArray()->GetShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
            if (rc == CGameObjectArray::SUCCESS) {
                if (pSprite->InControl()) {
                    pGame->GetObjectArray()->ReleaseShare(nCharacterId,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);
                    message = new CMessageAddEffect(pEffect, m_id, nCharacterId);
                    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
                } else {
                    pGame->GetObjectArray()->ReleaseShare(nCharacterId,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);
                }
            }
        } else {
            message = new CMessageAddEffect(pEffect, m_id, nCharacterId);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
    }
}

// 0x799E60
void CGameAIBase::AutoPause(DWORD type)
{
}

// 0x49FC40
BOOL CGameAIBase::GetCanSeeInvisible()
{
    return TRUE;
}

// 0x45F2A0
SHORT CGameAIBase::MoveView(CPoint dest, int speed)
{
    CGameArea* pVisibleArea = g_pBaldurChitin->GetObjectGame()->GetVisibleArea();

    INT x;
    INT y;
    pVisibleArea->GetInfinity()->GetViewPosition(x, y);

    if (dest.x < 0) {
        dest.x = 0;
    }

    if (dest.y < 0) {
        dest.y = 0;
    }

    int maxX = pVisibleArea->GetInfinity()->rViewPort.left
        - pVisibleArea->GetInfinity()->rViewPort.right
        + pVisibleArea->GetInfinity()->nAreaX;
    if (dest.x > maxX) {
        dest.x = maxX;
    }

    int maxY = pVisibleArea->GetInfinity()->rViewPort.top
        - pVisibleArea->GetInfinity()->rViewPort.bottom
        + pVisibleArea->GetInfinity()->nAreaY;
    if (dest.y > maxY) {
        dest.y = maxY;
    }

    if (m_curAction.m_actionID == CAIACTION_MOVEVIEWPOINTUNTILDONE) {
        if (!field_594) {
            CMessageStartScroll* pMessage = new CMessageStartScroll(pVisibleArea,
                CPoint(x, y),
                dest,
                static_cast<BYTE>(speed),
                m_id,
                m_id);

            g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);

            field_594 = TRUE;
        }

        if (x != dest.x && y != dest.y) {
            return ACTION_NORMAL;
        }

        field_594 = FALSE;
    } else {
        CMessageStartScroll* pMessage = new CMessageStartScroll(pVisibleArea,
            CPoint(x, y),
            dest,
            static_cast<BYTE>(speed),
            m_id,
            m_id);

        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
    }

    return ACTION_DONE;
}

// 0x45F5D0
SHORT CGameAIBase::MoveViewPoint()
{
    CPoint dest(m_curAction.m_dest);

    CRect viewPort(GetArea()->GetInfinity()->rViewPort);
    dest.x -= viewPort.Width() / 2;
    dest.y -= viewPort.Height() / 2;

    if (dest.x < 0) {
        dest.x = 0;
    }

    if (dest.x > GetArea()->GetInfinity()->nAreaX) {
        dest.x = GetArea()->GetInfinity()->nAreaX;
    }

    if (dest.y < 0) {
        dest.y = 0;
    }

    if (dest.y > GetArea()->GetInfinity()->nAreaY) {
        dest.y = GetArea()->GetInfinity()->nAreaY;
    }

    SHORT speed = static_cast<SHORT>(m_curAction.m_specificID);

    return MoveView(dest, speed);
}

// 0x45F660
SHORT CGameAIBase::MoveViewObject(CGameObject* target)
{
    if (target == NULL) {
        return ACTION_ERROR;
    }

    CPoint dest(target->GetPos());

    CRect viewPort(target->GetArea()->GetInfinity()->rViewPort);
    dest.x -= viewPort.Width() / 2;
    dest.y -= viewPort.Height() / 2;

    SHORT speed = static_cast<SHORT>(m_curAction.m_specificID);

    return MoveView(dest, speed);
}

// 0x45F6D0
SHORT CGameAIBase::MoveCursor(CPoint dest, SHORT speed)
{
    CInfinity* pInfinity = GetArea()->GetInfinity();

    INT x;
    INT y;
    pInfinity->GetViewPosition(x, y);

    CPoint pt;
    GetCursorPos(&pt);

    const SHORT nCursorStep = 4;
    const SHORT nDiagonalStep = static_cast<SHORT>(3 * nCursorStep / 4);
    const SHORT nDiagonalTolerance = static_cast<SHORT>(nDiagonalStep * speed);

    if (m_curAction.m_specificID == 0) {
        pInfinity->SetViewPosition(dest.x, dest.y, TRUE);
        return ACTION_DONE;
    }

    const SHORT nStraightStep = static_cast<SHORT>(nCursorStep * speed);
    // The exe computes the half-step in full 32-bit (imul eax,ecx with eax = 2);
    // it is the only step amount without a 16-bit truncation.
    const INT nHalfStep = nCursorStep / 2 * speed;
    const INT dx = dest.x - pt.x;
    const INT dy = dest.y - pt.y;

    if (dx <= nStraightStep) {
        if (dx >= -nStraightStep
            && dy <= nDiagonalTolerance
            && dy >= -nDiagonalTolerance) {
            return ACTION_DONE;
        }

        if (dx >= -nStraightStep) {
            if (dy > 0) {
                pt.y += nDiagonalStep * speed;
            } else if (dy < 0) {
                pt.y -= nDiagonalStep * speed;
            }

            pInfinity->SetViewPosition(pt.x, pt.y, TRUE);
            return ACTION_INTERRUPTABLE;
        }
    }

    if (dx > 0 && dy <= 6 && dy >= -nDiagonalTolerance) {
        pt.x += nCursorStep * speed;
    } else if (dx < 0 && dy <= 6 && dy >= -nDiagonalTolerance) {
        pt.x -= nCursorStep * speed;
    } else if (dx > 0 && dy > 0) {
        pt.x += nDiagonalStep * speed;
        pt.y += nHalfStep;
    } else if (dx < 0 && dy > 0) {
        pt.x -= nDiagonalStep * speed;
        pt.y += nHalfStep;
    } else if (dx > 0 && dy < 0) {
        pt.x += nDiagonalStep * speed;
        pt.y -= nHalfStep;
    } else if (dx < 0 && dy < 0) {
        pt.x -= nDiagonalStep * speed;
        pt.y -= nHalfStep;
    }

    pInfinity->SetViewPosition(pt.x, pt.y, TRUE);

    return ACTION_INTERRUPTABLE;
}

// 0x45F900
SHORT CGameAIBase::MoveCursorPoint()
{
    CPoint dest = m_curAction.m_dest;
    SHORT speed = static_cast<SHORT>(m_curAction.m_specificID);

    return MoveCursor(dest, speed);
}

// NOTE: Inlined.
SHORT CGameAIBase::ClickLButton(CPoint pt)
{
    g_pBaldurChitin->m_pEngineWorld->OnLButtonDown(pt);
    g_pBaldurChitin->m_pEngineWorld->OnLButtonUp(pt);
    return ACTION_DONE;
}

// 0x45F920
SHORT CGameAIBase::ClickLButtonPoint()
{
    CPoint dest = m_curAction.m_dest;
    SHORT speed = static_cast<SHORT>(m_curAction.m_specificID);

    SHORT moveReturn = MoveCursor(dest, speed);
    if (moveReturn != ACTION_DONE) {
        return moveReturn;
    }

    // NOTE: Uninline.
    return ClickLButton(dest);
}

// 0x45F980
SHORT CGameAIBase::ClickLButtonObject(CGameObject* target)
{
    if (target == NULL) {
        return ACTION_ERROR;
    }

    CPoint dest = target->GetPos();
    SHORT speed = static_cast<SHORT>(m_curAction.m_specificID);

    SHORT moveReturn = MoveCursor(dest, speed);
    if (moveReturn != ACTION_DONE) {
        return moveReturn;
    }

    // NOTE: Uninline.
    return ClickLButton(dest);
}

// NOTE: Inlined.
SHORT CGameAIBase::DoubleClickLButton(CPoint pt)
{
    g_pBaldurChitin->m_pEngineWorld->OnLButtonDown(pt);
    g_pBaldurChitin->m_pEngineWorld->OnLButtonUp(pt);
    g_pBaldurChitin->m_pEngineWorld->OnLButtonDblClk(pt);
    g_pBaldurChitin->m_pEngineWorld->OnLButtonUp(pt);
    return ACTION_DONE;
}

// 0x45F9F0
SHORT CGameAIBase::DoubleClickLButtonPoint()
{
    CPoint dest = m_curAction.m_dest;
    SHORT speed = static_cast<SHORT>(m_curAction.m_specificID);

    SHORT moveReturn = MoveCursor(dest, speed);
    if (moveReturn != ACTION_DONE) {
        return moveReturn;
    }

    // NOTE: Uninline.
    return DoubleClickLButton(dest);
}

// 0x45FA70
SHORT CGameAIBase::DoubleClickLButtonObject(CGameObject* target)
{
    if (target == NULL) {
        return ACTION_ERROR;
    }

    CPoint dest = target->GetPos();
    SHORT speed = static_cast<SHORT>(m_curAction.m_specificID);

    SHORT moveReturn = MoveCursor(dest, speed);
    if (moveReturn != ACTION_DONE) {
        return moveReturn;
    }

    // NOTE: Uninline.
    return DoubleClickLButton(dest);
}

// NOTE: Inlined.
SHORT CGameAIBase::ClickRButton(CPoint pt)
{
    g_pBaldurChitin->m_pEngineWorld->OnRButtonDown(pt);
    g_pBaldurChitin->m_pEngineWorld->OnRButtonUp(pt);
    return ACTION_DONE;
}

// 0x45FB10
SHORT CGameAIBase::ClickRButtonPoint()
{
    CPoint dest = m_curAction.m_dest;
    SHORT speed = static_cast<SHORT>(m_curAction.m_specificID);

    SHORT moveReturn = MoveCursor(dest, speed);
    if (moveReturn != ACTION_DONE) {
        return moveReturn;
    }

    // NOTE: Uninline.
    return ClickRButton(dest);
}

// 0x45FB70
SHORT CGameAIBase::ClickRButtonObject(CGameObject* target)
{
    if (target == NULL) {
        return ACTION_ERROR;
    }

    CPoint dest = target->GetPos();
    SHORT speed = static_cast<SHORT>(m_curAction.m_specificID);

    SHORT moveReturn = MoveCursor(dest, speed);
    if (moveReturn != ACTION_DONE) {
        return moveReturn;
    }

    // NOTE: Uninline.
    return ClickRButton(dest);
}

// NOTE: Inlined.
SHORT CGameAIBase::DoubleClickRButton(CPoint pt)
{
    g_pBaldurChitin->m_pEngineWorld->OnRButtonDown(pt);
    g_pBaldurChitin->m_pEngineWorld->OnRButtonUp(pt);
    g_pBaldurChitin->m_pEngineWorld->OnRButtonDblClk(pt);
    g_pBaldurChitin->m_pEngineWorld->OnRButtonUp(pt);
    return ACTION_DONE;
}

// 0x45FBF0
SHORT CGameAIBase::DoubleClickRButtonPoint()
{
    CPoint dest = m_curAction.m_dest;
    SHORT speed = static_cast<SHORT>(m_curAction.m_specificID);

    SHORT moveReturn = MoveCursor(dest, speed);
    if (moveReturn != ACTION_DONE) {
        return moveReturn;
    }

    // NOTE: Uninline.
    return DoubleClickRButton(dest);
}

// 0x45FC80
SHORT CGameAIBase::DoubleClickRButtonObject(CGameObject* target)
{
    if (target == NULL) {
        return ACTION_ERROR;
    }

    CPoint dest = target->GetPos();
    SHORT speed = static_cast<SHORT>(m_curAction.m_specificID);

    SHORT moveReturn = MoveCursor(dest, speed);
    if (moveReturn != ACTION_DONE) {
        return moveReturn;
    }

    // NOTE: Uninline.
    return DoubleClickLButton(dest);
}

// 0x45FD20
SHORT CGameAIBase::ChangeAIScript()
{
    CAIScript* script = new CAIScript(CResRef(m_curAction.GetString1()));
    SetScript(static_cast<SHORT>(m_curAction.m_specificID), script);

    if (GetObjectType() == TYPE_CONTAINER) {
        static_cast<CGameContainer*>(this)->SetScriptRes(m_curAction.GetString1());
    }

    return ACTION_DONE;
}

// 0x45FED0
SHORT CGameAIBase::StartTimer()
{
    BYTE id = static_cast<BYTE>(m_curAction.m_specificID);
    LONG time = m_curAction.m_specificID2;

    POSITION pos = m_timers.GetHeadPosition();
    while (pos != NULL) {
        CGameTimer* pTimer = m_timers.GetNext(pos);
        if (pTimer->m_id == id) {
            pTimer->m_time = time;
            return ACTION_DONE;
        }
    }

    CGameTimer* pTimer = new CGameTimer();
    pTimer->m_time = time;
    pTimer->m_id = id;
    m_timers.AddTail(pTimer);

    return ACTION_DONE;
}

// 0x45FF40
SHORT CGameAIBase::SendTrigger(CGameAIBase* sprite)
{
    if (sprite == NULL) {
        return ACTION_ERROR;
    }

    CAITrigger trigger(CAITrigger::TRIGGER, m_typeAI, m_curAction.m_specificID);

    CMessageSetTrigger* pMessage = new CMessageSetTrigger(trigger,
        m_id,
        sprite->GetId());

    g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);

    return ACTION_DONE;
}

// 0x4600B0
SHORT CGameAIBase::Wait()
{
    if (m_actionCount == 0) {
        m_curAction.m_specificID *= 15;
    }

    m_curAction.m_specificID--;

    if (m_curAction.m_specificID > 0) {
        return ACTION_INTERRUPTABLE;
    }

    return ACTION_DONE;
}

// 0x4600F0
SHORT CGameAIBase::SmallWait()
{
    m_curAction.m_specificID--;

    if (m_curAction.m_specificID > 0) {
        return ACTION_INTERRUPTABLE;
    }

    return ACTION_DONE;
}

// 0x460110
SHORT CGameAIBase::Shout()
{
    CTypedPtrList<CPtrList, LONG*> targets;

    SHORT range = m_curAction.m_actionID != CAIACTION_212
        ? GetVisualRange()
        : SHORT_MAX;

    m_pArea->GetAllInRange(m_pos,
        CAIObjectType::ANYONE,
        range,
        GetTerrainTable(),
        targets,
        FALSE,
        FALSE);

    CAITrigger trigger(CAITrigger::HEARD, m_typeAI, m_curAction.m_specificID);

    POSITION pos = targets.GetHeadPosition();
    while (pos != NULL) {
        LONG nId = static_cast<LONG>(reinterpret_cast<INT_PTR>(targets.GetNext(pos)));

        CMessage* message = new CMessageSetTrigger(trigger, m_id, nId);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    return ACTION_DONE;
}

// 0x4608A0
// The CreateCreature action: builds a CGameSprite straight from the CRE
// file named by the action's first string, adds it to the area, and
// registers it in the area's named creatures under the script name given
// by the second string. The destination is the action's destination point
// (or, when it is (-1, -1), the source position of the acting object).
// Action 7 places the new sprite on the nearest open square around the
// destination; action 228 (0xE4) takes the exact square containing it.
SHORT CGameAIBase::CreateCreature()
{
    CVariable cVariable;
    CPoint pos = m_curAction.m_dest;

    if (pos.x == -1 && pos.y == -1)
        CProjectile::GetSourcePosition(m_id, pos, TRUE);

    if (g_pBaldurChitin->cDimm.m_cKeyTable.FindKey(CResRef(m_curAction.GetString1()), 1009, TRUE) == NULL) {
        return ACTION_ERROR;
    }

    CCreatureFile creatureFile;

    // NOTE: Uninline.
    creatureFile.SetResRef(CResRef(m_curAction.GetString1()), TRUE, TRUE);

    // NOTE: The original evaluates the size first.
    DWORD nSize = creatureFile.GetDataSize();
    BYTE* pData = creatureFile.GetData();

    CGameSprite* pNewSprite = new CGameSprite(pData, nSize, 0, 0, -1, 0, 0, 0x7FFFFFFF,
        pos,
        static_cast<WORD>(m_curAction.m_specificID));
    if (pNewSprite == NULL) {
        creatureFile.ReleaseData();
        return ACTION_ERROR;
    }

    // NOTE: the exe reaches the resref through a call Ghidra labels
    // GetEllipseRect (0x4699e0); semantically it is &creatureFile.cResRef.
    pNewSprite->SetResRef(CResRef(creatureFile.GetResRef().GetResRefStr()));

    creatureFile.ReleaseData();

    pNewSprite->SetResRef(CResRef(m_curAction.GetString1()));

    if (m_curAction.m_actionID == 228) {
        // Action 228 (0xE4) takes the exact square containing the point.
        pos = CPoint(pos.x / CPathSearch::GRID_SQUARE_SIZEX,
            pos.y / CPathSearch::GRID_SQUARE_SIZEY);
    } else {
        // Action 7 searches for the nearest open square around it, using
        // the new sprite's personal space and this object's terrain table.
        BYTE nPersonalSpace = pNewSprite->m_animation.GetPersonalSpace();
        pos = m_pArea->m_search.GetNearestOpenSquare(
            CPoint(pos.x / CPathSearch::GRID_SQUARE_SIZEX,
                pos.y / CPathSearch::GRID_SQUARE_SIZEY),
            GetTerrainTable(),
            nPersonalSpace,
            -1);
    }

    BYTE nListType = pNewSprite->m_animation.GetListType();

    pNewSprite->AddToArea(m_pArea,
        CPoint(pos.x * CPathSearch::GRID_SQUARE_SIZEX,
            pos.y * CPathSearch::GRID_SQUARE_SIZEY),
        0,
        nListType);

    if (strcmp(m_curAction.GetString2(), "") != 0) {
        memcpy(pNewSprite->m_scriptName,
            m_curAction.GetString2().GetBuffer(SCRIPTNAME_SIZE),
            SCRIPTNAME_SIZE);
    }

    cVariable.SetName(CString(reinterpret_cast<const char*>(pNewSprite->GetScriptName())));
    cVariable.m_intValue = pNewSprite->GetId();
    m_pArea->m_namedCreatures.AddKey(cVariable);

    field_342.Set(pNewSprite->GetAIType());

    return ACTION_DONE;
}

// 0x460D70
// The CreateCreatureObject action (227): builds a CGameSprite straight
// from the CRE file named by the action's first string and places it on
// the nearest open square around the TARGET object's position. Unlike
// CreateCreature (0x4608A0), the destination is the target's position,
// there is no exact-square variant, no second-string script name (the
// sprite registers under whatever script name the CRE left), and
// field_342 is not set.
SHORT CGameAIBase::CreateCreatureObject(CGameObject* pTarget)
{
    // The instructions test the target before the CVariable constructor
    // runs; a null target fails the action at once.
    if (pTarget == NULL) {
        return ACTION_ERROR;
    }

    CVariable cVariable;
    CPoint pos = pTarget->GetPos();

    if (pos.x == -1 && pos.y == -1)
        CProjectile::GetSourcePosition(m_id, pos, TRUE);

    if (g_pBaldurChitin->cDimm.m_cKeyTable.FindKey(CResRef(m_curAction.GetString1()), 1009, TRUE) == NULL) {
        return ACTION_ERROR;
    }

    CCreatureFile creatureFile;

    // NOTE: Uninline.
    creatureFile.SetResRef(CResRef(m_curAction.GetString1()), TRUE, TRUE);

    // NOTE: The original evaluates the size first.
    DWORD nSize = creatureFile.GetDataSize();
    BYTE* pData = creatureFile.GetData();

    CGameSprite* pNewSprite = new CGameSprite(pData, nSize, 0, 0, -1, 0, 0, 0x7FFFFFFF,
        pos,
        static_cast<WORD>(m_curAction.m_specificID));
    if (pNewSprite == NULL) {
        creatureFile.ReleaseData();
        return ACTION_ERROR;
    }

    // NOTE: the exe reaches the resref through a call Ghidra labels
    // GetEllipseRect (0x4699e0); semantically it is &creatureFile.cResRef.
    pNewSprite->SetResRef(CResRef(creatureFile.GetResRef().GetResRefStr()));

    creatureFile.ReleaseData();

    pNewSprite->SetResRef(CResRef(m_curAction.GetString1()));

    // The instructions always take the nearest open square around the
    // target's square, using the new sprite's personal space and this
    // object's terrain table; there is no exact-square variant here.
    BYTE nPersonalSpace = pNewSprite->m_animation.GetPersonalSpace();
    pos = m_pArea->m_search.GetNearestOpenSquare(
        CPoint(pos.x / CPathSearch::GRID_SQUARE_SIZEX,
            pos.y / CPathSearch::GRID_SQUARE_SIZEY),
        GetTerrainTable(),
        nPersonalSpace,
        -1);

    BYTE nListType = pNewSprite->m_animation.GetListType();

    pNewSprite->AddToArea(m_pArea,
        CPoint(pos.x * CPathSearch::GRID_SQUARE_SIZEX,
            pos.y * CPathSearch::GRID_SQUARE_SIZEY),
        0,
        nListType);

    // No second-string script name here: the variable takes the script
    // name the CRE file itself left on the sprite.
    cVariable.SetName(CString(reinterpret_cast<const char*>(pNewSprite->GetScriptName())));
    cVariable.m_intValue = pNewSprite->GetId();
    m_pArea->m_namedCreatures.AddKey(cVariable);

    return ACTION_DONE;
}

// 0x462470
// Applies the action's spell to the target: the resref comes from the
// action's override string or, failing that, DecodeSpell on the spell id.
// Every effect of ability 0 is built by CSpell::GetAbilityEffect and
// dispatched on its target type -- the same fan-out ForceSpell performs
// with a caster level.
SHORT CGameAIBase::ApplySpell(CGameObject* pTarget)
{
    CString sResRef;
    CMessage* message;

    if (strcmp(m_curAction.GetString1(), "") == 0) {
        DecodeSpell(m_curAction.m_specificID, sResRef);
        if (strcmp(sResRef, "") == 0) {
            return ACTION_ERROR;
        }
    } else {
        sResRef = m_curAction.GetString1();
    }

    CSpell* pSpell = new CSpell(CResRef(sResRef));
    if (pSpell == NULL) {
        return ACTION_ERROR;
    }

    if (pTarget == NULL) {
        delete pSpell;
        return ACTION_ERROR;
    }

    pSpell->Demand();

    if (!CheckAppropriateTarget(pSpell, static_cast<CGameAIBase*>(pTarget))) {
        pSpell->Release();
        delete pSpell;
        return ACTION_ERROR;
    }

    SPELL_ABILITY* pAbility = pSpell->GetAbility(0);
    if (pAbility == NULL) {
        pSpell->Release();
        delete pSpell;
        return ACTION_ERROR;
    }

    BYTE nCasterLevel = 0;
    if (m_curAction.m_internalFlags & 0x20000000) {
        nCasterLevel = static_cast<BYTE>(m_curAction.m_specificID3);
    }

    for (LONG nEffect = 0; nEffect < pAbility->effectCount; nEffect++) {
        CGameEffect* pEffect = pSpell->GetAbilityEffect(0, nEffect, this, 0, 0, nCasterLevel);
        pEffect->m_source = m_pos;
        pEffect->m_sourceID = m_id;
        pEffect->m_target = pTarget->GetPos();

        switch (pEffect->m_targetType) {
        case 1:
            pEffect->m_flags |= 0x2;
            message = new CMessageAddEffect(pEffect, m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            break;
        case 2:
            message = new CMessageAddEffect(pEffect, m_id, pTarget->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            break;
        case 3:
            ApplyEffectToParty(pEffect);
            delete pEffect;
            break;
        case 4:
            GetArea()->ApplyEffectToSprites(pEffect, FALSE, FALSE, 0, NULL);
            delete pEffect;
            break;
        case 5:
            GetArea()->ApplyEffectToSprites(pEffect, TRUE, FALSE, 0, NULL);
            delete pEffect;
            break;
        case 6:
            GetArea()->ApplyEffectToSprites(pEffect, FALSE, TRUE, GetAIType().m_nSpecific, NULL);
            delete pEffect;
            break;
        case 7:
            GetArea()->ApplyEffectToSprites(pEffect, FALSE, TRUE, pTarget->GetAIType().m_nSpecific, NULL);
            delete pEffect;
            break;
        case 8:
            GetArea()->ApplyEffectToSprites(pEffect, FALSE, FALSE, 0, this);
            delete pEffect;
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameAIBaseActions.cpp
            // __LINE__: 2001
            UTIL_ASSERT(FALSE);
        }
    }

    pSpell->Release();
    delete pSpell;
    return ACTION_DONE;
}

// 0x463310
void CGameAIBase::PutItemGround(CItem* pItem)
{
    LONG nContainerId = g_pBaldurChitin->GetObjectGame()->GetGroundPile(m_id);
    if (nContainerId != CGameObjectArray::INVALID_INDEX) {
        CGameContainer* pContainer;

        BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(nContainerId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pContainer),
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            pContainer->PlaceItemInBlankSlot(pItem, TRUE, SHORT_MAX);
            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(nContainerId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }
}

// 0x4633A0
// The CreateItem action: makes the item named by the action's first
// string (usage counts from m_specificID through m_specificID3) and
// stores it. A party member takes it in the first blank personal-inventory
// slot; a creature outside the party in the first blank of its carried
// slots; a container in its first blank slot; any other object drops it
// on the ground pile beside the acting object. When a sprite has no blank
// slot left the pile still takes the item, but the action reports an
// error (as CGameAIBase::PlaceItem's full-pack drop does).
SHORT CGameAIBase::CreateItem()
{
    if (g_pBaldurChitin->cDimm.m_cKeyTable.FindKey(CResRef(m_curAction.GetString1()), 1005, TRUE) == NULL) {
        return ACTION_ERROR;
    }

    CItem* pItem = new CItem(CResRef(m_curAction.GetString1()),
        static_cast<WORD>(m_curAction.m_specificID),
        static_cast<WORD>(m_curAction.m_specificID2),
        static_cast<WORD>(m_curAction.m_specificID3),
        0,
        0);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    if (pGame->GetCharacterPortraitNum(m_id) != -1) {
        CGameSprite* pSprite = static_cast<CGameSprite*>(this);
        int index;
        for (index = 0; index < CScreenInventory::PERSONAL_INVENTORY_SIZE; index++) {
            if (pSprite->m_equipment.m_items[index + 18] == NULL)
                break;
        }
        if (index < CScreenInventory::PERSONAL_INVENTORY_SIZE) {
            pSprite->m_equipment.m_items[index + 18] = pItem;
            return ACTION_DONE;
        }

        // The pack is full: the ground pile takes the item, but the
        // action itself failed.
        LONG nPileId = pGame->GetGroundPile(m_id);
        CGameContainer* pPile;
        if (nPileId != CGameObjectArray::INVALID_INDEX && pGame->GetObjectArray()->GetDeny(nPileId,
                                                               CGameObjectArray::THREAD_ASYNCH,
                                                               reinterpret_cast<CGameObject**>(&pPile),
                                                               INFINITE)
            == CGameObjectArray::SUCCESS) {
            pPile->PlaceItemInBlankSlot(pItem, TRUE, SHORT_MAX);
            pGame->GetObjectArray()->ReleaseDeny(nPileId, CGameObjectArray::THREAD_ASYNCH, INFINITE);
        }
        return ACTION_ERROR;
    }

    if (m_objectType == TYPE_SPRITE) {
        // A creature outside the party: the first blank of its carried
        // slots (equipment slots and both packs, as PlaceItem scans them).
        CGameSprite* pSprite = static_cast<CGameSprite*>(this);
        int index;
        for (index = 0; index < 28; index++) {
            if (pSprite->m_equipment.m_items[index + 15] == NULL)
                break;
        }
        if (index < 28) {
            pSprite->m_equipment.m_items[index + 15] = pItem;
            return ACTION_DONE;
        }

        // No blank slot: the ground pile takes the item, but the action
        // itself failed.
        LONG nPileId = pGame->GetGroundPile(m_id);
        CGameContainer* pPile;
        if (nPileId != CGameObjectArray::INVALID_INDEX && pGame->GetObjectArray()->GetDeny(nPileId,
                                                               CGameObjectArray::THREAD_ASYNCH,
                                                               reinterpret_cast<CGameObject**>(&pPile),
                                                               INFINITE)
            == CGameObjectArray::SUCCESS) {
            pPile->PlaceItemInBlankSlot(pItem, TRUE, SHORT_MAX);
            pGame->GetObjectArray()->ReleaseDeny(nPileId, CGameObjectArray::THREAD_ASYNCH, INFINITE);
        }
        return ACTION_ERROR;
    }

    if (m_objectType == TYPE_CONTAINER) {
        static_cast<CGameContainer*>(this)->PlaceItemInBlankSlot(pItem, TRUE, SHORT_MAX);
        return ACTION_DONE;
    }

    // Any other object: the item goes to the ground pile beside it.
    LONG nPileId = pGame->GetGroundPile(m_id);
    if (nPileId == CGameObjectArray::INVALID_INDEX) {
        return ACTION_ERROR;
    }

    CGameContainer* pPile;
    if (pGame->GetObjectArray()->GetDeny(nPileId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pPile),
            INFINITE)
        != CGameObjectArray::SUCCESS) {
        return ACTION_ERROR;
    }

    pPile->PlaceItemInBlankSlot(pItem, TRUE, SHORT_MAX);
    pGame->GetObjectArray()->ReleaseDeny(nPileId, CGameObjectArray::THREAD_ASYNCH, INFINITE);

    return ACTION_DONE;
}

// 0x463B30
// The TakePartyItem family: TakePartyItem (116), TakePartyItemAll (188),
// TakePartyItemRange (193) and TakePartyItemNum (204) all funnel here.
// The item named by the action's first string is taken from each party
// member: the member's own slot is cleared through a CMessageRemoveItem,
// the quick buttons keyed to that slot are refreshed, and the taken copy
// lands in this sprite's inventory through PlaceItem. 116 and 193 stop
// after one item; 193 only reaches members standing in this sprite's
// area within m_specificID grid squares; 188 sweeps every member empty;
// 204 takes m_specificID charges, splitting a stack when it holds more.
// Members with nothing personal left are drained through their bags.
SHORT CGameAIBase::TakePartyItem()
{
    CString sName = m_curAction.GetString1();
    BOOL numbered = m_curAction.m_actionID == 204;
    LONG number = numbered ? m_curAction.m_specificID : 1;
    BOOL ranged = m_curAction.m_actionID == 193;
    LONG range = ranged ? m_curAction.m_specificID : 0;
    CItem* lastItem = NULL;

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    for (SHORT nPortrait = 0; nPortrait < pGame->GetNumCharacters(); nPortrait++) {
        LONG nCharacterId = pGame->GetCharacterId(nPortrait);

        CGameSprite* pSprite;
        BYTE rc;
        do {
            rc = pGame->GetObjectArray()->GetShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);
        if (rc != CGameObjectArray::SUCCESS) {
            return ACTION_NORMAL;
        }

        int match = 0;

        // 193: only members in this sprite's area, within range grid
        // squares; the distance is squared and measured on coordinates
        // already divided by the square size.
        BOOL inRange = TRUE;
        if (ranged) {
            if (pSprite->m_pArea != m_pArea) {
                inRange = FALSE;
            } else {
                const CPoint& pos = pSprite->GetPos();
                LONG dy = m_pos.y / CPathSearch::GRID_SQUARE_SIZEY - pos.y / CPathSearch::GRID_SQUARE_SIZEY;
                LONG dx = m_pos.x / CPathSearch::GRID_SQUARE_SIZEX - pos.x / CPathSearch::GRID_SQUARE_SIZEX;
                inRange = dy * dy + dx * dx <= range;
            }
        }

        if (inRange) {
            SHORT slot = pSprite->FindItemPersonal(sName, match, FALSE);
            while (slot != -1) {
                CItem* pTaken = new CItem(*pSprite->m_equipment.m_items[slot]);
                lastItem = pTaken;

                CMessage* message = new CMessageRemoveItem(slot, m_id, pSprite->m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

                // NOTE: the quick-list sweep keys on the slot, not on an
                // ability of the item: 2/slot/0, then 2/slot/1, then
                // 2/slot/2, as the action 223 arm does.
                CAbilityId ability;
                ability.m_res = "";
                ability.m_itemType = 2;
                ability.m_itemNum = slot;
                ability.m_abilityNum = 0;
                pSprite->CheckQuickLists(&ability, 0, TRUE, FALSE);
                ability.m_abilityNum = 1;
                pSprite->CheckQuickLists(&ability, 0, TRUE, FALSE);
                ability.m_abilityNum = 2;
                pSprite->CheckQuickLists(&ability, 0, TRUE, FALSE);

                // 204 counts charges. A non-stackable item spends one
                // charge of the count; a stackable item holding more
                // charges than remain is split: the taken copy is reduced
                // to the leftover count and handed back to the member
                // through a CMessageAddItem, while the taker's PlaceItem
                // below still receives that same reduced copy -- both
                // sides keep the remainder, an original quirk kept as-is.
                if (numbered) {
                    if (pTaken->GetMaxStackable() <= 1) {
                        number--;
                    } else {
                        WORD usage = pTaken->GetUsageCount(0);
                        if (usage > number) {
                            pTaken->SetUsageCount(0, usage - number);
                            number = 0;
                            CMessage* messageAdd = new CMessageAddItem(*pTaken, m_id, pSprite->m_id);
                            g_pBaldurChitin->GetMessageHandler()->AddMessage(messageAdd, FALSE);
                        } else {
                            number -= pTaken->GetUsageCount(0);
                        }
                    }
                }

                PlaceItem(pTaken, TRUE, TRUE, 1, TRUE);

                // The action id is re-read after the take: 116 and 193
                // stop at the first item; 188 empties this member; 204
                // continues until the count is met.
                if (m_curAction.m_actionID != 188 && m_curAction.m_actionID != 204) {
                    pGame->GetObjectArray()->ReleaseShare(nCharacterId,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);
                    return ACTION_DONE;
                }

                match++;
                if (m_curAction.m_actionID == 204 && number <= 0) {
                    pGame->GetObjectArray()->ReleaseShare(nCharacterId,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);
                    return ACTION_DONE;
                }

                slot = pSprite->FindItemPersonal(sName, match, FALSE);
            }

            // The member has no more personal copies. The bags are
            // drained while nothing personal was taken from anyone yet,
            // for 188 outright, or while 204 still wants charges; the bag
            // copies land as one freshly created stack of `taken`.
            if (lastItem == NULL || m_curAction.m_actionID == 188
                || (m_curAction.m_actionID == 204 && number > 0)) {
                SHORT taken = pSprite->TakeItemBags(sName, number, -1);
                if (taken > 0) {
                    number -= taken;
                    PlaceItem(new CItem(CResRef(sName), 1, 0, 0, 0, 0), TRUE, TRUE, taken, TRUE);
                    if (m_curAction.m_actionID == 204 && number <= 0) {
                        pGame->GetObjectArray()->ReleaseShare(nCharacterId,
                            CGameObjectArray::THREAD_ASYNCH,
                            INFINITE);
                        return ACTION_DONE;
                    }
                }
            }
        }

        pGame->GetObjectArray()->ReleaseShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    return lastItem != NULL ? ACTION_DONE : ACTION_ERROR;
}

// 0x464200
// The item-list pair: TakeItemListParty (220) and TakeItemListPartyNum
// (226) both funnel here. Both are engine-internal in IWD2 (ACTION.IDS
// has no entry for 205-228); IESDP documents them under the BG2
// numbering the engine inherits -- 220 TakeItemListParty(S:ResRef*) and
// 226 TakeItemListPartyNum(S:ResRef*,I:Num*) -- and IWD2's own scripts
// call TakeItemListPartyNum in the AR0602 party-import scene. The 2DA
// named by the action's first string is loaded and walked a row at a
// time; each row's first cell names an item, which is taken from every
// party member the way TakePartyItem takes one: the member's slot is
// cleared through a CMessageRemoveItem, the quick buttons keyed to that
// slot are refreshed, and the taken copy lands in this sprite's
// inventory through PlaceItem. 226 carries a count in m_specificID,
// spent once per 2DA row that yielded anything (not per item); if
// nothing at all was taken, the 2DA's default cell names a fallback
// item created into this sprite's inventory.
SHORT CGameAIBase::TakeItemListParty()
{
    C2DArray t2DA;
    CString sItemName;

    t2DA.Load(CResRef(m_curAction.GetString1()));

    LONG remaining = m_curAction.m_actionID == 226 ? m_curAction.m_specificID : -1;

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    for (SHORT i = 0; i < t2DA.GetHeight(); i++) {
        sItemName = t2DA.GetAt(CPoint(0, i));

        BOOL bTookSomething = FALSE;
        for (SHORT nPortrait = 0; nPortrait < pGame->GetNumCharacters(); nPortrait++) {
            LONG nCharacterId = pGame->GetCharacterId(nPortrait);

            CGameSprite* pSprite;
            BYTE rc;
            do {
                rc = pGame->GetObjectArray()->GetShare(nCharacterId,
                    CGameObjectArray::THREAD_ASYNCH,
                    reinterpret_cast<CGameObject**>(&pSprite),
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);
            if (rc != CGameObjectArray::SUCCESS) {
                return ACTION_NORMAL;
            }

            int match = 0;
            SHORT slot = pSprite->FindItemPersonal(sItemName, match, FALSE);
            while (slot != -1) {
                bTookSomething = TRUE;

                CItem* pTaken = new CItem(*pSprite->m_equipment.m_items[slot]);

                CMessage* message = new CMessageRemoveItem(slot, m_id, pSprite->m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

                // NOTE: the quick-list sweep keys on the slot, not on an
                // ability of the item: 2/slot/0, then 2/slot/1, then
                // 2/slot/2, as TakePartyItem does.
                CAbilityId ability;
                ability.m_res = "";
                ability.m_itemType = 2;
                ability.m_itemNum = slot;
                ability.m_abilityNum = 0;
                pSprite->CheckQuickLists(&ability, 0, TRUE, FALSE);
                ability.m_abilityNum = 1;
                pSprite->CheckQuickLists(&ability, 0, TRUE, FALSE);
                ability.m_abilityNum = 2;
                pSprite->CheckQuickLists(&ability, 0, TRUE, FALSE);

                PlaceItem(pTaken, TRUE, TRUE, 1, TRUE);

                match++;
                slot = pSprite->FindItemPersonal(sItemName, match, FALSE);
            }

            // The member has no more personal copies of this row's item.
            // The bags are drained next, up to 0x7FFF of the item; the bag
            // copies land as one freshly created stack of `taken` items,
            // placed without feedback, unlike the personal copies above.
            SHORT taken = pSprite->TakeItemBags(sItemName, 0x7FFF, -1);
            if (taken > 0) {
                bTookSomething = TRUE;
                PlaceItem(new CItem(CResRef(sItemName), 1, 0, 0, 0, 0), TRUE, TRUE, taken, FALSE);
            }

            pGame->GetObjectArray()->ReleaseShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }

        // 226 spends one count per 2DA row that yielded anything -- not
        // per item -- and is done as soon as the count runs out.
        if (bTookSomething && m_curAction.m_actionID == 226) {
            if (--remaining <= 0) {
                return ACTION_DONE;
            }
        }
    }

    // A 226 that took nothing anywhere leaves `remaining` equal to the
    // count the action carried. The 2DA's default cell then names a
    // fallback item, created into this sprite's inventory when the item
    // file exists.
    if (remaining == m_curAction.m_specificID && m_curAction.m_actionID == 226) {
        CString sDefault = t2DA.GetDefault();
        sDefault.MakeUpper();
        if (g_pBaldurChitin->cDimm.m_cKeyTable.FindKey(CResRef(sDefault), 1005, TRUE) != NULL) {
            PlaceItem(new CItem(CResRef(sDefault), 0, 0, 0, 0, 0), TRUE, TRUE, TRUE, TRUE);
        }
    }

    return ACTION_DONE;
}

// 0x463740
SHORT CGameAIBase::ClearActions(CGameObject* target)
{
    if (target == NULL) {
        return ACTION_ERROR;
    }

    CMessage* message = new CMessageClearActions(m_id, target->GetId());
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

    return ACTION_DONE;
}

// 0x464950
BOOL CGameAIBase::PlaceItem(CItem* item, BOOL deny, BOOL drop, DWORD count, BOOL feedback)
{
    if (item == NULL)
        return TRUE;
    if (count > 1)
        PlaceItem(new CItem(*item), deny, drop, count - 1, TRUE);
    item->m_flags |= 2;
    CInfGame* game = g_pBaldurChitin->GetObjectGame();
    CGameObjectArray* array = game->GetObjectArray();
    if (game->m_familiars.Find(reinterpret_cast<int*>(m_id), NULL) != NULL) {
        g_pBaldurChitin->GetMessageHandler()->AddMessage(new CMessageAddItem(*item, m_id, game->GetProtagonist()), FALSE);
        return TRUE;
    }
    BOOL party = game->GetCharacterPortraitNum(m_id) != -1;
    if (!party && GetObjectType() != CGameObject::TYPE_SPRITE) {
        if (GetObjectType() == CGameObject::TYPE_CONTAINER) {
            static_cast<CGameContainer*>(this)->PlaceItemInBlankSlot(item, TRUE, SHORT_MAX);
            return TRUE;
        }
        if (drop)
            delete item;
        return FALSE;
    }
    CGameSprite* sprite = static_cast<CGameSprite*>(this);
    int index;
    if (party) {
        for (index = 0; index < CScreenInventory::PERSONAL_INVENTORY_SIZE; index++) {
            if (sprite->m_equipment.m_items[index + 18] == NULL)
                break;
        }
    } else {
        if (feedback)
            game->FeedBack(CInfGame::FEEDBACK_ITEMLOST, 0, TRUE);
        for (index = 27; index >= 0; index--) {
            if (sprite->m_equipment.m_items[index + 15] == NULL)
                break;
        }
    }
    if ((party && index < CScreenInventory::PERSONAL_INVENTORY_SIZE) || (!party && index >= 0)) {
        if (deny) {
            sprite->m_equipment.m_items[index + (party ? 18 : 15)] = item;
            return TRUE;
        }
        CGameObject* object;
        BYTE rc;
        do {
            rc = array->GetDeny(m_id, CGameObjectArray::THREAD_ASYNCH, &object, INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);
        if (rc == CGameObjectArray::SUCCESS) {
            sprite->m_equipment.m_items[index + 18] = item;
            array->ReleaseDeny(m_id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
        }
        if (g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL && g_pChitin->cNetwork.m_idLocalPlayer != m_remotePlayerID) {
            g_pBaldurChitin->GetMessageHandler()->AddMessage(new CMessageAddItem(*item, m_id, m_id), FALSE);
        }
        return TRUE;
    }
    if (drop) {
        LONG pileId = game->GetGroundPile(m_id);
        CGameContainer* pile;
        if (pileId != CGameObjectArray::INVALID_INDEX && array->GetDeny(pileId, CGameObjectArray::THREAD_ASYNCH, reinterpret_cast<CGameObject**>(&pile), INFINITE) == CGameObjectArray::SUCCESS) {
            pile->PlaceItemInBlankSlot(item, TRUE, SHORT_MAX);
            array->ReleaseDeny(pileId, CGameObjectArray::THREAD_ASYNCH, INFINITE);
        }
    }
    return FALSE;
}

// 0x464EB0
SHORT CGameAIBase::GiveItemCreate(CGameObject* pTarget)
{
    if (pTarget == NULL) {
        return ACTION_ERROR;
    }

    if (g_pBaldurChitin->cDimm.m_cKeyTable.FindKey(CResRef(m_curAction.GetString1()), 1005, TRUE) == NULL) {
        return ACTION_ERROR;
    }

    CItem* pItem = new CItem(CResRef(m_curAction.GetString1()),
        static_cast<WORD>(m_curAction.m_specificID),
        static_cast<WORD>(m_curAction.m_specificID2),
        static_cast<WORD>(m_curAction.m_specificID3),
        0,
        0);

    if (pTarget->m_objectType == TYPE_SPRITE) {
        CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
        if (pGame->GetCharacterPortraitNum(pTarget->GetId()) != -1) {
            pGame->FeedBack(CInfGame::FEEDBACK_ITEMGAINED, 0, TRUE);
        }

        // The target puts a copy in its inventory; the game keeps the item
        // until it is thrown away.
        CMessageAddItem* pMessage = new CMessageAddItem(*pItem, m_id, pTarget->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);

        pGame->AddDisposableItem(pItem);
        return ACTION_DONE;
    }

    if (pTarget->m_objectType == TYPE_CONTAINER) {
        static_cast<CGameContainer*>(pTarget)->PlaceItemInBlankSlot(pItem, TRUE, 0x7FFF);
        return ACTION_DONE;
    }

    // NOTE: The original leaks the item for a target of another type.
    return ACTION_ERROR;
}

// 0x465110
SHORT CGameAIBase::TakePartyGold()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    DWORD gold = m_curAction.m_specificID;
    if (gold > pGame->GetGameSave()->m_nPartyGold) {
        gold = pGame->GetGameSave()->m_nPartyGold;
    }

    if (m_objectType == TYPE_SPRITE) {
        static_cast<CGameSprite*>(this)->GetBaseStats()->m_gold += gold;
        static_cast<CGameSprite*>(this)->GetDerivedStats()->m_nGold += gold;
    }

    CMessagePartyGold* pMessage = new CMessagePartyGold(TRUE,
        TRUE,
        -static_cast<LONG>(gold),
        m_id,
        m_id);

    g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);

    return ACTION_DONE;
}

// 0x4651A0
SHORT CGameAIBase::GivePartyGold()
{
    DWORD gold = m_curAction.m_specificID;

    if (m_objectType == TYPE_SPRITE && m_curAction.m_actionID == CAIAction::GIVEPARTYGOLD) {
        if (gold > static_cast<CGameSprite*>(this)->GetDerivedStats()->m_nGold) {
            gold = static_cast<CGameSprite*>(this)->GetDerivedStats()->m_nGold;
        }

        static_cast<CGameSprite*>(this)->GetBaseStats()->m_gold -= gold;
        static_cast<CGameSprite*>(this)->GetDerivedStats()->m_nGold -= gold;
    }

    CMessagePartyGold* pMessage = new CMessagePartyGold(TRUE,
        TRUE,
        gold,
        m_id,
        m_id);

    g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);

    return ACTION_DONE;
}

// 0x465220
SHORT CGameAIBase::GivePartyGoldGlobal()
{
    CString sName = m_curAction.GetString1();
    sName.MakeUpper();
    CString sScope = m_curAction.GetString2();
    sScope.MakeUpper();

    // Bounded to 0 on the missing-area path below, where the original reads a
    // stack address instead (see the NOTE there).
    LONG nAmount = 0;

    if (strcmp(sScope, "GLOBAL") == 0) {
        CVariable* pVariable = g_pBaldurChitin->GetObjectGame()->GetVariables()->FindKey(sName);
        if (pVariable != NULL) {
            nAmount = pVariable->m_intValue;
        }
    } else if (strcmp(sScope, "LOCALS") == 0) {
        if (GetObjectType() == CGameObject::TYPE_SPRITE) {
            CVariable* pVariable = static_cast<CGameSprite*>(this)->GetLocalVariables()->FindKey(sName);
            if (pVariable != NULL) {
                nAmount = pVariable->m_intValue;
            }
        }
    } else if (strcmp(sScope, "LOCALS") == 0) {
        // NOTE: dead in the original. The only predecessor of this second
        // LOCALS compare (0x465330) is the jne from the first (0x4652cb),
        // taken only when the scope is not LOCALS, so this compare always
        // fails and the game-globals lookup below never runs.
        CVariable* pVariable = g_pBaldurChitin->GetObjectGame()->GetVariables()->FindKey(sName);
        if (pVariable != NULL) {
            nAmount = pVariable->m_intValue;
        }
    } else {
        if (strcmp(sScope, "MYAREA") == 0) {
            // NOTE: The area's name is read as a C string from the start of
            // the area (its header's RESREF), as the original does.
            sScope = reinterpret_cast<const char*>(m_pArea->m_header.m_areaName);
        }

        CGameArea* pArea = g_pBaldurChitin->GetObjectGame()->GetArea(sScope);
        if (pArea != NULL) {
            CVariable* pVariable = pArea->GetVariables()->FindKey(sName);
            if (pVariable != NULL) {
                nAmount = pVariable->m_intValue;
            }
        } else {
            // NOTE: The original loads the amount from [esp+0x14] here (0x465416).
            // That slot holds the address of the CString argument passed to
            // GetArea (0x4653C8 stores esp into it), so the amount is a stack
            // address: a large positive value. Not reproducible; bounded to
            // the 0 above.
        }
    }

    if (m_objectType == TYPE_SPRITE) {
        CGameSprite* pSprite = static_cast<CGameSprite*>(this);
        if (static_cast<DWORD>(nAmount) > pSprite->GetDerivedStats()->m_nGold) {
            nAmount = pSprite->GetDerivedStats()->m_nGold;
        }
        pSprite->GetBaseStats()->m_gold -= nAmount;
        pSprite->GetDerivedStats()->m_nGold -= nAmount;
    }

    CMessagePartyGold* pMessage = new CMessagePartyGold(TRUE,
        TRUE,
        nAmount,
        m_id,
        m_id);

    g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);

    return ACTION_DONE;
}

// 0x4654C0
SHORT CGameAIBase::GiveOrder(CGameAIBase* sprite)
{
    if (sprite == NULL) {
        return ACTION_ERROR;
    }

    CAITrigger trigger(CAITrigger::RECEIVEDORDER, m_typeAI, m_curAction.m_specificID);

    CMessageSetTrigger* pMessage = new CMessageSetTrigger(trigger,
        m_id,
        sprite->GetId());
    g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);

    return ACTION_DONE;
}

// 0x465630
SHORT CGameAIBase::DisplayString(CGameAIBase* sprite)
{
    STR_RES strRes;
    COLORREF rgbColor = RGB(0, 0, 0);
    STRREF name;

    if (sprite == NULL) {
        return ACTION_ERROR;
    }

    if (sprite->GetObjectType() == TYPE_SPRITE) {
        rgbColor = CVidPalette::RANGE_COLORS[static_cast<CGameSprite*>(sprite)->GetBaseStats()->m_colors[2]];
        name = static_cast<CGameSprite*>(sprite)->GetNameRef();
    } else {
        name = -1;
    }

    g_pBaldurChitin->GetTlkTable().Fetch(m_curAction.m_specificID, strRes);
    strRes.szText.TrimLeft();

    if (sprite->m_typeAI.GetEnemyAlly() == CAIObjectType::EA_PC) {
        if (strRes.cSound.m_nLooping == 0) {
            strRes.cSound.SetFireForget(TRUE);
        }

        if (sprite->GetObjectType() == TYPE_SPRITE) {
            strRes.cSound.SetChannel(static_cast<CGameSprite*>(sprite)->GetChannel(),
                reinterpret_cast<DWORD_PTR>(m_pArea));
        } else {
            strRes.cSound.SetChannel(14, reinterpret_cast<DWORD_PTR>(m_pArea));
        }

        strRes.cSound.Play(m_pos.x, m_pos.y, 0, FALSE);

        if (strRes.szText != "") {
            g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(name,
                m_curAction.m_specificID,
                rgbColor,
                RGB(160, 200, 215),
                -1,
                m_id,
                sprite->GetId());
        }
    } else {
        if (strRes.cSound.m_nLooping == 0) {
            strRes.cSound.SetFireForget(TRUE);
        }

        if (sprite->GetObjectType() == TYPE_SPRITE) {
            strRes.cSound.SetChannel(static_cast<CGameSprite*>(sprite)->GetChannel(),
                reinterpret_cast<DWORD_PTR>(m_pArea));
        } else {
            strRes.cSound.SetChannel(14, reinterpret_cast<DWORD_PTR>(m_pArea));
        }

        strRes.cSound.Play(m_pos.x, m_pos.y, 0, FALSE);

        if (strRes.szText != "") {
            g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(name,
                m_curAction.m_specificID,
                rgbColor,
                RGB(215, 215, 190),
                -1,
                m_id,
                sprite->GetId());
        }
    }

    return ACTION_DONE;
}

// 0x465D50
SHORT CGameAIBase::AddexperiencePartyGlobal()
{
    CString sName = m_curAction.GetString1();
    sName.MakeUpper();
    CString sScope = m_curAction.GetString2();
    sScope.MakeUpper();

    // Bounded to 0 on the missing-area path below, where the original reads a
    // stack address instead (see the NOTE there).
    LONG nAmount = 0;

    if (strcmp(sScope, "GLOBAL") == 0) {
        CVariable* pVariable = g_pBaldurChitin->GetObjectGame()->GetVariables()->FindKey(sName);
        if (pVariable != NULL) {
            nAmount = pVariable->m_intValue;
        }
    } else if (strcmp(sScope, "LOCALS") == 0) {
        if (GetObjectType() == CGameObject::TYPE_SPRITE) {
            CVariable* pVariable = static_cast<CGameSprite*>(this)->GetLocalVariables()->FindKey(sName);
            if (pVariable != NULL) {
                nAmount = pVariable->m_intValue;
            }
        }
    } else {
        if (strcmp(sScope, "MYAREA") == 0) {
            // NOTE: The area's name is read as a C string from the start of
            // the area (its header's RESREF), as the original does.
            sScope = reinterpret_cast<const char*>(m_pArea->m_header.m_areaName);
        }

        CGameArea* pArea = g_pBaldurChitin->GetObjectGame()->GetArea(sScope);
        if (pArea != NULL) {
            CVariable* pVariable = pArea->GetVariables()->FindKey(sName);
            if (pVariable != NULL) {
                nAmount = pVariable->m_intValue;
            }
        } else {
            // NOTE: The original loads the amount from [esp+0x14] here (0x465ED5).
            // That slot holds the address of the CString argument passed to
            // GetArea (0x465E87 stores esp into it), so the amount is a stack
            // address: a large positive value. Not reproducible; bounded to
            // the 0 above.
        }
    }

    g_pBaldurChitin->m_pObjectGame->AddExperienceParty(nAmount, FALSE, FALSE);

    return ACTION_DONE;
}

// 0x465F30
SHORT CGameAIBase::StartMovie()
{
    CString sMovieFileName;

    CBaldurProjector* pProjector = g_pBaldurChitin->m_pEngineProjector;
    if (!pProjector->ResolveMovieFileName(CResRef(m_curAction.GetString1()), sMovieFileName)) {
        return ACTION_ERROR;
    }

    g_pBaldurChitin->m_pEngineWorld->ReadyMovie(CResRef(m_curAction.GetString1()), FALSE);

    return ACTION_DONE;
}

// 0x466030
SHORT CGameAIBase::RevealAreaOnMap()
{
    CString sArea = m_curAction.GetString1();

    CWorldMap* pWorldMap = g_pBaldurChitin->GetObjectGame()->GetWorldMap(sArea);
    pWorldMap->EnableArea(pWorldMap->sub_55A3A0(),
        CResRef(sArea),
        TRUE);

    g_pBaldurChitin->GetObjectGame()->FeedBack(CInfGame::FEEDBACK_WORLDMAP_UPDATE, 0, TRUE);

    g_pBaldurChitin->GetBaldurMessage()->SendMapWorldRevealArea(m_curAction.GetString1());

    return ACTION_DONE;
}

// 0x466120
SHORT CGameAIBase::ChangeTileState(CGameTiledObject* target)
{
    if (target == NULL) {
        return ACTION_ERROR;
    }

    if (m_curAction.m_specificID != 0) {
        if ((target->m_dwFlags & 0x1) != 0) {
            target->ToggleState();
        }
    } else {
        if ((target->m_dwFlags & 0x1) == 0) {
            target->ToggleState();
        }
    }

    return ACTION_DONE;
}

// 0x466170
SHORT CGameAIBase::TriggerActivation(CGameTrigger* target)
{
    if (target == NULL) {
        return ACTION_ERROR;
    }

    if (m_curAction.m_specificID != 0) {
        target->m_dwFlags &= ~0x100;
    } else {
        target->m_dwFlags |= 0x100;
    }

    CMessageTriggerStatus* pMessage = new CMessageTriggerStatus(target->m_dwFlags,
        target->m_trapActivated,
        target->m_trapDetected,
        m_id,
        target->GetId());
    g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);

    return ACTION_DONE;
}

// 0x4668B0
SHORT CGameAIBase::StartMusic()
{
    if (m_pArea == NULL) {
        return ACTION_ERROR;
    }

    // TODO: Check cast.
    m_pArea->PlaySong(static_cast<SHORT>(m_curAction.m_specificID), m_curAction.m_specificID2);

    return ACTION_DONE;
}

// 0x4668E0
SHORT CGameAIBase::SetMusic()
{
    if (m_pArea == NULL) {
        return ACTION_ERROR;
    }

    if (!m_pArea->SetSong(static_cast<SHORT>(m_curAction.m_specificID), static_cast<BYTE>(m_curAction.m_specificID2))) {
        return ACTION_ERROR;
    }

    return ACTION_DONE;
}

// 0x466910
SHORT CGameAIBase::LeaveAreaLUAPanic()
{
    if (g_pChitin->cNetwork.m_bConnectionEstablished == TRUE) {
        // TODO: 0x4669B6 calls 0x4381C0 (not ported): the CBaldurMessage
        // hand-off for the networked panic jump. It is passed m_curAction's
        // dest point, m_specificID, and copies of GetString1()/GetString2()
        // (the copies feed only this call and are omitted with it), and on
        // the host ends in CScreenWorld::SetLeaveAreaLuaPanicData
        // (0x4384B0, in tree).
    }

    return ACTION_DONE;
}

// 0x466A00
SHORT CGameAIBase::FinalSave()
{
    if (g_pChitin->cNetwork.GetSessionOpen()
        && g_pChitin->cNetwork.GetSessionHosting() != TRUE) {
        return ACTION_DONE;
    }

    if (g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(m_id, CGameObjectArray::THREAD_ASYNCH, INFINITE) != CGameObjectArray::SUCCESS) {
        return ACTION_DONE;
    }

    // NOTE: Looks like inlining.
    if (1) {
        CString sSaveName("000000002-Final-Save");
        g_pBaldurChitin->GetObjectGame()->m_sSaveGame = sSaveName;
        CScreenCharacter::SAVE_NAME = sSaveName;
    }

    g_pBaldurChitin->GetObjectGame()->SaveGame(1, 0, 1);

    CGameObject* pObject;

    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(m_id,
            CGameObjectArray::THREAD_ASYNCH,
            &pObject,
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        return ACTION_ERROR;
    }

    return ACTION_DONE;
}

// 0x466B30
SHORT CGameAIBase::Unlock(CGameAIBase* pObject)
{
    CMessage* message;

    if (pObject == NULL) {
        return ACTION_ERROR;
    }

    if (pObject->GetObjectType() != TYPE_DOOR
        && pObject->GetObjectType() != TYPE_CONTAINER) {
        return ACTION_ERROR;
    }

    message = new CMessageSetForceActionPick(TRUE, m_id, pObject->GetId());
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

    CAITrigger trigger(CAITrigger::NO_TRIGGER, 0);

    if (pObject->GetObjectType() == TYPE_DOOR
        && (static_cast<CGameDoor*>(pObject)->m_dwFlags & 0x2) != 0) {
        trigger = CAITrigger(CAITrigger::UNLOCKED, m_typeAI, 0);

        message = new CMessageSetTrigger(trigger, m_id, pObject->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

        static_cast<CGameDoor*>(pObject)->m_dwFlags &= ~0x2;

        message = new CMessageUnlock(static_cast<CGameDoor*>(pObject)->m_dwFlags,
            m_id,
            pObject->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    if (pObject->GetObjectType() == TYPE_CONTAINER
        && (static_cast<CGameContainer*>(pObject)->m_dwFlags & 0x1) != 0) {
        trigger = CAITrigger(CAITrigger::UNLOCKED, m_typeAI, 0);

        message = new CMessageSetTrigger(trigger, m_id, pObject->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

        message = new CMessageUnlock(static_cast<CGameContainer*>(pObject)->m_dwFlags & ~0x1,
            m_id,
            pObject->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    return ACTION_DONE;
}

// 0x466F90
SHORT CGameAIBase::MoveGlobal(CGameSprite* pSprite)
{
    if (pSprite == NULL) {
        return ACTION_ERROR;
    }

    CString sArea = m_curAction.GetString1();
    sArea.MakeUpper();

    CMessage* message = new CMessageMoveGlobal(sArea,
        m_curAction.m_dest,
        m_id,
        pSprite->GetId());
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

    return ACTION_DONE;
}

// 0x467110
SHORT CGameAIBase::Lock(CGameAIBase* pObject)
{
    if (pObject == NULL) {
        return ACTION_ERROR;
    }

    if (pObject->GetObjectType() != TYPE_DOOR
        && pObject->GetObjectType() != TYPE_CONTAINER) {
        return ACTION_ERROR;
    }

    CMessageSetForceActionPick* pSetForceActionPick = new CMessageSetForceActionPick(TRUE,
        m_id,
        pObject->GetId());
    g_pBaldurChitin->GetMessageHandler()->AddMessage(pSetForceActionPick, FALSE);

    CAITrigger trigger(CAITrigger::NO_TRIGGER, 0);

    if (pObject->GetObjectType() == TYPE_DOOR) {
        CGameDoor* pDoor = static_cast<CGameDoor*>(pObject);
        if ((pDoor->m_dwFlags & 0x2) == 0) {
            trigger = CAITrigger(CAITrigger::UNLOCKED, m_typeAI, 0);

            CMessageSetTrigger* pSetTrigger = new CMessageSetTrigger(trigger,
                m_id,
                pDoor->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(pSetTrigger, FALSE);

            pDoor->m_dwFlags |= 0x2;

            CMessageUnlock* pUnlock = new CMessageUnlock(pDoor->m_dwFlags,
                m_id,
                pDoor->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(pUnlock, FALSE);
        }
    }

    if (pObject->GetObjectType() == TYPE_CONTAINER) {
        CGameContainer* pContainer = static_cast<CGameContainer*>(pObject);
        if ((pContainer->m_dwFlags & 0x1) == 0) {
            trigger = CAITrigger(CAITrigger::UNLOCKED, m_typeAI, 0);

            CMessageSetTrigger* pSetTrigger = new CMessageSetTrigger(trigger,
                m_id,
                pContainer->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(pSetTrigger, FALSE);

            CMessageUnlock* pUnlock = new CMessageUnlock(pContainer->m_dwFlags | 0x1,
                m_id,
                pContainer->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(pUnlock, FALSE);
        }
    }

    return ACTION_DONE;
}

// 0x467550
SHORT CGameAIBase::DestroyItem()
{
    if (GetObjectType() == TYPE_SPRITE) {
        CGameSprite* pSprite = static_cast<CGameSprite*>(this);
        SHORT slotNum = pSprite->FindItemPersonal(m_curAction.GetString1(), 0, FALSE);
        if (slotNum != -1) {
            pSprite->Unequip(slotNum);
            g_pBaldurChitin->GetObjectGame()->AddDisposableItem(pSprite->GetEquipment()->m_items[slotNum]);
            pSprite->GetEquipment()->m_items[slotNum] = NULL;
            return ACTION_DONE;
        }

        if (pSprite->TakeItemBags(m_curAction.GetString1(), 1, -1) > 0) {
            return ACTION_DONE;
        }

        return ACTION_ERROR;
    }

    if (GetObjectType() == TYPE_CONTAINER) {
        CGameContainer* pContainer = static_cast<CGameContainer*>(this);
        SHORT slotNum = pContainer->FindItemSlot(CResRef(m_curAction.GetString1()));
        if (slotNum != -1) {
            pContainer->GetItem(slotNum);
            pContainer->SetItem(slotNum, NULL);
            pContainer->CompressContainer();
        }
        return ACTION_DONE;
    }

    return ACTION_ERROR;
}

// 0x467720
SHORT CGameAIBase::DetectSecretDoor(CGameDoor* target)
{
    if (target == NULL) {
        return ACTION_ERROR;
    }

    CGameDoor* pDoor;

    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(target->GetId(),
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pDoor),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        return ACTION_ERROR;
    }

    if ((pDoor->m_dwFlags & 0x80) != 0) {
        if ((pDoor->m_dwFlags & 0x100) == 0) {
            pDoor->SetDrawPoly(400);

            pDoor->m_dwFlags |= 0x100;

            CMessageDoorStatus* pDoorStatus = new CMessageDoorStatus(pDoor,
                m_id,
                target->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(pDoorStatus, FALSE);
        } else {
            pDoor->SetDrawPoly(400);
        }
    }

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(target->GetId(),
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    return ACTION_DONE;
}

// 0x467880
SHORT CGameAIBase::FadeToColor()
{
    CMessageFadeColor* pFadeColor = new CMessageFadeColor(TRUE,
        static_cast<BYTE>(m_curAction.m_dest.x),
        static_cast<BYTE>(m_curAction.m_dest.y),
        static_cast<BYTE>(m_curAction.m_specificID),
        m_id,
        m_id);

    g_pBaldurChitin->GetMessageHandler()->AddMessage(pFadeColor, FALSE);

    return ACTION_DONE;
}

// 0x467900
SHORT CGameAIBase::FadeFromColor()
{
    CMessageFadeColor* pFadeColor = new CMessageFadeColor(FALSE,
        static_cast<BYTE>(m_curAction.m_dest.x),
        static_cast<BYTE>(m_curAction.m_dest.y),
        static_cast<BYTE>(m_curAction.m_specificID),
        m_id,
        m_id);

    g_pBaldurChitin->GetMessageHandler()->AddMessage(pFadeColor, FALSE);

    return ACTION_DONE;
}

// 0x467970
SHORT CGameAIBase::sub_467970()
{
    CMessageFadeColor* pFadeColor = new CMessageFadeColor(255,
        static_cast<BYTE>(m_curAction.m_dest.x),
        static_cast<BYTE>(m_curAction.m_dest.y),
        static_cast<BYTE>(m_curAction.m_specificID),
        m_id,
        m_id);

    g_pBaldurChitin->GetMessageHandler()->AddMessage(pFadeColor, FALSE);

    return ACTION_DONE;
}

// 0x4679E0
SHORT CGameAIBase::SpawnPtActivate(CGameSpawning* target)
{
    if (target == NULL) {
        return ACTION_ERROR;
    }

    if (target->GetObjectType() != TYPE_SPAWNING) {
        return ACTION_ERROR;
    }

    if (!target->m_spawningObject.m_activated) {
        CMessageSpawnPtActivate* pMessage = new CMessageSpawnPtActivate(TRUE,
            m_id,
            target->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
    }

    return ACTION_DONE;
}

// 0x467A80
SHORT CGameAIBase::SpawnPtDeactivate(CGameSpawning* target)
{
    if (target == NULL) {
        return ACTION_ERROR;
    }

    if (target->GetObjectType() != TYPE_SPAWNING) {
        return ACTION_ERROR;
    }

    if (target->m_spawningObject.m_activated) {
        CMessageSpawnPtActivate* pMessage = new CMessageSpawnPtActivate(FALSE,
            m_id,
            target->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
    }

    return ACTION_DONE;
}

// 0x467B10
SHORT CGameAIBase::SpawnPtSpawn(CGameSpawning* target)
{
    if (target == NULL) {
        return ACTION_ERROR;
    }

    if (target->GetObjectType() != TYPE_SPAWNING) {
        return ACTION_ERROR;
    }

    if (target->m_spawningObject.m_activated) {
        CMessageSpawnPtSpawn* pMessage = new CMessageSpawnPtSpawn(m_pos,
            m_id,
            target->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
    }

    return ACTION_DONE;
}

// 0x467BB0
SHORT CGameAIBase::StaticStart(CGameStatic* target, BOOL bStart)
{
    if (target == NULL) {
        return ACTION_ERROR;
    }

    if (target->GetObjectType() != TYPE_STATIC) {
        return ACTION_ERROR;
    }

    // NOTE: The original compares bStart with the flag bit itself
    // (`and ecx,8; cmp ebx,ecx` at 0x467BEE), so StaticStart (TRUE, 1)
    // never matches the bit (8) and never sends; StaticStop (FALSE) sends
    // only to a static whose bit is clear. Kept as the original does.
    if (bStart == (target->m_header.m_dwFlags & 0x8)) {
        CMessageStaticStart* pMessage = new CMessageStaticStart(bStart,
            m_id,
            target->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
    }

    return ACTION_DONE;
}

// 0x460D60
SCRIPTNAME& CGameAIBase::GetScriptName()
{
    return m_scriptName;
}

// 0x50A400
void CGameAIBase::SetTrigger(const CAITrigger& trigger)
{
    m_pendingTriggers.AddTail(new CAITrigger(trigger));
}

// NOTE: Inlined.
void CGameAIBase::SetDefaultScript(CAIScript* script)
{
    if (m_movementScript != NULL) {
        delete m_movementScript;
    }

    m_movementScript = script;
}

// 0x6F2C20
SHORT CGameAIBase::GetVisualRange()
{
    return m_pArea->m_visibility.field_E * 32;
}

// 0x6F2C30
SHORT CGameAIBase::GetHelpRange()
{
    return m_pArea->m_visibility.field_E * 48;
}

// NOTE: Inlined.
void CGameAIBase::ResetCurrResponse()
{
    if (m_curAction.m_actionID == CAIAction::NO_ACTION) {
        m_curResponseNum = -1;
        m_curResponseSetNum = -1;
        m_curScriptNum = -1;
    }
}

// 0x45B970
CAIAction& CGameAIBase::GetNextAction(CAIAction& action)
{
    while (!m_queuedActions.IsEmpty()) {
        CAIAction* node = m_queuedActions.RemoveHead();
        if (node->GetActionID() != CAIAction::NO_ACTION) {
            action = *node;

            CAIObjectType actorType(node->m_actorID);
            if (actorType.OfType(CAIObjectType::ANYONE, FALSE, FALSE)
                && actorType.GetName() == ""
                && actorType.m_SpecialCase[0] == 0) {
                delete node;
                return action;
            }

            actorType.Decode(this);
            CGameAIBase* actor = static_cast<CGameAIBase*>(actorType.sub_40CB20(this, CGameObject::TYPE_AIBASE, FALSE));
            if (actor != NULL) {
                action.m_actorID = CAIObjectType::ANYONE;
                action.m_internalFlags |= 0x1;

                CMessage* message = new CMessageInsertAction(action, m_id, actor->GetId());
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(actor->GetId(),
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            }
        }
        delete node;
    }

    action = CAIAction::NULL_ACTION;
    return action;
}

// The rectangles that cover r for the polygon fills (CGameContainer::FillPoly,
// CGameDoor::FillClosedPoly and FillOpenPoly, the trigger's): r itself when
// it is at most 512 x 512, else new 512 x 512 pieces of it, row by row (the
// owners delete them when there are more than one).
//
// 0x481890
void CGameAIBase::sub_481890(CRect* r, CArray<CRect*>& ary)
{
    INT nWidth = r->right - r->left;
    INT nRows = 1;
    INT nColumns = 1;
    BOOL bWide = FALSE;

    if (nWidth > 512) {
        nColumns = nWidth / 512 + 1;
        bWide = TRUE;
    }

    INT nHeight = r->bottom - r->top;
    if (nHeight > 512) {
        nRows = nHeight / 512 + 1;
    } else if (!bWide) {
        ary.SetSize(0, -1);
        ary.SetAtGrow(ary.GetSize(), r);
        return;
    }

    ary.SetSize(0, 4);

    // NOTE: The original compares the offsets, which start at 0, with r's
    // right and bottom edges, not with its size (0x481913, 0x48192C); kept.
    for (INT nRow = 0, y = 0; nRow < nRows; nRow++, y += 512) {
        if (y > r->bottom) {
            return;
        }

        for (INT nColumn = 0, x = 0; nColumn < nColumns; nColumn++, x += 512) {
            if (x > r->right) {
                break;
            }

            CRect* pRect = new CRect(r->left + x, r->top + y, INT_MAX, INT_MAX);

            INT nPieceWidth = r->right - pRect->left;
            if (nPieceWidth >= 512) {
                nPieceWidth = 512;
            }
            pRect->right = pRect->left + nPieceWidth;

            INT nPieceHeight = r->bottom - pRect->top;
            if (nPieceHeight >= 512) {
                nPieceHeight = 512;
            }
            pRect->bottom = pRect->top + nPieceHeight;

            ary.SetAtGrow(ary.GetSize(), pRect);
        }
    }
}

// 0x467C50
SHORT CGameAIBase::FireSpell(const CResRef& resRef, CGameObject* target)
{
    CString sResRef;

    if (target == NULL) {
        return ACTION_ERROR;
    }

    resRef.CopyToString(sResRef);

    CSpell* pSpell = new CSpell(CResRef(sResRef));
    if (pSpell == NULL) {
        return ACTION_ERROR;
    }

    pSpell->Demand();

    SHORT nCasterLevel;
    if (GetObjectType() == CGameObject::TYPE_SPRITE) {
        SHORT nLevel = static_cast<CGameSprite*>(this)->GetCasterLevel(pSpell, 0, 0);
        nCasterLevel = 1;
        if (nLevel > 0) {
            nCasterLevel = nLevel;
        }
    } else {
        nCasterLevel = 1;
    }

    // The last ability the caster's level reaches.
    SHORT nAbilities = 0;
    LONG nIndex = 0;
    if (pSpell->GetAbilityCount() > 0) {
        do {
            if (nCasterLevel < pSpell->GetAbility(nIndex)->minCasterLevel) {
                break;
            }
            pSpell->GetAbility(nIndex);
            nAbilities++;
            nIndex = nAbilities;
        } while (nIndex < pSpell->GetAbilityCount());
    }

    LONG nAbility = static_cast<SHORT>(nAbilities - 1);
    SPELL_ABILITY* pAbility = pSpell->GetAbility(nAbility);
    if (pAbility == NULL) {
        pSpell->Release();
        delete pSpell;
        return ACTION_ERROR;
    }

    CProjectile* pProjectile = CProjectile::DecodeProjectile(pAbility->missileType, this, 0);

    if (GetObjectType() == CGameObject::TYPE_SPRITE) {
        BYTE nClass = 0;
        DWORD nSpecialization = 0;
        BYTE nLevel = 0;
        if (static_cast<CGameSprite*>(this)->sub_727720(pSpell->GetResRef(), nClass, nSpecialization, nLevel) == TRUE) {
            pProjectile->m_nSpellLevel = nLevel;
        }
        pProjectile->m_sourceRes = pSpell->GetResRef();
    }

    CMessage* message;
    for (LONG nEffect = 0; nEffect < pAbility->effectCount; nEffect++) {
        CGameEffect* pEffect = pSpell->GetAbilityEffect(nAbility, nEffect, this, 0, 0, 0);
        pEffect->m_source = m_pos;
        pEffect->m_sourceID = m_id;
        pEffect->m_target = target->GetPos();

        switch (pEffect->m_targetType) {
        case 1:
            pEffect->m_flags |= 0x2;
            message = new CMessageAddEffect(pEffect, m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            break;
        case 2:
            pProjectile->AddEffect(pEffect);
            break;
        case 3:
            ApplyEffectToParty(pEffect);
            delete pEffect;
            break;
        case 4:
            GetArea()->ApplyEffectToSprites(pEffect, FALSE, FALSE, 0, NULL);
            delete pEffect;
            break;
        case 5:
            GetArea()->ApplyEffectToSprites(pEffect, TRUE, FALSE, 0, NULL);
            delete pEffect;
            break;
        case 6:
            GetArea()->ApplyEffectToSprites(pEffect, FALSE, TRUE, GetAIType().m_nSpecific, NULL);
            delete pEffect;
            break;
        case 7:
            GetArea()->ApplyEffectToSprites(pEffect, FALSE, TRUE, target->GetAIType().m_nSpecific, NULL);
            delete pEffect;
            break;
        case 8:
            GetArea()->ApplyEffectToSprites(pEffect, FALSE, FALSE, 0, this);
            delete pEffect;
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameAIBaseActions.cpp
            // __LINE__: 4780
            UTIL_ASSERT(FALSE);
        }
    }

    if (GetObjectType() == CGameObject::TYPE_SPRITE) {
        static_cast<CGameSprite*>(this)->FeedBack(CGameSprite::FEEDBACK_SPELL,
            0,
            0,
            0,
            pSpell->GetGenericName(),
            0,
            0);
    }

    LONG nHeight = pProjectile->DetermineHeight(static_cast<CGameSprite*>(this));
    CMessageFireProjectile* pFire = new CMessageFireProjectile(pProjectile->m_projectileType,
        target->GetId(),
        target->GetPos(),
        nHeight,
        m_id,
        m_id,
        0);

    if (pProjectile->m_projectileType == 304) {
        INT nRandom = rand() % 1000000;
        // TODO: The original also stores it in the projectile's +0x356, a
        // member of the class DecodeProjectile builds for this type (not
        // ported yet).
        pFire->field_20 = nRandom;
    }

    g_pBaldurChitin->GetMessageHandler()->AddMessage(pFire, FALSE);

    pProjectile->Fire(m_pArea, m_id, target->GetId(), target->GetPos(), 30, 0);

    pSpell->Release();
    delete pSpell;

    return ACTION_DONE;
}

// 0x4681E0
SHORT CGameAIBase::FireSpellPoint(const CResRef& resRef, const CPoint& ptTarget)
{
    // NOTE: The original builds the spell from an empty string: `resRef` is
    // never read (0x468211-0x46827A), so the spell has no ability and the
    // action fails.
    CString sResRef;

    CSpell* pSpell = new CSpell(CResRef(sResRef));
    if (pSpell == NULL) {
        return ACTION_ERROR;
    }

    pSpell->Demand();

    SHORT nCasterLevel;
    if (GetObjectType() == CGameObject::TYPE_SPRITE) {
        SHORT nLevel = static_cast<CGameSprite*>(this)->GetCasterLevel(pSpell, 0, 0);
        nCasterLevel = 1;
        if (nLevel > 0) {
            nCasterLevel = nLevel;
        }
    } else {
        nCasterLevel = 1;
    }

    SHORT nAbilities = 0;
    if (pSpell->GetAbilityCount() > 0) {
        LONG nIndex = 0;
        do {
            if (nCasterLevel < pSpell->GetAbility(nIndex)->minCasterLevel) {
                break;
            }
            nAbilities++;
            nIndex = nAbilities;
        } while (nIndex < pSpell->GetAbilityCount());
    }

    LONG nAbility = static_cast<SHORT>(nAbilities - 1);
    SPELL_ABILITY* pAbility = pSpell->GetAbility(nAbility);
    if (pAbility == NULL) {
        pSpell->Release();
        delete pSpell;
        return ACTION_ERROR;
    }

    CProjectile* pProjectile = CProjectile::DecodeProjectile(pAbility->missileType, this, 0);

    if (GetObjectType() == CGameObject::TYPE_SPRITE) {
        DWORD nSpecialization = 0;
        BYTE nClass = 0;
        BYTE nLevel = 0;
        if (static_cast<CGameSprite*>(this)->sub_727720(pSpell->GetResRef(), nClass, nSpecialization, nLevel) == TRUE) {
            pProjectile->m_nSpellLevel = nLevel;
        }
        pProjectile->m_sourceRes = pSpell->GetResRef();
    }

    CMessage* message;
    for (LONG nEffect = 0; nEffect < pAbility->effectCount; nEffect++) {
        CGameEffect* pEffect = pSpell->GetAbilityEffect(nAbility, nEffect, this, 0, 0, 0);
        pEffect->m_source = m_pos;
        pEffect->m_sourceID = m_id;
        pEffect->m_target = ptTarget;

        switch (pEffect->m_targetType) {
        case 1:
            pEffect->m_flags |= 0x2;
            message = new CMessageAddEffect(pEffect, m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            break;
        case 2:
            pProjectile->AddEffect(pEffect);
            break;
        case 3:
            ApplyEffectToParty(pEffect);
            delete pEffect;
            break;
        case 4:
            GetArea()->ApplyEffectToSprites(pEffect, FALSE, FALSE, 0, NULL);
            delete pEffect;
            break;
        case 7:
            // NOTE: no target at a point; the original just deletes it.
            delete pEffect;
            break;
        case 5:
            GetArea()->ApplyEffectToSprites(pEffect, TRUE, FALSE, 0, NULL);
            delete pEffect;
            break;
        case 6:
            GetArea()->ApplyEffectToSprites(pEffect, FALSE, TRUE, GetAIType().m_nSpecific, NULL);
            delete pEffect;
            break;
        case 8:
            GetArea()->ApplyEffectToSprites(pEffect, FALSE, FALSE, 0, this);
            delete pEffect;
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameAIBaseActions.cpp
            // __LINE__: 4942
            UTIL_ASSERT(FALSE);
        }
    }

    if (GetObjectType() == CGameObject::TYPE_SPRITE) {
        static_cast<CGameSprite*>(this)->FeedBack(CGameSprite::FEEDBACK_SPELL,
            0,
            0,
            0,
            pSpell->GetGenericName(),
            0,
            0);
    }

    LONG nHeight = pProjectile->DetermineHeight(static_cast<CGameSprite*>(this));
    CMessageFireProjectile* pFire = new CMessageFireProjectile(pProjectile->m_projectileType,
        -1,
        ptTarget,
        nHeight,
        m_id,
        m_id,
        0);

    if (pProjectile->m_projectileType == 304) {
        INT nRandom = rand() % 1000000;
        // TODO: The original also stores it in the projectile's +0x356 (not
        // ported yet).
        pFire->field_20 = nRandom;
    }

    g_pBaldurChitin->GetMessageHandler()->AddMessage(pFire, FALSE);

    pProjectile->Fire(m_pArea, m_id, -1, ptTarget, 30, 0);

    pSpell->Release();
    delete pSpell;

    return ACTION_DONE;
}

// 0x461190
SHORT CGameAIBase::ForceSpell(CGameAIBase* target)
{
    // ForceSpell (113) and ReallyForceSpell (181): the spell is cast whether
    // or not the caster has it, at a creature.
    CString sResRef;
    SHORT nCasterLevel;
    CMessage* message;

    if (m_typeAI.m_nGeneral == CAIObjectType::G_DEAD) {
        return ACTION_DONE;
    }

    if (target == NULL) {
        return ACTION_ERROR;
    }

    if (strcmp(m_curAction.GetString1(), "") == 0) {
        DecodeSpell(m_curAction.m_specificID, sResRef);
        if (strcmp(sResRef, "") == 0) {
            return ACTION_ERROR;
        }

        if (GetObjectType() == CGameObject::TYPE_SPRITE) {
            nCasterLevel = static_cast<CGameSprite*>(this)->GetDerivedStats()->m_nLevel;
        } else {
            nCasterLevel = 1;
        }
    } else {
        sResRef = m_curAction.GetString1();
        nCasterLevel = static_cast<SHORT>(m_curAction.m_specificID);
    }

    if (nCasterLevel < 2) {
        nCasterLevel = 1;
    }

    CSpell* pSpell = new CSpell(CResRef(sResRef));
    if (pSpell == NULL) {
        return ACTION_ERROR;
    }

    pSpell->Demand();

    if (m_curAction.m_actionID != 181 && !CheckAppropriateTarget(pSpell, target)) {
        pSpell->Release();
        delete pSpell;
        return ACTION_ERROR;
    }

    // The last ability the caster level reaches.
    SHORT nAbilities = 0;
    if (pSpell->GetAbilityCount() > 0) {
        LONG nIndex = 0;
        do {
            if (nCasterLevel < pSpell->GetAbility(nIndex)->minCasterLevel) {
                break;
            }
            pSpell->GetAbility(nIndex);
            nAbilities++;
            nIndex = nAbilities;
        } while (nIndex < pSpell->GetAbilityCount());
    }

    LONG nAbility = static_cast<SHORT>(nAbilities - 1);
    SPELL_ABILITY* pAbility = pSpell->GetAbility(nAbility);
    if (pAbility == NULL) {
        pSpell->Release();
        delete pSpell;
        return ACTION_ERROR;
    }

    CProjectile* pProjectile = CProjectile::DecodeProjectile(pAbility->missileType, this, 0);

    if (GetObjectType() == CGameObject::TYPE_SPRITE) {
        BYTE nClass = 0;
        DWORD nSpecialization = 0;
        BYTE nLevel = 0;
        if (static_cast<CGameSprite*>(this)->sub_727720(pSpell->GetResRef(), nClass, nSpecialization, nLevel) == TRUE) {
            pProjectile->m_nSpellLevel = nLevel;
        }
        pProjectile->m_sourceRes = pSpell->GetResRef();
    }

    if (GetObjectType() == CGameObject::TYPE_SPRITE && m_curAction.m_actionID != 181) {
        // A creature casts: the casting animation and its time first
        // (ReallyForceSpell is instant).
        CGameSprite* pSprite = static_cast<CGameSprite*>(this);
        SHORT nSequence = pSprite->m_nSequence;

        if (m_actionCount == 0) {
            pSprite->sub_755A70(pSpell, pAbility, target->GetPos());

            if (!pSprite->IsVisibleToCaller(FALSE) && target->GetId() != m_id) {
                ITEM_EFFECT effect;
                CGameEffect::ClearItemEffect(&effect, CGAMEEFFECT_FORCEVISIBLE);
                // NOTE: The duration type is the WORD constant at 0x8491AA (1).
                effect.durationType = 1;

                CGameEffect* pEffect = CGameEffect::DecodeEffect(&effect, m_pos, m_id, CPoint(-1, -1));
                message = new CMessageAddEffect(pEffect, m_id, m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }

            if (pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_SANCTUARY]) {
                ITEM_EFFECT effect;
                CGameEffect::ClearItemEffect(&effect, CGAMEEFFECT_DISPELSANCTUARY);
                effect.durationType = 1;

                CGameEffect* pEffect = CGameEffect::DecodeEffect(&effect, m_pos, m_id, CPoint(-1, -1));
                message = new CMessageAddEffect(pEffect, m_id, m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }
        }

        INT nTicks = pAbility->speedFactor * 100 / 10;
        if (m_actionCount < nTicks - 4) {
            if (nSequence != CGameSprite::SEQ_CONJURE) {
                message = new CMessageSetSequence(CGameSprite::SEQ_CONJURE, m_id, m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }

            pSpell->Release();
            delete pSpell;
            delete pProjectile;
            return ACTION_NORMAL;
        }

        if (m_actionCount < nTicks) {
            if (nSequence != CGameSprite::SEQ_CAST) {
                message = new CMessageSetSequence(CGameSprite::SEQ_CAST, m_id, m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
                pSprite->ApplyCastingEffectPost(pSpell, pAbility);
            }

            pSpell->Release();
            delete pSpell;
            delete pProjectile;
            return ACTION_NORMAL;
        }

        if (nSequence != CGameSprite::SEQ_CAST) {
            message = new CMessageSetSequence(CGameSprite::SEQ_CAST, m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            pSprite->ApplyCastingEffectPost(pSpell, pAbility);
        }
    }

    for (LONG nEffect = 0; nEffect < pAbility->effectCount; nEffect++) {
        CGameEffect* pEffect = pSpell->GetAbilityEffect(nAbility, nEffect, this, 0, 0, static_cast<BYTE>(nCasterLevel));
        pEffect->m_source = m_pos;
        pEffect->m_sourceID = m_id;
        pEffect->m_target = target->GetPos();
        pEffect->m_casterLevel = nCasterLevel;

        switch (pEffect->m_targetType) {
        case 1:
            pEffect->m_flags |= 0x2;
            message = new CMessageAddEffect(pEffect, m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            break;
        case 2:
            pProjectile->AddEffect(pEffect);
            break;
        case 3:
            ApplyEffectToParty(pEffect);
            delete pEffect;
            break;
        case 4:
            GetArea()->ApplyEffectToSprites(pEffect, FALSE, FALSE, 0, NULL);
            delete pEffect;
            break;
        case 5:
            GetArea()->ApplyEffectToSprites(pEffect, TRUE, FALSE, 0, NULL);
            delete pEffect;
            break;
        case 6:
            GetArea()->ApplyEffectToSprites(pEffect, FALSE, TRUE, GetAIType().m_nSpecific, NULL);
            delete pEffect;
            break;
        case 7:
            GetArea()->ApplyEffectToSprites(pEffect, FALSE, TRUE, target->GetAIType().m_nSpecific, NULL);
            delete pEffect;
            break;
        case 8:
            GetArea()->ApplyEffectToSprites(pEffect, FALSE, FALSE, 0, this);
            delete pEffect;
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameAIBaseActions.cpp
            // __LINE__: 1594
            UTIL_ASSERT(FALSE);
        }
    }

    if (GetObjectType() == CGameObject::TYPE_SPRITE) {
        static_cast<CGameSprite*>(this)->FeedBack(CGameSprite::FEEDBACK_SPELL,
            0,
            0,
            0,
            pSpell->GetGenericName(),
            0,
            0);
    }

    LONG nHeight = pProjectile->DetermineHeight(static_cast<CGameSprite*>(this));
    CMessageFireProjectile* pFire = new CMessageFireProjectile(pProjectile->m_projectileType,
        target->GetId(),
        target->GetPos(),
        nHeight,
        m_id,
        m_id,
        0);

    if (pProjectile->m_projectileType == 304) {
        INT nRandom = rand() % 1000000;
        // NOTE: The original also stores the seed in the projectile's +0x356
        // (CProjectileWhirlwind::m_wanderSeed, 0x461A92) before the message
        // copy at +0x20 (0x461A98).
        static_cast<CProjectileWhirlwind*>(pProjectile)->m_wanderSeed = nRandom;
        pFire->field_20 = nRandom;
    }

    g_pBaldurChitin->GetMessageHandler()->AddMessage(pFire, FALSE);

    pProjectile->Fire(m_pArea, m_id, target->GetId(), target->GetPos(), 30, 0);

    pSpell->Release();
    delete pSpell;

    return ACTION_DONE;
}

// 0x461B80
SHORT CGameAIBase::ForceSpellPoint()
{
    // ForceSpellPoint (114): the spell is cast whether or not the caster has
    // it, at the action's point.
    CPoint ptTarget = m_curAction.m_dest;
    CString sResRef;
    SHORT nCasterLevel;
    CMessage* message;

    if (m_typeAI.m_nGeneral == CAIObjectType::G_DEAD) {
        return ACTION_DONE;
    }

    if (strcmp(m_curAction.GetString1(), "") == 0) {
        DecodeSpell(m_curAction.m_specificID, sResRef);
        if (strcmp(sResRef, "") == 0) {
            return ACTION_ERROR;
        }

        if (GetObjectType() == CGameObject::TYPE_SPRITE) {
            nCasterLevel = static_cast<CGameSprite*>(this)->GetDerivedStats()->m_nLevel;
            if (nCasterLevel < 2) {
                nCasterLevel = 1;
            }
        } else {
            nCasterLevel = 1;
        }
    } else {
        sResRef = m_curAction.GetString1();
        nCasterLevel = static_cast<SHORT>(m_curAction.m_specificID);
        if (nCasterLevel < 2) {
            nCasterLevel = 1;
        }
    }

    CSpell* pSpell = new CSpell(CResRef(sResRef));
    if (pSpell == NULL) {
        return ACTION_ERROR;
    }

    pSpell->Demand();

    SHORT nAbilities = 0;
    if (pSpell->GetAbilityCount() > 0) {
        LONG nIndex = 0;
        do {
            if (nCasterLevel < pSpell->GetAbility(nIndex)->minCasterLevel) {
                break;
            }
            nAbilities++;
            nIndex = nAbilities;
        } while (nIndex < pSpell->GetAbilityCount());
    }

    LONG nAbility = static_cast<SHORT>(nAbilities - 1);
    SPELL_ABILITY* pAbility = pSpell->GetAbility(nAbility);
    if (pAbility == NULL) {
        pSpell->Release();
        delete pSpell;
        return ACTION_ERROR;
    }

    if (GetObjectType() == CGameObject::TYPE_SPRITE) {
        // A creature casts: the casting animation and its time first.
        CGameSprite* pSprite = static_cast<CGameSprite*>(this);
        SHORT nSequence = pSprite->m_nSequence;

        if (m_actionCount == 0) {
            pSprite->sub_755A70(pSpell, pAbility, ptTarget);

            if (!pSprite->IsVisibleToCaller(FALSE)) {
                ITEM_EFFECT effect;
                CGameEffect::ClearItemEffect(&effect, CGAMEEFFECT_FORCEVISIBLE);
                // NOTE: The duration type is the WORD constant at 0x8491AA (1).
                effect.durationType = 1;

                CGameEffect* pEffect = CGameEffect::DecodeEffect(&effect, m_pos, m_id, CPoint(-1, -1));
                message = new CMessageAddEffect(pEffect, m_id, m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }

            if (pSprite->GetDerivedStats()->m_spellStates[SPLSTATE_SANCTUARY]) {
                ITEM_EFFECT effect;
                CGameEffect::ClearItemEffect(&effect, CGAMEEFFECT_DISPELSANCTUARY);
                effect.durationType = 1;

                CGameEffect* pEffect = CGameEffect::DecodeEffect(&effect, m_pos, m_id, CPoint(-1, -1));
                message = new CMessageAddEffect(pEffect, m_id, m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }
        }

        INT nTicks = pAbility->speedFactor * 100 / 10;
        if (m_actionCount < nTicks - 4) {
            if (nSequence != CGameSprite::SEQ_CONJURE) {
                message = new CMessageSetSequence(CGameSprite::SEQ_CONJURE, m_id, m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }

            pSpell->Release();
            delete pSpell;
            return ACTION_NORMAL;
        }

        if (m_actionCount < nTicks) {
            if (nSequence != CGameSprite::SEQ_CAST) {
                message = new CMessageSetSequence(CGameSprite::SEQ_CAST, m_id, m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
                pSprite->ApplyCastingEffectPost(pSpell, pAbility);
            }

            pSpell->Release();
            delete pSpell;
            return ACTION_NORMAL;
        }

        // NOTE: Unlike ForceSpell, the cast's last tick sets the casting
        // sequence without the casting effects.
        if (nSequence != CGameSprite::SEQ_CAST) {
            message = new CMessageSetSequence(CGameSprite::SEQ_CAST, m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
    }

    CProjectile* pProjectile = CProjectile::DecodeProjectile(pAbility->missileType, this, 0);

    if (GetObjectType() == CGameObject::TYPE_SPRITE) {
        BYTE nClass = 0;
        DWORD nSpecialization = 0;
        BYTE nLevel = 0;
        if (static_cast<CGameSprite*>(this)->sub_727720(pSpell->GetResRef(), nClass, nSpecialization, nLevel) == TRUE) {
            pProjectile->m_nSpellLevel = nLevel;
        }
        pProjectile->m_sourceRes = pSpell->GetResRef();
    }

    for (LONG nEffect = 0; nEffect < pAbility->effectCount; nEffect++) {
        CGameEffect* pEffect = pSpell->GetAbilityEffect(nAbility, nEffect, this, 0, 0, static_cast<BYTE>(nCasterLevel));
        pEffect->m_source = m_pos;
        pEffect->m_sourceID = m_id;
        pEffect->m_target = ptTarget;
        pEffect->m_casterLevel = nCasterLevel;

        switch (pEffect->m_targetType) {
        case 1:
            pEffect->m_flags |= 0x2;
            message = new CMessageAddEffect(pEffect, m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            break;
        case 2:
            pProjectile->AddEffect(pEffect);
            break;
        case 3:
            ApplyEffectToParty(pEffect);
            delete pEffect;
            break;
        case 4:
            GetArea()->ApplyEffectToSprites(pEffect, FALSE, FALSE, 0, NULL);
            delete pEffect;
            break;
        case 7:
            // No target creature: the effect is dropped.
            delete pEffect;
            break;
        case 5:
            GetArea()->ApplyEffectToSprites(pEffect, TRUE, FALSE, 0, NULL);
            delete pEffect;
            break;
        case 6:
            GetArea()->ApplyEffectToSprites(pEffect, FALSE, TRUE, GetAIType().m_nSpecific, NULL);
            delete pEffect;
            break;
        case 8:
            GetArea()->ApplyEffectToSprites(pEffect, FALSE, FALSE, 0, this);
            delete pEffect;
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameAIBaseActions.cpp
            // __LINE__: 1845
            UTIL_ASSERT(FALSE);
        }
    }

    if (GetObjectType() == CGameObject::TYPE_SPRITE) {
        static_cast<CGameSprite*>(this)->FeedBack(CGameSprite::FEEDBACK_SPELL,
            0,
            0,
            0,
            pSpell->GetGenericName(),
            0,
            0);
    }

    LONG nHeight = pProjectile->DetermineHeight(static_cast<CGameSprite*>(this));
    CMessageFireProjectile* pFire = new CMessageFireProjectile(pProjectile->m_projectileType,
        -1,
        ptTarget,
        nHeight,
        m_id,
        m_id,
        0);

    if (pProjectile->m_projectileType == 304) {
        INT nRandom = rand() % 1000000;
        // TODO: The original also stores it in the projectile's +0x356 (not
        // ported yet).
        pFire->field_20 = nRandom;
    }

    g_pBaldurChitin->GetMessageHandler()->AddMessage(pFire, FALSE);

    pProjectile->Fire(m_pArea, m_id, -1, ptTarget, 30, 0);

    pSpell->Release();
    delete pSpell;

    return ACTION_DONE;
}

// 0x4686C0
CVariable* CGameAIBase::GetGlobalVariable(const CString& sScope, const CString& sName, int a3)
{
    CString sScopeUpper(sScope);
    CString sNameUpper(sName);
    sScopeUpper.MakeUpper();
    sNameUpper.MakeUpper();

    CVariable var;
    CVariable* pVariable;

    if (strcmp(sScopeUpper, "GLOBAL") == 0) {
        pVariable = g_pBaldurChitin->GetObjectGame()->GetVariables()->FindKey(sNameUpper);
        if (pVariable != NULL) {
            return pVariable;
        }

        var.SetName(sNameUpper);
        var.m_intValue = 0;
        g_pBaldurChitin->GetObjectGame()->m_variables.AddKey(var);
    } else {
        CGameArea* pArea = g_pBaldurChitin->GetObjectGame()->GetArea(sScopeUpper);
        if (pArea != NULL) {
            pVariable = pArea->GetVariables()->FindKey(sNameUpper);
            if (pVariable != NULL) {
                return pVariable;
            }
        }

        // NOTE: A variable missing from an area is added to the game's
        // (GLOBAL) table, as the original does.
        var.SetName(sNameUpper);
        var.m_intValue = 0;
        g_pBaldurChitin->GetObjectGame()->m_variables.AddKey(var);
    }

    return g_pBaldurChitin->GetObjectGame()->GetVariables()->FindKey(sNameUpper);
}

// 0x4530F0
void CGameAIBase::sub_4530F0(const CAIObjectType& type)
{
    field_342.Set(type);
}

// 0x453110
void CGameAIBase::sub_453110(const CAIObjectType& type)
{
    field_37E.Set(type);
}

// 0x453130
void CGameAIBase::SetLastActionReturn(SHORT returnValue)
{
    m_nLastActionReturn = returnValue;
}

// 0x45B6D0
int CGameAIBase::sub_45B6D0()
{
    return field_58C;
}

// 0x45B6E0
void CGameAIBase::sub_45B6E0(const CAIObjectType& type)
{
    field_3BA.Set(type);
}

// -----------------------------------------------------------------------------

// 0x45E250
CGameAIArea::CGameAIArea()
{
    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Add(&m_id, this, INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        delete this;
    }
}

// 0x45E2D0
CGameAIArea::~CGameAIArea()
{
}

// 0x47C830
BOOLEAN CGameAIArea::CanSaveGame(STRREF& strError)
{
    strError = -1;
    return TRUE;
}

// 0x766660
BOOLEAN CGameAIArea::CompressTime(DWORD deltaTime)
{
    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x45E2E0
CGameAIGame::CGameAIGame()
{
    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Add(&m_id, this, INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        delete this;
    }
}

// 0x45E2D0
CGameAIGame::~CGameAIGame()
{
}

// 0x47C830
BOOLEAN CGameAIGame::CanSaveGame(STRREF& strError)
{
    strError = -1;
    return TRUE;
}

// 0x766660
BOOLEAN CGameAIGame::CompressTime(DWORD deltaTime)
{
    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x45BDD0
CGameObject* CGameAIBase::GetTargetShareType(BYTE type)
{
    CGameObject* pObject = m_curAction.m_acteeID.sub_40CCA0(this, FALSE);
    if (pObject != NULL
        && pObject->GetObjectType() == CGameObject::TYPE_SPRITE
        && static_cast<CGameSprite*>(pObject)->m_derivedStats.m_cImmunitiesAIType.OnList(m_typeAI)) {
        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(pObject->m_id,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        pObject = NULL;
    }

    SetTargetObject(pObject);

    if (pObject != NULL) {
        BYTE objectType = pObject->GetObjectType();
        if (objectType != type && type != CGameObject::TYPE_AIBASE && type != CGameObject::TYPE_NONE) {
            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(pObject->m_id,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            SetTargetObject(NULL);
            return NULL;
        }

        if (type == CGameObject::TYPE_AIBASE && (pObject->GetObjectType() & CGameObject::TYPE_AIBASE) == 0) {
            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(pObject->m_id,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            SetTargetObject(NULL);
            return NULL;
        }

        if (GetObjectType() == CGameObject::TYPE_SPRITE
            && g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id) == -1
            && (static_cast<CGameSprite*>(this)->m_derivedStats.m_generalState & STATE_BLIND) != 0
            && pObject->GetObjectType() == CGameObject::TYPE_SPRITE) {
            // A blind non-party sprite reaches only what is next to it.
            const CPoint& ptTarget = pObject->GetPos();
            CPoint ptTargetGrid(ptTarget.x / CPathSearch::GRID_SQUARE_SIZEX, ptTarget.y / CPathSearch::GRID_SQUARE_SIZEY);
            CPoint ptGrid(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX, m_pos.y / CPathSearch::GRID_SQUARE_SIZEY);
            LONG nSquares = CAIUtil::CountSquares(ptGrid, ptTargetGrid);
            if (nSquares > static_cast<CGameSprite*>(this)->m_animation.GetPersonalSpace() / 2 + 4) {
                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(pObject->m_id,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
                SetTargetObject(NULL);
                return NULL;
            }
        }
    }

    SetTargetObject(pObject);
    return pObject;
}

// 0x45C290
CGameObject* CGameAIBase::sub_45C290()
{
    CGameObject* pObject = m_curAction.m_acteeID.sub_40CCA0(this, FALSE);
    if (pObject != NULL
        && pObject->GetObjectType() == CGameObject::TYPE_SPRITE
        && static_cast<CGameSprite*>(pObject)->m_derivedStats.m_cImmunitiesAIType.OnList(m_typeAI)) {
        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(pObject->m_id,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        pObject = NULL;
    }

    SetTargetObject(pObject);
    return pObject;
}

// 0x45C030
CGameObject* CGameAIBase::GetTargetShareAIType(const CAIObjectType& typeAI, BYTE type)
{
    CGameObject* pObject = typeAI.sub_40CCA0(this, FALSE);
    if (pObject != NULL
        && pObject->GetObjectType() == CGameObject::TYPE_SPRITE
        && static_cast<CGameSprite*>(pObject)->m_derivedStats.m_cImmunitiesAIType.OnList(m_typeAI)) {
        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(pObject->m_id,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        pObject = NULL;
    }

    SetTargetObject(pObject);

    if (pObject != NULL) {
        BYTE objectType = pObject->GetObjectType();
        if (objectType != type && type != CGameObject::TYPE_AIBASE && type != CGameObject::TYPE_NONE) {
            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(pObject->m_id,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            SetTargetObject(NULL);
            return NULL;
        }

        if (type == CGameObject::TYPE_AIBASE && (pObject->GetObjectType() & CGameObject::TYPE_AIBASE) == 0) {
            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(pObject->m_id,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            SetTargetObject(NULL);
            return NULL;
        }

        if (GetObjectType() == CGameObject::TYPE_SPRITE
            && g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id) == -1
            && (static_cast<CGameSprite*>(this)->m_derivedStats.m_generalState & STATE_BLIND) != 0
            && pObject->GetObjectType() == CGameObject::TYPE_SPRITE) {
            // A blind non-party sprite reaches only what is next to it.
            const CPoint& ptTarget = pObject->GetPos();
            CPoint ptTargetGrid(ptTarget.x / CPathSearch::GRID_SQUARE_SIZEX, ptTarget.y / CPathSearch::GRID_SQUARE_SIZEY);
            CPoint ptGrid(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX, m_pos.y / CPathSearch::GRID_SQUARE_SIZEY);
            LONG nSquares = CAIUtil::CountSquares(ptGrid, ptTargetGrid);
            if (nSquares > static_cast<CGameSprite*>(this)->m_animation.GetPersonalSpace() / 2 + 4) {
                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(pObject->m_id,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
                SetTargetObject(NULL);
                return NULL;
            }
        }
    }

    SetTargetObject(pObject);
    return pObject;
}

// 0x799E60
void CGameAIBase::SetTargetObject(CGameObject* pTarget)
{
}

// 0x45EB60
SHORT CGameAIBase::PlaySound(LONG x, LONG y, BOOL bPoint)
{
    CSound sound;

    if (!bPoint) {
        x = m_pos.x;
        y = m_pos.y;
    }

    sound.SetResRef(CResRef(m_curAction.GetString1()), TRUE, TRUE);

    if (sound.GetLooping()) {
        return ACTION_ERROR;
    }

    // NOTE: A connected host sends the sound to the other players here
    // (vtable 0x848B5C). Omitted: single player only
    // (docs/adr/0002-single-player-only.md).

    sound.SetFireForget(TRUE);
    sound.SetChannel(2, reinterpret_cast<DWORD_PTR>(m_pArea));
    sound.Play(x, y, 0, FALSE);

    return ACTION_DONE;
}

// 0x45EDE0
SHORT CGameAIBase::SetGlobal()
{
    CString sString1 = m_curAction.GetString1();
    sString1.MakeUpper();
    CString sScope = sString1.Left(6);
    CString sName = sString1.Right(sString1.GetLength() - 6);
    CVariable var;
    CMessage* message;

    if (m_curAction.m_actionID == 306) {
        // SetGlobalRandom
        sName = sString1.Right(sString1.GetLength() - 7);

        LONG nMin = m_curAction.m_specificID;
        LONG nMax = m_curAction.m_specificID2;
        if (nMin < nMax) {
            m_curAction.m_specificID = rand() % (nMax - nMin + 1) + nMin;
        } else if (nMin > nMax) {
            m_curAction.m_specificID = rand() % (nMin - nMax + 1) + nMax;
        }
    }

    LONG nValue = m_curAction.m_specificID;

    if (strcmp(sScope, "GLOBAL") == 0) {
        CVariable* pVariable = g_pBaldurChitin->GetObjectGame()->GetVariables()->FindKey(sName);
        if (pVariable != NULL) {
            pVariable->m_intValue = nValue;
        } else {
            var.SetName(sName);
            var.m_intValue = nValue;
            g_pBaldurChitin->GetObjectGame()->m_variables.AddKey(var);
        }

        message = new CMessageSetVariable(sScope, sName, nValue, m_id, m_id);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    } else if (strcmp(sScope, "LOCALS") == 0) {
        if (GetObjectType() == CGameObject::TYPE_SPRITE) {
            CGameSprite* pSprite = static_cast<CGameSprite*>(this);
            CString sLocalName(sName);
            CVariable* pVariable = pSprite->GetLocalVariables()->FindKey(sLocalName);
            if (pVariable != NULL) {
                pVariable->m_intValue = nValue;
            } else {
                var.SetName(sLocalName);
                var.m_intValue = nValue;
                pSprite->GetLocalVariables()->AddKey(var);
            }

            message = new CMessageSetVariable(sScope, sLocalName, nValue, m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
    } else {
        if (strcmp(sScope, "MYAREA") == 0) {
            // NOTE: The area's name is read as a C string from the start of
            // the area (its header's RESREF), as the original does.
            sScope = reinterpret_cast<const char*>(m_pArea->m_header.m_areaName);
        }

        CGameArea* pArea = g_pBaldurChitin->GetObjectGame()->GetArea(sScope);
        if (pArea != NULL) {
            CVariable* pVariable = pArea->GetVariables()->FindKey(sName);
            if (pVariable != NULL) {
                pVariable->m_intValue = nValue;
            } else {
                var.SetName(sName);
                var.m_intValue = nValue;
                pArea->m_variables.AddKey(var);
            }

            message = new CMessageSetVariable(sScope, sName, nValue, m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
    }

    return ACTION_DONE;
}

// 0x460300
SHORT CGameAIBase::IncrementGlobal()
{
    CString sString1 = m_curAction.GetString1();
    sString1.MakeUpper();
    CString sScope = sString1.Left(6);
    CString sName = sString1.Right(sString1.GetLength() - 6);
    CMessage* message;

    if (strcmp(sScope, "GLOBAL") == 0) {
        CVariable* pVariable = g_pBaldurChitin->GetObjectGame()->GetVariables()->FindKey(sName);
        if (pVariable != NULL) {
            pVariable->m_intValue += m_curAction.m_specificID;
            message = new CMessageSetVariable(sScope, sName, pVariable->m_intValue, m_id, m_id);
        } else {
            CVariable var;
            var.SetName(sName);
            var.m_intValue += m_curAction.m_specificID;
            g_pBaldurChitin->GetObjectGame()->m_variables.AddKey(var);
            message = new CMessageSetVariable(sScope, sName, var.m_intValue, m_id, m_id);
        }
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    } else if (strcmp(sScope, "LOCALS") == 0) {
        if (GetObjectType() == CGameObject::TYPE_SPRITE) {
            CGameSprite* pSprite = static_cast<CGameSprite*>(this);
            CString sLocalName(sName);
            CVariable var;
            CVariable* pVariable = pSprite->GetLocalVariables()->FindKey(sLocalName);
            if (pVariable == NULL) {
                var.SetName(sLocalName);
                var.m_intValue = m_curAction.m_specificID;
                pSprite->GetLocalVariables()->AddKey(var);
                pVariable = &var;
            } else {
                pVariable->m_intValue += m_curAction.m_specificID;
            }

            message = new CMessageSetVariable(sScope, sLocalName, pVariable->m_intValue, m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
    } else {
        if (strcmp(sScope, "MYAREA") == 0) {
            // NOTE: The area's name is read as a C string from the start of
            // the area (its header's RESREF), as the original does.
            sScope = reinterpret_cast<const char*>(m_pArea->m_header.m_areaName);
        }

        CGameArea* pArea = g_pBaldurChitin->GetObjectGame()->GetArea(sScope);
        if (pArea != NULL) {
            CVariable* pVariable = pArea->GetVariables()->FindKey(sName);
            if (pVariable == NULL) {
                CVariable var;
                var.SetName(sName);
                var.m_intValue = m_curAction.m_specificID;
                pArea->m_variables.AddKey(var);
                message = new CMessageSetVariable(sScope, sName, var.m_intValue, m_id, m_id);
            } else {
                pVariable->m_intValue += m_curAction.m_specificID;
                message = new CMessageSetVariable(sScope, sName, pVariable->m_intValue, m_id, m_id);
            }
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
    }

    return ACTION_DONE;
}

namespace {

// The op dispatch shared by 0x469040 and 0x4688A0, reached once per
// destination arm: m_specificID(2) 1 AND, 2 OR, 3 XOR. FALSE for any other
// value, which every caller turns into ACTION_ERROR before writing anything.
BOOL CombineGlobalBit(LONG& nDest, LONG nSrc, LONG nOp)
{
    switch (nOp) {
    case 1:
        nDest &= nSrc;
        return TRUE;
    case 2:
        nDest |= nSrc;
        return TRUE;
    case 3:
        nDest ^= nSrc;
        return TRUE;
    default:
        return FALSE;
    }
}

} // namespace

// 0x4688A0
SHORT CGameAIBase::BitGlobal()
{
    // Action 247 (BitGlobal): combine an integer mask into a script
    // variable with a bitwise op. String1 names the variable as
    // "SCOPE:NAME" (uppercased); m_specificID is the mask and
    // m_specificID2 picks the op: 1 AND, 2 OR, 3 XOR. Any other op
    // returns ACTION_ERROR without writing the variable.

    CMessage* message;

    CString sString = m_curAction.GetString1();
    sString.MakeUpper();
    int nColon = sString.Find(":");
    CString sScope = sString.Left(nColon);
    CString sName = sString.Right(sString.GetLength() - nColon - 1);

    if (strcmp(sScope, "GLOBAL") == 0) {
        CVariable* pVariable = g_pBaldurChitin->GetObjectGame()->GetVariables()->FindKey(sName);
        if (pVariable != NULL) {
            if (!CombineGlobalBit(pVariable->m_intValue, m_curAction.m_specificID, m_curAction.m_specificID2)) {
                return ACTION_ERROR;
            }

            message = new CMessageSetVariable(sScope, sName, pVariable->m_intValue, m_id, m_id);
        } else {
            // The original news the variable here and leaks it on the
            // invalid-op path; a stack variable outlives the AddKey copy
            // the same way.
            CVariable var;
            var.SetName(sName);
            if (!CombineGlobalBit(var.m_intValue, m_curAction.m_specificID, m_curAction.m_specificID2)) {
                return ACTION_ERROR;
            }
            g_pBaldurChitin->GetObjectGame()->m_variables.AddKey(var);

            message = new CMessageSetVariable(sScope, sName, var.m_intValue, m_id, m_id);
        }
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    } else if (strcmp(sScope, "LOCALS") == 0) {
        // Only sprites carry local variables; any other object type
        // falls through to ACTION_DONE without a message.
        if (GetObjectType() == CGameObject::TYPE_SPRITE) {
            CGameSprite* pSprite = static_cast<CGameSprite*>(this);
            CString sLocalName(sName);
            CVariable* pVariable = pSprite->GetLocalVariables()->FindKey(sLocalName);
            if (pVariable != NULL) {
                if (!CombineGlobalBit(pVariable->m_intValue, m_curAction.m_specificID, m_curAction.m_specificID2)) {
                    return ACTION_ERROR;
                }
            } else {
                // The original constructs this one on the stack too.
                CVariable var;
                var.SetName(sLocalName);
                if (!CombineGlobalBit(var.m_intValue, m_curAction.m_specificID, m_curAction.m_specificID2)) {
                    return ACTION_ERROR;
                }
                pSprite->GetLocalVariables()->AddKey(var);
                pVariable = &var;
            }

            message = new CMessageSetVariable(sScope, sLocalName, pVariable->m_intValue, m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
    } else {
        if (strcmp(sScope, "MYAREA") == 0) {
            // NOTE: the scope itself is replaced with the area's name
            // here, so the lookup and the message below carry the area
            // name. The name is read as a C string from the start of the
            // area (its header's RESREF), as the original does.
            sScope = reinterpret_cast<const char*>(m_pArea->m_header.m_areaName);
        }

        CGameArea* pArea = g_pBaldurChitin->GetObjectGame()->GetArea(sScope);
        if (pArea != NULL) {
            CVariable* pVariable = pArea->GetVariables()->FindKey(sName);
            if (pVariable != NULL) {
                if (!CombineGlobalBit(pVariable->m_intValue, m_curAction.m_specificID, m_curAction.m_specificID2)) {
                    return ACTION_ERROR;
                }
            } else {
                // The original news the variable here and deletes it
                // after the AddKey copy; a stack variable does the same.
                CVariable var;
                var.SetName(sName);
                if (!CombineGlobalBit(var.m_intValue, m_curAction.m_specificID, m_curAction.m_specificID2)) {
                    return ACTION_ERROR;
                }
                pArea->m_variables.AddKey(var);
                pVariable = &var;
            }

            message = new CMessageSetVariable(sScope, sName, pVariable->m_intValue, m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
        // An unknown area name ends the action silently.
    }

    return ACTION_DONE;
}

// 0x469040
SHORT CGameAIBase::GlobalBitGlobal()
{
    // Action 248 (GlobalBitGlobal): combine a source variable into a
    // destination variable with a bitwise op. String1 names the
    // destination and string2 the source, both "SCOPE:NAME" (uppercased);
    // m_specificID picks the op: 1 AND, 2 OR, 3 XOR.

    LONG nSrc = 0;
    CMessage* message;

    CString sString = m_curAction.GetString1();
    sString.MakeUpper();
    int nColon = sString.Find(":");
    CString sDestScope = sString.Left(nColon);
    CString sDestName = sString.Right(sString.GetLength() - nColon - 1);

    sString = m_curAction.GetString2();
    sString.MakeUpper();
    nColon = sString.Find(":");
    CString sSrcScope = sString.Left(nColon);
    CString sSrcName = sString.Right(sString.GetLength() - nColon - 1);

    if (strcmp(sSrcScope, "GLOBAL") == 0) {
        CVariable* pSrc = g_pBaldurChitin->GetObjectGame()->GetVariables()->FindKey(sSrcName);
        if (pSrc != NULL) {
            nSrc = pSrc->m_intValue;
        }
    } else if (strcmp(sSrcScope, "LOCALS") == 0) {
        // NOTE: unlike the LOCALS write below, the original reaches the
        // local variables through CGameSprite::GetLocalVariables() without
        // a sprite-type check on the source side.
        CVariable* pSrc = static_cast<CGameSprite*>(this)->GetLocalVariables()->FindKey(sSrcName);
        if (pSrc != NULL) {
            nSrc = pSrc->m_intValue;
        }
    } else {
        // NOTE: the original tests the DESTINATION scope against "MYAREA"
        // here, not the source scope: a MYAREA destination also redirects
        // the source lookup into the acting area.
        if (strcmp(sDestScope, "MYAREA") == 0) {
            // The area's name is read as a C string from the start of the
            // area (its header's RESREF), as the original does.
            sSrcScope = reinterpret_cast<const char*>(m_pArea->m_header.m_areaName);
        }

        CGameArea* pSrcArea = g_pBaldurChitin->GetObjectGame()->GetArea(sSrcScope);
        if (pSrcArea != NULL) {
            CVariable* pSrc = pSrcArea->GetVariables()->FindKey(sSrcName);
            if (pSrc != NULL) {
                nSrc = pSrc->m_intValue;
            }
        }
    }

    if (strcmp(sDestScope, "GLOBAL") == 0) {
        CVariable* pVariable = g_pBaldurChitin->GetObjectGame()->GetVariables()->FindKey(sDestName);
        if (pVariable != NULL) {
            if (!CombineGlobalBit(pVariable->m_intValue, nSrc, m_curAction.m_specificID)) {
                return ACTION_ERROR;
            }

            message = new CMessageSetVariable(sDestScope, sDestName, pVariable->m_intValue, m_id, m_id);
        } else {
            // The original news the variable here and leaks it; a stack
            // variable outlives the AddKey copy the same way.
            CVariable var;
            var.SetName(sDestName);
            if (!CombineGlobalBit(var.m_intValue, nSrc, m_curAction.m_specificID)) {
                return ACTION_ERROR;
            }
            g_pBaldurChitin->GetObjectGame()->m_variables.AddKey(var);

            message = new CMessageSetVariable(sDestScope, sDestName, var.m_intValue, m_id, m_id);
        }
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    } else if (strcmp(sDestScope, "LOCALS") == 0) {
        if (GetObjectType() == CGameObject::TYPE_SPRITE) {
            CGameSprite* pSprite = static_cast<CGameSprite*>(this);
            CString sLocalName(sDestName);
            CVariable* pVariable = pSprite->GetLocalVariables()->FindKey(sLocalName);
            if (pVariable != NULL) {
                if (!CombineGlobalBit(pVariable->m_intValue, nSrc, m_curAction.m_specificID)) {
                    return ACTION_ERROR;
                }
            } else {
                CVariable var;
                var.SetName(sLocalName);
                if (!CombineGlobalBit(var.m_intValue, nSrc, m_curAction.m_specificID)) {
                    return ACTION_ERROR;
                }
                pSprite->GetLocalVariables()->AddKey(var);
                pVariable = &var;
            }

            message = new CMessageSetVariable(sDestScope, sLocalName, pVariable->m_intValue, m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
    } else {
        if (strcmp(sDestScope, "MYAREA") == 0) {
            // NOTE: the destination scope itself is replaced with the area's
            // name here, so the message below carries the area name. The
            // name is read as a C string from the start of the area (its
            // header's RESREF), as the original does.
            sDestScope = reinterpret_cast<const char*>(m_pArea->m_header.m_areaName);
        }

        CGameArea* pArea = g_pBaldurChitin->GetObjectGame()->GetArea(sDestScope);
        if (pArea != NULL) {
            CVariable* pVariable = pArea->GetVariables()->FindKey(sDestName);
            if (pVariable != NULL) {
                if (!CombineGlobalBit(pVariable->m_intValue, nSrc, m_curAction.m_specificID)) {
                    return ACTION_ERROR;
                }
            } else {
                // The original news the variable here and deletes it after
                // the AddKey copy; a stack variable does the same.
                CVariable var;
                var.SetName(sDestName);
                if (!CombineGlobalBit(var.m_intValue, nSrc, m_curAction.m_specificID)) {
                    return ACTION_ERROR;
                }
                pArea->m_variables.AddKey(var);
                pVariable = &var;
            }

            message = new CMessageSetVariable(sDestScope, sDestName, pVariable->m_intValue, m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
    }

    return ACTION_DONE;
}

// 0x462910
SHORT CGameAIBase::SetGlobalTimer()
{
    CString sString1 = m_curAction.GetString1();
    sString1.MakeUpper();
    CString sScope = sString1.Left(6);
    CString sName = sString1.Right(sString1.GetLength() - 6);
    CMessage* message;

    if (m_curAction.m_actionID == 307) {
        // SetGlobalTimerRandom
        sName = sString1.Right(sString1.GetLength() - 7);

        LONG nMin = m_curAction.m_specificID;
        LONG nMax = m_curAction.m_specificID2;
        if (nMin < nMax) {
            m_curAction.m_specificID = rand() % (nMax - nMin + 1) + nMin;
        } else if (nMin > nMax) {
            m_curAction.m_specificID = rand() % (nMin - nMax + 1) + nMax;
        }
    }

    LONG nTime = m_curAction.m_specificID;

    if (strcmp(sScope, "GLOBAL") == 0) {
        CVariable* pVariable = g_pBaldurChitin->GetObjectGame()->GetVariables()->FindKey(sName);
        if (pVariable == NULL) {
            CVariable var;
            var.SetName(sName);
            var.m_intValue = g_pBaldurChitin->GetObjectGame()->m_worldTime.m_gameTime
                + nTime * CTimerWorld::TIMESCALE_MSEC_PER_SEC;
            g_pBaldurChitin->GetObjectGame()->m_variables.AddKey(var);
            message = new CMessageSetVariable(sScope, sName, var.m_intValue, m_id, m_id);
        } else {
            if (m_curAction.m_actionID == 308) {
                // SetGlobalTimerOnce: a running timer is kept.
                return ACTION_DONE;
            }

            pVariable->m_intValue = g_pBaldurChitin->GetObjectGame()->m_worldTime.m_gameTime
                + nTime * CTimerWorld::TIMESCALE_MSEC_PER_SEC;
            message = new CMessageSetVariable(sScope, sName, pVariable->m_intValue, m_id, m_id);
        }
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    } else if (strcmp(sScope, "LOCALS") == 0) {
        if (GetObjectType() == CGameObject::TYPE_SPRITE) {
            CGameSprite* pSprite = static_cast<CGameSprite*>(this);
            CString sLocalName(sName);
            CVariable var;
            CVariable* pVariable = pSprite->GetLocalVariables()->FindKey(sLocalName);
            if (pVariable == NULL) {
                var.SetName(sLocalName);
                var.m_intValue = g_pBaldurChitin->GetObjectGame()->m_worldTime.m_gameTime
                    + nTime * CTimerWorld::TIMESCALE_MSEC_PER_SEC;
                pSprite->GetLocalVariables()->AddKey(var);
                pVariable = &var;
            } else if (m_curAction.m_actionID != 308) {
                pVariable->m_intValue = g_pBaldurChitin->GetObjectGame()->m_worldTime.m_gameTime
                    + nTime * CTimerWorld::TIMESCALE_MSEC_PER_SEC;
            }

            // NOTE: SetGlobalTimerOnce on a running LOCALS timer still sends
            // the message, as the original does.
            message = new CMessageSetVariable(sScope, sLocalName, pVariable->m_intValue, m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
    } else {
        if (strcmp(sScope, "MYAREA") == 0) {
            // NOTE: The area's name is read as a C string from the start of
            // the area (its header's RESREF), as the original does.
            sScope = reinterpret_cast<const char*>(m_pArea->m_header.m_areaName);
        }

        CGameArea* pArea = g_pBaldurChitin->GetObjectGame()->GetArea(sScope);
        if (pArea == NULL) {
            return ACTION_DONE;
        }

        CVariable* pVariable = pArea->GetVariables()->FindKey(sName);
        if (pVariable != NULL) {
            if (m_curAction.m_actionID == 308) {
                return ACTION_DONE;
            }

            pVariable->m_intValue = g_pBaldurChitin->GetObjectGame()->m_worldTime.m_gameTime
                + nTime * CTimerWorld::TIMESCALE_MSEC_PER_SEC;
            message = new CMessageSetVariable(sScope, sName, pVariable->m_intValue, m_id, m_id);
        } else {
            CVariable var;
            var.SetName(sName);
            var.m_intValue = g_pBaldurChitin->GetObjectGame()->m_worldTime.m_gameTime
                + nTime * CTimerWorld::TIMESCALE_MSEC_PER_SEC;
            pArea->m_variables.AddKey(var);
            message = new CMessageSetVariable(sScope, sName, var.m_intValue, m_id, m_id);
        }
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    return ACTION_DONE;
}

// -----------------------------------------------------------------------------

// 0x462F90
SHORT CGameAIBase::StartCutScene()
{
    if (strcmp(m_curAction.GetString1(), "") == 0) {
        return ACTION_ERROR;
    }

    CAIScript* pScript = new CAIScript(CResRef(m_curAction.GetString1()));

    POSITION pos = pScript->m_caList.GetHeadPosition();
    while (pos != NULL) {
        CAIConditionResponse* pConditionResponse = pScript->m_caList.GetNext(pos);
        if (pConditionResponse->m_responseSet.m_responseList.GetCount() != 0) {
            CAIResponse* pResponse = pConditionResponse->m_responseSet.m_responseList.GetHead();
            if (pResponse->m_actionList.GetCount() != 0) {
                CAIAction* pAction = pResponse->m_actionList.RemoveHead();
                pAction->Decode(this);

                CGameObject* pActor = pAction->m_acteeID.sub_40CB20(this, CGameObject::TYPE_AIBASE, FALSE);
                if (pActor != NULL) {
                    CMessage* message = new CMessageInsertResponse(*pResponse, FALSE, FALSE, FALSE, m_id, pActor->m_id);
                    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

                    if ((pActor->GetObjectType() & CGameObject::TYPE_AIBASE) != 0) {
                        message = new CMessageSetInCutScene(TRUE, m_id, pActor->m_id);
                        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
                    }

                    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(pActor->m_id,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);
                }

                if (pAction != NULL) {
                    delete pAction;
                }
            }
        }
    }

    if (pScript != NULL) {
        delete pScript;
    }

    return ACTION_DONE;
}

// 0x4637B0
SHORT CGameAIBase::ClearAllActions()
{
    POSITION pos = m_pArea->m_lVertSort.GetTailPosition();
    while (pos != NULL) {
        LONG nObjectId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_pArea->m_lVertSort.GetPrev(pos)));
        if (nObjectId == m_id) {
            continue;
        }

        CGameObject* pObject;
        if (g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(nObjectId,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE)
            != CGameObjectArray::SUCCESS) {
            continue;
        }

        BYTE objectType = pObject->GetObjectType();
        if (objectType == CGameObject::TYPE_AIBASE || objectType == CGameObject::TYPE_SPRITE) {
            if (pObject != NULL) {
                CMessage* message = new CMessageClearActions(m_id, pObject->m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }

            CAIAction action(CAIAction::NO_ACTION, CPoint(-1, -1), 0, -1);
            action.m_specificID3 = 0;
            CMessage* message = new CMessageAddAction(action, m_id, pObject->m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nObjectId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    return ACTION_DONE;
}

// 0x4658A0
SHORT CGameAIBase::IncrementChapter()
{
    CScreenChapter* pChapter = g_pBaldurChitin->m_pEngineChapter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameAIBaseActions.cpp
    // __LINE__: 3423
    UTIL_ASSERT(pChapter != NULL);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameAIBaseActions.cpp
    // __LINE__: 3425
    UTIL_ASSERT(pGame != NULL);

    CResRef cResText("Chapters");

    BYTE nChapter = static_cast<BYTE>(m_curAction.m_specificID);
    if (m_curAction.m_specificID == -1) {
        CVariable* pChapterVariable = pGame->GetVariables()->FindKey(CInfGame::CHAPTER_GLOBAL);
        BYTE nCurrent = pChapterVariable != NULL ? static_cast<BYTE>(pChapterVariable->m_intValue) : 0;
        nChapter = nCurrent + 1;
    }

    if (g_pChitin->cNetwork.m_bConnectionEstablished == TRUE
        && !g_pChitin->cNetwork.m_bIsHost) {
        // NOTE: A client asks the host for the chapter (CBaldurMessage
        // 0x434BA0) and is done. Not ported: iwd2r has no client
        // (docs/adr/0002-single-player-only.md).
        return ACTION_DONE;
    }

    CVariable* pChapterVariable = pGame->GetVariables()->FindKey(CInfGame::CHAPTER_GLOBAL);
    if (pChapterVariable != NULL) {
        pChapterVariable->m_intValue = nChapter;
    }

    if (g_pChitin->cNetwork.m_bConnectionEstablished == TRUE
        && g_pChitin->cNetwork.m_bIsHost == TRUE) {
        // 0x4659CB: the host (a single-player game is a host session of the
        // null provider) starts the chapter through
        // StartChapterMultiplayerHost, which pauses the game for the
        // chapter text ("Paused for chapter text") and announces it, with
        // the chapter now in the variable and the text's resref.
        CString sResText;
        cResText.CopyToString(sResText);

        CVariable* pCurrentVariable = pGame->GetVariables()->FindKey(CInfGame::CHAPTER_GLOBAL);
        LONG nCurrentChapter = pCurrentVariable != NULL ? pCurrentVariable->m_intValue : 0;

        // NOTE: The original copies the string's characters (its length,
        // at most a resref's) over eight zero bytes.
        BYTE resRef[RESREF_SIZE] = { 0 };
        INT nLength = sResText.GetLength() < RESREF_SIZE ? sResText.GetLength() : RESREF_SIZE;
        memcpy(resRef, static_cast<LPCSTR>(sResText), nLength);

        pChapter->StartChapterMultiplayerHost(static_cast<BYTE>(nCurrentChapter), resRef);
    } else {
        pChapter->StartChapter(cResText);
        g_pBaldurChitin->GetActiveEngine()->SelectEngine(pChapter);
    }

    C2DArray tChapters;
    tChapters.Load(CResRef("chapters"));
    STRREF strEntry = tChapters.GetAtLong(CPoint(1, nChapter + 1));
    pGame->m_cJournal.AddEntry(strEntry, 0);

    return ACTION_DONE;
}

// 0x466750
SHORT CGameAIBase::MultiPlayerSync()
{
    if (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL) {
        return ACTION_DONE;
    }

    if (!g_pChitin->cNetwork.m_bConnectionEstablished) {
        return ACTION_DONE;
    }

    // NOTE: The rest synchronises the players of a session (0x46677B to
    // 0x4668A5). Omitted: single player only
    // (docs/adr/0002-single-player-only.md).
    return ACTION_DONE;
}

// 0x466220
SHORT CGameAIBase::OpenDoor(CGameDoor* pDoor)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    if (pDoor == NULL) {
        return ACTION_ERROR;
    }

    if ((pDoor->m_dwFlags & 0x1) != 0) {
        return ACTION_DONE;
    }

    pDoor->GetCloserDestPt(m_pos);

    // NOTE: An object the local player does not control asks its owner to
    // open the door (vtable 0x848C58). Omitted: single player only
    // (docs/adr/0002-single-player-only.md); without a service provider the
    // original never takes that path either.
    if (g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL
        && g_pChitin->cNetwork.m_idLocalPlayer != pDoor->m_remotePlayerID) {
        return ACTION_DONE;
    }

    BYTE rc;
    CGameObject* pObject;
    do {
        rc = pGame->GetObjectArray()->GetDeny(pDoor->m_id,
            CGameObjectArray::THREAD_ASYNCH,
            &pObject,
            INFINITE);
    } while (rc == CGameObjectArray::SHARED);

    if (rc != CGameObjectArray::SUCCESS) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameAIBaseActions.cpp
        // __LINE__: 3796
        UTIL_ASSERT(FALSE);
        return ACTION_ERROR;
    }

    CAITrigger trigger(CAITrigger::OPENED, m_typeAI, 0);

    CMessage* message = new CMessageSetTrigger(trigger, m_id, pDoor->m_id);
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

    pDoor->ToggleDoor(m_typeAI, GetObjectType() != CGameObject::TYPE_SPRITE);

    pGame->GetObjectArray()->ReleaseDeny(pDoor->m_id,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    return ACTION_DONE;
}

// 0x4664E0
SHORT CGameAIBase::CloseDoor(CGameDoor* pDoor)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    if (pDoor == NULL) {
        return ACTION_ERROR;
    }

    if ((pDoor->m_dwFlags & 0x1) == 0) {
        return ACTION_DONE;
    }

    pDoor->GetCloserDestPt(m_pos);

    // NOTE: An object the local player does not control asks its owner to
    // close the door (vtable 0x848C58). Omitted: single player only
    // (docs/adr/0002-single-player-only.md); without a service provider the
    // original never takes that path either.
    if (g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL
        && g_pChitin->cNetwork.m_idLocalPlayer != pDoor->m_remotePlayerID) {
        return ACTION_DONE;
    }

    BYTE rc;
    CGameObject* pObject;
    do {
        rc = pGame->GetObjectArray()->GetDeny(pDoor->m_id,
            CGameObjectArray::THREAD_ASYNCH,
            &pObject,
            INFINITE);
    } while (rc == CGameObjectArray::SHARED);

    if (rc != CGameObjectArray::SUCCESS) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameAIBaseActions.cpp
        // __LINE__: 3876
        UTIL_ASSERT(FALSE);
        return ACTION_ERROR;
    }

    CAITrigger trigger(CAITRIGGER_CLOSED, m_typeAI, 0);

    CMessage* message = new CMessageSetTrigger(trigger, m_id, pDoor->m_id);
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

    pDoor->ToggleDoor(m_typeAI, GetObjectType() != CGameObject::TYPE_SPRITE);

    pGame->GetObjectArray()->ReleaseDeny(pDoor->m_id,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    return ACTION_DONE;
}

// 0x45B290
// TRUE when the two names differ (the neg/sbb/neg tail normalizes
// strcmp's result to "not equal"): the TotalItemCnt trio's fist test
// (0x458BCF) and its "00MFIST" prefix test (0x458C09) both ask "is this
// NOT the default fist?".
static BOOL sub_45B290(const CString* psName, LPCTSTR szName)
{
    return strcmp(static_cast<LPCTSTR>(*psName), szName) != 0;
}

// 0x453840
BOOL CGameAIBase::EvaluateStatusTrigger(const CAITrigger& trigger)
{
    CAITrigger triggerCopy(trigger);
    BOOL bResult = FALSE;
    CGameObject* pObject = NULL;
    CGameObject* pObject2 = NULL;
    BOOL bNoRelease = FALSE;
    CString sScope;
    CString sName;
    CString sString;
    CString sUnused;

    switch (triggerCopy.m_triggerID) {
    case 0x4023:
        // True
        return TRUE;
    case 0x40E4:
        // IsTeamBitOn (0x455E44-0x455E58)
        bResult = triggerCopy.m_specificID & sub_45B6D0();
        break;
    case 0x4030:
    case 0x40C8:
        // False, HasInnateAbility
        bResult = FALSE;
        break;
    case 0x400F:
        // Global
        sString = triggerCopy.m_string1;
        sString.MakeUpper();
        sScope = sString.Left(6);
        sName = sString.Right(sString.GetLength() - 6);
        if (strcmp(sScope, "GLOBAL") == 0) {
            CVariable* pVariable = g_pBaldurChitin->GetObjectGame()->GetVariables()->FindKey(sName);
            if (pVariable != NULL) {
                bResult = pVariable->m_intValue == triggerCopy.m_specificID;
            } else {
                bResult = triggerCopy.m_specificID == 0;
            }
        } else if (strcmp(sScope, "LOCALS") == 0) {
            if (GetObjectType() != CGameObject::TYPE_SPRITE) {
                bResult = FALSE;
                break;
            }

            CVariable* pVariable = static_cast<CGameSprite*>(this)->GetLocalVariables()->FindKey(sName);
            if (pVariable != NULL) {
                bResult = pVariable->m_intValue == triggerCopy.m_specificID;
            } else {
                bResult = triggerCopy.m_specificID == 0;
            }
        } else {
            if (strcmp(sScope, "MYAREA") == 0) {
                // NOTE: The area's name is read as a C string from the start
                // of the area (its header's RESREF), as the original does.
                sScope = reinterpret_cast<const char*>(m_pArea->m_header.m_areaName);
            }

            CGameArea* pArea = g_pBaldurChitin->GetObjectGame()->GetArea(sScope);
            if (pArea == NULL) {
                break;
            }

            CVariable* pVariable = pArea->GetVariables()->FindKey(sName);
            if (pVariable != NULL) {
                bResult = pVariable->m_intValue == triggerCopy.m_specificID;
            } else {
                bResult = triggerCopy.m_specificID == 0;
            }
        }
        break;
    case 0x4034:
        // GlobalGT
        sString = triggerCopy.m_string1;
        sString.MakeUpper();
        sScope = sString.Left(6);
        sName = sString.Right(sString.GetLength() - 6);
        if (strcmp(sScope, "GLOBAL") == 0) {
            CVariable* pVariable = g_pBaldurChitin->GetObjectGame()->GetVariables()->FindKey(sName);
            if (pVariable != NULL) {
                bResult = pVariable->m_intValue > triggerCopy.m_specificID;
            } else {
                bResult = triggerCopy.m_specificID < 0;
            }
        } else if (strcmp(sScope, "LOCALS") == 0) {
            if (GetObjectType() != CGameObject::TYPE_SPRITE) {
                bResult = FALSE;
                break;
            }

            CVariable* pVariable = static_cast<CGameSprite*>(this)->GetLocalVariables()->FindKey(sName);
            if (pVariable != NULL) {
                bResult = pVariable->GetIntValue() > triggerCopy.GetSpecifics();
            } else {
                bResult = triggerCopy.GetSpecifics() < 0;
            }
        } else {
            if (strcmp(sScope, "MYAREA") == 0) {
                // NOTE: As in Global.
                sScope = reinterpret_cast<const char*>(m_pArea->m_header.m_areaName);
            }

            CGameArea* pArea = g_pBaldurChitin->GetObjectGame()->GetArea(sScope);
            if (pArea == NULL) {
                break;
            }

            CVariable* pVariable = pArea->GetVariables()->FindKey(sName);
            if (pVariable != NULL) {
                bResult = pVariable->GetIntValue() > triggerCopy.GetSpecifics();
            } else {
                bResult = triggerCopy.GetSpecifics() < 0;
            }
        }
        break;
    case 0x4035:
        // GlobalLT
        sString = triggerCopy.m_string1;
        sString.MakeUpper();
        sScope = sString.Left(6);
        sName = sString.Right(sString.GetLength() - 6);
        if (strcmp(sScope, "GLOBAL") == 0) {
            CVariable* pVariable = g_pBaldurChitin->GetObjectGame()->GetVariables()->FindKey(sName);
            if (pVariable != NULL) {
                bResult = pVariable->GetIntValue() < triggerCopy.GetSpecifics();
            } else {
                bResult = triggerCopy.GetSpecifics() > 0;
            }
        } else if (strcmp(sScope, "LOCALS") == 0) {
            if (GetObjectType() != CGameObject::TYPE_SPRITE) {
                bResult = FALSE;
                break;
            }

            CVariable* pVariable = static_cast<CGameSprite*>(this)->GetLocalVariables()->FindKey(sName);
            if (pVariable != NULL) {
                bResult = pVariable->GetIntValue() < triggerCopy.GetSpecifics();
            } else {
                bResult = triggerCopy.GetSpecifics() > 0;
            }
        } else {
            if (strcmp(sScope, "MYAREA") == 0) {
                // NOTE: As in Global.
                sScope = reinterpret_cast<const char*>(m_pArea->m_header.m_areaName);
            }

            CGameArea* pArea = g_pBaldurChitin->GetObjectGame()->GetArea(sScope);
            if (pArea == NULL) {
                break;
            }

            CVariable* pVariable = pArea->GetVariables()->FindKey(sName);
            if (pVariable != NULL) {
                bResult = pVariable->GetIntValue() < triggerCopy.GetSpecifics();
            } else {
                bResult = triggerCopy.GetSpecifics() > 0;
            }
        }
        break;
    case 0x4040:
        // GlobalTimerExpired: the name in string1, the scope in string2.
        {
            sString = triggerCopy.m_string2;
            sString.MakeUpper();
            sScope = sString.Left(6);
            sName = triggerCopy.m_string1;

            CVariable* pVariable;
            if (sScope == "GLOBAL") {
                pVariable = g_pBaldurChitin->GetObjectGame()->GetVariables()->FindKey(sName);
            } else if (sScope == "LOCALS") {
                if (GetObjectType() != CGameObject::TYPE_SPRITE) {
                    bResult = FALSE;
                    break;
                }
                pVariable = static_cast<CGameSprite*>(this)->GetLocalVariables()->FindKey(sName);
            } else {
                if (sScope == "MYAREA") {
                    sScope = reinterpret_cast<const char*>(m_pArea->GetHeader());
                }

                CGameArea* pArea = g_pBaldurChitin->GetObjectGame()->GetArea(sScope);
                if (pArea == NULL) {
                    break;
                }
                pVariable = pArea->GetVariables()->FindKey(sName);
            }

            if (pVariable == NULL) {
                bResult = TRUE;
                break;
            }

            ULONG nGameTime = g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime;
            bResult = static_cast<ULONG>(pVariable->GetIntValue()) < nGameTime;
        }
        break;
    case 0x400B:
        // Allegiance: the decoded cause's enemy-ally byte.
        triggerCopy.m_triggerCause.Decode(this);
        bResult = triggerCopy.m_triggerCause.m_nEnemyAlly == triggerCopy.m_specificID;
        break;
    case 0x400C:
        // Class
        triggerCopy.m_triggerCause.Decode(this);
        bResult = triggerCopy.m_triggerCause.IsClassValid(static_cast<BYTE>(triggerCopy.m_specificID));
        break;
    case 0x400E:
        // General
        triggerCopy.m_triggerCause.Decode(this);
        bResult = triggerCopy.m_triggerCause.m_nGeneral == triggerCopy.m_specificID;
        break;
    case 0x404D:
        // Gender
        triggerCopy.m_triggerCause.Decode(this);
        bResult = triggerCopy.m_triggerCause.m_nGender == triggerCopy.m_specificID;
        break;
    case 0x4010:
    case 0x4011:
    case 0x4012:
        // HP, HPGT, HPLT: the cause's current hit points (base stats).
        {
            CGameSprite* pSprite = NULL;
            bNoRelease = GetTriggerSprite(triggerCopy, &pSprite);
            pObject = pSprite;
            if (pSprite == NULL) {
                // NOTE: The original leaves through its early epilogue
                // (0x45AB7A) without releasing anything; nothing is shared.
                return FALSE;
            }

            LONG nHitPoints = pSprite->GetBaseStats()->m_hitPoints;
            if (triggerCopy.m_triggerID == 0x4010) {
                bResult = nHitPoints == triggerCopy.GetSpecifics();
            } else if (triggerCopy.m_triggerID == 0x4011) {
                bResult = nHitPoints > triggerCopy.GetSpecifics();
            } else {
                bResult = nHitPoints < triggerCopy.GetSpecifics();
            }
        }
        break;
    case 0x4044:
    case 0x4045:
    case 0x4046:
        // CheckStat, CheckStatGT, CheckStatLT: the cause's derived stat
        // (int1) against the value.
        {
            CGameSprite* pSprite = NULL;
            bNoRelease = GetTriggerSprite(triggerCopy, &pSprite);
            pObject = pSprite;
            if (pSprite == NULL) {
                // NOTE: The original leaves through its early epilogue
                // (0x45AB7A) without releasing anything; nothing is shared.
                return FALSE;
            }

            LONG nStat = pSprite->GetDerivedStats()->GetAtOffset(static_cast<SHORT>(triggerCopy.GetInt1()));
            if (triggerCopy.m_triggerID == 0x4044) {
                bResult = nStat == triggerCopy.GetSpecifics();
            } else if (triggerCopy.m_triggerID == 0x4045) {
                bResult = nStat > triggerCopy.GetSpecifics();
            } else {
                bResult = nStat < triggerCopy.GetSpecifics();
            }
        }
        break;
    case 0x4017:
        // Race(object, race): the decoded cause's race byte against the
        // trigger's specifics.
        triggerCopy.Decode(this);
        bResult = triggerCopy.GetCause().GetRace() == triggerCopy.GetSpecifics();
        break;
    case 0x4018:
        // Range(object, range, mode): the squared search-grid distance to
        // the object (a door's nearer walk-to point) against (range + 1)
        // squared, by mode 1 equal, 2 less, 3 greater.
        {
            triggerCopy.Decode(this);
            pObject2 = triggerCopy.GetCause().sub_40CB20(this, CGameObject::TYPE_AIBASE, FALSE);
            if (pObject2 == NULL) {
                return FALSE;
            }

            // NOTE: The original passes an unset stack CPoint to a door's
            // GetCloserDestPt (0x454881); ours starts it at (0, 0).
            CPoint pt(0, 0);
            BYTE objectType = pObject2->GetObjectType();
            if (objectType == CGameObject::TYPE_DOOR) {
                pt = static_cast<CGameDoor*>(pObject2)->GetCloserDestPt(pt);
            } else {
                pt = pObject2->GetPos();
            }

            CPoint ptTarget(pt.x / CPathSearch::GRID_SQUARE_SIZEX, pt.y / CPathSearch::GRID_SQUARE_SIZEY);
            const CPoint& pos = GetPos();
            CPoint ptThis(pos.x / CPathSearch::GRID_SQUARE_SIZEX, pos.y / CPathSearch::GRID_SQUARE_SIZEY);
            LONG nDistance = (ptTarget.x - ptThis.x) * (ptTarget.x - ptThis.x)
                + (ptTarget.y - ptThis.y) * (ptTarget.y - ptThis.y);
            LONG nRange = (triggerCopy.GetSpecifics() + 1) * (triggerCopy.GetSpecifics() + 1);

            switch (triggerCopy.m_specific2) {
            case 1:
                bResult = nDistance == nRange;
                break;
            case 2:
                bResult = nDistance < nRange;
                break;
            case 3:
                bResult = nDistance > nRange;
                break;
            default:
                bResult = FALSE;
                goto release;
            }

            if (bResult == TRUE) {
                sub_4530F0(pObject2->GetAIType());
            }
        }
        break;
    case 0x401C:
        // See(object, seeDead)
        triggerCopy.Decode(this);
        pObject2 = triggerCopy.GetCause().sub_40CCA0(this, FALSE);
        bResult = SeeTarget(pObject2, triggerCopy.GetSpecifics());
        if (bResult == TRUE) {
            if (!m_lSeen.Equal(pObject2->GetAIType())) {
                if (g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(pObject2->GetId()) != -1) {
                    m_canBeSeen = CGameObject::VISIBLE_DELAY * 4 + 4;
                }

                m_lSeen.Set(pObject2->GetAIType());
                CMessage* message = new CMessageSetLastObject(pObject2->GetAIType(), CAITRIGGER_SEE, m_id, m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }

            sub_4530F0(pObject2->GetAIType());
        }
        break;
    case 0x401D:
        // Specifics(object, specifics): the decoded cause's specific byte
        // against the trigger's specifics, through the same equality tail
        // (0x454F87) the Race arm jumps to.
        triggerCopy.Decode(this);
        bResult = triggerCopy.GetCause().GetSpecific() == triggerCopy.GetSpecifics();
        break;
    case 0x400D:
        // Exists
        triggerCopy.m_triggerCause.Decode(this);
        pObject2 = triggerCopy.m_triggerCause.sub_40D0F0(this, TRUE);
        bResult = pObject2 != NULL;
        break;
    case 0x4051:
        // Dead: no such object, not a sprite, or a sprite dead now or by
        // its base state.
        triggerCopy.Decode(this);
        pObject2 = triggerCopy.GetCause().sub_40D0F0(this, TRUE);
        if (pObject2 == NULL
            || pObject2->GetObjectType() != CGameObject::TYPE_SPRITE
            || (static_cast<CGameSprite*>(pObject2)->GetDerivedStats()->m_generalState & STATE_DEAD) != 0
            || (static_cast<CGameSprite*>(pObject2)->GetBaseStats()->m_generalState & STATE_DEAD) != 0) {
            bResult = TRUE;
        } else {
            bResult = FALSE;
        }
        break;
    case 0x40D7:
        // IsActive: a sound, a sprite or an info trigger that is active.
        triggerCopy.Decode(this);
        pObject2 = triggerCopy.GetCause().sub_40CCA0(this, FALSE);
        if (pObject2 != NULL) {
            BYTE objectType = pObject2->GetObjectType();
            if (objectType == CGameObject::TYPE_SOUND) {
                bResult = static_cast<CGameSound*>(pObject2)->IsActive();
            } else if (objectType == CGameObject::TYPE_SPRITE) {
                bResult = static_cast<CGameSprite*>(pObject2)->GetActive();
            } else if (objectType == CGameObject::TYPE_TRIGGER) {
                bResult = static_cast<WORD>(static_cast<CGameTrigger*>(pObject2)->sub_45B950());
            }
        }
        break;
    case 0x4047:
        // RandomNum(range, value): the object's random value of this
        // update. NOTE: a range of 0 divides by zero, as in the original.
        bResult = m_randValue % triggerCopy.GetSpecifics() + 1 == triggerCopy.m_specific2;
        break;
    case 0x4048:
        // RandomNumGT
        bResult = m_randValue % triggerCopy.GetSpecifics() + 1 > triggerCopy.m_specific2;
        break;
    case 0x4049:
        // RandomNumLT
        bResult = m_randValue % triggerCopy.GetSpecifics() + 1 < triggerCopy.m_specific2;
        break;
    case 0x40D1:
        // EntirePartyOnMap
        {
            CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
            INT nCharacters = pGame->m_nCharacters;
            if (nCharacters <= 0) {
                break;
            }

            bResult = TRUE;
            for (SHORT nIndex = 0; nIndex < nCharacters && bResult == TRUE; nIndex++) {
                CGameArea* pArea = m_pArea;
                pGame = g_pBaldurChitin->GetObjectGame();
                LONG nCharacterId = nIndex < pGame->m_nCharacters
                    ? pGame->m_characterPortraits[nIndex]
                    : CGameObjectArray::INVALID_INDEX;

                // NOTE: The original keeps the object of the previous
                // character when the share fails, and its epilogue releases
                // the last one a second time (a no-op on the AI thread).
                pGame->GetObjectArray()->GetShare(nCharacterId,
                    CGameObjectArray::THREAD_ASYNCH,
                    &pObject,
                    INFINITE);
                if (pObject != NULL) {
                    if ((static_cast<CGameSprite*>(pObject)->m_derivedStats.m_generalState & STATE_DEAD) == 0
                        && pObject->m_pArea != pArea) {
                        bResult = FALSE;
                    }

                    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nCharacterId,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);
                } else {
                    bResult = FALSE;
                }
            }
        }
        break;
    case 0x40D2:
        // AreaRestDisabled
        if (m_pArea != NULL && (m_pArea->m_header.m_flags & 0x2) != 0) {
            bResult = TRUE;
        }
        break;
    case 0x402B:
        // ActionListEmpty: nothing queued and no current action.
        if (m_queuedActions.GetCount() != 0) {
            bResult = FALSE;
        } else if (m_curAction.m_actionID == CAIAction::NO_ACTION) {
            bResult = TRUE;
        } else {
            bResult = FALSE;
        }
        break;
    case 0x40CD:
        // SubRace: the cause's race and subrace against the specifics'
        // high and low words.
        triggerCopy.m_triggerCause.Decode(this);
        bResult = triggerCopy.m_triggerCause.m_nRace == (triggerCopy.m_specificID >> 16)
            && triggerCopy.m_triggerCause.m_nSubRace == (triggerCopy.m_specificID & 0xFFFF);
        break;
    case 0x40CE:
        // Kit: every bit of the specifics in the sprite's specialization.
        triggerCopy.m_triggerCause.Decode(this);
        pObject = triggerCopy.m_triggerCause.sub_40CB20(this, CGameObject::TYPE_SPRITE, FALSE);
        if (pObject != NULL) {
            DWORD nKit = static_cast<DWORD>(triggerCopy.m_specificID);
            bResult = (static_cast<CGameSprite*>(pObject)->GetSpecialization() & nKit) == nKit;
        }
        break;
    case 0x40E7:
    case 0x40E8:
        // CheckSkillGT, CheckSkillLT: the object's skill m_specific2
        // against the specifics.
        {
            CGameSprite* pSprite = NULL;
            bNoRelease = GetTriggerSprite(triggerCopy, &pSprite);
            pObject = pSprite;
            if (pSprite == NULL) {
                // NOTE: The original leaves through its early epilogue
                // (0x45AB7A) without releasing anything; nothing is shared.
                return FALSE;
            }

            // NOTE: The skill index is not checked, as in the original.
            INT nSkill = pSprite->GetDerivedStats()->m_nSkills[triggerCopy.m_specific2];
            if (triggerCopy.m_triggerID == 0x40E7) {
                bResult = nSkill > triggerCopy.m_specificID;
            } else {
                bResult = nSkill < triggerCopy.m_specificID;
            }
        }
        break;
    case 0x4042:
        // PartyHasItem: a party member carries the item (string1),
        // personally or in a bag.
        {
            CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
            for (SHORT nIndex = 0; nIndex < pGame->GetNumCharacters(); nIndex++) {
                LONG nCharacterId = pGame->GetCharacterId(nIndex);
                CGameObject* pCharacter;
                BYTE rc;
                do {
                    rc = pGame->GetObjectArray()->GetShare(nCharacterId,
                        CGameObjectArray::THREAD_ASYNCH,
                        &pCharacter,
                        INFINITE);
                } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

                if (rc != CGameObjectArray::SUCCESS) {
                    // NOTE: The original gives up at the first character it
                    // cannot share (the early epilogue 0x45AB7A), without
                    // trying the rest.
                    return FALSE;
                }

                CGameSprite* pSprite = static_cast<CGameSprite*>(pCharacter);
                BOOL bFound = pSprite->FindItemPersonal(triggerCopy.GetString1(), 0, FALSE) != -1
                    || pSprite->FindItemBags(triggerCopy.GetString1(), 0, FALSE) != -1;

                pGame->GetObjectArray()->ReleaseShare(nCharacterId,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);

                if (bFound) {
                    return TRUE;
                }
            }
            return FALSE;
        }
    case 0x40A7:
        // CreatureHidden: the object's hidden flag (base stats field_294).
        {
            CGameSprite* pSprite = NULL;
            bNoRelease = GetTriggerSprite(triggerCopy, &pSprite);
            pObject = pSprite;
            if (pSprite == NULL) {
                // NOTE: The original leaves through its early epilogue
                // (0x45AB7A) without releasing anything; nothing is shared.
                return FALSE;
            }

            bResult = pSprite->m_baseStats.field_294;
        }
        break;
    case 0x40A9:
        // NearLocation(object, x, y, range): the object within `range`
        // search squares of (x, y).
        {
            CGameSprite* pSprite = NULL;
            bNoRelease = GetTriggerSprite(triggerCopy, &pSprite);
            pObject = pSprite;
            if (pSprite == NULL) {
                // NOTE: The original leaves through its early epilogue
                // (0x45AB7A) without releasing anything; nothing is shared.
                return FALSE;
            }

            // NOTE: The saved location (-2) is read through the sprite's
            // base stats (0x453190) whatever the caller is, as the original
            // does.
            if (triggerCopy.m_specificID == -2) {
                triggerCopy.m_specificID = static_cast<USHORT>(static_cast<CGameSprite*>(this)->m_baseStats.field_2E4);
            } else if (triggerCopy.m_specificID == -1) {
                triggerCopy.m_specificID = GetPos().x;
            }

            if (triggerCopy.m_specific2 == -2) {
                triggerCopy.m_specific2 = static_cast<USHORT>(static_cast<CGameSprite*>(this)->m_baseStats.field_2E6);
            } else if (triggerCopy.m_specific2 == -1) {
                triggerCopy.m_specific2 = GetPos().y;
            }

            if (triggerCopy.m_specificID > 0 && triggerCopy.m_specific2 > 0) {
                LONG nRange = triggerCopy.m_specific3;
                LONG dx = (pSprite->GetPos().x - triggerCopy.m_specificID) / CPathSearch::GRID_SQUARE_SIZEX;
                LONG dy = (pSprite->GetPos().y - triggerCopy.m_specific2) / CPathSearch::GRID_SQUARE_SIZEY;
                bResult = dy * dy + dx * dx <= nRange * nRange;
            }
        }
        break;
    case 0x4083:
    case 0x4084:
    case 0x4085:
    case 0x4086:
        // Unnamed in TRIGGER.IDS (the file jumps 0x407D -> 0x4089): the
        // area's battle song counter against the trigger's specifics, by
        // equal (0x4083), less (0x4084, jumping to the setl tail 0x456AA3)
        // and greater (0x4085, jumping to the setg tail 0x459A21); 0x4086
        // answers the area type word AND the low word of the specifics,
        // raw, not 0 or 1.
        if (triggerCopy.m_triggerID == 0x4083) {
            bResult = m_pArea->GetBattleSongCounter() == triggerCopy.GetSpecifics();
        } else if (triggerCopy.m_triggerID == 0x4084) {
            bResult = m_pArea->GetBattleSongCounter() < triggerCopy.GetSpecifics();
        } else if (triggerCopy.m_triggerID == 0x4085) {
            bResult = m_pArea->GetBattleSongCounter() > triggerCopy.GetSpecifics();
        } else {
            bResult = m_pArea->GetHeader()->m_areaType & (triggerCopy.GetSpecifics() & 0xFFFF);
        }
        break;
    case 0x40AE:
        // IsHeartOfFuryModeOn(): the nightmare-mode option's value itself is
        // the answer, not a normalized 0 or 1.
        bResult = g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nNightmareMode;
        break;
    case 0x40EE:
        // IsExtendedNight(): bit 0x40 of the area type word; the raw value
        // is the answer, not 0 or 1.
        bResult = m_pArea->GetHeader()->m_areaType & 0x40;
        break;
    case 0x40EF:
        // InCutsceneMode
        bResult = g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_mode == 0x142;
        break;
    case 0x4027:
        // Delay(n): this sprite's AI round counter (field_54C, which
        // ProcessAI increments when it runs this sprite's AI) divided by n
        // (signed idiv) with the remainder compared by <= against the
        // offscreen rounds counter (field_550, the short ProcessAI raises
        // while this sprite is out of the viewport, capped at 120).
        bResult = field_54C % triggerCopy.GetSpecifics() <= field_550;
        break;
    case 0x404E:
    case 0x404F:
    case 0x4050:
        // PartyGold, PartyGoldGT, PartyGoldLT: the party gold dword of the
        // game save against the trigger's specifics. The three arms share
        // the same loads (GetObjectGame, GetGameSave, GetSpecifics); 0x404E
        // compares inline with sete, while 0x404F and 0x4050 each answer a
        // carry pair (cmp/sbb/neg: 1 iff the first operand is below the
        // second as unsigned).
        {
            DWORD nPartyGold = g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_nPartyGold;
            if (triggerCopy.m_triggerID == 0x404E) {
                bResult = nPartyGold == triggerCopy.GetSpecifics();
            } else if (triggerCopy.m_triggerID == 0x404F) {
                bResult = nPartyGold > triggerCopy.GetSpecifics();
            } else {
                bResult = nPartyGold < triggerCopy.GetSpecifics();
            }
        }
        break;
    case 0x4066:
        // HappinessGT(object, n): the trigger sprite's happiness against n
        // by greater. As in HP (0x4010), a null sprite leaves through the
        // early epilogue (0x45AB7A); the original then jumps straight into
        // the setl/setg tail of the NumInParty arms below (0x45683F) with
        // ax holding GetHappiness' short, so the greater-than compare is
        // that other arm's code, entered with a different ax.
        {
            CGameSprite* pSprite = NULL;
            bNoRelease = GetTriggerSprite(triggerCopy, &pSprite);
            pObject = pSprite;
            if (pSprite == NULL) {
                // NOTE: The original leaves through its early epilogue
                // (0x45AB7A) without releasing anything; nothing is shared.
                return FALSE;
            }

            bResult = pSprite->GetHappiness() > triggerCopy.GetSpecifics();
        }
        break;
    case 0x406A:
    case 0x406B:
    case 0x406C:
        // NumInParty, NumInPartyGT, NumInPartyLT: GetNumCharacters' short
        // (sign-extended, movsx) against the trigger's specifics. The three
        // arms share the same loads; 0x406A answers equality through the
        // sub/neg/sbb/inc sequence (1 iff the difference is zero), 0x406B
        // and 0x406C compare that difference's operands directly (setg,
        // setl).
        {
            LONG nNumInParty = g_pBaldurChitin->GetObjectGame()->GetNumCharacters();
            if (triggerCopy.m_triggerID == 0x406A) {
                bResult = nNumInParty == triggerCopy.GetSpecifics();
            } else if (triggerCopy.m_triggerID == 0x406B) {
                bResult = nNumInParty > triggerCopy.GetSpecifics();
            } else {
                bResult = nNumInParty < triggerCopy.GetSpecifics();
            }
        }
        break;
    case 0x40B5:
        // TotalItemCnt(object, n): the count of the items the named object
        // carries against n, by equal. A sprite's equipment counts each
        // stackable by its charges, each bag (item type 0x3A) by the game's
        // count of that bag's contents, and everything else as one item --
        // except the default fists ("00FIST", or any "00MFIST*" by its
        // seven-letter prefix, with file flags 0x40 without 0x4). A
        // container counts its whole list. The original repeats this walk
        // in full for each of the five other TotalItemCnt arms; this file
        // keeps the repetitions instead of sharing a helper.
        {
            triggerCopy.Decode(this);
            pObject2 = triggerCopy.GetCause().sub_40CB20(this, CGameObject::TYPE_AIBASE, FALSE);
            if (pObject2 == NULL) {
                return FALSE;
            }

            LONG nCount = 0;
            if (pObject2->GetObjectType() == CGameObject::TYPE_SPRITE) {
                CGameSprite* pSprite = static_cast<CGameSprite*>(pObject2);
                for (INT nSlot = CGameSpriteEquipment::NUM_SLOT - 1; nSlot >= 0; nSlot--) {
                    CItem* pItem = pSprite->GetEquipment()->m_items[nSlot];
                    if (pItem == NULL) {
                        continue;
                    }

                    CString sResRef = pItem->GetResRef().GetResRefStr();
                    if (pItem->GetMaxStackable() > 1) {
                        nCount += pItem->GetUsageCount(0);
                    } else if (pItem->GetItemType() == 0x3A) {
                        nCount += g_pBaldurChitin->GetObjectGame()->GetNumberOfItemsInBag(
                            pItem->GetResRef(), CString(""));
                    } else {
                        CString sLeft = sResRef.Left(7);
                        if (sub_45B290(&sResRef, "00FIST")
                            && sub_45B290(&sLeft, "00MFIST")
                            && (pItem->GetFlagsFile() & 0x44) != 0x40) {
                            nCount += 1;
                        }
                    }
                }
            } else if (pObject2->GetObjectType() == CGameObject::TYPE_CONTAINER) {
                nCount = static_cast<CGameContainer*>(pObject2)->CountItemExcept(CString(""));
            }

            bResult = nCount == triggerCopy.GetSpecifics();
        }
        break;
    case 0x40B6:
        // TotalItemCntGT(object, n): the walk of 0x40B5 above, against n by
        // greater.
        {
            triggerCopy.Decode(this);
            pObject2 = triggerCopy.GetCause().sub_40CB20(this, CGameObject::TYPE_AIBASE, FALSE);
            if (pObject2 == NULL) {
                return FALSE;
            }

            LONG nCount = 0;
            if (pObject2->GetObjectType() == CGameObject::TYPE_SPRITE) {
                CGameSprite* pSprite = static_cast<CGameSprite*>(pObject2);
                for (INT nSlot = CGameSpriteEquipment::NUM_SLOT - 1; nSlot >= 0; nSlot--) {
                    CItem* pItem = pSprite->GetEquipment()->m_items[nSlot];
                    if (pItem == NULL) {
                        continue;
                    }

                    CString sResRef = pItem->GetResRef().GetResRefStr();
                    if (pItem->GetMaxStackable() > 1) {
                        nCount += pItem->GetUsageCount(0);
                    } else if (pItem->GetItemType() == 0x3A) {
                        nCount += g_pBaldurChitin->GetObjectGame()->GetNumberOfItemsInBag(
                            pItem->GetResRef(), CString(""));
                    } else {
                        CString sLeft = sResRef.Left(7);
                        if (sub_45B290(&sResRef, "00FIST")
                            && sub_45B290(&sLeft, "00MFIST")
                            && (pItem->GetFlagsFile() & 0x44) != 0x40) {
                            nCount += 1;
                        }
                    }
                }
            } else if (pObject2->GetObjectType() == CGameObject::TYPE_CONTAINER) {
                nCount = static_cast<CGameContainer*>(pObject2)->CountItemExcept(CString(""));
            }

            bResult = nCount > triggerCopy.GetSpecifics();
        }
        break;
    case 0x40B7:
        // TotalItemCntLT(object, n): the walk of 0x40B5 above, against n by
        // less.
        {
            triggerCopy.Decode(this);
            pObject2 = triggerCopy.GetCause().sub_40CB20(this, CGameObject::TYPE_AIBASE, FALSE);
            if (pObject2 == NULL) {
                return FALSE;
            }

            LONG nCount = 0;
            if (pObject2->GetObjectType() == CGameObject::TYPE_SPRITE) {
                CGameSprite* pSprite = static_cast<CGameSprite*>(pObject2);
                for (INT nSlot = CGameSpriteEquipment::NUM_SLOT - 1; nSlot >= 0; nSlot--) {
                    CItem* pItem = pSprite->GetEquipment()->m_items[nSlot];
                    if (pItem == NULL) {
                        continue;
                    }

                    CString sResRef = pItem->GetResRef().GetResRefStr();
                    if (pItem->GetMaxStackable() > 1) {
                        nCount += pItem->GetUsageCount(0);
                    } else if (pItem->GetItemType() == 0x3A) {
                        nCount += g_pBaldurChitin->GetObjectGame()->GetNumberOfItemsInBag(
                            pItem->GetResRef(), CString(""));
                    } else {
                        CString sLeft = sResRef.Left(7);
                        if (sub_45B290(&sResRef, "00FIST")
                            && sub_45B290(&sLeft, "00MFIST")
                            && (pItem->GetFlagsFile() & 0x44) != 0x40) {
                            nCount += 1;
                        }
                    }
                }
            } else if (pObject2->GetObjectType() == CGameObject::TYPE_CONTAINER) {
                nCount = static_cast<CGameContainer*>(pObject2)->CountItemExcept(CString(""));
            }

            bResult = nCount < triggerCopy.GetSpecifics();
        }
        break;
    case 0x40C1:
        // TotalItemCntExclude(object, n, excluded): the count of 0x40B5
        // above with the named resref left out, against n by equal. The
        // sprite walk is the exclude family's own: the excluded resref is
        // skipped before the stack test, a bag counts through
        // GetNumberOfItemsInBag(resref, excluded) -- not "" -- and a plain
        // item counts one with no fist test. A container still counts
        // CountItemExcept(""): of the exclude family only 0x40C2 hands the
        // container the excluded name. The original repeats this walk in
        // full for 0x40C2 and 0x40C3 as well.
        {
            triggerCopy.Decode(this);
            pObject2 = triggerCopy.GetCause().sub_40CB20(this, CGameObject::TYPE_AIBASE, FALSE);
            if (pObject2 == NULL) {
                return FALSE;
            }

            LONG nCount = 0;
            if (pObject2->GetObjectType() == CGameObject::TYPE_SPRITE) {
                CGameSprite* pSprite = static_cast<CGameSprite*>(pObject2);
                for (INT nSlot = CGameSpriteEquipment::NUM_SLOT - 1; nSlot >= 0; nSlot--) {
                    CItem* pItem = pSprite->GetEquipment()->m_items[nSlot];
                    if (pItem == NULL) {
                        continue;
                    }

                    if (pItem->GetResRef() == triggerCopy.GetString1()) {
                        continue;
                    }

                    if (pItem->GetMaxStackable() > 1) {
                        nCount += pItem->GetUsageCount(0);
                    } else if (pItem->GetItemType() == 0x3A) {
                        nCount += g_pBaldurChitin->GetObjectGame()->GetNumberOfItemsInBag(
                            pItem->GetResRef(), CString(triggerCopy.GetString1()));
                    } else {
                        nCount += 1;
                    }
                }
            } else if (pObject2->GetObjectType() == CGameObject::TYPE_CONTAINER) {
                nCount = static_cast<CGameContainer*>(pObject2)->CountItemExcept(CString(""));
            }

            bResult = nCount == triggerCopy.GetSpecifics();
        }
        break;
    case 0x40C2:
        // TotalItemCntExcludeGT(object, n, excluded): the walk of 0x40C1
        // above, against n by greater -- and the one arm of the family
        // whose container count takes the excluded name instead of "".
        {
            triggerCopy.Decode(this);
            pObject2 = triggerCopy.GetCause().sub_40CB20(this, CGameObject::TYPE_AIBASE, FALSE);
            if (pObject2 == NULL) {
                return FALSE;
            }

            LONG nCount = 0;
            if (pObject2->GetObjectType() == CGameObject::TYPE_SPRITE) {
                CGameSprite* pSprite = static_cast<CGameSprite*>(pObject2);
                for (INT nSlot = CGameSpriteEquipment::NUM_SLOT - 1; nSlot >= 0; nSlot--) {
                    CItem* pItem = pSprite->GetEquipment()->m_items[nSlot];
                    if (pItem == NULL) {
                        continue;
                    }

                    if (pItem->GetResRef() == triggerCopy.GetString1()) {
                        continue;
                    }

                    if (pItem->GetMaxStackable() > 1) {
                        nCount += pItem->GetUsageCount(0);
                    } else if (pItem->GetItemType() == 0x3A) {
                        nCount += g_pBaldurChitin->GetObjectGame()->GetNumberOfItemsInBag(
                            pItem->GetResRef(), CString(triggerCopy.GetString1()));
                    } else {
                        nCount += 1;
                    }
                }
            } else if (pObject2->GetObjectType() == CGameObject::TYPE_CONTAINER) {
                nCount = static_cast<CGameContainer*>(pObject2)->CountItemExcept(CString(triggerCopy.GetString1()));
            }

            bResult = nCount > triggerCopy.GetSpecifics();
        }
        break;
    case 0x40C3:
        // TotalItemCntExcludeLT(object, n, excluded): the walk of 0x40C1
        // above, against n by less.
        {
            triggerCopy.Decode(this);
            pObject2 = triggerCopy.GetCause().sub_40CB20(this, CGameObject::TYPE_AIBASE, FALSE);
            if (pObject2 == NULL) {
                return FALSE;
            }

            LONG nCount = 0;
            if (pObject2->GetObjectType() == CGameObject::TYPE_SPRITE) {
                CGameSprite* pSprite = static_cast<CGameSprite*>(pObject2);
                for (INT nSlot = CGameSpriteEquipment::NUM_SLOT - 1; nSlot >= 0; nSlot--) {
                    CItem* pItem = pSprite->GetEquipment()->m_items[nSlot];
                    if (pItem == NULL) {
                        continue;
                    }

                    if (pItem->GetResRef() == triggerCopy.GetString1()) {
                        continue;
                    }

                    if (pItem->GetMaxStackable() > 1) {
                        nCount += pItem->GetUsageCount(0);
                    } else if (pItem->GetItemType() == 0x3A) {
                        nCount += g_pBaldurChitin->GetObjectGame()->GetNumberOfItemsInBag(
                            pItem->GetResRef(), CString(triggerCopy.GetString1()));
                    } else {
                        nCount += 1;
                    }
                }
            } else if (pObject2->GetObjectType() == CGameObject::TYPE_CONTAINER) {
                nCount = static_cast<CGameContainer*>(pObject2)->CountItemExcept(CString(""));
            }

            bResult = nCount < triggerCopy.GetSpecifics();
        }
        break;
    case 0x401E:
    case 0x4068:
    case 0x4069:
        // Time, TimeGT, TimeLT(I:Time*): the world timer's current hour (a
        // byte, zero-extended) against the trigger's specifics. The three
        // arms share the same loads; 0x401E answers equality through the
        // sub/neg/sbb/inc sequence (1 iff the difference is zero), 0x4068
        // and 0x4069 compare the operands directly (setg, setl).
        {
            LONG nHour = g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->GetCurrentHour();
            if (triggerCopy.m_triggerID == 0x401E) {
                bResult = nHour == triggerCopy.GetSpecifics();
            } else if (triggerCopy.m_triggerID == 0x4068) {
                bResult = nHour > triggerCopy.GetSpecifics();
            } else {
                bResult = nHour < triggerCopy.GetSpecifics();
            }
        }
        break;
    case 0x40B3:
        // SummoningLimitGT(object, n): the summon registry's count of
        // occupied slots (Icewind586B70::GetCount) against the specifics
        // by greater. The sprite is resolved first only so the epilogue
        // can release it; as in HP (0x4010), a null sprite leaves through
        // the early epilogue (0x45AB7A).
        {
            CGameSprite* pSprite = NULL;
            bNoRelease = GetTriggerSprite(triggerCopy, &pSprite);
            pObject = pSprite;
            if (pSprite == NULL) {
                // NOTE: The original leaves through its early epilogue
                // (0x45AB7A) without releasing anything; nothing is shared.
                return FALSE;
            }

            bResult = Icewind586B70::Instance()->GetCount() > triggerCopy.GetSpecifics();
        }
        break;
    case 0x4095:
    case 0x4096:
        // LevelGT, LevelLT(object, level): the trigger sprite's combined
        // class level -- GetClassMaskLevel over the full class mask 0xFFF,
        // the sum of its class levels -- against the specifics. The two
        // arms share the same loads; 0x4095 answers greater through the
        // setg tail 0x459A21 and 0x4096 less through the setl tail
        // 0x456AA3. As in HP (0x4010), a null sprite leaves through the
        // early epilogue (0x45AB7A).
        {
            CGameSprite* pSprite = NULL;
            bNoRelease = GetTriggerSprite(triggerCopy, &pSprite);
            pObject = pSprite;
            if (pSprite == NULL) {
                // NOTE: The original leaves through its early epilogue
                // (0x45AB7A) without releasing anything; nothing is shared.
                return FALSE;
            }

            LONG nLevel = pSprite->GetDerivedStats()->GetClassMaskLevel(0xFFF);
            if (triggerCopy.m_triggerID == 0x4095) {
                bResult = nLevel > triggerCopy.GetSpecifics();
            } else {
                bResult = nLevel < triggerCopy.GetSpecifics();
            }
        }
        break;
    case 0x40D5:
        // IsCreatureHiddenInShadows(object): the resolved sprite's hide
        // flag (sub_453170, m_bHiding) taken raw as the answer. As in
        // IsActive (0x40D7), a cause that resolves to nothing zeroes
        // bResult (0x45AB39) and falls into the release tail.
        triggerCopy.Decode(this);
        pObject2 = triggerCopy.GetCause().sub_40CB20(this, CGameObject::TYPE_SPRITE, FALSE);
        if (pObject2 != NULL) {
            bResult = static_cast<CGameSprite*>(pObject2)->sub_453170();
        }
        break;
    case 0x40F5:
        // IsPathCriticalObject(object): bit 0x2000 of the resolved
        // object's base stats flags (the flag action 283
        // SetCriticalPathObject sets and clears) shifted down and masked
        // to 0/1. NOTE: as in the original, the object is not type-checked
        // -- whatever sub_40CCA0 returns is read as a sprite.
        triggerCopy.Decode(this);
        pObject2 = triggerCopy.GetCause().sub_40CCA0(this, FALSE);
        if (pObject2 != NULL) {
            bResult = (static_cast<CGameSprite*>(pObject2)->GetBaseStats()->m_flags & 0x2000) != 0;
        }
        break;
    case 0x4037:
    case 0x4038:
        // StateCheck, NotStateCheck(object, state): the trigger's state
        // mask (the specifics) AND the trigger sprite's derived general
        // state word. The two arms share the same loads; 0x4037 answers
        // the nonzero test (neg/sbb/neg tail) and 0x4038 the zero test
        // (neg/sbb/inc tail). As in HP (0x4010), a null sprite leaves
        // through the early epilogue (0x45AB7A).
        {
            CGameSprite* pSprite = NULL;
            bNoRelease = GetTriggerSprite(triggerCopy, &pSprite);
            pObject = pSprite;
            if (pSprite == NULL) {
                // NOTE: The original leaves through its early epilogue
                // (0x45AB7A) without releasing anything; nothing is shared.
                return FALSE;
            }

            DWORD dwState = triggerCopy.GetSpecifics()
                & pSprite->GetDerivedStats()->m_generalState;
            if (triggerCopy.m_triggerID == 0x4037) {
                bResult = dwState != 0;
            } else {
                bResult = dwState == 0;
            }
        }
        break;
    case 0x4064:
        // HasWeaponEquiped(object): the trigger sprite's selected weapon
        // slot (the equipment byte at +0xCC) against SLOT_FIST (10, the
        // byte 0x85BD83), setne. As in HP (0x4010), a null sprite leaves
        // through the early epilogue (0x45AB7A).
        {
            CGameSprite* pSprite = NULL;
            bNoRelease = GetTriggerSprite(triggerCopy, &pSprite);
            pObject = pSprite;
            if (pSprite == NULL) {
                // NOTE: The original leaves through its early epilogue
                // (0x45AB7A) without releasing anything; nothing is shared.
                return FALSE;
            }

            bResult = pSprite->GetEquipment()->m_selectedWeapon
                != CGameSpriteEquipment::SLOT_FIST;
        }
        break;
    case 0x4065:
        // Happiness(object, n): the trigger sprite's happiness against the
        // specifics by equal (sub/neg/sbb/inc). The loads are HappinessGT's
        // (0x4066); only the normalizer differs. As in HP (0x4010), a null
        // sprite leaves through the early epilogue (0x45AB7A).
        {
            CGameSprite* pSprite = NULL;
            bNoRelease = GetTriggerSprite(triggerCopy, &pSprite);
            pObject = pSprite;
            if (pSprite == NULL) {
                // NOTE: The original leaves through its early epilogue
                // (0x45AB7A) without releasing anything; nothing is shared.
                return FALSE;
            }

            bResult = pSprite->GetHappiness() == triggerCopy.GetSpecifics();
        }
        break;
    case 0x407E:
        // Unnamed in TRIGGER.IDS (the file jumps 0x407D -> 0x4089): the
        // trigger's string1 against the current area's name, read as a C
        // string from the start of the area (its header's RESREF, as the
        // MYAREA scope in Global 0x400F reads it). The original builds two
        // CStrings only to hand their buffers to CompareStrings (0x7E8182,
        // lstrcmpi) and takes the raw compare as the answer -- 0 when the
        // names match, nonzero when they differ -- so unlike every other
        // arm nothing normalizes it to 0/1.
        bResult = _stricmp(triggerCopy.m_string1,
            reinterpret_cast<const char*>(m_pArea->m_header.m_areaName));
        break;
    case 0x40DD:
        // HPLostLT(object, n): the trigger sprite's lost hit points -- the
        // derived max (m_nMaxHitPoints) minus the base current
        // (m_hitPoints), both sign-extended shorts -- against the
        // specifics by less (the shared setl tail 0x4551AF, which other
        // arms jump into as 0x4551AB). As in HP (0x4010), a null sprite
        // leaves through the early epilogue (0x45AB7A).
        {
            CGameSprite* pSprite = NULL;
            bNoRelease = GetTriggerSprite(triggerCopy, &pSprite);
            pObject = pSprite;
            if (pSprite == NULL) {
                // NOTE: The original leaves through its early epilogue
                // (0x45AB7A) without releasing anything; nothing is shared.
                return FALSE;
            }

            LONG nHPLost = pSprite->GetDerivedStats()->m_nMaxHitPoints
                - pSprite->GetBaseStats()->m_hitPoints;
            bResult = nHPLost < triggerCopy.GetSpecifics();
        }
        break;
    case 0x40EA:
    case 0x40EB:
        // LevelInClassGT, LevelInClassLT(object, level, class): the
        // trigger sprite's level in the trigger's class (int1,
        // m_specific2) -- CDerivedStats::GetClassLevel, whose UTIL_ASSERT
        // fires for a class of 0 or less, as in the original -- against
        // the specifics. The two arms share the same loads; 0x40EA
        // answers greater through the setg tail 0x459A21 and 0x40EB less
        // through the setl tail 0x456AA3 (both tails are LevelGT/LT's,
        // 0x4095/0x4096). As in HP (0x4010), a null sprite leaves through
        // the early epilogue (0x45AB7A).
        {
            CGameSprite* pSprite = NULL;
            bNoRelease = GetTriggerSprite(triggerCopy, &pSprite);
            pObject = pSprite;
            if (pSprite == NULL) {
                // NOTE: The original leaves through its early epilogue
                // (0x45AB7A) without releasing anything; nothing is shared.
                return FALSE;
            }

            LONG nLevel = pSprite->GetDerivedStats()->GetClassLevel(triggerCopy.GetInt1());
            if (triggerCopy.m_triggerID == 0x40EA) {
                bResult = nLevel > triggerCopy.GetSpecifics();
            } else {
                bResult = nLevel < triggerCopy.GetSpecifics();
            }
        }
        break;
    case 0x4014:
    case 0x4015:
    case 0x4016:
        // Morale, MoraleGT, MoraleLT(object, n): the trigger sprite's base
        // morale byte (m_morale, +0x264 of GetBaseStats) against the LOW
        // byte of the specifics. The three arms share the same loads and
        // differ only in the normalizer: 0x4014 answers sete (equal),
        // 0x4015 sbb/neg after comparing the specifics byte first (morale
        // greater) and 0x4016 sbb/neg after comparing the morale first
        // (morale less); the byte compares are unsigned. As in HP (0x4010),
        // a null sprite leaves through the early epilogue (0x45AB7A).
        {
            CGameSprite* pSprite = NULL;
            bNoRelease = GetTriggerSprite(triggerCopy, &pSprite);
            pObject = pSprite;
            if (pSprite == NULL) {
                // NOTE: The original leaves through its early epilogue
                // (0x45AB7A) without releasing anything; nothing is shared.
                return FALSE;
            }

            BYTE nMorale = pSprite->GetBaseStats()->m_morale;
            BYTE nSpecifics = static_cast<BYTE>(triggerCopy.GetSpecifics());
            if (triggerCopy.m_triggerID == 0x4014) {
                bResult = nMorale == nSpecifics;
            } else if (triggerCopy.m_triggerID == 0x4015) {
                bResult = nMorale > nSpecifics;
            } else {
                bResult = nMorale < nSpecifics;
            }
        }
        break;
    case 0x40EC:
        // IsCreatureAreaFlag(object, flags): the trigger's flag mask (the
        // specifics) AND the trigger sprite's base stats flags dword
        // (m_flags, +0x8 of GetBaseStats -- the dword action 283's bit
        // 0x2000 lives in), normalized by neg/sbb/neg to nonzero 1, as in
        // StateCheck (0x4037). Unlike Morale above, a null sprite does NOT
        // leave through the early epilogue: the arm zeroes bResult
        // (0x454606) and falls into the release tail.
        {
            CGameSprite* pSprite = NULL;
            bNoRelease = GetTriggerSprite(triggerCopy, &pSprite);
            pObject = pSprite;
            if (pSprite != NULL) {
                bResult = (triggerCopy.GetSpecifics()
                    & pSprite->GetBaseStats()->m_flags) != 0;
            }
        }
        break;
    default:
        // TODO: Every trigger not listed above (the byte table 0x45B0C4 and
        // the jump table 0x45AE40, cases 0x453BDE-0x45AD4C) is not ported;
        // it answers FALSE until it is.
        bResult = FALSE;
        break;
    }

release:
    if (pObject != NULL && !bNoRelease) {
        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(pObject->GetId(),
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    if (pObject2 != NULL) {
        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(pObject2->GetId(),
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    return bResult;
}

// 0x45D050
BOOL CGameAIBase::GetTriggerSprite(CAITrigger& trigger, CGameSprite** ppSprite)
{
    trigger.m_triggerCause.Decode(this);
    *ppSprite = static_cast<CGameSprite*>(trigger.m_triggerCause.sub_40CB20(this, CGameObject::TYPE_SPRITE, FALSE));
    if (*ppSprite != NULL && (*ppSprite)->GetAIType().Equal(CAIObjectType::NOT_SPRITE)) {
        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare((*ppSprite)->m_id,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        *ppSprite = NULL;
    }

    return FALSE;
}

// 0x44DAC0
BOOL CGameAIBase::SeeTarget(CGameObject* pTarget, BOOL bSeeDead)
{
    if (pTarget == NULL) {
        return FALSE;
    }

    if (pTarget->m_pArea != m_pArea) {
        return FALSE;
    }

    if (pTarget->GetObjectType() == CGameObject::TYPE_SPRITE) {
        CGameSprite* pSprite = static_cast<CGameSprite*>(pTarget);
        if (pSprite->m_baseStats.field_294) {
            return FALSE;
        }

        if (!bSeeDead && (pSprite->m_derivedStats.m_generalState & STATE_DEAD) != 0) {
            return FALSE;
        }
    }

    BOOL bCheckIfExplored = FALSE;

    const CPoint& ptTargetPos = pTarget->GetPos();
    LONG nTargetX = ptTargetPos.x / CPathSearch::GRID_SQUARE_SIZEX;
    LONG nTargetY = ptTargetPos.y / CPathSearch::GRID_SQUARE_SIZEY;
    const CPoint& ptPos = GetPos();
    LONG nX = ptPos.x / CPathSearch::GRID_SQUARE_SIZEX;
    LONG nY = ptPos.y / CPathSearch::GRID_SQUARE_SIZEY;
    LONG nDistance = (nY - nTargetY) * (nY - nTargetY) + (nX - nTargetX) * (nX - nTargetX);

    if (pTarget->GetObjectType() == CGameObject::TYPE_SPRITE) {
        bCheckIfExplored = static_cast<CGameSprite*>(pTarget)->Orderable(FALSE);
    }

    if (nDistance > GetVisualRange() * GetVisualRange()) {
        return FALSE;
    }

    if (!m_pArea->CheckLOS(pTarget->GetPos(), GetPos(), GetVisibleTerrainTable(), static_cast<BOOLEAN>(bCheckIfExplored))) {
        return FALSE;
    }

    return TRUE;
}

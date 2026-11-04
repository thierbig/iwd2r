#include "CVisualEffect.h"

#include <cmath>

#include "CBaldurChitin.h"
#include "CChitin.h"
#include "CGameObjectArray.h"
#include "CGameSprite.h"
#include "CInfGame.h"

// 0x84ED40
const SHORT CVEFVidCell::OFFSET_FROMX_X[16] = {1000, 924, 707, 383, 0, -383, -707, -924, -1000, -924, -707, -383, 0, 383, 707, 924};
// 0x84ED60
const SHORT CVEFVidCell::OFFSET_FROMY_X[16] = {0, -383, -707, -924, -1000, -924, -707, -383, 0, 383, 707, 924, 1000, 924, 707, 383};
// 0x84ED80
const SHORT CVEFVidCell::OFFSET_FROMX_Y[16] = {0, 383, 707, 924, 1000, 924, 707, 383, 0, -383, -707, -924, -1000, -924, -707, -383};
// 0x84EDA0
const SHORT CVEFVidCell::OFFSET_FROMY_Y[16] = {1000, 924, 707, 383, 0, -383, -707, -924, -1000, -924, -707, -383, 0, 383, 707, 924};

// 0x84EDF8
const LONG CVisualEffect::MAX_FRAME_RATE = 30;

// 0x5513F0
void CVEFVidCell::FrameAdvance(LONG nCounter)
{
    BOOL advance;
    if (m_animationSpeed > CVisualEffect::MAX_FRAME_RATE / 2) {
        advance = nCounter % (CVisualEffect::MAX_FRAME_RATE / (CVisualEffect::MAX_FRAME_RATE - m_animationSpeed)) != 0;
    } else {
        advance = nCounter % (CVisualEffect::MAX_FRAME_RATE / m_animationSpeed) == 0;
    }
    if (advance) {
        if ((m_bltFlagsExtra & 1) == 0 && m_cVidCell->IsEndOfSequence(FALSE)) {
            m_done = TRUE;
        }
        m_cVidCell->FrameAdvance();
        if (m_cShadowVidCell != NULL) {
            m_cShadowVidCell->FrameAdvance();
        }
    }
    if (m_nDuration > 0 && nCounter > m_nDuration) {
        m_done = TRUE;
    }
}

// 0x551490
void CVEFVidCell::CalculatePos(const CPoint& pos)
{
    LONG x = m_offset.x;
    LONG y = m_offset.y;
    if ((m_offsetFlags & 1) != 0) {
        x = static_cast<LONG>(static_cast<DWORD>(OFFSET_FROMX_X[m_newDirection]) * static_cast<DWORD>(m_offset.x)
            + static_cast<DWORD>(OFFSET_FROMY_X[m_newDirection]) * static_cast<DWORD>(m_offset.y)) / 1000;
        y = static_cast<LONG>(static_cast<DWORD>(OFFSET_FROMX_Y[m_newDirection]) * static_cast<DWORD>(m_offset.x)
            + static_cast<DWORD>(OFFSET_FROMY_Y[m_newDirection]) * static_cast<DWORD>(m_offset.y)) / 1000;
    }
    m_pos.x = static_cast<LONG>(static_cast<DWORD>(pos.x) + static_cast<DWORD>(x));
    m_pos.y = static_cast<LONG>(static_cast<DWORD>(pos.y) + static_cast<DWORD>(y));
}

// 0x551520
void CVEFVidCell::SetDirection(const CPoint& pos, const CPoint& oldPos, const CPoint& targetPos, SHORT nDirection)
{
    if ((m_directionFlags & 1) != 0) {
        if (m_pos == targetPos) {
            m_newDirection = static_cast<SHORT>(m_direction);
        } else {
            CPoint start(m_pos.x, static_cast<LONG>(static_cast<DWORD>(m_pos.y) << 2) / 3);
            CPoint target(targetPos.x, static_cast<LONG>(static_cast<DWORD>(targetPos.y) << 2) / 3);
            m_newDirection = CGameSprite::GetDirection(start, target);
        }
    } else if ((m_directionFlags & 2) != 0) {
        m_newDirection = nDirection;
    } else {
        if (oldPos == pos) {
            m_newDirection = static_cast<SHORT>(m_direction);
        } else {
            CPoint start(oldPos.x, static_cast<LONG>(static_cast<DWORD>(oldPos.y) << 2) / 3);
            CPoint target(pos.x, static_cast<LONG>(static_cast<DWORD>(pos.y) << 2) / 3);
            m_newDirection = CGameSprite::GetDirection(start, target);
        }
    }
}

// 0x551670
SHORT CVEFVidCell::GetDirection(const CPoint& pos, const CPoint& targetPos, SHORT nDirection)
{
    if (pos == targetPos) {
        return nDirection;
    }
    CPoint start(pos.x, static_cast<LONG>(static_cast<DWORD>(pos.y) << 2) / 3);
    CPoint target(targetPos.x, static_cast<LONG>(static_cast<DWORD>(targetPos.y) << 2) / 3);
    return CGameSprite::GetDirection(start, target);
}

// 0x5544C0
CVisualEffectFile::CVisualEffectFile(CResRef res)
{
    // The binary inlines the helper's initial SetResRef operation here.
    if (cResRef != res) {
        if (pRes != NULL && cResRef != "" && bLoading) {
            static_cast<CRes*>(pRes)->CancelRequest();
        }
        if (res == "") {
            pRes = NULL;
            cResRef = "";
        } else {
            pRes = static_cast<CResBinary*>(g_pChitin->cDimm.GetResObject(res, 1020, TRUE));
            if (pRes == NULL) {
                cResRef = "";
            } else {
                bLoading = TRUE;
                static_cast<CRes*>(pRes)->Request();
                cResRef = res;
            }
        }
    }
}

// 0x5545E0
CVisualEffect* CVisualEffectFile::GetVEF()
{
    if (pRes == NULL) {
        return NULL;
    }
    BYTE* data = static_cast<BYTE*>(pRes->Demand());
    if (data == NULL) {
        pRes->Release();
        return NULL;
    }
    const CVEFHeader* header = reinterpret_cast<const CVEFHeader*>(data);
    CVisualEffect* effect = new CVisualEffect;
    for (DWORD i = 0; i < header->timedCount; ++i) {
        const CVEFEvent* record = reinterpret_cast<const CVEFEvent*>(data + header->timedOffset) + i;
        CVEFEvent* event = new CVEFEvent;
        *event = *record;
        event->m_res.MakeUpper();
        effect->m_timedEvents.AddTail(event);
    }
    for (DWORD i = 0; i < header->triggeredCount; ++i) {
        const CVEFEvent* record = reinterpret_cast<const CVEFEvent*>(data + header->triggeredOffset) + i;
        CVEFEvent* event = new CVEFEvent;
        *event = *record;
        event->m_res.MakeUpper();
        // Both native append calls address +0x24A, including this second array.
        effect->m_timedEvents.AddTail(event);
    }
    effect->m_nTargetType = header->targetType;
    effect->m_nSpeed = header->speed;
    effect->m_nDuration = header->duration;
    pRes->Release();
    return effect;
}

// 0x554870
CVisualEffect::CVisualEffect()
    : m_activeAnimations(10)
    , m_timedEvents(10)
    , m_triggeredEvents(10)
{
    memset(static_cast<CVisualEffectBase*>(this), 0, sizeof(CVisualEffectBase));
    m_ptTargetPos = CPoint(0, 0);
    m_nTargetId = CGameObjectArray::INVALID_INDEX;
    m_posExact = CPoint(0, 0);
    m_posOld = CPoint(0, 0);
    m_posDelta = CPoint(0, 0);
    m_nSpeed = -1;
    m_nDuration = -1;
    m_nExistanceCounter = 0;
    m_nDirection = 0;
    m_nNewDirection = 0;
    memcpy(m_terrainTable, DEFAULT_TERRAIN_TABLE, sizeof(m_terrainTable));
    m_AISpeed = AI_SPEED_NORMAL;
}

// 0x5549D0
CVisualEffect::~CVisualEffect()
{
    CGameObjectArray* objects = g_pBaldurChitin->GetObjectGame()->GetObjectArray();
    POSITION pos = m_activeAnimations.GetHeadPosition();
    while (pos != NULL) {
        POSITION current = pos;
        LONG id = m_activeAnimations.GetNext(pos);
        CGameObject* animation;
        BYTE rc;
        do {
            rc = objects->GetDeny(id, CGameObjectArray::THREAD_ASYNCH, &animation, INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);
        if (rc == CGameObjectArray::SUCCESS) {
            m_activeAnimations.RemoveAt(current);
            delete animation;
            objects->ReleaseDeny(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
        }
    }
    while (!m_timedEvents.IsEmpty()) {
        delete m_timedEvents.RemoveHead();
    }
    while (!m_triggeredEvents.IsEmpty()) {
        delete m_triggeredEvents.RemoveHead();
    }
}

// 0x554B70
void CVisualEffect::AIUpdate()
{
    CPoint targetPos = m_ptTargetPos;
    Move(targetPos);
    CGameObjectArray* objects = g_pBaldurChitin->GetObjectGame()->GetObjectArray();
    POSITION pos = m_activeAnimations.GetHeadPosition();
    POSITION current = pos;
    while (pos != NULL) {
        LONG id = m_activeAnimations.GetNext(pos);
        CGameObject* object;
        BYTE rc;
        do {
            rc = objects->GetDeny(id, CGameObjectArray::THREAD_ASYNCH, &object, INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);
        if (rc == CGameObjectArray::SUCCESS) {
            CVEFVidCell* animation = static_cast<CVEFVidCell*>(object);
            if ((animation->m_bltFlagsExtra & 0x20) != 0) {
                m_activeAnimations.RemoveAt(current);
                delete object;
                objects->ReleaseDeny(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
                // 0x554C42 also skips updating the saved removal position.
                continue;
            } else {
                animation->SetDirection(m_pos, m_posOld, targetPos, 0);
                animation->CalculatePos(m_pos);
                if ((m_flags & 1) != 0 && (animation->m_bltFlagsExtra & 4) != 0) {
                    animation->m_posZ = m_posZ;
                }
                animation->FrameAdvance(m_nExistanceCounter);
                if (animation->m_done) {
                    m_activeAnimations.RemoveAt(current);
                    delete object;
                }
            }
            objects->ReleaseDeny(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
        }
        current = pos;
    }
    m_nExistanceCounter = static_cast<LONG>(static_cast<DWORD>(m_nExistanceCounter) + 1);
    m_posOld = m_pos;
}

// 0x554D20
LONG CVisualEffect::Load(CString res, CGameArea* pArea, const CPoint& pos,
    LONG targetId, const CPoint& targetPos, LONG height)
{
    CPoint target = targetPos == CPoint(-1, -1) ? pos : targetPos;
    res.MakeUpper();
    CResRef ref(res);
    if (g_pBaldurChitin->cDimm.m_cKeyTable.FindKey(ref, 1020, TRUE) == NULL) {
        return CGameObjectArray::INVALID_INDEX;
    }
    LONG id = CGameObjectArray::INVALID_INDEX;
    {
        CVisualEffectFile file(ref);
        CVisualEffect* effect = file.GetVEF();
        if (effect != NULL) {
            effect->m_nTargetId = targetId;
            effect->m_ptTargetPos = target;
            if (g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Add(&effect->m_id, effect, INFINITE) == CGameObjectArray::SUCCESS) {
                effect->AddToArea(pArea, pos, height, LIST_FRONT);
                effect->m_posExact.x = static_cast<LONG>(static_cast<DWORD>(effect->m_pos.x) << 10);
                effect->m_posExact.y = static_cast<LONG>(static_cast<DWORD>(static_cast<LONG>(static_cast<DWORD>(effect->m_pos.y) << 2) / 3) << 10);
                id = effect->m_id;
            } else {
                // Native 0x554EC5 reads this unchanged ID after deleting the
                // controller; capture the same value while it is still alive.
                id = effect->m_id;
                delete effect;
            }
        }
    }
    return id;
}

// 0x554F00
void CVisualEffect::Move(const CPoint& targetPos)
{
    LONG x = static_cast<LONG>(static_cast<DWORD>(targetPos.x) - static_cast<DWORD>(m_pos.x));
    LONG y = static_cast<LONG>(static_cast<DWORD>(targetPos.y) << 2) / 3
        - static_cast<LONG>(static_cast<DWORD>(m_pos.y) << 2) / 3;
    m_nNewDirection = CVEFVidCell::GetDirection(m_pos, targetPos, static_cast<SHORT>(m_nNewDirection));
    DWORD xx = static_cast<DWORD>(x) * static_cast<DWORD>(x);
    DWORD yy = static_cast<DWORD>(y) * static_cast<DWORD>(y);
    DWORD radius = static_cast<DWORD>(m_nSpeed) + 1;
    LONG distance = static_cast<LONG>(static_cast<DWORD>(static_cast<LONG>(yy << 4) / 9) + xx);
    if (distance > static_cast<LONG>(radius * radius)) {
        LONG vx = static_cast<LONG>(static_cast<DWORD>(CVEFVidCell::OFFSET_FROMX_X[m_nNewDirection]) * static_cast<DWORD>(m_ptStartDeltaV.x)
            + static_cast<DWORD>(CVEFVidCell::OFFSET_FROMY_X[m_nNewDirection]) * static_cast<DWORD>(m_ptStartDeltaV.y)) / 1000;
        LONG vy = static_cast<LONG>(static_cast<DWORD>(CVEFVidCell::OFFSET_FROMX_Y[m_nNewDirection]) * static_cast<DWORD>(m_ptStartDeltaV.x)
            + static_cast<DWORD>(CVEFVidCell::OFFSET_FROMY_Y[m_nNewDirection]) * static_cast<DWORD>(m_ptStartDeltaV.y)) / 1000;
        LONG length = static_cast<LONG>(sqrt(static_cast<double>(static_cast<LONG>(xx + yy))) + 0.5);
        if (length == 0) {
            m_posDelta = CPoint(1, 1);
        } else {
            m_posDelta.x = static_cast<LONG>((static_cast<DWORD>(x) << 10) * static_cast<DWORD>(m_nSpeed)) / length;
            m_posDelta.y = static_cast<LONG>((static_cast<DWORD>(y) << 10) * static_cast<DWORD>(m_nSpeed)) / length;
            m_posDelta.x = static_cast<LONG>(static_cast<DWORD>(m_posDelta.x) + static_cast<DWORD>(vx));
            m_posDelta.y = static_cast<LONG>(static_cast<DWORD>(m_posDelta.y) + static_cast<DWORD>(vy));
        }
        m_posExact.x = static_cast<LONG>(static_cast<DWORD>(m_posExact.x) + static_cast<DWORD>(m_posDelta.x));
        m_posExact.y = static_cast<LONG>(static_cast<DWORD>(m_posExact.y) + static_cast<DWORD>(m_posDelta.y));
        if (m_ptStartDeltaV.x < 0) {
            m_ptStartDeltaV.x = m_ptStartDeltaV.x > static_cast<LONG>(0u - static_cast<DWORD>(m_nDeltaVDec))
                ? 0 : static_cast<LONG>(static_cast<DWORD>(m_ptStartDeltaV.x) + static_cast<DWORD>(m_nDeltaVDec));
        } else if (m_ptStartDeltaV.x > 0) {
            m_ptStartDeltaV.x = m_ptStartDeltaV.x < m_nDeltaVDec
                ? 0 : static_cast<LONG>(static_cast<DWORD>(m_ptStartDeltaV.x) - static_cast<DWORD>(m_nDeltaVDec));
        }
        if (m_ptStartDeltaV.y < 0) {
            m_ptStartDeltaV.y = m_ptStartDeltaV.y > static_cast<LONG>(0u - static_cast<DWORD>(m_nDeltaVDec))
                ? 0 : static_cast<LONG>(static_cast<DWORD>(m_ptStartDeltaV.y) + static_cast<DWORD>(m_nDeltaVDec));
        } else if (m_ptStartDeltaV.y > 0) {
            m_ptStartDeltaV.y = m_ptStartDeltaV.y < m_nDeltaVDec
                ? 0 : static_cast<LONG>(static_cast<DWORD>(m_ptStartDeltaV.y) - static_cast<DWORD>(m_nDeltaVDec));
        }
        m_pos.x = m_posExact.x >> 10;
        m_pos.y = (static_cast<LONG>(static_cast<DWORD>(m_posExact.y) * 3) / 4) >> 10;
    }
}

// 0x555200
void CVisualEffect::SetAllDelete()
{
    CGameObjectArray* objects = g_pBaldurChitin->GetObjectGame()->GetObjectArray();
    POSITION pos = m_activeAnimations.GetHeadPosition();
    while (pos != NULL) {
        LONG id = m_activeAnimations.GetNext(pos);
        CGameObject* animation;
        BYTE rc;
        do {
            rc = objects->GetDeny(id, CGameObjectArray::THREAD_ASYNCH, &animation, INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);
        if (rc == CGameObjectArray::SUCCESS) {
            static_cast<CVEFVidCell*>(animation)->m_bltFlagsExtra |= 0x20;
            objects->ReleaseDeny(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
        }
    }
}

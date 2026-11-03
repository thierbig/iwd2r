#include "CVisualEffect.h"

#include <cmath>

#include "CBaldurChitin.h"
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

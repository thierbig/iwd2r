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

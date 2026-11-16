#include "CGameSound.h"

#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CInfGame.h"
#include "CUtil.h"

// 0x4C8B10
CGameSound::CGameSound(CGameArea* pArea, CAreaFileSoundObject* pSoundObject)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameSound.cpp
    // __LINE__: 107
    UTIL_ASSERT(pArea != NULL && pSoundObject != NULL);

    m_period = 0;
    m_periodCount = 0;
    m_currentSound = 0;
    m_objectType = TYPE_SOUND;
    memcpy(&m_soundObject, pSoundObject, sizeof(CAreaFileSoundObject));

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameSound.cpp
    // __LINE__: 117
    UTIL_ASSERT(m_soundObject.m_soundObjectNum > 0);

    m_bLoopPlaying = FALSE;

    // The active-hours bit field rotates right by one bit (0x4C8BF5: nBits
    // 18 from the byte at 0x84EC0F, nLowBits 1).
    m_timeOfDayActive
        = CUtil::RotateFieldRight(m_soundObject.m_timeOfDayActive, 18, 1);

    if (m_timeOfDayActive == 0x3FFC0) {
        m_timeOfDayActive = 0x3FFF80;
    } else if (m_timeOfDayActive == ~0x3FFC0) {
        m_timeOfDayActive = ~0x3FFF80;
    }

    if (m_soundObject.m_volume > 100) {
        // FIXME: Unused.
        CString sArea;
        pArea->m_resRef.CopyToString(sArea);

        m_soundObject.m_volume = 100;
    }

    if (m_soundObject.m_soundObjectNum == 1
        && (m_soundObject.m_dwFlags & 0x2) != 0) {
        if ((m_soundObject.m_dwFlags & 0x10) == 0 || g_pChitin->cDimm.GetMemoryAmount() != 1) {
            // NOTE: Uninline.
            m_looping.SetResRef(CResRef(m_soundObject.m_soundObject[0]), TRUE, TRUE);
        }

        m_looping.SetChannel(16, reinterpret_cast<DWORD_PTR>(pArea));
        m_looping.SetLoopingFlag(1);
        if ((m_soundObject.m_dwFlags & 0x4) == 0) {
            m_looping.SetRange(m_soundObject.m_range);
        }
        m_looping.m_nPitchVariance = m_soundObject.m_pitchVariance;
        m_looping.SetVolume(m_soundObject.m_volume);
        m_looping.m_nVolumeVariance = m_soundObject.m_volumeVariance;
    } else {
        m_soundObject.m_dwFlags &= ~0x2;
        if (m_soundObject.m_periodVariance > m_soundObject.m_period / 2) {
            m_soundObject.m_periodVariance = m_soundObject.m_period / 2;
        }

        int v1;
        if (2 * m_soundObject.m_periodVariance == -1) {
            v1 = 0;
        } else {
            v1 = rand() % (2 * m_soundObject.m_periodVariance + 1);
        }

        m_periodCount = 0;
        m_currentSound = static_cast<BYTE>(m_soundObject.m_soundObjectNum);
        m_period = (v1 + m_soundObject.m_period - m_soundObject.m_periodVariance) * CTimerWorld::TIMESCALE_MSEC_PER_SEC;
    }

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Add(&m_id,
        this,
        INFINITE);
    if (rc == CGameObjectArray::SUCCESS) {
        CAIObjectType typeAI;
        typeAI.Set(m_typeAI);
        typeAI.SetName(pSoundObject->m_scriptName);
        m_typeAI.Set(typeAI);

        CVariable name;
        name.SetName(GetAIType().GetName());
        name.m_intValue = m_id;
        pArea->GetNamedCreatures()->AddKey(name);

        AddToArea(pArea,
            CPoint(m_soundObject.m_posX, m_soundObject.m_posY),
            0,
            LIST_BACK);
    } else {
        delete this;
    }
}

// 0x4C9010
CGameSound::~CGameSound()
{
    if (m_bLoopPlaying) {
        m_looping.Stop();
    }
}

// 0x4C90A0
BOOLEAN CGameSound::DoAIUpdate(BOOLEAN active, LONG counter)
{
    if (!active) {
        return FALSE;
    }

    if ((counter & m_AISpeed) != (m_id & m_AISpeed)) {
        return FALSE;
    }

    DWORD dwFlags = m_soundObject.m_dwFlags;
    if ((dwFlags & 0x1) == 0) {
        return FALSE;
    }

    // NOTE: Uninline.
    BYTE nHour = g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->GetCurrentHour();

    if ((dwFlags & 0x2) != 0) {
        // A looping sound plays during its hours.
        if (((m_timeOfDayActive >> nHour) & 0x1) != 0) {
            if (m_bLoopPlaying) {
                return FALSE;
            }

            if ((dwFlags & 0x10) != 0 && g_pChitin->cDimm.GetMemoryAmount() == 1) {
                return FALSE;
            }

            if ((m_soundObject.m_dwFlags & 0x4) != 0) {
                m_looping.Play(FALSE);
            } else {
                m_looping.Play(m_pos.x, m_pos.y, 0, FALSE);
            }

            m_bLoopPlaying = TRUE;
        } else if (m_bLoopPlaying) {
            m_looping.Stop();
            m_bLoopPlaying = FALSE;
        }

        return FALSE;
    }

    // NOTE: The file's hours (+0xFA), not the adjusted copy the looping
    // test reads.
    if (((m_soundObject.m_timeOfDayActive >> nHour) & 0x1) == 0) {
        return FALSE;
    }

    if (m_periodCount++ != m_period) {
        return FALSE;
    }

    CSound sound;

    dwFlags = m_soundObject.m_dwFlags;
    if ((dwFlags & 0x8) != 0) {
        // A random one of the sounds.
        if ((dwFlags & 0x10) == 0 || g_pChitin->cDimm.GetMemoryAmount() != 1) {
            INT nCount = m_soundObject.m_soundObjectNum;
            INT nIndex = nCount != 0 ? rand() % nCount : 0;
            sound.SetResRef(CResRef(m_soundObject.m_soundObject[nIndex]), TRUE, TRUE);
        }
    } else {
        // The next one in turn.
        // NOTE: No check for an empty list: the original divides by zero.
        // `inc al` (0x4C937C): the increment wraps as a byte.
        m_currentSound = static_cast<BYTE>(static_cast<BYTE>(m_currentSound + 1) % m_soundObject.m_soundObjectNum);

        if ((dwFlags & 0x10) == 0 || g_pChitin->cDimm.GetMemoryAmount() != 1) {
            sound.SetResRef(CResRef(m_soundObject.m_soundObject[m_currentSound]), TRUE, TRUE);
        }
    }

    sound.SetFireForget(TRUE);
    sound.SetChannel(17, reinterpret_cast<DWORD_PTR>(m_pArea));

    if ((m_soundObject.m_dwFlags & 0x4) == 0) {
        sound.SetRange(m_soundObject.m_range);
    }

    sound.m_nPitchVariance = m_soundObject.m_pitchVariance;
    sound.SetVolume(m_soundObject.m_volume);
    sound.m_nVolumeVariance = m_soundObject.m_volumeVariance;

    if ((m_soundObject.m_dwFlags & 0x10) == 0 || g_pChitin->cDimm.GetMemoryAmount() != 1) {
        if ((m_soundObject.m_dwFlags & 0x4) != 0) {
            sound.Play(FALSE);
        } else {
            sound.Play(m_pos.x, m_pos.y, 0, FALSE);
        }
    }

    INT nVariance = 2 * m_soundObject.m_periodVariance + 1;
    INT nRandom = nVariance != 0 ? rand() % nVariance : 0;
    m_periodCount = 0;
    m_period = (m_soundObject.m_period - m_soundObject.m_periodVariance + nRandom) * CTimerWorld::TIMESCALE_MSEC_PER_SEC;

    return FALSE;
}

// 0x47C830
BOOLEAN CGameSound::CanSaveGame(STRREF& strError)
{
    strError = -1;
    return TRUE;
}

// 0x766660
BOOLEAN CGameSound::CompressTime(DWORD deltaTime)
{
    return TRUE;
}

// 0x4C95B0
void CGameSound::RemoveFromArea()
{
    CGameObject::RemoveFromArea();

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Delete(m_id,
        CGameObjectArray::THREAD_ASYNCH,
        NULL,
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameSound.cpp
        // __LINE__: 346
        UTIL_ASSERT(FALSE);
    }

    delete this;
}

// 0x4C9610
void CGameSound::SetActive(BOOL bActive)
{
    if (bActive) {
        if ((m_soundObject.m_dwFlags & 0x1) == 0) {
            m_soundObject.m_dwFlags |= 0x1;
        }
    } else {
        if ((m_soundObject.m_dwFlags & 0x1) != 0) {
            if (m_bLoopPlaying) {
                m_looping.Stop();
                m_bLoopPlaying = FALSE;
            }

            m_soundObject.m_dwFlags &= ~0x1;
        }
    }
}

// 0x4C9670
BOOL CGameSound::IsActive()
{
    return (m_soundObject.m_dwFlags & 0x1) != 0;
}

// 0x4C9680
void CGameSound::Marshal(CAreaFileSoundObject** pSoundObject)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameSound.cpp
    // __LINE__: 432
    UTIL_ASSERT(pSoundObject != NULL);

    *pSoundObject = new CAreaFileSoundObject;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameSound.cpp
    // __LINE__: 435
    UTIL_ASSERT(*pSoundObject != NULL);

    memcpy(*pSoundObject, &m_soundObject, sizeof(CAreaFileSoundObject));
}

#include "CResBIO.h"

// 0x4029F0
// The parsed-once latch: the first demand that comes back with data marks
// the resource parsed, and further demands return the buffer while more
// than one holder keeps it; a demand for unparsed data reports NULL.
void* CResBIO::Demand()
{
    void* pData = CRes::Demand();
    if (m_bParsed && GetDemands() > 1) {
        return pData;
    }
    if (pData != NULL) {
        m_bParsed = TRUE;
    }
    return m_bParsed ? pData : NULL;
}

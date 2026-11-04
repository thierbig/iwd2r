#ifndef CRESBIO_H_
#define CRESBIO_H_

#include "CRes.h"

// BIO (character biography) resource. Offset comments give the exe's binary
// layout and are informational only. The rest of the class - the ctor
// (inlined in CBaldurChitin::AllocResObject), ~CResBIO (0x423790) and
// Release (0x4022B0, shared with CResCHR::Release) - is not imported with
// this batch.
class CResBIO : public CRes {
public:
    void* Demand(); // 0x4029F0

    /* 0050 */ BOOL m_bParsed;
};

#endif /* CRESBIO_H_ */

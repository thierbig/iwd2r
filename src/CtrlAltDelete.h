#ifndef CTRLALTDELETE_H_
#define CTRLALTDELETE_H_

#include "FileFormat.h"

// The debug console's "CtrlAltDelete:" cheat-command layer. The console's
// dispatcher (0x5838A0, not ported; docs/followups.md tracks it) hands every
// handler its own this in ecx and the parsed command argument as a CString by
// value on the stack. None of the ported handlers reads ecx, so they are free
// functions here; each body states what it does with ecx.

// 0x4EC1C0
void CtrlAltDelete_DisplayText(CString sText);

// 0x4EC250
void CtrlAltDelete_MoveToArea(CString sAreaName);

#endif /* CTRLALTDELETE_H_ */

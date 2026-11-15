---
address: 0x44DC10
name: CGameAIBase::ExecuteAction (Task 84z arms)
exemption: "dispatcher over engine state, as the parent file 44DC10.md records; the arm is fully observable without game data and is driven through ExecuteAction by tests/test_ai.cpp [ai][data] (commit ffb39645): 295 answers ACTION_DONE on all four transfer walks (message shapes asserted per walk) and ACTION_ERROR on the no-target path, and the error sides of the callee are exercised through it. The CHARBASE.CRE sprites and fresh CGameContainers carry all the state the arm reads."
parity_waiver: "asm lints 2026-11-15 T84z: lints - the arm's two RE004s are its calls by tree address-derived names: sub_45C290 at src/CGameAIBase.cpp (ported with its own evidence 45C290.md — the already-waived category the 83i/83l segments record for its 281/305/235/278 call sites) and sub_40CCA0 (the CAIObjectType share resolve, src/CAIObjectType.cpp, evidence 40CCA0.md), the same category. asm - the arm's one new call target 0x4C83F0 is the in-tree TransferInventory this task ports (its own row, 4C83F0.md); the arm adds no named-callee miss, and the row's remaining findings are the standing ones the 83d/83i/83l waiver segments record."
kind: replay
ghidra_version: 12.1.4
---

# 0x44DC10 CGameAIBase::ExecuteAction, Task 84z arm

The dispatcher itself is 44DC10.md; this file records the one inline arm
Task 84z wired (the case-295 body, replacing the TODO stub), the commit,
and the verified decode. Register roles as in 83d/83e/83o: ebx = this,
ebp = pTarget, esi = returnValue, [esp+0x10] = bShare; edi is repurposed
to hold the actee once the action fields have been read.

## The arm (0x451D7F-0x451DE8)

    mov ecx, ebx; call 0x45C290        -> ebp (the giver)
    test ebp, ebp; je 0x451451         ACTION_ERROR; bShare NOT yet set
    push 0; push ebx                    sub_40CCA0(FALSE, this) args
    mov [esp+0x18], 1                   bShare = TRUE (seen through the
                                        two pushed args)
    call 0x41E170                       GetActee2
    mov ecx, eax; call 0x40CCA0        -> edi (the receiver)
    test edi, edi; je 0x44E1DE          plain release test; bShare TRUE
                                        so the tail releases the giver
    push edi; mov ecx, ebp
    call 0x4C83F0                       TransferInventory
    push -1                             INFINITE
    mov esi, eax                        the return is the action result
    xor eax, eax; mov al, [0x851873]    THREAD_ASYNCH byte
    push eax
    mov ecx, edi; call 0x4530B0         GetId ON THE ACTEE
    push eax
    g_pBaldurChitin from 0x8CF6DC
    call 0x40CB10                       GetObjectGame
    call 0x453030                       GetObjectArray
    call 0x599E70                       ReleaseShare
    jmp 0x44E1DE

Points the wiring had to preserve: the bShare store happens between the
giver's NULL test and the actee resolution, so a NULL actee still releases
the giver on the shared tail; the transfer's return value lands in esi
(the action result) before the ReleaseShare; the released id is the
actee's, not the giver's (the giver is released by the tail's own share
bookkeeping). sub_45C290's immunity check is sprite-only, so a container
giver reaches the transfer through the dispatcher — the test uses that.

## Callees

- 0x45C290 sub_45C290 (in tree, src/CGameAIBase.cpp:6151): resolves
  m_curAction.m_acteeID; the OnList immunity drop applies only to
  TYPE_SPRITE results.
- 0x41E170 CAIAction::GetActee2 (in tree).
- 0x40CCA0 CAIObjectType::operator-> / share resolve (in tree).
- 0x4C83F0 CGameObject::TransferInventory (in tree, this task,
  4C83F0.md).
- 0x4530B0 CGameObject::GetId (in tree).
- 0x40CB10 CChitin::GetObjectGame, 0x453030 GetObjectArray,
  0x599E70 CGameObjectArray::ReleaseShare (in tree).

## Test

tests/test_ai.cpp [ai][data] "ai: ExecuteAction 295 TransferInventory
hands the actee the whole inventory" (commit ffb39645) — the no-target
path (resolver miss) answers ACTION_ERROR in the pre-existing case at
the same file, and the new case drives the live arm through all four
walks and both error sides of the callee.

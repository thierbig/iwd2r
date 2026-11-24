---
address: 0x4FECF0
name: CMessageEnterStoreMode::MarshalMessage
exemption: "the message is broadcast so other machines enter the same store screen; single player never sends it (the dispatcher arm is omitted, docs/adr/0002-single-player-only.md), and the body is driven by tests/test_message_dialog_store.cpp [msg]."
kind: read-only
parity_waiver: "asm 2026-11-24 w121-msg2: CAIObjectType::CAIObjectType 0x45B320 is the local temp CAIObjectType the exe builds before Set (the port declares it with the same default ctor, no call token); CString_Destruct/CString_GetBuffer 0x7FCC1A/0x7FCFFE are the CString temporaries from GetName and CopyToString; CUtil::UtilAssert 0x780C00 is the UTIL_ASSERT macro. Every named game call (GetShare, ReleaseShare, CAIObjectType::Set, GetName) is present."
ghidra_version: 12.1.4
---

# 0x4FECF0 CMessageEnterStoreMode::MarshalMessage

Read-only evidence from the MD5-pinned original via
`python3 -I tools/parity/annotate.py 0x4FECF0` and the Ghidra decompile
below; Ghidra names the function `FUN_004fecf0`.

## Class identity

The owning class is CMessageEnterStoreMode (0xD0-byte CMessage subclass):
m_cAIProprietor at +0x0C, m_cAICustomer at +0x48, m_cResStore at +0x84
(src/CMessage.h). Its ctor, empty dtor, GetCommType 0x4536E0
(BROADCAST_FORCED_OTHERS), GetMsgType 0x40A0E0 and GetMsgSubType 0x4F5600
(subtype 23) are already ported; this pair is the message's codec. It is
receive-only in single player: the message is broadcast so the other
machines enter the same store screen.

## Body

`__thiscall void MarshalMessage(BYTE** pData, DWORD* dwSize)`, 1150 bytes,
`ret 8`. Share the target and take its remote pair, or (0, 0xFFFFFFFF) when
the share fails -- unlike the other marshals this arm does not zero the
size. Size: (resref length) + 9 plus, per block, name length + 2 + 0x1A
(0x4FECF0-0x4FEDD0). Then a local CAIObjectType is Set from each member in
turn (0x4FEE00-0x4FF0D0), its GetName gives the length-prefixed name, the
scalar fields and m_nClassMask are written, the block's m_nInstance is
shared for its remote pair, and the five m_SpecialCase bytes close the
block; the closing assert checks the count (0x4FF0D0).
## Wire format

Payload (without the 3-byte SPEC_MSG_HEADER): the target's remote pair
(PLAYER_ID m_remotePlayerID, LONG m_remoteObjectID, or (0, 0xFFFFFFFF) when
the target does not resolve), a length-prefixed store resref, then two
CAIObjectType blocks, each: SHORT name length and name bytes, five bytes
(m_nEnemyAlly, m_nGeneral, m_nRace, m_nSubRace, m_nClass), four bytes
(m_nSpecific, m_nGender, m_nAlignment, m_nAvClass), the DWORD
m_nClassMask, the block's m_nInstance as a remote pair ((0, 0xFFFFFFFF)
when it does not resolve) and the five m_SpecialCase bytes -- 0x1A fixed
bytes after the name. The unmarshal resolves the target and both instance
pairs through CGameRemoteObjectArray::Find and copies the temp
CAIObjectType into m_cAIProprietor / m_cAICustomer with
CAIObjectType::Set. An unresolved target is not fatal (INVALID_INDEX), but
an instance pair that is neither resolved nor (0, 0xFFFFFFFF) rejects the
whole message (FALSE).

## Decompiled (Ghidra)


```c
/* WARNING: Removing unreachable block (ram,0x004ff0c7) */

void FUN_004fecf0(int *param_1,uint *param_2)

{
  int *piVar1;
  char cVar2;
  byte bVar3;
  int *piVar4;
  undefined4 *puVar5;
  byte bVar6;
  int in_ECX;
  uint uVar7;
  uint uVar8;
  undefined4 uVar9;
  int iVar10;
  short sVar11;
  undefined1 *puVar12;
  undefined4 *puVar13;
  undefined4 uVar14;
  undefined *local_68;
  uint uStack_64;
  int local_60;
  uint uStack_5c;
  int iStack_58;
  int iStack_54;
  undefined1 auStack_50 [4];
  int aiStack_4c [2];
  undefined1 uStack_44;
  undefined1 uStack_43;
  undefined1 uStack_42;
  undefined1 uStack_41;
  undefined1 uStack_40;
  undefined4 uStack_3e;
  undefined1 auStack_3a [5];
  undefined1 uStack_35;
  undefined1 uStack_34;
  undefined1 uStack_33;
  undefined1 uStack_32;
  undefined4 uStack_30;
  void *pvStack_c;
  undefined1 *puStack_8;
  uint local_4;
  
  piVar1 = param_1;
  puStack_8 = &LAB_00818598;
  pvStack_c = ExceptionList;
  local_68 = PTR_DAT_008c1758;
  local_4 = 0;
  ExceptionList = &pvStack_c;
  CAIObjectType::CAIObjectType(0,0,0,0,0,0,0,0,0xffffffff,0,0);
  local_4 = CONCAT31(local_4._1_3_,1);
  if ((param_1 == (int *)0x0) || (param_2 == (uint *)0x0)) {
    CUtil::UtilAssert(0x29ac,s_C__Projects_Icewind2_src_Baldur__008abe54,
                      s_pData____NULL____dwSize____NULL_008abe84,0);
  }
  do {
    do {
      cVar2 = CGameObjectArray::GetShare(*(undefined4 *)(in_ECX + 4),0,&local_60,0xffffffff);
    } while (cVar2 == '\x04');
  } while (cVar2 == '\x05');
  if (cVar2 == '\0') {
    param_1 = *(int **)(local_60 + 0x62);
    uVar9 = *(undefined4 *)(local_60 + 0x66);
    CGameObjectArray::ReleaseShare(*(undefined4 *)(in_ECX + 4),0,0xffffffff);
  }
  else {
    uVar9 = 0xffffffff;
    param_1 = (int *)0x0;
  }
  CResRef::CopyToString(&local_68);
  uStack_5c = *(uint *)(local_68 + -8);
  iStack_58 = in_ECX + 0xc;
  *param_2 = (uStack_5c & 0xff) + 9;
  piVar4 = (int *)CAIObjectType::GetName(&uStack_64);
  *param_2 = *param_2 + *(int *)(*piVar4 + -8) + 2;
  CString_Destruct();
  iStack_54 = in_ECX + 0x48;
  *param_2 = *param_2 + 0x1a;
  piVar4 = (int *)CAIObjectType::GetName(&uStack_64);
  *param_2 = *param_2 + *(int *)(*piVar4 + -8) + 2;
  CString_Destruct();
  uVar8 = *param_2;
  *param_2 = uVar8 + 0x1a;
  if (DAT_008abe0c < uVar8 + 0x1a) {
    CUtil::UtilAssert(0x29da,s_C__Projects_Icewind2_src_Baldur__008abe54,
                      s__dwSize_<__STATICBUFFERSIZE_008abe38,0);
  }
  *(int **)*piVar1 = param_1;
  *(undefined4 *)(*piVar1 + 4) = uVar9;
  *(undefined1 *)(*piVar1 + 8) = (undefined1)uStack_5c;
  uVar8 = *(uint *)(local_68 + -8);
  puVar5 = (undefined4 *)CString_GetBuffer(uVar8);
  puVar13 = (undefined4 *)(*piVar1 + 9);
  for (uVar7 = uVar8 >> 2; uVar7 != 0; uVar7 = uVar7 - 1) {
    *puVar13 = *puVar5;
    puVar5 = puVar5 + 1;
    puVar13 = puVar13 + 1;
  }
  for (uVar8 = uVar8 & 3; uVar8 != 0; uVar8 = uVar8 - 1) {
    *(undefined1 *)puVar13 = *(undefined1 *)puVar5;
    puVar5 = (undefined4 *)((int)puVar5 + 1);
    puVar13 = (undefined4 *)((int)puVar13 + 1);
  }
  uVar8 = *(int *)(local_68 + -8) + 9;
  bVar3 = 0;
  do {
    iVar10 = iStack_58;
    if ((bVar3 == 0) || (iVar10 = iStack_54, bVar3 == 1)) {
      CAIObjectType::Set(iVar10);
    }
    piVar4 = (int *)CAIObjectType::GetName(&uStack_5c);
    uVar9 = *(undefined4 *)(*piVar4 + -8);
    CString_Destruct();
    sVar11 = (short)uVar9;
    *(short *)(*piVar1 + uVar8) = sVar11;
    CAIObjectType::GetName(auStack_50);
    uStack_64 = (uint)sVar11;
    local_4._0_1_ = 2;
    puVar5 = (undefined4 *)CString_GetBuffer(uStack_64);
    puVar13 = (undefined4 *)(*piVar1 + uVar8 + 2);
    for (uVar7 = uStack_64 >> 2; uVar7 != 0; uVar7 = uVar7 - 1) {
      *puVar13 = *puVar5;
      puVar5 = puVar5 + 1;
      puVar13 = puVar13 + 1;
    }
    for (uVar7 = uStack_64 & 3; uVar7 != 0; uVar7 = uVar7 - 1) {
      *(undefined1 *)puVar13 = *(undefined1 *)puVar5;
      puVar5 = (undefined4 *)((int)puVar5 + 1);
      puVar13 = (undefined4 *)((int)puVar13 + 1);
    }
    local_4 = CONCAT31(local_4._1_3_,1);
    CString_Destruct();
    iVar10 = uVar8 + 2 + uStack_64;
    *(undefined1 *)(*piVar1 + iVar10) = uStack_44;
    *(undefined1 *)(*piVar1 + 1 + iVar10) = uStack_43;
    *(undefined1 *)(*piVar1 + iVar10 + 2) = uStack_42;
    *(undefined1 *)(*piVar1 + iVar10 + 3) = uStack_41;
    *(undefined1 *)(*piVar1 + iVar10 + 4) = uStack_40;
    *(undefined1 *)(*piVar1 + iVar10 + 5) = uStack_35;
    *(undefined1 *)(*piVar1 + iVar10 + 6) = uStack_34;
    *(undefined1 *)(*piVar1 + iVar10 + 7) = uStack_33;
    *(undefined1 *)(*piVar1 + iVar10 + 8) = uStack_32;
    *(undefined4 *)(*piVar1 + iVar10 + 9) = uStack_30;
    do {
      do {
        cVar2 = CGameObjectArray::GetShare(uStack_3e,0,aiStack_4c,0xffffffff);
      } while (cVar2 == '\x04');
    } while (cVar2 == '\x05');
    if (cVar2 == '\0') {
      uVar9 = *(undefined4 *)(aiStack_4c[0] + 0x66);
      uVar14 = *(undefined4 *)(aiStack_4c[0] + 0x62);
      CGameObjectArray::ReleaseShare(uStack_3e,0,0xffffffff);
    }
    else {
      uVar9 = 0xffffffff;
      uVar14 = 0;
    }
    *(undefined4 *)(*piVar1 + iVar10 + 0xd) = uVar14;
    *(undefined4 *)(*piVar1 + iVar10 + 0x11) = uVar9;
    bVar6 = 0;
    uVar8 = iVar10 + 0x15;
    puVar12 = auStack_3a;
    do {
      uVar8 = uVar8 + 1;
      *(undefined1 *)(*piVar1 + -1 + uVar8) = *puVar12;
      bVar6 = bVar6 + 1;
      puVar12 = puVar12 + 1;
    } while (bVar6 < 5);
    bVar3 = bVar3 + 1;
  } while (bVar3 < 2);
  if (uVar8 != *param_2) {
    CUtil::UtilAssert(0x2a53,s_C__Projects_Icewind2_src_Baldur__008abe54,s_cnt_____dwSize_008a89ec,0
                     );
  }
  local_4 = local_4 & 0xffffff00;
  CString_Destruct();
  local_4 = 0xffffffff;
  CString_Destruct();
  ExceptionList = pvStack_c;
  return;
}
```

## Callers


- 0x84D124 (not in a function)

## Callees


- 0x40AE80 CAIObjectType::CAIObjectType
- 0x40B6E0 CAIObjectType::Set
- 0x40CAC0 CAIObjectType::GetName
- 0x599A50 CGameObjectArray::GetShare
- 0x599E70 CGameObjectArray::ReleaseShare
- 0x780C00 CUtil::UtilAssert
- 0x78A9F0 CResRef::CopyToString
- 0x7FCC1A CString_Destruct
- 0x7FCFFE CString_GetBuffer

## Test

`tests/test_message_dialog_store.cpp`: the two-block body (remote pair,
store resref, every CAIObjectType field, instance pair and special-case
bytes), the not-zeroed failure arm, the round trip and the rejected
unresolved instance pair.

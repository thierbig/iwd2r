---
address: 0x44DC10
name: CGameAIBase::ExecuteAction (290 and 291 arms)
exemption: "dispatcher over engine state, as the parent file 44DC10.md records; the two arms this change rewired are driven through ExecuteAction by tests/test_ai.cpp [ai][data][ar3000] 'ExecuteAction 290 and 291 fire the target sprite's spell hit' (31 assertions): 290 on the sprite branch (type 0xD2 -> the plain 0xD3 decode, action dest, target id 0) and 291 on the success branch (type 1 -> the spell-hit decode, actee pos and id). Each arm's return and its queued CMessageSpellHitEffectSecondaryProjectile fields are pinned; the projectile Fire's object/area effects are removed after each arm. The no-target paths remain covered by the existing 290/291/292 case; the 290 container fallthrough (0x482040) is unchanged and keeps its own evidence."
kind: read-only
parity_waiver: "none needed: the two replacement calls are direct calls to the new in-tree CGameSprite::SpellHitEffect 0x7624F0 (under its own row 7624F0.md) and keep all existing in-tree callees (GetTargetShareType, GetDest, GetSpecifics/2, GetActee2, sub_40CB20, GetArea, GetId, ReleaseShare). The TODO comments they replace were the only source of the previous not-ported note; no new RE004 or asm-callee finding is introduced inside the arms."
ghidra_version: 12.1.4
---

# 0x44DC10 CGameAIBase::ExecuteAction, arms 290 and 291

The dispatcher itself is 44DC10.md; this file records the two arms whose TODO
stubs were replaced by the 0x7624F0 call, and the verified argument order and
exit tails. The parent evidence records the dispatcher, register roles and the
exit labels; the 83e arms file records the rest of the 290/291 structure.

## Changed arms

- 290 SpellHitEffectPoint (0x45176D-0x451805): GetTargetShareType 0x45BDD0
  with the TYPE_SPRITE byte at 0x84C507; on success bShare, GetSpecifics2,
  GetSpecifics, push 0, GetDest 0x452B70, push the target, ECX = the target,
  call 0x7624F0. The result goes to ESI and jumps to 0x44E1DE (the plain
  release tail). The miss falls to the container probe (0x84C502 = 0x11,
  TYPE_CONTAINER) and the existing 0x482040 branch; a double miss jumps to
  0x451451 (ACTION_ERROR). The replaced stub previously left the entry
  default ACTION_ERROR, so the new call also fixes the success return.
- 291 SpellHitEffectSprite (0x451805-0x45189C): GetTargetShareType
  (TYPE_SPRITE); a null jumps 0x451451. GetActee2 0x41E170 + sub_40CB20
  0x40CB20(this, TYPE_SPRITE, 0) acquires the actee share; a null or an
  area-less actee jumps 0x451451 without releasing that share (the pre-existing
  NOTE). Success: GetSpecifics2, GetSpecifics, actee GetId 0x4530B0, actee
  GetPos through vtable slot 0x1C, push the target, ECX = the target,
  call 0x7624F0; its result goes to ESI, then the shared 0x451DC9 actee
  ReleaseShare sequence runs and the arm jumps 0x44E1DE. The new call
  replaces the ACTION_ERROR default, so the success branch now returns the
  helper's ACTION_DONE (0x8485C4).

## Annotated disassembly (original 0x45176D-0x45189C)

Pinned local vanilla, MD5 200f038bec3ed50a964d68b3f0f1f0e7 (SHA-256 ba56349a30240058fa9b110dca5b92c32715492356c80017df9648c838a9db8e), `tools/parity/annotate.py 0x45176D --end 0x4518A0`:

```
0x45176d:	xor	edx, edx
0x45176f:	mov	dl, byte ptr [0x84c507]  ; "10AQP"
0x451775:	mov	ecx, ebx
0x451777:	push	edx
0x451778:	call	0x45bdd0  ; CGameAIBase::GetTargetShareType
0x45177d:	mov	ebp, eax
0x45177f:	test	ebp, ebp
0x451781:	je	0x4517b4
0x451783:	mov	ecx, edi
0x451785:	mov	dword ptr [esp + 0x10], 1
0x45178d:	call	0x41e190  ; CAIAction::GetSpecifics2
0x451792:	push	eax
0x451793:	mov	ecx, edi
0x451795:	call	0x41e180  ; CAIAction::GetSpecifics
0x45179a:	push	eax
0x45179b:	push	0
0x45179d:	mov	ecx, edi
0x45179f:	call	0x452b70  ; CAIAction::GetDest
0x4517a4:	push	eax
0x4517a5:	push	ebp
0x4517a6:	mov	ecx, ebp
0x4517a8:	call	0x7624f0  ; CGameAIBase_SpellHitEffect
0x4517ad:	mov	esi, eax
0x4517af:	jmp	0x44e1de  ; ?
0x4517b4:	xor	eax, eax
0x4517b6:	mov	al, byte ptr [0x84c502]
0x4517bb:	mov	ecx, ebx
0x4517bd:	push	eax
0x4517be:	call	0x45bdd0  ; CGameAIBase::GetTargetShareType
0x4517c3:	mov	ebp, eax
0x4517c5:	test	ebp, ebp
0x4517c7:	je	0x451451  ; returnValue
0x4517cd:	mov	ecx, edi
0x4517cf:	mov	dword ptr [esp + 0x10], 1
0x4517d7:	call	0x41e190  ; CAIAction::GetSpecifics2
0x4517dc:	push	eax
0x4517dd:	mov	ecx, edi
0x4517df:	call	0x41e180  ; CAIAction::GetSpecifics
0x4517e4:	push	eax
0x4517e5:	mov	ecx, ebp
0x4517e7:	call	0x4530b0  ; CGameObject::GetId
0x4517ec:	mov	edx, dword ptr [ebp]
0x4517ef:	push	eax
0x4517f0:	mov	ecx, ebp
0x4517f2:	call	dword ptr [edx + 0x1c]
0x4517f5:	push	eax
0x4517f6:	push	ebp
0x4517f7:	mov	ecx, ebp
0x4517f9:	call	0x482040  ; CGameContainer::SpellHitEffect
0x4517fe:	mov	esi, eax
0x451800:	jmp	0x44e1de  ; ?
0x451805:	xor	eax, eax
0x451807:	mov	al, byte ptr [0x84c507]  ; "10AQP"
0x45180c:	mov	ecx, ebx
0x45180e:	push	eax
0x45180f:	call	0x45bdd0  ; CGameAIBase::GetTargetShareType
0x451814:	mov	ebp, eax
0x451816:	test	ebp, ebp
0x451818:	je	0x451451  ; returnValue
0x45181e:	xor	ecx, ecx
0x451820:	mov	cl, byte ptr [0x84c507]  ; "10AQP"
0x451826:	push	0
0x451828:	mov	dword ptr [esp + 0x14], 1
0x451830:	push	ecx
0x451831:	push	ebx
0x451832:	mov	ecx, edi
0x451834:	call	0x41e170  ; CAIAction::GetActee2
0x451839:	mov	ecx, eax
0x45183b:	call	0x40cb20  ; CAIObjectType::sub_40CB20
0x451840:	test	eax, eax
0x451842:	mov	dword ptr [esp + 0x14], eax
0x451846:	je	0x451451  ; returnValue
0x45184c:	mov	ecx, eax
0x45184e:	call	0x40cb00  ; CGameObject::GetArea
0x451853:	test	eax, eax
0x451855:	je	0x451451  ; returnValue
0x45185b:	mov	ecx, edi
0x45185d:	call	0x41e190  ; CAIAction::GetSpecifics2
0x451862:	push	eax
0x451863:	mov	ecx, edi
0x451865:	call	0x41e180  ; CAIAction::GetSpecifics
0x45186a:	mov	esi, dword ptr [esp + 0x18]
0x45186e:	push	eax
0x45186f:	mov	ecx, esi
0x451871:	call	0x4530b0  ; CGameObject::GetId
0x451876:	mov	edx, dword ptr [esi]
0x451878:	push	eax
0x451879:	mov	ecx, esi
0x45187b:	call	dword ptr [edx + 0x1c]
0x45187e:	push	eax
0x45187f:	push	ebp
0x451880:	mov	ecx, ebp
0x451882:	call	0x7624f0  ; CGameAIBase_SpellHitEffect
0x451887:	mov	ecx, dword ptr [esp + 0x14]
0x45188b:	push	-1
0x45188d:	mov	esi, eax
0x45188f:	xor	eax, eax
0x451891:	mov	al, byte ptr [0x851873]
0x451896:	push	eax
0x451897:	jmp	0x451dc9  ; ?
```

## Notes

- The call is a direct `call 0x7624F0`, not a virtual slot: CGameSprite::SpellHitEffect is a non-virtual method, so no vtable entry is involved.
- The fifth argument pushed at 0x45179B is the literal 0 (the target id for the point form); the sprite form pushes the actee's GetId at 0x451878.
- Every argument is taken from the current in-place expressions (GetDest, GetSpecifics, GetSpecifics2, the actee's GetPos/GetId); no other action arm or dispatcher body was changed.

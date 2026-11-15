---
address: 0x7ACCB0
name: CVidImage::BuildBlendTable
exemption: "no oracle case: the callers that reach it standalone (the 24/32-bpp tile blitters FUN_007CE3D0 and FUN_007CE930 under 0x7B1F70) are not ported and no capture reaches them; the table contents are deterministic and pinned in-tree by tests/test_vid.cpp vid_image_blend_table against the formula (m * (64 - 2i) + k * 2i) >> 6."
kind: read-only
ghidra_version: 12.1.4
---

# 0x7ACCB0 CVidImage::BuildBlendTable

Standalone helper of the video layer (CVidImage.cpp in the original),
`__cdecl` `int BuildBlendTable(void)`, 138 bytes (0x7ACCB0-0x7ACD39).
Extracted from the constructor body that the tree already carried
inline.

## Body

operator_new(0x20000) stored at the blend-table global 0x9060F0; on a
NULL result the function returns with eax 0. Otherwise three nested
loops fill the table — outer i (the weight) 0..31 with step counter
64, 62, ... 2; middle m 0..63 at byte stride 0x20 accumulating
nRowStart by the step; inner k 0..63 at byte stride 0x800 adding
2 * i per element — storing `(nRowStart + 2 * i * k) >> 6` = 
`(m * (64 - 2i) + k * 2i) >> 6` as a byte at `k * 0x800 + m * 0x20 + i`.
Returns 1.

## Callers

Not in tree: 0x7CE3D0 and 0x7CE930 (the 24- and 32-bpp tile blitters
under dispatcher 0x7B1F70) call it when they find the table pointer
NULL; those blitters are unported. The tree reaches the body through
CVidImage::CVidImage (0x7ACA60), which inlines its own identical copy
under the m_nBlendTableRefs (0x9068F8) count; the tree now factors the
single copy into this function and the constructor calls it.

## Test

tests/test_vid.cpp [vid] vid_image_blend_table drives the helper
directly (returns TRUE, entry (0,0,0) is 0, entry (63,63,31) is 63,
restoring the previous table pointer afterwards) and then covers the
constructor path with the existing entry-formula checks.

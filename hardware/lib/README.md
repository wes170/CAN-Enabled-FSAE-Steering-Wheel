# `hardware/lib/` — shared Altium library

`FSAE-Common.SchLib` and `FSAE-Common.PcbLib` live here, shared by the wheel, dash, and any future
board (including the planned servo power conditioning board). Build them per
`memory/wheel-pcb-altium-instructions.md` §2.

## The one rule

**Verify every footprint against the datasheet mechanical drawing before it is used.** Library errors
are the single most common cause of a respin, and they are invisible in every check downstream:
ERC passes, DRC passes, the 3D view looks right, and the part does not fit the board that arrives.

Print the datasheet dimension page and tick it off. A footprint imported from SnapEDA or Ultra
Librarian is a starting point, not a verified part — run Altium's IPC compliance check on it.

## Per-part parameters to fill in

Every part carries `LCSC = Cxxxxxx` and `JLC-Rotation`. The rotation field starts empty and gets
back-annotated after the first JLCPCB order preview, where rotations routinely mismatch Altium
footprints (lesson L3). Filling it in is what stops the next order repeating the same corrections.

Verified so far: STM32G474RET6 `C521608`, WS2812B-2020 `C965555`, LS013B7DH05 `C17500193`,
USB-C `C165948`.

## Footprints needing extra care

| Part | Watch out for |
|---|---|
| Sharp LS013B7DH05 FPC | 10-pin 0.5 mm **bottom-contact** ZIF — a top-contact part silently mirrors the pinout |
| Riverdi EVE4 module | Pin table differs between module revisions — transcribe from the revision actually purchased (L6) |
| PEC09 (right angle) | Body sits flat, shaft exits **parallel** to the board. Check shaft length against faceplate depth |
| PEC11H | Bushing hole Ø9.5 mm + anti-rotation slot — put both on the faceplate drawing too |
| STM32G474RET6 | LQFP-64, 0.5 mm pitch — verify pad geometry (~0.28 × 1.5 mm class) |
| WS2812B-2020 | 2.2 × 2.0 mm, pin-1 dot orientation; 24 of them, so one rotation error repeats 24 times |
| TC2030 | Not a component — copper pads plus three locating holes |

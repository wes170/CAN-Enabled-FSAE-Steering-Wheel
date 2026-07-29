# From Nothing to Ordered PCBs: A Plain-English Build Process Guide

> **What this file is.** A narrative walkthrough of *how* this project goes from the current set of
> memory files to working steering-wheel and dash assemblies in the car. It is the story, not the
> reference. When you need an exact menu path, rule value, or pin number, this file will point you to
> the precise document that has it — `wheel-pcb-altium-instructions.md`, `dash-pcb-altium-instructions.md`,
> `sim-variant-instructions.md`, `engineering-rigor.md`, and `PROJECT-LOG.md`. This file does not
> introduce or change any technical fact found in those documents.
>
> **What this file is not.** It is not a claim that the project is ready to order. As of this writing
> there are nine tracked open assumptions, A1 through A9 in `PROJECT-LOG.md` §1 (A6 must be closed
> before dash layout — see §2 below). The MCU pin maps themselves are now verified; what's still open
> is the `[OPEN]` items in the two schematic-definition files. Read the "What's still open" section
> near the end before you touch a distributor cart.

## Who this is for

You've used Altium a little — you can place a part, draw a wire, maybe you've run a DRC (Design Rule
Check, the tool that checks your copper against manufacturing and electrical rules like minimum trace
width or clearance) once. You have never taken a board all the way from a blank project to something
soldered, powered up, and bolted into a car. This guide is the spine you follow in order; the precise
instruction files are where you dip in for the actual values.

## The one-sentence version of the whole process

Buy the slow part first, build a shared parts library, draw the schematic and check it electrically,
lay out the copper and check it physically, get a second human to look at all of it, order the boards
and the assembly, then power each one up in small deliberate steps before it goes anywhere near the
car. Every step in that sentence exists because skipping it has already cost this project real
mistakes — eleven of them, found so far (lesson L19 in `engineering-rigor.md`), several of which
slipped past the person who wrote them.

## 1. Buy the long-lead part before you touch Altium at all

The very first action in `hardware/README.md`'s order of work is not schematic capture — it's
procurement. The Riverdi dash display (`RVT50HQBNWN00`) is single-source (one supplier makes it, so
there's no second vendor to fall back on if stock dries up) with thin distributor stock, and it is
the part most likely to stall the whole build if you wait to order it. Nothing about the PCB layout
depends on the display having already arrived — you can draw schematics and lay out copper against
its datasheet pin table long before the physical part shows up. So the display order goes out on day
one, in parallel with the earliest schematic work, not after.

This is a general pattern worth internalizing: **order things in parallel with the work they don't
block, not in the order you'll physically assemble them.** The display doesn't block schematic
capture; schematic capture doesn't block ordering the display. Do both at once.

The second thing to buy immediately, and for the same reason — it's cheap, slow to matter if you
forget it, and blocks nothing else — is a roughly $30 USB-CAN sniffer (a small dongle that lets a
laptop send and listen on a CAN bus, the vehicle's shared serial network, without needing a full
ECU). More on why in the next section.

## 2. Close the cheap assumptions before you spend on board money

This project tracks its open technical assumptions by number (A1 through A9 currently, in
`PROJECT-LOG.md` §1, cross-referenced from `system-architecture-and-can.md` §5) so that nobody has to
remember which parts of the design are still "we think" rather than "we verified." Two of them, A1
and A2, are called out specifically as **closable for the price of the sniffer, with no PCB spend at
all** — and they are also the two most likely to force a full board respin if they turn out wrong.

A1 is whether Haltech's NSP (the ECU's configuration software) actually detects an emulated keypad
device at CAN node address `0x15` running at 1 Mbit/s the way the wheel firmware assumes. A2 is
whether the IO12 "Box B" CAN message ID set — needed for the dash to re-broadcast its 8 DAQ channels
onto the bus — actually exists and is documented; Box A is already verified, but Box B isn't.

Both of these are protocol questions, not layout questions. You can answer "does the ECU accept this
message shape" by plugging a $30 sniffer into a bench CAN bus and either sniffing a real Haltech
keypad, or emailing Haltech support for the write protocol (they hand it to device owners on
request), long before a single trace is routed. If either answer comes back "no, the format is
different," that's a firmware and message-format problem, cheap to fix on a laptop. If you'd instead
discovered it after boards were fabricated and assembled, it's the same discovery made against $ that
already left the building. That asymmetry — cheap to test now, expensive to discover later — is
exactly why the project log puts these two assumptions ahead of every other procurement item.

## 3. Set up the Altium project

With the long-lead parts on order and the cheap protocol risk being retired on the bench in parallel,
schematic work can start. Both boards' instruction files describe the same project-creation steps
(the dash file explicitly says "follow the wheel doc for every generic step" and only documents what
differs), so walk through them once and apply the pattern to both boards.

You create the project with **File ▸ New ▸ Project**, using the *Default* template, and name it
`FSAE-WHEEL.PrjPcb` (or `FSAE-DASH.PrjPcb`), saved under `hardware/wheel/` or `hardware/dash/` in this
repo. Version control (Git) gets enabled in the project options immediately, and the empty project is
committed right away — not after there's something in it. This matters because standing rule 8 in
`engineering-rigor.md` is blunt about it: "a design that isn't in git didn't happen."

You then add the empty schematic sheet documents up front — for the wheel that's `wheel-power.SchDoc`,
`wheel-mcu.SchDoc`, `wheel-can-io.SchDoc`, `wheel-hmi.SchDoc`, `wheel-leds.SchDoc`,
`wheel-display.SchDoc`, a top sheet `wheel-top.SchDoc`, and the `FSAE-WHEEL.PcbDoc` board file. The
dash mirrors this with its own set of sheet names.

One setting here is doing real work, not just paperwork: **Project ▸ Project Options ▸ Error
Reporting** needs *Nets with only one pin*, *Floating power object*, and *Duplicate part designators*
all set to **Fatal Error**. These are the settings that make the schematic-level electrical checker —
ERC, Electrical Rule Check, the automated pass that looks for things like an unconnected pin or a
power net with no source — actually stop you instead of quietly warning you. This one setting is
itself gate G1 in miniature: it's what makes "zero errors" in the ERC report a meaningful claim later.

Last, set the project parameters — `Rev = B`, `Board = FSAE-WHEEL` (or `FSAE-DASH`) — and wire them
into the title block template so every printed sheet self-identifies its revision. This sounds
cosmetic; it isn't. A gerber file (the industry-standard set of files — one per copper/silkscreen/
solder-mask layer plus drill data — that a fab house actually manufactures from) with no revision
marking is a gerber file nobody can later prove came from the reviewed design.

## 4. Build the shared parts library before you draw a single schematic symbol

Both boards pull from one shared library, `FSAE-Common.SchLib` (schematic symbols) and
`FSAE-Common.PcbLib` (physical footprints), created once and kept in `hardware/lib/`. This is worth
doing carefully and up front rather than ad hoc as parts come up in the schematic, because the
instructions call library errors "the #1 cause of respins."

The sourcing order matters and is meant to be followed in sequence: first try Altium's own
**Manufacturer Part Search** panel and place the part directly (then right-click ▸ Add to library to
keep a local copy); if that doesn't have it, pull a symbol/footprint from SnapEDA or Ultra Librarian
and run Altium's **IPC-compliance check** (Reports ▸ Footprint comparison) against it before trusting
it; if neither has what you need, build the footprint yourself with the **IPC Footprint Wizard** using
the datasheet's nominal dimensions at density level **N**. In every case — and this is the part that's
easy to skip when you're in a hurry — you verify the footprint against the part's actual datasheet
mechanical drawing. A footprint that "looks right" but wasn't checked against the drawing is exactly
the kind of error that only shows up after the board comes back from the fab.

The instructions list specific parts that get a mandatory footprint check with the datasheet page
printed and ticked off: the STM32G474RET6 microcontroller (LQFP-64 package, 0.5 mm pin pitch — verify
the 0.28×1.5 mm pad class), the TJA1051 CAN transceiver (SO-8 package), the PEC09 right-angle
encoders (through-hole — check the body sits flat and the shaft exits parallel to the board at the
intended edge, and check shaft length 15/20/25 mm against the actual faceplate depth before
committing to one), the PEC11H encoders (bushing hole diameter and anti-rotation slot, checked against
the faceplate drawing too, not just the part datasheet), the WS2812B-2020 addressable LEDs (2.2×2.0
mm, watch the pin-1 dot orientation), the wheel display's 10-pin FPC connector — now the JDI
LPM013M126A colour memory-in-pixel part, with the Sharp LS013B7DH05 mono display kept as a
pin-compatible fallback (flexible printed circuit connector — a bottom-contact ZIF type; picking a
top-contact part by mistake flips the contacts and the display won't work), the USB-C connector, the
tactile switches, the JST-GH connectors, the Tag-Connect TC2030 programming footprint (no actual part
sits here — it's copper pads plus three locating holes for a pogo-pin programming clip), and the buck
regulator (LMR36015) and the LDO (AP2112K) with their inductors and passives. The wheel has one buck
(the LMR36015) plus one LDO (the AP2112K) — not two bucks; an earlier candidate, the AP63205, was
considered and rejected. The LMR36015's footprint check matters more than most: it's a VQFN-HR-12
"HotRod" package, which is PCBA-only (not hand-solderable), so a wrong footprint isn't something you
can fix with an iron later the way you could with a TSOT-26 part.

For every part that JLCPCB will assemble onto the board (as opposed to something you hand-solder
later), you also add two parameters to the library part: `LCSC` (their catalog part number, format
`Cxxxxxx`) and `JLC-Rotation` (filled in later, after the manufacturing-output check in step 8). Two
numbers are already known and verified: the MCU is `C521608` and the LEDs are `C965555`.

## 5. Draw the schematic, sheet by sheet — and don't drift the net names

With the library in place, schematic capture proceeds sheet by sheet, and net naming is meant to
match the fixed design-facts tables at the top of each instruction file *exactly* — no shortening, no
renaming for convenience. Ports connect sheets to each other; the only net allowed to work as a
"hidden" implicit connection across the whole design is `GND`.

This is where the "precise reference doc, not this file" rule really applies: the wheel doc walks
through `wheel-power.SchDoc` (the 12V entry stage — polyfuse, reverse-polarity protection FET, buck
converter, and LDO), `wheel-mcu.SchDoc`, `wheel-can-io.SchDoc`, `wheel-hmi.SchDoc` (human-machine interface —
the encoders and buttons), `wheel-leds.SchDoc`, and `wheel-display.SchDoc`. The dash doc names its own
sheet set and tells you explicitly which wheel sheets to copy-and-edit versus draw fresh. Rather than
restate every value here (that's what those files are for), the process point is this: **draw sheets
in the order given**, because later sheets sometimes say "copy this from the earlier sheet, don't
redraw it" — the wheel power stage and the dash power stage share almost the same topology, and the
instructions say so explicitly so you don't reinvent it slightly differently by accident.

A few things worth flagging here because they're the kind of mistake the process is specifically
built to catch:

The **MCU pin maps have been verified** against STM32G474 datasheet Table 13 and live in
`wheel-schematic-complete.md` §9 and `dash-schematic-complete.md` §8 — capture from those. The older
tables in the two Altium instruction files are marked superseded; those files remain authoritative for
PROCESS (project setup, libraries, layout, DRC, outputs) but not for what to draw. This connects
directly to a lesson the project already learned the hard way (lesson L16, discussed below) — a pin
map that looks internally consistent isn't the same thing as a pin map that was checked against the
actual chip, which is exactly why the verified tables now live in the schematic-definition files
rather than being re-derived from memory each time.

The **BOOT0 pin gets no pulldown and no strap**, on both boards, because on this specific package
`PB8`, which is normally a safe place to add a pulldown resistor, is *also* `FDCAN1_RX` — the CAN
receive line. A pulldown there would fight against the CAN transceiver's own output. Instead, BOOT0
behavior is controlled by an option bit (`nBOOT_SEL = 1`) set at first flash and re-checked after any
mass erase. Miss this and the failure mode is nasty and intermittent: an idle CAN bus sits
"recessive," which reads as high, and a board with a strap would boot into its system bootloader
every single time it's powered up with a live bus attached — while working perfectly on a bench with
the bus unplugged. That gap between bench behavior and installed behavior is exactly why this note
exists in bold in the source file.

The wheel display's (now the JDI LPM013M126A colour part, with the Sharp LS013B7DH05 mono part kept as
a pin-compatible fallback) **EXTMODE pin must not float.** It selects how the display's internal
charge-balancing scheme (COM inversion, which prevents a DC bias building up across the liquid crystal
and permanently damaging it) is triggered — by a hardware pin toggling, or by a software command. Leave
it floating and that behavior is undefined, risking permanent image sticking. The fix is a 0 Ω link
strapping EXTMODE to +3V3, with a DNP 0 Ω pulldown alternative on the footprint.

Once every sheet is wired and the sheet symbols connect on the top sheet, you run **Project ▸
Validate**, which executes the ERC you configured back in step 3. The bar is zero errors and zero
*unsuppressed* warnings — any warning you decide to suppress needs a written justification comment
right there in the schematic. That's gate **G1**, and what it's actually for is explained in the gates
section below.

## 6. Lay out the copper

Layout starts with the physical board outline: import the chassis or panel mechanical drawing (a DXF
file) onto a mechanical layer in Altium, draw the board outline from it, and place mounting holes.
Both boards then use a 4-layer stackup (the sequence of copper and insulating layers that makes up the
PCB) on the **JLC7628** preset, with layer 2 reserved entirely as an unbroken ground plane — no
routing is allowed on that layer, full stop. An unbroken ground plane gives every signal on the other
layers a clean, low-impedance return path directly underneath it, which matters both for noise and
for controlling impedance (a trace's effective electrical "characteristic," which has to be held to a
target value on high-speed signal pairs so a receiver sees a clean edge instead of reflections).

Placement follows physical logic, not layout convenience: on the wheel, protection parts sit closest
to the connector, in the exact physical order the energy would travel — polyfuse, then the
reverse-polarity FET, then the TVS clamp diode, then the buck regulator — because "energy must be
clamped before it travels" is the standing rule (rule 4 in `engineering-rigor.md`: everything that
leaves the board gets conditioned at the connector). The switching regulator's "hot loop" (the small,
high-current loop formed by the switch node, its input capacitors, and the inductor — the loop whose
area directly sets how much electromagnetic noise the converter radiates) gets kept physically tight
and separated from sensitive parts, with an explicit loop-area target and a keep-out distance from the
display's connector and the encoders.

Routing order also matters and is specified: power trunks first, then the CAN and USB differential
pairs (two traces routed as a matched pair so a receiver reads the *difference* between them, immune
to common noise), then the display and encoder buses, then the LED daisy-chain, then cleanup. Every
signal is meant to be referenced to the layer-2 ground plane; if a trace has to jump to a different
copper layer, a ground via (a plated hole connecting layers) goes in within 2 mm of the jump so the
return current has a short path back. On the dash, the analog front end for the DAQ channels is kept
on the opposite side of the board from the switching regulators and the servo PWM traces, specifically
because switching edges are electrical noise and the DAQ channels are reading millivolt-resolution
sensor signals — physical distance is the cheapest noise mitigation there is.

Two small manufacturing choices worth knowing the reason for: **teardrops** (a small filleted widening
of copper where a trace meets a pad or via, which reduces stress concentration and makes the joint
less likely to crack or lift during drilling and thermal cycling) are turned on project-wide. And
where a trace needs a controlled impedance — the USB differential pair specifically — the exact trace
width and spacing get pulled from JLCPCB's own impedance calculator for the JLC7628 stackup rather
than guessed, because impedance is a function of the specific fab's copper thickness and dielectric,
not a universal number.

Run **Tools ▸ Design Rule Check** and drive it to zero errors — that's the DRC referenced throughout,
the layout equivalent of the ERC you ran on the schematic.

## 7. Variants: one PCB, two (or more) ways to build it

Both boards use Altium's **assembly variant** feature — a way to describe more than one populated
configuration from a single PCB layout, where each variant lists which parts are fitted and which are
**DNP** (Do Not Populate — the footprint exists on the board for future flexibility, but no part goes
there for this build). The wheel has a `CAR` variant (the base design, everything fitted, the USB
power-input link left unpopulated so the board never backfeeds a PC) and a `SIM` variant for the
sim-rig build, detailed fully in `sim-variant-instructions.md`.

The sim variant is worth understanding conceptually because it's a good example of *why* variants beat
a second board design: the sim rig differs from the car build only in where it gets power (USB instead
of vehicle 12V), what it talks to (a PC over USB HID instead of the car's CAN bus), and how paddle
input reaches the microcontroller — three differences that are all either "populate a different set of
parts" or "run different firmware," never a change to the copper itself. A second layout would double
the design and review effort and fork the parts list for no benefit; a variant reuses one reviewed
board. Concretely, the CAR build fits the whole 12V input stage (polyfuse, protection FET, buck
converter) and leaves a 0-ohm USB-power link unpopulated; the SIM build does the opposite — leaves the
12V stage entirely unpopulated and fits that USB link instead, so the board runs directly from a
laptop's USB port. This is set up under **Project ▸ Variants**, and the sim instructions give the
exact per-part fitted/not-fitted list.

## 8. Generate manufacturing outputs

Once layout passes DRC, you generate what actually goes to the fab and assembly house: **Gerber
Files** (the layer-by-layer manufacturing format) and **NC Drill** data via File ▸ Fabrication
Outputs, or — preferred, because it's a single reproducible, commit-able job file — an OutJob that
captures the same thing. For assembly you generate a **BOM** (Bill of Materials — the parts list with
designator, value, footprint, and LCSC catalog number) and a pick-and-place file, sometimes called
**CPL** (Component Placement List — the X/Y coordinates and rotation for every part, which is what lets
a pick-and-place machine place parts automatically instead of by hand).

Before the assembly order goes out, there's a manual step that's easy to skip and has already bitten
this project's category of mistakes before: JLCPCB's own preview of your pick-and-place data routinely
shows parts rotated differently than Altium expects, so you have to walk through their preview against
your own 3D view and fix every mismatched rotation, then record the correction back into the
`JLC-Rotation` library parameter so the *next* order for that part is already correct.

The recommended order quantity is **at least 3 assembled boards plus 2 bare boards** — bare spares
exist because bring-up testing sometimes destroys a board, and having a bare PCB on hand means a
mistake costs a hand-soldering session, not a second fab run.

## 9. The review gates — and what each one is actually for

`engineering-rigor.md` defines six gates, G1 through G6, that must pass **in order**, and `PROJECT-LOG.md`
is where each one gets signed and dated — not silently checked off in someone's head. The log file
itself makes the stakes explicit: "a gate marked passed that wasn't is worse than no gate, because it
silently removes the check everyone downstream assumes happened." Here's what each gate is actually
defending against, in the class of mistake it catches:

**G1 — Schematic review.** This is where you confirm the ERC came back with zero errors, every
suppressed warning has a written justification sitting next to it, the pin-map tables in the
instruction docs match what's actually drawn on the schematic (crucially, read aloud by *someone
other than the author* — a documented pin map and a schematic can silently drift apart even when both
individually look fine), the power tree is drawn out and the current budget re-added by hand, and
every single connector pin is accounted for. This gate exists to catch the class of mistake where the
schematic is internally consistent but doesn't actually match what the documentation promised, or
where a current budget was assumed rather than added up.

**G2 — Layout review.** DRC clean, a 3D collision check against the mechanical STEP file (a 3D model
of the enclosure or faceplate, checked in Altium's 3D viewer to make sure a tall component doesn't
physically collide with the case), a visual sweep confirming the ground plane on layer 2 is genuinely
unbroken, an audit that protection parts really are placed closest to the connector as intended, and
a specific audit of the switching regulator's hot loop. This catches the class of mistake where the
rules passed automatically but the physical reality — a part that doesn't fit, a ground plane
accidentally split by a via, protection parts placed in the wrong physical order — would not have
shown up in a rules check at all.

**G3 — Paper build.** Print the board 1:1 (true physical size) and tape it to the actual wheel or
dash panel; put hands on it; do a reach test for every encoder and button, wearing gloves, because the
driver will be wearing gloves. This catches ergonomic mistakes that no electrical or DRC check could
ever catch — a control placed somewhere a gloved thumb can't actually reach.

**G4 — Netlist cross-check.** An independent re-derivation of the connector pinouts — from the
schematic, compared against the §0 fixed-facts tables, compared against the harness drawing — done by
someone re-deriving it, not re-reading the same table twice. Crucially this includes checking *which
ground each signal references*, not just that a ground pin exists. This is the gate that caught one of
the project's real defects: the first version of the dash's servo-signal wiring shared a ground pin
between eight sensitive analog return lines and two digital PWM signal returns, an error that "survived
the first write-up and was only caught on a re-read" — which the project's own lessons file says is
"exactly what gate G4 exists for."

**G5 — Peer sign-off.** A second person reviews the evidence from G1 through G4 and their name goes in
the project log. This is the gate that exists because, plainly, **the author of a design cannot see
their own blind spots.** It is not a formality layered on top of the other four gates — it's the
recognition that a design can be self-consistent and still wrong in a way that only becomes visible to
someone who didn't write it. This project's own defect history backs that up directly: eleven defects
have been found across this design so far (lesson L19 in `engineering-rigor.md`), and multiple of them — the BOOT0/CAN conflict, the missing
EXTMODE strap, three errors in one supposedly-frozen pin table — passed an initial self-review by the
person who wrote the section, and were only caught on a later, harder look. "Plausible-sounding detail
is the most dangerous kind of wrong, because it survives review — a reviewer checks whether the pin
map is self-consistent, not whether it was invented" is how the project's own lessons file puts it.
That's precisely the failure mode G5 is built to intercept: get a second, independent set of eyes
before money is spent.

**G6 — Pre-order.** BOM availability is re-checked the *same day* as ordering — not from notes taken
a week earlier — including each part's lifecycle status (Active, NRND meaning "Not Recommended for New
Design," or EOL meaning discontinued; stock quantity alone tells you nothing about whether a part is
still in production). The variant fitted-lists get diffed against each other one more time, and the
gerbers get visually inspected in a *third-party* viewer, deliberately not Altium itself, so a bug or
blind spot in Altium's own rendering isn't the last line of defense. This gate exists because
availability and correctness both have a shelf life — a BOM checked weeks ago can be stale by order
day, and a part in stock today can have gone EOL since you selected it.

## 10. Bring-up: powering a board on is a story, not a switch flip

Once boards and assemblies arrive, the process explicitly does **not** go straight to bolting the
board into the car. `engineering-rigor.md` §4 lays out a staged bring-up procedure, and it's worth
understanding *why* the stages are ordered the way they are, because the order is the safety
mechanism.

**Step 1 is visual and passive, before any power at all.** Inspect the solder joints under
magnification, check continuity on every ground connection, and measure resistance from each power
rail to ground — expecting something above 100 ohms. This step exists to catch a dead short *before*
you ever apply power to find it the expensive way, with smoke.

**Step 2 is power-only, from a current-limited bench supply.** Both boards get 12V from a supply that
is deliberately capped at a low current limit — 150 mA for the wheel, 200 mA for the dash — well below
what the board would draw if everything were working normally. This is the single most important
sequencing decision in the whole bring-up: **a current-limited supply turns a short circuit into a
supply that refuses to give more current, instead of a supply that dumps unlimited amps into a fault
and destroys parts or starts a fire.** If something is wrong with the power stage, the failure is a
supply that can't reach the expected voltage — annoying, diagnosable, and safe — rather than a burned
board. Only after every rail checks out within about 3% of its expected value, with a thermal camera
or a finger sweep checking that the buck inductor isn't running hot, does bring-up proceed to
anything more active.

**Step 3 brings up the debug connection** (Tag-Connect SWD attach, read the MCU's ID, flash a trivial
"blinky" program, confirm 3.3V holds up under load) — proving the microcontroller itself is alive and
programmable before asking it to do anything specific.

**Step 4 tests CAN in isolation** — first an external loopback test — the MCU transmits through the
real transceiver and reads its own frames back, which proves the transceiver and its wiring rather
than just the peripheral — then a real two-node bench bus using the USB-CAN sniffer from step 2 of
the whole build, checking bit timing on a scope, where the scope check confirms the sample point lands
at roughly 80%. This proves the CAN hardware works *before* it's asked to talk to a real ECU.

**Step 5 closes out the protocol assumptions** — the emulated keypad against a bench ECU (closing A1),
IO12 frames checked in the ECU software (closing A2 for Box B), broadcast data checked against known
values — using exactly the sniffer-based bench setup the assumptions were designed to be closed with,
just now validated against the real, assembled hardware instead of a laptop-only bench test.

Only after HMI controls, LEDs, and the display have each been separately verified does the dash reach
its servo bring-up, and this is worth walking through as its own story because the stated safety
reasoning is direct: **you scope the PWM outputs and confirm clean pulses before a servo is ever
connected, you verify the firmware end-stops that clamp commands to the servo's actual mechanical
travel *before* the servo is ever bolted to a physical linkage**, because "a servo grinding a hard stop
is the most likely way to destroy one." Only once the end-stops are proven in isolation does a servo
get connected, and even then testing proceeds with the servo off the car and the linkage disconnected:
confirm the shared ground reference isn't floating, confirm that losing CAN makes the servo hold its
last position rather than snap to center (a servo that recentres itself mid-corner is a handling
event, not a bug), and confirm the divergence alarm by manually stalling the servo by hand.

Threading through every stage of bring-up is one more standing rule worth calling out on its own:
**change one thing at a time**, and write down every observation in a per-board log
(`hardware/<board>/bringup-log.md`). The reason this matters in bring-up specifically is that when
several things change between one power-up and the next — a jumper, a firmware flag, a cable — and
something then goes wrong, you no longer know which change caused it. One variable at a time keeps
every test result attributable to a single cause.

## What's still open — read this before you order anything

This project's own documentation is explicit that it is not ready to order boards, and the honest
picture as of today looks like this:

Nine tracked open assumptions, A1 through A9 in `PROJECT-LOG.md` §1, are open — with a note that A6
(the ARB servo's actual torque and stall current, and whether it's even a PWM servo or a serial-bus
one) must specifically be closed before dash layout, because the answer could change the servo output
stage from analog PWM to a half-duplex UART interface entirely — a change that would ripple into the
schematic, not just the BOM.

The MCU pin maps on **both** boards are closed — verified against the STM32G474 datasheet and captured
in `wheel-schematic-complete.md` §9 and `dash-schematic-complete.md` §8. What remains open are the
`[OPEN]` items still listed in those two schematic-definition files.

Two footprint details on the wheel display — which side of the FPC connector the contacts are on, and
the exact SCS chip-select polarity and timing — are called out as still unverified and need confirming
from the spec sheet before the footprint and firmware are frozen.

None of the six review gates (G1–G6) have been signed off yet on either board — the project log's
gate tables are still blank checkboxes with no names or dates.

None of this is a reason to stall; it's the opposite — it's the punch list. The order of operations
this whole guide describes exists specifically so these items get closed in the cheapest possible
place: protocol assumptions on a bench with a $30 sniffer, pin-map correctness in a datasheet
verification pass, ergonomics with a paper printout, and everything else in a gated review — all
before any of them can turn into a wasted board order.

## Things I could not explain simply

A few items in the source material are precise engineering judgment calls that I could not compress
into plain language without either oversimplifying them or restating the source verbatim, so I'm
flagging them here rather than guessing:

- The exact reasoning for why CAN bus signaling at 1 Mbit/s on a board under 100 mm doesn't need
  impedance-controlled routing while USB does (both are described as differential pairs, but only USB
  gets a calculated impedance target) is a signal-integrity judgment tied to rise time versus trace
  length that the source states as a conclusion rather than deriving — I did not want to invent the
  underlying math.
- **TVS versus buck absolute maximum — settled, do not reopen.** The SMBJ33A clamps at 53.3 V. The
  original 35 V-class buck could not survive that, so the converter was changed, not the TVS. The
  LMR36015's 66 V absolute maximum leaves 12.7 V of margin, and the SMBJ33A's 33 V standoff is what
  clears a 24 V jump start. Do NOT substitute an SMBJ26A or any lower-standoff TVS: no SMBJ part fits
  between jump-start standoff and a 35–42 V absolute maximum, which is precisely why the buck moved
  instead. What IS still open is assumption A8 — scope the wheel's real 12 V transient environment to
  confirm clamp behaviour in the car.
- The finer mechanics of hardware quadrature decoding (why an encoder's A/B channels specifically need
  to be "both channels of the same timer with the correct alternate function," as opposed to any two
  GPIO pins) is referenced as the reason for one of the project's real defects, but the underlying
  timer-peripheral mechanism is stated as a fact in the source rather than explained from first
  principles, so I kept my explanation at the same level rather than adding detail the source doesn't
  provide.

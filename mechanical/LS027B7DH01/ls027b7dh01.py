"""Parametric mechanical model of the Sharp LS027B7DH01 2.7" memory LCD.

Every dimension below is taken from Sharp spec LCP-2110015A (the LS027B7DH01
specification, Rev. Jun 2010), section 3 "Mechanical Specification" and
Figure 8-1 "2.7" WQVGA Monochrome Outline Dimension". Values marked DERIVED
were measured off the vector geometry of Figure 8-1 rather than read from a
callout; they are noted individually.

Coordinate system
-----------------
    origin  active-area centre, on the front (display) surface
    +X      right,  viewed from the front
    +Y      up,     viewed from the front
    +Z      out of the display, toward the viewer

The module therefore occupies Z = 0 (front surface) down to Z = -1.64
(rear surface), so mating a bezel to the display face is just Z = 0.

The active-area centre is exactly the horizontal centre of the glass
(31.4 mm from either edge) but sits 19.64 mm below the top edge and
23.18 mm above the bottom edge -- Sharp dimension both of those, so this
origin is the datum the datasheet itself works from.

Run with:  python3 ls027b7dh01.py
"""

import cadquery as cq

# --------------------------------------------------------------------------
# Outline (Figure 8-1)
# --------------------------------------------------------------------------
GLASS_W = 62.8      # 62.8 +/-0.2   overall module width
GLASS_H = 42.82     # 42.82 +/-0.2  overall module height, set by the rear glass

ACT_W = 58.8        # active area
ACT_H = 35.28
ACT_FROM_LEFT = 2.0  # 2.0 +/-0.15
ACT_FROM_TOP = 2.0   # 2.0 +/-0.15

POL_W = 62.0        # both polarizer films are 62 x 40.42
POL_H = 40.42
POL_FROM_LEFT = 0.4  # (0.4) reference
POL_FROM_TOP = 0.3   # (0.3) reference

# The front (counter) glass is 1.6 mm shorter than the rear (TFT) glass -- the
# "(1.6)" reference in Figure 8-1. That step exposes a ledge along the bottom
# edge of the rear glass, which is where the FPC is bonded.
FRONT_GLASS_STEP = 1.6

# --------------------------------------------------------------------------
# Thickness stack, front -> rear. Sums to the 1.64 mm quoted in section 3.
# --------------------------------------------------------------------------
T_POL_FRONT = 0.23
T_GLASS_FRONT = 0.6
T_GLASS_REAR = 0.7
T_POL_REAR = 0.11

# --------------------------------------------------------------------------
# FPC tail. Widths are callouts; the lengthwise stations are DERIVED from the
# Figure 8-1 vector geometry (Sharp dimension the tail's width and its 47.28
# station, but not its overall free length).
# --------------------------------------------------------------------------
FPC_WIDE_W = 9.47    # 36.135 - 26.665, centred on the active centre
FPC_NECK_W = 8.64    # 8.64 +/-0.1, at 27.08 +/-0.4 from the left edge
FPC_WIDE_END = 46.49    # DERIVED: below glass top, where the taper starts
FPC_NECK_START = 47.28  # 47.28 +/-0.5 below glass top, taper complete
FPC_TIP = 56.60         # DERIVED: below glass top => 13.78 mm of free tail

FPC_T = 0.10         # DERIVED: base film thickness
STIFF_T = 0.20       # DERIVED: PI stiffener thickness
# FPC_T + STIFF_T = 0.30, which is the dimensioned 0.3 +/-0.03 insertion
# thickness at the contact end (and matches the 0.30 mm FPC that the
# recommended Molex 51441-1093 / SMK CFP-4610-0150F connectors accept).

STIFF_W = 5.5        # 5.5 +/-0.05, at 28.65 +/-0.4 from the left edge
STIFF_L = 4.0        # "4"

N_PINS = 10
PIN_PITCH = 0.5      # 0.5 +/-0.02
PIN_W = 0.35         # 0.35 +0.04/-0.03
PIN_L = 3.5          # "3.5", running back from the tip
PIN_T = 0.03         # DERIVED: plating recessed into the film, not proud of it

# Terminal names, Table 4-1. Pin 1 is on the right viewed from the display
# side, i.e. on the left viewed from the contact (rear) side.
PIN_NAMES = ["SCLK", "SI", "SCS", "EXTCOMIN", "DISP",
             "VDDA", "VDD", "EXTMODE", "VSS", "VSSA"]

# --------------------------------------------------------------------------
# Datum conversion: the datasheet works in (from left edge, from top edge);
# the model works from the active-area centre.
# --------------------------------------------------------------------------
CX = ACT_FROM_LEFT + ACT_W / 2.0   # 31.4 from the left edge
CY = ACT_FROM_TOP + ACT_H / 2.0    # 19.64 below the top edge


def x_of(from_left):
    return from_left - CX


def y_of(from_top):
    return CY - from_top


def plate(width, height, from_left, from_top, z_top, thickness):
    """A rectangular slab placed from datasheet datums, growing in -Z."""
    return (
        cq.Workplane("XY")
        .workplane(offset=z_top)
        .moveTo(x_of(from_left) + width / 2.0, y_of(from_top) - height / 2.0)
        .rect(width, height)
        .extrude(-thickness)
    )


# --------------------------------------------------------------------------
# Z stations, front surface at 0
# --------------------------------------------------------------------------
Z_FRONT = 0.0
Z_GLASS_FRONT = Z_FRONT - T_POL_FRONT              # -0.23
Z_GLASS_REAR = Z_GLASS_FRONT - T_GLASS_FRONT       # -0.83
Z_POL_REAR = Z_GLASS_REAR - T_GLASS_REAR           # -1.53
Z_BACK = Z_POL_REAR - T_POL_REAR                   # -1.64

# The FPC lies on the rear glass's front face, in the cavity left by the
# shorter front glass, so it never protrudes past the module envelope.
Z_FPC_REAR = Z_GLASS_REAR                          # -0.83
Z_FPC_FRONT = Z_FPC_REAR + FPC_T                   # -0.73
Z_STIFF_FRONT = Z_FPC_FRONT + STIFF_T              # -0.53

# --------------------------------------------------------------------------
# Bodies
# --------------------------------------------------------------------------
active_area = plate(ACT_W, ACT_H, ACT_FROM_LEFT, ACT_FROM_TOP,
                    Z_FRONT, T_POL_FRONT)

# Front polarizer with the active window removed, so the visible display
# region is its own coloured body without overlapping solids.
pol_front = plate(POL_W, POL_H, POL_FROM_LEFT, POL_FROM_TOP,
                  Z_FRONT, T_POL_FRONT).cut(active_area)

glass_front = plate(GLASS_W, GLASS_H - FRONT_GLASS_STEP, 0.0, 0.0,
                    Z_GLASS_FRONT, T_GLASS_FRONT)

glass_rear = plate(GLASS_W, GLASS_H, 0.0, 0.0,
                   Z_GLASS_REAR, T_GLASS_REAR)

pol_rear = plate(POL_W, POL_H, POL_FROM_LEFT, POL_FROM_TOP,
                 Z_POL_REAR, T_POL_REAR)

# --- FPC tail -------------------------------------------------------------
# Starts at the front glass's bottom edge (the top of the bond ledge) and runs
# out past the glass: wide section, short taper, then the neck to the tip.
y_bond = y_of(GLASS_H - FRONT_GLASS_STEP)
y_wide_end = y_of(FPC_WIDE_END)
y_neck_start = y_of(FPC_NECK_START)
y_tip = y_of(FPC_TIP)

hw = FPC_WIDE_W / 2.0
hn = FPC_NECK_W / 2.0

fpc = (
    cq.Workplane("XY")
    .workplane(offset=Z_FPC_FRONT)
    .polyline([
        (hw, y_bond),
        (hw, y_wide_end),
        (hn, y_neck_start),
        (hn, y_tip),
        (-hn, y_tip),
        (-hn, y_neck_start),
        (-hw, y_wide_end),
        (-hw, y_bond),
    ])
    .close()
    .extrude(-FPC_T)
)

stiffener = (
    cq.Workplane("XY")
    .workplane(offset=Z_STIFF_FRONT)
    .moveTo(0.0, y_tip + STIFF_L / 2.0)
    .rect(STIFF_W, STIFF_L)
    .extrude(-STIFF_T)
)

# Contacts sit on the rear face of the film, recessed into it so the tip stays
# within the 0.30 mm insertion thickness. Pin 1 at +X (right, seen from front).
pin_x = [(N_PINS - 1 - i) * PIN_PITCH - (N_PINS - 1) * PIN_PITCH / 2.0
         for i in range(N_PINS)]

contacts = None
for cx_i in pin_x:
    pad = (
        cq.Workplane("XY")
        .workplane(offset=Z_FPC_REAR + PIN_T)
        .moveTo(cx_i, y_tip + PIN_L / 2.0)
        .rect(PIN_W, PIN_L)
        .extrude(-PIN_T)
    )
    contacts = pad if contacts is None else contacts.union(pad)

fpc = fpc.cut(contacts)

# --------------------------------------------------------------------------
# Assembly
# --------------------------------------------------------------------------
BLACK = cq.Color(0.11, 0.11, 0.12)
SILVER = cq.Color(0.78, 0.80, 0.82)
GLASS = cq.Color(0.62, 0.70, 0.74)
AMBER = cq.Color(0.72, 0.45, 0.13)
DARK_AMBER = cq.Color(0.45, 0.28, 0.09)
GOLD = cq.Color(0.85, 0.70, 0.28)

assembly = (
    cq.Assembly(name="LS027B7DH01")
    .add(active_area, name="active_area", color=SILVER)
    .add(pol_front, name="polarizer_front", color=BLACK)
    .add(glass_front, name="glass_front", color=GLASS)
    .add(glass_rear, name="glass_rear", color=GLASS)
    .add(pol_rear, name="polarizer_rear", color=BLACK)
    .add(fpc, name="fpc", color=AMBER)
    .add(stiffener, name="fpc_stiffener", color=DARK_AMBER)
    .add(contacts, name="fpc_contacts", color=GOLD)
)

if __name__ == "__main__":
    import os

    here = os.path.dirname(os.path.abspath(__file__))
    step_path = os.path.join(here, "LS027B7DH01.step")
    assembly.export(step_path)

    bb = assembly.toCompound().BoundingBox()
    print("wrote", step_path)
    print("bounding box (mm):")
    print("  X %+8.3f .. %+8.3f   (%.3f)" % (bb.xmin, bb.xmax, bb.xlen))
    print("  Y %+8.3f .. %+8.3f   (%.3f)" % (bb.ymin, bb.ymax, bb.ylen))
    print("  Z %+8.3f .. %+8.3f   (%.3f)" % (bb.zmin, bb.zmax, bb.zlen))

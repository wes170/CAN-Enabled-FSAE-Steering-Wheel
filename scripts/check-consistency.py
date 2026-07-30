#!/usr/bin/env python3
"""
Cross-document consistency checker.

Why this exists: defect 8.1. A correction was applied to one table and silently
missed the same fact in a differently-formatted table elsewhere — and the missed
one was the authoritative pin map people are told to build from. Prose review
does not catch that. A parser does.

Each check below exists because a real defect got past a human review. Do not
delete a check because it is currently passing; that is what it is for.

Run from the repo root:  python3 scripts/check-consistency.py
Exit code 1 if anything fails, so it can gate a commit.
"""
import re, sys, csv, pathlib, collections

ROOT = pathlib.Path(__file__).resolve().parent.parent
DOCS = {p.relative_to(ROOT).as_posix(): p.read_text(encoding="utf-8")
        for p in list((ROOT / "memory").rglob("*.md"))
        + list((ROOT / "firmware").rglob("*.h"))
        + list((ROOT / "hardware").rglob("*.csv"))
        + [ROOT / "PROJECT-LOG.md"] if p.exists()}

fails = []
notes = []


def check_wheel_pin_table():
    """The §9 table is authoritative. No pin may appear twice, and the encoder
    timers must all be ones that actually implement quadrature mode (defect 1.5)."""
    ENCODER_CAPABLE = {"TIM1", "TIM2", "TIM3", "TIM4", "TIM5", "TIM8", "TIM20", "LPTIM1"}
    src = DOCS.get("memory/wheel-schematic-complete.md", "")
    if "## 9." not in src:
        fails.append("wheel §9 pin table not found")
        return
    sec = src.split("## 9.")[1].split("## 10.")[0]
    pins = collections.defaultdict(list)
    for line in sec.splitlines():
        if not line.startswith("|") or "---" in line or "| Pin " in line:
            continue
        cells = line.split("|")
        if len(cells) < 4:
            continue
        net = cells[2].strip()[:40]
        for pin in re.findall(r"P[A-F]\d{1,2}\b", cells[1]):
            pins[pin].append(net)
        if "ENC" in net and "_A" in net:
            # Only the ASSIGNMENT counts, not the prose after it. Rows legitimately
            # discuss the superseded timer ("was TIM15..."), and an earlier version
            # of this check flagged that history as a live defect.
            assignment = re.split(r"—|--|\(", cells[3])[0]
            timers = set(re.findall(r"TIM\d+", assignment))
            bad = timers - ENCODER_CAPABLE
            if bad:
                fails.append(f"wheel §9: {net} uses {sorted(bad)} which cannot decode quadrature")
            if not timers:
                fails.append(f"wheel §9: {net} names no timer before its explanatory text")
    dupes = {p: n for p, n in pins.items() if len(n) > 1}
    if dupes:
        fails.append(f"wheel §9: pins assigned more than once: {dupes}")
    print(f"  wheel §9 pin table: {len(pins)} pins, "
          f"{'DUPLICATES' if dupes else 'no duplicates'}")


def check_pin_table_complete():
    """§3.1 lists all 64 physical pins of the LQFP-64 and is what people capture
    from. Two failure modes: a pin listed twice or missing entirely, and §3.1
    disagreeing with the signal-level map in §9. The second is defect 8.1's
    exact shape -- two tables stating the same fact in different formats."""
    src = DOCS.get("memory/wheel-schematic-complete.md", "")
    if "### 3.1 Complete pin assignment" not in src:
        fails.append("wheel §3.1 pin table not found")
        return
    sec = src.split("### 3.1 Complete pin assignment")[1].split("#### Power-pin summary")[0]

    seen, by_port = [], {}
    for m in re.finditer(r"^\| (\d+) \| `([^`]+)` \| (.+?) \|", sec, re.M):
        seen.append(int(m.group(1)))
        port = re.match(r"(P[A-F]\d{1,2})", m.group(2))
        if port:
            nets = re.findall(r"`([^`]+)`", m.group(3))
            by_port[port.group(1)] = (nets[0] if nets else
                                      ("spare" if "spare" in m.group(3).lower() else m.group(3)))

    missing = [i for i in range(1, 65) if i not in seen]
    dupes = sorted({i for i in seen if seen.count(i) > 1})
    if missing: fails.append(f"wheel §3.1: pins missing from the table: {missing}")
    if dupes:   fails.append(f"wheel §3.1: pins listed more than once: {dupes}")

    # Cross-check against §9. Grouped rows using an ellipsis are expanded only in
    # §3.1, so they are skipped here rather than mis-paired.
    sec9 = src.split("## 9.")[1].split("## 10.")[0]
    n = 0
    for line in sec9.splitlines():
        if not line.startswith("|") or "---" in line or "| Pin " in line:
            continue
        c = line.split("|")
        if len(c) < 3 or "\u2026" in c[2] or "..." in c[2]:
            continue
        pins = re.findall(r"P[A-F]\d{1,2}\b", c[1])
        nets = re.findall(r"`([^`]+)`", c[2])
        if "spare" in c[2].lower():
            for pin in pins:
                n += 1
                if "spare" not in (by_port.get(pin) or "").lower():
                    fails.append(f"§9 says {pin} is spare, §3.1 says '{by_port.get(pin)}'")
        else:
            for pin, net in zip(pins, nets):
                n += 1
                if by_port.get(pin) != net:
                    fails.append(f"{pin}: §9 '{net}' vs §3.1 '{by_port.get(pin)}'")
    print(f"  wheel §3.1 pin table: {len(seen)}/64 pins, {n} cross-checked against §9")


def check_adc_channels():
    """Defect 8.3: channel numbers are not pin numbers. Verified DS12288 Table 12."""
    TRUTH = {"PA0": 1, "PA1": 2, "PA2": 3, "PA3": 4,
             "PC0": 6, "PC1": 7, "PC2": 8, "PC3": 9,
             "PB0": 15, "PB1": 12}
    fw = DOCS.get("firmware/include/board_config.h", "")
    for macro, pin in [("V12_SENSE_CH", "PA1"), ("V5_SENSE_CH", "PA2")]:
        m = re.search(rf"#define\s+{macro}\s+(\d+)u", fw)
        if not m:
            fails.append(f"{macro} not found in board_config.h")
            continue
        got, want = int(m.group(1)), TRUTH[pin]
        if got != want:
            fails.append(f"{macro} = {got}, but {pin} is ADC channel {want} (DS12288 Table 12)")
        print(f"  {macro}: {got} ({pin} = IN{want}) {'OK' if got == want else 'MISMATCH'}")

    m = re.search(r"#define\s+AIN_CHANNELS\s*\{([^}]*)\}", fw)
    if m:
        got = [int(x) for x in re.findall(r"\d+", m.group(1))]
        want = [TRUTH[p] for p in ["PA0", "PA1", "PA2", "PA3", "PC0", "PC1", "PC2", "PC3"]]
        if got != want:
            fails.append(f"AIN_CHANNELS = {got}, expected {want}")
        print(f"  AIN_CHANNELS: {got} {'OK' if got == want else 'MISMATCH'}")


def check_firmware_matches_schematic():
    """board_config.h is downstream of the schematic definitions."""
    fw = DOCS.get("firmware/include/board_config.h", "")
    wheel = DOCS.get("memory/wheel-schematic-complete.md", "")
    pairs = [
        ("ENC5 timer", r"ENC5_TIM\s+(TIM\d+)", fw, r"`ENC5_A`[^|]*\|[^|]*?(TIM\d+)_CH1", wheel),
        ("LED timer",  r"LED_DATA_TIM\s+(TIM\d+)", fw, r"`LED_DATA_3V3`[^|]*\|\s*(TIM\d+)_CH1", wheel),
    ]
    for label, rx_a, src_a, rx_b, src_b in pairs:
        a, b = re.search(rx_a, src_a), re.search(rx_b, src_b)
        if not (a and b):
            fails.append(f"{label}: could not locate in both files")
            continue
        if a.group(1) != b.group(1):
            fails.append(f"{label}: firmware says {a.group(1)}, schematic says {b.group(1)}")
        print(f"  {label}: firmware={a.group(1)} schematic={b.group(1)} "
              f"{'OK' if a.group(1) == b.group(1) else 'MISMATCH'}")


def check_ucpd_hazard_documented():
    """Defect 8.2. The only fix is a firmware bit, so the documentation IS the
    mitigation. If these references vanish, the hazard silently returns."""
    required = {
        "firmware/include/board_config.h": "UCPD1_DBDIS",
        "memory/wheel-schematic-complete.md": "UCPD1_DBDIS",
        "memory/dash-schematic-complete.md": "UCPD1_DBDIS",
        "memory/engineering-rigor.md": "UCPD1_DBDIS",
    }
    for f, token in required.items():
        if token not in DOCS.get(f, ""):
            fails.append(f"{f}: lost the {token} hazard note (defect 8.2 mitigation)")
    print(f"  UCPD dead-battery note present in {len(required)} required files")


def check_buck_support_parts():
    """Defects 8.5 and 8.8: every part a converter datasheet calls *required* must
    be an orderable BOM line, not prose inside another part's notes cell."""
    # LMR36015 (both boards)
    REQUIRED = ["C_BOOT", "C_VCC", "R_FBT", "R_FBB", "C_FF"]
    # AP63203 (dash only) -- C_BST2 was defect 8.8 and was originally left out of
    # this list, which an audit caught: the check would have passed with the part
    # deleted again.
    DASH_EXTRA = ["C_BST2"]
    for f in [k for k in DOCS if k.endswith(".csv")]:
        refs = {r[0].strip() for r in csv.reader(DOCS[f].splitlines()) if r}
        want = REQUIRED + (DASH_EXTRA if "dash" in f else [])
        missing = [p for p in want if p not in refs]
        if missing:
            fails.append(f"{f}: datasheet-required parts missing as BOM lines: {missing}")
        print(f"  {f}: buck support parts {'OK' if not missing else 'MISSING ' + str(missing)}")


def check_battery_rail_cap_ratings():
    """Defect 8.10: the SMBJ33A clamps +12V_P at 53.3 V, so every capacitor on that
    rail needs a rating above it. 50 V parts were specified for a long time."""
    for f in [k for k in DOCS if k.endswith(".csv")]:
        for row in csv.reader(DOCS[f].splitlines()):
            if len(row) < 4 or not row[0].strip():
                continue
            desc, val = row[2].lower(), row[3]
            # Only the 12 V entry stage sees the clamp. The 3.3 V buck's input sits
            # on the regulated +5V rail, where 50 V is enormous margin -- an earlier
            # version of this check flagged it and was wrong.
            downstream_of_5v = "3v3" in desc or "3.3v" in desc
            on_battery_rail = (not downstream_of_5v
                               and ("buck input" in desc or "input decoupling" in desc
                                    or "input bulk" in desc))
            if on_battery_rail and "50V" in val.replace(" ", ""):
                fails.append(f"{f}: {row[0]} is on the 53.3 V-clamped rail but rated 50V "
                             f"({val.strip()}) - defect 8.10")
    print("  battery-rail capacitor ratings checked against the 53.3 V clamp")


def check_no_rejected_parts_as_live_spec():
    """Rejected parts may appear in explanatory / historical context, but never as
    a live instruction. Scope is limited to the files someone BUILDS from —
    verification and selection records are comparative by nature (defect 8.4)."""
    REJECTED = ["AP63205", "AMS1117", "BAT54S", "LMR33630"]
    # Anything someone captures, assembles or orders from. sim-variant-instructions.md
    # belongs here and was originally missed -- it carries live "fit this / do not
    # fit that" steps and, unlike the two Altium files, no superseded banner.
    BUILD_FILES = [k for k in DOCS
                   if k.endswith("-altium-instructions.md")
                   or k.endswith("-schematic-complete.md")
                   or k.endswith("sim-variant-instructions.md")
                   or k.endswith(".csv")]
    # A rejected part may be NAMED as long as the line makes clear it is not the
    # choice. These verbs are what "explaining why we didn't use it" looks like.
    EXCUSE = re.compile(r"reject|supersed|defect|~~|legacy|historical|earlier|not the|"
                        r"\bnot\b|instead of|rather than|fallback|was |trap|lesson|"
                        r"unverified|classic|unlike|replace[sd]?|leaks?|requires? "
                        r"tantalum|abs.?max", re.I)
    for part in REJECTED:
        for f in BUILD_FILES:
            for i, line in enumerate(DOCS[f].splitlines(), 1):
                if part in line and not EXCUSE.search(line):
                    fails.append(f"{f}:{i} names rejected part {part} as a live spec")
    print(f"  rejected parts: scanned {len(BUILD_FILES)} build-from files")


def check_stale_values():
    """Defect 8.1/8.6: after a correction, the OLD value must be gone from the
    files people build from — confirming the new value appears is not enough."""
    STALE = [
        ("TIM15_CH1", "ENC5 was moved off TIM15 (defect 1.5)"),
        ("LMR33630ADDAR", "rejected buck (defect 5.1)"),
        ("nBOOT_SEL", "bit name does not exist on STM32G4 (defect 1.6)"),
    ]
    # Anything someone captures, assembles or orders from. sim-variant-instructions.md
    # belongs here and was originally missed -- it carries live "fit this / do not
    # fit that" steps and, unlike the two Altium files, no superseded banner.
    BUILD_FILES = [k for k in DOCS
                   if k.endswith("-altium-instructions.md")
                   or k.endswith("-schematic-complete.md")
                   or k.endswith("sim-variant-instructions.md")
                   or k.endswith(".csv") or k.endswith(".h")]
    for token, why in STALE:
        for f in BUILD_FILES:
            for i, line in enumerate(DOCS[f].splitlines(), 1):
                if token in line and not re.search(r"was |defect|not |never|rejected|earlier",
                                                   line, re.I):
                    fails.append(f"{f}:{i} still contains stale '{token}' — {why}")
    print(f"  stale-value sweep: {len(STALE)} superseded tokens checked")


def check_boms_parse():
    for f in [k for k in DOCS if k.endswith(".csv")]:
        rows = list(csv.reader(DOCS[f].splitlines()))
        width = len(rows[0])
        bad = [i + 1 for i, r in enumerate(rows)
               if len(r) != width and any(c.strip() for c in r)]
        if bad:
            fails.append(f"{f}: malformed CSV rows {bad}")
        print(f"  {f}: {len(rows)-1} rows, {width} cols, "
              f"{'malformed ' + str(bad) if bad else 'well-formed'}")


print("Cross-document consistency check\n")
for fn in (check_wheel_pin_table, check_pin_table_complete, check_adc_channels, check_firmware_matches_schematic,
           check_ucpd_hazard_documented, check_buck_support_parts,
           check_battery_rail_cap_ratings,
           check_no_rejected_parts_as_live_spec, check_stale_values, check_boms_parse):
    fn()

print()
for n in notes:
    print(f"  note: {n}")
if fails:
    print(f"FAILED — {len(fails)} issue(s):")
    for f in fails:
        print(f"  ✗ {f}")
    sys.exit(1)
print("PASSED — all checks clean")

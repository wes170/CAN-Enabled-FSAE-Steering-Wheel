# Altium SchDoc extraction

Turns `.SchDoc` files into reviewable text: a component report, a computed netlist, and a
cross-sheet net cross-reference. Written so a schematic review can be grounded in the
actual capture rather than in a written description of it.

## Why this exists

`.SchDoc` comes in two flavours and neither is reviewable as-is:

- **Binary** — an OLE compound file. The `FileHeader` stream holds length-prefixed
  pipe-delimited records.
- **ASCII** — the same pipe-delimited records, one per line, CRLF-terminated.

`extract.py` reads both and normalises them to the same record dicts.

More importantly, **Altium schematic connectivity is geometric**. Nothing in the file says
"R3 pin 1 connects to Q1 gate"; it says a wire runs from (150,250) to (190,250) and a pin
happens to end at each point. `netlist.py` reconstructs connectivity from that geometry.

## Usage

```sh
pip install olefile
python3 scripts/altium/extract.py  <dir-of-SchDocs> <outdir>   # record dumps
python3 scripts/altium/netlist.py  <dir-of-SchDocs> <outdir>   # component report + netlist
python3 scripts/altium/xref.py     <outdir> <outdir>/GLOBAL-NET-XREF.txt
```

`netlist.py` writes `<sheet>.netlist.txt`; `extract.py` writes `<sheet>.records.txt`.

## How connectivity is reconstructed

1. Wire segments (`RECORD=27` polylines) are split into segments and unioned where they
   share an endpoint, or where one's endpoint lands on another's interior — Altium
   auto-junctions a T, so that is a real connection. Two wires *crossing* with neither
   endpoint involved is **not** a connection, and is not unioned.
2. Pin connection points are computed from `LOCATION` plus `PINLENGTH` in the direction of
   the low 2 bits of `PINCONGLOMERATE` (0=right, 1=up, 2=left, 3=down). Which end of the
   pin is electrically hot is resolved **empirically** per sheet — both hypotheses are
   scored against how many pins land on a wire, and the winner is used and reported in the
   file header. Every sheet in this project resolves to the far end.
3. Pins are attached to any segment they touch; coincident pins are unioned directly.
4. Net labels (`RECORD=25`) and power ports (`RECORD=17`) attach by coordinate.
5. Islands carrying the same name are then merged, because power ports are always global
   and net labels are global in a flat project with no ports or sheet symbols. The report
   states how many separate wire islands a net was assembled from, so a net joined only by
   name is visible as such rather than looking like one drawn connection.

## Gotchas found the hard way

- **Pins are duplicated** across display modes / part records. They are deduped on
  `(owner, designator, location, direction, length)`. Without this, every passive shows
  four pins and every net double-counts.
- **`OWNERINDEX` is off by one** from the record's position in the file, because the
  `HEADER` line is not itself a record. Owner *i* is record *i+1*.
- **Same-name nets look like separate single-pin nets** before the name merge. Skipping
  step 5 produces a flood of false "single-pin net" findings.
- **A designator of `*` means the part is un-annotated**, not that extraction failed.
- Coordinates are in 10-mil units. `_FRAC` fields carry a sub-unit remainder and are
  ignored — everything of interest here sits on the 10-mil grid.

## Limitations

- Buses, bus entries, and harnesses are not resolved. This project uses none.
- Ports and sheet-symbol entries are not resolved. This project uses none — every
  inter-sheet connection is by matching net name, which is what `xref.py` checks.
- Multi-part components are reported per-part without merging parts into one device.
- Off-grid or diagonal wiring is handled by a collinearity test, but a pin that misses a
  wire by a fraction of a unit will read as unconnected — which is usually the truth.

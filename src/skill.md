---
name: lintel
description: |
  Inspect and compare construction drawing sets (PDF) with Lintel. Use when
  asked what changed between two revisions of a drawing set, which sheets a
  set contains, or to look at a sheet or its revision overlay as an image.
---

# Lintel

Lintel reads PDF drawing sets: architectural, structural, and MEP sheets, one
sheet per page. The `lintel` command lists, compares, and renders them. No
command needs a display, and none changes the PDFs.

## Commands

- `lintel sheets <set.pdf> --json` lists each sheet's number, label (A-101,
  when the PDF labels its pages), and size in inches.
- `lintel compare <new.pdf> <old.pdf> --json` compares two revisions sheet by
  sheet. Sheets pair up by label, or by position when the PDFs have no labels.
  Each sheet's `status` is `changed`, `unchanged`, or `new`; `changes` lists the
  changed areas as fractions of the sheet (x, y, width, height from the top
  left), in reading order; `aligned` says the old revision was re-plotted
  with a shift or scale that Lintel corrected. `removed` lists sheets only in
  the old set.
- `lintel render <set.pdf> --sheet A-101 -o sheet.png` renders a sheet.
  Add `--against <old.pdf>` for the revision overlay: removed work red, added
  work green, unchanged faded. `--previous` renders the old sheet instead, and
  `--width` sets the image width (default 2400).

## Working with the person

- To describe what changed, run `compare`, then render the overlay of each
  changed sheet and look at the areas `changes` points to before summarizing.
  Name sheets by label.
- A change in the title block's revision table or date is routine; mention
  it briefly and focus on changes in the drawing itself.
- `lintel open <new.pdf> <old.pdf>` opens the viewer on their desktop, where
  `]` steps through every change. It needs their display, so do not wait on it.
- Every command takes `--json`, exits 0 on success and 1 on failure, and
  writes errors to stderr.

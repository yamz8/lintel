# Lintel

A fast viewer for construction drawing sets, for [Omarchy](https://omarchy.org).
Open a PDF set, flip through its sheets, and overlay an older revision to see
what changed: removed work in red, added work in green.

![Lintel comparing two revisions of a floor plan](docs/screenshot.png)

## What it does

- Opens a PDF drawing set and shows every sheet as a thumbnail.
- Zooms and pans smoothly on large sheets, sharp at any zoom: past a quick
  base image, only the visible part renders, in tiles, at full resolution.
- Compares two revisions. Sheets are paired by their page label (A-101 to
  A-101) when the PDFs label their pages, and by position otherwise.
- Lines the older revision up first when it was re-plotted with a slight
  shift or scale, so the overlay shows real changes, not a misregistration.
- Compares every sheet in the background and marks each one **Changed** or
  **New**, then walks you through the changes one at a time: `]` zooms to the
  next changed area, across sheets.
- Follows the Omarchy theme accent, window rounding, and language live.

- Marks up sheets with revision clouds, arrows, notes, and measurements. Set
  each sheet's scale from the standard architectural and metric scales, or
  calibrate it by drawing along a dimension you know. Markups save as you
  go, with undo, in `~/.local/share/lintel/markups/`, keyed by the PDF's
  contents: the PDF itself is never changed.
- Exports the set with its markups drawn on (`E`), every sheet or just the
  marked-up ones. The markups are stamped onto the original pages with
  qpdf, so the drawings stay vector and their text stays searchable.

## Usage

```sh
lintel open                                     # then press O, or drop a PDF on the window
lintel open drawings-rev1.pdf drawings-rev0.pdf # open a set and compare
```

### For scripts and coding agents

These need no display, take `--json`, and exit 0 or 1 with errors on stderr.

```sh
lintel sheets set.pdf                           # sheet labels and sizes
lintel compare rev1.pdf rev0.pdf --json         # which sheets changed, and where
lintel render rev1.pdf --sheet A-101 --against rev0.pdf -o overlay.png
lintel markups set.pdf --json                   # markups made in the viewer
lintel export set.pdf --marked-only -o out.pdf  # the marked-up sheets as PDF
lintel skill install                            # teach your coding agents to use Lintel
```

With a set open, drop a PDF on the left half of the window to open it as the
set, or on the right half to compare with it.

## Shortcuts

| Key | Action |
| --- | --- |
| `O` | Open a drawing set |
| `C` | Compare with an older revision |
| `X` | Close the older revision |
| `Space` | Toggle the overlay |
| `1` / `2` / `3` | Current, previous, overlay |
| `]` / `[` | Next and previous change |
| `N` / `Shift+N` | Next and previous changed sheet |
| `J` / `Page Down` | Next sheet |
| `K` / `Page Up` | Previous sheet |
| `Home` / `End` | First and last sheet |
| `+` / `-` | Zoom in and out |
| `0` / `F` | Fit the sheet |
| Arrow keys | Pan |
| Wheel | Zoom at the pointer |
| Drag, touchpad scroll | Pan |
| `Ctrl` + touchpad scroll, pinch | Zoom |
| `V` | Pan, and pick markups (drag one to move it) |
| `R` / `A` / `T` | Cloud, arrow, text |
| `M` | Measure |
| `S` | Set the sheet's scale |
| `Delete` | Delete the picked markup |
| `E` | Export marked-up sheets to PDF |
| `Ctrl+Z` / `Ctrl+Shift+Z` | Undo and redo |
| `Esc` | Back to panning |
| `?` | Show or hide shortcuts |
| `Q` | Quit |

## Build

Lintel is C++ and Qt Quick, built with qmake. Qt PDF comes from
`qt6-webengine` on Arch, and exporting uses `qpdf`.

```sh
bin/build      # builds build/lintel
bin/test       # runs the tests offscreen
bin/install    # builds and installs the package, so Lintel is in the app launcher
```

## License

MIT. The theme and portal file picker code follows
[Omacut](https://github.com/omacom-io/omacut) by David Heinemeier Hansson, also MIT.

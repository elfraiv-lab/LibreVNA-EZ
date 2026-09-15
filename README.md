# LibreVNA-GUI — antenna & cable build

A fork of the [LibreVNA](https://github.com/jankae/LibreVNA) PC GUI (version 1.6.5) reworked for
one thing: measuring antennas and coaxial feed lines with a LibreVNA, on a desktop **and** on a
small Windows tablet. Everything below is added on top of upstream — the measurement engine,
calibration and device communication are untouched.

The short version: a set of guided measurements borrowed from handheld analyzers (R&S ZVH and the
ZNB/ZNA application notes), marker types that answer the question instead of making you read it off
the graph, a PDF report, four themes, and a touch-friendly layout.

---
![Alt text](https://github.com/elfraiv-lab/LibreVNA-EZ/blob/4f82859f42f147f9281dfb8d13dd5389f8ec1072/IMG/%7BD4B09C32-6362-4DF6-8FF1-CF9F6B7A2148%7D.png?raw=true)
## Guided measurements

Each of these is a dialog that configures the sweep, creates the traces and graphs, and places the
right marker — reachable from the **Tools** menu and from a short button in the sweep toolbar.

### Antenna band — `Ant`
Enter the band the antenna has to cover and the maximum acceptable VSWR. The dialog sets the sweep
to the band plus a margin, drops a PASS/FAIL limit line on a VSWR (or magnitude) graph and places an
antenna band marker on the first live reflection trace. A cable between the calibration plane and
the antenna can be compensated here with a port extension (delay and loss from the cable library),
so VSWR and impedance refer to the antenna connector rather than to the end of your test lead.

### Cable impedance — `TDR`
Characteristic impedance of a cable section from the S11 step response. Either the physical length
is known and the velocity factor is derived from the measured delay, or the velocity factor is known
and the length is derived. Sets up a 1000-point sweep, a TDR trace and an impedance-vs-distance graph
with a fixed 0…100 Ω axis (with autorange the open end squeezes the cable plateau to nothing).

### Cable loss — `Loss`
One-port cable loss, the ZVH method: leave the far end open (or short it), so everything that comes
back has passed the cable twice and the one-way loss is `-|S11| dB / 2`. The ripple caused by the
imperfect open/short is averaged out and reported separately. Creates a trace whose magnitude reads
the one-way loss directly in positive dB, and a marker with min/max/mean, dB per meter and the
datasheet value for comparison.

### Antenna report — `PDF`
A full A4 report: header (antenna name, date, device, calibration state, sweep, notes), a summary of
every reflection trace (resonance, VSWR, |S|, impedance, worst VSWR), tables for each marker type,
PASS/FAIL for every limit line, and the graphs — Smith chart inline if it fits, VSWR and magnitude
plots on their own landscape pages. Graphs are rendered in a temporary print mode (white background,
black axes, darkened trace colours) so they stay readable on paper. A Touchstone `.s1p` file is
written next to the PDF.

All four dialogs warn when the sweep is not covered by an active calibration, instead of quietly
reporting raw receiver data as if it were a measurement.

---

## Marker types

| Type | What it reports |
|---|---|
| **Antenna band (VSWR)** | Sits on the resonance; helper markers find the band edges where VSWR crosses a threshold. Shows `fL / fH` and `BW(VSWR≤2): 600 MHz (27.3 %)`. `<` and `>` mark edges clipped by the sweep. Draws the threshold line on the graph. |
| **Band statistics** | min / max / mean of VSWR or \|S\| over the band, marker at the worst point, plus a ripple check: peak-to-peak of the detrended \|S\| against an optional limit, with PASS/FAIL. |
| **Cable impedance (TDR)** | Averages the impedance over the first flat section of the step response, reports min/max/mean, length or velocity factor, delay, and whether the far end looks open or shorted — a cable you know is open showing "short end" means the phase is inverted, i.e. no calibration. Section edges can be dragged. |
| **Cable loss (open/short)** | One-way loss with the ripple smoothed out: worst case, min/max/mean, dB/m and dB/100 m against the datasheet, remaining ripple. Flags \|S11\| above 0 dB, which can only mean a missing calibration. |

The band each marker works over is its existing *Restrict* range, so a multi-band antenna is handled
by restricting one marker per band.

### Cable library
Velocity factor, nominal impedance and an attenuation model for a handful of common coax types
(RG-58 A/U foam PE from the Radiolab datasheet with a real `a·√f + b·f` fit, RG-58 C/U, RG-316,
RG-174, LMR-240). Used by the TDR dialog (fills in the velocity factor, keeps the datasheet value
next to the measured one), by the antenna cable port extension, and by the cable loss comparison.

---

## Appearance

**Four themes**, switchable in *View → Theme* without restarting:

- **Light** and **Dark** — a bench-instrument graphite palette with no blue cast and a single amber
  accent (`#f5b800`), which is also the S11 trace colour.
- **Acid green** — green-tinted dark chrome with an acid-green accent.
- **Phosphor scope** — light grey enamel front panel, dark CRT screen with a bright graticule and a
  phosphor-green trace.

Supporting changes:

- Graph background, axes, grid and division bands follow the theme (*View → Graph colors follow
  theme*, on by default; editing colours in Preferences turns it off automatically).
- A curated 16-colour trace palette per theme, with contrast ≥ 4.5:1 on white in the light themes.
  Switching themes re-tints traces that still use palette colours; custom colours are left alone,
  and legacy Qt defaults from older setups are mapped into the palette on load.
- Numeric fields and all graph readouts use a monospace font (Cascadia Mono, Consolas fallback), so
  digits stop dancing as values update.
- Trace names and marker readouts on graphs are darkened or lightened to keep 3:1 contrast against
  the graph background.
- 15 toolbar glyphs redrawn as SVG, auto-inverted in dark themes.

---

## Tablet and touch

*View → Tablet mode* switches the whole UI over:

- 40 px controls and 28 px icons, thicker splitter handles and dock separators.
- Graph fonts ×1.4 and trace lines ×1.5 — applied at draw time, so the stored preferences never
  accumulate the scaling.
- Dock panels (markers, traces, device log) move into tabs on the right, the log is hidden.
- An on-screen SI numpad opens on a tap in any value field.
- The sweep toolbar wraps into two rows, and the acquisition toolbar starts a new row.
- Window layouts are stored separately per mode and per desktop/tablet, so switching back and forth
  does not destroy either arrangement.

**Compact toolbar** (on by default, *View → Compact toolbar*) hides the controls that never change
during antenna work — sweep type, log sweep, dwell time, averaging.

**Lock panels / Hide panels** freeze the dock arrangement or get the panels out of the way with one
button, without losing the layout.

---

## Workflow

- **Undo / redo** of the whole VNA setup — `Ctrl+Z`, `Ctrl+Y`, `Ctrl+Shift+Z`, 50 steps deep.
  Snapshots are taken on a timer and compared on a normalised JSON, so sweeping, auto-markers and
  window resizing do not flood the history. Marker positions are deliberately not undoable.
- **VNA-only mode** (`UI.vnaOnly`): the signal generator and spectrum analyzer modes are hidden. The
  code is still there, nothing is deleted.
- **Frequency limits** (`UI.minFrequency` / `UI.maxFrequency`, 100 kHz … 6 GHz by default) clamp the
  sweep and the graph X axis, so panning and zooming cannot wander outside what the hardware can do.
  The driver also raises the device's reported minimum, which the LibreVNA reports as 0 Hz.
- **Square tiles for circular charts** — Smith and polar plots ask their splitter for a width equal
  to their height plus the marker data column, so they stop being ovals.
- Per-graph **"Show marker data"** toggle, saved with the setup.

---

## Fixes to upstream behaviour

- Crash on narrow plots (a `QRect` overflow assertion under Qt 6.11).
- VSWR axis clamped to 1…20 everywhere — autorange, axis dialog, pan and zoom — instead of running
  off to infinity at a resonance.
- *Fit traces* on time/distance axes starts at 0, hiding the acausal half of the TDR.
- Sweep frequency fields show at most two decimals, with the full value restored while editing.
- Y axis margin derived from the actual font metrics, so labels no longer overlap the axis name.
- Several PDF report layout fixes: wrapped table cells instead of elided text, correct label metrics,
  graphs stretched to the page.

---

## Building

Qt 6 / C++17. **Build through `CMakeLists.txt`, not through the `.pro` file.**

With Qt 6.11.2 MinGW, `moc.exe` crashes while parsing libstdc++ headers, and qmake silently skips
generating the moc rules as a result. The CMake build works around it with
`set(CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES "")` — do not remove that line. The `.pro` file is kept
in sync for upstream compatibility only.

```
Kit:    Desktop Qt 6.11.2 MinGW 64-bit, Release
libusb: libusb-1.0.a next to CMakeLists.txt, headers in <Qt>/mingw_64/include/libusb-1.0
```

For a standalone build run `windeployqt` and add `libwinpthread-1.dll`, `libgcc_s_seh-1.dll` and
`libstdc++-6.dll` from the MinGW `bin` directory.

---

## SCPI / automation compatibility

The SCPI server is unchanged and still enabled by default on TCP port 19542, including the
`--no-gui` and `-p` command line options. This build works with
[mcp-librevna](https://github.com/OOHehir/mcp-librevna) for the VNA tools. The `sa_*` and `gen_*`
tools do not work, because the spectrum analyzer and generator modes are not created in VNA-only
mode — that is intentional.

---

## Scope and status

This is a personal build shaped around one workflow: S11 and VSWR on a LibreVNA HW1 Rev.B over
100 kHz … 6 GHz, on a desktop and an 11.6" Windows tablet. It is shared in case any of it is useful,
not as a general-purpose replacement for upstream.

The UI is English only — a translation mechanism is not wired up (`tr()` is essentially absent from
upstream, ~800 strings).

Fork of [jankae/LibreVNA](https://github.com/jankae/LibreVNA); it carries upstream's license, whose
`LICENSE` file lives in the repository root above this directory.

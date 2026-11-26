# M1.2 editor spike — raw results (2026-11-26)

Machine: Linux, 12 cores, Qt 6.12.0, RelWithDebInfo build. Reproduce: `editor_spike --bench --size-mb 5`, `--variants`, `--typing`.

# M1.2 editor spike — wayland, Qt 6.12.0, GPU renderer
Corpus: 5 MB, 5243253 chars

| Measurement | Result |
|---|---|
| Load + highlight (highlighter attached first) | 3169 ms |
| Load: next frame | 16 ms |
| Blocks (lines) | 119851 |
| Memory after load + highlight (RSS delta) | 315 MB |
| Highlight formats present on a block | yes (11 ranges) |
| positionToRectangle (avg of 1000, mid-document) | 8.9 µs |
| Typing mid-document, key→frame p50 / p95 / max | 40.0 / 51.7 / 56.2 ms |
| Typing at document start, p50 / p95 / max | 44.2 / 59.3 / 92.7 ms |
| Typing `/*` mid-document (re-highlights the rest) | `/` 62 ms, `*` 109 ms |
| Undo `/*` (Ctrl+Z ×2) → frame | 612 ms |
| Jump to end of document → frame | 208 ms |
| Memory at end of typing tests (RSS delta) | 319 MB |
| Theme switch: full re-highlight with batch highlighter → frame | 2724 ms |
| Folding: hide 100 lines, text below moves up | **yes** (y 1089636 → 1087836) |
| Auto-close `()` as one QTextCursor edit block, TextArea.undo() once | canUndo=true, text after undo `SELECT ` → **one step** |
| IME: preedit `ni` shown, commit `你好`, undo | preedit=`ni`, after commit `SELECT 1; -- 你好`, after undo `SELECT 1; -- ` |
| Peak memory (RSS delta at end) | 534 MB |

# M1.2 editor spike — offscreen, Qt 6.12.0, software renderer
Corpus: 5 MB, 5243253 chars

| Measurement | Result |
|---|---|
| Load + highlight (highlighter attached first) | 3522 ms |
| Load: next frame | 5 ms |
| Blocks (lines) | 119851 |
| Memory after load + highlight (RSS delta) | 307 MB |
| Highlight formats present on a block | yes (11 ranges) |
| positionToRectangle (avg of 1000, mid-document) | 3.9 µs |
| Typing mid-document, key→frame p50 / p95 / max | 39.3 / 44.4 / 59.5 ms |
| Typing at document start, p50 / p95 / max | 47.0 / 58.8 / 101.4 ms |
| Typing `/*` mid-document (re-highlights the rest) | `/` 63 ms, `*` 74 ms |
| Undo `/*` (Ctrl+Z ×2) → frame | 466 ms |
| Jump to end of document → frame | 147 ms |
| Memory at end of typing tests (RSS delta) | 308 MB |
| Theme switch: full re-highlight with batch highlighter → frame | 2865 ms |

# M1.2 editor spike — offscreen, Qt 6.12.0, software renderer
Corpus: 1 MB, 1049466 chars

| Measurement | Result |
|---|---|
| Load: setText | 261 ms |
| Load: next frame | 5 ms |
| Blocks (lines) | 24201 |
| Memory after load (RSS delta) | 82 MB |
| Highlight whole document (sync) | 79669 ms |
| Frame after highlight | 7 ms |
| Memory after highlight (RSS delta) | 96 MB |
| Theme switch (re-highlight) | 91052 ms |
| Highlight formats present on a block | yes (11 ranges) |
| positionToRectangle (avg of 1000, mid-document) | 3.5 µs |
| Typing mid-document, key→frame p50 / p95 / max | 11.5 / 12.1 / 12.2 ms |
| Typing at document start, p50 / p95 / max | 11.6 / 12.1 / 20.5 ms |
| Typing `/*` mid-document (re-highlights the rest) | `/` 18 ms, `*` 18 ms |
| Undo `/*` (Ctrl+Z ×2) → frame | 84 ms |
| Jump to end of document → frame | 33 ms |
| Memory at end of typing tests (RSS delta) | 97 MB |
| Folding: hide 100 lines, text below moves up | **yes** (y 223692 → 221892) |
| Auto-close `()` as one QTextCursor edit block, TextArea.undo() once | canUndo=true, text after undo `SELECT ` → **one step** |
| IME: preedit `ni` shown, commit `你好`, undo | preedit=`ni`, after commit `SELECT 1; -- 你好`, after undo `SELECT 1; -- ` |
| Peak memory (RSS delta at end) | 94 MB |

## Highlight variants — 0.25 MB, 6100 lines
| A. QTextDocument without a view | 46 ms |
| B. TextArea, highlighter attached before setText (load + highlight) | 159 ms |
| C. TextArea hidden during rehighlight | 4048 ms |

## Highlight variants — 0.5 MB, 12150 lines
| A. QTextDocument without a view | 110 ms |
| B. TextArea, highlighter attached before setText (load + highlight) | 467 ms |
| C. TextArea hidden during rehighlight | 29959 ms |

## Highlight variants — 0.25 MB, 6100 lines
| A. QTextDocument without a view | 44 ms |
| B. TextArea, highlighter attached before setText (load + highlight) | 159 ms |
| C. TextArea hidden during rehighlight | 4025 ms |
| D. Signals blocked during rehighlight (+1 markContentsDirty) | 4143 ms (rehighlight 4046 ms), formats on block: yes |

## Highlight variants — 0.5 MB, 12150 lines
| A. QTextDocument without a view | 84 ms |
| B. TextArea, highlighter attached before setText (load + highlight) | 296 ms |
| C. TextArea hidden during rehighlight | 17300 ms |
| D. Signals blocked during rehighlight (+1 markContentsDirty) | 21474 ms (rehighlight 21293 ms), formats on block: yes |

## Highlight variants — 0.5 MB, 12150 lines
| A. QTextDocument without a view | 98 ms |
| B. TextArea, highlighter attached before setText (load + highlight) | 367 ms |
| C. TextArea hidden during rehighlight | 21214 ms |
| D. Signals blocked during rehighlight (+1 markContentsDirty) | 19438 ms (rehighlight 19258 ms), formats on block: yes |
| G. Own batch highlighter (setFormats + one dirty mark) → frame | 258 ms (highlight 253 ms), formats on block: yes (5) |

## Highlight variants — 1 MB, 24200 lines
| A. QTextDocument without a view | 164 ms |
| B. TextArea, highlighter attached before setText (load + highlight) | 583 ms |
| C. TextArea hidden during rehighlight | 76011 ms |
| D. Signals blocked during rehighlight (+1 markContentsDirty) | 82771 ms (rehighlight 82402 ms), formats on block: yes |
| G. Own batch highlighter (setFormats + one dirty mark) → frame | 520 ms (highlight 515 ms), formats on block: yes (5) |

## Typing without a highlighter (key→frame, mid-document)
| Size | p50 | p95 | max |
|---|---|---|---|
| 0.5 MB | 9.2 ms | 9.8 ms | 9.9 ms |
| 1 MB | 10.2 ms | 10.9 ms | 10.9 ms |
| 2 MB | 15.8 ms | 16.5 ms | 16.7 ms |
| 5 MB | 38.4 ms | 39.3 ms | 39.6 ms |

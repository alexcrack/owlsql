# Spec: Design system (draft v0.1)

Status: **draft**, decisions 054–069 (see `../decisions.md`).
Covers: theme tokens and palettes, typography, sizing, motion, control list, and mockups of
the main screens. Colour values are first proposals — expect tuning once real screens exist.

---

## 1. Character

- **Modern IDE-like**, compact, flat, 1px dividers (decision 010).
- **Owl amber** accent on **warm neutral** greys — a deliberate break from the blue-IDE norm.
- **Native title bar** and OS menu bar (macOS uses the global menu bar).
- **Subtle motion** (100–150 ms), never on data (grid scrolling, typing).
- Accessibility target: **basic** — everything keyboard-reachable, visible focus; no formal
  contrast audit, but palettes below aim for readable text contrast anyway.

---

## 2. Theme architecture

```
resources/themes/owl-dark.json   owl-light.json      (built-in, token format v1)
            │
   ThemeManager (C++, QML_SINGLETON "Theme")
     · mode: System | Light | Dark  (System follows Qt.styleHints.colorScheme)
     · accent override (later: user picks accent)
     · exposes grouped tokens: Theme.color.*, Theme.font.*, Theme.size.*, Theme.radius.*, Theme.motion.*
            │
   OwlSql.Style (custom Qt Quick Controls style, based on Basic templates)
   OwlSql.Ui components read only Theme tokens — never literal colours or sizes
```

- Token files are JSON from day one, so user theme files (post-1.0, decision 030) are only a
  matter of loading from a user directory.
- The editor highlighting theme is a separate KSyntaxHighlighting theme JSON (`owl-dark.theme`,
  `owl-light.theme`), selected together with the UI theme unless overridden.
- Switching themes is live (no restart).

---

## 3. Colour tokens

### 3.1 Neutrals (warm)

| Token | Dark | Light | Use |
|---|---|---|---|
| `bg.canvas` | `#161514` | `#F2F0EC` | Window background behind panes |
| `bg.surface` | `#1C1B19` | `#FAF9F7` | Panes: tree, workspace, bottom panel |
| `bg.raised` | `#242320` | `#FFFFFF` | Inputs, popups, menus, cards |
| `bg.sunken` | `#131211` | `#EDEAE6` | Editor gutter, grid header, status bar |
| `bg.hover` | `#2B2926` | `#ECE9E4` | Row / control hover |
| `bg.selected` | `#3A2E1B` | `#F6E6CC` | Selected row (accent-tinted) |
| `bg.selectedInactive` | `#2E2C29` | `#E6E2DC` | Selection in an unfocused view |
| `border.subtle` | `#2A2825` | `#E8E4DF` | Grid lines, inner dividers |
| `border.default` | `#38352F` | `#D8D3CC` | Pane dividers, input borders |
| `border.strong` | `#4D4943` | `#B9B3AA` | Hovered inputs, emphasis |
| `text.primary` | `#ECE8E2` | `#1E1C19` | Main text |
| `text.secondary` | `#B3ADA4` | `#4E4943` | Labels, secondary info |
| `text.muted` | `#857F76` | `#7A746B` | Placeholders, NULL, hints |
| `text.disabled` | `#5A5650` | `#AEA89F` | Disabled |

### 3.2 Accent (owl amber)

| Token | Dark | Light | Use |
|---|---|---|---|
| `accent.fill` | `#E8A33D` | `#D99128` | Primary button, switch on, progress |
| `accent.fillHover` | `#F0B356` | `#C98320` | |
| `accent.fillPressed` | `#D08F2A` | `#B57318` | |
| `accent.onFill` | `#1C1300` | `#1C1300` | Text/icons on accent fill (dark text in both themes) |
| `accent.line` | `#E8A33D` | `#C7801A` | Active tab underline, focus ring, selection bar |
| `accent.text` | `#F0B356` | `#955C08` | Links, accent-coloured text |
| `accent.subtle` | `#3A2E1B` | `#F6E6CC` | Tinted backgrounds (= `bg.selected`) |

### 3.3 Semantic

| Token | Dark | Light |
|---|---|---|
| `danger` (errors, prod, destructive) | `#EF5B5B` | `#C93A3A` |
| `warning` (warnings, unsaved, truncated) | `#E6C34A` | `#9C7A0C` |
| `success` (committed, connected) | `#5BBE83` | `#2E8A57` |
| `info` (notices, hints) | `#6AA7E8` | `#2F6DB5` |
| `*.subtle` variants | 15 % alpha of the above over `bg.surface` | 12 % alpha |

Staged-edit colours in the grid: edited cell `warning.subtle` + left marker; inserted row
`success.subtle`; deleted row `danger.subtle` + strikethrough.

### 3.4 Session tags and safety levels

Session colour tags (user-picked) come from a fixed 8-colour set tuned for both themes:
`red, orange, amber, green, teal, blue, violet, pink`.

| Level | Default tag | Extra treatment |
|---|---|---|
| dev | green | — |
| staging | blue | — |
| prod | red | 2px `danger` frame around the workspace, red **PROD** badge in status bar, confirmations (decision 015) |

### 3.5 Grid value colours (toggle, decision 059)

Default: only NULL is styled (`text.muted`, italic). With "Colour values by type" on:

| Family | Dark | Light |
|---|---|---|
| number | `#7FB6F0` | `#1F5FA8` |
| text | `text.primary` | `text.primary` |
| date/time | `#8CCB8F` | `#2F7A3A` |
| boolean | `#D7A2E8` | `#7D3E98` |
| json / array | `#E6C34A` | `#8A6A00` |
| binary | `#C9A27A` | `#7A5530` |

---

## 4. Typography

| Token | Font | Size / line | Use |
|---|---|---|---|
| `font.ui` | Inter | 13 / 18 | Default UI text |
| `font.uiSmall` | Inter | 12 / 16 | Secondary, badges, status bar |
| `font.uiTiny` | Inter | 11 / 14 | Captions, key chips |
| `font.title` | Inter SemiBold | 15 / 20 | Pane and dialog titles |
| `font.heading` | Inter SemiBold | 20 / 26 | Welcome tab, empty states |
| `font.code` | JetBrains Mono | 13 / 20 | SQL editor, DDL views |
| `font.grid` | JetBrains Mono | 12 / 16 | Grid cells (tabular figures, aligned numbers) |

Numbers in the UI use Inter's tabular figures (`font.features: { "tnum": 1 }`).

---

## 5. Size, spacing, radius, elevation, motion

**Spacing scale** (`Theme.space.*`): 2, 4, 6, 8, 12, 16, 24, 32.

| Size token | Value |
|---|---|
| `size.row` (tree, grid, list) | 24 |
| `size.control` (button, input, combo) | 28 |
| `size.controlSmall` | 22 |
| `size.tabBar` | 34 |
| `size.toolbar` | 36 |
| `size.statusBar` | 24 |
| `size.icon` | 16 (14 in dense spots) |
| `size.treeIndent` | 16 |
| `size.splitterHit` | 6 (1px visible line) |

**Radius**: `radius.control` 4, `radius.popup` 8, `radius.pill` 999.

**Elevation**: popups/menus/dialogs = `bg.raised` + `border.default` + soft shadow
(dark: black 40 %, 16px blur; light: black 12 %, 12px blur). Panes have no shadow.

**Motion**: `motion.fast` 100 ms, `motion.normal` 150 ms, easing `OutCubic`.
Used for popup fade/scale, tree expand, panel collapse, toast in/out. Disabled when the OS
requests reduced motion (where Qt exposes it) or by a setting.

**Focus**: 2px `accent.line` ring, radius-matched, offset 1px. Only for keyboard focus.

---

## 6. Iconography

- **Tabler Icons** outline set, 16px, 1.5px stroke, tinted from `text.secondary` (normal),
  `text.primary` (hover/active), `accent.text` (active toggles).
- `OwlIcon { name: "table" }` component → loads the SVG from resources, tints via `color`.
- Engine logos (PostgreSQL, MySQL, MariaDB, SQLite) are separate full-colour SVGs used only in the
  session list and connection form.
- Object-kind → icon mapping lives in the catalog (`ObjectNode.icon`), not in QML.

---

## 7. Control inventory (`OwlSql.Style` + `OwlSql.Ui`)

**Style (Qt Quick Controls templates):**
Button (primary / secondary / ghost / danger; normal + small; icon-only) · ToolButton ·
SplitButton (`Run ▾`) · TextField · SearchField (icon, clear, shortcut hint) · PasswordField
(reveal toggle) · TextArea · SpinBox · ComboBox (+ searchable variant) · CheckBox · Switch ·
RadioButton · Menu / MenuItem / MenuSeparator · ToolTip (with key chip) · ScrollBar (thin overlay,
widens on hover) · SplitView handle · ProgressBar + BusyIndicator · Dialog · Popup.

**App components:**
WorkspaceTabBar (underline tabs, dirty dot, close on hover, middle-click close, overflow menu,
session colour dot) · InnerTabBar (smaller underline tabs inside an object tab) ·
ObjectTree (tree row: chevron, icon, name, badge, muted counts; filter field on top) ·
DataGrid (header: name, type hint, sort arrow, filter button; row-number column; staged-edit
markers; per-type cell editors) · FilterBar (WHERE box + chips for column filters) ·
SqlEditor (gutter: line numbers, error marks, run marker) · ResultTabs · BottomPanel (Log /
History / Messages) · StatusBar items · Badge (DEV/STAGE/PROD, counts) · KeyChip (`Ctrl` `K`) ·
ColorTag dot · Banner (inline error/warning in a pane) · Toast · EmptyState (icon, title, text,
action) · FormRow (label, control, hint, error) · SegmentedControl (`Code | Constructor`) ·
CommandPalette.

Every component appears in a dev-only **Component Gallery** window (`owlsql --gallery`) with a
theme switch (one global `Theme`, so themes are shown one at a time; the UI smoke test
screenshots both). Button variants: `variant: "primary" | "secondary" | "ghost" | "danger"`,
`small: true`, icons via `icon.name: "<tabler-name>"`.

---

## 8. Screen mockups

### 8.1 Main shell (native title bar, prod session active)

```
┌ OwlSql — prod-pg / app_db ───────────────────────────────────── _ □ ✕ ┐
│ File  Edit  View  Session  Query  Tools  Help                         │
├────────────────┬──────────────────────────────────────────────────────┤
│ 🔍 Filter  ⌘K  │ ● Welcome │ ● users │ ● query 1 • │ ● orders │   +    │
│                │━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━│
│ ▾ ● prod-pg    ┃ Data  Structure  Indexes  Foreign keys  DDL  Stats   ┃
│   ▾ app_db     ┃ ───────                                               ┃
│     ▾ public   ┃ WHERE [ status = 'active'                ] (email ▾ ✕)┃
│       ▾ Tables ┃ ┌───┬──────┬──────────────────┬────────────┬────────┐ ┃
│         users  ┃ │ # │ id   │ email            │ created_at │ meta   │ ┃
│         orders ┃ │ 1 │    1 │ ann@example.com  │ 2026-01-02 │ {…}    │ ┃
│       ▸ Views  ┃ │ 2 │    2 │ bob@example.com ▌│ 2026-01-03 │ NULL   │ ┃
│       ▸ Funcs  ┃ └───┴──────┴──────────────────┴────────────┴────────┘ ┃
│   ▸ postgres   ┃ 1 change staged  [Apply] [Discard] [Preview SQL]      ┃
│ ▸ ● local-lite ┃                                                        ┃
├────────────────┴──────────────────────────────────────────────────────┤
│ PROD  prod-pg · PG 17.2 │ Auto-commit │ 1–1000 of ~52k · 38 ms │ Sel 1 │
└───────────────────────────────────────────────────────────────────────┘
   ┃ = 2px red prod frame around the workspace      bottom panel collapsed (Ctrl+J)
```

### 8.2 Welcome tab (session manager)

```
┌ ● Welcome ─────────────────────────────────────────────────────────────┐
│  🦉 OwlSql                                                             │
│ ┌────────────────────────────┐ ┌────────────────────────────────────┐ │
│ │ 🔍 Search sessions…        │ │ prod-pg                      PROD  │ │
│ │ ▾ Work                     │ │ Driver  [🐘 PostgreSQL        ▾]   │ │
│ │   ● prod-pg         PROD   │ │ Name    [prod-pg              ]    │ │
│ │   ● stage-pg        STAGE  │ │ Folder  [Work                 ▾]   │ │
│ │ ▸ Personal                 │ │ Level   ( dev | staging | ●prod )  │ │
│ │ Recent                     │ │ Colour  ● ● ● ● ● ● ● ●            │ │
│ │   ● local-lite  2 h ago    │ │ ─ Connection  SSL  SSH  Advanced ─ │ │
│ │                            │ │ Host [db.internal   ] Port [5432]  │ │
│ │                            │ │ User [app           ] Pass [•••• ] │ │
│ │                            │ │ Database [app_db    ]              │ │
│ │ [+ New] [⇪ Import ▾]       │ │ [Paste URL]  [Test] [Save] [Open ▶]│ │
│ └────────────────────────────┘ └────────────────────────────────────┘ │
└────────────────────────────────────────────────────────────────────────┘
```

> **Implemented (M1.9b)** as `WelcomePage.qml` (`SessionList`, `SessionEditor`, `ConnectionField`,
> `SessionRow`, `SafetyBadge`, `TagDot`, `TagSwatch`) over the `Sessions` QML singleton
> (decisions 171–174). The editor has Connection / TLS / Advanced tabs (SSH joins with tunnels in M3);
> "Recent" is not a separate section yet — the list is ordered by folder.

### 8.3 Query tab

```
┌ ● query 1 • ─────────────────────────────────────────────────────────┐
│ [▶ Run ▾] [■ Stop] │ app_db ▾ │ public ▾ │ ⟲ History │ ☆ Snippets  │
├──────────────────────────────────────────────────────────────────────┤
│  1 │ SELECT u.id, u.email, count(o.id) AS orders                    │
│  2 │ FROM users u                                                    │
│▶ 3 │ JOIN orders o ON o.user_id = u.id                               │
│  4 │ GROUP BY 1, 2                                                   │
│  5 │ ORDER BY orders DESC;                                           │
├══════════════════════════════════════════════════════════════════════┤
│ Result 1 (1000+) │ Result 2 │ Messages                               │
│ ┌──────┬───────────────────┬────────┐                                │
│ │ id   │ email             │ orders │                                │
│ │   12 │ zoe@example.com   │    214 │                                │
│ └──────┴───────────────────┴────────┘                                │
│ Paused at 1000 rows  [Load 1000 more] [Fetch all]          120 ms    │
└──────────────────────────────────────────────────────────────────────┘
```

### 8.4 Table structure editor

```
┌ Structure ── users ──────────────────────────────────────────────────┐
│ Name [users        ]  Schema [public ▾]  Comment [App users       ]  │
│ ┌─────────────────────────────────────────────────────────────────┐  │
│ │ ⠿ │ PK │ Name       │ Type          │ Null │ Default │ Comment  │  │
│ │ ⠿ │ 🔑 │ id         │ bigint        │      │ identity│          │  │
│ │ ⠿ │    │ email      │ text          │      │         │          │  │
│ │ ⠿ │    │ meta       │ jsonb         │  ✓   │ '{}'    │  (new)   │  │
│ └─────────────────────────────────────────────────────────────────┘  │
│ [+ Column] [− Remove] [↑] [↓]      Indexes · FKs · Checks · Triggers │
├──────────────────────────────────────────────────────────────────────┤
│ Preview SQL                                                          │
│  ALTER TABLE public.users ADD COLUMN meta jsonb DEFAULT '{}';        │
│                                        [Discard]  [Apply ▶]          │
└──────────────────────────────────────────────────────────────────────┘
```

### 8.5 JSON cell editor (decision 022)

```
┌ meta · users #2 ───────────────────── ( Code | ●Constructor ) ─ ✕ ┐
│ ▾ {} object (3)                                        [+ key ▾]  │
│     theme      "string"   │ dark                       │ [type ▾] │
│     beta       boolean    │ ☑                          │          │
│   ▾ tags       [] array (2)                            [+ item]  │
│       0        "string"   │ admin                      │          │
│       1        "string"   │ ops                        │          │
├───────────────────────────────────────────────────────────────────┤
│ ✓ valid JSON · 3 keys                       [Cancel]  [Apply]     │
└───────────────────────────────────────────────────────────────────┘
   Code mode: SqlEditor in JSON highlighting, Format / Minify buttons, live validation.
```

### 8.6 Command palette (Ctrl+K)

```
            ┌──────────────────────────────────────────────┐
            │ 🔍 ord                                       │
            ├──────────────────────────────────────────────┤
            │ ▤ orders          table · prod-pg/app_db     │
            │ ▤ order_items     table · prod-pg/app_db     │
            │ ⚡ Open order report.sql      snippet         │
            │ ⟲ SELECT … FROM orders …     history · 2h    │
            │ ⌘ Toggle order of …          action     Alt+O│
            └──────────────────────────────────────────────┘
   Prefixes: > actions   @ objects   # history   ! snippets
```

---

## 9. Behaviour notes

- **Bottom panel**: collapsed by default; opens itself on an error or server notice; Ctrl+J
  toggles; tabs Log / History / Messages.
- **Status bar** (left → right): session + server version + safety badge · transaction state
  (Auto-commit toggle, or `TX open (3)` with Commit / Rollback) · active tab metrics (rows, time,
  fetch progress) · cursor (Ln/Col) or grid selection (count, sum, avg for numbers).
- **Object tabs**: one workspace tab per object, inner tabs Data | Structure | Indexes |
  Foreign keys | DDL | Stats (kinds without data drop the Data tab).
- **Grid lines**: zebra rows + vertical lines by default; both toggleable in settings.
- **Session colour dot** appears on tree root, every tab belonging to the session, and status bar.

---

## 10. Open questions

- Logo: placeholder owl mark — monochrome line owl in amber; final logo later.
- ✅ OS colour scheme: `QStyleHints::colorScheme` / `colorSchemeChanged`; explicit Light/Dark is
  passed back with `setColorScheme()` so native title bars match (decision 107).
- ✅ Reduced motion: Qt 6.12 exposes no OS hint, so it is an OwlSql setting
  (`Theme.reduceMotion`, zeroes `Theme.motion.*`) (decision 108).
- Comfortable density preset (post-1.0) — tokens already allow it.

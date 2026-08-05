# Spectron sidecar for Exult

A **separate Rust process** that sits next to Exult (same git checkout, not inside the C++ tree). It is a window into **the Avatar's mind** as they travel Britannia and learn: first-person thoughts before and after talks, passing insights about signs, places, deaths, and connections the dialogue never quite said aloud.

It:

1. Opens an **egui** window for those thoughts (greeting / passing thoughts / parting)
2. Listens on `http://127.0.0.1:8765/event` for JSON game events
3. Calls Spectron (`/chat`, `/facts`) with fixed prompts — the player never types

Exult’s C++ bridge (`usecode/spectron_bridge.cc`) POSTs events for talks, books, signs, combat/sleep/dungeon, quantised travel, and Avatar death. Demo buttons in the window still work for offline testing.

The UI leans on an Ultima VII-ish look: gold speech text with a dark drop-shadow, stone/wood panels, and the OFL font **MedievalSharp** under `assets/fonts/` (see `OFL.txt`).

```
exult/                  ← Ultima VII engine (C++, SDL, autoconf)
└── spectron-sidecar/   ← this crate (Rust only)
```

## What the Avatar's mind does

Voice: **first person** (`I` / `me` / `my`). Curious, warm, lightly period in flavour. Hunches are welcome; certainty is not required.

| Moment | What you see | Spectron call |
| --- | --- | --- |
| Start talking | **As I greet them** — what I already know about this `npc_id`, what I still have not asked | `/chat` briefing |
| Copy-protection question mid-talk (Finnigan / Batlin) | **Passing thoughts** — cloth-map / manual answer recalled locally (`docs/copy-protection.md`), so the mind prompts the number even if Spectron retrieval is thin | `/chat` (answer injected) |
| End talking (`Bye` / talk end) | **As we part** + **People** card (what I know about them now) | `/facts` transcript + `/chat` farewell + `/chat` dossier |
| **Ask Spectron: who have I met?** (Setup) | Rebuilds the **People** panel from Spectron's enumeration | `/chat` roster list |
| Read a book | Silent ingest as authoritative knowledge (no read-aloud thought — the Avatar is assumed to attend to books themself) | `/facts` (`knowledge`) only |
| Read a sign | **Passing thoughts** — \"This runic sign says…\" plus a guess at where I am | `/facts` (`knowledge`) + `/chat` |
| Single-click examine (name popup) | Silent ingest — \"Injured Man\", \"a dog\", furniture, … (glance only, not a talk) | `/facts` (`knowledge`) only |
| Die | **Passing thoughts** — true death (BG: often wake in Paws; SI: monk hourglass). Distinct from knock-out. | `/facts` + `/chat` |
| Knocked out (Avatar or companion, HP ≤ 0) | **Passing thoughts** — down but not dead; may rise if combat leaves them | `/facts` + `/chat` |
| Companion dies | **Passing thoughts** — true companion death (body / party dead-list) | `/facts` + `/chat` |
| Open map with sextant (outdoors) | **Passing thoughts** — cloth-map latitude/longitude bound into the current **situation** stretch | `/facts` (`situation` + sextant) + `/chat` |
| Double-click watch / sundial | **Passing thoughts** — \"It is 10 o'clock\" (exact bark; ambient dawn/day/dusk/night still rides other events) | `/facts` (`context`) + `/chat` |
| Companion join/leave | **Passing thoughts** — reaction (prior Quests colour Iolo / Shamino / …) | `/facts` + `/chat` |
| Enter dungeon / every few travel stretches | Soft **where am I?** musing (often wrong; signs, sextant, and situation `seen` glances keep this honest). Travel covers **foot**, **cart**, **ship**, and **magic carpet**. | `/chat` (situation always `/facts`) |
| Sudden relocate (moongate / jail teleport / …) | **Passing thoughts** — disorientation + arrival `seen`; walk stretch reset so walks don't span the jump | `/facts` + `/chat` |
| Weather flip while outdoors on the road | Soft **silence on the road** — atmosphere only (clear→rain/snow…), not a location guess | `/chat` |

Stored facts stay third-person for extraction quality; only the spoken mind is first-person.

**Authority.** Books, signs, and **sextant readings** are **written / observed geography** knowledge (`memory_category=knowledge`, labels `kind=book|sign|sextant`, `authority=written`). **Examine** glances are **seen labels** (`kind=examine`, `authority=seen`, `epistemic=examined`) — short but often dense (animals, injured NPCs you cannot talk to, objects). Sextant coords use the cloth-map grid (same as Finnigan’s questions), emitted only when the Avatar opens the world map **outdoors with a sextant** — never raw tile GPS. NPC dialogue is **spoken testimony** (`kind=conversation`, `authority=spoken`) — what was said, not proven fact. Vetron's Guide to Weapons and Armour is a good example of the book path: ratings and comparisons should stick as reference facts.

Identity rules still hold: durable key is `npc_id`; `appears_as` is the shape/role; personal names only after dialogue reveals them.

### Tracking decisions

| Idea | Decision |
| --- | --- |
| **Companion join/leave** | Tracked — `companion_join` / `companion_leave`. |
| **Quest flags / key items** | Prefer dialogue/examine text; no special flag hook unless Exult later makes a clean signal. |
| **Time leaps after sleep** | Wanted — sleep duration if Exult exposes hours chosen. |
| **Books** | Silent authoritative ingest only (no book "Passing thoughts"). |
| **Re-meeting cadence** | Wanted — "how long since this npc_id". |
| **Silence on the road** | Tracked — soft thought when **outdoor weather flips** mid-travel (clear→rain/snow, etc.); not every travel batch. |
| **Propaganda vs reference** | Seed prior memory from Ultima IV–VI via the sidecar button so Virtues, Lord British, and companions are familiar; do **not** name later-age factions in those seeds. |
| **Kal Lor / crime heat** | Still later. |

Keep the surface sparse: chronicle noise that never becomes a thought is fine; chat sparingly.

### Prior Britannia (inject at Context start)

Use the sidecar button **Give Avatar memories of prior Britannia** (Setup row) once per fresh Context. That ingests the three notes under [`docs/prior-memory/`](docs/prior-memory/) into Spectron as lived knowledge (Ultima IV–VI). Re-running may duplicate facts.

## Spectron (cloud or self-hosted)

The sidecar talks to Spectron’s **user/data API**. Create a Context and API key in **Surrealist** (or your self-hosted management flow). You do not need Gemini keys inside this crate — Spectron’s deployment holds provider config.

### 1. Context and key

1. Open Surrealist against your Spectron deployment.
2. Create a Context (display name can be anything) and mint a **Context API key**.
3. Copy the Context **ID** (not the display name) and the key. Never commit the key.

### 2. Export and run the sidecar

```bash
export SPECTRON_BASE_URL=https://your-spectron-host.example   # no trailing slash
export SPECTRON_CONTEXT_ID=your-context-id
export SPECTRON_API_KEY='sp-…'   # Context key from Surrealist; do not commit

# sanity check
curl -fsS -H "Authorization: Bearer $SPECTRON_API_KEY" -H 'api-version: 1' \
  "$SPECTRON_BASE_URL/api/v1/health"

cd spectron-sidecar   # from the Exult checkout root: cd spectron-sidecar
cargo run
```

Then click **Give Avatar memories of prior Britannia** once if this Context is empty. Optionally ingest [`docs/copy-protection.md`](docs/copy-protection.md) in Surrealist for Finnigan / Batlin geography.

Offline UI only (no Spectron calls): `SPECTRON_SIDECAR_DRY_RUN=1 cargo run`.

### Sidecar env vars

| Env var | Meaning |
| --- | --- |
| `SPECTRON_BASE_URL` | Spectron user API base URL (no trailing slash) |
| `SPECTRON_CONTEXT_ID` | Context **id** from Surrealist |
| `SPECTRON_API_KEY` | Context API key (`Authorization: Bearer …`); required unless dry-run |
| `SPECTRON_SIDECAR_LISTEN` | Sidecar `/event` bind (default `127.0.0.1:8765`) |
| `SPECTRON_SIDECAR_DRY_RUN` | `1` = fake replies, no API key needed |
| `SPECTRON_SIDECAR_HANDOFF` | `1` = write Exult→Spectron JSONL under `handoff/sessions/` (local debug capture only) |

### Manual event (same as Exult sends)

```bash
curl -sS -X POST http://127.0.0.1:8765/event \
  -H 'content-type: application/json' \
  -d '{"type":"talk_start","npc_id":2,"appears_as":"a boy"}'
```

Event shapes: see `src/events.rs` (`talk_*`, `book_read`, `sign_read`, `examine`, combat/sleep/dungeon, quantised `travel` with `mode` foot/cart/ship/carpet and optional `in_view` glance names, `death`, `sextant_reading`, and `time_reading` from watches/sundials).

Many events carry an optional `ambient` object: `time_of_day` (`night`, `dawn`, `day`, `dusk`), `weather` (`clear`, `snow`, `rain`, `other`), and `setting` (`dungeon`, `outdoors`). No map coordinates are sent. Travel flushes may include `in_view`: a short deduplicated list of nearby examine-style names (peripheral vision, not a tile dump).

Copy-protection Q&A for Finnigan and Batlin (manual geography facts) lives in [`docs/copy-protection.md`](docs/copy-protection.md) for Spectron ingest.
Use `npc_id` as the durable key and `appears_as` for the shape/role label. Personal names belong in `talk_line` text only after the Avatar learns them — do not rely on schedule `npc_name` data.

`sign_read` includes `text_raw` (usecode rune encoding), `text_latin` (transliteration), and `text_runes` (phonetic Unicode Runic block — not pixel-identical to Exult’s fonts.vga). `script` is `runic` / `latin` / `serpentine`. Gold plaque lettering is tagged `script=latin`. Wood / tombstone gumps default to `runic`, but uppercase-only usecode with no rune ligatures (e.g. Fellowship street plaques that reuse the woodsign gump) is retagged `latin`. Signs fire from Exult’s `display_runes` intrinsic when the plaque gump opens — double-click/use the sign, not a single examine glance.

### Exult → sidecar bridge

| Env var (on the Exult process) | Default | Meaning |
| --- | --- | --- |
| `SPECTRON_BRIDGE` | on | Set to `0` to disable POSTs |
| `SPECTRON_SIDECAR_PORT` | `8765` | Port of the sidecar `/event` listener |

## Exult itself: build and run on macOS

Rebuild after changing bridge hooks, then restart Exult (sidecar can keep running).

### Do you need Xcode?

**No full Xcode.app.** You do **not** need the App Store Xcode IDE or an Apple ID login for that.

| What | Needed? |
| --- | --- |
| **Running** `./exult` | No Apple tools at all (binary is already built) |
| **Building** Exult from source | Apple **Command Line Tools** (`clang`, `make`) — *not* the full Xcode app |
| **Sidecar** (`cargo run`) | Only a Rust toolchain (`rustup`) |

Install CLT if `clang` is missing:

```bash
xcode-select --install
```

That downloads Apple’s compiler package. It is much smaller than Xcode and does not require installing Xcode from the App Store. (Apple may still ask you to accept a license once in Terminal.)

This machine already has CLT at `/Library/Developer/CommandLineTools` and no `/Applications/Xcode.app`, which is a common preferred setup.

### Build (once)

```bash
brew install autoconf automake libtool pkg-config autoconf-archive \
  sdl2 libvorbis libpng fluidsynth

cd ..   # Exult repo root (parent of spectron-sidecar)
autoreconf -v -i   # only needed from a fresh git clone

export CFLAGS="-I/opt/homebrew/include"
export CXXFLAGS="$CFLAGS"
export CPPFLAGS="$CFLAGS"
export LDFLAGS="-L/opt/homebrew/lib"

./configure
make -j"$(sysctl -n hw.ncpu)"
```

That produces `./exult` in the repo root.

### Ultima VII static data (required to play)

Exult is only the engine. You need a legal copy of **Ultima VII** (Black Gate and/or Serpent Isle). Exult reads the game’s `STATIC` folder (copied as `static/` under its search paths).

```
…/blackgate/static/      ← Black Gate (+ Forge of Virtue on GOG)
…/serpentisle/static/    ← Serpent Isle (+ Silver Seed on GOG)
```

Typical files inside: `usecode`, `shapes.vga`, `u7map`, `initgame.dat`, …

#### From GOG.com on macOS (recommended)

GOG’s Mac builds install as DOSBox wrapper apps (usually under `/Applications`). Exult does **not** use those apps directly — you copy the embedded `STATIC` data into Exult’s default folders (from [Exult ReadMe §12.2](../docs/ReadMe.html#gog_mac)).

**1. Download and install the Mac game apps**

Purchase alone is not enough; the `STATIC` files only appear after install.

1. Install [GOG Galaxy](https://www.gog.com/galaxy) (or use a manual Mac download from your GOG library page).
2. In your library, open Ultima VII and choose the **macOS** build (not the Windows `.exe`).
3. Install:
   - **Ultima VII — The Black Gate + The Forge of Virtue**
   - **Ultima VII — Serpent Isle + The Silver Seed** (if you own it)
4. Confirm the apps exist:

```bash
ls /Applications | grep -i ultima
```

You should see names containing “Black Gate” / “Serpent Isle”. If this prints nothing, finish the Galaxy/Mac install before copying.

**2. Copy `STATIC` into Exult’s folders**

```bash
# Create Exult’s default game folders (asks for your Mac password):
sudo mkdir -p "/Library/Application Support/Exult/blackgate"
sudo mkdir -p "/Library/Application Support/Exult/serpentisle"

# Copy STATIC out of the GOG app bundles.
# Tab-complete the paths — names include a ™ and odd spacing.
sudo cp -p -R "/Applications/Ultima VII™  - The Black Gate + The Forge of Virtue.app/Contents/Resources/game/STATIC" \
  "/Library/Application Support/Exult/blackgate"

sudo cp -p -R "/Applications/Ultima VII™  - Serpent Isle + The Silver Seed.app/Contents/Resources/game/STATIC" \
  "/Library/Application Support/Exult/serpentisle"
```

**3. Check the copy, then run Exult**

```bash
ls "/Library/Application Support/Exult/blackgate/static" | head
# Expect files like usecode, shapes.vga, …

cd ..   # Exult repo root
./exult --bg --verify-files   # optional integrity check
./exult                       # menu, or: ./exult --bg / ./exult --si
```

You can keep or delete the GOG DOSBox apps afterward; Exult only needs the copied `static` trees. Savegames go to `~/Library/Application Support/exult/blackgate/` (note lowercase `exult` in your *user* Library).

**If `cp` says “No such file or directory”**

Usually either the Mac apps are not installed yet (`ls /Applications | grep -i ultima` is empty), or GOG renamed the bundle. Locate `STATIC` and copy from the real path:

```bash
ls /Applications | grep -i ultima
find /Applications -iname 'STATIC' -type d 2>/dev/null
# Also check Galaxy’s install folder if you changed it in Galaxy settings.
```

Then copy each hit, e.g.:

```bash
sudo cp -p -R "/path/to/.../STATIC" "/Library/Application Support/Exult/blackgate"
```

Black Gate vs Serpent Isle: use the Black Gate / Forge of Virtue app for `blackgate`, and the Serpent Isle / Silver Seed app for `serpentisle`.

Upstream notes (including older Complete Edition `.boxer` layouts): [docs/ReadMe.html §12.2](../docs/ReadMe.html#gog_mac).

#### Or put the data anywhere and edit the config

Config file (created on first launch):

```text
~/Library/Preferences/exult.cfg
```

Example path block — each `<path>` is the folder that **contains** `static/`, not `static` itself:

```xml
<config>
  <disk>
    <game>
      <blackgate>
        <path>/path/to/ultima7/blackgate</path>
      </blackgate>
      <serpentisle>
        <path>/path/to/ultima7/serpentisle</path>
      </serpentisle>
    </game>
  </disk>
</config>
```

More detail: [docs/ReadMe.html](../docs/ReadMe.html) §2.3 and §11.4.

## What comes next

1. Confirm Black Gate/SI launches with real `static` data
2. Run the sidecar against your Spectron Context (exports above) without `DRY_RUN`
3. Click **Give Avatar memories of prior Britannia** once on a fresh Context
4. Later: NPC distance hints from travel events

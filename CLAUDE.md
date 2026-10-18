# Folio — notes for Claude Code

Folio is a native notebook app for **Haiku** and **VitruvianOS** (V\OS: the
Haiku API on a Linux kernel), modelled on OneNote, with Obsidian-style
`[[wiki links]]` that are rewritten when the page they point to is renamed or
moved. It is written from scratch in C++17 against the Be/Haiku API. OneNote
can't be ported because its source is closed.

The history of how it got here, every decision, and what has and hasn't been
verified is in **docs/HANDOFF.md**. Read it before larger changes.

## Commands

```sh
make test                              # link-engine unit tests; any OS, no Haiku API
sh tools/typecheck/haiku-check.sh      # type-check GUI code against real Haiku headers (Linux)
make                                   # build ./Folio (on Haiku or VitruvianOS)
make deb                               # .deb for VitruvianOS (see below)
make hpkg                              # .hpkg, on Haiku R1/beta6+
python3 tools/mockups/build.py OUTDIR  # regenerate the two mockup PNGs (needs playwright)
```

On a plain Linux host (not VitruvianOS) build the link-time SDK once, then
package:

```sh
sh tools/vos-sdk/make-sdk.sh .vos-sdk      # ~4 min; VitruvianOS 0.6.2 libs from source
. .vos-sdk/sdk/env.sh && make deb          # → folio_0.1.0-1_amd64.deb
```

On VitruvianOS itself, `make` / `make deb` work directly: the headers come from
`/system/develop/headers` (0.6.x), or from `pkg-config vos` on newer builds
with `vos-dev`.

## Layout

| Path | What |
|---|---|
| `src/MarkdownScan.*` | Portable parser: `[[wiki]]`/`[md](links)`, `#tags`, `- [ ]` tasks, style spans; skips code spans and blocks. |
| `src/LinkGraph.*` | Portable model: link resolution (`FileIndex`), backlinks, search, tags, and the rename planner `PlanMoves` / `ApplyMoves`. |
| `src/Vault.*` | Disk layer (Haiku API): load/save, BFS attributes, moves, Trash (incl. XDG `.trashinfo` on V\OS), attachments, node monitoring. |
| `src/NoteView.*` | Editor, a stylable `BTextView`: live Markdown styling, Alt-click links, checkbox toggling, `[[` completion, file drops. |
| `src/MainWindow.*`, `src/Widgets.*`, `src/App.cpp` | The rest of the UI: tree, title rename, side panel tabs, menus, settings. |
| `tests/test_core.cpp` | 38 checks over the portable core (renames, folder moves, ambiguity, md links, search). |
| `packaging/` | `build-deb.sh`, `debian/{postinst,changelog,copyright}`, `haiku/{build-hpkg.sh,PackageInfo.in}`. |
| `tools/vos-sdk/` | `make-sdk.sh` + `vos-0.6.2-linkonly.patch`. |
| `tools/typecheck/` | `haiku-check.sh`. |
| `tools/mockups/` | Mockup generator (HTML in Haiku style → PNG). |

Storage model: notebooks are top-level folders, sections are sub-folders (any
depth), pages are `.md` files, and attachments go in `<notebook>/attachments/`.
The default notebooks folder is `~/Notebooks`. Settings are in
`B_USER_SETTINGS_DIRECTORY/Folio_settings`.

## Rules that matter

* **Keep `MarkdownScan` and `LinkGraph` free of Haiku headers.** They are the
  only code that can be compiled and run here, so they carry the logic and the
  tests. Add a test in `tests/test_core.cpp` for any change to link
  resolution or rename rewriting.
* The rename planner rewrites **only the target range** of a link, keeping
  `#heading`, `|alias`, `!` and any `.md` extension as written. It leaves
  links that were already broken alone. When a basename becomes ambiguous it
  writes the shortest path that still resolves.
* Code style follows Haiku: tabs, the return type on its own line,
  `fMember`, and `_PrivateMethod`.
* Node-monitor messages differ by system. Haiku sends
  `device`/`node`/`directory` ints, while V\OS sends a `virtual:node`
  node_ref and `virtual:…directory` entry_refs. `Vault::HandleNodeMonitor`
  handles both, with a path fallback. Don't simplify that away.
* On V\OS, attributes are `user.beos.*` xattrs, and **dpkg drops xattrs**. The
  package ships them in `/usr/share/folio/apps_attrs.tar`, and `postinst`
  restores them with `getfattr`/`setfattr`, mirroring V\OS's own package.
* `xres`/`resattr` need **absolute paths** (the host-built tools fail on
  relative ones). Strip **before** `xres`: resources are appended after the
  ELF data, so stripping afterwards would remove them. `build-deb.sh` strips
  at link time with `-s`.
* `CXXFLAGS` from the command line are additive (`override CXXFLAGS +=` in the
  Makefile). Keep it that way so packagers can pass hardening flags.

## Status

v0.1.1 is published at github.com/idumych/folio with a .deb for
VitruvianOS 0.6.x and an .hpkg for Haiku R1/beta6 (`make hpkg`, built on
Haiku). Both were installed and tested in VirtualBox. See docs/HANDOFF.md
§5 for what was verified and §7 for the next steps.

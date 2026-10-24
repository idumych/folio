# Folio — notes for Claude Code

Folio is a native notebook app for **Haiku**, **VitruvianOS** (V\OS, a
BeOS-like OS based on Linux) and plain **Linux** via Cosmoe (the Haiku API
as a library on X11/Wayland), modelled on OneNote, with Obsidian-style
`[[wiki links]]` that are rewritten when the page they point to is renamed or
moved. It is written from scratch in C++17 against the Be/Haiku API. OneNote
can't be ported because its source is closed.

The history of how it got here, every decision, and what has and hasn't been
verified is in **docs/HANDOFF.md**. Read it before larger changes.

## Commands

```sh
make test                              # link-engine unit tests; any OS, no Haiku API
sh tools/haiku-typecheck.sh            # type-check GUI code against real Haiku headers (Linux)
make                                   # build ./Folio (on Haiku or VitruvianOS)
make deb                               # .deb for VitruvianOS (see below)
make hpkg                              # .hpkg, on Haiku R1/beta6+
sh packaging/appimage/build-appimage.sh  # Linux AppImage (builds Hokku; releases come from CI: appimage.yml)
python3 tools/mockups/build.py OUTDIR  # regenerate the two mockup PNGs (needs playwright)
```

Release `.deb`s (amd64 and arm64) are built by `.github/workflows/deb.yml` in
a Debian trixie container, never on the host (glibc must match V\OS's 2.41).
On a Debian trixie host or container you can build the link-time SDK and
package locally:

```sh
sh tools/vos-sdk/make-sdk.sh .vos-sdk      # VitruvianOS 0.6.2 libs from source
. .vos-sdk/sdk/env.sh && make deb          # → folio_<version>-1_<arch>.deb
```

On VitruvianOS itself, `make` / `make deb` work directly: the headers come from
`/system/develop/headers` (0.6.x), or from `pkg-config vos` on newer builds
with `vos-dev`.

For building, packaging, releasing or testing on Renku OS, VitruvianOS or
Linux (Cosmoe), including driving the VirtualBox test VMs, use the
**beos-app-platforms** skill (github.com/idumych/beos-app-platforms, installed
per user in `~/.claude/skills/`, not shipped in this repo; Folio's specifics
are in its `references/folio.md`).

## Layout

| Path | What |
|---|---|
| `src/MarkdownScan.*` | Portable parser: `[[wiki]]`/`[md](links)`, `#tags`, `- [ ]` tasks, style spans; skips code spans and blocks. |
| `src/LinkGraph.*` | Portable model: link resolution (`FileIndex`), backlinks, search, tags, and the rename planner `PlanMoves` / `ApplyMoves`. |
| `src/Vault.*` | Disk layer (Haiku API): load/save, BFS attributes, moves, Trash (incl. XDG `.trashinfo` on V\OS), attachments, node monitoring. |
| `src/NoteView.*` | Editor, a stylable `BTextView`: live Markdown styling, Alt-click links, checkbox toggling, `[[` completion, file drops. |
| `src/MainWindow.*`, `src/Widgets.*`, `src/App.cpp` | The rest of the UI: tree, title rename, side panel tabs, menus, settings. |
| `tests/test_core.cpp` | 38 checks over the portable core (renames, folder moves, ambiguity, md links, search). |
| `haiku-app.env` | App identity, version and package metadata for the release pipeline in the beos-app-platforms skill; `packaging/`, `.github/workflows/` and `tools/` are copies of its `templates/`. |
| `packaging/` | `build-deb.sh`, `build-hpkg.sh`, `description.txt`, `debian/{postinst.in,changelog}`, `appimage/` (build script, AppRun.in, fonts.conf, icon, `x11-smoke.sh`, `wayland-smoke.sh`). The scripts are template copies; change `haiku-app.env`, not them. |
| `release/test-plan.md` | Folio's feature checks for the release test on each platform. |
| `tools/vos-sdk/` | `make-sdk.sh` + `vos-0.6.2-linkonly.patch`. |
| `tools/haiku-typecheck.sh` | Type-check against Haiku headers (files from `TYPECHECK_SRCS`). |
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
  `device`/`node`/`directory` ints, V\OS sends a `virtual:node`
  node_ref and `virtual:…directory` entry_refs, and Cosmoe sends
  dev_t/ino_t-typed fields with the entry's **full path** as `name`.
  `Vault::HandleNodeMonitor` handles all three, with a path fallback.
  Don't simplify that away.
* Cosmoe-specific code is under `#ifdef FOLIO_COSMOE` (set by the Makefile):
  its `watch_node()` is unsupported, so `Vault::_Watch` uses `watch_path()`
  and re-watches moved folders; files/URLs open via `xdg-open`. Its `entry_ref::name` may be a full path, so
  take `md::Basename()` of it.
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

v0.1.5 is published at github.com/idumych/folio with .debs for
VitruvianOS 0.6.x (amd64 and arm64, built in CI on Debian trixie), an .hpkg
for Haiku R1/beta6 (x86_64, built in the Haiku VM; Haiku has no arm64 target
yet), and Linux AppImages for x86_64 and aarch64 built with Hokku on Ubuntu
24.04 in CI. All came through the release gate: Haiku and V\OS amd64 tested
in VirtualBox, Linux x86_64 under Xvfb + Openbox; both AppImages started under
X11 and Wayland in CI (aarch64 on GitHub's arm64 runners); the arm64 .deb was
checked statically only.
See docs/HANDOFF.md §5 for what was verified and §7 for the next steps.

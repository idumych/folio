# Handoff: how Folio got here

This file is the memory of the conversation in which Folio was built, from
October 15–17, 2026, in a cloud session with no Haiku or VitruvianOS machine.
It records what the user asked for, the decisions made and why, what has
actually been verified, and what is still open.

## 1. What the user asked for, in order

1. **"Port Microsoft OneNote 365 to Haiku, and add hyperlinks like Obsidian
   uses, including the way the links dynamically update if you rename the
   note they link to."**
   OneNote is closed source, so the answer was an original native app with
   the same model. Before building, the user picked:
   * **Editing surface:** linear rich text, not OneNote's free-form canvas.
   * **Storage:** Markdown files plus BFS attributes, laid out as notebook =
     folder, section = sub-folder, page = `.md`. This is Obsidian-compatible.
   * **Extras:** all four were chosen: backlinks panel, full-text search,
     images/attachments, and tags/to-do checkboxes.
2. **"I want to run this on Vitruvian OS instead."** Same source, ported to
   VitruvianOS (see §3).
3. **"Generate a couple of mockup screenshots."** Done:
   `screenshots/folio-main-window.png` and
   `screenshots/folio-links-and-todos.png`, made by `tools/mockups/build.py`.
   They are drawn to match the code's styling rules; they are not captures of
   a running app.
4. **"Build a deb."** Done: `folio_0.1.0-1_amd64.deb` for V\OS 0.6.x (see §4).
5. **"Make a zip with the memory of this conversation and the source so we can
   continue from Claude Code."** This bundle, which added `CLAUDE.md`, this
   file, and `tools/`.

## 2. What Folio does

* A tree of notebooks, sections and pages, a page editor with live Markdown
  styling, and a side panel with **Backlinks / Tags / Search** tabs.
* **Links:** `[[Page]]`, `[[Page#Heading]]`, `[[Page|alias]]`, `![[img.png]]`
  and `[text](rel/path.md)`. Type `[[` for a page picker. Alt-click or
  Alt+Enter follows a link, and following a broken (red) link creates the
  page.
* **Rename rewriting** covers renaming a page from its title, moving a page,
  renaming or moving whole sections and notebooks (including relative md links
  inside moved pages), and renames done in Tracker or a terminal (via the node
  monitor). It also handles name collisions: links that become ambiguous are
  rewritten to a path such as `[[Home/Ideas]]`.
* **To-dos and tags:** click a box to toggle it, Alt+T turns a line into a
  to-do, and Enter continues a list. `#tags` and `#nested/tags` are counted in
  the Tags tab. Search understands words, `"phrases"`, `#tag`/`tag:x`,
  `path:x`, and `todo`/`done`.
* **Attachments:** dropped or attached files are copied to
  `<notebook>/attachments/` and shown as a thumbnail strip under the page.
* **Other:** autosave about 0.9 s after typing stops, back/forward history,
  Trash with restore support, and a Welcome notebook on first run.
* `FOLIO:tags` and `FOLIO:open_tasks` attributes are written on every page,
  and indexes are created for Tracker queries.

## 3. VitruvianOS: what was learned and changed

Sources: v-os.dev FAQ and blog, the Linuxiac 0.3 article, and the source at
github.com/VitruvianOS/Vitruvian.

* V\OS runs Haiku apps through **source compatibility**, not binary: apps are
  recompiled. It is Debian **trixie**-based (glibc 2.41, ICU 76) and uses ext4
  with xattrs. Releases are tagged up to **0.6.2**; main reports 0.6.3.
* **No indexing or live queries yet.** They are on the roadmap. Folio's
  attributes are written anyway; its own Search tab doesn't need them.
* **Node monitor:** the `nexus` kernel module sends a `virtual:node`
  node_ref, `virtual:directory` / `virtual:from directory` / `virtual:to
  directory` entry_refs, plus `name`/`from name`, and int64
  `from directory`/`to directory` on moves. `Vault::HandleNodeMonitor` now
  handles this format and Haiku's.
* **Trash** follows the XDG spec (`files/` + `info/`); on V\OS 0.6.2 the
  Trash dir is `~/config/data/Trash`, not `~/.local/share/Trash`.
  `Vault::Trash` writes a `.trashinfo` when the Trash dir is named `files`,
  and also `_trk/original_path`.
* **Headers and dev files:** 0.6.2 ships headers in `/system/develop/headers`
  and has **no vos-dev or vos.pc**. Main adds a `vos-dev` package with
  `pkg-config vos`. The Makefile supports both.
* **Apps** live in `/system/apps`. Deskbar menu entries are symlinks in
  `/system/data/deskbar/menu/Applications/`.
* **App attributes** are `user.beos.*` xattrs. V\OS's own package ships them
  in a tarball of empty placeholder files and restores them in postinst.
  Folio does the same.

## 4. The .deb

* Built by `packaging/build-deb.sh` (`make deb`). Version `0.1.0-1`,
  architecture amd64, Section editors.
* **Contents:** `/system/apps/Folio`, the Deskbar symlink, `/usr/bin/folio` →
  the app, `/usr/share/folio/apps_attrs.tar`, and the usual docs.
* **Depends:** `vos (>= 0.6.2), vos (<< 0.7~), libc6 (>= 2.38),
  libstdc++6 (>= 11), libgcc-s1 (>= 3.0), attr`. The upper bound exists
  because the Haiku API is C++ and V\OS is pre-1.0, so class layouts may
  change. **Rebuild for 0.7.**
* **Linking:** because no V\OS libraries could be downloaded,
  `tools/vos-sdk/make-sdk.sh` builds libroot, libbe, libtranslation,
  libtracker and libtextencoding **from the 0.6.2 sources**, using a small
  link-only patch. The patch builds only system/libs/kits, skips OS-image
  packaging and the nexus DKMS packaging step, and drops the bluetooth, game,
  media2, network, opengl and screensaver kits.
* **Placeholder libraries:** libtracker needs libgame, which needs libmedia2
  (PipeWire/GStreamer), so empty placeholders with those names stand in.
  libgame's one symbol used by libtracker, `set_mouse_position`, is defined
  in its placeholder.
* **None of these libraries ship in the .deb.** They only let the linker
  prove every symbol resolves. The libraries have no symbol versioning, so
  runtime binding is by name.
* **Hardening:** PIE, full RELRO, BIND_NOW, NX stack, stack protector,
  FORTIFY_SOURCE=2, no RPATH. The binary is stripped before `xres`.
* **Placeholders the user should confirm:**
  * License is **MIT** (`LICENSE`, `packaging/debian/copyright`), picked as
    a placeholder.
  * Maintainer is "Folio maintainer <19998719+idumych@users.noreply.github.com>". Override it with
    `DEBFULLNAME`/`DEBEMAIL`.

## 5. Verification: done vs. not done

| Check | Result |
|---|---|
| Portable core unit tests (`make test`) | 38/38 pass (also run under ASan/UBSan early on) |
| GUI code vs. real Haiku headers (`haiku-check.sh`) | clean, all 5 files |
| GUI code vs. VitruvianOS headers (main and 0.6.2) | compiles cleanly, both build paths |
| Link vs. V\OS 0.6.2 libraries | 565 imported symbols, all resolve (500 from V\OS libs; 3 standard weak) |
| Resources survive stripping | yes, signature and file types appended after ELF data |
| postinst attribute restore | 5 attrs restored, tested with getfattr/setfattr stand-ins in a scratch root (the real attr tools weren't available) |
| dpkg install/remove (scratch root) | clean; refuses to install without `vos`, as it should |
| `make-sdk.sh` from an empty dir → `make deb` | works, about 4 min; same dynamic symbols as the delivered .deb |
| **Running on Haiku** | **not done** |
| **Running on VitruvianOS** | **works** (2026-10-18, live 0.6.2 image in VirtualBox): release .deb installs via apt, postinst restores all 5 attrs, Deskbar entry and `folio` command present, Welcome notebook created, styling and backlinks render, rename via Page ▸ Rename and via `mv` in Terminal both rewrite `[[…]]`, `[[…#heading]]` and `[[…\|alias]]` links; no stderr output. Command key is Ctrl. Mouse pass (driven through the VirtualBox API): tree context menu (New Section, Move To with links intact, Show in Tracker, Move to Trash with missing-link warning), Ctrl-click on a red link creates the page, checkbox click, Tags and Search tabs, Attach File via BFilePanel (thumbnail shown), drag-and-drop from Tracker (deduplicated to `sample 2.png`), `FOLIO:tags`/`FOLIO:open_tasks` xattrs, `.trashinfo` written and Tracker *Restore* works. All pass. |
| lintian | not run (unavailable); expect complaints about non-FHS `/system`, which is normal for V\OS |

## 6. Known limits (v0.1)

* Pages are flowing text, not OneNote's free-form canvas.
* Images appear in the strip under the page, not inline.
* Renaming a heading doesn't update `[[Page#Heading]]` links.
* An open page isn't reloaded when another app edits it; switching away and
  back reloads it.
* Name matching is case-insensitive for ASCII only.
* No app icon, no arm64 build, and the V\OS keymap decides which key acts as
  "Alt"/command.

## 7. Suggested next steps

1. **First real run on Haiku** (`make`). V\OS 0.6.2 is fully exercised
   (see §5).
2. Use the result to fix whatever breaks. Keep logic changes in the portable
   core, with tests.
3. **Inline images** in the editor (BTextView can't do it; it needs a custom
   view or drawing over the text).
4. **Heading-rename rewriting:** extend `PlanMoves` with a heading map.
5. An app icon (HVIF in `Folio.rdef`), an arm64 build, and a lintian pass on
   a Debian box.

## 8. Environment notes for whoever builds next

* The cloud session could reach **GitHub** and PyPI/npm, but **not** apt
  mirrors, GitHub release downloads, or v-os.dev. That is why `make-sdk.sh`
  fetches flex, ICU, xkbcommon and systemd headers from GitHub when the host
  lacks them.
* flex has no bootstrap scanner in git, so the script builds flex 2.6.4 using
  `scan.c` from winflexbison, whose `scan.l` is checked to be identical.
* On a normal Debian/Ubuntu box with apt you can skip most of that: install
  `flex libicu-dev libxkbcommon-dev libudev-dev libsystemd-dev libblkid-dev
  libfreetype-dev` and the script uses the system copies.

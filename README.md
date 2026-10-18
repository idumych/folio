# Folio

A native Haiku notebook app in the spirit of OneNote, with Obsidian-style
`[[wiki links]]` that rewrite themselves when you rename or move the page they
point to. It is written from scratch against the Be/Haiku API, since OneNote's
own source is closed.

![Folio on VitruvianOS: a project page with wiki links, to-dos and tags, and its backlinks in the side panel](screenshots/vitruvianos-links.png)

![Folio on VitruvianOS: meeting notes, with a search listing every open to-do across notebooks](screenshots/vitruvianos-todos.png)

*Folio 0.1.0 running on VitruvianOS 0.6.2.*

## Build and run

One source tree builds on both **Haiku** and **VitruvianOS** (V\OS), which
runs Haiku applications recompiled from source on a Linux kernel. The
Makefile detects which system it is on.

**Haiku** (R1/beta5 or a nightly, with `haiku_devel`):

```sh
make && ./Folio
```

**VitruvianOS**: the quickest way is the package. Download
`folio_0.1.0-1_amd64.deb` from the
[Releases page](https://github.com/idumych/folio/releases), then:

```sh
sudo apt install ./folio_0.1.0-1_amd64.deb
```

This puts Folio in `/system/apps`, adds it to the Deskbar's Applications
menu, and adds a `folio` command. It is built for VitruvianOS 0.6.x on
amd64 and needs a rebuild for 0.7.

To build from source on VitruvianOS:

```sh
sudo apt install build-essential
make && ./Folio
```

VitruvianOS 0.6.x ships its headers in `/system/develop/headers`, and the
Makefile uses them from there. Newer builds that have the `vos-dev` package
are picked up through `pkg-config vos` instead. The Makefile then links
`libbe`, `libroot`, `libtracker` and `libtranslation`. It embeds the app
signature with `rc`, `xres` and `resattr`; VitruvianOS keeps it both as
resources and as attributes.

`make deb` (or `sh packaging/build-deb.sh`) builds the package. On
VitruvianOS the attributes are Linux extended attributes, which dpkg
doesn't preserve, so the package carries them in
`/usr/share/folio/apps_attrs.tar`. Its install script copies them onto the
installed binary, the same way VitruvianOS's own package does for the
bundled apps.

Both systems also support `make install` and `make test`. The tests cover
the link engine only, so they run on any OS.

On first launch Folio creates `~/Notebooks` with a short **Welcome** notebook
that walks through the features. Use *File ▸ Open Notebooks Folder…* to point
it at any other folder, including an existing Obsidian vault.

### Differences on VitruvianOS

* **Renames in other apps.** Folio reads both systems' file-change
  messages. Haiku identifies files by device and inode numbers, while
  VitruvianOS sends `virtual:node` and directory `entry_ref`s. If a node
  can't be identified, Folio falls back to the file's path. Renames made
  in Tracker or a terminal update links on both systems.
* **Tags and to-dos in Tracker.** `FOLIO:tags` and `FOLIO:open_tasks` are
  stored as Linux extended attributes. ext4, the default, supports them.
  VitruvianOS doesn't have file indexing or live queries yet; its roadmap
  lists them next. Until then Tracker queries won't find these attributes,
  but Folio's own Search tab works the same as on Haiku.
* **Trash.** VitruvianOS uses the freedesktop.org Trash layout, kept in
  `~/config/data/Trash` (`files/` plus `info/`). *Move to Trash* writes the
  matching `.trashinfo` record, so Tracker's *Restore* puts the page back
  where it was.
* **Web links.** These open through the `application/x-vnd.Be.URL.*`
  handlers, so they need VitruvianOS's URL wrapper to be set up.
* **Shortcuts.** The README says "Alt" because that is the default
  command key on Haiku. On VitruvianOS's default keymap the command key
  is Ctrl, so Ctrl+R renames, Ctrl+N makes a new page, and so on.

## How it is organised

| OneNote  | Folio                        | On disk                                |
|----------|------------------------------|----------------------------------------|
| Notebook | top-level folder             | `~/Notebooks/Work/`                    |
| Section  | sub-folder (any depth)       | `~/Notebooks/Work/Meetings/`           |
| Page     | Markdown file                | `~/Notebooks/Work/Meetings/Standup.md` |
| Images / files | attachments            | `~/Notebooks/Work/attachments/`        |

Pages are plain UTF-8 Markdown, so other editors, `grep`, Git and Obsidian
can all use them. Folio also writes two indexed BFS attributes on every page:
`FOLIO:tags` and `FOLIO:open_tasks`. That lets Tracker queries find, for
example, every page tagged `#project` or every page with open to-dos.

## Links

* `[[Page]]`, `[[Page#Heading]]`, `[[Page|shown text]]`, `![[image.png]]`
  (embed), and ordinary `[text](relative/path.md)` Markdown links all work.
* Type `[[` to pick a page from a menu.
* **Alt-click** a link to follow it, or put the cursor in it and press
  **Alt+Enter**. Following a link to a page that doesn't exist (shown in red)
  creates it.
* The **Backlinks** tab lists every page that links to the current one, with
  the line around each link.

### What happens when you rename

When you rename or move something, Folio first works out what every link in
the notebooks folder pointed to, then rewrites only the part of each link
that names the target. The rest is kept as written: `#heading`, `|alias`, the
`!` of an embed, and a `.md` extension if you wrote one.

This covers:

* renaming a page (edit its title above the page and press Enter, or Alt+R);
* moving a page to another section or notebook (*Page ▸ Move To*, or the
  tree's context menu);
* renaming or moving a whole section or notebook, which updates every link
  into and out of it, including relative `[x](../Other.md)` links in the
  moved pages themselves;
* renames and moves done in **Tracker or Terminal** while Folio is running,
  caught through Haiku's node monitor;
* name collisions. If renaming makes `[[Ideas]]` ambiguous, links that meant
  the other page are rewritten to a path such as `[[Home/Ideas]]`.

Like Obsidian, links are written in the shortest form that is still
unambiguous, and links that were already broken are left alone.

## Pages

* Live Markdown styling: headings, **bold**, *italic*, `code`, code blocks,
  quotes, links, #tags. The Markdown syntax characters are dimmed rather
  than hidden.
* **To-dos:** click a `[ ]` box to tick it. Alt+T turns any line into a
  to-do, and Enter continues a to-do, bullet or numbered list.
* **Tags:** `#tag` and `#nested/tag`. The **Tags** tab lists them with
  counts; click one to search for it.
* **Search** (Alt+F): words, `"exact phrases"`, `#tag` or `tag:x`, `path:x`,
  and `todo` / `done` to list to-dos across all notebooks.
* **Attachments:** drop files onto a page or use *Page ▸ Attach File…*
  (Alt+I). Files are copied into the notebook's `attachments` folder and
  linked; images are embedded. Everything a page links to appears as
  thumbnails under the page. Click a thumbnail to open it.
* Pages autosave about a second after you stop typing.
* Back and forward with Alt+[ and Alt+]. *Move to Trash* uses the real
  Trash, and Tracker can restore from it.

## Shortcuts

| Keys            | Action                     |
|-----------------|----------------------------|
| Alt+N           | New page                   |
| Alt+Shift+N     | New section                |
| Alt+R           | Rename page                |
| Alt+K           | Insert link                |
| Alt+T           | To-do on/off               |
| Alt+I           | Attach file                |
| Alt+Enter       | Follow link at cursor      |
| Alt+F           | Search                     |
| Alt+[ / Alt+]   | Back / forward             |
| Alt+J           | Show/hide the side panel   |

## Code layout

* `src/MarkdownScan.*` finds links, tags, to-dos and styling spans; it skips
  code spans and code blocks.
* `src/LinkGraph.*` is the in-memory index. It handles link resolution,
  backlinks, search, tags, and the rename planner (`PlanMoves`).
* `src/Vault.*` handles the files on disk: loading, saving, BFS attributes,
  moves, Trash, attachments and node monitoring.
* `src/NoteView.*` is the editor, a stylable `BTextView`.
* `src/MainWindow.*`, `src/Widgets.*` and `src/App.cpp` are the rest of the
  user interface.

The first two use only portable C++ and are covered by `tests/test_core.cpp`.

Tools for working on a Linux host:

* `tools/typecheck/haiku-check.sh` type-checks the user-interface code
  against Haiku's real headers.
* `tools/vos-sdk/make-sdk.sh` builds VitruvianOS 0.6.2's libraries from
  source, so `make deb` works without a VitruvianOS machine.
* `tools/mockups/` regenerates the mockup screenshots.

`docs/HANDOFF.md` records the design decisions and what has been verified.

## Known limits (v0.1)

* Pages are flowing rich text, not OneNote's free-form canvas.
* Images appear in the strip under the page, not inline in the text.
* Renaming a *heading* does not yet update `[[Page#Heading]]` links.
* A page that is already open is not reloaded if another app edits it.
  Switching away and back picks up the change.
* Case-insensitive name matching covers ASCII letters only.
* There is no app icon yet.
* Folio has been built and linked against VitruvianOS 0.6.2's own
  libraries, but not yet run on it. VitruvianOS is itself pre-1.0.

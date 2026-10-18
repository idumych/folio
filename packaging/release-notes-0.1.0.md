First public build of Folio, a native notebook app for VitruvianOS and Haiku in the spirit of OneNote, with Obsidian-style `[[wiki links]]` that are rewritten when the page they point to is renamed or moved.

> **Pre-release:** Folio has not yet been run on real VitruvianOS or Haiku hardware. The GUI code is type-checked against the real Haiku and VitruvianOS headers, linked against VitruvianOS 0.6.2 libraries built from source, and the package has been test-installed with dpkg. Bug reports from a first run are very welcome.

## Install (VitruvianOS 0.6.x, amd64)

```sh
sudo apt install ./folio_0.1.0-1_amd64.deb
```

This installs Folio to `/system/apps`, adds it to the Deskbar's Applications menu, and adds a `folio` command. It is built for VitruvianOS 0.6.x and needs a rebuild for 0.7.

On Haiku, build from source: `make && ./Folio`.

## What's in it

* Notebooks, sections and pages in a tree. Pages are stored as plain Markdown files under `~/Notebooks`.
* `[[wiki links]]` and Markdown links that are rewritten when a page, section or notebook is renamed or moved, including renames made in Tracker or a terminal.
* Backlinks, `#tags`, to-do checkboxes, full-text search, and drag-and-drop attachments.

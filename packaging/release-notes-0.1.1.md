Folio is a native notebook app for Haiku and VitruvianOS in the spirit of OneNote, with Obsidian-style `[[wiki links]]` that are rewritten when the page they point to is renamed or moved.

This release adds a **Haiku package** and fixes the window size on small screens.

## Install

**Haiku R1/beta6 or later (x86_64):**

```sh
pkgman install ./folio-0.1.1-1-x86_64.hpkg
```

**VitruvianOS 0.6.x (amd64):**

```sh
sudo apt install ./folio_0.1.1-1_amd64.deb
```

Either way, Folio is added to the Deskbar's Applications menu and a `folio` command is installed. The command key is **Alt** on Haiku and **Ctrl** on VitruvianOS (Alt-click or Ctrl-click follows a link).

## Changes since 0.1.0

* New: Haiku package, built and tested on R1/beta6 (hrev59866).
* Fixed: the main window no longer runs off screens smaller than about 1240×820, and a saved window size from a bigger display is shrunk to fit.
* Fixed: `make install` used a `finddir` constant that doesn't exist.

## Tested

Both packages were installed and tested in VirtualBox:

* **Haiku R1/beta6:** `pkgman` install, the Deskbar entry, first launch, renaming a page in Folio and with `mv` in Terminal (links, `#heading` and `|alias` rewritten), Alt-click on a missing page to create it, to-dos, attachments through the file panel, Move to Trash (restorable from Tracker), and `FOLIO:tags` / `FOLIO:open_tasks` attributes found by Tracker queries.
* **VitruvianOS 0.6.2:** everything above, plus moving pages, drag and drop from Tracker, and restoring from the Trash.

## What's in it

* Notebooks, sections and pages in a tree. Pages are stored as plain Markdown files under `~/Notebooks`.
* `[[wiki links]]` and Markdown links that are rewritten when a page, section or notebook is renamed or moved, including renames made in Tracker or a terminal.
* Backlinks, `#tags`, to-do checkboxes, full-text search, and drag-and-drop attachments.

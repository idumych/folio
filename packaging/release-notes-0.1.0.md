First public build of Folio, a native notebook app for VitruvianOS and Haiku in the spirit of OneNote, with Obsidian-style `[[wiki links]]` that are rewritten when the page they point to is renamed or moved.

> **Tested on VitruvianOS 0.6.2** (amd64, in VirtualBox): installing this package, the Deskbar entry, first launch, page and link renames (in Folio and with `mv` in Terminal), moving pages, to-dos, tags, search, attachments via the file panel and by drag and drop from Tracker, and Move to Trash with restore from Tracker. Not yet run on Haiku; bug reports from there are very welcome.
>
> On VitruvianOS the command key is **Ctrl**: where the docs say Alt, use Ctrl (Ctrl+R renames, Ctrl-click follows a link).

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

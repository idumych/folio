Folio is a native notebook app for Haiku, VitruvianOS and Linux in the spirit of OneNote, with Obsidian-style `[[wiki links]]` that are rewritten when the page they point to is renamed or moved.

This release adds **Linux**: an AppImage that runs on any x86_64 desktop, built on [Cosmoe](https://github.com/haydentech/cosmoe), which brings the Haiku API to X11 and Wayland.

## Install

**Linux (x86_64; needs glibc 2.38 and GCC 13's libstdc++ or newer: Ubuntu 24.04, Fedora 39, Debian 13 and later):**

```sh
chmod +x Folio-0.1.2-x86_64.AppImage
./Folio-0.1.2-x86_64.AppImage
```

**Haiku R1/beta6 or later (x86_64):**

```sh
pkgman install ./folio-0.1.2-1-x86_64.hpkg
```

**VitruvianOS 0.6.x (amd64):**

```sh
sudo apt install ./folio_0.1.2-1_amd64.deb
```

The command key is **Alt** on Haiku and **Ctrl** on VitruvianOS and Linux.

## Changes since 0.1.1

* New: Linux AppImage, built with Cosmoe 0.9.0 (`v0.9.0_20261013`).
* Fixed: an attachment added through the file panel could be named after its full path on systems whose `entry_ref` carries one.
* The Attach File panel has its own title.

## Tested

* **Linux** (CachyOS, X11): first launch, renaming pages in Folio and with `mv` while it runs (including renaming a section folder, then a page inside it), following a link to a missing page, to-dos, attachments through the file panel, Move to Trash into the desktop Trash with a `.trashinfo` record, Show in File Manager, and the `FOLIO:*` attributes. The Wayland backend and dragging files in from a Linux file manager are not tested yet. Cosmoe doesn't draw underlined or struck-through text, so links are shown in colour and done to-dos in grey.
* **Haiku R1/beta6** and **VitruvianOS 0.6.2**: the 0.1.2 packages install and run; the full test pass from 0.1.1 still applies, as the changes there are small.

The AppImage bundles Cosmoe (MIT licence) and the libraries it needs, such as ICU, pango and cairo; their licences are in `usr/share/doc` inside it.

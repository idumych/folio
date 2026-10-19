# VitruvianOS (V\OS)

V\OS runs the Haiku API on a Linux kernel, and its userland is Debian trixie. Folio ships as a `.deb` installed to `/system/apps`.

## Build the .deb on the host

1. **Link-time SDK.** Build it once (about 4 min), from source, VitruvianOS 0.6.2:
   ```sh
   sh tools/vos-sdk/make-sdk.sh .vos-sdk
   ```
   It needs `git cmake ninja g++ gcc bison m4 make pkg-config`. Note that a fish/zsh `ninja` *alias* isn't enough; the binary must exist.
   On Arch-family hosts the compiler triple is `x86_64-pc-linux-gnu`, which VitruvianOS's `engine.cmake` doesn't know. The script passes `-DVOS_DYNAMIC_LINKER=/lib64/ld-linux-x86-64.so.2` for that.
2. **Package.** This needs `dpkg-deb` (`pacman -S dpkg` on Arch):
   ```sh
   sh -c '. .vos-sdk/sdk/env.sh && make deb'      # → folio_<v>-1_amd64.deb
   ```
3. **Check the result:**
   - `dpkg-deb -I` and `dpkg-deb -c`: control fields, plus the files `/system/apps/Folio`, the Deskbar link and `/usr/bin/folio`.
   - The app signature is embedded (resources survive because `build-deb.sh` strips at link time):
     `dpkg-deb --fsys-tarfile x.deb | tar -xO ./system/apps/Folio | strings -a | grep -c x-vnd.Folio`
   - The attributes travel in `/usr/share/folio/apps_attrs.tar`, because dpkg drops xattrs; `postinst` restores them.
   - glibc must suit Debian trixie:
     `objdump -T Folio | grep -oE '(GLIBC|GLIBCXX)_[0-9.]+' | sort -Vu`
     GLIBC ≤ 2.41 is fine; the build so far needs 2.38.

## Make a test VM

The images are raw **UEFI** disk images (`vitruvian-0.6.2-amd64.raw` in `~/Downloads`; there are 0.6.3 nightlies there too):

```sh
sh .claude/skills/folio-platforms/scripts/make-vm.sh folio-vos-test ~/Downloads/vitruvian-0.6.2-amd64.raw efi
```

The live session runs at 1280×800.

- **Boot:** "Welcome" screen → **Try It Now** (Enter, or a click at about (900,582)). An "Installing AudioMixer in Deskbar failed" alert appears because audio is off in the VM; press Enter.
- **User:** `vos-live`, logged in automatically.
- **Don't** try the text consoles (Ctrl+Alt+F2) and **don't** read the image's `/etc/shadow` to find a password: auto mode blocks it as credential exploration. Use the desktop session instead.
- **Terminal:** click the Deskbar "VitruvianOS" title at (1210,11), Applications (1040,226), slide left (880,226), then Terminal at (885,607). Folio itself is at (875,407) in the same menu once installed.
- **Persistence:** what you write in the live session survives a VM restart (the VDI is writable), but the welcome screen appears again on each boot.
- **Command key is Ctrl.** Alt+R types an "r" into the page, so undo it with Backspace `k:0e,8e`. Ctrl+Tab doesn't switch windows: quit Folio (Ctrl+Q) to get back to the Terminal, or click it.
- No `wget`, no `file`; use `curl`.
- **SSH:** none set up. Use keyboard, mouse and screenshots, and the GitHub release URLs for files.

## Test checklist

In the guest Terminal:

```sh
cd /tmp && curl -fsSLO https://github.com/idumych/folio/releases/download/v<v>/folio_<v>-1_amd64.deb
sha256sum folio_*.deb                       # compare with SHA256SUMS
sudo apt install -y ./folio_<v>-1_amd64.deb # passwordless sudo in the live session
dpkg -s folio | grep Version
getfattr -d -m user.beos /system/apps/Folio | grep -c beos   # expect 5
(folio > /tmp/folio.log 2>&1 &)
```

Then the core feature test from SKILL.md (Ctrl+R = `k:1d,13,93,9d`), plus:

- **Follow a red link:** Ctrl-click it (`m:ctrlclick,x,y`). It should create the page.
- **Right-click a tree item:** New Section, Move To (links stay valid), Show in Tracker (opens the folder), Move to Trash.
- **Trash:** the file goes to `~/config/data/Trash/files`, with a freedesktop `.trashinfo` in `info/` (a URL-encoded `Path=` and a `DeletionDate=`). Tracker's Trash window offers **Restore**, which should put the page back.
- **Attach:** Ctrl+I opens the file panel. The panel shows "0 bytes" for files; that's a VitruvianOS display quirk, not Folio. Dragging a file from a Tracker window into the editor should attach it, and a duplicate name becomes `name 2.png`.
- **Attributes:** `FOLIO:tags` and `FOLIO:open_tasks` are `user.beos.FOLIO:*` xattrs. VitruvianOS has no indexing yet, so there are no queries.

## Known VitruvianOS differences that Folio handles

- **Node-monitor messages** carry `virtual:node` node_refs and `virtual:…directory` entry_refs (see `Vault::HandleNodeMonitor`).
- **Trash location:** `find_directory(B_TRASH_DIRECTORY)` gives `~/config/data/Trash/files`, which isn't the XDG `~/.local/share/Trash`. So GNOME and KDE file managers don't see those files.
- **The package pins** `vos (>= 0.6.2), vos (<< 0.7~)`. Rebuild against the new SDK when 0.7 arrives.

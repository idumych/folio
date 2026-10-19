---
name: folio-platforms
description: Build, package, test and release Folio for its three targets — Renku OS (the Haiku fork; .hpkg), VitruvianOS (.deb) and Linux via Cosmoe (AppImage) — including driving VirtualBox VMs headlessly (keyboard, mouse, screenshots, SSH) and testing Cosmoe builds under Xvfb. Use this whenever the task involves building or packaging Folio for any platform, cutting or verifying a release, booting or driving a test VM, checking that Folio runs on Renku/VitruvianOS/Linux, porting code to one of these systems, or reporting a platform bug upstream — even if the user just says "test it on Renku", "make a release", "does it still work on V\OS" or "check the AppImage".
---

# Building and testing Folio on Renku OS, VitruvianOS and Linux (Cosmoe)

Folio is one C++ source tree against the Haiku API. Three platforms ship it:

| Platform | What it is | Package | Built where | Tested how |
|---|---|---|---|---|
| **Renku OS** | Fork of Haiku (still names its system package `haiku`, version `r1~beta6`) | `folio-<v>-1-x86_64.hpkg` via `make hpkg` | Inside a Haiku-family VM, over SSH | VirtualBox VM: keyboard + mouse + screenshots |
| **VitruvianOS** (V\OS) | Haiku API on a Linux kernel (Debian-based) | `folio_<v>-1_amd64.deb` via `make deb` | On the Linux host, with a link-time SDK | VirtualBox VM (UEFI), live session, keyboard + mouse |
| **Linux** | Any desktop, through the Cosmoe library | `Folio-<v>-x86_64.AppImage` | **GitHub Actions** (Ubuntu 24.04) for releases; locally only for porting | Xvfb + Openbox on the host, driven with xdotool |

Read the reference file for the platform you're working on. Each one has the build steps, the VM or headless setup, a test checklist, and the traps found the hard way:

- `references/renku.md`: Renku OS (and Haiku-family details in general)
- `references/vitruvianos.md`: VitruvianOS
- `references/cosmoe-linux.md`: Linux / Cosmoe / AppImage
- `references/virtualbox.md`: driving any VM: scancodes, mouse via the VirtualBox SDK, screenshots, SSH, files in and out

Helper scripts (in `scripts/`; read the header of each before use):

- `make-vm.sh NAME IMAGE [efi]`: create and boot a VM from an ISO or raw image, using a copy so the original is never touched
- `vm.sh STEP…`: drive a VM: `k:` scancodes, `t:` text, `m:` mouse, `w:` wait, `shot:` screenshot (`VM=name` selects it)
- `mouse.py`: absolute mouse input through `vboxapi` (needs the `virtualbox-sdk` package)
- `xtest.sh STEP…`: the same idea for a Cosmoe build under Xvfb (`start`, `c:` held click, `k:`, `t:`, `shot:`)
- `serve-ssh-setup.sh`: one-time SSH setup for a Haiku-family guest

## Rules that matter

- **Never contact the Haiku project** (no Trac tickets, patches or forum posts), even for bugs Renku or Cosmoe inherited from Haiku: the user says Haiku doesn't welcome AI contributions. Upstream reports go to **RenkuOS/Source** (AI-assisted work is explicitly allowed; keep write-ups short and plain) and **haydentech/cosmoe**. File only after you've reproduced the bug in the platform's own code or apps, not just in Folio.
- **Test the exact artifact you publish.** Build all three packages from one commit, download the CI AppImage rather than a local build, install the `.deb` and `.hpkg` from the GitHub release URL inside the VM, and compare checksums against `SHA256SUMS`.
- **Don't build the release AppImage on the host.** CachyOS packages are built for x86-64-v3 (AVX2) and its glibc is newer than most distros', so a host-built AppImage would crash on older CPUs and refuse to start on Ubuntu 24.04. Release AppImages come from `.github/workflows/appimage.yml`.
- **Report results honestly.** Say what was tested where; "type-checked" isn't "ran", and a headless X11 run isn't a Wayland run. Keep the README's platform table and the "Tested" section of the release notes in step with what was actually exercised.
- Keep Folio's shared rules from `CLAUDE.md`: the portable core (`MarkdownScan`, `LinkGraph`) stays free of Haiku headers, `make test` must pass, and platform-specific code goes behind `#ifdef FOLIO_COSMOE` (Cosmoe) or the Makefile's platform branches.

## The release workflow (all three platforms)

1. **Bump the version** in all of `Folio.rdef` (`middle`/`minor`), `packaging/build-deb.sh`, `packaging/haiku/build-hpkg.sh`, `packaging/appimage/build-appimage.sh`, `packaging/debian/changelog` (new entry), and the file names in `README.md`. `git grep -n "0\.1\.<old>"` finds what's left.
2. **Check on the host:** `make test` (38+ checks), `sh tools/typecheck/haiku-check.sh` (all `ok`), and a VitruvianOS link with `. .vos-sdk/sdk/env.sh && make` (see vitruvianos.md).
3. **Commit and push**, then start the AppImage workflow: `gh workflow run appimage.yml --ref main`. Wait for it in the background with `gh run watch <id> --exit-status`.
4. **Build the others from the same commit:**
   - `.deb`: `sh -c '. .vos-sdk/sdk/env.sh && make deb'` on the host.
   - `.hpkg`: `git archive HEAD | ssh <guest> 'rm -rf ~/folio-rel && mkdir ~/folio-rel && tar -x -C ~/folio-rel && cd ~/folio-rel && make hpkg'`, then `scp` it back.
5. **Test** each artifact on its platform (checklists in the reference files). For the AppImage, download the CI artifact with `gh run download <id> -n Folio-AppImage`.
6. **Publish:** `sha256sum *.deb *.hpkg *.AppImage > SHA256SUMS`; `git tag -a v<v>` and push the tag; `gh release create v<v> <files> SHA256SUMS --verify-tag --latest --notes-file packaging/release-notes-<v>.md`. The tag also triggers the AppImage workflow, which leaves an already-attached AppImage alone.
7. **Re-test from the release:** install the `.deb` and `.hpkg` in their VMs by `curl`ing the release URLs, and check the downloaded AppImage's checksum.

## Platform cheat sheet

| | Renku OS | VitruvianOS | Linux (Cosmoe) |
|---|---|---|---|
| Command key | **Alt** | **Ctrl** | **Ctrl** |
| Install | `pkgman install ./x.hpkg` | `sudo apt install ./x.deb` | `chmod +x`; run |
| App lives in | `/boot/system/apps/Folio` | `/system/apps/Folio` | inside the AppImage |
| Attributes | BFS, indexed; `listattr`, `query` | `user.beos.*` xattrs; `getfattr -d -m user.beos` | `user.*` xattrs; `getfattr -d -m -` |
| Trash | `/boot/trash` + `_trk/original_path` attribute | `~/config/data/Trash/{files,info}` | `~/.local/share/Trash/{files,info}` |
| Settings | `~/config/settings/Folio_settings` | `~/config/settings/…` | `~/.config/cosmoe/Folio_settings` |
| Node monitor | Haiku ints, leaf names | `virtual:*` refs | dev_t/ino_t fields, **full-path** names; `watch_path()` only |
| Open folder/URL | Tracker / `be_roster` | Tracker / `be_roster` | `xdg-open` |

## Core feature test (every platform)

Whatever else you check, prove the thing Folio exists for:

1. Fresh launch creates `~/Notebooks/Welcome` with three pages.
2. Rename "Start Here" through Folio (command key + R). `Links and Renaming.md` must then contain `[[New]]`, `[[New#Links]]` and `[[New|the welcome page]]`.
3. With Folio running, `mv` that page in a terminal. All three links follow, keeping `#Links` and the `|alias`.
4. Rename a **section folder** with `mv`, then a page inside it. The second rename is the one that catches stale folder watches.
5. Folio's stderr log stays empty.

Check results with `grep -o '\[\[[^]]*\]\]' *.md` in the notebook folder rather than trusting the screen.

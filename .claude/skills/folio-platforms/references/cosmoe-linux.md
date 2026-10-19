# Linux via Cosmoe (AppImage)

Cosmoe (github.com/haydentech/cosmoe, MIT licence) is the Haiku API as a C++ library on X11, Wayland, macOS and Windows. It needs no servers. It finds its add-ons and data relative to wherever `libbe.so` is loaded (`dladdr`), which is what makes an AppImage possible. Folio pins the release tag `COSMOE_TAG` in `packaging/appimage/build-appimage.sh` (currently `v0.9.0_20261013`).

## Release builds: CI only

`.github/workflows/appimage.yml` runs on `ubuntu-24.04`. It installs Cosmoe's Debian dependencies, caches `.cosmoe/stage`, runs `build-appimage.sh` and uploads the artifact. On a `v*` tag it attaches the AppImage to the release, unless one is already there.

```sh
gh workflow run appimage.yml --ref main
gh run watch <id> --exit-status            # in the background; the first run builds Cosmoe
gh run download <id> -n Folio-AppImage -D <dir>
```

Check the requirements of what was built:

```sh
./Folio-*.AppImage --appimage-extract >/dev/null
find squashfs-root -type f \( -name '*.so*' -o -path '*/usr/bin/Folio' \) -exec objdump -T {} \; \
  | grep -oE '(GLIBC|GLIBCXX|CXXABI)_[0-9.]+' | sort -Vu
readelf -d squashfs-root/usr/lib/libbe.so | grep RUNPATH     # must be just $ORIGIN
```

As of 0.1.2 it needs GLIBC_2.38 and GLIBCXX_3.4.32, so Ubuntu 24.04, Fedora 39, Debian 13 or newer; Debian 12 is too old.

## Local builds (porting and debugging only)

1. **Build Cosmoe into a staging dir.** Install `meson` first; on Arch, also `libicns` (optional).
   ```sh
   git clone --depth 1 --branch <COSMOE_TAG> https://github.com/haydentech/cosmoe.git cosmoe-src
   cd cosmoe-src && meson setup builddir --prefix=/usr --libdir=lib -Denable_unit_test_compile=false
   ninja -C builddir && DESTDIR=<stage> meson install -C builddir --no-rebuild
   ```
   Use `DESTDIR`. A plain `--prefix=$HOME/x` install fails, because Cosmoe hard-codes `/usr/share/wayland-sessions`. A full build is about 1,250 steps.
2. **Build Folio against it:**
   ```sh
   make COSMOE_ROOT=<stage>/usr
   ```
   The Makefile picks the Cosmoe branch when `$(COSMOE_ROOT)/include/os/app/Application.h` exists, defines `FOLIO_COSMOE`, and links the resources as a `.cosmoe_rsrc` section.
3. **Run it:** `LD_LIBRARY_PATH=<stage>/usr/lib ./Folio`, because the RUNPATH is `$ORIGIN/../lib` for the AppImage.
4. **Local AppImage** (to test the script, **not** for release): `COSMOE_STAGE=<stage> sh packaging/appimage/build-appimage.sh`.

What `build-appimage.sh` does, and why:

- **Copies** `libbe`, `libtracker`, both backend plugins, `addons/`, `share/cosmoe/{mime_db,artwork,locale}` and six Noto fonts.
- **Rewrites RUNPATHs** with the `patchelf` from inside linuxdeploy (`$ORIGIN`; `$ORIGIN/../..` for add-ons). Cosmoe bakes in `/usr/lib`, and linuxdeploy only *appends* `$ORIGIN`, so without this the host's ICU, pango and cairo would win.
- **Runs linuxdeploy with `NO_STRIP=1`,** because its bundled `strip` can't read newer distros' `.relr.dyn` sections. The script strips Cosmoe's libraries itself.
- **`AppRun`** sets `FONTCONFIG_FILE` (bundled `fonts.conf` includes the system one) and `TRANSLATORS=<bundle>/usr/lib/addons/Translators`. Without that, the very first run from a *mounted* AppImage with a fresh `$HOME` loads no image translators, so there are no thumbnails (cosmoe#4).

## Headless test setup (no VM needed)

Needs `xorg-server-xvfb`, `openbox`, `xdotool` and ImageMagick.

```sh
Xvfb :99 -screen 0 1280x800x24 -nolisten tcp &      # run in the background
DISPLAY=:99 openbox &                               # needed: without a WM, Cosmoe windows never get focus
sh .claude/skills/folio-platforms/scripts/xtest.sh start      # see the script for APP=, HOME_DIR=
```

`xtest.sh` takes care of the traps:

- **Fresh `$HOME`.** Folio's first run creates `~/Notebooks`, and Cosmoe creates `~/cosmoe` and `~/.config/cosmoe`.
- **`env -u WAYLAND_DISPLAY` and `COSMOE_BACKEND=x11`,** so it never opens a window on the user's real Wayland desktop.
- **A fake `xdg-open`** first in `PATH`, which logs its arguments. The real one would open Dolphin on the user's desktop.
- **Held clicks** (mousedown, 150 ms, mouseup). xdotool's zero-length `click` permanently stops all input to a Cosmoe window; that's a Cosmoe bug (cosmoe#5), not Folio's.

Screenshots: `import -window root shot.png` (cropping with `-crop WxH+X+Y +repage` makes good README images).

**Debugging:**

- **Backtraces:** Yama blocks `gdb -p`, so launch under gdb instead:
  `gdb -batch -ex run -ex "thread apply all bt 20" --args ./Folio`, then `pkill -INT -x Folio` after reproducing.
- **Library loading:** `LD_DEBUG=files` shows which libraries and add-ons get loaded. In an AppImage, `/proc/<pid>/maps` shows whether a library came from `/tmp/.mount_*` (bundled) or `/usr/lib` (host).
- **Timing:** `LD_DEBUG` and gdb slow startup down, which can hide timing bugs. Confirm a fix without them.

## Test checklist (Ctrl is the command key)

The core feature test from SKILL.md (Ctrl+R; `mv` with Folio running; renaming a section folder then a page inside it), plus:

- **Follow a red link:** Ctrl-click it. It should create the page.
- **Checkbox:** ticking it updates `user.FOLIO:open_tasks` (`getfattr -d -m - file`).
- **Attach:** Ctrl+I opens the panel. Check the title reads "Folio: Attach file" (not a path), the file name stays the leaf, and the thumbnail shows **on a first run with a fresh HOME from the mounted AppImage**.
- **Trash:** `~/.local/share/Trash/{files,info}` with a `.trashinfo`.
- **Show in File Manager:** `xdg-open <folder>` appears in the fake log.
- **AppImage only:** `/proc/<pid>/maps` shows `libicuuc` and `libpango` from `/tmp/.mount_*`.

Not testable headlessly yet: the Wayland backend, and dragging files in from a Linux file manager (XDND). To test those, use a desktop Linux VM in VirtualBox (see virtualbox.md), or ask the user to try on their Plasma session.

## Cosmoe behaviour Folio works around (and the upstream issues)

| What | Folio's workaround | Upstream |
|---|---|---|
| `watch_node()` returns `B_NOT_SUPPORTED` | `Vault::_Watch` uses `watch_path()` and re-watches moved folders | cosmoe#2 |
| Node-monitor fields are dev_t/ino_t types; `name` is a full path | `FindNumber()`; `_EventEntry()` accepts absolute names | cosmoe#2 |
| Underline/strikeout faces aren't drawn and pick a Light style | Faces left out under `FOLIO_COSMOE`; colour only | cosmoe#3 |
| First run from a mounted AppImage loads no translators | `AppRun` sets `TRANSLATORS` | cosmoe#4 |
| Zero-length clicks stop all input | Tests hold clicks for 150 ms | cosmoe#5 |
| `entry_ref::name` is a full path | Vault uses `md::Basename(ref.name)`; the attach panel sets its own title | cosmoe#6 |
| Install: hard-coded session dir, host `rm` during DESTDIR installs, absolute RUNPATH | DESTDIR installs; patchelf | cosmoe#7 |

Check those issues before relying on a workaround. Once fixed upstream (and the pinned tag bumped), drop the workaround and retest.

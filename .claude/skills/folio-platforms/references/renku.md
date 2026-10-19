# Renku OS (and Haiku-family guests)

Renku OS (github.com/RenkuOS/Source) is a fork of Haiku that started in September 2026. Folio's `.hpkg` targets it. For packaging purposes it's still Haiku:

- The system package is still called `haiku`, with `HAIKU_VERSION = r1~beta6` (`build/jam/BuildSetup`). Folio's `requires { haiku >= r1~beta6 }` is satisfied, and a package built on Haiku R1/beta6 installs and runs unchanged. That was verified on nightly build00055 (hrev60072+253).
- Nightlies: `gh release view nightly -R RenkuOS/Source` (x86_64 and x86_gcc2h ISOs, each with a `.sha256`). Download into `~/Downloads` and verify with `sha256sum -c`.
- Policy: `AGENTS.md` in RenkuOS/Source and `RenkuOS/PlanAndGovernance` (CONTRIBUTING, REVIEW). Machine-assisted work is allowed. Pull requests need a short human-style summary, a "How this was tested" section and a DCO `Signed-off-by`. Issues go to RenkuOS/Source.

## Make a test VM

```sh
sh .claude/skills/folio-platforms/scripts/make-vm.sh folio-renku ~/Downloads/RenkuOS-x86_64-<date>-build<NNN>.iso
```

The ISO is an *anyboot* hybrid image, so converted to VDI it boots as a writable hard disk with BIOS firmware (not EFI). The screen is 1024×768.

- **First screen:** "Welcome!", with **Try it out** at about (778,566). Clicking it gives the desktop, logged in, with the Deskbar top-right.
- **Terminal:** click the Deskbar logo at (955,12), hover Applications (785,226), move left to (640,226) and down to Terminal at (625,707), then click. Positions shift if the app list changes, so screenshot first.
- **Free space:** about 300 MB on the boot disk. Packages stay compressed, so `pkgman install gcc make` fits. Note that the Haiku beta6 image needed `gcc`/`make` installed, while `haiku_devel` was already there.
- **Clock:** the VM clock runs in local time while the host thinks UTC, so tar and make warn "time stamp in the future". Harmless after `find . -exec touch {} +`, and `make hpkg` starts with `make clean` anyway.
- **Screen blanker:** if a screenshot comes back black, move the mouse (`m:move,…`) and take it again.

## Get files in and out

- **Folio's release packages:** `curl -fsSLO https://github.com/idumych/folio/releases/download/v<v>/folio-<v>-1-x86_64.hpkg` inside the guest (no wget needed; curl ships).
- **Anything else:** set up SSH once (below), then use `ssh`/`scp`/`tar | ssh`. For a few KB without SSH, type base64 into the terminal with `vm.sh t:` (slow: about 1 KB per 30 s).

## SSH into a Haiku-family guest (once per VM)

Ask the user before doing this: it installs a key and enables sshd at boot, which auto mode flags as persistence. Steps:

1. `VBoxManage controlvm <vm> poweroff`, or better, `sync; shutdown` typed in the guest. Then add a port forward: `VBoxManage modifyvm <vm> --natpf1 "ssh,tcp,127.0.0.1,2222,,22"`. Start the VM again.
2. Key: `ssh-keygen -t ed25519 -N '' -f ~/.ssh/folio_vm_ed25519` (reuse it if it already exists). Add a host alias in `~/.ssh/config` (`HostName 127.0.0.1`, `Port 2222`, `User user`, `IdentityFile`, `IdentitiesOnly yes`, `StrictHostKeyChecking accept-new`, `UserKnownHostsFile ~/.ssh/known_hosts.folio-vm`). The Haiku VM uses `folio-haiku`.
3. Serve the setup script: `sh scripts/serve-ssh-setup.sh ~/.ssh/folio_vm_ed25519.pub` (run in the background). It binds the host's **LAN IP**, because VirtualBox 7 NAT blocks guest-to-host-localhost.
4. In the guest Terminal: `curl http://<lan-ip>:8731/setup.sh | sh` (type it with `vm.sh t:`). Then stop the server.

What makes Haiku's sshd different (the script handles both):

- Keys are read from **`~/config/settings/ssh/authorized_keys`**, not `~/.ssh/`.
- The user is uid 0, so `PermitRootLogin prohibit-password` is needed (key only).
- launch_daemon already starts sshd at boot. If it was started before the config changed, kill it and run `/bin/sshd` (absolute path required).

## Build the hpkg

On a Haiku-family guest with `gcc`, `make` and `haiku_devel` installed:

```sh
git archive HEAD | ssh folio-haiku 'rm -rf ~/folio-rel && mkdir ~/folio-rel && tar -x -C ~/folio-rel && cd ~/folio-rel && make hpkg'
scp folio-haiku:folio-rel/folio-<v>-1-x86_64.hpkg .
```

`packaging/haiku/build-hpkg.sh` strips at link time, because `xres` appends resources after the ELF data. It then runs `mimeset` and builds the package with `package create -C`. Check the result with `package list <file>`, which shows `apps/Folio`, `bin/folio`, the `data/deskbar/menu/Applications/Folio` link, the docs, and `requires: haiku>=r1~beta6`.

So far packages have been **built** on the Haiku beta6 VM and **tested** on Renku. To build on Renku itself, give its VM the same SSH setup and `pkgman install gcc make haiku_devel`. Check that its repositories provide them first; this hasn't been verified yet.

## Test checklist (keyboard: Alt is the command key)

From a guest Terminal (or SSH):

```sh
pkgman install -y ./folio-<v>-1-x86_64.hpkg         # dependency check happens here
listattr /boot/system/apps/Folio | grep -c BEOS      # expect 5
which folio                                          # /bin/folio
(folio > /tmp/folio.log 2>&1 &)                      # launch with a log
```

Then the core feature test from SKILL.md, plus:

- **Rename dialog:** Alt+R is scancodes `k:38,13,93,b8`. Type the name, then Enter `k:1c,9c`.
- **Rename from a shell while Folio runs:** `(folio &); sleep 5; mv 'A.md' 'B.md'; sleep 4; hey Folio quit`. `hey` may print "Error when sending message"; that's harmless.
- **Follow a red link:** Alt-click it (`m:altclick,x,y`). It should create the page.
- **Checkbox:** click the `[ ]` and it turns to `[x]` with strikethrough.
- **Attach:** Alt+I opens the file panel. Type-ahead the name and press Enter. A thumbnail should appear in the strip.
- **Trash:** right-click a tree item, Move to Trash, and confirm.
  - `ls $(finddir B_TRASH_DIRECTORY)`
  - `catattr _trk/original_path "<trash>/<file>"` should show the original path
- **Queries:**
  - `lsindex | grep FOLIO` should list `FOLIO:tags` and `FOLIO:open_tasks`
  - `query 'FOLIO:tags=*welcome*'` should list the tagged pages
- **Window fit:** at 1024×768 the window has to stay on screen. Folio clamps its frame to the screen; an earlier bug let the side panel overflow.

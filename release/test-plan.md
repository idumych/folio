# Test plan: Folio

Folio is a **documents** app: Markdown pages in folders under `~/Notebooks`.
Its main job is keeping `[[wiki links]]` correct when pages are renamed or
moved, inside the app or from outside it. `{p}` is the platform (`haiku`,
`vos`, `linux`); it goes into the names typed, so each platform's evidence
is different.

## Fixtures

None. The first launch writes the Welcome notebook, and the checks use it.
Run the file commands in a shell on the same machine: `ssh haiku-b6-test` or
`ssh vos-test` for the VMs, and the host shell with `HOME=$HOME_DIR` for
Linux under Xvfb (the throwaway HOME that `xtest.sh` gives the app).

    N=~/Notebooks/Welcome/"Getting Started"     # Linux: N="$HOME_DIR/Notebooks/Welcome/Getting Started"

## Fresh state

Delete these before the first launch:

- Haiku: `rm -rf ~/Notebooks ~/config/settings/Folio_settings`
- VitruvianOS: `rm -rf ~/Notebooks ~/config/settings/Folio_settings`
- Linux: `xtest.sh` starts with an empty HOME; nothing to delete.

## Feature checks

### welcome

Steps: launch Folio on the fresh state. Take a screenshot.
Then run `find ~/Notebooks -name '*.md' | sort > $E/{p}-tree.txt` (copy the
file back to the host for the VMs).

PASS when: the screenshot's OCR shows "Start Here" and `{p}-tree.txt` lists
`Welcome/Getting Started/Links and Renaming.md` (EVIDENCE_1).

### rename-links

Steps: in the tree on the left, click "Start Here" so it opens. Choose
Rename… from the Page menu (command key + R: Alt+R on Haiku, Ctrl+R
elsewhere). In the "New name:" field select all, type `Renamed {p}` and
press Enter. Take a screenshot. Then run
`grep -o '\[\[[^]]*\]\]' "$N/Links and Renaming.md" > $E/{p}-links-rename.txt`.

PASS when: the tree shows "Renamed {p}" in OCR, and
`{p}-links-rename.txt` contains `[[Renamed {p}]]`, `[[Renamed {p}#Links]]`
and `[[Renamed {p}|the welcome page]]` (EVIDENCE_2 checks the alias one).

### external-move

Steps: with Folio still running, in the shell run
`mv "$N/Renamed {p}.md" "$N/Moved {p}.md"`, wait 3 seconds, then
`grep -o '\[\[[^]]*\]\]' "$N/Links and Renaming.md" > $E/{p}-links-move.txt`.
Take a screenshot; save `ocr-find.sh --text` of it as `$E/{p}-screen.txt`.

PASS when: `{p}-links-move.txt` contains `[[Moved {p}]]`,
`[[Moved {p}#Links]]` and `[[Moved {p}|the welcome page]]` (EVIDENCE_3
checks the heading one), and the tree in the screenshot shows "Moved {p}".

"""Builds two Folio mockup screens (HTML in Haiku style) and captures PNGs."""
import pathlib
import sys

from playwright.sync_api import sync_playwright

HERE = pathlib.Path(__file__).parent
OUT = pathlib.Path(sys.argv[1])

LEAF = """<svg viewBox="0 0 16 16"><path d="M8 1c4 2 6 6 4 11-1 2-3 3-4 3s-3-1-4-3C2 7 4 3 8 1z"
 fill="#3f9a3f" stroke="#1f5a1f" stroke-width=".8"/><path d="M8 3v11" stroke="#1f5a1f" stroke-width=".7"/></svg>"""


def deskbar(active):
    teams = [("Folio", "#ffcb00"), ("Tracker", "#7aa7d9"), ("Terminal", "#444")]
    rows = "".join(
        f'<div class="team{" active" if n == active else ""}"><i style="background:{c}"></i>{n}</div>'
        for n, c in teams)
    return f"""<div class="deskbar"><div class="leaf">{LEAF}VitruvianOS</div>
<div class="tray"><span class="chip" style="background:#5b8fd4"></span>
<span class="chip" style="background:#7cb36a"></span><span style="font-weight:600">7:50 PM</span></div>
{rows}</div>"""


def desktop_icons():
    folder = """<svg class="img" viewBox="0 0 34 34"><path d="M3 9h11l3 3h14v17H3z" fill="#f4c542" stroke="#8a6a10"/>
<path d="M3 13h28v16H3z" fill="#ffd968" stroke="#8a6a10"/></svg>"""
    trash = """<svg class="img" viewBox="0 0 34 34"><path d="M8 9h18l-2 22H10z" fill="#c9d3dc" stroke="#55606a"/>
<rect x="6" y="6" width="22" height="4" rx="1" fill="#e2e8ee" stroke="#55606a"/></svg>"""
    return (f'<div class="dicon" style="right:42px;top:640px">{folder}Notebooks</div>'
            f'<div class="dicon" style="right:42px;top:716px">{trash}Trash</div>')


def tree(rows):
    out = []
    for level, kind, label, state in rows:
        tri = {"open": "open", "closed": "closed"}.get(state, "")
        cls = "row sel" if state == "sel" else "row"
        name = f'<span class="folder">{label}</span>' if kind == "f" else label
        out.append(f'<div class="{cls}" style="padding-left:{4 + level * 16}px">'
                   f'<span class="tri {tri}"></span>{name}</div>')
    return "".join(out)


TREE_ROWS = lambda sel: [
    (0, "f", "Home", "closed"),
    (0, "f", "Welcome", "open"),
    (1, "f", "Getting Started", "open"),
    (2, "p", "Links and Renaming", ""),
    (2, "p", "Start Here", ""),
    (2, "p", "To-dos and Tags", ""),
    (0, "f", "Work", "open"),
    (1, "f", "Meetings", "open"),
    (2, "p", "Retro — Oct 9", ""),
    (2, "p", "Standup", "sel" if sel == "Standup" else ""),
    (1, "f", "People", "closed"),
    (1, "f", "Projects", "closed"),
    (1, "p", "Roadmap 2027", "sel" if sel == "Roadmap 2027" else ""),
]


def vbar(top, height):
    return (f'<div class="vbar"><div class="arrow"></div><div class="track">'
            f'<div class="knob" style="top:{top}px;height:{height}px"></div></div>'
            f'<div class="arrow down"></div></div>')


def wl(target, shown=None, bad=False, embed=False):
    """A [[wiki link]] styled the way NoteView renders it."""
    cls = "bad" if bad else "lnk"
    bang = "!" if embed else ""
    if shown is not None:
        return (f'<span class="{cls}"><span class="syn">{bang}[[{target}|</span>{shown}'
                f'<span class="syn">]]</span></span>')
    return f'<span class="{cls}"><span class="syn">{bang}[[</span>{target}<span class="syn">]]</span></span>'


def window(title, sel_page, title_field, doc_html, strip_html, panel_tabs, panel_html,
           extra="", title_focus=False):
    tabs = "".join(f'<span class="{"on" if on else ""}">{t}</span>' for t, on in panel_tabs)
    return f"""
<div class="win" style="left:22px;top:44px;width:1082px;height:732px">
  <div class="tab"><span class="box"></span>{title}<span class="box zoom"></span></div>
  <div class="frame">
    <div class="menubar"><span>File</span><span>Edit</span><span>Page</span><span>View</span></div>
    <div class="body">
      <div style="width:226px;display:flex;flex-direction:column;flex:none">
        <div class="scroll list tree" style="border-top:none">
          <div style="flex:1;padding-top:3px">{tree(TREE_ROWS(sel_page))}</div>{vbar(0, 0)}
        </div>
      </div>
      <div class="split"></div>
      <div class="editor-col">
        <div class="titlebar-field"><div class="titlefield{" focus" if title_focus else ""}">{title_field}</div></div>
        <div class="scroll"><div class="doc">{doc_html}</div>{vbar(0, 230)}</div>
        {strip_html}
      </div>
      <div class="split"></div>
      <div class="panel"><div class="tabs">{tabs}</div><div class="content">{panel_html}</div></div>
    </div>
  </div>
  {extra}
</div>"""


def page(body, active="Folio"):
    return f"""<!doctype html><html><head><meta charset="utf-8">
<link rel="stylesheet" href="haiku.css"></head><body>
{desktop_icons()}{deskbar(active)}{body}</body></html>"""


BURNDOWN = """<svg viewBox="0 0 110 80"><rect x=".5" y=".5" width="109" height="79" fill="#fff" stroke="#9a9a9a"/>
<g stroke="#e3e3e3"><path d="M14 18h88M14 34h88M14 50h88M14 66h88"/></g>
<path d="M14 12 L102 70" stroke="#b9b9b9" stroke-dasharray="3 2"/>
<path d="M14 12 L26 18 L38 20 L50 31 L62 36 L74 47 L86 52 L98 63" fill="none" stroke="#2463c4" stroke-width="2"/>
<g fill="#2463c4"><circle cx="50" cy="31" r="1.8"/><circle cx="74" cy="47" r="1.8"/><circle cx="98" cy="63" r="1.8"/></g>
<text x="14" y="9" font-size="6.5" font-family="Noto Sans" fill="#333">Sprint 14 burndown</text></svg>"""

WHITEBOARD = """<svg viewBox="0 0 110 76"><rect width="110" height="76" rx="2" fill="#8f8a80"/>
<rect x="4" y="4" width="102" height="68" fill="#f7f7f2"/>
<g fill="none" stroke-width="1.6" stroke-linecap="round">
<rect x="10" y="12" width="26" height="15" rx="2" stroke="#2a6bc4"/>
<rect x="44" y="12" width="26" height="15" rx="2" stroke="#2a6bc4"/>
<rect x="77" y="38" width="24" height="15" rx="2" stroke="#c43b2a"/>
<path d="M36 19h8M70 19 C82 19 88 26 89 37" stroke="#333"/>
<path d="M12 40 q8 -4 16 0 t16 0 M12 48 q8 -4 16 0 t14 0 M12 56 q6 -3 12 0" stroke="#3c9a4a"/></g>
<rect x="44" y="60" width="16" height="5" rx="1" fill="#3a3a3a"/></svg>"""

PDF = """<svg viewBox="0 0 48 58"><path d="M4 2h28l12 12v42H4z" fill="#fff" stroke="#555"/>
<path d="M32 2v12h12" fill="#e6e6e6" stroke="#555"/><rect x="4" y="36" width="30" height="13" fill="#c8312a"/>
<text x="9" y="46" font-size="10" font-weight="700" fill="#fff" font-family="Noto Sans">PDF</text>
<g stroke="#bbb"><path d="M10 14h16M10 20h26M10 26h22"/></g></svg>"""


def strip(cells):
    inner = "".join(f'<div class="cell"><div class="thumb">{svg}</div>{name}</div>' for svg, name in cells)
    return f'<div class="strip">{inner}</div>'


def hits(rows, sel=None):
    out = []
    for i, (pg, ctx) in enumerate(rows):
        out.append(f'<div class="hit{" sel" if i == sel else ""}"><span class="pg">{pg}</span>'
                   f'<span class="ctx">{ctx}</span></div>')
    return out


# ---------------------------------------------------------------- screen 1
doc1 = f"""
<p class="h1"><span class="syn"># </span>Roadmap 2027</p>
<p>Owner: {wl("Priya")} · reviewed every week in {wl("Standup#Decisions", "Monday standup")}</p>
<p></p>
<p class="h2"><span class="syn">## </span>Q1</p>
<p>- Ship the <b><span class="syn">**</span>link engine<span class="syn">**</span></b> to beta testers, see {wl("Link Engine Spec")}</p>
<p>- Move the team's old notebooks over: {wl("Import Notes", bad=True)}</p>
<p>- Tracker queries on <span class="code">`FOLIO:tags`</span> once indexing lands <span class="tag">#planning</span></p>
<p></p>
<p class="h2"><span class="syn">## </span>Q2</p>
<p>- Inline images in the editor</p>
<p>- Heading renames update <span class="code">`[[Page#Heading]]`</span> links</p>
<p></p>
<p style="color:#5f6e78"><span class="syn">&gt; </span>Keep every page plain Markdown so any tool can read it.</p>
<p></p>
<p>{wl("sprint-burndown.png", embed=True)}</p>
"""

back_rows = [
    ("Standup", f'- [ ] Update <em>[[Roadmap 2027]]</em> with the Q2 dates'),
    ("Retro — Oct 9", f'Went well: the <em>[[Roadmap 2027]]</em> review was quick'),
    ("Priya", f'Owns <em>[[Roadmap 2027|the roadmap]]</em> and hiring'),
]
panel1 = ('<div class="listbox"><div class="items">' + "".join(hits(back_rows)) +
          '</div>' + vbar(0, 0) + '</div>')

screen1 = page(window(
    "Folio — Roadmap 2027", "Roadmap 2027", "Roadmap 2027<span class='caret'></span>",
    doc1, strip([(BURNDOWN, "sprint-burndown.png")]),
    [("Backlinks (3)", True), ("Tags", False), ("Search", False)], panel1,
    title_focus=True))

# ---------------------------------------------------------------- screen 2
doc2 = f"""
<p class="h1"><span class="syn"># </span>Standup</p>
<p><span class="tag">#meeting</span> <span class="tag">#team/core</span></p>
<p></p>
<p class="h2"><span class="syn">## </span>Decisions</p>
<p>- Pages stay one Markdown file each; see {wl("Roadmap 2027")}</p>
<p>- Whiteboard from Monday: {wl("whiteboard.jpg", embed=True)}</p>
<p></p>
<p class="h2"><span class="syn">## </span>To-dos</p>
<p>- <span class="box">[x]</span><span class="done"> Send the Q3 numbers to </span><span class="lnk" style="color:#969696"><span class="syn">[[</span>Priya<span class="syn">]]</span></span></p>
<p>- <span class="box">[ ]</span> Update {wl("Roadmap 2027")} with the Q2 dates</p>
<p>- <span class="box">[ ]</span> Book a room for the design review, budget in {wl("Q3 budget.pdf")}</p>
<p>- <span class="box">[ ]</span> Draft the onboarding notes, start from <span class="lnk"><span class="syn">[[</span></span><span class="caret"></span></p>
"""

menu_items = [
    ("Link Engine Spec", "Work/Projects"),
    ("Links and Renaming", "Welcome/Getting Started"),
    ("Priya", "Work/People"),
    ("Retro — Oct 9", "Work/Meetings"),
    ("Roadmap 2027", "Work"),
    ("Start Here", "Welcome/Getting Started"),
    ("To-dos and Tags", "Welcome/Getting Started"),
]
menu = '<div class="menu" style="left:586px;top:386px">' + "".join(
    f'<div class="mi{" hl" if n == "Start Here" else ""}"><span>{n}</span><span class="where">— {w}</span></div>'
    for n, w in menu_items) + "</div>"
pointer = """<svg class="pointer" style="left:712px;top:500px" viewBox="0 0 14 20">
<path d="M1 1v15l4-3.5 2.6 6 2.2-1-2.6-5.8H13z" fill="#fff" stroke="#000" stroke-width="1.1" stroke-linejoin="round"/></svg>"""

todo_rows = [
    ("Standup", "- [ ] Update [[Roadmap 2027]] with the Q2 dates"),
    ("Standup", "- [ ] Book a room for the design review…"),
    ("Standup", "- [ ] Draft the onboarding notes, start from…"),
    ("Errands", "- [ ] Renew the library card"),
    ("Link Engine Spec", "- [ ] Unicode case folding for names"),
    ("To-dos and Tags", "- [ ] Click a box to check it off"),
    ("To-dos and Tags", "- [ ] Press Enter at the end of a to-do…"),
]
panel2 = ('<div class="textfield focus">todo</div>'
          '<div class="listbox"><div class="items">' + "".join(hits(todo_rows)) +
          '</div>' + vbar(0, 0) + '</div>')

screen2 = page(window(
    "Folio — Standup", "Standup", "Standup", doc2,
    strip([(WHITEBOARD, "whiteboard.jpg"), (PDF, "Q3 budget.pdf")]),
    [("Backlinks (2)", False), ("Tags", False), ("Search", True)], panel2,
    extra=menu + pointer))

(HERE / "screen1.html").write_text(screen1)
(HERE / "screen2.html").write_text(screen2)

with sync_playwright() as p:
    browser = p.chromium.launch()
    pg = browser.new_page(viewport={"width": 1280, "height": 800}, device_scale_factor=2)
    for name in ("screen1", "screen2"):
        pg.goto((HERE / f"{name}.html").as_uri())
        pg.wait_for_timeout(300)
        pg.screenshot(path=str(OUT / f"{name}.png"))
    browser.close()
print("ok")

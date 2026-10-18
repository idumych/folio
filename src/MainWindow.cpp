#include "MainWindow.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <map>

#include <Alert.h>
#include <Application.h>
#include <Directory.h>
#include <File.h>
#include <FilePanel.h>
#include <FindDirectory.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <MessageRunner.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <Roster.h>
#include <ScrollView.h>
#include <SplitView.h>
#include <TabView.h>
#include <TextControl.h>

#include "Constants.h"
#include "NoteView.h"
#include "Widgets.h"


static const bigtime_t kSaveDelay = 900000;
static const bigtime_t kRestyleDelay = 120000;


static BMenuItem*
Item(const char* label, uint32 what, char shortcut = 0, uint32 modifiers = 0)
{
	return new BMenuItem(label, new BMessage(what), shortcut, modifiers);
}


static std::string
Stem(const std::string& rel)
{
	return md::StripMdExtension(md::Basename(rel));
}


static bool
IsImagePath(const std::string& path)
{
	static const char* kExtensions[] = {".png", ".jpg", ".jpeg", ".gif",
		".webp", ".bmp", ".tga", ".tif", ".tiff", ".svg", ".avif", ".hvif"};
	for (const char* ext : kExtensions) {
		if (md::EndsWithNoCase(path, ext))
			return true;
	}
	return false;
}


static bool
LessNoCase(const std::string& a, const std::string& b)
{
	return md::ToLower(a) < md::ToLower(b);
}


MainWindow::MainWindow(BRect frame, const char* vaultPath, const char* lastNote)
	:
	BWindow(frame, "Folio", B_DOCUMENT_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
		B_AUTO_UPDATE_SIZE_LIMITS | B_QUIT_ON_WINDOW_CLOSE),
	fDirty(false),
	fMoveMenu(NULL),
	fSaveRunner(NULL),
	fRestyleRunner(NULL),
	fAttachPanel(NULL),
	fVaultPanel(NULL),
	fFirstTreeBuild(true)
{
	fTree = new NotebookTree("tree");
	fTree->SetSelectionMessage(new BMessage(M_TREE_SELECTED));

	fTitle = new BTextControl("title", NULL, "", new BMessage(M_TITLE_CHANGED));
	BFont titleFont(be_bold_font);
	titleFont.SetSize(titleFont.Size() * 1.6f);
	fTitle->SetFont(&titleFont);
	fTitle->TextView()->SetFontAndColor(&titleFont);
	fTitle->SetEnabled(false);

	fEditor = new NoteView("editor");
	fEditor->MakeEditable(false);
	BScrollView* editorScroll = new BScrollView("editorScroll", fEditor, 0,
		false, true, B_NO_BORDER);

	fStrip = new AttachmentStrip();

	fBacklinks = new BListView("backlinks");
	fBacklinks->SetSelectionMessage(new BMessage(M_BACKLINK_INVOKED));
	fTags = new BListView("tags");
	fTags->SetSelectionMessage(new BMessage(M_TAG_INVOKED));
	fResults = new BListView("results");
	fResults->SetSelectionMessage(new BMessage(M_SEARCH_INVOKED));
	fSearch = new BTextControl("search", NULL, "", NULL);
	fSearch->SetModificationMessage(new BMessage(M_SEARCH_CHANGED));

	BGroupView* searchTab = new BGroupView("Search", B_VERTICAL, 4);
	BLayoutBuilder::Group<>(searchTab)
		.SetInsets(4)
		.Add(fSearch)
		.Add(new BScrollView("resultsScroll", fResults, 0, false, true));

	fTabs = new BTabView("panel", B_WIDTH_FROM_LABEL);
	fTabs->AddTab(new BScrollView("Backlinks", fBacklinks, 0, false, true));
	fTabs->AddTab(new BScrollView("Tags", fTags, 0, false, true));
	fTabs->AddTab(searchTab);
	fTabs->SetBorder(B_NO_BORDER);
	fPanel = fTabs;

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(_BuildMenus())
		.AddSplit(B_HORIZONTAL, 0, 1.0f)
			.Add(new BScrollView("treeScroll", fTree, 0, false, true,
				B_NO_BORDER), 0.22f)
			.AddGroup(B_VERTICAL, 0, 0.56f)
				.AddGroup(B_HORIZONTAL)
					.SetInsets(16, 10, 16, 6)
					.Add(fTitle)
				.End()
				.Add(editorScroll)
				.Add(fStrip)
			.End()
			.Add(fTabs, 0.22f)
		.End();

	fStrip->Hide();
	OpenVault(vaultPath, lastNote);
}


MainWindow::~MainWindow()
{
	delete fSaveRunner;
	delete fRestyleRunner;
	delete fAttachPanel;
	delete fVaultPanel;
}


BMenuBar*
MainWindow::_BuildMenus()
{
	BMenuBar* bar = new BMenuBar("menu");

	BMenu* file = new BMenu("File");
	file->AddItem(Item("New Page", M_NEW_PAGE, 'N'));
	file->AddItem(Item("New Section…", M_NEW_SECTION, 'N', B_SHIFT_KEY));
	file->AddItem(Item("New Notebook…", M_NEW_NOTEBOOK));
	file->AddSeparatorItem();
	file->AddItem(Item("Open Notebooks Folder…", M_OPEN_VAULT, 'O'));
	file->AddItem(Item(kShowInFolderLabel, M_SHOW_IN_TRACKER));
	file->AddSeparatorItem();
	BMenuItem* quit = Item("Quit", B_QUIT_REQUESTED, 'Q');
	quit->SetTarget(be_app);
	file->AddItem(quit);
	bar->AddItem(file);

	BMenu* edit = new BMenu("Edit");
	edit->AddItem(Item("Undo", B_UNDO, 'Z'));
	edit->AddSeparatorItem();
	edit->AddItem(Item("Cut", B_CUT, 'X'));
	edit->AddItem(Item("Copy", B_COPY, 'C'));
	edit->AddItem(Item("Paste", B_PASTE, 'V'));
	edit->AddItem(Item("Select All", B_SELECT_ALL, 'A'));
	edit->AddSeparatorItem();
	edit->AddItem(Item("Search Notebooks", M_FIND, 'F'));
	bar->AddItem(edit);

	BMenu* page = new BMenu("Page");
	page->AddItem(Item("Rename…", M_RENAME, 'R'));
	fMoveMenu = new BMenu("Move To");
	page->AddItem(fMoveMenu);
	page->AddItem(Item("Move to Trash", M_DELETE));
	page->AddSeparatorItem();
	page->AddItem(Item("Insert Link", M_INSERT_LINK, 'K'));
	page->AddItem(Item("To-do", M_INSERT_TODO, 'T'));
	page->AddItem(Item("Attach File…", M_ATTACH_FILE, 'I'));
	page->AddItem(Item("Follow Link", M_FOLLOW_LINK_AT_CARET, B_ENTER));
	page->AddSeparatorItem();
	page->AddItem(Item("Back", M_BACK, '['));
	page->AddItem(Item("Forward", M_FORWARD, ']'));
	bar->AddItem(page);

	BMenu* view = new BMenu("View");
	view->AddItem(Item("Show Side Panel", M_TOGGLE_PANEL, 'J'));
	bar->AddItem(view);
	return bar;
}


// #pragma mark - vault


void
MainWindow::OpenVault(const char* path, const char* note)
{
	_SaveCurrent();
	_CloseNote();
	fBack.clear();
	fForward.clear();

	status_t status = fVault.Open(path, BMessenger(this));
	if (status != B_OK) {
		_Error("Could not open the notebooks folder", status);
		return;
	}
	if (fVault.Graph().NotePaths().empty() && fVault.Folders().empty())
		_CreateWelcome();

	fFirstTreeBuild = true;
	_RebuildTree();
	_UpdateTags();

	std::string open = note != NULL ? note : "";
	if (open.empty() || fVault.Graph().FindNote(open) == nullptr) {
		std::vector<std::string> notes = fVault.Graph().NotePaths();
		open = notes.empty() ? "" : notes.front();
		for (const std::string& n : notes) {
			if (Stem(n) == "Start Here")
				open = n;
		}
	}
	if (!open.empty())
		_OpenNote(open, -1, "", false);
}


void
MainWindow::OpenPath(const char* absPath)
{
	std::string rel = fVault.RelPath(absPath);
	if (!rel.empty() && fVault.Graph().FindNote(rel) != nullptr) {
		_OpenNote(rel);
		return;
	}
	// a file outside the current folder: open its folder as notebooks folder
	BPath path(absPath);
	BPath parent;
	if (path.GetParent(&parent) == B_OK) {
		OpenVault(parent.Path());
		rel = fVault.RelPath(absPath);
		if (fVault.Graph().FindNote(rel) != nullptr)
			_OpenNote(rel);
	}
}


void
MainWindow::_CreateWelcome()
{
	const std::string dir = "Welcome/Getting Started";
	fVault.CreateNote(dir, "Start Here", NULL,
		"# Start Here\n"
		"\n"
		"Folio keeps **notebooks** (top-level folders), **sections** "
		"(folders inside them) and **pages** (Markdown files). Everything "
		"is plain files in your notebooks folder, so other tools can read "
		"them too.\n"
		"\n"
		"## Links\n"
		"- Type `[[` to link to another page: [[Links and Renaming]]\n"
		"- Alt-click a link to follow it, or put the cursor in it and press "
		"Alt+Enter.\n"
		"- Links to pages that don't exist yet are red: [[My First Idea]]. "
		"Follow one to create the page.\n"
		"- Link to a heading: [[Links and Renaming#Try it]]\n"
		"- Give a link different text: [[To-dos and Tags|to-dos & tags]]\n"
		"\n"
		"The Backlinks tab on the right lists every page that links here.\n"
		"\n"
		"#welcome\n");
	fVault.CreateNote(dir, "Links and Renaming", NULL,
		"# Links and Renaming\n"
		"\n"
		"Rename a page — click its title above, type, press Enter — and "
		"every link to it is rewritten on the spot, in every notebook. "
		"Moving a page to another section, or renaming a whole section or "
		"notebook, works the same way. So do renames you make in Tracker "
		"while Folio is running.\n"
		"\n"
		"## Try it\n"
		"1. Look at the links to this page in [[Start Here]].\n"
		"2. Rename this page to `Linking 101`.\n"
		"3. Go back with Alt+[ and see the links now say `Linking 101`.\n"
		"\n"
		"Headings and display text survive renames: [[Start Here#Links]], "
		"[[Start Here|the welcome page]].\n"
		"\n"
		"#welcome\n");
	fVault.CreateNote(dir, "To-dos and Tags", NULL,
		"# To-dos and Tags\n"
		"\n"
		"- [ ] Click a box to check it off\n"
		"- [x] Press Alt+T to turn any line into a to-do\n"
		"- [ ] Press Enter at the end of a to-do to start the next one\n"
		"\n"
		"Tags like #welcome or #project/folio show up in the Tags tab. "
		"Search for `todo` to list every open to-do in every notebook.\n"
		"\n"
		"Tags and open to-do counts are also stored as file attributes "
		"(`FOLIO:tags`, `FOLIO:open_tasks`), so Tracker queries find them.\n"
		"\n"
		"Drop an image or any file onto a page to attach it. It is copied "
		"into the notebook's attachments folder and embedded with "
		"`![[name]]`, and shows in the strip under the page.\n");
}


// #pragma mark - notes


void
MainWindow::_CloseNote()
{
	fCurrent.clear();
	fDirty = false;
	fEditor->SetNote("");
	fEditor->MakeEditable(false);
	fTitle->SetText("");
	fTitle->SetEnabled(false);
	SetTitle("Folio");
	_UpdatePanels();
}


void
MainWindow::_OpenNote(const std::string& rel, int32 offset,
	const std::string& heading, bool pushHistory)
{
	if (rel != fCurrent) {
		_SaveCurrent();
		fVault.RefreshFromDisk(rel);
		const Note* note = fVault.Graph().FindNote(rel);
		if (note == nullptr)
			return;
		if (pushHistory && !fCurrent.empty()) {
			fBack.push_back(fCurrent);
			fForward.clear();
		}
		fCurrent = rel;
		fDirty = false;
		fEditor->MakeEditable(true);
		fEditor->SetNote(note->text);
		fTitle->SetEnabled(true);
		fTitle->SetText(Stem(rel).c_str());
		BString title("Folio — ");
		title << Stem(rel).c_str();
		SetTitle(title.String());
		_Restyle();
		_UpdatePanels();
		_SelectInTree(rel);
	}
	if (offset >= 0)
		fEditor->SelectAndShow(offset, 0);
	else if (!heading.empty())
		fEditor->ScrollToHeading(heading);
	fEditor->MakeFocus(true);
}


void
MainWindow::_SaveCurrent()
{
	delete fSaveRunner;
	fSaveRunner = NULL;
	if (!fDirty || fCurrent.empty())
		return;
	status_t status = fVault.SaveNote(fCurrent, fEditor->Content());
	if (status != B_OK) {
		_Error("Could not save the page", status);
		return;
	}
	fDirty = false;
	_UpdateTags();
}


void
MainWindow::_TextChanged()
{
	if (fCurrent.empty())
		return;
	fDirty = true;

	BMessage restyle(M_RESTYLE_TIMER);
	delete fRestyleRunner;
	fRestyleRunner = new BMessageRunner(BMessenger(this), &restyle,
		kRestyleDelay, 1);
	BMessage save(M_SAVE_TIMER);
	delete fSaveRunner;
	fSaveRunner = new BMessageRunner(BMessenger(this), &save, kSaveDelay, 1);
}


void
MainWindow::_Restyle()
{
	fEditor->Restyle(&fVault.Graph(), fCurrent);
	_UpdateAttachments();
}


// #pragma mark - panels


void
MainWindow::_UpdatePanels()
{
	_UpdateBacklinks();
	_UpdateAttachments();
	if (fSearch->Text()[0] != '\0')
		_RunSearch();
}


static void
ClearList(BListView* list)
{
	for (int32 i = list->CountItems() - 1; i >= 0; i--)
		delete list->RemoveItem(i);
}


void
MainWindow::_UpdateBacklinks()
{
	ClearList(fBacklinks);
	if (!fCurrent.empty()) {
		for (const Backlink& b : fVault.Graph().Backlinks(fCurrent)) {
			BString label;
			label << Stem(b.source).c_str() << ":  " << b.context.c_str();
			fBacklinks->AddItem(new HitItem(b.source, b.start, label));
		}
	}
	BString tab("Backlinks");
	if (fBacklinks->CountItems() > 0)
		tab << " (" << fBacklinks->CountItems() << ")";
	fTabs->TabAt(0)->SetLabel(tab.String());
	fTabs->Invalidate();
}


void
MainWindow::_UpdateTags()
{
	ClearList(fTags);
	for (const auto& t : fVault.Graph().TagCounts()) {
		BString label;
		label << "#" << t.first.c_str() << "  (" << t.second << ")";
		fTags->AddItem(new HitItem(t.first, -1, label));
	}
}


void
MainWindow::_UpdateAttachments()
{
	std::vector<std::string> files;
	if (!fCurrent.empty()) {
		std::string text = fEditor->Content();
		for (const md::Link& l : md::FindLinks(text)) {
			std::string target = fVault.Graph().Resolve(fCurrent, l);
			if (target.empty() || FileIndex::IsNote(target))
				continue;
			std::string abs = fVault.AbsPath(target);
			if (std::find(files.begin(), files.end(), abs) == files.end())
				files.push_back(abs);
		}
	}
	fStrip->SetFiles(files);
	if (files.empty() && !fStrip->IsHidden())
		fStrip->Hide();
	else if (!files.empty() && fStrip->IsHidden())
		fStrip->Show();
}


void
MainWindow::_RunSearch()
{
	ClearList(fResults);
	std::string query = fSearch->Text();
	if (query.empty())
		return;
	_SaveCurrent();
	for (const SearchHit& hit : fVault.Graph().Search(query)) {
		BString label;
		label << Stem(hit.path).c_str() << ":  " << hit.snippet.c_str();
		fResults->AddItem(new HitItem(hit.path, hit.offset, label));
	}
}


// #pragma mark - tree


void
MainWindow::_RebuildTree()
{
	std::set<std::string> expanded;
	for (int32 i = 0; i < fTree->FullListCountItems(); i++) {
		TreeItem* item = dynamic_cast<TreeItem*>(fTree->FullListItemAt(i));
		if (item != NULL && item->IsFolder() && item->IsExpanded())
			expanded.insert(item->Path());
	}
	std::vector<BListItem*> old;
	for (int32 i = 0; i < fTree->FullListCountItems(); i++)
		old.push_back(fTree->FullListItemAt(i));
	fTree->MakeEmpty();
	for (BListItem* item : old)
		delete item;

	std::map<std::string, std::vector<std::string> > folders, pages;
	for (const std::string& f : fVault.Folders()) {
		if (md::Basename(f) == kAttachmentFolder)
			continue;
		folders[md::Dirname(f)].push_back(f);
	}
	for (const std::string& p : fVault.Graph().NotePaths())
		pages[md::Dirname(p)].push_back(p);

	std::function<void(const std::string&, BListItem*, uint32)> add
		= [&](const std::string& dir, BListItem* parent, uint32 level) {
		std::vector<std::string>& subs = folders[dir];
		std::sort(subs.begin(), subs.end(), LessNoCase);
		for (const std::string& f : subs) {
			TreeItem* item = new TreeItem(f, true, level);
			item->SetExpanded(fFirstTreeBuild ? level == 0
				: expanded.count(f) > 0);
			if (parent == NULL)
				fTree->AddItem(item);
			else
				fTree->AddUnder(item, parent);
			add(f, item, level + 1);
		}
		std::vector<std::string>& list = pages[dir];
		std::sort(list.begin(), list.end(), LessNoCase);
		for (const std::string& p : list) {
			TreeItem* item = new TreeItem(p, false, level);
			if (parent == NULL)
				fTree->AddItem(item);
			else
				fTree->AddUnder(item, parent);
		}
	};
	add("", NULL, 0);
	fFirstTreeBuild = false;

	// Page ▸ Move To lists every notebook and section
	if (fMoveMenu != NULL) {
		while (BMenuItem* item = fMoveMenu->RemoveItem((int32)0))
			delete item;
		BMessage* top = new BMessage(M_MOVE_TO);
		top->AddString("to", "");
		fMoveMenu->AddItem(new BMenuItem("(top level)", top));
		std::vector<std::string> all(fVault.Folders().begin(),
			fVault.Folders().end());
		std::sort(all.begin(), all.end(), LessNoCase);
		for (const std::string& f : all) {
			if (md::Basename(f) == kAttachmentFolder)
				continue;
			BMessage* message = new BMessage(M_MOVE_TO);
			message->AddString("to", f.c_str());
			fMoveMenu->AddItem(new BMenuItem(f.c_str(), message));
		}
		fMoveMenu->SetTargetForItems(this);
	}

	if (!fCurrent.empty())
		_SelectInTree(fCurrent);
}


void
MainWindow::_SelectInTree(const std::string& rel)
{
	for (int32 i = 0; i < fTree->FullListCountItems(); i++) {
		TreeItem* item = dynamic_cast<TreeItem*>(fTree->FullListItemAt(i));
		if (item == NULL || item->Path() != rel)
			continue;
		for (BListItem* super = fTree->Superitem(item); super != NULL;
				super = fTree->Superitem(super)) {
			fTree->Expand(super);
		}
		int32 index = fTree->IndexOf(item);
		if (index >= 0 && fTree->CurrentSelection() != index) {
			fTree->Select(index);
			fTree->ScrollToSelection();
		}
		return;
	}
}


std::string
MainWindow::_SelectedPath() const
{
	TreeItem* item = dynamic_cast<TreeItem*>(
		fTree->ItemAt(fTree->CurrentSelection()));
	return item != NULL ? item->Path() : std::string();
}


std::string
MainWindow::_TargetDir() const
{
	std::string selected = _SelectedPath();
	if (!selected.empty())
		return fVault.IsFolder(selected) ? selected : md::Dirname(selected);
	if (!fCurrent.empty())
		return md::Dirname(fCurrent);
	return std::string();
}


// #pragma mark - changes


void
MainWindow::_ApplyChange(const VaultChange& change)
{
	auto mapPath = [&](std::string& p) {
		auto it = change.moved.find(p);
		if (it != change.moved.end())
			p = it->second;
	};
	for (std::string& p : fBack)
		mapPath(p);
	for (std::string& p : fForward)
		mapPath(p);

	std::string current = fCurrent;
	mapPath(current);
	bool removed = std::find(change.removed.begin(), change.removed.end(),
		fCurrent) != change.removed.end();

	if (!fCurrent.empty() && removed) {
		_CloseNote();
	} else if (!fCurrent.empty()) {
		bool renamed = current != fCurrent;
		fCurrent = current;
		if (change.rewritten.count(current) > 0) {
			// links inside the open page were updated on disk: reload,
			// keeping the caret near where it was
			int32 start, end;
			fEditor->GetSelection(&start, &end);
			const Note* note = fVault.Graph().FindNote(current);
			if (note != nullptr) {
				fEditor->SetNote(note->text);
				int32 length = fEditor->TextLength();
				fEditor->Select(std::min(start, length), std::min(end, length));
				fEditor->ScrollToSelection();
			}
			fDirty = false;
		} else if (fDirty)
			_SaveCurrent();
		if (renamed) {
			fTitle->SetText(Stem(current).c_str());
			BString title("Folio — ");
			title << Stem(current).c_str();
			SetTitle(title.String());
		}
		_Restyle();
	}

	if (change.structure)
		_RebuildTree();
	_UpdatePanels();
	_UpdateTags();
}


void
MainWindow::_Move(const std::string& from, const std::string& to)
{
	if (from == to)
		return;
	_SaveCurrent();
	VaultChange change;
	status_t status = fVault.Move(from, to, &change);
	if (status == B_FILE_EXISTS) {
		BString text("There already is a page or section called “");
		text << md::Basename(to).c_str() << "” there.";
		(new BAlert("Folio", text, "OK"))->Go();
		fTitle->SetText(Stem(fCurrent).c_str());
		return;
	}
	if (status != B_OK) {
		_Error("Could not rename or move", status);
		fTitle->SetText(Stem(fCurrent).c_str());
		return;
	}
	_ApplyChange(change);
}


void
MainWindow::_RenameFromTitle()
{
	if (fCurrent.empty())
		return;
	std::string name = Vault::CleanName(fTitle->Text());
	if (name.empty() || name == Stem(fCurrent)) {
		fTitle->SetText(Stem(fCurrent).c_str());
		return;
	}
	_Move(fCurrent, md::JoinPath(md::Dirname(fCurrent), name + ".md"));
}


// #pragma mark - links


void
MainWindow::_FollowLink(int32 offset)
{
	if (fCurrent.empty())
		return;
	std::string text = fEditor->Content();
	const md::Link* found = NULL;
	std::vector<md::Link> links = md::FindLinks(text);
	for (const md::Link& l : links) {
		if ((int32)l.start <= offset && offset <= (int32)l.end) {
			found = &l;
			break;
		}
	}
	if (found == NULL)
		return;
	const md::Link& link = *found;

	if (link.external) {
		OpenURL(link.target.c_str());
		return;
	}

	_SaveCurrent();
	std::string target = fVault.Graph().Resolve(fCurrent, link);
	if (!target.empty()) {
		if (FileIndex::IsNote(target)) {
			_OpenNote(target, -1, link.heading);
		} else {
			entry_ref ref;
			if (fVault.RefFor(target, &ref) == B_OK)
				OpenEntry(ref);
		}
		return;
	}
	if (link.target.empty()) {
		fEditor->ScrollToHeading(link.heading);
		return;
	}

	// Following a link to a page that doesn't exist creates it, next to the
	// current page (or at the path the link spells out).
	if (link.kind != md::kWikiLink || link.embed)
		return;
	std::string t = md::StripMdExtension(link.target);
	std::string dir = md::Dirname(fCurrent);
	if (t.find('/') != std::string::npos) {
		dir = md::NormalizePath(t[0] == '.'
			? md::JoinPath(md::Dirname(fCurrent), md::Dirname(t))
			: md::Dirname(t));
		t = md::Basename(t);
	}
	std::string rel;
	status_t status = fVault.CreateNote(dir, t, &rel,
		"# " + Vault::CleanName(t) + "\n\n");
	if (status != B_OK) {
		_Error("Could not create the page", status);
		return;
	}
	_RebuildTree();
	_OpenNote(rel);
	fEditor->SelectAndShow(fEditor->TextLength(), 0);
}


void
MainWindow::_CompleteLink(int32 offset)
{
	if (fCurrent.empty())
		return;
	std::vector<std::string> notes = fVault.Graph().NotePaths();
	std::sort(notes.begin(), notes.end(),
		[](const std::string& a, const std::string& b) {
			return LessNoCase(Stem(a), Stem(b));
		});

	BPopUpMenu* menu = new BPopUpMenu("links", false, false);
	std::map<std::string, BMenu*> notebooks;
	bool group = notes.size() > 40;
	for (const std::string& path : notes) {
		if (path == fCurrent)
			continue;
		BString label(Stem(path).c_str());
		std::string dir = md::Dirname(path);
		if (!group && !dir.empty())
			label << "   — " << dir.c_str();
		BMessage* message = new BMessage(M_COMPLETE_CHOSEN);
		message->AddString("target",
			fVault.Graph().WikiTargetFor(fCurrent, path).c_str());
		BMenuItem* item = new BMenuItem(label, message);
		if (!group) {
			menu->AddItem(item);
			continue;
		}
		std::string notebook = dir.empty() ? "(top level)"
			: md::TopComponent(path);
		BMenu*& sub = notebooks[notebook];
		if (sub == NULL)
			sub = new BMenu(notebook.c_str());
		sub->AddItem(item);
	}
	for (auto& n : notebooks)
		menu->AddItem(n.second);
	if (menu->CountItems() == 0) {
		delete menu;
		return;
	}

	float height = 0;
	BPoint where = fEditor->PointAt(offset, &height);
	where.y += height;
	fEditor->ConvertToScreen(&where);
	BMenuItem* chosen = menu->Go(where, false, true);
	if (chosen != NULL && chosen->Message() != NULL) {
		const char* target = chosen->Message()->FindString("target");
		std::string text = fEditor->Content();
		if (target != NULL && offset >= 2 && offset <= (int32)text.size()
			&& text.compare(offset - 2, 2, "[[") == 0) {
			std::string insert = target;
			if (text.compare(offset, 2, "]]") != 0)
				insert += "]]";
			fEditor->Insert(offset, insert.c_str(), insert.size());
			int32 caret = offset + insert.size();
			fEditor->Select(caret, caret);
		}
	}
	delete menu;
	fEditor->MakeFocus(true);
}


void
MainWindow::_InsertAttachments(BMessage* refs, int32 offset)
{
	if (fCurrent.empty())
		return;
	_SaveCurrent();
	std::string insert;
	entry_ref ref;
	for (int32 i = 0; refs->FindRef("refs", i, &ref) == B_OK; i++) {
		BEntry entry(&ref, true);
		if (entry.IsDirectory())
			continue;
		std::string rel;
		status_t status = fVault.ImportAttachment(ref, fCurrent, &rel);
		if (status != B_OK) {
			_Error("Could not attach the file", status);
			continue;
		}
		std::string target = fVault.Graph().WikiTargetFor(fCurrent, rel);
		if (!insert.empty())
			insert += "\n";
		insert += (IsImagePath(rel) ? "![[" : "[[") + target + "]]";
	}
	if (insert.empty())
		return;
	if (offset < 0) {
		int32 end;
		fEditor->GetSelection(&offset, &end);
	}
	fEditor->Insert(offset, insert.c_str(), insert.size());
	fEditor->Select(offset + insert.size(), offset + insert.size());
	fEditor->MakeFocus(true);
}


// #pragma mark - UI helpers


void
MainWindow::_ShowContextMenu(BMessage* message)
{
	BPoint where;
	message->FindPoint("where", &where);
	const char* pathString = message->FindString("path");
	std::string path = pathString != NULL ? pathString : "";
	bool folder = !path.empty() && fVault.IsFolder(path);
	std::string dir = path.empty() ? std::string()
		: (folder ? path : md::Dirname(path));

	BPopUpMenu* menu = new BPopUpMenu("context", false, false);
	BMessage* newPage = new BMessage(M_NEW_PAGE);
	newPage->AddString("dir", dir.c_str());
	menu->AddItem(new BMenuItem("New Page", newPage));
	BMessage* newSection = new BMessage(M_NEW_SECTION);
	newSection->AddString("dir", dir.c_str());
	menu->AddItem(new BMenuItem(dir.empty() ? "New Notebook…"
		: "New Section…", newSection));

	BMenu* moveMenu = NULL;
	if (!path.empty()) {
		menu->AddSeparatorItem();
		BMessage* rename = new BMessage(M_RENAME);
		rename->AddString("path", path.c_str());
		menu->AddItem(new BMenuItem("Rename…", rename));

		moveMenu = new BMenu("Move To");
		std::vector<std::string> targets;
		targets.push_back("");
		for (const std::string& f : fVault.Folders()) {
			if (md::Basename(f) != kAttachmentFolder
				&& (!folder || (f != path
					&& f.compare(0, path.size() + 1, path + "/") != 0))
				&& f != md::Dirname(path))
				targets.push_back(f);
		}
		std::sort(targets.begin(), targets.end(), LessNoCase);
		for (const std::string& t : targets) {
			if (t.empty() && md::Dirname(path).empty())
				continue;
			BMessage* move = new BMessage(M_MOVE_TO);
			move->AddString("path", path.c_str());
			move->AddString("to", t.c_str());
			moveMenu->AddItem(new BMenuItem(t.empty() ? "(top level)"
				: t.c_str(), move));
		}
		moveMenu->SetTargetForItems(this);
		menu->AddItem(moveMenu);

		BMessage* show = new BMessage(M_SHOW_IN_TRACKER);
		show->AddString("path", path.c_str());
		menu->AddItem(new BMenuItem(kShowInFolderLabel, show));
		menu->AddSeparatorItem();
		BMessage* trash = new BMessage(M_DELETE);
		trash->AddString("path", path.c_str());
		menu->AddItem(new BMenuItem("Move to Trash", trash));
	}
	menu->SetTargetForItems(this);
	menu->SetAsyncAutoDestruct(true);
	menu->Go(where, true, true, true);
}


void
MainWindow::_AskName(const char* title, const char* label,
	const char* initial, BMessage message)
{
	(new NameWindow(title, label, initial, BMessenger(this), message))->Show();
}


void
MainWindow::_Navigate(bool back)
{
	std::vector<std::string>& from = back ? fBack : fForward;
	std::vector<std::string>& to = back ? fForward : fBack;
	while (!from.empty()) {
		std::string path = from.back();
		from.pop_back();
		if (fVault.Graph().FindNote(path) == nullptr || path == fCurrent)
			continue;
		if (!fCurrent.empty())
			to.push_back(fCurrent);
		_OpenNote(path, -1, "", false);
		return;
	}
}


void
MainWindow::_Error(const char* what, status_t status)
{
	BString text(what);
	text << ":\n" << strerror(status);
	BAlert* alert = new BAlert("Folio", text, "OK", NULL, NULL,
		B_WIDTH_AS_USUAL, B_STOP_ALERT);
	alert->Go(NULL);
}


void
MainWindow::_SaveSettings()
{
	BPath path;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK)
		return;
	path.Append(kSettingsFile);
	BMessage settings;
	settings.AddString("vault", fVault.Root().Path());
	settings.AddRect("frame", Frame());
	settings.AddString("note", fCurrent.c_str());
	BFile file(path.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() == B_OK)
		settings.Flatten(&file);
}


bool
MainWindow::QuitRequested()
{
	_SaveCurrent();
	_SaveSettings();
	return true;
}


// #pragma mark - messages


void
MainWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case M_TEXT_CHANGED:
			_TextChanged();
			break;
		case M_RESTYLE_TIMER:
			_Restyle();
			break;
		case M_SAVE_TIMER:
			_SaveCurrent();
			_UpdateBacklinks();
			break;

		case M_TREE_SELECTED:
		{
			std::string path = _SelectedPath();
			if (!path.empty() && !fVault.IsFolder(path) && path != fCurrent)
				_OpenNote(path);
			break;
		}
		case M_TREE_CONTEXT:
			_ShowContextMenu(message);
			break;

		case M_TITLE_CHANGED:
			_RenameFromTitle();
			break;

		case M_NEW_PAGE:
		{
			const char* dirString = message->FindString("dir");
			std::string dir = dirString != NULL ? dirString : _TargetDir();
			if (dir.empty() && dirString == NULL) {
				// pages belong in a notebook: use the first one
				for (const std::string& f : fVault.Folders()) {
					if (f.find('/') == std::string::npos) {
						dir = f;
						break;
					}
				}
			}
			_SaveCurrent();
			std::string rel;
			status_t status = fVault.CreateNote(dir, "Untitled", &rel);
			if (status != B_OK) {
				_Error("Could not create the page", status);
				break;
			}
			_RebuildTree();
			_OpenNote(rel);
			// type the page name straight away, like in OneNote
			fTitle->MakeFocus(true);
			fTitle->TextView()->SelectAll();
			break;
		}
		case M_NEW_SECTION:
		case M_NEW_NOTEBOOK:
		{
			const char* dirString = message->FindString("dir");
			std::string dir = message->what == M_NEW_NOTEBOOK ? ""
				: (dirString != NULL ? dirString : _TargetDir());
			BMessage create(M_NAME_ENTERED);
			create.AddInt32("action", M_NEW_SECTION);
			create.AddString("dir", dir.c_str());
			_AskName(dir.empty() ? "New Notebook" : "New Section", "Name:",
				"", create);
			break;
		}
		case M_RENAME:
		{
			const char* pathString = message->FindString("path");
			std::string path = pathString != NULL ? pathString : fCurrent;
			if (path.empty())
				break;
			BMessage rename(M_NAME_ENTERED);
			rename.AddInt32("action", M_RENAME);
			rename.AddString("path", path.c_str());
			_AskName("Rename", "New name:",
				fVault.IsFolder(path) ? md::Basename(path).c_str()
					: Stem(path).c_str(), rename);
			break;
		}
		case M_NAME_ENTERED:
		{
			int32 action = message->GetInt32("action", 0);
			std::string name = Vault::CleanName(message->GetString("name", ""));
			if (name.empty())
				break;
			if (action == M_NEW_SECTION) {
				std::string dir = message->GetString("dir", "");
				std::string rel = md::JoinPath(dir,
					fVault.UniqueName(dir, name, ""));
				status_t status = fVault.CreateFolder(rel);
				if (status != B_OK)
					_Error("Could not create the section", status);
				_RebuildTree();
			} else if (action == M_RENAME) {
				std::string path = message->GetString("path", "");
				std::string dir = md::Dirname(path);
				_Move(path, md::JoinPath(dir,
					fVault.IsFolder(path) ? name : name + ".md"));
			}
			break;
		}
		case M_MOVE_TO:
		{
			const char* pathString = message->FindString("path");
			std::string path = pathString != NULL ? pathString : fCurrent;
			std::string to = message->GetString("to", "");
			if (!path.empty())
				_Move(path, md::JoinPath(to, md::Basename(path)));
			break;
		}
		case M_DELETE:
		{
			const char* pathString = message->FindString("path");
			std::string path = pathString != NULL ? pathString : fCurrent;
			if (path.empty())
				break;
			BString text("Move “");
			text << md::Basename(path).c_str() << "” to the Trash?";
			if (!fVault.IsFolder(path)) {
				size_t links = fVault.Graph().Backlinks(path).size();
				if (links > 0)
					text << "\n\n" << (int32)links
						<< " link(s) to it will show as missing.";
			} else
				text << "\n\nEverything inside it goes too.";
			BAlert* alert = new BAlert("Folio", text, "Cancel",
				"Move to Trash", NULL, B_WIDTH_AS_USUAL, B_WARNING_ALERT);
			alert->SetShortcut(0, B_ESCAPE);
			if (alert->Go() != 1)
				break;
			_SaveCurrent();
			VaultChange change;
			status_t status = fVault.Trash(path, &change);
			if (status != B_OK)
				_Error("Could not move to the Trash", status);
			else
				_ApplyChange(change);
			break;
		}
		case M_SHOW_IN_TRACKER:
		{
			const char* pathString = message->FindString("path");
			std::string path = pathString != NULL ? pathString : fCurrent;
			std::string dir = fVault.IsFolder(path) ? path : md::Dirname(path);
			entry_ref ref;
			if (fVault.RefFor(dir, &ref) == B_OK)
				OpenEntry(ref);
			break;
		}

		case M_INSERT_TODO:
		{
			int32 start, end;
			fEditor->GetSelection(&start, &end);
			fEditor->ToggleTaskAtLine(start);
			break;
		}
		case M_INSERT_LINK:
		{
			if (fCurrent.empty())
				break;
			int32 start, end;
			fEditor->GetSelection(&start, &end);
			fEditor->Insert(start, "[[", 2);
			fEditor->Select(start + 2, start + 2);
			_CompleteLink(start + 2);
			break;
		}
		case M_LINK_COMPLETE:
			_CompleteLink(message->GetInt32("offset", 0));
			break;
		case M_FOLLOW_LINK:
			_FollowLink(message->GetInt32("offset", 0));
			break;
		case M_FOLLOW_LINK_AT_CARET:
		{
			int32 start, end;
			fEditor->GetSelection(&start, &end);
			_FollowLink(start);
			break;
		}
		case M_BACK:
			_Navigate(true);
			break;
		case M_FORWARD:
			_Navigate(false);
			break;

		case M_ATTACH_FILE:
			if (fAttachPanel == NULL) {
				BMessenger target(this);
				fAttachPanel = new BFilePanel(B_OPEN_PANEL, &target, NULL,
					B_FILE_NODE, true, new BMessage(M_ATTACH_PANEL_REFS));
				fAttachPanel->Window()->SetTitle("Folio: Attach file");
			}
			fAttachPanel->Show();
			break;
		case M_ATTACH_PANEL_REFS:
			_InsertAttachments(message, -1);
			break;
		case M_DROP_REFS:
			_InsertAttachments(message, message->GetInt32("offset", -1));
			break;

		case M_OPEN_VAULT:
			if (fVaultPanel == NULL) {
				BMessenger target(this);
				fVaultPanel = new BFilePanel(B_OPEN_PANEL, &target, NULL,
					B_DIRECTORY_NODE, false, new BMessage(M_VAULT_CHOSEN));
				fVaultPanel->Window()->SetTitle("Folio: Open notebooks folder");
			}
			fVaultPanel->Show();
			break;
		case M_VAULT_CHOSEN:
		{
			entry_ref ref;
			if (message->FindRef("refs", &ref) == B_OK) {
				BPath path(&ref);
				OpenVault(path.Path());
			}
			break;
		}

		case M_FIND:
			if (fPanel->IsHidden())
				fPanel->Show();
			fTabs->Select(2);
			fSearch->MakeFocus(true);
			fSearch->TextView()->SelectAll();
			break;
		case M_TOGGLE_PANEL:
			if (fPanel->IsHidden())
				fPanel->Show();
			else
				fPanel->Hide();
			break;
		case M_SEARCH_CHANGED:
			_RunSearch();
			break;
		case M_SEARCH_INVOKED:
		case M_BACKLINK_INVOKED:
		{
			BListView* list = message->what == M_SEARCH_INVOKED
				? fResults : fBacklinks;
			HitItem* hit = dynamic_cast<HitItem*>(
				list->ItemAt(list->CurrentSelection()));
			if (hit != NULL) {
				std::string path = hit->Path();
				int32 offset = hit->Offset();
				_OpenNote(path, offset);
			}
			break;
		}
		case M_TAG_INVOKED:
		{
			HitItem* tag = dynamic_cast<HitItem*>(
				fTags->ItemAt(fTags->CurrentSelection()));
			if (tag == NULL)
				break;
			BString query("#");
			query << tag->Path().c_str();
			fSearch->SetText(query.String());
			fTabs->Select(2);
			_RunSearch();
			break;
		}

		case B_NODE_MONITOR:
		{
			// Keep unsaved edits: the planner rewrites links in what's on
			// screen, not in the older copy on disk.
			if (fDirty && !fCurrent.empty())
				fVault.Graph().SetNote(fCurrent, fEditor->Content());
			VaultChange change;
			fVault.HandleNodeMonitor(message, &change);
			if (!change.Empty())
				_ApplyChange(change);
			break;
		}

		default:
			BWindow::MessageReceived(message);
	}
}

#include "Widgets.h"

#include <algorithm>
#include <cstring>

#ifdef FOLIO_COSMOE
#include <spawn.h>
#include <sys/wait.h>
#endif

#include <Bitmap.h>
#include <Button.h>
#include <LayoutBuilder.h>
#include <MimeType.h>
#include <NodeInfo.h>
#include <Path.h>
#include <Roster.h>
#include <StringView.h>
#include <TextControl.h>
#include <TranslationUtils.h>

#include "Constants.h"
#include "MarkdownScan.h"


#ifdef FOLIO_COSMOE
extern char** environ;


static status_t
XdgOpen(const char* target)
{
	char* argv[] = {(char*)"xdg-open", (char*)target, NULL};
	pid_t pid;
	if (posix_spawnp(&pid, "xdg-open", NULL, NULL, argv, environ) != 0)
		return B_ERROR;
	// xdg-open hands off and exits quickly; don't leave a zombie behind.
	waitpid(pid, NULL, 0);
	return B_OK;
}
#endif


status_t
OpenEntry(const entry_ref& ref)
{
#ifdef FOLIO_COSMOE
	BPath path(&ref);
	if (path.InitCheck() != B_OK)
		return path.InitCheck();
	return XdgOpen(path.Path());
#else
	return be_roster->Launch(&ref);
#endif
}


status_t
OpenURL(const char* url)
{
#ifdef FOLIO_COSMOE
	return XdgOpen(url);
#else
	std::string scheme(url, strcspn(url, ":"));
	std::string mime = "application/x-vnd.Be.URL." + md::ToLower(scheme);
	const char* args[] = {url, NULL};
	return be_roster->Launch(mime.c_str(), 1, args);
#endif
}


static const float kThumbHeight = 84;
static const float kCellWidth = 120;
static const float kStripHeight = kThumbHeight + 30;


static std::string
DisplayName(const std::string& path, bool folder)
{
	std::string name = md::Basename(path);
	return folder ? name : md::StripMdExtension(name);
}


// #pragma mark - TreeItem


TreeItem::TreeItem(const std::string& path, bool folder, uint32 level)
	:
	BStringItem(DisplayName(path, folder).c_str(), level, true),
	fPath(path),
	fFolder(folder)
{
}


void
TreeItem::DrawItem(BView* owner, BRect frame, bool complete)
{
	if (!fFolder) {
		BStringItem::DrawItem(owner, frame, complete);
		return;
	}
	BFont saved;
	owner->GetFont(&saved);
	owner->SetFont(be_bold_font);
	BStringItem::DrawItem(owner, frame, complete);
	owner->SetFont(&saved);
}


void
TreeItem::Update(BView* owner, const BFont* font)
{
	BStringItem::Update(owner, fFolder ? be_bold_font : font);
}


// #pragma mark - NotebookTree


NotebookTree::NotebookTree(const char* name)
	:
	BOutlineListView(name, B_SINGLE_SELECTION_LIST)
{
}


void
NotebookTree::MouseDown(BPoint where)
{
	int32 buttons = 0;
	if (Window()->CurrentMessage() != NULL)
		Window()->CurrentMessage()->FindInt32("buttons", &buttons);
	if ((buttons & B_SECONDARY_MOUSE_BUTTON) == 0) {
		BOutlineListView::MouseDown(where);
		return;
	}
	int32 index = IndexOf(where);
	if (index >= 0)
		Select(index);
	BMessage context(M_TREE_CONTEXT);
	BPoint screen = ConvertToScreen(where);
	context.AddPoint("where", screen);
	TreeItem* item = index >= 0 ? dynamic_cast<TreeItem*>(ItemAt(index)) : NULL;
	if (item != NULL)
		context.AddString("path", item->Path().c_str());
	Window()->PostMessage(&context);
}


// #pragma mark - HitItem


HitItem::HitItem(const std::string& path, int32 offset, const char* label)
	:
	BStringItem(label),
	fPath(path),
	fOffset(offset)
{
}


// #pragma mark - AttachmentStrip


AttachmentStrip::AttachmentStrip()
	:
	BView("attachments", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	SetExplicitMinSize(BSize(B_SIZE_UNSET, kStripHeight));
	SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, kStripHeight));
}


AttachmentStrip::~AttachmentStrip()
{
	_Clear();
}


void
AttachmentStrip::_Clear()
{
	for (Entry& e : fEntries)
		delete e.bitmap;
	fEntries.clear();
}


void
AttachmentStrip::SetFiles(const std::vector<std::string>& paths)
{
	_Clear();
	float x = 8;
	for (const std::string& path : paths) {
		Entry e;
		e.path = path;
		e.label = md::Basename(path);
		e.isIcon = false;
		e.bitmap = BTranslationUtils::GetBitmapFile(path.c_str());
		if (e.bitmap == NULL) {
			// not an image: show the file's Tracker icon instead
			entry_ref ref;
			if (get_ref_for_path(path.c_str(), &ref) == B_OK) {
				BBitmap* icon = new BBitmap(BRect(0, 0, 47, 47), B_RGBA32);
				if (BNodeInfo::GetTrackerIcon(&ref, icon, B_LARGE_ICON)
						== B_OK)
					e.bitmap = icon;
				else
					delete icon;
			}
			e.isIcon = true;
		}
		e.frame = BRect(x, 6, x + kCellWidth - 10, 6 + kThumbHeight);
		x += kCellWidth;
		fEntries.push_back(e);
	}
	Invalidate();
}


void
AttachmentStrip::Draw(BRect updateRect)
{
	SetHighUIColor(B_PANEL_TEXT_COLOR);
	SetDrawingMode(B_OP_ALPHA);
	for (const Entry& e : fEntries) {
		if (e.bitmap != NULL) {
			BRect src = e.bitmap->Bounds();
			BRect dst = e.frame;
			// keep aspect ratio inside the cell
			float scale = std::min(dst.Width() / src.Width(),
				dst.Height() / src.Height());
			if (e.isIcon)
				scale = std::min(scale, 1.0f);
			float w = src.Width() * scale;
			float h = src.Height() * scale;
			dst.left += (dst.Width() - w) / 2;
			dst.top += (dst.Height() - h) / 2;
			dst.right = dst.left + w;
			dst.bottom = dst.top + h;
			DrawBitmap(e.bitmap, src, dst, B_FILTER_BITMAP_BILINEAR);
		}
		BString label(e.label.c_str());
		be_plain_font->TruncateString(&label, B_TRUNCATE_MIDDLE,
			e.frame.Width());
		float width = StringWidth(label.String());
		DrawString(label.String(), BPoint(e.frame.left
			+ (e.frame.Width() - width) / 2, e.frame.bottom + 16));
	}
	SetDrawingMode(B_OP_COPY);
	SetHighUIColor(B_PANEL_BACKGROUND_COLOR, B_DARKEN_2_TINT);
	StrokeLine(Bounds().LeftTop(), Bounds().RightTop());
}


void
AttachmentStrip::MouseDown(BPoint where)
{
	for (const Entry& e : fEntries) {
		BRect cell = e.frame;
		cell.bottom += 20;
		if (!cell.Contains(where))
			continue;
		entry_ref ref;
		if (get_ref_for_path(e.path.c_str(), &ref) == B_OK)
			OpenEntry(ref);
		return;
	}
}


// #pragma mark - NameWindow


NameWindow::NameWindow(const char* title, const char* label,
	const char* initial, BMessenger target, const BMessage& message)
	:
	BWindow(BRect(0, 0, 360, 100), title, B_MODAL_WINDOW_LOOK,
		B_MODAL_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_NOT_V_RESIZABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_CLOSE_ON_ESCAPE),
	fTarget(target),
	fMessage(message)
{
	fName = new BTextControl("name", label, initial, NULL);
	BButton* cancel = new BButton("Cancel", new BMessage(B_QUIT_REQUESTED));
	BButton* ok = new BButton("OK", new BMessage(M_NAME_ENTERED));

	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_SPACING)
		.Add(fName)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(cancel)
			.Add(ok)
		.End();

	SetDefaultButton(ok);
	fName->MakeFocus(true);
	fName->TextView()->SelectAll();
	CenterOnScreen();
}


void
NameWindow::MessageReceived(BMessage* message)
{
	if (message->what != M_NAME_ENTERED) {
		BWindow::MessageReceived(message);
		return;
	}
	BString name = fName->Text();
	name.Trim();
	if (name.IsEmpty())
		return;
	BMessage result(fMessage);
	result.AddString("name", name);
	fTarget.SendMessage(&result);
	Quit();
}

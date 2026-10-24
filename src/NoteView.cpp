#include "NoteView.h"

#include <cctype>
#include <vector>

#include <Cursor.h>
#include <Window.h>

#include "Constants.h"
#include "LinkGraph.h"
#include "MarkdownScan.h"


enum {
	A_BOLD		= 1 << 0,
	A_ITALIC	= 1 << 1,
	A_CODE		= 1 << 2,
	A_LINK		= 1 << 3,
	A_BROKEN	= 1 << 4,
	A_TAG		= 1 << 5,
	A_SYNTAX	= 1 << 6,
	A_DONE		= 1 << 7,
	A_QUOTE		= 1 << 8,
	A_BOX		= 1 << 9,
};

struct Attr {
	uint16	flags = 0;
	uint8	heading = 0;

	bool operator==(const Attr& o) const
		{ return flags == o.flags && heading == o.heading; }
	bool operator!=(const Attr& o) const { return !(*this == o); }
};

static const float kHeadingScale[] = {1.0f, 1.7f, 1.45f, 1.25f, 1.12f,
	1.05f, 1.0f};


static rgb_color
Accent(rgb_color color)
{
	rgb_color background = ui_color(B_DOCUMENT_BACKGROUND_COLOR);
	if (background.IsDark())
		return mix_color(color, make_color(255, 255, 255), 110);
	return color;
}


NoteView::NoteView(const char* name)
	:
	BTextView(name),
	fLoading(false),
	fHandCursor(false)
{
	SetStylable(true);
	SetDoesUndo(true);
	SetWordWrap(true);
	SetInsets(24, 16, 24, 16);
	SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
}


void
NoteView::SetNote(const std::string& text)
{
	fLoading = true;
	SetText(text.c_str(), text.size());
	Select(0, 0);
	ScrollToOffset(0);
	fLoading = false;
}


std::string
NoteView::Content() const
{
	return std::string(Text(), TextLength());
}


void
NoteView::Restyle(const LinkGraph* graph, const std::string& path)
{
	std::string text = Content();
	const size_t n = text.size();
	std::vector<Attr> attrs(n);

	auto mark = [&](size_t s, size_t e, uint16 flag) {
		for (size_t i = s; i < e && i < n; i++)
			attrs[i].flags |= flag;
	};

	for (const md::Span& span : md::StyleSpans(text)) {
		switch (span.kind) {
			case md::kHeading:
				for (size_t i = span.start; i < span.end && i < n; i++)
					attrs[i].heading = span.level;
				break;
			case md::kBold:			mark(span.start, span.end, A_BOLD); break;
			case md::kItalic:		mark(span.start, span.end, A_ITALIC); break;
			case md::kCode:
			case md::kCodeBlock:	mark(span.start, span.end, A_CODE); break;
			case md::kSyntax:		mark(span.start, span.end, A_SYNTAX); break;
			case md::kLinkSpan:		mark(span.start, span.end, A_LINK); break;
			case md::kTagSpan:		mark(span.start, span.end, A_TAG); break;
			case md::kTaskBox:		mark(span.start, span.end, A_BOX); break;
			case md::kTaskDone:		mark(span.start, span.end, A_DONE); break;
			case md::kQuote:		mark(span.start, span.end, A_QUOTE); break;
		}
	}
	if (graph != NULL) {
		for (const md::Link& l : md::FindLinks(text)) {
			if (!l.external && !l.target.empty()
				&& graph->Resolve(path, l).empty())
				mark(l.start, l.end, A_BROKEN);
		}
	}

	std::vector<std::pair<int32, Attr> > runs;
	for (size_t i = 0; i < n; i++) {
		if (i == 0 || attrs[i] != attrs[i - 1])
			runs.push_back(std::make_pair((int32)i, attrs[i]));
	}
	if (runs.empty())
		runs.push_back(std::make_pair(0, Attr()));

	BFont plain(be_plain_font);
	const float size = plain.Size() * 1.15f;
	const rgb_color textColor = ui_color(B_DOCUMENT_TEXT_COLOR);

	text_run_array* array = AllocRunArray(runs.size());
	if (array == NULL)
		return;
	for (size_t k = 0; k < runs.size(); k++) {
		const Attr& a = runs[k].second;
		text_run& run = array->runs[k];
		run.offset = runs[k].first;

		BFont font((a.flags & A_CODE) != 0 ? be_fixed_font : be_plain_font);
		font.SetSize(size * kHeadingScale[a.heading]
			* ((a.flags & A_CODE) != 0 ? 0.92f : 1.0f));
		uint16 face = 0;
		if ((a.flags & A_BOLD) != 0 || a.heading > 0)
			face |= B_BOLD_FACE;
		if ((a.flags & A_ITALIC) != 0)
			face |= B_ITALIC_FACE;
		if ((a.flags & A_LINK) != 0 && (a.flags & A_SYNTAX) == 0)
			face |= B_UNDERSCORE_FACE;
		if ((a.flags & A_DONE) != 0 && (a.flags & A_LINK) == 0)
			face |= B_STRIKEOUT_FACE;
		font.SetFace(face != 0 ? face : B_REGULAR_FACE);
		run.font = font;

		rgb_color color = textColor;
		if ((a.flags & A_SYNTAX) != 0)
			color = Accent(kSyntaxColor);
		else if ((a.flags & A_DONE) != 0)
			color = Accent(kDoneColor);
		else if ((a.flags & A_BROKEN) != 0)
			color = Accent(kBrokenLinkColor);
		else if ((a.flags & A_LINK) != 0)
			color = Accent(kLinkColor);
		else if ((a.flags & A_TAG) != 0)
			color = Accent(kTagColor);
		else if ((a.flags & A_BOX) != 0)
			color = Accent(kLinkColor);
		else if ((a.flags & A_CODE) != 0)
			color = Accent(kCodeColor);
		else if ((a.flags & A_QUOTE) != 0)
			color = Accent(kQuoteColor);
		run.color = color;
	}
	SetRunArray(0, n, array);
	FreeRunArray(array);
}


// #pragma mark - lines, tasks, links


static void
LineBounds(const std::string& text, int32 offset, size_t* start, size_t* end)
{
	size_t s = offset > 0 ? text.rfind('\n', offset - 1) : std::string::npos;
	*start = s == std::string::npos ? 0 : s + 1;
	size_t e = text.find('\n', offset);
	*end = e == std::string::npos ? text.size() : e;
}


int32
NoteView::_TaskBoxAt(int32 offset) const
{
	std::string text = Content();
	size_t s, e;
	LineBounds(text, offset, &s, &e);
	std::vector<md::Task> tasks = md::FindTasks(text.substr(s, e - s));
	return tasks.empty() ? -1 : (int32)(s + tasks[0].boxStart);
}


int32
NoteView::_LinkAt(int32 offset, int32* end) const
{
	for (const md::Link& l : md::FindLinks(Content())) {
		if ((int32)l.start <= offset && offset < (int32)l.end) {
			if (end != NULL)
				*end = l.end;
			return l.start;
		}
	}
	return -1;
}


void
NoteView::ToggleTaskAtLine(int32 offset)
{
	int32 selStart, selEnd;
	GetSelection(&selStart, &selEnd);
	int32 box = _TaskBoxAt(offset);
	if (box >= 0) {
		char c = ByteAt(box + 1) == ' ' ? 'x' : ' ';
		Delete(box + 1, box + 2);
		Insert(box + 1, &c, 1);
		Select(selStart, selEnd);
		return;
	}
	// Not a to-do yet: turn the line into one.
	std::string text = Content();
	size_t s, e;
	LineBounds(text, offset, &s, &e);
	size_t p = s;
	while (p < e && (text[p] == ' ' || text[p] == '\t'))
		p++;
	const char* prefix = "- [ ] ";
	int32 removed = 0;
	if (p + 1 < e && (text[p] == '-' || text[p] == '*' || text[p] == '+')
		&& text[p + 1] == ' ') {
		Delete(p, p + 2);	// already a bullet: replace its marker
		removed = 2;
	}
	Insert(p, prefix, 6);
	int32 shift = 6 - removed;
	Select(selStart + shift, selEnd + shift);
}


void
NoteView::ScrollToHeading(const std::string& heading)
{
	std::string text = Content();
	std::string want = md::ToLower(heading);
	size_t i = 0;
	while (i < text.size()) {
		size_t e = text.find('\n', i);
		if (e == std::string::npos)
			e = text.size();
		size_t p = i;
		while (p < e && text[p] == '#')
			p++;
		if (p > i && p < e && text[p] == ' ') {
			std::string title = md::ToLower(text.substr(p + 1, e - p - 1));
			if (title == want) {
				SelectAndShow(i, 0);
				return;
			}
		}
		i = e + 1;
	}
}


void
NoteView::SelectAndShow(int32 offset, int32 length)
{
	Select(offset, offset + length);
	ScrollToSelection();
}


// #pragma mark - input


void
NoteView::MouseDown(BPoint where)
{
	int32 buttons = 0;
	if (Window()->CurrentMessage() != NULL)
		Window()->CurrentMessage()->FindInt32("buttons", &buttons);

	if ((buttons & B_PRIMARY_MOUSE_BUTTON) != 0) {
		int32 offset = OffsetAt(where);

		// a plain click on [ ] / [x] toggles the to-do
		int32 box = _TaskBoxAt(offset);
		if (box >= 0 && LineAt(where) == LineAt(box)) {
			float left = PointAt(box).x;
			float right = PointAt(box + 3).x;
			if (where.x >= left && where.x <= right) {
				ToggleTaskAtLine(box);
				return;
			}
		}

		// Alt-click (the Haiku "command" key) follows a link
		if ((modifiers() & B_COMMAND_KEY) != 0 && _LinkAt(offset, NULL) >= 0) {
			BMessage follow(M_FOLLOW_LINK);
			follow.AddInt32("offset", offset);
			Window()->PostMessage(&follow);
			return;
		}
	}
	BTextView::MouseDown(where);
}


void
NoteView::MouseMoved(BPoint where, uint32 transit, const BMessage* drag)
{
	BTextView::MouseMoved(where, transit, drag);
	if (drag != NULL || transit == B_EXITED_VIEW)
		return;

	int32 offset = OffsetAt(where);
	bool hand = false;
	if ((modifiers() & B_COMMAND_KEY) != 0)
		hand = _LinkAt(offset, NULL) >= 0;
	if (!hand) {
		int32 box = _TaskBoxAt(offset);
		hand = box >= 0 && LineAt(where) == LineAt(box)
			&& where.x >= PointAt(box).x && where.x <= PointAt(box + 3).x;
	}
	if (hand) {
		BCursor cursor(B_CURSOR_ID_FOLLOW_LINK);
		SetViewCursor(&cursor);
	}
}


// Returns the prefix to continue a list item with, or "" if not a list.
static std::string
ListContinuation(const std::string& line, bool* empty)
{
	size_t p = 0;
	while (p < line.size() && (line[p] == ' ' || line[p] == '\t'))
		p++;
	std::string indent = line.substr(0, p);
	std::string marker;
	if (p < line.size() && (line[p] == '-' || line[p] == '*' || line[p] == '+')
		&& p + 1 < line.size() && line[p + 1] == ' ') {
		marker = line.substr(p, 2);
		p += 2;
	} else if (p < line.size() && isdigit((unsigned char)line[p])) {
		size_t q = p;
		while (q < line.size() && isdigit((unsigned char)line[q]))
			q++;
		if (q + 1 < line.size() && (line[q] == '.' || line[q] == ')')
			&& line[q + 1] == ' ') {
			int number = atoi(line.substr(p, q - p).c_str()) + 1;
			marker = std::to_string(number) + line[q] + " ";
			p = q + 2;
		}
	}
	if (marker.empty())
		return std::string();
	if (line.compare(p, 4, "[ ] ") == 0 || line.compare(p, 4, "[x] ") == 0
		|| line.compare(p, 4, "[X] ") == 0) {
		marker += "[ ] ";
		p += 4;
	}
	*empty = line.find_first_not_of(" \t", p) == std::string::npos;
	return indent + marker;
}


void
NoteView::KeyDown(const char* bytes, int32 numBytes)
{
	if (numBytes == 1 && bytes[0] == B_ENTER && IsEditable()) {
		int32 s, e;
		GetSelection(&s, &e);
		if (s == e) {
			std::string text = Content();
			size_t ls, le;
			LineBounds(text, s, &ls, &le);
			bool empty = false;
			std::string prefix = ListContinuation(text.substr(ls, s - ls),
				&empty);
			if (!prefix.empty()) {
				if (empty) {
					// Enter on an empty item ends the list
					Delete(ls, s);
					Select(ls, ls);
				} else {
					std::string insert = "\n" + prefix;
					Insert(s, insert.c_str(), insert.size());
					Select(s + insert.size(), s + insert.size());
				}
				ScrollToSelection();
				return;
			}
		}
	}

	BTextView::KeyDown(bytes, numBytes);

	if (numBytes == 1 && bytes[0] == '[') {
		int32 s, e;
		GetSelection(&s, &e);
		if (s >= 2 && ByteAt(s - 1) == '[' && ByteAt(s - 2) == '[') {
			BMessage complete(M_LINK_COMPLETE);
			complete.AddInt32("offset", s);
			Window()->PostMessage(&complete);
		}
	}
}


void
NoteView::MessageReceived(BMessage* message)
{
	if (message->WasDropped() && message->HasRef("refs")) {
		BPoint drop = message->DropPoint();
		ConvertFromScreen(&drop);
		BMessage refs(*message);
		refs.what = M_DROP_REFS;
		refs.AddInt32("offset", OffsetAt(drop));
		Window()->PostMessage(&refs);
		return;
	}
	BTextView::MessageReceived(message);
}


void
NoteView::InsertText(const char* text, int32 length, int32 offset,
	const text_run_array* runs)
{
	BTextView::InsertText(text, length, offset, runs);
	_Changed();
}


void
NoteView::DeleteText(int32 fromOffset, int32 toOffset)
{
	BTextView::DeleteText(fromOffset, toOffset);
	_Changed();
}


void
NoteView::_Changed()
{
	if (!fLoading && Window() != NULL)
		Window()->PostMessage(M_TEXT_CHANGED);
}

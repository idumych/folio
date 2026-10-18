/*
 * NoteView — the page editor. A stylable BTextView that renders the Markdown
 * subset live (headings, emphasis, code, links, tags, to-dos), lets you
 * click checkboxes, Alt-click links, autocompletes [[links]] and accepts
 * dropped files.
 */
#ifndef NOTE_VIEW_H
#define NOTE_VIEW_H

#include <string>

#include <TextView.h>

class LinkGraph;


class NoteView : public BTextView {
public:
								NoteView(const char* name);

			void				SetNote(const std::string& text);
			std::string			Content() const;
			void				Restyle(const LinkGraph* graph,
									const std::string& path);

			void				ToggleTaskAtLine(int32 offset);
			void				ScrollToHeading(const std::string& heading);
			void				SelectAndShow(int32 offset, int32 length);

	virtual	void				MouseDown(BPoint where);
	virtual	void				MouseMoved(BPoint where, uint32 transit,
									const BMessage* dragMessage);
	virtual	void				KeyDown(const char* bytes, int32 numBytes);
	virtual	void				MessageReceived(BMessage* message);

protected:
	virtual	void				InsertText(const char* text, int32 length,
									int32 offset, const text_run_array* runs);
	virtual	void				DeleteText(int32 fromOffset, int32 toOffset);

private:
			void				_Changed();
			int32				_LinkAt(int32 offset, int32* end) const;
			int32				_TaskBoxAt(int32 offset) const;

			bool				fLoading;
			bool				fHandCursor;
};


#endif	// NOTE_VIEW_H

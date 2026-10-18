/*
 * Small UI pieces: the notebook tree, the attachment strip under the editor,
 * a name prompt, and list items carrying a page path + offset.
 */
#ifndef WIDGETS_H
#define WIDGETS_H

#include <string>
#include <vector>

#include <Entry.h>
#include <ListItem.h>
#include <Messenger.h>
#include <OutlineListView.h>
#include <View.h>
#include <Window.h>

class BBitmap;
class BTextControl;


class TreeItem : public BStringItem {
public:
								TreeItem(const std::string& path, bool folder,
									uint32 level);

			const std::string&	Path() const { return fPath; }
			bool				IsFolder() const { return fFolder; }

	virtual	void				DrawItem(BView* owner, BRect frame,
									bool complete);
	virtual	void				Update(BView* owner, const BFont* font);

private:
			std::string			fPath;
			bool				fFolder;
};


class NotebookTree : public BOutlineListView {
public:
								NotebookTree(const char* name);
	virtual	void				MouseDown(BPoint where);
};


// One result row: page path, an offset into it, and a display string.
class HitItem : public BStringItem {
public:
								HitItem(const std::string& path, int32 offset,
									const char* label);
			const std::string&	Path() const { return fPath; }
			int32				Offset() const { return fOffset; }

private:
			std::string			fPath;
			int32				fOffset;
};


class AttachmentStrip : public BView {
public:
								AttachmentStrip();
	virtual						~AttachmentStrip();

			// absolute paths of files the current page links or embeds
			void				SetFiles(const std::vector<std::string>& paths);

	virtual	void				Draw(BRect updateRect);
	virtual	void				MouseDown(BPoint where);

private:
			struct Entry {
				std::string	path;
				std::string	label;
				BBitmap*	bitmap;
				bool		isIcon;
				BRect		frame;
			};
			void				_Clear();

			std::vector<Entry>	fEntries;
};


class NameWindow : public BWindow {
public:
								NameWindow(const char* title,
									const char* label, const char* initial,
									BMessenger target, const BMessage& message);

	virtual	void				MessageReceived(BMessage* message);

private:
			BTextControl*		fName;
			BMessenger			fTarget;
			BMessage			fMessage;
};


#endif	// WIDGETS_H

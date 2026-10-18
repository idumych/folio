#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <string>
#include <vector>

#include <Window.h>

#include "MarkdownScan.h"
#include "Vault.h"

class AttachmentStrip;
class BFilePanel;
class BListView;
class BMenu;
class BMenuBar;
class BMessageRunner;
class BTabView;
class BTextControl;
class BView;
class NotebookTree;
class NoteView;


class MainWindow : public BWindow {
public:
								MainWindow(BRect frame, const char* vaultPath,
									const char* lastNote);
	virtual						~MainWindow();

	virtual	void				MessageReceived(BMessage* message);
	virtual	bool				QuitRequested();

			void				OpenVault(const char* path,
									const char* note = NULL);
			void				OpenPath(const char* absPath);

private:
			BMenuBar*			_BuildMenus();
			void				_OpenNote(const std::string& rel,
									int32 offset = -1,
									const std::string& heading = "",
									bool pushHistory = true);
			void				_CloseNote();
			void				_SaveCurrent();
			void				_TextChanged();
			void				_Restyle();
			void				_UpdatePanels();
			void				_UpdateBacklinks();
			void				_UpdateTags();
			void				_UpdateAttachments();
			void				_RunSearch();
			void				_RebuildTree();
			void				_SelectInTree(const std::string& rel);
			std::string			_TargetDir() const;
			std::string			_SelectedPath() const;
			void				_ApplyChange(const VaultChange& change);
			void				_Move(const std::string& from,
									const std::string& to);
			void				_RenameFromTitle();
			void				_FollowLink(int32 offset);
			void				_CompleteLink(int32 offset);
			void				_InsertAttachments(BMessage* refs,
									int32 offset);
			void				_ShowContextMenu(BMessage* message);
			void				_AskName(const char* title, const char* label,
									const char* initial, BMessage message);
			void				_Navigate(bool back);
			void				_CreateWelcome();
			void				_Error(const char* what, status_t status);
			void				_SaveSettings();

			Vault				fVault;
			std::string			fCurrent;
			bool				fDirty;

			NotebookTree*		fTree;
			BTextControl*		fTitle;
			NoteView*			fEditor;
			AttachmentStrip*	fStrip;
			BTabView*			fTabs;
			BView*				fPanel;
			BListView*			fBacklinks;
			BListView*			fTags;
			BTextControl*		fSearch;
			BListView*			fResults;
			BMenu*				fMoveMenu;

			BMessageRunner*		fSaveRunner;
			BMessageRunner*		fRestyleRunner;
			BFilePanel*			fAttachPanel;
			BFilePanel*			fVaultPanel;

			std::vector<std::string> fBack;
			std::vector<std::string> fForward;
			bool				fFirstTreeBuild;
};


#endif	// MAIN_WINDOW_H

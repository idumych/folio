#include <Alert.h>
#include <Application.h>
#include <File.h>
#include <FindDirectory.h>
#include <Path.h>
#include <Screen.h>

#include "Constants.h"
#include "MainWindow.h"


class FolioApp : public BApplication {
public:
	FolioApp()
		:
		BApplication(kAppSignature),
		fWindow(NULL)
	{
	}

	virtual void ReadyToRun()
	{
		if (fWindow == NULL)
			_CreateWindow(NULL);
	}

	virtual void RefsReceived(BMessage* message)
	{
		entry_ref ref;
		if (message->FindRef("refs", &ref) != B_OK)
			return;
		BPath path(&ref);
		if (fWindow == NULL) {
			_CreateWindow(path.Path());
			return;
		}
		if (fWindow->Lock()) {
			fWindow->OpenPath(path.Path());
			fWindow->Activate();
			fWindow->Unlock();
		}
	}

	virtual void ArgvReceived(int32 argc, char** argv)
	{
		BMessage refs(B_REFS_RECEIVED);
		for (int32 i = 1; i < argc; i++) {
			entry_ref ref;
			if (get_ref_for_path(argv[i], &ref) == B_OK)
				refs.AddRef("refs", &ref);
		}
		if (refs.HasRef("refs"))
			RefsReceived(&refs);
	}

	virtual void AboutRequested()
	{
		(new BAlert("About Folio", "Folio\n\nNotebooks, sections and pages "
			"in plain Markdown, with [[links]] that follow your renames.",
			"OK"))->Go(NULL);
	}

private:
	void _CreateWindow(const char* openPath)
	{
		BPath vault;
		find_directory(B_USER_DIRECTORY, &vault);
		vault.Append("Notebooks");
		BRect frame(80, 60, 1240, 820);
		BString note;

		BPath settingsPath;
		if (find_directory(B_USER_SETTINGS_DIRECTORY, &settingsPath) == B_OK) {
			settingsPath.Append(kSettingsFile);
			BFile file(settingsPath.Path(), B_READ_ONLY);
			BMessage settings;
			if (file.InitCheck() == B_OK && settings.Unflatten(&file) == B_OK) {
				const char* saved;
				if (settings.FindString("vault", &saved) == B_OK)
					vault.SetTo(saved);
				BRect savedFrame;
				if (settings.FindRect("frame", &savedFrame) == B_OK
					&& BScreen().Frame().Intersects(savedFrame))
					frame = savedFrame;
				settings.FindString("note", &note);
			}
		}

		fWindow = new MainWindow(frame, vault.Path(), note.String());
		if (openPath != NULL && fWindow->Lock()) {
			fWindow->OpenPath(openPath);
			fWindow->Unlock();
		}
		fWindow->Show();
	}

	MainWindow*	fWindow;
};


int
main()
{
	FolioApp app;
	app.Run();
	return 0;
}

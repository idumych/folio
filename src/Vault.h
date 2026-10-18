/*
 * Vault — a folder on disk holding notebooks (top-level folders), sections
 * (sub-folders, any depth) and pages (.md files). Owns the LinkGraph, keeps
 * BFS attributes in sync, and watches the folder tree so that renames done
 * in Tracker or Terminal also update links.
 */
#ifndef VAULT_H
#define VAULT_H

#include <map>
#include <set>
#include <string>
#include <vector>

#include <Messenger.h>
#include <Node.h>
#include <Path.h>
#include <String.h>

#include "LinkGraph.h"


struct VaultChange {
	std::vector<std::string>			added;
	std::vector<std::string>			removed;
	std::map<std::string, std::string>	moved;		// old -> new (files)
	std::set<std::string>				rewritten;	// notes whose text changed
	bool								structure = false;	// tree changed

	bool Empty() const { return added.empty() && removed.empty()
		&& moved.empty() && rewritten.empty() && !structure; }
};


class Vault {
public:
								Vault();
								~Vault();

			status_t			Open(const char* path, BMessenger watcher);
			void				Close();
			const BPath&		Root() const { return fRoot; }
			bool				IsOpen() const { return fRoot.InitCheck() == B_OK; }

			LinkGraph&			Graph() { return fGraph; }
			const std::set<std::string>& Folders() const { return fFolders; }
			bool				IsFolder(const std::string& rel) const
									{ return fFolders.count(rel) > 0; }

			std::string			AbsPath(const std::string& rel) const;
			status_t			RefFor(const std::string& rel,
									entry_ref* ref) const;
			std::string			RelPath(const char* absPath) const;

			// Writes a page and re-indexes it.
			status_t			SaveNote(const std::string& rel,
									const std::string& text);
			// Re-reads a page if it changed on disk since it was loaded.
			bool				RefreshFromDisk(const std::string& rel);

	static	std::string			CleanName(const std::string& name);
			std::string			UniqueName(const std::string& dir,
									const std::string& base,
									const char* extension) const;
			status_t			CreateNote(const std::string& dir,
									const std::string& title,
									std::string* _rel,
									const std::string& body = std::string());
			status_t			CreateFolder(const std::string& rel);

			// Renames or moves a page, attachment or whole folder and
			// rewrites every link that pointed into it.
			status_t			Move(const std::string& from,
									const std::string& to,
									VaultChange* change);
			status_t			Trash(const std::string& rel,
									VaultChange* change);

			// Copies a file into "<notebook>/attachments" for the given page.
			status_t			ImportAttachment(const entry_ref& ref,
									const std::string& forNote,
									std::string* _rel);

			// Handles a B_NODE_MONITOR message; fills change.
			void				HandleNodeMonitor(BMessage* message,
									VaultChange* change);

private:
			void				_Scan(const std::string& relDir);
			void				_AddEntry(const std::string& rel,
									const node_ref& nref, bool isDir);
			void				_RemoveSubtree(const std::string& rel,
									VaultChange* change);
			status_t			_LoadNote(const std::string& rel);
			void				_WriteAttributes(const std::string& rel);
			status_t			_WriteFile(const std::string& rel,
									const std::string& text);
			std::map<std::string, std::string> _ExpandMove(
									const std::string& from,
									const std::string& to) const;
			void				_MoveBookkeeping(const std::string& from,
									const std::string& to);
			void				_ApplyLinkRewrites(
									const std::map<std::string, std::string>&
										fileMoves,
									const std::map<std::string, std::string>&
										rewritten,
									VaultChange* change);
			void				_EnsureIndices();
			bool				_Ignored(const std::string& name) const;
			void				_Watch(const std::string& rel,
									const node_ref& nref);
			void				_Unwatch(const std::string& rel,
									const node_ref& nref);
			bool				_RelIfInside(const std::string& absPath,
									std::string* rel) const;
			bool				_EventDir(BMessage* message,
									const char* refField,
									const char* inodeField, const char* name,
									bool entryGone, std::string* rel) const;
			bool				_EventEntry(BMessage* message,
									const char* refField,
									const char* inodeField, const char* name,
									bool entryGone, std::string* rel) const;

			BPath				fRoot;
			dev_t				fDevice;
			BMessenger			fWatcher;
			LinkGraph			fGraph;
			std::set<std::string> fFolders;
			std::map<node_ref, std::string> fNodes;	// every entry we know
			std::map<std::string, node_ref> fNodeOf;
			std::map<std::string, bigtime_t> fModified;
};


#endif	// VAULT_H

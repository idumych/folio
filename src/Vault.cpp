#include "Vault.h"

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <sys/stat.h>

#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <NodeInfo.h>
#include <NodeMonitor.h>
#include <Volume.h>
#include <fs_index.h>

#include "Constants.h"


static bool
HasPrefixPath(const std::string& path, const std::string& prefix)
{
	return path == prefix
		|| (path.size() > prefix.size()
			&& path.compare(0, prefix.size(), prefix) == 0
			&& path[prefix.size()] == '/');
}


Vault::Vault()
	:
	fDevice(-1)
{
	fRoot.Unset();
}


Vault::~Vault()
{
	Close();
}


status_t
Vault::Open(const char* path, BMessenger watcher)
{
	Close();
	create_directory(path, 0755);

	BEntry entry(path, true);
	if (!entry.IsDirectory())
		return B_NOT_A_DIRECTORY;
	status_t status = fRoot.SetTo(&entry);
	if (status != B_OK)
		return status;

	struct stat st;
	entry.GetStat(&st);
	fDevice = st.st_dev;
	fWatcher = watcher;

	_EnsureIndices();
	_Scan("");
	return B_OK;
}


void
Vault::Close()
{
	if (fWatcher.IsValid())
		stop_watching(fWatcher);
	fWatcher = BMessenger();
	fGraph.Clear();
	fFolders.clear();
	fNodes.clear();
	fNodeOf.clear();
	fModified.clear();
	fRoot.Unset();
}


bool
Vault::_Ignored(const std::string& name) const
{
	return name.empty() || name[0] == '.';
}


std::string
Vault::AbsPath(const std::string& rel) const
{
	BPath path(fRoot);
	if (!rel.empty())
		path.Append(rel.c_str());
	return path.Path() != NULL ? path.Path() : "";
}


status_t
Vault::RefFor(const std::string& rel, entry_ref* ref) const
{
	return get_ref_for_path(AbsPath(rel).c_str(), ref);
}


std::string
Vault::RelPath(const char* absPath) const
{
	std::string abs = absPath;
	std::string root = fRoot.Path();
	if (abs == root)
		return std::string();
	if (abs.size() > root.size() && abs.compare(0, root.size(), root) == 0
		&& abs[root.size()] == '/')
		return abs.substr(root.size() + 1);
	return std::string();
}


void
Vault::_EnsureIndices()
{
	// Lets Tracker queries find pages by tag or open to-dos.
	fs_create_index(fDevice, kAttrTags, B_STRING_TYPE, 0);
	fs_create_index(fDevice, kAttrOpenTasks, B_INT32_TYPE, 0);
}


void
Vault::_AddEntry(const std::string& rel, const node_ref& nref, bool isDir)
{
	fNodes[nref] = rel;
	fNodeOf[rel] = nref;
	if (isDir && !rel.empty())
		fFolders.insert(rel);
	if (isDir)
		_Watch(rel, nref);
}


// Haiku and VitruvianOS watch a folder by node, so a watch survives the
// folder being renamed. Cosmoe only watches by path (its watch_node() is
// unsupported), so moved folders are re-watched in _MoveBookkeeping().
void
Vault::_Watch(const std::string& rel, const node_ref& nref)
{
#ifdef FOLIO_COSMOE
	(void)nref;
	watch_path(AbsPath(rel).c_str(), B_WATCH_DIRECTORY, fWatcher);
#else
	watch_node(&nref, B_WATCH_DIRECTORY, fWatcher);
#endif
}


void
Vault::_Unwatch(const std::string& rel, const node_ref& nref)
{
#ifdef FOLIO_COSMOE
	(void)nref;
	watch_path(AbsPath(rel).c_str(), B_STOP_WATCHING, fWatcher);
#else
	(void)rel;
	watch_node(&nref, B_STOP_WATCHING, fWatcher);
#endif
}


void
Vault::_Scan(const std::string& relDir)
{
	BDirectory dir(AbsPath(relDir).c_str());
	if (dir.InitCheck() != B_OK)
		return;
	node_ref dirRef;
	dir.GetNodeRef(&dirRef);
	_AddEntry(relDir, dirRef, true);

	BEntry entry;
	while (dir.GetNextEntry(&entry) == B_OK) {
		char name[B_FILE_NAME_LENGTH];
		entry.GetName(name);
		if (_Ignored(name) || entry.IsSymLink())
			continue;
		std::string rel = md::JoinPath(relDir, name);
		if (entry.IsDirectory()) {
			_Scan(rel);
			continue;
		}
		node_ref nref;
		entry.GetNodeRef(&nref);
		_AddEntry(rel, nref, false);
		if (FileIndex::IsNote(rel))
			_LoadNote(rel);
		else
			fGraph.AddFile(rel);
	}
}


status_t
Vault::_LoadNote(const std::string& rel)
{
	BFile file(AbsPath(rel).c_str(), B_READ_ONLY);
	status_t status = file.InitCheck();
	if (status != B_OK)
		return status;
	off_t size = 0;
	file.GetSize(&size);
	std::string text;
	text.resize(size);
	ssize_t read = size > 0 ? file.Read(&text[0], size) : 0;
	if (read < 0)
		return read;
	text.resize(read);
	fGraph.SetNote(rel, text);

	time_t modified = 0;
	file.GetModificationTime(&modified);
	fModified[rel] = modified;
	return B_OK;
}


bool
Vault::RefreshFromDisk(const std::string& rel)
{
	struct stat st;
	if (stat(AbsPath(rel).c_str(), &st) != 0)
		return false;
	auto it = fModified.find(rel);
	if (it != fModified.end() && it->second == st.st_mtime)
		return false;
	_LoadNote(rel);
	return true;
}


status_t
Vault::_WriteFile(const std::string& rel, const std::string& text)
{
	std::string abs = AbsPath(rel);
	BFile file(abs.c_str(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	status_t status = file.InitCheck();
	if (status != B_OK)
		return status;
	ssize_t written = file.Write(text.data(), text.size());
	if (written < 0)
		return written;
	if ((size_t)written != text.size())
		return B_IO_ERROR;

	BNodeInfo info(&file);
	char type[B_MIME_TYPE_LENGTH];
	if (info.GetType(type) != B_OK)
		info.SetType(kMarkdownMimeType);

	time_t modified = 0;
	file.GetModificationTime(&modified);
	fModified[rel] = modified;
	return B_OK;
}


void
Vault::_WriteAttributes(const std::string& rel)
{
	const Note* note = fGraph.FindNote(rel);
	if (note == nullptr)
		return;
	BNode node(AbsPath(rel).c_str());
	if (node.InitCheck() != B_OK)
		return;

	std::set<std::string> seen;
	BString tags;
	for (const md::Tag& t : note->tags) {
		if (!seen.insert(md::ToLower(t.name)).second)
			continue;
		if (tags.Length() > 0)
			tags << " ";
		tags << "#" << t.name.c_str();
	}
	node.WriteAttrString(kAttrTags, &tags);

	int32 open = 0;
	for (const md::Task& t : note->tasks) {
		if (!t.done)
			open++;
	}
	node.WriteAttr(kAttrOpenTasks, B_INT32_TYPE, 0, &open, sizeof(open));
}


status_t
Vault::SaveNote(const std::string& rel, const std::string& text)
{
	status_t status = _WriteFile(rel, text);
	if (status != B_OK)
		return status;
	fGraph.SetNote(rel, text);
	_WriteAttributes(rel);
	return B_OK;
}


std::string
Vault::CleanName(const std::string& base)
{
	std::string clean;
	for (char c : base) {
		if (c == '/')
			clean += '-';
		else if (c == '\n' || c == '\r' || c == '\t')
			clean += ' ';
		else if (c == '[' || c == ']' || c == '|' || c == '#' || c == '^')
			continue;	// would break [[links]]
		else
			clean += c;
	}
	while (!clean.empty() && (clean[0] == '.' || clean[0] == ' '))
		clean.erase(0, 1);
	while (!clean.empty() && clean.back() == ' ')
		clean.pop_back();
	if (clean.size() > 200)
		clean.resize(200);
	return clean;
}


std::string
Vault::UniqueName(const std::string& dir, const std::string& base,
	const char* extension) const
{
	std::string clean = CleanName(base);
	if (clean.empty())
		clean = "Untitled";

	std::string ext = extension != NULL ? extension : "";
	for (int i = 1; ; i++) {
		std::string name = clean;
		if (i > 1) {
			char suffix[16];
			snprintf(suffix, sizeof(suffix), " %d", i);
			name += suffix;
		}
		std::string rel = md::JoinPath(dir, name + ext);
		if (!fGraph.Exists(rel) && !IsFolder(rel)) {
			struct stat st;
			if (stat(AbsPath(rel).c_str(), &st) != 0)
				return name + ext;
		}
	}
}


status_t
Vault::CreateFolder(const std::string& rel)
{
	if (rel.empty() || IsFolder(rel))
		return B_OK;
	std::string parent = md::Dirname(rel);
	if (!parent.empty() && !IsFolder(parent)) {
		status_t status = CreateFolder(parent);
		if (status != B_OK)
			return status;
	}
	std::string abs = AbsPath(rel);
	if (mkdir(abs.c_str(), 0755) != 0 && errno != EEXIST)
		return errno;
	BDirectory dir(abs.c_str());
	node_ref nref;
	dir.GetNodeRef(&nref);
	_AddEntry(rel, nref, true);
	return B_OK;
}


status_t
Vault::CreateNote(const std::string& dir, const std::string& title,
	std::string* _rel, const std::string& body)
{
	if (!dir.empty()) {
		status_t status = CreateFolder(dir);
		if (status != B_OK)
			return status;
	}
	std::string rel = md::JoinPath(dir, UniqueName(dir, title, ".md"));
	status_t status = _WriteFile(rel, body);
	if (status != B_OK)
		return status;

	BEntry entry(AbsPath(rel).c_str());
	node_ref nref;
	entry.GetNodeRef(&nref);
	_AddEntry(rel, nref, false);
	fGraph.SetNote(rel, body);
	_WriteAttributes(rel);
	if (_rel != NULL)
		*_rel = rel;
	return B_OK;
}


std::map<std::string, std::string>
Vault::_ExpandMove(const std::string& from, const std::string& to) const
{
	std::map<std::string, std::string> moves;
	if (IsFolder(from)) {
		for (const std::string& p : fGraph.AllPaths()) {
			if (HasPrefixPath(p, from))
				moves[p] = to + p.substr(from.size());
		}
	} else
		moves[from] = to;
	return moves;
}


void
Vault::_MoveBookkeeping(const std::string& from, const std::string& to)
{
	std::vector<std::pair<std::string, node_ref> > entries;
	for (auto it = fNodeOf.begin(); it != fNodeOf.end();) {
		if (!from.empty() && HasPrefixPath(it->first, from)) {
			entries.push_back(*it);
			it = fNodeOf.erase(it);
		} else
			++it;
	}
	for (auto& e : entries) {
		std::string newRel = to + e.first.substr(from.size());
		fNodeOf[newRel] = e.second;
		fNodes[e.second] = newRel;
		if (fFolders.erase(e.first) > 0) {
			fFolders.insert(newRel);
#ifdef FOLIO_COSMOE
			_Unwatch(e.first, e.second);
			_Watch(newRel, e.second);
#endif
		}
		auto m = fModified.find(e.first);
		if (m != fModified.end()) {
			time_t t = m->second;
			fModified.erase(m);
			fModified[newRel] = t;
		}
	}
}


void
Vault::_ApplyLinkRewrites(const std::map<std::string, std::string>& fileMoves,
	const std::map<std::string, std::string>& rewritten, VaultChange* change)
{
	fGraph.ApplyMoves(fileMoves, rewritten);
	for (const auto& r : rewritten) {
		_WriteFile(r.first, r.second);
		_WriteAttributes(r.first);
		change->rewritten.insert(r.first);
	}
	for (const auto& m : fileMoves)
		change->moved[m.first] = m.second;
	change->structure = true;
}


status_t
Vault::Move(const std::string& from, const std::string& to,
	VaultChange* change)
{
	if (from == to || from.empty() || to.empty())
		return B_OK;
	if (IsFolder(from) && HasPrefixPath(to, from))
		return B_BAD_VALUE;		// folder into itself
	if (!md::EqualsNoCase(from, to) && (fGraph.Exists(to) || IsFolder(to)))
		return B_FILE_EXISTS;

	std::map<std::string, std::string> moves = _ExpandMove(from, to);
	std::map<std::string, std::string> rewritten = fGraph.PlanMoves(moves);

	std::string parent = md::Dirname(to);
	if (!parent.empty() && !IsFolder(parent)) {
		status_t status = CreateFolder(parent);
		if (status != B_OK)
			return status;
	}
	if (rename(AbsPath(from).c_str(), AbsPath(to).c_str()) != 0)
		return errno;

	_MoveBookkeeping(from, to);
	_ApplyLinkRewrites(moves, rewritten, change);
	return B_OK;
}


void
Vault::_RemoveSubtree(const std::string& rel, VaultChange* change)
{
	for (auto it = fNodeOf.begin(); it != fNodeOf.end();) {
		if (!rel.empty() && HasPrefixPath(it->first, rel)) {
			if (fFolders.erase(it->first) > 0)
				_Unwatch(it->first, it->second);
			else {
				fGraph.Remove(it->first);
				change->removed.push_back(it->first);
			}
			fModified.erase(it->first);
			fNodes.erase(it->second);
			it = fNodeOf.erase(it);
		} else
			++it;
	}
	change->structure = true;
}


status_t
Vault::Trash(const std::string& rel, VaultChange* change)
{
	BVolume volume(fDevice);
	BPath trash;
	status_t status = find_directory(B_TRASH_DIRECTORY, &trash, true, &volume);
	if (status != B_OK)
		return status;

	std::string abs = AbsPath(rel);
	std::string name = md::Basename(rel);
	BPath target(trash);
	target.Append(name.c_str());
	for (int i = 2; BEntry(target.Path()).Exists(); i++) {
		char unique[B_FILE_NAME_LENGTH];
		snprintf(unique, sizeof(unique), "%s %d", name.c_str(), i);
		target = trash;
		target.Append(unique);
	}
	// VitruvianOS (like other Linux desktops) uses the freedesktop.org Trash:
	// ".../Trash/files" plus a ".../Trash/info/<name>.trashinfo" record,
	// which is what Tracker reads to restore. Haiku's Trash has no info/.
	std::string infoPath;
	if (strcmp(trash.Leaf(), "files") == 0) {
		BPath info;
		trash.GetParent(&info);
		info.Append("info");
		create_directory(info.Path(), 0700);
		info.Append((std::string(target.Leaf()) + ".trashinfo").c_str());
		infoPath = info.Path();

		std::string encoded;
		for (unsigned char c : abs) {
			if (isalnum(c) || strchr("/-_.~", c) != NULL)
				encoded += (char)c;
			else {
				char hex[4];
				snprintf(hex, sizeof(hex), "%%%02X", c);
				encoded += hex;
			}
		}
		char date[32];
		time_t now = time(NULL);
		strftime(date, sizeof(date), "%Y-%m-%dT%H:%M:%S", localtime(&now));
		std::string record = "[Trash Info]\nPath=" + encoded
			+ "\nDeletionDate=" + date + "\n";
		BFile file(infoPath.c_str(), B_WRITE_ONLY | B_CREATE_FILE
			| B_FAIL_IF_EXISTS);
		if (file.InitCheck() == B_OK)
			file.Write(record.data(), record.size());
		else
			infoPath.clear();
	}

	if (rename(abs.c_str(), target.Path()) != 0) {
		status_t error = errno;
		if (!infoPath.empty())
			unlink(infoPath.c_str());
		return error;
	}

	// lets Tracker's "Restore" put it back
	BNode node(target.Path());
	BString original(abs.c_str());
	node.WriteAttrString("_trk/original_path", &original);

	_RemoveSubtree(rel, change);
	return B_OK;
}


status_t
Vault::ImportAttachment(const entry_ref& ref, const std::string& forNote,
	std::string* _rel)
{
	BPath source(&ref);
	std::string inside = RelPath(source.Path());
	if (!inside.empty() && fGraph.Exists(inside)) {
		*_rel = inside;		// already in the vault: just link to it
		return B_OK;
	}

	std::string notebook = md::Dirname(forNote).empty()
		? std::string() : md::TopComponent(forNote);
	std::string dir = md::JoinPath(notebook, kAttachmentFolder);
	status_t status = CreateFolder(dir);
	if (status != B_OK)
		return status;

	// Cosmoe's entry_ref carries the full path as its name; keep the leaf.
	std::string name = md::Basename(ref.name);
	std::string stem = name, ext;
	size_t dot = name.rfind('.');
	if (dot != std::string::npos && dot > 0) {
		stem = name.substr(0, dot);
		ext = name.substr(dot);
	}
	std::string rel = md::JoinPath(dir, UniqueName(dir, stem, ext.c_str()));

	BFile in(&ref, B_READ_ONLY);
	if ((status = in.InitCheck()) != B_OK)
		return status;
	BFile out(AbsPath(rel).c_str(), B_WRITE_ONLY | B_CREATE_FILE | B_FAIL_IF_EXISTS);
	if ((status = out.InitCheck()) != B_OK)
		return status;
	char buffer[64 * 1024];
	ssize_t got;
	while ((got = in.Read(buffer, sizeof(buffer))) > 0) {
		if (out.Write(buffer, got) != got)
			return B_IO_ERROR;
	}
	char type[B_MIME_TYPE_LENGTH];
	if (BNodeInfo(&in).GetType(type) == B_OK)
		BNodeInfo(&out).SetType(type);

	node_ref nref;
	out.GetNodeRef(&nref);
	_AddEntry(rel, nref, false);
	fGraph.AddFile(rel);
	*_rel = rel;
	return B_OK;
}


// Node monitor messages differ between systems: Haiku identifies the node
// and folders by "device"/"node"/"directory" numbers, VitruvianOS sends a
// "virtual:node" node_ref and "virtual:…directory" entry_refs, and Cosmoe
// sends the numbers as dev_t/ino_t fields and the entry's full path as its
// "name". All are handled, with a path-based fallback for nodes that are
// already gone.

// Reads an integer field whatever its declared type: Haiku stores device
// and inode numbers as int32/int64, Cosmoe as its own dev_t/ino_t types.
static bool
FindNumber(BMessage* message, const char* field, int64* value)
{
	type_code type;
	if (message->GetInfo(field, &type) != B_OK)
		return false;
	const void* data;
	ssize_t size;
	if (message->FindData(field, type, &data, &size) != B_OK)
		return false;
	if (size == sizeof(int32))
		*value = *(const int32*)data;
	else if (size == sizeof(int64))
		*value = *(const int64*)data;
	else
		return false;
	return true;
}


static bool
EventNode(BMessage* message, node_ref* node)
{
	if (message->FindNodeRef("virtual:node", node) == B_OK)
		return true;
	int64 device;
	int64 inode;
	if (FindNumber(message, "device", &device)
		&& FindNumber(message, "node", &inode)) {
		*node = node_ref((dev_t)device, (ino_t)inode);
		return true;
	}
	return false;
}


static bool
PathExists(const std::string& path, bool* isDirectory = NULL)
{
	struct stat st;
	if (stat(path.c_str(), &st) != 0)
		return false;
	if (isDirectory != NULL)
		*isDirectory = S_ISDIR(st.st_mode);
	return true;
}


bool
Vault::_RelIfInside(const std::string& absPath, std::string* rel) const
{
	std::string root = fRoot.Path();
	std::string r;
	if (absPath == root)
		r = std::string();
	else if (absPath.size() > root.size()
		&& absPath.compare(0, root.size(), root) == 0
		&& absPath[root.size()] == '/')
		r = absPath.substr(root.size() + 1);
	else
		return false;
	for (const std::string& part : md::SplitPath(r)) {
		if (_Ignored(part))
			return false;	// inside a hidden folder such as .git
	}
	*rel = r;
	return true;
}


bool
Vault::_EventDir(BMessage* message, const char* refField,
	const char* inodeField, const char* name, bool entryGone,
	std::string* rel) const
{
	entry_ref ref;
	if (message->FindRef(refField, &ref) == B_OK) {
		BPath path(&ref);
		if (path.InitCheck() != B_OK)
			return false;
		std::string p = path.Path();
		// The ref names either the folder itself or the entry inside it.
		bool isFolder = false;
		bool folderItself;
		if (entryGone) {
			folderItself = PathExists(p, &isFolder) && isFolder;
		} else {
			folderItself = name != NULL
				&& PathExists(md::JoinPath(p, name));
		}
		return _RelIfInside(folderItself ? p : md::Dirname(p), rel);
	}

	int64 device;
	int64 inode;
	if (FindNumber(message, "device", &device)
		&& FindNumber(message, inodeField, &inode)) {
		auto it = fNodes.find(node_ref((dev_t)device, (ino_t)inode));
		if (it == fNodes.end())
			return false;
		*rel = it->second;
		return true;
	}
	return false;
}


// The vault-relative path of the entry an event names.
bool
Vault::_EventEntry(BMessage* message, const char* refField,
	const char* inodeField, const char* name, bool entryGone,
	std::string* rel) const
{
	if (name == NULL)
		return false;
	// Cosmoe sends the entry's full path as its name.
	if (name[0] == '/') {
		std::string r;
		if (!_RelIfInside(name, &r) || r.empty())
			return false;
		*rel = r;
		return true;
	}
	std::string dirRel;
	if (!_EventDir(message, refField, inodeField, name, entryGone, &dirRel))
		return false;
	*rel = md::JoinPath(dirRel, name);
	return true;
}


void
Vault::HandleNodeMonitor(BMessage* message, VaultChange* change)
{
	int32 opcode;
	if (message->FindInt32("opcode", &opcode) != B_OK)
		return;

	node_ref nref;
	bool haveNode = EventNode(message, &nref);
	auto known = haveNode ? fNodes.find(nref) : fNodes.end();
	const char* name = NULL;
	message->FindString("name", &name);

	if (opcode == B_ENTRY_CREATED || opcode == B_ENTRY_MOVED) {
		bool created = opcode == B_ENTRY_CREATED;
		std::string newRel;
		if (!_EventEntry(message,
				created ? "virtual:directory" : "virtual:to directory",
				created ? "directory" : "to directory", name, false, &newRel))
			newRel.clear();
		bool ignoredName = name == NULL || _Ignored(md::Basename(name));

		std::string oldRel;
		if (known != fNodes.end())
			oldRel = known->second;
		else if (!created) {
			const char* fromName = NULL;
			std::string candidate;
			if (message->FindString("from name", &fromName) == B_OK
				&& _EventEntry(message, "virtual:from directory",
					"from directory", fromName, true, &candidate)
				&& fNodeOf.count(candidate) > 0)
				oldRel = candidate;
		}

		if (!oldRel.empty()) {
			if (oldRel == newRel)
				return;		// our own change, already accounted for
			if (newRel.empty() || ignoredName) {
				_RemoveSubtree(oldRel, change);	// moved out of the vault
				return;
			}
			// Renamed or moved elsewhere (Tracker, Terminal…): the files
			// already moved, so plan the rewrites against the old model.
			std::map<std::string, std::string> moves
				= _ExpandMove(oldRel, newRel);
			std::map<std::string, std::string> rewritten
				= fGraph.PlanMoves(moves);
			_MoveBookkeeping(oldRel, newRel);
			_ApplyLinkRewrites(moves, rewritten, change);
			return;
		}

		if (newRel.empty() || ignoredName || fNodeOf.count(newRel) > 0)
			return;
		BEntry entry(AbsPath(newRel).c_str());
		if (!entry.Exists() || entry.IsSymLink())
			return;
		if (entry.IsDirectory()) {
			_Scan(newRel);
		} else {
			entry.GetNodeRef(&nref);
			_AddEntry(newRel, nref, false);
			if (FileIndex::IsNote(newRel))
				_LoadNote(newRel);
			else
				fGraph.AddFile(newRel);
		}
		change->added.push_back(newRel);
		change->structure = true;
		return;
	}

	if (opcode == B_ENTRY_REMOVED) {
		std::string oldRel;
		if (known != fNodes.end())
			oldRel = known->second;
		else {
			std::string candidate;
			if (_EventEntry(message, "virtual:directory", "directory", name,
					true, &candidate) && fNodeOf.count(candidate) > 0)
				oldRel = candidate;
		}
		if (!oldRel.empty())
			_RemoveSubtree(oldRel, change);
	}
}

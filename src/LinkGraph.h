/*
 * LinkGraph — the in-memory model of a vault: every note's text, every
 * attachment path, link resolution, backlinks, search, tags, and the planner
 * that rewrites links when files or folders move.
 *
 * Portable C++ (no Haiku headers); paths are vault-relative with '/'.
 */
#ifndef LINK_GRAPH_H
#define LINK_GRAPH_H

#include <map>
#include <set>
#include <string>
#include <vector>

#include "MarkdownScan.h"


struct Note {
	std::string				path;
	std::string				text;
	std::string				lower;		// lowercased text for search
	std::vector<md::Link>	links;
	std::vector<md::Tag>	tags;
	std::vector<md::Task>	tasks;
};


// The set of files that link targets are resolved against. Kept separate
// from LinkGraph so the rename planner can resolve against the vault as it
// will look *after* a move.
class FileIndex {
public:
	void		Add(const std::string& path);
	void		Remove(const std::string& path);
	// Returns the stored spelling of path (case-insensitive), or "".
	std::string	Find(const std::string& path) const;
	std::string	Resolve(const std::string& from, const md::Link& link) const;
	std::string	ResolveWiki(const std::string& from,
					const std::string& target) const;

	static std::string NameKey(const std::string& path);
	static bool	IsNote(const std::string& path);

private:
	std::string	_Best(const std::string& from,
					const std::vector<std::string>& candidates) const;

	std::map<std::string, std::string>				fByLowerPath;
	std::map<std::string, std::vector<std::string> >	fByName;
};


struct Backlink {
	std::string	source;
	size_t		start;
	size_t		end;
	std::string	context;
};


struct SearchHit {
	std::string	path;
	size_t		offset;
	std::string	snippet;
};


class LinkGraph {
public:
	void		Clear();
	void		SetNote(const std::string& path, const std::string& text);
	void		AddFile(const std::string& path);
	void		Remove(const std::string& path);

	const Note*	FindNote(const std::string& path) const;
	bool		Exists(const std::string& path) const;
	std::vector<std::string> NotePaths() const;
	std::vector<std::string> AllPaths() const;
	const FileIndex& Index() const { return fIndex; }

	std::string	Resolve(const std::string& from, const md::Link& link) const
					{ return fIndex.Resolve(from, link); }
	std::vector<Backlink> Backlinks(const std::string& path) const;
	std::vector<md::Link> UnresolvedLinks(const std::string& path) const;

	// moves: old path -> new path, for files only (expand folders first).
	// Returns new path -> new text for every note whose text must change.
	std::map<std::string, std::string> PlanMoves(
		const std::map<std::string, std::string>& moves) const;
	// Updates the model after the files were moved on disk.
	void		ApplyMoves(const std::map<std::string, std::string>& moves,
					const std::map<std::string, std::string>& rewritten);

	// Shortest wiki-link target that resolves from `from` to `to`.
	std::string	WikiTargetFor(const std::string& from,
					const std::string& to) const;

	// Query: words, "quoted phrases", #tag or tag:x, path:x, todo / done
	std::vector<SearchHit> Search(const std::string& query,
		size_t limit = 300) const;
	std::map<std::string, int> TagCounts() const;

private:
	std::map<std::string, Note>	fNotes;
	FileIndex					fIndex;
	std::set<std::string>		fFiles;
};


#endif	// LINK_GRAPH_H

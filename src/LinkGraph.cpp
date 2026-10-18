#include "LinkGraph.h"

#include <algorithm>

using md::npos;


// #pragma mark - FileIndex


std::string
FileIndex::NameKey(const std::string& path)
{
	return md::ToLower(md::StripMdExtension(md::Basename(path)));
}


bool
FileIndex::IsNote(const std::string& path)
{
	return md::EndsWithNoCase(path, ".md");
}


void
FileIndex::Add(const std::string& path)
{
	fByLowerPath[md::ToLower(path)] = path;
	std::vector<std::string>& v = fByName[NameKey(path)];
	if (std::find(v.begin(), v.end(), path) == v.end())
		v.push_back(path);
}


void
FileIndex::Remove(const std::string& path)
{
	fByLowerPath.erase(md::ToLower(path));
	auto it = fByName.find(NameKey(path));
	if (it == fByName.end())
		return;
	std::vector<std::string>& v = it->second;
	v.erase(std::remove(v.begin(), v.end(), path), v.end());
	if (v.empty())
		fByName.erase(it);
}


std::string
FileIndex::Find(const std::string& path) const
{
	auto it = fByLowerPath.find(md::ToLower(path));
	return it == fByLowerPath.end() ? std::string() : it->second;
}


static size_t
Depth(const std::string& p)
{
	return std::count(p.begin(), p.end(), '/');
}


std::string
FileIndex::_Best(const std::string& from,
	const std::vector<std::string>& candidates) const
{
	if (candidates.empty())
		return std::string();
	std::string dir = md::Dirname(from);
	std::vector<std::string> c = candidates;
	std::sort(c.begin(), c.end(),
		[&](const std::string& a, const std::string& b) {
			bool sa = md::Dirname(a) == dir;
			bool sb = md::Dirname(b) == dir;
			if (sa != sb)
				return sa;
			bool na = IsNote(a);
			bool nb = IsNote(b);
			if (na != nb)
				return na;
			if (Depth(a) != Depth(b))
				return Depth(a) < Depth(b);
			return a < b;
		});
	return c.front();
}


std::string
FileIndex::ResolveWiki(const std::string& from, const std::string& target) const
{
	if (target.empty())
		return Find(from);

	std::string t = target;
	if (t.compare(0, 2, "./") == 0 || t.compare(0, 3, "../") == 0) {
		std::string p = md::NormalizePath(md::JoinPath(md::Dirname(from), t));
		std::string f = Find(p);
		return f.empty() ? Find(p + ".md") : f;
	}
	if (t[0] == '/')
		t = t.substr(1);

	if (t.find('/') != npos) {
		std::string p = md::NormalizePath(t);
		std::string f = Find(p);
		if (f.empty())
			f = Find(p + ".md");
		if (!f.empty())
			return f;
		// partial path: "Section/Page" matches "Notebook/Section/Page.md"
		std::string low = "/" + md::ToLower(p);
		std::vector<std::string> candidates;
		for (const auto& kv : fByLowerPath) {
			if (md::EndsWithNoCase(kv.first, low)
				|| md::EndsWithNoCase(kv.first, low + ".md"))
				candidates.push_back(kv.second);
		}
		return _Best(from, candidates);
	}

	auto it = fByName.find(md::ToLower(md::StripMdExtension(t)));
	if (it == fByName.end())
		return std::string();
	// "Foo" must not match "Foo.png"; "Foo.png" must not match "Foo.png.md"
	bool hasMd = md::EndsWithNoCase(t, ".md");
	std::vector<std::string> candidates;
	for (const std::string& p : it->second) {
		std::string base = md::Basename(p);
		if (IsNote(p)) {
			if (hasMd || md::EqualsNoCase(md::StripMdExtension(base), t))
				candidates.push_back(p);
		} else if (md::EqualsNoCase(base, t))
			candidates.push_back(p);
	}
	return _Best(from, candidates);
}


std::string
FileIndex::Resolve(const std::string& from, const md::Link& link) const
{
	if (link.external)
		return std::string();
	if (link.kind == md::kWikiLink)
		return ResolveWiki(from, link.target);
	if (link.target.empty())
		return Find(from);

	const std::string& t = link.target;
	std::string p = t[0] == '/'
		? md::NormalizePath(t.substr(1))
		: md::NormalizePath(md::JoinPath(md::Dirname(from), t));
	if (p.empty() || p.compare(0, 2, "..") == 0)
		return std::string();
	std::string f = Find(p);
	return f.empty() ? Find(p + ".md") : f;
}


// #pragma mark - LinkGraph


void
LinkGraph::Clear()
{
	fNotes.clear();
	fFiles.clear();
	fIndex = FileIndex();
}


void
LinkGraph::SetNote(const std::string& path, const std::string& text)
{
	Note& n = fNotes[path];
	n.path = path;
	n.text = text;
	n.lower = md::ToLower(text);
	n.links = md::FindLinks(text);
	n.tags = md::FindTags(text);
	n.tasks = md::FindTasks(text);
	if (fFiles.insert(path).second)
		fIndex.Add(path);
}


void
LinkGraph::AddFile(const std::string& path)
{
	if (fFiles.insert(path).second)
		fIndex.Add(path);
}


void
LinkGraph::Remove(const std::string& path)
{
	fNotes.erase(path);
	if (fFiles.erase(path) > 0)
		fIndex.Remove(path);
}


const Note*
LinkGraph::FindNote(const std::string& path) const
{
	auto it = fNotes.find(path);
	return it == fNotes.end() ? nullptr : &it->second;
}


bool
LinkGraph::Exists(const std::string& path) const
{
	return !fIndex.Find(path).empty();
}


std::vector<std::string>
LinkGraph::NotePaths() const
{
	std::vector<std::string> paths;
	for (const auto& kv : fNotes)
		paths.push_back(kv.first);
	return paths;
}


std::vector<std::string>
LinkGraph::AllPaths() const
{
	return std::vector<std::string>(fFiles.begin(), fFiles.end());
}


static std::string
LineAround(const std::string& text, size_t offset, size_t maxLen = 110)
{
	size_t s = text.rfind('\n', offset == 0 ? 0 : offset - 1);
	s = (s == npos || offset == 0) ? 0 : s + 1;
	if (offset > 0 && text[offset - 1] == '\n')
		s = offset;
	size_t e = text.find('\n', offset);
	if (e == npos)
		e = text.size();
	if (e - s > maxLen) {
		size_t half = maxLen / 2;
		size_t ns = offset > s + half ? offset - half : s;
		size_t ne = std::min(e, ns + maxLen);
		while (ns > s && ((unsigned char)text[ns] & 0xC0) == 0x80)
			ns--;
		while (ne < e && ((unsigned char)text[ne] & 0xC0) == 0x80)
			ne++;
		std::string r = text.substr(ns, ne - ns);
		if (ns > s)
			r = "…" + r;
		if (ne < e)
			r += "…";
		return r;
	}
	std::string r = text.substr(s, e - s);
	size_t a = r.find_first_not_of(" \t");
	return a == npos ? std::string() : r.substr(a);
}


std::vector<Backlink>
LinkGraph::Backlinks(const std::string& path) const
{
	std::vector<Backlink> result;
	for (const auto& kv : fNotes) {
		const Note& n = kv.second;
		if (n.path == path)
			continue;
		for (const md::Link& l : n.links) {
			if (l.target.empty() || fIndex.Resolve(n.path, l) != path)
				continue;
			result.push_back({n.path, l.start, l.end,
				LineAround(n.text, l.start)});
		}
	}
	return result;
}


std::vector<md::Link>
LinkGraph::UnresolvedLinks(const std::string& path) const
{
	std::vector<md::Link> result;
	const Note* n = FindNote(path);
	if (n == nullptr)
		return result;
	for (const md::Link& l : n->links) {
		if (!l.external && !l.target.empty()
			&& fIndex.Resolve(path, l).empty())
			result.push_back(l);
	}
	return result;
}


static std::string
NewTarget(const FileIndex& index, const std::string& from,
	const std::string& to, const md::Link& link, const std::string& text)
{
	std::string raw = text.substr(link.targetStart,
		link.targetEnd - link.targetStart);
	bool isNote = FileIndex::IsNote(to);

	if (link.kind == md::kMarkdownLink) {
		bool rootStyle = !raw.empty() && raw[0] == '/';
		std::string p = rootStyle
			? "/" + to : md::RelativePath(md::Dirname(from), to);
		if (isNote && !md::EndsWithNoCase(link.target, ".md"))
			p = md::StripMdExtension(p);
		bool angle = link.targetStart > 0 && text[link.targetStart - 1] == '<';
		return angle ? p : md::EncodeSpaces(p);
	}

	const std::string& t = link.target;
	bool keepExt = md::EndsWithNoCase(t, ".md");
	std::string bare = (isNote && !keepExt) ? md::StripMdExtension(to) : to;

	std::vector<std::string> candidates;
	if (t.compare(0, 2, "./") == 0 || t.compare(0, 3, "../") == 0) {
		std::string r = md::RelativePath(md::Dirname(from), bare);
		if (r.compare(0, 2, "..") != 0)
			r = "./" + r;
		candidates.push_back(r);
	}
	if (t.find('/') != npos) {
		candidates.push_back(bare);
		candidates.push_back(md::Basename(bare));
	} else {
		candidates.push_back(md::Basename(bare));
		candidates.push_back(bare);
	}
	for (const std::string& c : candidates) {
		if (index.ResolveWiki(from, c) == to)
			return c;
	}
	return bare;
}


std::map<std::string, std::string>
LinkGraph::PlanMoves(const std::map<std::string, std::string>& moves) const
{
	FileIndex after = fIndex;
	for (const auto& m : moves)
		after.Remove(m.first);
	for (const auto& m : moves)
		after.Add(m.second);

	auto mapPath = [&](const std::string& p) {
		auto it = moves.find(p);
		return it == moves.end() ? p : it->second;
	};

	std::map<std::string, std::string> result;
	for (const auto& kv : fNotes) {
		const Note& note = kv.second;
		const std::string newSource = mapPath(note.path);
		int changed = 0;
		std::string text = md::RewriteLinks(note.text,
			[&](const md::Link& link, std::string& newTarget) {
				if (link.external || link.target.empty())
					return false;
				std::string target = fIndex.Resolve(note.path, link);
				if (target.empty())
					return false;	// was broken before; leave it alone
				std::string newTargetPath = mapPath(target);
				if (after.Resolve(newSource, link) == newTargetPath)
					return false;	// still resolves to the same file
				newTarget = NewTarget(after, newSource, newTargetPath, link,
					note.text);
				return !newTarget.empty();
			}, &changed);
		if (changed > 0)
			result[newSource] = text;
	}
	return result;
}


void
LinkGraph::ApplyMoves(const std::map<std::string, std::string>& moves,
	const std::map<std::string, std::string>& rewritten)
{
	std::vector<Note> moved;
	for (const auto& m : moves) {
		auto it = fNotes.find(m.first);
		if (it != fNotes.end()) {
			moved.push_back(it->second);
			moved.back().path = m.second;
			fNotes.erase(it);
		}
		if (fFiles.erase(m.first) > 0)
			fIndex.Remove(m.first);
	}
	for (const auto& m : moves) {
		if (fFiles.insert(m.second).second)
			fIndex.Add(m.second);
	}
	for (Note& n : moved)
		fNotes[n.path] = n;
	for (const auto& r : rewritten)
		SetNote(r.first, r.second);
}


std::string
LinkGraph::WikiTargetFor(const std::string& from, const std::string& to) const
{
	md::Link probe;
	probe.target = md::Basename(to);
	std::string fake = "[[" + probe.target + "]]";
	probe.targetStart = 2;
	probe.targetEnd = 2 + probe.target.size();
	if (!FileIndex::IsNote(to)) {
		std::string r = NewTarget(fIndex, from, to, probe, fake);
		return r;
	}
	std::string bare = md::StripMdExtension(to);
	probe.target = md::Basename(bare);
	fake = "[[" + probe.target + "]]";
	probe.targetEnd = 2 + probe.target.size();
	return NewTarget(fIndex, from, to, probe, fake);
}


// #pragma mark - search


static std::vector<std::string>
Tokenize(const std::string& query)
{
	std::vector<std::string> tokens;
	size_t i = 0;
	while (i < query.size()) {
		while (i < query.size() && query[i] == ' ')
			i++;
		if (i >= query.size())
			break;
		if (query[i] == '"') {
			size_t e = query.find('"', i + 1);
			if (e == npos)
				e = query.size();
			if (e > i + 1)
				tokens.push_back(query.substr(i + 1, e - i - 1));
			i = e + 1;
		} else {
			size_t e = query.find(' ', i);
			if (e == npos)
				e = query.size();
			tokens.push_back(query.substr(i, e - i));
			i = e;
		}
	}
	return tokens;
}


std::vector<SearchHit>
LinkGraph::Search(const std::string& query, size_t limit) const
{
	std::vector<std::string> words, tags, paths;
	bool todo = false, done = false;
	for (std::string tok : Tokenize(md::ToLower(query))) {
		if (tok.size() > 1 && tok[0] == '#')
			tags.push_back(tok.substr(1));
		else if (tok.compare(0, 4, "tag:") == 0 && tok.size() > 4)
			tags.push_back(tok[4] == '#' ? tok.substr(5) : tok.substr(4));
		else if (tok.compare(0, 5, "path:") == 0 && tok.size() > 5)
			paths.push_back(tok.substr(5));
		else if (tok == "todo" || tok == "task:open")
			todo = true;
		else if (tok == "done" || tok == "task:done")
			done = true;
		else
			words.push_back(tok);
	}

	std::vector<SearchHit> titleHits, bodyHits;
	for (const auto& kv : fNotes) {
		const Note& n = kv.second;
		std::string lowPath = md::ToLower(n.path);

		bool ok = true;
		for (const std::string& p : paths)
			ok = ok && lowPath.find(p) != npos;
		size_t tagOffset = npos;
		for (const std::string& t : tags) {
			bool found = false;
			for (const md::Tag& nt : n.tags) {
				std::string low = md::ToLower(nt.name);
				if (low == t || low.compare(0, t.size() + 1, t + "/") == 0) {
					found = true;
					tagOffset = std::min(tagOffset, nt.start);
				}
			}
			ok = ok && found;
		}
		size_t wordOffset = npos;
		for (const std::string& w : words) {
			size_t at = n.lower.find(w);
			if (at == npos && lowPath.find(w) == npos)
				ok = false;
			else if (at != npos && wordOffset == npos)
				wordOffset = at;
		}
		if (!ok)
			continue;

		if (todo || done) {
			for (const md::Task& t : n.tasks) {
				if (t.done ? !done : !todo)
					continue;
				std::string line = n.text.substr(t.lineStart,
					t.lineEnd - t.lineStart);
				bool wordsInLine = true;
				for (const std::string& w : words)
					wordsInLine = wordsInLine && md::ToLower(line).find(w) != npos;
				if (!words.empty() && !wordsInLine)
					continue;
				bodyHits.push_back({n.path, t.boxStart,
					LineAround(n.text, t.boxStart)});
			}
			continue;
		}

		size_t offset = wordOffset != npos ? wordOffset
			: (tagOffset != npos ? tagOffset : 0);
		SearchHit hit = {n.path, offset, LineAround(n.text, offset)};
		bool inTitle = !words.empty()
			&& md::ToLower(md::Basename(n.path)).find(words[0]) != npos;
		(inTitle ? titleHits : bodyHits).push_back(hit);
	}

	titleHits.insert(titleHits.end(), bodyHits.begin(), bodyHits.end());
	if (titleHits.size() > limit)
		titleHits.resize(limit);
	return titleHits;
}


std::map<std::string, int>
LinkGraph::TagCounts() const
{
	std::map<std::string, int> counts;
	for (const auto& kv : fNotes) {
		std::set<std::string> seen;
		for (const md::Tag& t : kv.second.tags)
			seen.insert(md::ToLower(t.name));
		for (const std::string& t : seen)
			counts[t]++;
	}
	return counts;
}

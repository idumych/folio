/*
 * MarkdownScan — portable scanning of the Markdown subset Folio understands:
 * [[wiki links]], [markdown](links), #tags, - [ ] tasks, and styling spans.
 *
 * Deliberately free of Haiku headers so it can be unit-tested anywhere.
 */
#ifndef MARKDOWN_SCAN_H
#define MARKDOWN_SCAN_H

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace md {

static const size_t npos = std::string::npos;

enum LinkKind {
	kWikiLink = 0,		// [[Target#Heading|Alias]]  or  ![[embed.png]]
	kMarkdownLink = 1	// [label](relative/path.md#heading)  or  ![alt](img.png)
};

struct Link {
	LinkKind	kind = kWikiLink;
	bool		embed = false;
	bool		external = false;	// http://, mailto: … never rewritten
	size_t		start = 0;			// whole link, including '!' and brackets
	size_t		end = 0;
	size_t		targetStart = 0;	// raw target text in the source; this is
	size_t		targetEnd = 0;		// the only range a rename rewrites
	size_t		aliasStart = npos;	// wiki: first byte after '|'
	size_t		labelEnd = npos;	// markdown: offset of the closing ']'
	std::string	target;				// decoded, trimmed, without #heading
	std::string	heading;
	std::string	alias;
};

struct Tag {
	size_t		start;
	size_t		end;
	std::string	name;				// without the leading '#'
};

struct Task {
	size_t		boxStart;			// offset of '[' in "[ ]" / "[x]"
	bool		done;
	size_t		lineStart;
	size_t		lineEnd;
};

enum SpanKind {
	kHeading, kBold, kItalic, kCode, kCodeBlock, kSyntax,
	kLinkSpan, kTagSpan, kTaskBox, kTaskDone, kQuote
};

struct Span {
	SpanKind	kind;
	size_t		start;
	size_t		end;
	int			level;				// heading level, otherwise 0
};

// true for every byte inside a fenced code block or an inline `code` span
std::vector<bool> CodeMask(const std::string& text);

std::vector<Link> FindLinks(const std::string& text);
std::vector<Tag> FindTags(const std::string& text);
std::vector<Task> FindTasks(const std::string& text);
std::vector<Span> StyleSpans(const std::string& text);

// Calls func for every link; when it returns true the link's target range is
// replaced with newTarget. Everything else (alias, heading, '!') is kept.
typedef std::function<bool(const Link&, std::string& newTarget)> RewriteFunc;
std::string RewriteLinks(const std::string& text, const RewriteFunc& func,
	int* count = nullptr);

// String and vault-relative path helpers (paths use '/', no leading '/').
std::string ToLower(const std::string& s);
bool EqualsNoCase(const std::string& a, const std::string& b);
bool EndsWithNoCase(const std::string& s, const std::string& suffix);
std::string StripMdExtension(const std::string& name,
	bool* hadExtension = nullptr);
std::string PercentDecode(const std::string& s);
std::string EncodeSpaces(const std::string& s);
std::vector<std::string> SplitPath(const std::string& path);
std::string Dirname(const std::string& path);
std::string Basename(const std::string& path);
std::string JoinPath(const std::string& a, const std::string& b);
std::string NormalizePath(const std::string& path);
std::string RelativePath(const std::string& fromDir, const std::string& to);
std::string TopComponent(const std::string& path);

}	// namespace md

#endif	// MARKDOWN_SCAN_H

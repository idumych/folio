#include "MarkdownScan.h"

#include <algorithm>
#include <cctype>

namespace md {


static inline bool
IsSpace(char c)
{
	return c == ' ' || c == '\t';
}


static inline size_t
LineEnd(const std::string& t, size_t i)
{
	size_t e = t.find('\n', i);
	return e == npos ? t.size() : e;
}


// 0 = normal text, 1 = inline code, 2 = fenced code block
static std::vector<char>
CodeKinds(const std::string& text)
{
	std::vector<char> kind(text.size(), 0);
	bool inFence = false;
	char fenceChar = 0;
	size_t fenceLen = 0;

	size_t i = 0;
	while (i < text.size()) {
		size_t end = LineEnd(text, i);

		size_t p = i;
		int indent = 0;
		while (p < end && text[p] == ' ' && indent < 3) {
			p++;
			indent++;
		}
		char c = p < end ? text[p] : 0;
		size_t run = 0;
		if (c == '`' || c == '~') {
			while (p + run < end && text[p + run] == c)
				run++;
		}

		if (inFence) {
			for (size_t k = i; k < end; k++)
				kind[k] = 2;
			if (run >= 3 && c == fenceChar && run >= fenceLen) {
				bool blank = true;
				for (size_t k = p + run; k < end; k++) {
					if (!IsSpace(text[k]))
						blank = false;
				}
				if (blank)
					inFence = false;
			}
		} else if (run >= 3) {
			inFence = true;
			fenceChar = c;
			fenceLen = run;
			for (size_t k = i; k < end; k++)
				kind[k] = 2;
		} else {
			size_t k = i;
			while (k < end) {
				if (text[k] != '`') {
					k++;
					continue;
				}
				size_t r = 0;
				while (k + r < end && text[k + r] == '`')
					r++;
				size_t s = k + r;
				size_t close = npos;
				while (s < end) {
					if (text[s] == '`') {
						size_t r2 = 0;
						while (s + r2 < end && text[s + r2] == '`')
							r2++;
						if (r2 == r) {
							close = s;
							break;
						}
						s += r2;
					} else
						s++;
				}
				if (close == npos) {
					k += r;
					continue;
				}
				for (size_t m = k; m < close + r; m++)
					kind[m] = 1;
				k = close + r;
			}
		}
		i = end + 1;
	}
	return kind;
}


std::vector<bool>
CodeMask(const std::string& text)
{
	std::vector<char> kinds = CodeKinds(text);
	std::vector<bool> mask(kinds.size());
	for (size_t i = 0; i < kinds.size(); i++)
		mask[i] = kinds[i] != 0;
	return mask;
}


static bool
IsExternal(const std::string& t)
{
	if (t.find("://") != npos)
		return true;
	std::string l = ToLower(t);
	return l.compare(0, 7, "mailto:") == 0 || l.compare(0, 4, "tel:") == 0
		|| l.compare(0, 5, "data:") == 0;
}


static void
TrimRange(const std::string& t, size_t& s, size_t& e)
{
	while (s < e && IsSpace(t[s]))
		s++;
	while (e > s && IsSpace(t[e - 1]))
		e--;
}


std::vector<Link>
FindLinks(const std::string& text)
{
	std::vector<Link> links;
	std::vector<bool> mask = CodeMask(text);
	const size_t n = text.size();

	size_t i = 0;
	while (i < n) {
		if (mask[i] || text[i] != '[' || (i > 0 && text[i - 1] == '\\')) {
			i++;
			continue;
		}
		size_t lineEnd = LineEnd(text, i);
		bool embed = i > 0 && text[i - 1] == '!';

		if (i + 1 < n && text[i + 1] == '[') {
			// [[target#heading|alias]]
			size_t close = text.find("]]", i + 2);
			if (close == npos || close > lineEnd) {
				i += 2;
				continue;
			}
			size_t innerS = i + 2;
			size_t innerE = close;
			if (text.find('[', innerS) < innerE) {
				i++;
				continue;
			}

			Link l;
			l.kind = kWikiLink;
			l.embed = embed;
			l.start = embed ? i - 1 : i;
			l.end = close + 2;

			size_t bar = text.find('|', innerS);
			size_t targetPartEnd = innerE;
			if (bar < innerE) {
				targetPartEnd = bar;
				l.aliasStart = bar + 1;
				l.alias = text.substr(bar + 1, innerE - bar - 1);
			}
			size_t hash = text.find('#', innerS);
			size_t tE = targetPartEnd;
			if (hash < targetPartEnd) {
				tE = hash;
				l.heading = text.substr(hash + 1, targetPartEnd - hash - 1);
			}
			size_t tS = innerS;
			TrimRange(text, tS, tE);
			l.targetStart = tS;
			l.targetEnd = tE;
			l.target = text.substr(tS, tE - tS);
			l.external = IsExternal(l.target);
			links.push_back(l);
			i = close + 2;
			continue;
		}

		// [label](destination "title")
		size_t close = npos;
		int depth = 0;
		for (size_t k = i + 1; k < lineEnd; k++) {
			if (text[k] == '\\') {
				k++;
				continue;
			}
			if (text[k] == '[')
				depth++;
			else if (text[k] == ']') {
				if (depth == 0) {
					close = k;
					break;
				}
				depth--;
			}
		}
		if (close == npos || close + 1 >= lineEnd || text[close + 1] != '(') {
			i++;
			continue;
		}

		size_t p = close + 2;
		size_t dS, dE, parenEnd;
		if (p < lineEnd && text[p] == '<') {
			size_t gt = text.find('>', p + 1);
			if (gt == npos || gt > lineEnd) {
				i++;
				continue;
			}
			dS = p + 1;
			dE = gt;
			size_t q = text.find(')', gt);
			if (q == npos || q > lineEnd) {
				i++;
				continue;
			}
			parenEnd = q;
		} else {
			while (p < lineEnd && IsSpace(text[p]))
				p++;
			dS = p;
			int pd = 0;
			size_t q = p;
			for (; q < lineEnd; q++) {
				char c = text[q];
				if (c == '\\') {
					q++;
					continue;
				}
				if (c == '(')
					pd++;
				else if (c == ')') {
					if (pd == 0)
						break;
					pd--;
				} else if (IsSpace(c) && pd == 0)
					break;
			}
			if (q > lineEnd)
				q = lineEnd;
			dE = q;
			size_t r = q;
			while (r < lineEnd && IsSpace(text[r]))
				r++;
			if (r < lineEnd && (text[r] == '"' || text[r] == '\'')) {
				size_t qe = text.find(text[r], r + 1);
				if (qe == npos || qe > lineEnd) {
					i++;
					continue;
				}
				r = qe + 1;
				while (r < lineEnd && IsSpace(text[r]))
					r++;
			}
			if (r >= lineEnd || text[r] != ')') {
				i++;
				continue;
			}
			parenEnd = r;
		}

		Link l;
		l.kind = kMarkdownLink;
		l.embed = embed;
		l.start = embed ? i - 1 : i;
		l.end = parenEnd + 1;
		l.labelEnd = close;
		l.alias = text.substr(i + 1, close - i - 1);

		std::string rawDest = text.substr(dS, dE - dS);
		l.external = IsExternal(rawDest);
		size_t hash = l.external ? npos : text.find('#', dS);
		size_t tE = dE;
		if (hash < dE) {
			tE = hash;
			l.heading = PercentDecode(text.substr(hash + 1, dE - hash - 1));
		}
		l.targetStart = dS;
		l.targetEnd = tE;
		std::string raw = text.substr(dS, tE - dS);
		l.target = l.external ? raw : PercentDecode(raw);
		links.push_back(l);
		i = parenEnd + 1;
	}
	return links;
}


static inline bool
IsTagChar(unsigned char c)
{
	return isalnum(c) || c == '_' || c == '-' || c == '/' || c >= 0x80;
}


std::vector<Tag>
FindTags(const std::string& text)
{
	std::vector<Tag> tags;
	std::vector<bool> mask = CodeMask(text);
	std::vector<bool> inLink(text.size(), false);
	for (const Link& l : FindLinks(text)) {
		for (size_t k = l.start; k < l.end; k++)
			inLink[k] = true;
	}

	for (size_t i = 0; i < text.size(); i++) {
		if (text[i] != '#' || mask[i] || inLink[i])
			continue;
		if (i > 0) {
			char p = text[i - 1];
			if (!(p == ' ' || p == '\t' || p == '\n' || p == '(' || p == ','))
				continue;
		}
		size_t e = i + 1;
		while (e < text.size() && IsTagChar((unsigned char)text[e]))
			e++;
		while (e > i + 1 && text[e - 1] == '/')
			e--;
		if (e == i + 1)
			continue;
		std::string name = text.substr(i + 1, e - i - 1);
		bool digits = true;
		for (char c : name) {
			if (!isdigit((unsigned char)c))
				digits = false;
		}
		if (digits)
			continue;
		tags.push_back({i, e, name});
		i = e - 1;
	}
	return tags;
}


// Returns the offset of '[' when the line at [s, e) is a task item, else npos
static size_t
TaskBoxAt(const std::string& t, size_t s, size_t e)
{
	size_t p = s;
	while (p < e && IsSpace(t[p]))
		p++;
	if (p >= e)
		return npos;
	if (t[p] == '-' || t[p] == '*' || t[p] == '+')
		p++;
	else if (isdigit((unsigned char)t[p])) {
		while (p < e && isdigit((unsigned char)t[p]))
			p++;
		if (p >= e || (t[p] != '.' && t[p] != ')'))
			return npos;
		p++;
	} else
		return npos;
	if (p >= e || !IsSpace(t[p]))
		return npos;
	while (p < e && IsSpace(t[p]))
		p++;
	if (p + 3 > e || t[p] != '[' || t[p + 2] != ']')
		return npos;
	char c = t[p + 1];
	if (c != ' ' && c != 'x' && c != 'X')
		return npos;
	if (p + 3 < e && !IsSpace(t[p + 3]))
		return npos;
	return p;
}


std::vector<Task>
FindTasks(const std::string& text)
{
	std::vector<Task> tasks;
	std::vector<bool> mask = CodeMask(text);
	size_t i = 0;
	while (i < text.size()) {
		size_t end = LineEnd(text, i);
		size_t box = TaskBoxAt(text, i, end);
		if (box != npos && !mask[box])
			tasks.push_back({box, text[box + 1] != ' ', i, end});
		i = end + 1;
	}
	return tasks;
}


static void
Emphasis(const std::string& t, size_t s, size_t e,
	const std::vector<char>& code, std::vector<Span>& out)
{
	size_t i = s;
	while (i < e) {
		char c = t[i];
		if (code[i] || (c != '*' && c != '_')) {
			i++;
			continue;
		}
		bool dbl = i + 1 < e && t[i + 1] == c;
		size_t ml = dbl ? 2 : 1;
		size_t o = i + ml;
		if (o >= e || IsSpace(t[o])
			|| (c == '_' && i > s && isalnum((unsigned char)t[i - 1]))) {
			i += ml;
			continue;
		}
		size_t close = npos;
		size_t k = o;
		while (k < e) {
			if (!code[k] && t[k] == c) {
				size_t run = 0;
				while (k + run < e && t[k + run] == c)
					run++;
				if (run >= ml && !IsSpace(t[k - 1])
					&& (c != '_' || k + ml >= e
						|| !isalnum((unsigned char)t[k + ml]))) {
					close = k;
					break;
				}
				k += run;
			} else
				k++;
		}
		if (close == npos) {
			i += ml;
			continue;
		}
		out.push_back({dbl ? kBold : kItalic, i, close + ml, 0});
		out.push_back({kSyntax, i, i + ml, 0});
		out.push_back({kSyntax, close, close + ml, 0});
		i = close + ml;
	}
}


std::vector<Span>
StyleSpans(const std::string& text)
{
	std::vector<Span> spans;
	std::vector<char> code = CodeKinds(text);

	size_t i = 0;
	while (i < text.size()) {
		size_t end = LineEnd(text, i);
		if (i < end && code[i] == 2) {
			spans.push_back({kCodeBlock, i, end, 0});
			i = end + 1;
			continue;
		}
		size_t p = i;
		while (p < end && p - i < 3 && text[p] == ' ')
			p++;
		size_t h = 0;
		while (p + h < end && text[p + h] == '#' && h < 7)
			h++;
		if (h >= 1 && h <= 6 && (p + h == end || IsSpace(text[p + h]))) {
			spans.push_back({kHeading, i, end, (int)h});
			spans.push_back({kSyntax, p, std::min(end, p + h + 1), 0});
		} else if (p < end && text[p] == '>') {
			spans.push_back({kQuote, i, end, 0});
			spans.push_back({kSyntax, p, p + 1, 0});
		}
		Emphasis(text, i, end, code, spans);

		size_t k = i;
		while (k < end) {
			if (code[k] == 1) {
				size_t s = k;
				while (k < end && code[k] == 1)
					k++;
				spans.push_back({kCode, s, k, 0});
			} else
				k++;
		}
		i = end + 1;
	}

	for (const Link& l : FindLinks(text)) {
		spans.push_back({kLinkSpan, l.start, l.end, 0});
		if (l.kind == kWikiLink) {
			size_t open = l.embed ? l.start + 3 : l.start + 2;
			spans.push_back({kSyntax, l.start, open, 0});
			spans.push_back({kSyntax, l.end - 2, l.end, 0});
			if (l.aliasStart != npos)
				spans.push_back({kSyntax, open, l.aliasStart, 0});
		} else {
			size_t open = l.embed ? l.start + 2 : l.start + 1;
			spans.push_back({kSyntax, l.start, open, 0});
			spans.push_back({kSyntax, l.labelEnd, l.end, 0});
		}
	}
	for (const Tag& t : FindTags(text))
		spans.push_back({kTagSpan, t.start, t.end, 0});
	for (const Task& t : FindTasks(text)) {
		spans.push_back({kTaskBox, t.boxStart, t.boxStart + 3, 0});
		if (t.done)
			spans.push_back({kTaskDone, t.boxStart + 3, t.lineEnd, 0});
	}
	return spans;
}


std::string
RewriteLinks(const std::string& text, const RewriteFunc& func, int* count)
{
	std::vector<Link> links = FindLinks(text);
	std::string out = text;
	int changed = 0;
	for (auto it = links.rbegin(); it != links.rend(); ++it) {
		std::string newTarget;
		if (func(*it, newTarget)) {
			out.replace(it->targetStart, it->targetEnd - it->targetStart,
				newTarget);
			changed++;
		}
	}
	if (count != nullptr)
		*count = changed;
	return out;
}


// #pragma mark - strings & paths


std::string
ToLower(const std::string& s)
{
	std::string r = s;
	for (char& c : r) {
		if (c >= 'A' && c <= 'Z')
			c = c - 'A' + 'a';
	}
	return r;
}


bool
EqualsNoCase(const std::string& a, const std::string& b)
{
	return a.size() == b.size() && ToLower(a) == ToLower(b);
}


bool
EndsWithNoCase(const std::string& s, const std::string& suffix)
{
	return s.size() >= suffix.size()
		&& EqualsNoCase(s.substr(s.size() - suffix.size()), suffix);
}


std::string
StripMdExtension(const std::string& name, bool* hadExtension)
{
	bool has = EndsWithNoCase(name, ".md");
	if (hadExtension != nullptr)
		*hadExtension = has;
	return has ? name.substr(0, name.size() - 3) : name;
}


static int
HexValue(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}


std::string
PercentDecode(const std::string& s)
{
	std::string r;
	r.reserve(s.size());
	for (size_t i = 0; i < s.size(); i++) {
		if (s[i] == '%' && i + 2 < s.size()) {
			int hi = HexValue(s[i + 1]);
			int lo = HexValue(s[i + 2]);
			if (hi >= 0 && lo >= 0) {
				r += (char)(hi * 16 + lo);
				i += 2;
				continue;
			}
		}
		r += s[i];
	}
	return r;
}


std::string
EncodeSpaces(const std::string& s)
{
	std::string r;
	for (char c : s) {
		if (c == ' ')
			r += "%20";
		else
			r += c;
	}
	return r;
}


std::vector<std::string>
SplitPath(const std::string& path)
{
	std::vector<std::string> parts;
	size_t i = 0;
	while (i <= path.size()) {
		size_t e = path.find('/', i);
		if (e == npos)
			e = path.size();
		if (e > i)
			parts.push_back(path.substr(i, e - i));
		i = e + 1;
	}
	return parts;
}


std::string
Dirname(const std::string& path)
{
	size_t s = path.rfind('/');
	return s == npos ? std::string() : path.substr(0, s);
}


std::string
Basename(const std::string& path)
{
	size_t s = path.rfind('/');
	return s == npos ? path : path.substr(s + 1);
}


std::string
JoinPath(const std::string& a, const std::string& b)
{
	if (a.empty())
		return b;
	if (b.empty())
		return a;
	return a + "/" + b;
}


std::string
NormalizePath(const std::string& path)
{
	std::vector<std::string> out;
	for (const std::string& part : SplitPath(path)) {
		if (part == ".")
			continue;
		if (part == ".." && !out.empty() && out.back() != "..")
			out.pop_back();
		else
			out.push_back(part);
	}
	std::string r;
	for (const std::string& part : out)
		r = JoinPath(r, part);
	return r;
}


std::string
RelativePath(const std::string& fromDir, const std::string& to)
{
	std::vector<std::string> a = SplitPath(fromDir);
	std::vector<std::string> b = SplitPath(to);
	size_t common = 0;
	while (common < a.size() && common + 1 < b.size() && a[common] == b[common])
		common++;
	std::string r;
	for (size_t i = common; i < a.size(); i++)
		r = JoinPath(r, "..");
	for (size_t i = common; i < b.size(); i++)
		r = JoinPath(r, b[i]);
	return r;
}


std::string
TopComponent(const std::string& path)
{
	size_t s = path.find('/');
	return s == npos ? path : path.substr(0, s);
}


}	// namespace md

// Unit tests for the portable core. Build anywhere: make -f Makefile.tests
#include <cstdio>
#include <cstdlib>
#include <string>

#include "../src/LinkGraph.h"

static int sFailures = 0;
static int sChecks = 0;

#define CHECK_EQ(a, b) do { \
	sChecks++; \
	std::string _a = (a), _b = (b); \
	if (_a != _b) { \
		sFailures++; \
		printf("FAIL %s:%d\n  got:      %s\n  expected: %s\n", __FILE__, \
			__LINE__, _a.c_str(), _b.c_str()); \
	} } while (0)

#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; \
	printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)


// Plans a set of moves and applies them; returns the new text of `path`.
static void
Move(LinkGraph& g, std::map<std::string, std::string> moves)
{
	auto rewritten = g.PlanMoves(moves);
	g.ApplyMoves(moves, rewritten);
}


static std::string
Text(const LinkGraph& g, const std::string& path)
{
	const Note* n = g.FindNote(path);
	return n ? n->text : "<missing " + path + ">";
}


static void
TestParsing()
{
	std::string t = "See [[Foo#Intro|the foo]] and ![[pic.png]] and "
		"[bar](Sec%20A/Bar.md#x \"t\") and [web](https://a.b/c) `[[nope]]`\n"
		"```\n[[also nope]]\n```\n";
	auto links = md::FindLinks(t);
	CHECK(links.size() == 4);
	CHECK_EQ(links[0].target, "Foo");
	CHECK_EQ(links[0].heading, "Intro");
	CHECK_EQ(links[0].alias, "the foo");
	CHECK(links[1].embed);
	CHECK_EQ(links[1].target, "pic.png");
	CHECK_EQ(links[2].target, "Sec A/Bar.md");
	CHECK_EQ(links[2].heading, "x");
	CHECK(links[3].external);

	auto tags = md::FindTags("#one some #two/sub and a#no, #123 [[x#notag]]\n"
		"# Heading\n`#code`");
	CHECK(tags.size() == 2);
	CHECK_EQ(tags[0].name, "one");
	CHECK_EQ(tags[1].name, "two/sub");

	auto tasks = md::FindTasks("- [ ] open\n* [x] done\n1. [ ] num\n- [] no\n");
	CHECK(tasks.size() == 3);
	CHECK(!tasks[0].done && tasks[1].done && !tasks[2].done);

	CHECK_EQ(md::RelativePath("A/B", "A/C/d.md"), "../C/d.md");
	CHECK_EQ(md::RelativePath("", "x.md"), "x.md");
	CHECK_EQ(md::NormalizePath("a/./b/../c"), "a/c");
}


static void
TestSimpleRename()
{
	LinkGraph g;
	g.SetNote("Work/Meetings/Standup.md", "# Standup\nsee [[Roadmap]] and "
		"[[Roadmap#Q3|Q3 plans]] and ![[Roadmap]]\n");
	g.SetNote("Work/Roadmap.md", "# Roadmap\n");
	g.SetNote("Work/Other.md", "nothing here [[Roadmapping]] `[[Roadmap]]`\n");

	Move(g, {{"Work/Roadmap.md", "Work/Plan 2027.md"}});
	CHECK_EQ(Text(g, "Work/Meetings/Standup.md"), "# Standup\nsee [[Plan 2027]] "
		"and [[Plan 2027#Q3|Q3 plans]] and ![[Plan 2027]]\n");
	CHECK_EQ(Text(g, "Work/Other.md"),
		"nothing here [[Roadmapping]] `[[Roadmap]]`\n");
	CHECK(g.Backlinks("Work/Plan 2027.md").size() == 3);
}


static void
TestAmbiguity()
{
	// Two "Ideas" pages: renaming one must not hijack links to the other,
	// and links whose basename becomes ambiguous get a disambiguating path.
	LinkGraph g;
	g.SetNote("Home/Ideas.md", "home ideas\n");
	g.SetNote("Home/Index.md", "[[Ideas]]\n");
	g.SetNote("Work/Draft.md", "work draft\n");
	g.SetNote("Work/Index.md", "[[Draft]]\n");

	Move(g, {{"Work/Draft.md", "Work/Ideas.md"}});
	CHECK_EQ(Text(g, "Work/Index.md"), "[[Ideas]]\n");	// same folder wins
	CHECK_EQ(Text(g, "Home/Index.md"), "[[Ideas]]\n");	// untouched

	g.SetNote("Journal.md", "[[Draft2]]\n");
	g.SetNote("Notes/Draft2.md", "x\n");
	Move(g, {{"Notes/Draft2.md", "Notes/Ideas.md"}});
	// from the vault root, plain [[Ideas]] would be ambiguous → full path
	CHECK_EQ(Text(g, "Journal.md"), "[[Notes/Ideas]]\n");
}


static void
TestCollision()
{
	// A link that keeps pointing at an unmoved file can become ambiguous
	// because another file was renamed to the same name.
	LinkGraph g;
	g.SetNote("Index.md", "[[Target]]\n");
	g.SetNote("B/Target.md", "the real one\n");
	g.SetNote("A/Other.md", "other\n");
	Move(g, {{"A/Other.md", "Target.md"}});
	CHECK_EQ(Text(g, "Index.md"), "[[B/Target]]\n");
}


static void
TestFolderMoveAndMarkdownLinks()
{
	LinkGraph g;
	g.SetNote("Notebook/Section A/Page.md",
		"[sib](Sibling.md) [up](../Top.md#h) ![i](img/cat%20pic.png) "
		"[[Top]]\n");
	g.SetNote("Notebook/Section A/Sibling.md", "[back](Page)\n");
	g.SetNote("Notebook/Top.md", "[p](Section%20A/Page.md)\n");
	g.AddFile("Notebook/Section A/img/cat pic.png");

	// Rename the section folder: every file under it moves.
	Move(g, {
		{"Notebook/Section A/Page.md", "Notebook/Projects/Deep/Page.md"},
		{"Notebook/Section A/Sibling.md", "Notebook/Projects/Deep/Sibling.md"},
		{"Notebook/Section A/img/cat pic.png",
			"Notebook/Projects/Deep/img/cat pic.png"}});

	// relative links between moved files stay valid; ../Top.md must change
	CHECK_EQ(Text(g, "Notebook/Projects/Deep/Page.md"),
		"[sib](Sibling.md) [up](../../Top.md#h) ![i](img/cat%20pic.png) "
		"[[Top]]\n");
	CHECK_EQ(Text(g, "Notebook/Projects/Deep/Sibling.md"), "[back](Page)\n");
	CHECK_EQ(Text(g, "Notebook/Top.md"), "[p](Projects/Deep/Page.md)\n");
}


static void
TestPathStyleAndExtensions()
{
	LinkGraph g;
	g.SetNote("a/Note.md", "x\n");
	g.SetNote("Index.md", "[[a/Note]] [[Note.md]] [[./a/Note]] ![[a/pic.png]]\n");
	g.AddFile("a/pic.png");
	Move(g, {{"a/Note.md", "b/Renamed.md"}, {"a/pic.png", "b/photo.png"}});
	CHECK_EQ(Text(g, "Index.md"),
		"[[b/Renamed]] [[Renamed.md]] [[./b/Renamed]] ![[b/photo.png]]\n");
}


static void
TestBrokenLinksUntouched()
{
	LinkGraph g;
	g.SetNote("A.md", "[[Missing]] [[B]]\n");
	g.SetNote("B.md", "\n");
	Move(g, {{"B.md", "Missing.md"}});
	// [[Missing]] was broken; now it resolves by coincidence — leave it.
	CHECK_EQ(Text(g, "A.md"), "[[Missing]] [[Missing]]\n");
	CHECK(g.UnresolvedLinks("A.md").empty());
}


static void
TestSearch()
{
	LinkGraph g;
	g.SetNote("Work/Plan.md", "Budget review #finance/q3\n- [ ] call bank\n"
		"- [x] send deck\n");
	g.SetNote("Home/Groceries.md", "- [ ] milk #shopping\n");
	CHECK(g.Search("budget").size() == 1);
	CHECK(g.Search("#finance").size() == 1);
	CHECK(g.Search("tag:shopping").size() == 1);
	CHECK(g.Search("todo").size() == 2);
	CHECK(g.Search("done").size() == 1);
	CHECK(g.Search("todo path:work").size() == 1);
	CHECK(g.Search("todo bank").size() == 1);
	CHECK(g.TagCounts().size() == 2);
}


int
main()
{
	TestParsing();
	TestSimpleRename();
	TestAmbiguity();
	TestCollision();
	TestFolderMoveAndMarkdownLinks();
	TestPathStyleAndExtensions();
	TestBrokenLinksUntouched();
	TestSearch();
	printf("%d checks, %d failures\n", sChecks, sFailures);
	return sFailures == 0 ? 0 : 1;
}

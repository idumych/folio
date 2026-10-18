#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <GraphicsDefs.h>
#include <SupportDefs.h>

#define kAppSignature		"application/x-vnd.Folio"
#define kMarkdownMimeType	"text/markdown"
#define kAttrTags			"FOLIO:tags"
#define kAttrOpenTasks		"FOLIO:open_tasks"
#define kAttachmentFolder	"attachments"
#define kSettingsFile		"Folio_settings"

enum {
	// File
	M_NEW_NOTEBOOK			= 'nNbk',
	M_NEW_SECTION			= 'nSec',
	M_NEW_PAGE				= 'nPag',
	M_OPEN_VAULT			= 'oVlt',
	M_SHOW_IN_TRACKER		= 'shTr',

	// Page
	M_RENAME				= 'rnam',
	M_MOVE_TO				= 'mvTo',
	M_DELETE				= 'dele',
	M_INSERT_TODO			= 'iTdo',
	M_INSERT_LINK			= 'iLnk',
	M_ATTACH_FILE			= 'atch',
	M_FOLLOW_LINK_AT_CARET	= 'flwC',
	M_BACK					= 'back',
	M_FORWARD				= 'frwd',
	M_FIND					= 'find',
	M_TOGGLE_PANEL			= 'tPnl',

	// internal
	M_NAME_ENTERED			= 'nmEn',
	M_TITLE_CHANGED			= 'tChg',
	M_TREE_SELECTED			= 'trSl',
	M_TREE_CONTEXT			= 'trCx',
	M_TEXT_CHANGED			= 'txCh',
	M_SAVE_TIMER			= 'svTm',
	M_RESTYLE_TIMER			= 'rsTm',
	M_FOLLOW_LINK			= 'flwL',
	M_LINK_COMPLETE			= 'lnkC',
	M_COMPLETE_CHOSEN		= 'lnkS',
	M_DROP_REFS				= 'drop',
	M_ATTACH_PANEL_REFS		= 'atRf',
	M_SEARCH_CHANGED		= 'srCh',
	M_SEARCH_INVOKED		= 'srIv',
	M_BACKLINK_INVOKED		= 'blIv',
	M_TAG_INVOKED			= 'tgIv',
	M_VAULT_CHOSEN			= 'vlCh',
};

// Editor palette (light theme; the dark variant is derived from ui_color)
static const rgb_color kLinkColor		= {36, 99, 196, 255};
static const rgb_color kBrokenLinkColor	= {176, 64, 48, 255};
static const rgb_color kTagColor		= {124, 72, 168, 255};
static const rgb_color kSyntaxColor		= {160, 160, 160, 255};
static const rgb_color kCodeColor		= {140, 60, 30, 255};
static const rgb_color kDoneColor		= {150, 150, 150, 255};
static const rgb_color kQuoteColor		= {96, 110, 120, 255};

#endif	// CONSTANTS_H

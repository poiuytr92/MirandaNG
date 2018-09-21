/*
Scriver

Copyright (c) 2000-12 Miranda ICQ/IM project,

all portions of this codebase are copyrighted to the people
listed in contributors.txt.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "stdafx.h"

GlobalMessageData g_dat;

static const char *buttonIcons[] =
{
	"scriver_CLOSEX", "scriver_QUOTE", "scriver_ADD", nullptr, 
	"scriver_USERDETAILS", "scriver_HISTORY", "scriver_SEND"
};

static const char *chatButtonIcons[] =
{
	"scriver_CLOSEX", "chat_bold", "chat_italics", "chat_underline", 
	"chat_fgcol", "chat_bkgcol", "chat_history", "chat_filter", 
	"chat_settings", "chat_nicklist", "scriver_SEND"
};

static IconItem iconList1[] =
{
	{ LPGEN("Add contact"),            "scriver_ADD",          IDI_ADDCONTACT  }, //  1
	{ LPGEN("User's details"),         "scriver_USERDETAILS",  IDI_USERDETAILS }, //  2
	{ LPGEN("User's history"),         "scriver_HISTORY",      IDI_HISTORY     }, //  3
	{ LPGEN("Send message"),           "scriver_SEND",         IDI_SEND        }, //  4
	{ LPGEN("Smiley button"),          "scriver_SMILEY",       IDI_SMILEY      }, //  5
	{ LPGEN("User is typing"),         "scriver_TYPING",       IDI_TYPING      }, //  6
	{ LPGEN("Typing notification off"), "scriver_TYPINGOFF",   IDI_TYPINGOFF   }, //  7
	{ LPGEN("Sending"),                "scriver_DELIVERING",   IDI_TIMESTAMP   }, //  8
	{ LPGEN("Quote button"),           "scriver_QUOTE",        IDI_QUOTE       }, //  9
	{ LPGEN("Close button"),           "scriver_CLOSEX",       IDI_CLOSEX      }, // 10
	{ LPGEN("Icon overlay"),           "scriver_OVERLAY",      IDI_OVERLAY     }, // 11
	{ LPGEN("Incoming message (10x10)"),"scriver_INCOMING",    IDI_INCOMING, 10}, // 12
	{ LPGEN("Outgoing message (10x10)"),"scriver_OUTGOING",    IDI_OUTGOING, 10}, // 13  
	{ LPGEN("Notice (10x10)"),         "scriver_NOTICE",       IDI_NOTICE,   10}, // 14  
};

static IconItem iconList2[] =
{
	{ LPGEN("Window Icon"),            "chat_window",          IDI_CHANMGR     }, //  1 
	{ LPGEN("Text color"),             "chat_fgcol",           IDI_COLOR       }, //  2
	{ LPGEN("Background color") ,      "chat_bkgcol",          IDI_BKGCOLOR    }, //  3
	{ LPGEN("Bold"),                   "chat_bold",            IDI_BBOLD       }, //  4
	{ LPGEN("Italics"),                "chat_italics",         IDI_BITALICS    }, //  5
	{ LPGEN("Underlined"),             "chat_underline",       IDI_BUNDERLINE  }, //  6
	{ LPGEN("Smiley button"),          "chat_smiley",          IDI_SMILEY      }, //  7
	{ LPGEN("Room history"),           "chat_history",         IDI_HISTORY     }, //  8
	{ LPGEN("Room settings"),          "chat_settings",        IDI_TOPICBUT    }, //  9
	{ LPGEN("Event filter disabled"),  "chat_filter",          IDI_FILTER      }, // 10
	{ LPGEN("Event filter enabled"),   "chat_filter2",         IDI_FILTER2     }, // 11
	{ LPGEN("Hide nick list"),         "chat_nicklist",        IDI_NICKLIST    }, // 12
	{ LPGEN("Show nick list"),         "chat_nicklist2",       IDI_NICKLIST2   }, // 13
	{ LPGEN("Icon overlay"),           "chat_overlay",         IDI_OVERLAY     }, // 14
	{ LPGEN("Status 1 (10x10)"),       "chat_status0",         IDI_STATUS0,  10}, // 15
	{ LPGEN("Status 2 (10x10)"),       "chat_status1",         IDI_STATUS1,  10}, // 16
	{ LPGEN("Status 3 (10x10)"),       "chat_status2",         IDI_STATUS2,  10}, // 17
	{ LPGEN("Status 4 (10x10)"),       "chat_status3",         IDI_STATUS3,  10}, // 18
	{ LPGEN("Status 5 (10x10)"),       "chat_status4",         IDI_STATUS4,  10}, // 19
	{ LPGEN("Status 6 (10x10)"),       "chat_status5",         IDI_STATUS5,  10}, // 20
};

static IconItem iconList3[] =
{
	{ LPGEN("Message in (10x10)"),     "chat_log_message_in",  IDI_INCOMING, 10}, //  1
	{ LPGEN("Message out (10x10)"),    "chat_log_message_out", IDI_OUTGOING, 10}, //  2
	{ LPGEN("Action (10x10)"),         "chat_log_action",      IDI_ACTION,   10}, //  3
	{ LPGEN("Add Status (10x10)"),     "chat_log_addstatus",   IDI_ADDSTATUS,10}, //  4
	{ LPGEN("Remove status (10x10)"),  "chat_log_removestatus", IDI_REMSTATUS,10}, //  5
	{ LPGEN("Join (10x10)"),           "chat_log_join",        IDI_JOIN,     10}, //  6
	{ LPGEN("Leave (10x10)"),          "chat_log_part",        IDI_PART,     10}, //  7
	{ LPGEN("Quit (10x10)"),           "chat_log_quit",        IDI_QUIT,     10}, //  8
	{ LPGEN("Kick (10x10)"),           "chat_log_kick",        IDI_KICK,     10}, //  9
	{ LPGEN("Nickchange (10x10)"),     "chat_log_nick",        IDI_NICK,     10}, // 10
	{ LPGEN("Notice (10x10)"),         "chat_log_notice",   IDI_CHAT_NOTICE, 10}, // 11
	{ LPGEN("Topic (10x10)"),          "chat_log_topic",       IDI_TOPIC,    10}, // 12
	{ LPGEN("Highlight (10x10)"),      "chat_log_highlight",   IDI_NOTICE,   10}, // 13
	{ LPGEN("Information (10x10)"),    "chat_log_info",        IDI_INFO,     10}, // 14
};

void RegisterIcons(void)
{
	HookEvent(ME_SKIN2_ICONSCHANGED, IconsChanged);

	g_plugin.registerIcon(LPGEN("Single Messaging"), iconList1);
	g_plugin.registerIcon(LPGEN("Group chats"),      iconList2);
	g_plugin.registerIcon(LPGEN("Single Messaging"), iconList3);
}

/////////////////////////////////////////////////////////////////////////////////////////

static int ackevent(WPARAM, LPARAM lParam)
{
	ACKDATA *pAck = (ACKDATA *)lParam;
	if (!pAck)
		return 0;

	if (pAck->type != ACKTYPE_MESSAGE)
		return 0;

	MCONTACT hContact = pAck->hContact;
	MessageSendQueueItem *item = FindSendQueueItem(hContact, pAck->hProcess);
	if (item == nullptr)
		item = FindSendQueueItem(hContact = db_mc_getMeta(pAck->hContact), pAck->hProcess);
	if (item == nullptr)
		return 0;

	HWND hwndSender = item->hwndSender;
	if (pAck->result == ACKRESULT_FAILED) {
		if (item->hwndErrorDlg != nullptr)
			item = FindOldestPendingSendQueueItem(hwndSender, hContact);

		if (item != nullptr && item->hwndErrorDlg == nullptr) {
			if (hwndSender != nullptr) {
				SendMessage(hwndSender, DM_STOPMESSAGESENDING, 0, 0);

				CErrorDlg *pDlg = new CErrorDlg((wchar_t*)pAck->lParam, hwndSender, item);
				SendMessage(hwndSender, DM_SHOWERRORMESSAGE, 0, (LPARAM)pDlg);
			}
			else RemoveSendQueueItem(item);
		}
		return 0;
	}

	hContact = (db_mc_isMeta(hContact)) ? db_mc_getSrmmSub(item->hContact) : item->hContact;

	DBEVENTINFO dbei = {};
	dbei.eventType = EVENTTYPE_MESSAGE;
	dbei.flags = DBEF_UTF | DBEF_SENT | ((item->flags & PREF_RTL) ? DBEF_RTL : 0);
	dbei.szModule = GetContactProto(hContact);
	dbei.timestamp = time(0);
	dbei.cbBlob = (int)mir_strlen(item->sendBuffer) + 1;
	dbei.pBlob = (PBYTE)item->sendBuffer;

	MessageWindowEvent evt = { item->hSendId, hContact, &dbei };
	NotifyEventHooks(g_chatApi.hevPreCreate, 0, (LPARAM)&evt);

	item->sendBuffer = (char *)dbei.pBlob;
	MEVENT hNewEvent = db_event_add(hContact, &dbei);
	if (hNewEvent && pAck->lParam)
		db_event_setId(dbei.szModule, hNewEvent, (char *)pAck->lParam);

	if (item->hwndErrorDlg != nullptr)
		DestroyWindow(item->hwndErrorDlg);

	if (RemoveSendQueueItem(item) && db_get_b(0, SRMM_MODULE, SRMSGSET_AUTOCLOSE, SRMSGDEFSET_AUTOCLOSE)) {
		if (hwndSender != nullptr)
			DestroyWindow(hwndSender);
	}
	else if (hwndSender != nullptr) {
		SendMessage(hwndSender, DM_STOPMESSAGESENDING, 0, 0);
		Skin_PlaySound("SendMsg");
	}

	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

int ImageList_AddIcon_Ex(HIMAGELIST hIml, int id)
{
	HICON hIcon = Skin_LoadIcon(id);
	int res = ImageList_AddIcon(hIml, hIcon);
	IcoLib_ReleaseIcon(hIcon);
	return res;
}

int ImageList_AddIcon_Ex2(HIMAGELIST hIml, HICON hIcon)
{
	int res = ImageList_AddIcon(hIml, hIcon);
	IcoLib_ReleaseIcon(hIcon);
	return res;
}

int ImageList_ReplaceIcon_Ex(HIMAGELIST hIml, int nIndex, int id)
{
	HICON hIcon = Skin_LoadIcon(id);
	int res = ImageList_ReplaceIcon(hIml, nIndex, hIcon);
	IcoLib_ReleaseIcon(hIcon);
	return res;
}

int ImageList_AddIcon_ProtoEx(HIMAGELIST hIml, const char* szProto, int status)
{
	HICON hIcon = Skin_LoadProtoIcon(szProto, status);
	int res = ImageList_AddIcon(hIml, hIcon);
	IcoLib_ReleaseIcon(hIcon);
	return res;
}

void ReleaseIcons()
{
	IcoLib_ReleaseIcon(g_dat.hMsgIcon);
	IcoLib_ReleaseIcon(g_dat.hMsgIconBig);
	IcoLib_ReleaseIcon(g_dat.hIconChatBig);
}

HICON GetCachedIcon(const char *name)
{
	for (auto &it : iconList1)
		if (!mir_strcmp(it.szName, name))
			return IcoLib_GetIconByHandle(it.hIcolib);

	for (auto &it : iconList2)
		if (!mir_strcmp(it.szName, name))
			return IcoLib_GetIconByHandle(it.hIcolib);

	for (auto &it : iconList3)
		if (!mir_strcmp(it.szName, name))
			return IcoLib_GetIconByHandle(it.hIcolib);

	return nullptr;
}

void LoadGlobalIcons()
{
	g_dat.hMsgIcon = Skin_LoadIcon(SKINICON_EVENT_MESSAGE);
	g_dat.hMsgIconBig = Skin_LoadIcon(SKINICON_EVENT_MESSAGE, true);
	g_dat.hIconChatBig = IcoLib_GetIcon("chat_window", true);

	ImageList_RemoveAll(g_dat.hButtonIconList);
	ImageList_RemoveAll(g_dat.hChatButtonIconList);
	ImageList_RemoveAll(g_dat.hHelperIconList);
	ImageList_RemoveAll(g_dat.hSearchEngineIconList);
	
	for (auto &it : buttonIcons) {
		if (it == nullptr)
			ImageList_AddIcon_ProtoEx(g_dat.hButtonIconList, nullptr, ID_STATUS_OFFLINE);
		else
			ImageList_AddIcon(g_dat.hButtonIconList, GetCachedIcon(it));
	}
	
	for (auto &it : chatButtonIcons)
		ImageList_AddIcon(g_dat.hChatButtonIconList, GetCachedIcon(it));

	ImageList_AddIcon(g_dat.hHelperIconList, GetCachedIcon("scriver_OVERLAY"));
	int overlayIcon = ImageList_AddIcon(g_dat.hHelperIconList, GetCachedIcon("scriver_OVERLAY"));
	ImageList_SetOverlayImage(g_dat.hHelperIconList, overlayIcon, 1);
	for (int i = IDI_GOOGLE; i < IDI_LASTICON; i++) {
		HICON hIcon = (HICON)LoadImage(g_plugin.getInst(), MAKEINTRESOURCE(i), IMAGE_ICON, 0, 0, 0);
		ImageList_AddIcon(g_dat.hSearchEngineIconList, hIcon);
		DestroyIcon(hIcon);
	}
}

static struct { UINT cpId; const wchar_t *cpName; } cpTable[] =
{
	{ 874, LPGENW("Thai") }, //
	{ 932, LPGENW("Japanese") }, //
	{ 936, LPGENW("Simplified Chinese") }, //
	{ 949, LPGENW("Korean") }, //
	{ 950, LPGENW("Traditional Chinese") }, //
	{ 1250, LPGENW("Central European") }, //
	{ 1251, LPGENW("Cyrillic") }, //
	{ 1252, LPGENW("Latin I") }, //
	{ 1253, LPGENW("Greek") }, //
	{ 1254, LPGENW("Turkish") }, //
	{ 1255, LPGENW("Hebrew") }, //
	{ 1256, LPGENW("Arabic") }, //
	{ 1257, LPGENW("Baltic") }, //
	{ 1258, LPGENW("Vietnamese") }, //
	{ 1361, LPGENW("Korean (Johab)") }
};

void LoadInfobarFonts()
{
	LOGFONT lf;
	LoadMsgDlgFont(MSGFONTID_MESSAGEAREA, &lf, nullptr);
	g_dat.minInputAreaHeight = db_get_dw(0, SRMM_MODULE, SRMSGSET_AUTORESIZELINES, SRMSGDEFSET_AUTORESIZELINES) * abs(lf.lfHeight) * g_dat.logPixelSY / 72;
	
	if (g_dat.hInfobarBrush != nullptr)
		DeleteObject(g_dat.hInfobarBrush);
	g_dat.hInfobarBrush = CreateSolidBrush(db_get_dw(0, SRMM_MODULE, SRMSGSET_INFOBARBKGCOLOUR, SRMSGDEFSET_INFOBARBKGCOLOUR));
}

void InitGlobals()
{
	HDC hdc = GetDC(nullptr);

	memset(&g_dat, 0, sizeof(struct GlobalMessageData));
	g_dat.hParentWindowList = WindowList_Create();

	if (!db_get_b(0, "Compatibility", "Scriver", 0)) {
		if (g_plugin.getByte("SendOnEnter"))
			g_dat.sendMode = SEND_ON_ENTER;
		else if (g_plugin.getByte("SendOnDblEnter"))
			g_dat.sendMode = SEND_ON_DBL_ENTER;
		else if (g_plugin.getByte("SendOnCtrlEnter"))
			g_dat.sendMode = SEND_ON_CTRL_ENTER;

		g_plugin.setByte(SRMSGSET_SENDMODE, g_dat.sendMode);

		g_plugin.delSetting("SendOnEnter");
		g_plugin.delSetting("SendOnDblEnter");
		g_plugin.delSetting("SendOnCtrlEnter");
		db_set_b(0, "Compatibility", "Scriver", 1);
	}

	HookEvent(ME_PROTO_ACK, ackevent);
	ReloadGlobals();
	g_dat.lastParent = nullptr;
	g_dat.lastChatParent = nullptr;
	g_dat.hTabIconList = nullptr;
	g_dat.tabIconListUsage = nullptr;
	g_dat.tabIconListUsageSize = 0;
	g_dat.hButtonIconList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 0, 0);
	g_dat.hChatButtonIconList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 0, 0);
	g_dat.hTabIconList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 0, 0);
	g_dat.hHelperIconList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 0, 0);
	g_dat.hSearchEngineIconList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 0, 0);
	g_dat.logPixelSX = GetDeviceCaps(hdc, LOGPIXELSX);
	g_dat.logPixelSY = GetDeviceCaps(hdc, LOGPIXELSY);
	LoadInfobarFonts();
	ReleaseDC(nullptr, hdc);
}

void FreeGlobals()
{
	if (g_dat.hInfobarBrush != nullptr)
		DeleteObject(g_dat.hInfobarBrush);
	if (g_dat.hTabIconList)
		ImageList_Destroy(g_dat.hTabIconList);
	if (g_dat.hButtonIconList)
		ImageList_Destroy(g_dat.hButtonIconList);
	if (g_dat.hChatButtonIconList)
		ImageList_Destroy(g_dat.hChatButtonIconList);
	if (g_dat.hHelperIconList)
		ImageList_Destroy(g_dat.hHelperIconList);
	if (g_dat.hSearchEngineIconList)
		ImageList_Destroy(g_dat.hSearchEngineIconList);
	mir_free(g_dat.tabIconListUsage);

	WindowList_Destroy(g_dat.hParentWindowList);

	memset(&g_dat, 0, sizeof(g_dat));
}

void ReloadGlobals()
{
	g_dat.flags = 0;
	g_dat.flags2 = 0;
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_AVATARENABLE, SRMSGDEFSET_AVATARENABLE))
		g_dat.flags |= SMF_AVATAR;
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_SHOWPROGRESS, SRMSGDEFSET_SHOWPROGRESS))
		g_dat.flags |= SMF_SHOWPROGRESS;
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_SHOWLOGICONS, SRMSGDEFSET_SHOWLOGICONS))
		g_dat.flags |= SMF_SHOWICONS;
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_SHOWTIME, SRMSGDEFSET_SHOWTIME))
		g_dat.flags |= SMF_SHOWTIME;
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_SHOWSECONDS, SRMSGDEFSET_SHOWSECONDS))
		g_dat.flags |= SMF_SHOWSECONDS;
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_SHOWDATE, SRMSGDEFSET_SHOWDATE))
		g_dat.flags |= SMF_SHOWDATE;
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_USELONGDATE, SRMSGDEFSET_USELONGDATE))
		g_dat.flags |= SMF_LONGDATE;
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_USERELATIVEDATE, SRMSGDEFSET_USERELATIVEDATE))
		g_dat.flags |= SMF_RELATIVEDATE;
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_GROUPMESSAGES, SRMSGDEFSET_GROUPMESSAGES))
		g_dat.flags |= SMF_GROUPMESSAGES;
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_MARKFOLLOWUPS, SRMSGDEFSET_MARKFOLLOWUPS))
		g_dat.flags |= SMF_MARKFOLLOWUPS;
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_MESSAGEONNEWLINE, SRMSGDEFSET_MESSAGEONNEWLINE))
		g_dat.flags |= SMF_MSGONNEWLINE;
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_DRAWLINES, SRMSGDEFSET_DRAWLINES))
		g_dat.flags |= SMF_DRAWLINES;
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_HIDENAMES, SRMSGDEFSET_HIDENAMES))
		g_dat.flags |= SMF_HIDENAMES;
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_AUTOPOPUP, SRMSGDEFSET_AUTOPOPUP))
		g_dat.flags |= SMF_AUTOPOPUP;
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_STAYMINIMIZED, SRMSGDEFSET_STAYMINIMIZED))
		g_dat.flags |= SMF_STAYMINIMIZED;
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_SAVEDRAFTS, SRMSGDEFSET_SAVEDRAFTS))
		g_dat.flags |= SMF_SAVEDRAFTS;

	if (db_get_b(0, SRMM_MODULE, SRMSGSET_DELTEMP, SRMSGDEFSET_DELTEMP))
		g_dat.flags |= SMF_DELTEMP;
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_INDENTTEXT, SRMSGDEFSET_INDENTTEXT))
		g_dat.flags |= SMF_INDENTTEXT;

	g_dat.sendMode = (SendMode)db_get_b(0, SRMM_MODULE, SRMSGSET_SENDMODE, SRMSGDEFSET_SENDMODE);
	g_dat.openFlags = db_get_dw(0, SRMM_MODULE, SRMSGSET_POPFLAGS, SRMSGDEFSET_POPFLAGS);
	g_dat.indentSize = db_get_w(0, SRMM_MODULE, SRMSGSET_INDENTSIZE, SRMSGDEFSET_INDENTSIZE);
	g_dat.logLineColour = db_get_dw(0, SRMM_MODULE, SRMSGSET_LINECOLOUR, SRMSGDEFSET_LINECOLOUR);

	if (db_get_b(0, SRMM_MODULE, SRMSGSET_USETABS, SRMSGDEFSET_USETABS))
		g_dat.flags2 |= SMF2_USETABS;
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_TABSATBOTTOM, SRMSGDEFSET_TABSATBOTTOM))
		g_dat.flags2 |= SMF2_TABSATBOTTOM;
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_SWITCHTOACTIVE, SRMSGDEFSET_SWITCHTOACTIVE))
		g_dat.flags2 |= SMF2_SWITCHTOACTIVE;
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_LIMITNAMES, SRMSGDEFSET_LIMITNAMES))
		g_dat.flags2 |= SMF2_LIMITNAMES;
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_HIDEONETAB, SRMSGDEFSET_HIDEONETAB))
		g_dat.flags2 |= SMF2_HIDEONETAB;
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_SEPARATECHATSCONTAINERS, SRMSGDEFSET_SEPARATECHATSCONTAINERS))
		g_dat.flags2 |= SMF2_SEPARATECHATSCONTAINERS;
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_TABCLOSEBUTTON, SRMSGDEFSET_TABCLOSEBUTTON))
		g_dat.flags2 |= SMF2_TABCLOSEBUTTON;
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_LIMITTABS, SRMSGDEFSET_LIMITTABS))
		g_dat.flags2 |= SMF2_LIMITTABS;
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_LIMITCHATSTABS, SRMSGDEFSET_LIMITCHATSTABS))
		g_dat.flags2 |= SMF2_LIMITCHATSTABS;
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_HIDECONTAINERS, SRMSGDEFSET_HIDECONTAINERS))
		g_dat.flags2 |= SMF2_HIDECONTAINERS;

	if (db_get_b(0, SRMM_MODULE, SRMSGSET_SHOWSTATUSBAR, SRMSGDEFSET_SHOWSTATUSBAR))
		g_dat.flags2 |= SMF2_SHOWSTATUSBAR;
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_SHOWTITLEBAR, SRMSGDEFSET_SHOWTITLEBAR))
		g_dat.flags2 |= SMF2_SHOWTITLEBAR;
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_SHOWBUTTONLINE, SRMSGDEFSET_SHOWBUTTONLINE))
		g_dat.flags2 |= SMF2_SHOWTOOLBAR;
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_SHOWINFOBAR, SRMSGDEFSET_SHOWINFOBAR))
		g_dat.flags2 |= SMF2_SHOWINFOBAR;

	if (db_get_b(0, SRMM_MODULE, SRMSGSET_SHOWTYPING, SRMSGDEFSET_SHOWTYPING))
		g_dat.flags2 |= SMF2_SHOWTYPING;
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_SHOWTYPINGWIN, SRMSGDEFSET_SHOWTYPINGWIN))
		g_dat.flags2 |= SMF2_SHOWTYPINGWIN;
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_SHOWTYPINGNOWIN, SRMSGDEFSET_SHOWTYPINGNOWIN))
		g_dat.flags2 |= SMF2_SHOWTYPINGTRAY;
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_SHOWTYPINGCLIST, SRMSGDEFSET_SHOWTYPINGCLIST))
		g_dat.flags2 |= SMF2_SHOWTYPINGCLIST;
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_SHOWTYPINGSWITCH, SRMSGDEFSET_SHOWTYPINGSWITCH))
		g_dat.flags2 |= SMF2_SHOWTYPINGSWITCH;

	if (LOBYTE(LOWORD(GetVersion())) >= 5) {
		if (db_get_b(0, SRMM_MODULE, SRMSGSET_USETRANSPARENCY, SRMSGDEFSET_USETRANSPARENCY))
			g_dat.flags2 |= SMF2_USETRANSPARENCY;
		g_dat.activeAlpha = db_get_dw(0, SRMM_MODULE, SRMSGSET_ACTIVEALPHA, SRMSGDEFSET_ACTIVEALPHA);
		g_dat.inactiveAlpha = db_get_dw(0, SRMM_MODULE, SRMSGSET_INACTIVEALPHA, SRMSGDEFSET_INACTIVEALPHA);
	}
	if (db_get_b(0, SRMM_MODULE, SRMSGSET_USEIEVIEW, SRMSGDEFSET_USEIEVIEW))
		g_dat.flags |= SMF_USEIEVIEW;

	g_dat.limitNamesLength = db_get_dw(0, SRMM_MODULE, SRMSGSET_LIMITNAMESLEN, SRMSGDEFSET_LIMITNAMESLEN);
	g_dat.limitTabsNum = db_get_dw(0, SRMM_MODULE, SRMSGSET_LIMITTABSNUM, SRMSGDEFSET_LIMITTABSNUM);
	g_dat.limitChatsTabsNum = db_get_dw(0, SRMM_MODULE, SRMSGSET_LIMITCHATSTABSNUM, SRMSGDEFSET_LIMITCHATSTABSNUM);

	ptrW wszTitleFormat(db_get_wsa(0, SRMM_MODULE, SRMSGSET_WINDOWTITLE));
	if (wszTitleFormat == nullptr)
		g_dat.wszTitleFormat[0] = 0;
	else
		wcsncpy_s(g_dat.wszTitleFormat, wszTitleFormat, _TRUNCATE);
}

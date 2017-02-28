/////////////////////////////////////////////////////////////////////////////////////////
// Miranda NG: the free IM client for Microsoft* Windows*
//
// Copyright (�) 2012-17 Miranda NG project,
// Copyright (c) 2000-09 Miranda ICQ/IM project,
// all portions of this codebase are copyrighted to the people
// listed in contributors.txt.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// you should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// part of tabSRMM messaging plugin for Miranda.
//
// (C) 2005-2010 by silvercircle _at_ gmail _dot_ com and contributors
//
// Helper functions for the message dialog.

#include "stdafx.h"

#ifndef SHVIEW_THUMBNAIL
#define SHVIEW_THUMBNAIL 0x702D
#endif

#define EVENTTYPE_WAT_ANSWER            9602
#define EVENTTYPE_JABBER_CHATSTATES     2000
#define EVENTTYPE_JABBER_PRESENCE       2001

static int g_status_events[] = {
	EVENTTYPE_STATUSCHANGE,
	EVENTTYPE_WAT_ANSWER,
	EVENTTYPE_JABBER_CHATSTATES,
	EVENTTYPE_JABBER_PRESENCE
};

static int g_status_events_size = 0;

bool TSAPI IsStatusEvent(int eventType)
{
	if (g_status_events_size == 0)
		g_status_events_size = _countof(g_status_events);

	for (int i = 0; i < g_status_events_size; i++) {
		if (eventType == g_status_events[i])
			return true;
	}
	return false;
}

bool TSAPI IsCustomEvent(int eventType)
{
	if (eventType == EVENTTYPE_MESSAGE || eventType == EVENTTYPE_URL || eventType == EVENTTYPE_CONTACTS ||
		eventType == EVENTTYPE_ADDED || eventType == EVENTTYPE_AUTHREQUEST || eventType == EVENTTYPE_FILE)
		return false;

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////
// reorder tabs within a container. fSavePos indicates whether the new position should
// be saved to the contacts db record (if so, the container will try to open the tab
// at the saved position later)

void TSAPI RearrangeTab(HWND hwndDlg, const CTabBaseDlg *dat, int iMode, BOOL fSavePos)
{
	if (dat == NULL || !IsWindow(hwndDlg))
		return;

	HWND hwndTab = GetParent(hwndDlg);
	wchar_t oldText[512];

	TCITEM item = { 0 };
	item.mask = TCIF_IMAGE | TCIF_TEXT | TCIF_PARAM;
	item.pszText = oldText;
	item.cchTextMax = _countof(oldText);
	TabCtrl_GetItem(hwndTab, dat->iTabID, &item);

	int newIndex = LOWORD(iMode);
	if (newIndex >= 0 && newIndex <= TabCtrl_GetItemCount(hwndTab)) {
		TabCtrl_DeleteItem(hwndTab, dat->iTabID);
		item.lParam = (LPARAM)hwndDlg;
		TabCtrl_InsertItem(hwndTab, newIndex, &item);
		BroadCastContainer(dat->pContainer, DM_REFRESHTABINDEX, 0, 0);
		ActivateTabFromHWND(hwndTab, hwndDlg);
		if (fSavePos)
			db_set_dw(dat->m_hContact, SRMSGMOD_T, "tabindex", newIndex * 100);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// subclassing for the save as file dialog (needed to set it to thumbnail view on Windows 2000
// or later

static UINT_PTR CALLBACK OpenFileSubclass(HWND hwnd, UINT msg, WPARAM, LPARAM lParam)
{
	switch (msg) {
	case WM_INITDIALOG:
		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)lParam);
		break;

	case WM_NOTIFY:
		OPENFILENAMEA *ofn = (OPENFILENAMEA *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
		HWND hwndParent = GetParent(hwnd);
		HWND hwndLv = FindWindowEx(hwndParent, NULL, L"SHELLDLL_DefView", NULL);

		if (hwndLv != NULL && *((DWORD *)(ofn->lCustData))) {
			SendMessage(hwndLv, WM_COMMAND, SHVIEW_THUMBNAIL, 0);
			*((DWORD *)(ofn->lCustData)) = 0;
		}
		break;
	}
	return FALSE;
}

/////////////////////////////////////////////////////////////////////////////////////////
// saves a contact picture to disk
// takes hbm (bitmap handle) and bool isOwnPic (1 == save the picture as your own avatar)
// requires AVS and ADVAIMG services (Miranda 0.7+)

static void SaveAvatarToFile(CTabBaseDlg *dat, HBITMAP hbm, int isOwnPic)
{
	wchar_t szFinalFilename[MAX_PATH];
	time_t t = time(NULL);
	struct tm *lt = localtime(&t);
	DWORD setView = 1;

	wchar_t szTimestamp[100];
	mir_snwprintf(szTimestamp, L"%04u %02u %02u_%02u%02u", lt->tm_year + 1900, lt->tm_mon, lt->tm_mday, lt->tm_hour, lt->tm_min);

	wchar_t *szProto = mir_a2u(dat->cache->getActiveProto());

	wchar_t szFinalPath[MAX_PATH];
	mir_snwprintf(szFinalPath, L"%s\\%s", M.getSavedAvatarPath(), szProto);
	mir_free(szProto);

	if (CreateDirectory(szFinalPath, 0) == 0) {
		if (GetLastError() != ERROR_ALREADY_EXISTS) {
			MessageBox(0, TranslateT("Error creating destination directory"),
				TranslateT("Save contact picture"), MB_OK | MB_ICONSTOP);
			return;
		}
	}

	wchar_t szBaseName[MAX_PATH];
	if (isOwnPic)
		mir_snwprintf(szBaseName, L"My Avatar_%s", szTimestamp);
	else
		mir_snwprintf(szBaseName, L"%s_%s", dat->cache->getNick(), szTimestamp);

	mir_snwprintf(szFinalFilename, L"%s.png", szBaseName);

	// do not allow / or \ or % in the filename
	Utils::sanitizeFilename(szFinalFilename);

	wchar_t filter[MAX_PATH];
	mir_snwprintf(filter, L"%s%c*.bmp;*.png;*.jpg;*.gif%c%c", TranslateT("Image files"), 0, 0, 0);

	OPENFILENAME ofn = { 0 };
	ofn.lpstrDefExt = L"png";
	ofn.lpstrFilter = filter;
	ofn.Flags = OFN_HIDEREADONLY | OFN_EXPLORER | OFN_ENABLESIZING | OFN_ENABLEHOOK;
	ofn.lpfnHook = OpenFileSubclass;
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFile = szFinalFilename;
	ofn.lpstrInitialDir = szFinalPath;
	ofn.nMaxFile = MAX_PATH;
	ofn.nMaxFileTitle = MAX_PATH;
	ofn.lCustData = (LPARAM)&setView;
	if (GetSaveFileName(&ofn)) {
		if (PathFileExists(szFinalFilename))
			if (MessageBox(0, TranslateT("The file exists. Do you want to overwrite it?"), TranslateT("Save contact picture"), MB_YESNO | MB_ICONQUESTION) == IDNO)
				return;

		IMGSRVC_INFO ii;
		ii.cbSize = sizeof(ii);
		ii.wszName = szFinalFilename;
		ii.hbm = hbm;
		ii.dwMask = IMGI_HBITMAP;
		ii.fif = FIF_UNKNOWN;			// get the format from the filename extension. png is default.
		CallService(MS_IMG_SAVE, (WPARAM)&ii, IMGL_WCHAR);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// flash a tab icon if mode = true, otherwise restore default icon
// store flashing state into bState

void CTabBaseDlg::FlashTab(bool bInvertMode)
{
	if (bInvertMode)
		m_bTabFlash = !m_bTabFlash;

	TCITEM item = { 0 };
	item.mask = TCIF_IMAGE;
	TabCtrl_SetItem(m_hwndParent, iTabID, &item);
	if (pContainer->dwFlags & CNT_SIDEBAR)
		pContainer->SideBar->updateSession(this);
}

/////////////////////////////////////////////////////////////////////////////////////////
// calculates avatar layouting, based on splitter position to find the optimal size
// for the avatar w/o disturbing the toolbar too much.

void CTabBaseDlg::CalcDynamicAvatarSize(BITMAP *bminfo)
{
	if (dwFlags & MWF_WASBACKGROUNDCREATE || pContainer->dwFlags & CNT_DEFERREDCONFIGURE || pContainer->dwFlags & CNT_CREATE_MINIMIZED || IsIconic(pContainer->hwnd))
		return;  // at this stage, the layout is not yet ready...

	RECT rc;
	GetClientRect(GetHwnd(), &rc);

	BOOL bBottomToolBar = pContainer->dwFlags & CNT_BOTTOMTOOLBAR;
	BOOL bToolBar = pContainer->dwFlags & CNT_HIDETOOLBAR ? 0 : 1;
	int  iSplitOffset = bIsAutosizingInput ? 1 : 0;

	double picAspect = (bminfo->bmWidth == 0 || bminfo->bmHeight == 0) ? 1.0 : (double)(bminfo->bmWidth / (double)bminfo->bmHeight);
	double picProjectedWidth = (double)((dynaSplitter - ((bBottomToolBar && bToolBar) ? DPISCALEX_S(24) : 0) + ((bShowUIElements) ? DPISCALEX_S(28) : DPISCALEX_S(2)))) * picAspect;

	if ((rc.right - (int)picProjectedWidth) > (iButtonBarReallyNeeds) && !PluginConfig.m_bAlwaysFullToolbarWidth && bToolBar)
		iRealAvatarHeight = dynaSplitter + 3 + (bShowUIElements ? DPISCALEY_S(28) : DPISCALEY_S(2));
	else
		iRealAvatarHeight = dynaSplitter + DPISCALEY_S(6) + DPISCALEY_S(iSplitOffset);

	iRealAvatarHeight -= ((bBottomToolBar&&bToolBar) ? DPISCALEY_S(22) : 0);

	if (PluginConfig.m_LimitStaticAvatarHeight > 0)
		iRealAvatarHeight = min(iRealAvatarHeight, PluginConfig.m_LimitStaticAvatarHeight);

	if (M.GetByte(m_hContact, "dontscaleavatars", M.GetByte("dontscaleavatars", 0)))
		iRealAvatarHeight = min(bminfo->bmHeight, iRealAvatarHeight);

	double aspect = (bminfo->bmHeight != 0) ? (double)iRealAvatarHeight / (double)bminfo->bmHeight : 1.0;
	double newWidth = (double)bminfo->bmWidth * aspect;
	if (newWidth > (double)(rc.right) * 0.8)
		newWidth = (double)(rc.right) * 0.8;
	pic.cy = iRealAvatarHeight + 2;
	pic.cx = (int)newWidth + 2;
}

int CTabBaseDlg::MsgWindowUpdateMenu(HMENU submenu, int menuID)
{
	bool bInfoPanel = Panel->isActive();

	if (menuID == MENU_TABCONTEXT) {
		EnableMenuItem(submenu, ID_TABMENU_LEAVECHATROOM, (bType == SESSIONTYPE_CHAT && ProtoServiceExists(szProto, PS_LEAVECHAT)) ? MF_ENABLED : MF_DISABLED);
		EnableMenuItem(submenu, ID_TABMENU_ATTACHTOCONTAINER, (M.GetByte("useclistgroups", 0) || M.GetByte("singlewinmode", 0)) ? MF_GRAYED : MF_ENABLED);
		EnableMenuItem(submenu, ID_TABMENU_CLEARSAVEDTABPOSITION, (M.GetDword(m_hContact, "tabindex", -1) != -1) ? MF_ENABLED : MF_GRAYED);
	}
	else if (menuID == MENU_PICMENU) {
		wchar_t *szText = NULL;
		char  avOverride = (char)M.GetByte(m_hContact, "hideavatar", -1);
		HMENU visMenu = GetSubMenu(submenu, 0);
		BOOL picValid = bInfoPanel ? (hOwnPic != 0) : (ace && ace->hbmPic && ace->hbmPic != PluginConfig.g_hbmUnknown);

		MENUITEMINFO mii = { 0 };
		mii.cbSize = sizeof(mii);
		mii.fMask = MIIM_STRING;

		EnableMenuItem(submenu, ID_PICMENU_SAVETHISPICTUREAS, MF_BYCOMMAND | (picValid ? MF_ENABLED : MF_GRAYED));

		CheckMenuItem(visMenu, ID_VISIBILITY_DEFAULT, MF_BYCOMMAND | (avOverride == -1 ? MF_CHECKED : MF_UNCHECKED));
		CheckMenuItem(visMenu, ID_VISIBILITY_HIDDENFORTHISCONTACT, MF_BYCOMMAND | (avOverride == 0 ? MF_CHECKED : MF_UNCHECKED));
		CheckMenuItem(visMenu, ID_VISIBILITY_VISIBLEFORTHISCONTACT, MF_BYCOMMAND | (avOverride == 1 ? MF_CHECKED : MF_UNCHECKED));

		CheckMenuItem(submenu, ID_PICMENU_ALWAYSKEEPTHEBUTTONBARATFULLWIDTH, MF_BYCOMMAND | (PluginConfig.m_bAlwaysFullToolbarWidth ? MF_CHECKED : MF_UNCHECKED));
		if (!bInfoPanel) {
			EnableMenuItem(submenu, ID_PICMENU_SETTINGS, MF_BYCOMMAND | (ServiceExists(MS_AV_GETAVATARBITMAP) ? MF_ENABLED : MF_GRAYED));
			szText = TranslateT("Contact picture settings...");
			EnableMenuItem(submenu, 0, MF_BYPOSITION | MF_ENABLED);
		}
		else {
			EnableMenuItem(submenu, 0, MF_BYPOSITION | MF_GRAYED);
			EnableMenuItem(submenu, ID_PICMENU_SETTINGS, MF_BYCOMMAND | ((ServiceExists(MS_AV_SETMYAVATARW) && CallService(MS_AV_CANSETMYAVATAR, (WPARAM)(cache->getActiveProto()), 0)) ? MF_ENABLED : MF_GRAYED));
			szText = TranslateT("Set your avatar...");
		}
		mii.dwTypeData = szText;
		mii.cch = (int)mir_wstrlen(szText) + 1;
		SetMenuItemInfo(submenu, ID_PICMENU_SETTINGS, FALSE, &mii);
	}
	else if (menuID == MENU_PANELPICMENU) {
		HMENU visMenu = GetSubMenu(submenu, 0);
		char  avOverride = (char)M.GetByte(m_hContact, "hideavatar", -1);

		CheckMenuItem(visMenu, ID_VISIBILITY_DEFAULT, MF_BYCOMMAND | (avOverride == -1 ? MF_CHECKED : MF_UNCHECKED));
		CheckMenuItem(visMenu, ID_VISIBILITY_HIDDENFORTHISCONTACT, MF_BYCOMMAND | (avOverride == 0 ? MF_CHECKED : MF_UNCHECKED));
		CheckMenuItem(visMenu, ID_VISIBILITY_VISIBLEFORTHISCONTACT, MF_BYCOMMAND | (avOverride == 1 ? MF_CHECKED : MF_UNCHECKED));

		EnableMenuItem(submenu, ID_PICMENU_SETTINGS, MF_BYCOMMAND | (ServiceExists(MS_AV_GETAVATARBITMAP) ? MF_ENABLED : MF_GRAYED));
		EnableMenuItem(submenu, ID_PANELPICMENU_SAVETHISPICTUREAS, MF_BYCOMMAND | ((ace && ace->hbmPic && ace->hbmPic != PluginConfig.g_hbmUnknown) ? MF_ENABLED : MF_GRAYED));
	}
	return 0;
}

int CTabBaseDlg::MsgWindowMenuHandler(int selection, int menuId)
{
	if (menuId == MENU_PICMENU || menuId == MENU_PANELPICMENU || menuId == MENU_TABCONTEXT) {
		switch (selection) {
		case ID_TABMENU_ATTACHTOCONTAINER:
			CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_SELECTCONTAINER), m_hwnd, SelectContainerDlgProc, (LPARAM)m_hwnd);
			return 1;
		case ID_TABMENU_CONTAINEROPTIONS:
			if (pContainer->hWndOptions == 0)
				CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_CONTAINEROPTIONS), m_hwnd, DlgProcContainerOptions, (LPARAM)pContainer);
			return 1;
		case ID_TABMENU_CLOSECONTAINER:
			SendMessage(pContainer->hwnd, WM_CLOSE, 0, 0);
			return 1;
		case ID_TABMENU_CLOSETAB:
			SendMessage(m_hwnd, WM_CLOSE, 1, 0);
			return 1;
		case ID_TABMENU_SAVETABPOSITION:
			db_set_dw(m_hContact, SRMSGMOD_T, "tabindex", iTabID * 100);
			break;
		case ID_TABMENU_CLEARSAVEDTABPOSITION:
			db_unset(m_hContact, SRMSGMOD_T, "tabindex");
			break;
		case ID_TABMENU_LEAVECHATROOM:
			if (bType == SESSIONTYPE_CHAT) {
				if (si != NULL && m_hContact != NULL) {
					char *szProto = GetContactProto(m_hContact);
					if (szProto)
						CallProtoService(szProto, PS_LEAVECHAT, m_hContact, 0);
				}
			}
			return 1;

		case ID_VISIBILITY_DEFAULT:
		case ID_VISIBILITY_HIDDENFORTHISCONTACT:
		case ID_VISIBILITY_VISIBLEFORTHISCONTACT:
			{
				BYTE avOverrideMode;
				if (selection == ID_VISIBILITY_DEFAULT)
					avOverrideMode = -1;
				else if (selection == ID_VISIBILITY_VISIBLEFORTHISCONTACT)
					avOverrideMode = 1;
				else
					avOverrideMode = 0;
				db_set_b(m_hContact, SRMSGMOD_T, "hideavatar", avOverrideMode);
			}

			ShowPicture(false);
			SendMessage(m_hwnd, WM_SIZE, 0, 0);
			DM_ScrollToBottom(this, 0, 1);
			return 1;

		case ID_PICMENU_ALWAYSKEEPTHEBUTTONBARATFULLWIDTH:
			PluginConfig.m_bAlwaysFullToolbarWidth = !PluginConfig.m_bAlwaysFullToolbarWidth;
			db_set_b(0, SRMSGMOD_T, "alwaysfulltoolbar", (BYTE)PluginConfig.m_bAlwaysFullToolbarWidth);
			M.BroadcastMessage(DM_CONFIGURETOOLBAR, 0, 1);
			break;

		case ID_PICMENU_SAVETHISPICTUREAS:
			if (Panel->isActive())
				SaveAvatarToFile(this, hOwnPic, 1);
			else if (ace)
				SaveAvatarToFile(this, ace->hbmPic, 0);
			break;

		case ID_PANELPICMENU_SAVETHISPICTUREAS:
			if (ace)
				SaveAvatarToFile(this, ace->hbmPic, 0);
			break;

		case ID_PICMENU_SETTINGS:
			if (menuId == MENU_PANELPICMENU)
				CallService(MS_AV_CONTACTOPTIONS, m_hContact, 0);
			else if (menuId == MENU_PICMENU) {
				if (Panel->isActive()) {
					if (ServiceExists(MS_AV_SETMYAVATARW) && CallService(MS_AV_CANSETMYAVATAR, (WPARAM)(cache->getActiveProto()), 0))
						CallService(MS_AV_SETMYAVATARW, (WPARAM)(cache->getActiveProto()), 0);
				}
				else
					CallService(MS_AV_CONTACTOPTIONS, m_hContact, 0);
			}
			return 1;
		}
	}
	else if (menuId == MENU_LOGMENU) {
		switch (selection) {
		case ID_MESSAGELOGSETTINGS_GLOBAL:
			Options_Open(NULL, L"Message sessions", L"Message log");
			return 1;

		case ID_MESSAGELOGSETTINGS_FORTHISCONTACT:
			CallService(MS_TABMSG_SETUSERPREFS, m_hContact, 0);
			return 1;
		}
	}
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// update the status bar field which displays the number of characters in the input area
// and various indicators (caps lock, num lock, insert mode).

void CTabBaseDlg::UpdateReadChars() const
{
	if (!pContainer->hwndStatus || pContainer->hwndActive != m_hwnd)
		return;

	int len;
	if (bType == SESSIONTYPE_CHAT)
		len = GetWindowTextLength(m_message.GetHwnd());
	else {
		// retrieve text length in UTF8 bytes, because this is the relevant length for most protocols
		GETTEXTLENGTHEX gtxl = { 0 };
		gtxl.codepage = CP_UTF8;
		gtxl.flags = GTL_DEFAULT | GTL_PRECISE | GTL_NUMBYTES;

		len = m_message.SendMsg(EM_GETTEXTLENGTHEX, (WPARAM)&gtxl, 0);
	}

	BOOL fCaps = (GetKeyState(VK_CAPITAL) & 1);
	BOOL fNum = (GetKeyState(VK_NUMLOCK) & 1);

	wchar_t szBuf[20]; szBuf[0] = 0;
	if (fInsertMode)
		mir_wstrcat(szBuf, L"O");
	if (fCaps)
		mir_wstrcat(szBuf, L"C");
	if (fNum)
		mir_wstrcat(szBuf, L"N");
	if (fInsertMode || fCaps || fNum)
		mir_wstrcat(szBuf, L" | ");

	wchar_t buf[128];
	mir_snwprintf(buf, L"%s%s %d/%d", szBuf, lcID, iOpenJobs, len);
	SendMessage(pContainer->hwndStatus, SB_SETTEXT, 1, (LPARAM)buf);
	if (PluginConfig.m_visualMessageSizeIndicator)
		InvalidateRect(pContainer->hwndStatus, NULL, FALSE);
}

/////////////////////////////////////////////////////////////////////////////////////////
// update all status bar fields and force a redraw of the status bar.

void CTabBaseDlg::UpdateStatusBar() const
{
	if (pContainer->hwndStatus && pContainer->hwndActive == m_hwnd) {
		if (bType == SESSIONTYPE_IM) {
			if (szStatusBar[0]) {
				SendMessage(pContainer->hwndStatus, SB_SETICON, 0, (LPARAM)PluginConfig.g_buttonBarIcons[ICON_DEFAULT_TYPING]);
				SendMessage(pContainer->hwndStatus, SB_SETTEXT, 0, (LPARAM)szStatusBar);
			}
			else if (sbCustom) {
				SendMessage(pContainer->hwndStatus, SB_SETICON, 0, (LPARAM)sbCustom->hIcon);
				SendMessage(pContainer->hwndStatus, SB_SETTEXT, 0, (LPARAM)sbCustom->tszText);
			}
			else {
				SendMessage(pContainer->hwndStatus, SB_SETICON, 0, 0);
				DM_UpdateLastMessage(this);
			}
		}
		else {
			if (sbCustom) {
				SendMessage(pContainer->hwndStatus, SB_SETICON, 0, (LPARAM)sbCustom->hIcon);
				SendMessage(pContainer->hwndStatus, SB_SETTEXT, 0, (LPARAM)sbCustom->tszText);
			}
			else SendMessage(pContainer->hwndStatus, SB_SETICON, 0, 0);
		}
		UpdateReadChars();
		InvalidateRect(pContainer->hwndStatus, NULL, TRUE);
		SendMessage(pContainer->hwndStatus, WM_USER + 101, 0, (LPARAM)this);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// provide user feedback via icons on tabs.Used to indicate "send in progress" or
// any error state.
//
// NOT used for typing notification feedback as this is handled directly from the
// MTN handler.

void TSAPI HandleIconFeedback(CTabBaseDlg *dat, HICON iIcon)
{
	TCITEM item = { 0 };

	if (iIcon == (HICON)-1) { // restore status image
		if (dat->dwFlags & MWF_ERRORSTATE)
			dat->hTabIcon = PluginConfig.g_iconErr;
		else
			dat->hTabIcon = dat->hTabStatusIcon;
	}
	else dat->hTabIcon = iIcon;

	item.iImage = 0;
	item.mask = TCIF_IMAGE;
	if (dat->pContainer->dwFlags & CNT_SIDEBAR)
		dat->pContainer->SideBar->updateSession(dat);
	else
		TabCtrl_SetItem(GetDlgItem(dat->pContainer->hwnd, IDC_MSGTABS), dat->iTabID, &item);
}

/////////////////////////////////////////////////////////////////////////////////////////
// catches notifications from the AVS controls

void TSAPI ProcessAvatarChange(HWND hwnd, LPARAM lParam)
{
	if (((LPNMHDR)lParam)->code == NM_AVATAR_CHANGED) {
		HWND hwndDlg = GetParent(hwnd);
		CTabBaseDlg *dat = (CTabBaseDlg*)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
		if (!dat)
			return;

		dat->ace = Utils::loadAvatarFromAVS(dat->cache->getActiveContact());

		dat->GetAvatarVisibility();
		dat->ShowPicture(true);
		if (dat->Panel->isActive())
			SendMessage(hwndDlg, WM_SIZE, 0, 0);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// retrieve the visiblity of the avatar window, depending on the global setting
// and local mode

bool CTabBaseDlg::GetAvatarVisibility()
{
	BYTE bAvatarMode = pContainer->avatarMode;
	BYTE bOwnAvatarMode = pContainer->ownAvatarMode;
	char hideOverride = (char)M.GetByte(m_hContact, "hideavatar", -1);

	// infopanel visible, consider own avatar display
	bShowAvatar = false;

	if (Panel->isActive() && bAvatarMode != 3) {
		if (!bOwnAvatarMode) {
			bShowAvatar = (hOwnPic && hOwnPic != PluginConfig.g_hbmUnknown);
			if (!hwndContactPic)
				hwndContactPic = CreateWindowEx(WS_EX_TOPMOST, AVATAR_CONTROL_CLASS, L"", WS_VISIBLE | WS_CHILD, 1, 1, 1, 1, GetDlgItem(m_hwnd, IDC_CONTACTPIC), (HMENU)0, NULL, NULL);
		}

		switch (bAvatarMode) {
		case 2:
			bShowInfoAvatar = false;
			break;
		case 0:
			bShowInfoAvatar = true;
		case 1:
			HBITMAP hbm = ((ace && !(ace->dwFlags & AVS_HIDEONCLIST)) ? ace->hbmPic : 0);
			if (hbm == NULL && !bAvatarMode) {
				bShowInfoAvatar = false;
				break;
			}

			if (!hwndPanelPic) {
				hwndPanelPic = CreateWindowEx(WS_EX_TOPMOST, AVATAR_CONTROL_CLASS, L"", WS_VISIBLE | WS_CHILD, 1, 1, 1, 1, hwndPanelPicParent, (HMENU)7000, NULL, NULL);
				if (hwndPanelPic)
					SendMessage(hwndPanelPic, AVATAR_SETAEROCOMPATDRAWING, 0, TRUE);
			}

			if (bAvatarMode != 0)
				bShowInfoAvatar = (hbm && hbm != PluginConfig.g_hbmUnknown);
			break;
		}

		if (bShowInfoAvatar)
			bShowInfoAvatar = hideOverride == 0 ? false : bShowInfoAvatar;
		else
			bShowInfoAvatar = hideOverride == 1 ? true : bShowInfoAvatar;

		Utils::setAvatarContact(hwndPanelPic, m_hContact);
		SendMessage(hwndContactPic, AVATAR_SETPROTOCOL, 0, (LPARAM)cache->getActiveProto());
	}
	else {
		bShowInfoAvatar = false;

		switch (bAvatarMode) {
		case 0: // globally on
			bShowAvatar = true;
LBL_Check:
			if (!hwndContactPic)
				hwndContactPic = CreateWindowEx(WS_EX_TOPMOST, AVATAR_CONTROL_CLASS, L"", WS_VISIBLE | WS_CHILD, 1, 1, 1, 1, GetDlgItem(m_hwnd, IDC_CONTACTPIC), (HMENU)0, NULL, NULL);
			break;
		case 2: // globally OFF
			bShowAvatar = false;
			break;
		case 3: // on, if present
		case 1:
			HBITMAP hbm = (ace && !(ace->dwFlags & AVS_HIDEONCLIST)) ? ace->hbmPic : 0;
			bShowAvatar = (hbm && hbm != PluginConfig.g_hbmUnknown);
			goto LBL_Check;
		}

		if (bShowAvatar)
			bShowAvatar = hideOverride == 0 ? 0 : bShowAvatar;
		else
			bShowAvatar = hideOverride == 1 ? 1 : bShowAvatar;

		// reloads avatars
		if (hwndPanelPic) { // shows contact or user picture, depending on panel visibility
			SendMessage(hwndContactPic, AVATAR_SETPROTOCOL, 0, (LPARAM)cache->getActiveProto());
			Utils::setAvatarContact(hwndPanelPic, m_hContact);
		}
		else Utils::setAvatarContact(hwndContactPic, m_hContact);
	}
	return bShowAvatar;
}

/////////////////////////////////////////////////////////////////////////////////////////
// checks, if there is a valid smileypack installed for the given protocol

int TSAPI CheckValidSmileyPack(const char *szProto, MCONTACT hContact)
{
	if (!PluginConfig.g_SmileyAddAvail)
		return 0;

	SMADD_INFO2 smainfo = { 0 };
	smainfo.cbSize = sizeof(smainfo);
	smainfo.Protocolname = const_cast<char *>(szProto);
	smainfo.hContact = hContact;
	CallService(MS_SMILEYADD_GETINFO2, 0, (LPARAM)&smainfo);
	if (smainfo.ButtonIcon)
		DestroyIcon(smainfo.ButtonIcon);
	return smainfo.NumberOfVisibleSmileys;
}

/////////////////////////////////////////////////////////////////////////////////////////
// return value MUST be mir_free()'d by caller.

wchar_t* TSAPI QuoteText(const wchar_t *text)
{
	int outChar, lineChar;
	int iCharsPerLine = M.GetDword("quoteLineLength", 64);

	size_t bufSize = mir_wstrlen(text) + 23;
	wchar_t *strout = (wchar_t*)mir_alloc(bufSize * sizeof(wchar_t));
	int inChar = 0;
	int justDoneLineBreak = 1;
	for (outChar = 0, lineChar = 0; text[inChar];) {
		if (outChar >= (int)bufSize - 8) {
			bufSize += 20;
			strout = (wchar_t*)mir_realloc(strout, bufSize * sizeof(wchar_t));
		}
		if (justDoneLineBreak && text[inChar] != '\r' && text[inChar] != '\n') {
			strout[outChar++] = '>';
			strout[outChar++] = ' ';
			lineChar = 2;
		}
		if (lineChar == iCharsPerLine && text[inChar] != '\r' && text[inChar] != '\n') {
			int decreasedBy;
			for (decreasedBy = 0; lineChar > 10; lineChar--, inChar--, outChar--, decreasedBy++)
				if (strout[outChar] == ' ' || strout[outChar] == '\t' || strout[outChar] == '-') break;
			if (lineChar <= 10) {
				lineChar += decreasedBy;
				inChar += decreasedBy;
				outChar += decreasedBy;
			}
			else inChar++;
			strout[outChar++] = '\r';
			strout[outChar++] = '\n';
			justDoneLineBreak = 1;
			continue;
		}
		strout[outChar++] = text[inChar];
		lineChar++;
		if (text[inChar] == '\n' || text[inChar] == '\r') {
			if (text[inChar] == '\r' && text[inChar + 1] != '\n')
				strout[outChar++] = '\n';
			justDoneLineBreak = 1;
			lineChar = 0;
		}
		else justDoneLineBreak = 0;
		inChar++;
	}
	strout[outChar++] = '\r';
	strout[outChar++] = '\n';
	strout[outChar] = 0;
	return strout;
}

void CTabBaseDlg::AdjustBottomAvatarDisplay()
{
	GetAvatarVisibility();

	bool bInfoPanel = Panel->isActive();
	HBITMAP hbm = (bInfoPanel && pContainer->avatarMode != 3) ? hOwnPic : (ace ? ace->hbmPic : PluginConfig.g_hbmUnknown);
	if (hbm) {
		if (dynaSplitter == 0 || splitterY == 0)
			LoadSplitter();
		dynaSplitter = splitterY - DPISCALEY_S(34);
		DM_RecalcPictureSize();
		Utils::showDlgControl(m_hwnd, IDC_CONTACTPIC, bShowAvatar ? SW_SHOW : SW_HIDE);
		InvalidateRect(GetDlgItem(m_hwnd, IDC_CONTACTPIC), NULL, TRUE);
	}
	else {
		Utils::showDlgControl(m_hwnd, IDC_CONTACTPIC, bShowAvatar ? SW_SHOW : SW_HIDE);
		pic.cy = pic.cx = DPISCALEY_S(60);
		InvalidateRect(GetDlgItem(m_hwnd, IDC_CONTACTPIC), NULL, TRUE);
	}
}

void CTabBaseDlg::ShowPicture(bool showNewPic)
{
	if (!Panel->isActive())
		pic.cy = pic.cx = DPISCALEY_S(60);

	if (showNewPic) {
		if (Panel->isActive() && pContainer->avatarMode != 3) {
			if (!hwndPanelPic) {
				InvalidateRect(m_hwnd, NULL, TRUE);
				UpdateWindow(m_hwnd);
				SendMessage(m_hwnd, WM_SIZE, 0, 0);
			}
			return;
		}
		AdjustBottomAvatarDisplay();
	}
	else {
		bShowAvatar = !bShowAvatar;
		db_set_b(m_hContact, SRMSGMOD_T, "MOD_ShowPic", bShowAvatar);
	}

	RECT rc;
	GetWindowRect(GetDlgItem(m_hwnd, IDC_CONTACTPIC), &rc);
	if (minEditBoxSize.cy + DPISCALEY_S(3) > splitterY)
		SendMessage(m_hwnd, DM_SPLITTERMOVED, (WPARAM)rc.bottom - minEditBoxSize.cy, (LPARAM)GetDlgItem(m_hwnd, IDC_SPLITTER));
	if (!showNewPic)
		SetDialogToType(m_hwnd);
	else
		SendMessage(m_hwnd, WM_SIZE, 0, 0);
}

void CTabBaseDlg::FlashOnClist(MEVENT hEvent, DBEVENTINFO *dbei)
{
	dwTickLastEvent = GetTickCount();

	if ((GetForegroundWindow() != pContainer->hwnd || pContainer->hwndActive != m_hwnd) && !(dbei->flags & DBEF_SENT) && dbei->eventType == EVENTTYPE_MESSAGE) {
		dwUnread++;
		UpdateTrayMenu(this, (WORD)(cache->getActiveStatus()), cache->getActiveProto(), szStatus, m_hContact, 0);
		if (nen_options.bTraySupport)
			return;
	}
	if (hEvent == 0)
		return;

	if (!PluginConfig.m_bFlashOnClist)
		return;

	if ((GetForegroundWindow() != pContainer->hwnd || pContainer->hwndActive != m_hwnd) && !(dbei->flags & DBEF_SENT) && dbei->eventType == EVENTTYPE_MESSAGE && !(dwFlagsEx & MWF_SHOW_FLASHCLIST)) {
		CLISTEVENT cle = {};
		cle.hContact = m_hContact;
		cle.hDbEvent = hEvent;
		cle.hIcon = Skin_LoadIcon(SKINICON_EVENT_MESSAGE);
		cle.pszService = "SRMsg/ReadMessage";
		pcli->pfnAddEvent(&cle);

		dwFlagsEx |= MWF_SHOW_FLASHCLIST;
		hFlashingEvent = hEvent;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// retrieve contents of the richedit control by streaming.Used to get the
// typed message before sending it.
// caller must mir_free the returned pointer.
// UNICODE version returns UTF-8 encoded string.

static DWORD CALLBACK Message_StreamCallback(DWORD_PTR dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb)
{
	static DWORD dwRead;
	char **ppText = (char **)dwCookie;

	if (*ppText == NULL) {
		*ppText = (char *)mir_alloc(cb + 2);
		memcpy(*ppText, pbBuff, cb);
		*pcb = cb;
		dwRead = cb;
		*(*ppText + cb) = '\0';
	}
	else {
		char *p = (char *)mir_realloc(*ppText, dwRead + cb + 2);
		memcpy(p + dwRead, pbBuff, cb);
		*ppText = p;
		*pcb = cb;
		dwRead += cb;
		*(*ppText + dwRead) = '\0';
	}
	return 0;
}

char* TSAPI Message_GetFromStream(HWND hwndRtf, DWORD dwPassedFlags)
{
	if (hwndRtf == 0)
		return NULL;

	DWORD dwFlags = (CP_UTF8 << 16) | SF_USECODEPAGE;
	if (dwPassedFlags == 0)
		dwFlags |= (SF_RTFNOOBJS | SFF_PLAINRTF);
	else
		dwFlags |= dwPassedFlags;

	char *pszText = NULL;
	EDITSTREAM stream = { 0 };
	stream.pfnCallback = Message_StreamCallback;
	stream.dwCookie = (DWORD_PTR)&pszText; // pass pointer to pointer
	SendMessage(hwndRtf, EM_STREAMOUT, dwFlags, (LPARAM)&stream);
	return pszText; // pszText contains the text
}

static wchar_t tszRtfBreaks[] = L" \\\n\r";

static void CreateColorMap(CMStringW &Text, int iCount, COLORREF *pSrc, int *pDst)
{
	const wchar_t *pszText = Text;
	int iIndex = 1;

	static const wchar_t *lpszFmt = L"\\red%[^ \x5b\\]\\green%[^ \x5b\\]\\blue%[^ \x5b;];";
	wchar_t szRed[10], szGreen[10], szBlue[10];

	const wchar_t *p1 = wcsstr(pszText, L"\\colortbl");
	if (!p1)
		return;

	const wchar_t *pEnd = wcschr(p1, '}');

	const wchar_t *p2 = wcsstr(p1, L"\\red");

	for (int i = 0; i < iCount; i++)
		pDst[i] = -1;

	while (p2 && p2 < pEnd) {
		if (swscanf(p2, lpszFmt, &szRed, &szGreen, &szBlue) > 0) {
			for (int i = 0; i < iCount; i++) {
				if (pSrc[i] == RGB(_wtoi(szRed), _wtoi(szGreen), _wtoi(szBlue)))
					pDst[i] = iIndex;
			}
		}
		iIndex++;
		p1 = p2;
		p1++;

		p2 = wcsstr(p1, L"\\red");
	}
}

static int GetRtfIndex(int iCol, int iCount, int *pIndex)
{
	for (int i = 0; i < iCount; i++)
		if (pIndex[i] == iCol)
			return i;

	return -1;
}

/////////////////////////////////////////////////////////////////////////////////////////
// convert rich edit code to bbcode (if wanted). Otherwise, strip all RTF formatting
// tags and return plain text

BOOL CTabBaseDlg::DoRtfToTags(CMStringW &pszText, int iNumColors, COLORREF *pColors) const
{
	if (pszText.IsEmpty())
		return FALSE;

	// used to filter out attributes which are already set for the default message input area font
	LOGFONTA lf = pContainer->theme.logFonts[MSGFONTID_MESSAGEAREA];

	// create an index of colors in the module and map them to
	// corresponding colors in the RTF color table
	int *pIndex = (int*)_alloca(iNumColors * sizeof(int));
	CreateColorMap(pszText, iNumColors, pColors, pIndex);

	// scan the file for rtf commands and remove or parse them
	int idx = pszText.Find(L"\\pard");
	if (idx == -1) {
		if ((idx = pszText.Find(L"\\ltrpar")) == -1)
			return FALSE;
		idx += 7;
	}
	else idx += 5;

	bool bInsideColor = false, bInsideUl = false;
	CMStringW res;

	// iterate through all characters, if rtf control character found then take action
	for (const wchar_t *p = pszText.GetString() + idx; *p;) {
		switch (*p) {
		case '\\':
			if (p[1] == '\\' || p[1] == '{' || p[1] == '}') { // escaped characters
				res.AppendChar(p[1]);
				p += 2; break;
			}
			if (p[1] == '~') { // non-breaking space
				res.AppendChar(0xA0);
				p += 2; break;
			}

			if (!wcsncmp(p, L"\\cf", 3)) { // foreground color
				int iCol = _wtoi(p + 3);
				int iInd = GetRtfIndex(iCol, iNumColors, pIndex);

				if (iCol && bType != SESSIONTYPE_CHAT)
					res.AppendFormat((iInd > 0) ? (bInsideColor ? L"[/color][color=%s]" : L"[color=%s]") : (bInsideColor ? L"[/color]" : L""), Utils::rtf_ctable[iInd - 1].szName);

				bInsideColor = iInd > 0;
			}
			else if (!wcsncmp(p, L"\\highlight", 10)) { //background color
				wchar_t szTemp[20];
				int iCol = _wtoi(p + 10);
				mir_snwprintf(szTemp, L"%d", iCol);
			}
			else if (!wcsncmp(p, L"\\line", 5)) { // soft line break;
				res.AppendChar('\n');
			}
			else if (!wcsncmp(p, L"\\endash", 7)) {
				res.AppendChar(0x2013);
			}
			else if (!wcsncmp(p, L"\\emdash", 7)) {
				res.AppendChar(0x2014);
			}
			else if (!wcsncmp(p, L"\\bullet", 7)) {
				res.AppendChar(0x2022);
			}
			else if (!wcsncmp(p, L"\\ldblquote", 10)) {
				res.AppendChar(0x201C);
			}
			else if (!wcsncmp(p, L"\\rdblquote", 10)) {
				res.AppendChar(0x201D);
			}
			else if (!wcsncmp(p, L"\\lquote", 7)) {
				res.AppendChar(0x2018);
			}
			else if (!wcsncmp(p, L"\\rquote", 7)) {
				res.AppendChar(0x2019);
			}
			else if (!wcsncmp(p, L"\\b", 2)) { //bold
				if (!(lf.lfWeight == FW_BOLD)) // only allow bold if the font itself isn't a bold one, otherwise just strip it..
					if (SendFormat)
						res.Append((p[2] != '0') ? L"[b]" : L"[/b]");
			}
			else if (!wcsncmp(p, L"\\i", 2)) { // italics
				if (!lf.lfItalic && SendFormat)
					res.Append((p[2] != '0') ? L"[i]" : L"[/i]");
			}
			else if (!wcsncmp(p, L"\\strike", 7)) { // strike-out
				if (!lf.lfStrikeOut && SendFormat)
					res.Append((p[7] != '0') ? L"[s]" : L"[/s]");
			}
			else if (!wcsncmp(p, L"\\ul", 3)) { // underlined
				if (!lf.lfUnderline && SendFormat) {
					if (p[3] == 0 || wcschr(tszRtfBreaks, p[3])) {
						res.Append(L"[u]");
						bInsideUl = true;
					}
					else if (!wcsncmp(p + 3, L"none", 4)) {
						if (bInsideUl)
							res.Append(L"[/u]");
						bInsideUl = false;
					}
				}
			}
			else if (!wcsncmp(p, L"\\tab", 4)) { // tab
				res.AppendChar('\t');
			}
			else if (p[1] == '\'') { // special character
				if (p[2] != ' ' && p[2] != '\\') {
					wchar_t tmp[10];

					if (p[3] != ' ' && p[3] != '\\') {
						wcsncpy(tmp, p + 2, 3);
						tmp[3] = 0;
					}
					else {
						wcsncpy(tmp, p + 2, 2);
						tmp[2] = 0;
					}

					// convert string containing char in hex format to int.
					wchar_t *stoppedHere;
					res.AppendChar(wcstol(tmp, &stoppedHere, 16));
				}
			}

			p++; // skip initial slash
			p += wcscspn(p, tszRtfBreaks);
			if (*p == ' ')
				p++;
			break;

		case '{': // other RTF control characters
		case '}':
			p++;
			break;

		default: // other text that should not be touched
			res.AppendChar(*p++);
			break;
		}
	}

	if (bInsideColor && bType != SESSIONTYPE_CHAT)
		res.Append(L"[/color]");
	if (bInsideUl)
		res.Append(L"[/u]");

	pszText = res;
	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////
// retrieve both buddys and my own UIN for a message session and store them in the message window *dat
// respects metacontacts and uses the current protocol if the contact is a MC

void CTabBaseDlg::GetMYUIN()
{
	ptrW uid(Contact_GetInfo(CNF_DISPLAYUID, NULL, cache->getActiveProto()));
	if (uid != NULL)
		wcsncpy_s(myUin, uid, _TRUNCATE);
	else
		myUin[0] = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

static int g_IEViewAvail = -1;
static int g_HPPAvail = -1;

UINT TSAPI GetIEViewMode(MCONTACT hContact)
{
	int iWantIEView = 0, iWantHPP = 0;

	if (g_IEViewAvail == -1)
		g_IEViewAvail = ServiceExists(MS_IEVIEW_WINDOW);

	if (g_HPPAvail == -1)
		g_HPPAvail = ServiceExists("History++/ExtGrid/NewWindow");

	PluginConfig.g_WantIEView = g_IEViewAvail && M.GetByte("default_ieview", 0);
	PluginConfig.g_WantHPP = g_HPPAvail && M.GetByte("default_hpp", 0);

	iWantIEView = (PluginConfig.g_WantIEView) || (M.GetByte(hContact, "ieview", 0) == 1 && g_IEViewAvail);
	iWantIEView = (M.GetByte(hContact, "ieview", 0) == (BYTE)-1) ? 0 : iWantIEView;

	iWantHPP = (PluginConfig.g_WantHPP) || (M.GetByte(hContact, "hpplog", 0) == 1 && g_HPPAvail);
	iWantHPP = (M.GetByte(hContact, "hpplog", 0) == (BYTE)-1) ? 0 : iWantHPP;

	return iWantHPP ? WANT_HPP_LOG : (iWantIEView ? WANT_IEVIEW_LOG : 0);
}

/////////////////////////////////////////////////////////////////////////////////////////

void CTabBaseDlg::SetMessageLog()
{
	unsigned int iLogMode = GetIEViewMode(m_hContact);

	if (iLogMode == WANT_IEVIEW_LOG && hwndIEView == 0) {
		IEVIEWWINDOW ieWindow;

		memset(&ieWindow, 0, sizeof(ieWindow));
		ieWindow.cbSize = sizeof(IEVIEWWINDOW);
		ieWindow.iType = IEW_CREATE;
		ieWindow.dwFlags = 0;
		ieWindow.dwMode = IEWM_TABSRMM;
		ieWindow.parent = m_hwnd;
		ieWindow.x = 0;
		ieWindow.y = 0;
		ieWindow.cx = 200;
		ieWindow.cy = 200;
		CallService(MS_IEVIEW_WINDOW, 0, (LPARAM)&ieWindow);
		hwndIEView = ieWindow.hwnd;
		Utils::showDlgControl(m_hwnd, IDC_LOG, SW_HIDE);
		Utils::enableDlgControl(m_hwnd, IDC_LOG, false);
	}
	else if (iLogMode == WANT_HPP_LOG && hwndHPP == 0) {
		IEVIEWWINDOW ieWindow;
		ieWindow.cbSize = sizeof(IEVIEWWINDOW);
		ieWindow.iType = IEW_CREATE;
		ieWindow.dwFlags = 0;
		ieWindow.dwMode = IEWM_TABSRMM;
		ieWindow.parent = m_hwnd;
		ieWindow.x = 0;
		ieWindow.y = 0;
		ieWindow.cx = 10;
		ieWindow.cy = 10;
		CallService(MS_HPP_EG_WINDOW, 0, (LPARAM)&ieWindow);
		hwndHPP = ieWindow.hwnd;
		Utils::showDlgControl(m_hwnd, IDC_LOG, SW_HIDE);
		Utils::enableDlgControl(m_hwnd, IDC_LOG, false);
	}
}

void CTabBaseDlg::FindFirstEvent()
{
	int historyMode = db_get_b(m_hContact, SRMSGMOD, SRMSGSET_LOADHISTORY, -1);
	if (historyMode == -1)
		historyMode = (int)M.GetByte(SRMSGMOD, SRMSGSET_LOADHISTORY, SRMSGDEFSET_LOADHISTORY);

	hDbEventFirst = db_event_firstUnread(m_hContact);

	if (bActualHistory)
		historyMode = LOADHISTORY_COUNT;

	switch (historyMode) {
	case LOADHISTORY_COUNT:
		int i;
		MEVENT hPrevEvent;
		{
			DBEVENTINFO dbei = {};
			// ability to load only current session's history
			if (bActualHistory)
				i = cache->getSessionMsgCount();
			else
				i = db_get_w(NULL, SRMSGMOD, SRMSGSET_LOADCOUNT, SRMSGDEFSET_LOADCOUNT);

			for (; i > 0; i--) {
				if (hDbEventFirst == NULL)
					hPrevEvent = db_event_last(m_hContact);
				else
					hPrevEvent = db_event_prev(m_hContact, hDbEventFirst);
				if (hPrevEvent == NULL)
					break;
				dbei.cbBlob = 0;
				hDbEventFirst = hPrevEvent;
				db_event_get(hDbEventFirst, &dbei);
				if (!DbEventIsShown(&dbei))
					i++;
			}
		}
		break;

	case LOADHISTORY_TIME:
		DBEVENTINFO dbei = {};
		if (hDbEventFirst == NULL)
			dbei.timestamp = time(NULL);
		else
			db_event_get(hDbEventFirst, &dbei);

		DWORD firstTime = dbei.timestamp - 60 * db_get_w(NULL, SRMSGMOD, SRMSGSET_LOADTIME, SRMSGDEFSET_LOADTIME);
		for (;;) {
			if (hDbEventFirst == NULL)
				hPrevEvent = db_event_last(m_hContact);
			else
				hPrevEvent = db_event_prev(m_hContact, hDbEventFirst);
			if (hPrevEvent == NULL)
				break;
			dbei.cbBlob = 0;
			db_event_get(hPrevEvent, &dbei);
			if (dbei.timestamp < firstTime)
				break;
			hDbEventFirst = hPrevEvent;
		}
		break;
	}
}

void CTabBaseDlg::SaveSplitter()
{
	// group chats save their normal splitter position independently
	if (bType == SESSIONTYPE_CHAT || bIsAutosizingInput)
		return;

	if (splitterY < DPISCALEY_S(MINSPLITTERY) || splitterY < 0)
		splitterY = DPISCALEY_S(MINSPLITTERY);

	if (dwFlagsEx & MWF_SHOW_SPLITTEROVERRIDE)
		db_set_dw(m_hContact, SRMSGMOD_T, "splitsplity", splitterY);
	else {
		if (pContainer->settings->fPrivate)
			pContainer->settings->splitterPos = splitterY;
		else
			db_set_dw(0, SRMSGMOD_T, "splitsplity", splitterY);
	}
}

void CTabBaseDlg::LoadSplitter()
{
	if (bIsAutosizingInput) {
		splitterY = GetDefaultMinimumInputHeight();
		return;
	}

	if (!(dwFlagsEx & MWF_SHOW_SPLITTEROVERRIDE)) {
		if (!pContainer->settings->fPrivate)
			splitterY = (int)M.GetDword("splitsplity", (DWORD)60);
		else
			splitterY = pContainer->settings->splitterPos;
	}
	else splitterY = (int)M.GetDword(m_hContact, "splitsplity", M.GetDword("splitsplity", (DWORD)70));

	if (splitterY < MINSPLITTERY || splitterY < 0)
		splitterY = 150;
}

void CTabBaseDlg::PlayIncomingSound() const
{
	int iPlay = Utils::mustPlaySound(this);

	if (iPlay) {
		if (GetForegroundWindow() == pContainer->hwnd && pContainer->hwndActive == m_hwnd)
			SkinPlaySound("RecvMsgActive");
		else
			SkinPlaySound("RecvMsgInactive");
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// reads send format and configures the toolbar buttons
// if mode == 0, int only configures the buttons and does not change send format

static UINT controls[] = { IDC_FONTBOLD, IDC_FONTITALIC, IDC_FONTUNDERLINE, IDC_FONTSTRIKEOUT };

void CTabBaseDlg::GetSendFormat()
{
	SendFormat = M.GetDword(m_hContact, "sendformat", PluginConfig.m_SendFormat);
	if (SendFormat == -1)          // per contact override to disable it..
		SendFormat = 0;
	else if (SendFormat == 0)
		SendFormat = PluginConfig.m_SendFormat ? 1 : 0;

	for (int i = 0; i < _countof(controls); i++)
		Utils::enableDlgControl(m_hwnd, controls[i], SendFormat != 0);
	return;
}

/////////////////////////////////////////////////////////////////////////////////////////
// get user - readable locale information for the currently selected
// keyboard layout.
//
// GetLocaleInfo() should no longer be used on Vista and later

void CTabBaseDlg::GetLocaleID(const wchar_t *szKLName)
{
	wchar_t szLI[256], *stopped = NULL;
	USHORT langID;
	WORD   wCtype2[3];
	PARAFORMAT2 pf2;
	BOOL fLocaleNotSet;
	BYTE szTest[4] = { 0xe4, 0xf6, 0xfc, 0 };

	szLI[0] = szLI[1] = 0;

	memset(&pf2, 0, sizeof(PARAFORMAT2));
	langID = (USHORT)wcstol(szKLName, &stopped, 16);
	lcid = MAKELCID(langID, 0);
	/*
	 * Vista+: read ISO locale names from the registry
	 */
	if (PluginConfig.m_bIsVista) {
		HKEY	hKey = 0;
		wchar_t	szKey[20];
		DWORD	dwLID = wcstoul(szKLName, &stopped, 16);

		mir_snwprintf(szKey, L"%04.04x", LOWORD(dwLID));
		if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_CLASSES_ROOT, L"MIME\\Database\\Rfc1766", 0, KEY_READ, &hKey)) {
			DWORD dwLength = 255;
			if (ERROR_SUCCESS == RegQueryValueEx(hKey, szKey, 0, 0, (unsigned char *)szLI, &dwLength)) {
				wchar_t*	p;

				szLI[255] = 0;
				if ((p = wcschr(szLI, ';')) != 0)
					*p = 0;
			}
			RegCloseKey(hKey);
		}
		szLI[0] = towupper(szLI[0]);
		szLI[1] = towupper(szLI[1]);
	}
	else {
		GetLocaleInfo(lcid, LOCALE_SISO639LANGNAME, szLI, 10);
		wcsupr(szLI);
	}
	fLocaleNotSet = (lcID[0] == 0 && lcID[1] == 0);
	mir_snwprintf(lcID, szLI);
	GetStringTypeA(lcid, CT_CTYPE2, (char*)szTest, 3, wCtype2);
	pf2.cbSize = sizeof(pf2);
	pf2.dwMask = PFM_RTLPARA;
	m_message.SendMsg(EM_GETPARAFORMAT, 0, (LPARAM)&pf2);
	if (Utils::FindRTLLocale(this) && fLocaleNotSet) {
		if (wCtype2[0] == C2_RIGHTTOLEFT || wCtype2[1] == C2_RIGHTTOLEFT || wCtype2[2] == C2_RIGHTTOLEFT) {
			memset(&pf2, 0, sizeof(pf2));
			pf2.dwMask = PFM_RTLPARA;
			pf2.cbSize = sizeof(pf2);
			pf2.wEffects = PFE_RTLPARA;
			m_message.SendMsg(EM_SETPARAFORMAT, 0, (LPARAM)&pf2);
		}
		else {
			memset(&pf2, 0, sizeof(pf2));
			pf2.dwMask = PFM_RTLPARA;
			pf2.cbSize = sizeof(pf2);
			pf2.wEffects = 0;
			m_message.SendMsg(EM_SETPARAFORMAT, 0, (LPARAM)&pf2);
		}
		m_message.SendMsg(EM_SETLANGOPTIONS, 0, m_message.SendMsg(EM_GETLANGOPTIONS, 0, 0) & ~IMF_AUTOKEYBOARD);
	}
}

void CTabBaseDlg::LoadContactAvatar()
{
	ace = Utils::loadAvatarFromAVS(bIsMeta ? db_mc_getSrmmSub(m_hContact) : m_hContact);

	BITMAP bm;
	if (ace && ace->hbmPic)
		GetObject(ace->hbmPic, sizeof(bm), &bm);
	else if (ace == NULL)
		GetObject(PluginConfig.g_hbmUnknown, sizeof(bm), &bm);
	else
		return;

	AdjustBottomAvatarDisplay();
	CalcDynamicAvatarSize(&bm);

	if (!Panel->isActive() || pContainer->avatarMode == 3) {
		iRealAvatarHeight = 0;
		PostMessage(m_hwnd, WM_SIZE, 0, 0);
	}
	else if (Panel->isActive())
		GetAvatarVisibility();
}

void CTabBaseDlg::LoadOwnAvatar()
{
	if (ServiceExists(MS_AV_GETMYAVATAR))
		ownAce = (AVATARCACHEENTRY *)CallService(MS_AV_GETMYAVATAR, 0, (LPARAM)(cache->getActiveProto()));
	else
		ownAce = nullptr;

	if (ownAce)
		hOwnPic = ownAce->hbmPic;
	else
		hOwnPic = PluginConfig.g_hbmUnknown;

	if (Panel->isActive() && pContainer->avatarMode != 3) {
		BITMAP bm;

		iRealAvatarHeight = 0;
		AdjustBottomAvatarDisplay();
		GetObject(hOwnPic, sizeof(bm), &bm);
		CalcDynamicAvatarSize(&bm);
		SendMessage(m_hwnd, WM_SIZE, 0, 0);
	}
}

// paste contents of the clipboard into the message input area and send it immediately
void CTabBaseDlg::HandlePasteAndSend()
{
	// is feature disabled?
	if (!PluginConfig.m_PasteAndSend) {
		SendMessage(m_hwnd, DM_ACTIVATETOOLTIP, IDC_MESSAGE, (LPARAM)TranslateT("The 'paste and send' feature is disabled. You can enable it on the 'General' options page in the 'Sending messages' section"));
		return;
	}

	m_message.SendMsg(EM_PASTESPECIAL, CF_UNICODETEXT, 0);
	if (GetWindowTextLength(m_message.GetHwnd()) > 0)
		SendMessage(m_hwnd, WM_COMMAND, IDOK, 0);
}

/////////////////////////////////////////////////////////////////////////////////////////
// draw various elements of the message window, like avatar(s), info panel fields
// and the color formatting menu

int CTabBaseDlg::MsgWindowDrawHandler(WPARAM, LPARAM lParam)
{
	LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
	if (dis->CtlType == ODT_MENU && dis->hwndItem == (HWND)GetSubMenu(PluginConfig.g_hMenuContext, 7)) {
		RECT rc = { 0 };
		HBRUSH old, col;
		COLORREF clr;
		switch (dis->itemID) {
		case ID_FONT_RED:
			clr = RGB(255, 0, 0);
			break;
		case ID_FONT_BLUE:
			clr = RGB(0, 0, 255);
			break;
		case ID_FONT_GREEN:
			clr = RGB(0, 255, 0);
			break;
		case ID_FONT_MAGENTA:
			clr = RGB(255, 0, 255);
			break;
		case ID_FONT_YELLOW:
			clr = RGB(255, 255, 0);
			break;
		case ID_FONT_WHITE:
			clr = RGB(255, 255, 255);
			break;
		case ID_FONT_DEFAULTCOLOR:
			clr = GetSysColor(COLOR_MENU);
			break;
		case ID_FONT_CYAN:
			clr = RGB(0, 255, 255);
			break;
		case ID_FONT_BLACK:
			clr = RGB(0, 0, 0);
			break;
		default:
			clr = 0;
		}
		col = (HBRUSH)CreateSolidBrush(clr);
		old = (HBRUSH)SelectObject(dis->hDC, col);
		rc.left = 2;
		rc.top = dis->rcItem.top - 5;
		rc.right = 15;
		rc.bottom = dis->rcItem.bottom + 4;
		Rectangle(dis->hDC, rc.left - 1, rc.top - 1, rc.right + 1, rc.bottom + 1);
		FillRect(dis->hDC, &rc, col);
		SelectObject(dis->hDC, old);
		DeleteObject(col);
		return TRUE;
	}

	HBITMAP hbmAvatar = ace ? ace->hbmPic : PluginConfig.g_hbmUnknown;
	if ((dis->hwndItem == GetDlgItem(m_hwnd, IDC_CONTACTPIC) && bShowAvatar) || (dis->hwndItem == m_hwnd && Panel->isActive())) {
		if (hbmAvatar == NULL)
			return TRUE;

		int top, cx, cy;
		RECT rcClient, rcFrame;
		bool bPanelPic = (dis->hwndItem == m_hwnd);
		if (bPanelPic && !bShowInfoAvatar)
			return TRUE;

		RECT rc;
		GetClientRect(m_hwnd, &rc);
		if (bPanelPic) {
			rcClient = dis->rcItem;
			cx = (rcClient.right - rcClient.left);
			cy = (rcClient.bottom - rcClient.top) + 1;
		}
		else {
			GetClientRect(dis->hwndItem, &rcClient);
			cx = rcClient.right;
			cy = rcClient.bottom;
		}

		if (cx < 5 || cy < 5)
			return TRUE;

		HDC hdcDraw = CreateCompatibleDC(dis->hDC);
		HBITMAP hbmDraw = CreateCompatibleBitmap(dis->hDC, cx, cy);
		HBITMAP hbmOld = (HBITMAP)SelectObject(hdcDraw, hbmDraw);

		bool bAero = M.isAero();

		HRGN clipRgn = 0;
		HBRUSH hOldBrush = (HBRUSH)SelectObject(hdcDraw, bAero ? (HBRUSH)GetStockObject(HOLLOW_BRUSH) : GetSysColorBrush(COLOR_3DFACE));
		rcFrame = rcClient;

		if (!bPanelPic) {
			top = (cy - pic.cy) / 2;
			RECT rcEdge = { 0, top, pic.cx, top + pic.cy };
			if (CSkin::m_skinEnabled)
				CSkin::SkinDrawBG(dis->hwndItem, pContainer->hwnd, pContainer, &dis->rcItem, hdcDraw);
			else if (PluginConfig.m_fillColor) {
				HBRUSH br = CreateSolidBrush(PluginConfig.m_fillColor);
				FillRect(hdcDraw, &rcFrame, br);
				DeleteObject(br);
			}
			else if (bAero && CSkin::m_pCurrentAeroEffect) {
				COLORREF clr = PluginConfig.m_tbBackgroundHigh ? PluginConfig.m_tbBackgroundHigh :
					(CSkin::m_pCurrentAeroEffect ? CSkin::m_pCurrentAeroEffect->m_clrToolbar : 0xf0f0f0);

				HBRUSH br = CreateSolidBrush(clr);
				FillRect(hdcDraw, &rcFrame, br);
				DeleteObject(br);
			}
			else FillRect(hdcDraw, &rcFrame, GetSysColorBrush(COLOR_3DFACE));

			HPEN hPenBorder = CreatePen(PS_SOLID, 1, CSkin::m_avatarBorderClr);
			HPEN hPenOld = (HPEN)SelectObject(hdcDraw, hPenBorder);

			if (CSkin::m_bAvatarBorderType == 1)
				Rectangle(hdcDraw, rcEdge.left, rcEdge.top, rcEdge.right, rcEdge.bottom);
			else if (CSkin::m_bAvatarBorderType == 2) {
				int iRad = PluginConfig.m_WinVerMajor >= 5 ? 4 : 6;
				clipRgn = CreateRoundRectRgn(rcEdge.left, rcEdge.top, rcEdge.right + 1, rcEdge.bottom + 1, iRad, iRad);
				SelectClipRgn(hdcDraw, clipRgn);

				HBRUSH hbr = CreateSolidBrush(CSkin::m_avatarBorderClr);
				FrameRgn(hdcDraw, clipRgn, hbr, 1, 1);
				DeleteObject(hbr);
				DeleteObject(clipRgn);
			}

			SelectObject(hdcDraw, hPenOld);
			DeleteObject(hPenBorder);
		}

		if (bPanelPic) {
			bool bBorder = (CSkin::m_bAvatarBorderType ? true : false);

			int border_off = bBorder ? 1 : 0;
			int iMaxHeight = iPanelAvatarY - (bBorder ? 2 : 0);
			int iMaxWidth = iPanelAvatarX - (bBorder ? 2 : 0);

			rcFrame.left = rcFrame.top = 0;
			rcFrame.right = (rcClient.right - rcClient.left);
			rcFrame.bottom = (rcClient.bottom - rcClient.top);

			rcFrame.left = rcFrame.right - (LONG)iPanelAvatarX;
			rcFrame.bottom = (LONG)iPanelAvatarY;

			int height_off = (cy - iMaxHeight - (bBorder ? 2 : 0)) / 2;
			rcFrame.top += height_off;
			rcFrame.bottom += height_off;

			SendMessage(hwndPanelPic, AVATAR_SETAEROCOMPATDRAWING, 0, bAero ? TRUE : FALSE);
			SetWindowPos(hwndPanelPic, HWND_TOP, rcFrame.left + border_off, rcFrame.top + border_off,
				iMaxWidth, iMaxHeight, SWP_SHOWWINDOW | SWP_ASYNCWINDOWPOS | SWP_DEFERERASE | SWP_NOSENDCHANGING);
		}

		SelectObject(hdcDraw, hOldBrush);
		if (!bPanelPic)
			BitBlt(dis->hDC, 0, 0, cx, cy, hdcDraw, 0, 0, SRCCOPY);
		SelectObject(hdcDraw, hbmOld);
		DeleteObject(hbmDraw);
		DeleteDC(hdcDraw);
		return TRUE;
	}

	if (dis->hwndItem == GetDlgItem(m_hwnd, IDC_STATICTEXT) || dis->hwndItem == GetDlgItem(m_hwnd, IDC_LOGFROZENTEXT)) {
		wchar_t szWindowText[256];
		if (CSkin::m_skinEnabled) {
			SetTextColor(dis->hDC, CSkin::m_DefaultFontColor);
			CSkin::SkinDrawBG(dis->hwndItem, pContainer->hwnd, pContainer, &dis->rcItem, dis->hDC);
		}
		else {
			SetTextColor(dis->hDC, GetSysColor(COLOR_BTNTEXT));
			CSkin::FillBack(dis->hDC, &dis->rcItem);
		}
		GetWindowText(dis->hwndItem, szWindowText, _countof(szWindowText));
		szWindowText[255] = 0;
		SetBkMode(dis->hDC, TRANSPARENT);
		DrawText(dis->hDC, szWindowText, -1, &dis->rcItem, DT_SINGLELINE | DT_VCENTER | DT_NOCLIP | DT_END_ELLIPSIS);
		return TRUE;
	}

	if (dis->hwndItem == GetDlgItem(m_hwnd, IDC_STATICERRORICON)) {
		if (CSkin::m_skinEnabled)
			CSkin::SkinDrawBG(dis->hwndItem, pContainer->hwnd, pContainer, &dis->rcItem, dis->hDC);
		else
			CSkin::FillBack(dis->hDC, &dis->rcItem);
		DrawIconEx(dis->hDC, (dis->rcItem.right - dis->rcItem.left) / 2 - 8, (dis->rcItem.bottom - dis->rcItem.top) / 2 - 8,
			PluginConfig.g_iconErr, 16, 16, 0, 0, DI_NORMAL);
		return TRUE;
	}

	if (dis->CtlType == ODT_MENU && Panel->isHovered()) {
		DrawMenuItem(dis, (HICON)dis->itemData, 0);
		return TRUE;
	}

	return Menu_DrawItem(lParam);
}

void TSAPI LoadThemeDefaults(TContainerData *pContainer)
{
	memset(&pContainer->theme, 0, sizeof(TLogTheme));
	pContainer->theme.bg = M.GetDword(FONTMODULE, SRMSGSET_BKGCOLOUR, GetSysColor(COLOR_WINDOW));
	pContainer->theme.statbg = PluginConfig.crStatus;
	pContainer->theme.oldinbg = PluginConfig.crOldIncoming;
	pContainer->theme.oldoutbg = PluginConfig.crOldOutgoing;
	pContainer->theme.inbg = PluginConfig.crIncoming;
	pContainer->theme.outbg = PluginConfig.crOutgoing;
	pContainer->theme.hgrid = M.GetDword(FONTMODULE, "hgrid", RGB(224, 224, 224));
	pContainer->theme.left_indent = M.GetDword("IndentAmount", 20) * 15;
	pContainer->theme.right_indent = M.GetDword("RightIndent", 20) * 15;
	pContainer->theme.inputbg = M.GetDword(FONTMODULE, "inputbg", SRMSGDEFSET_BKGCOLOUR);

	for (int i = 1; i <= 5; i++) {
		char szTemp[40];
		mir_snprintf(szTemp, "cc%d", i);
		COLORREF	colour = M.GetDword(szTemp, RGB(224, 224, 224));
		if (colour == 0)
			colour = RGB(1, 1, 1);
		pContainer->theme.custom_colors[i - 1] = colour;
	}
	pContainer->theme.logFonts = logfonts;
	pContainer->theme.fontColors = fontcolors;
	pContainer->theme.rtfFonts = NULL;
	pContainer->ltr_templates = &LTR_Active;
	pContainer->rtl_templates = &RTL_Active;
	pContainer->theme.dwFlags = (M.GetDword("mwflags", MWF_LOG_DEFAULT) & MWF_LOG_ALL);
	pContainer->theme.isPrivate = false;
}

void TSAPI LoadOverrideTheme(TContainerData *pContainer)
{
	memset(&pContainer->theme, 0, sizeof(TLogTheme));
	if (mir_wstrlen(pContainer->szAbsThemeFile) > 1) {
		if (PathFileExists(pContainer->szAbsThemeFile)) {
			if (CheckThemeVersion(pContainer->szAbsThemeFile) == 0) {
				LoadThemeDefaults(pContainer);
				return;
			}
			if (pContainer->ltr_templates == NULL) {
				pContainer->ltr_templates = (TTemplateSet *)mir_alloc(sizeof(TTemplateSet));
				memcpy(pContainer->ltr_templates, &LTR_Active, sizeof(TTemplateSet));
			}
			if (pContainer->rtl_templates == NULL) {
				pContainer->rtl_templates = (TTemplateSet *)mir_alloc(sizeof(TTemplateSet));
				memcpy(pContainer->rtl_templates, &RTL_Active, sizeof(TTemplateSet));
			}

			pContainer->theme.logFonts = (LOGFONTA *)mir_alloc(sizeof(LOGFONTA) * (MSGDLGFONTCOUNT + 2));
			pContainer->theme.fontColors = (COLORREF *)mir_alloc(sizeof(COLORREF) * (MSGDLGFONTCOUNT + 2));
			pContainer->theme.rtfFonts = (char *)mir_alloc((MSGDLGFONTCOUNT + 2) * RTFCACHELINESIZE);

			ReadThemeFromINI(pContainer->szAbsThemeFile, pContainer, 0, THEME_READ_ALL);
			pContainer->theme.left_indent *= 15;
			pContainer->theme.right_indent *= 15;
			pContainer->theme.isPrivate = true;
			if (CSkin::m_skinEnabled)
				pContainer->theme.bg = SkinItems[ID_EXTBKCONTAINER].COLOR;
			else
				pContainer->theme.bg = PluginConfig.m_fillColor ? PluginConfig.m_fillColor : GetSysColor(COLOR_WINDOW);
			return;
		}
	}
	LoadThemeDefaults(pContainer);
}

HICON CTabBaseDlg::GetXStatusIcon() const
{
	BYTE xStatus = cache->getXStatusId();
	if (xStatus == 0)
		return NULL;

	if (!ProtoServiceExists(cache->getActiveProto(), PS_GETCUSTOMSTATUSICON))
		return NULL;

	return (HICON)(CallProtoService(cache->getActiveProto(), PS_GETCUSTOMSTATUSICON, xStatus, 0));
}

LRESULT TSAPI GetSendButtonState(HWND hwnd)
{
	HWND hwndIDok = GetDlgItem(hwnd, IDOK);

	if (hwndIDok)
		return(SendMessage(hwndIDok, BUTTONGETSTATEID, TRUE, 0));
	else
		return 0;
}

void CTabBaseDlg::EnableSendButton(bool bMode) const
{
	SendDlgItemMessage(m_hwnd, IDOK, BUTTONSETASNORMAL, bMode, 0);
	SendDlgItemMessage(m_hwnd, IDC_PIC, BUTTONSETASNORMAL, fEditNotesActive ? TRUE : (!bMode && iOpenJobs == 0) ? TRUE : FALSE, 0);

	HWND hwndOK = GetDlgItem(GetParent(GetParent(m_hwnd)), IDOK);
	if (IsWindow(hwndOK))
		SendMessage(hwndOK, BUTTONSETASNORMAL, bMode, 0);
}

void CTabBaseDlg::SendNudge() const
{
	if (ProtoServiceExists(cache->getActiveProto(), PS_SEND_NUDGE) && ServiceExists(MS_NUDGE_SEND))
		CallService(MS_NUDGE_SEND, cache->getActiveContact(), 0);
	else
		SendMessage(m_hwnd, DM_ACTIVATETOOLTIP, IDC_MESSAGE,
			(LPARAM)TranslateT("Either the nudge plugin is not installed or the contact's protocol does not support sending a nudge event."));
}

void CTabBaseDlg::GetClientIcon()
{
	if (hClientIcon)
		DestroyIcon(hClientIcon);

	hClientIcon = nullptr;
	if (ServiceExists(MS_FP_GETCLIENTICONT)) {
		ptrW tszMirver(db_get_wsa(cache->getActiveContact(), cache->getActiveProto(), "MirVer"));
		if (tszMirver)
			hClientIcon = Finger_GetClientIcon(tszMirver, 1);
	}
}

void CTabBaseDlg::GetMyNick()
{
	ptrW tszNick(Contact_GetInfo(CNF_NICK, NULL, cache->getActiveProto()));
	if (tszNick != NULL) {
		if (mir_wstrlen(tszNick) == 0 || !mir_wstrcmp(tszNick, TranslateT("'(Unknown contact)'")))
			wcsncpy_s(szMyNickname, (myUin[0] ? myUin : TranslateT("'(Unknown contact)'")), _TRUNCATE);
		else
			wcsncpy_s(szMyNickname, tszNick, _TRUNCATE);
	}
	else wcsncpy_s(szMyNickname, L"<undef>", _TRUNCATE); // same here
}

HICON CTabBaseDlg::GetMyContactIcon(LPCSTR szSetting)
{
	int bUseMeta = (szSetting == NULL) ? false : M.GetByte(szSetting, mir_strcmp(szSetting, "MetaiconTab") == 0);
	if (bUseMeta)
		return Skin_LoadProtoIcon(cache->getProto(), cache->getStatus());
	return Skin_LoadProtoIcon(cache->getActiveProto(), cache->getActiveStatus());
}

/////////////////////////////////////////////////////////////////////////////////////////
// read keyboard state and return the state of the modifier keys

void CTabBaseDlg::KbdState(bool &isShift, bool &isControl, bool &isAlt)
{
	GetKeyboardState(kstate);
	isShift = (kstate[VK_SHIFT] & 0x80) != 0;
	isControl = (kstate[VK_CONTROL] & 0x80) != 0;
	isAlt = (kstate[VK_MENU] & 0x80) != 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// clear the message log
// code needs to distuingish between IM and MUC sessions.

void CTabBaseDlg::ClearLog()
{
	if (bType == SESSIONTYPE_IM) {
		if (hwndIEView || hwndHPP) {
			IEVIEWEVENT event;
			event.cbSize = sizeof(IEVIEWEVENT);
			event.iType = IEE_CLEAR_LOG;
			event.dwFlags = (dwFlags & MWF_LOG_RTL) ? IEEF_RTL : 0;
			event.hContact = m_hContact;
			if (hwndIEView) {
				event.hwnd = hwndIEView;
				CallService(MS_IEVIEW_EVENT, 0, (LPARAM)&event);
			}
			else {
				event.hwnd = hwndHPP;
				CallService(MS_HPP_EG_EVENT, 0, (LPARAM)&event);
			}
		}
		m_log.SetText(L"");
		hDbEventFirst = 0;
	}
	else if (bType == SESSIONTYPE_CHAT && si) {
		SESSION_INFO *s = pci->SM_FindSession(si->ptszID, si->pszModule);
		if (s) {
			m_log.SetText(L"");
			pci->LM_RemoveAll(&s->pLog, &s->pLogEnd);
			s->iEventCount = 0;
			s->LastTime = 0;
			si->iEventCount = 0;
			si->LastTime = 0;
			si->pLog = s->pLog;
			si->pLogEnd = s->pLogEnd;
			PostMessage(m_hwnd, WM_MOUSEACTIVATE, 0, 0);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// calculate the minimum required client height for the given message
// window layout
//
// the container will use this in its WM_GETMINMAXINFO handler to set
// minimum tracking height.

void CTabBaseDlg::DetermineMinHeight()
{
	RECT rc;
	LONG height = (Panel->isActive() ? Panel->getHeight() + 2 : 0);
	if (!(pContainer->dwFlags & CNT_HIDETOOLBAR))
		height += DPISCALEY_S(24); // toolbar
	GetClientRect(m_message.GetHwnd(), &rc);
	height += rc.bottom; // input area
	height += 40; // min space for log area and some padding

	pContainer->uChildMinHeight = height;
}

bool CTabBaseDlg::IsAutoSplitEnabled() const
{
	return (pContainer->dwFlags & CNT_AUTOSPLITTER) && !(dwFlagsEx & MWF_SHOW_SPLITTEROVERRIDE);
}

LONG CTabBaseDlg::GetDefaultMinimumInputHeight() const
{
	LONG height;

	if (bType == SESSIONTYPE_IM)
		height = ((pContainer->dwFlags & CNT_BOTTOMTOOLBAR) ? DPISCALEY_S(46 + 22) : DPISCALEY_S(46));
	else
		height = ((pContainer->dwFlags & CNT_BOTTOMTOOLBAR) ? DPISCALEY_S(22 + 46) : DPISCALEY_S(46)) - DPISCALEY_S(23);

	if (CSkin::m_skinEnabled && !SkinItems[ID_EXTBKINPUTAREA].IGNORED)
		height += (SkinItems[ID_EXTBKINPUTAREA].MARGIN_BOTTOM + SkinItems[ID_EXTBKINPUTAREA].MARGIN_TOP - 2);

	return height;
}

static LIST<wchar_t> vTempFilenames(5);

// send a pasted bitmap by file transfer.
void CTabBaseDlg::SendHBitmapAsFile(HBITMAP hbmp) const
{
	const wchar_t *mirandatempdir = L"Miranda";
	const wchar_t *filenametemplate = L"\\clp-%Y%m%d-%H%M%S0.jpg";
	wchar_t filename[MAX_PATH];
	size_t tempdirlen = GetTempPath(MAX_PATH, filename);
	bool fSend = true;

	const char *szProto = cache->getActiveProto();
	WORD  wMyStatus = (WORD)CallProtoService(szProto, PS_GETSTATUS, 0, 0);

	DWORD protoCaps = CallProtoService(szProto, PS_GETCAPS, PFLAGNUM_1, 0);
	DWORD typeCaps = CallProtoService(szProto, PS_GETCAPS, PFLAGNUM_4, 0);

	// check protocol capabilities, status modes and visibility lists (privacy)
	// to determine whether the file can be sent. Throw a warning if any of
	// these checks fails.
	if (!(protoCaps & PF1_FILESEND))
		fSend = false;

	if ((ID_STATUS_OFFLINE == wMyStatus) || (ID_STATUS_OFFLINE == cache->getActiveStatus() && !(typeCaps & PF4_OFFLINEFILES)))
		fSend = false;

	if (protoCaps & PF1_VISLIST && db_get_w(cache->getActiveContact(), szProto, "ApparentMode", 0) == ID_STATUS_OFFLINE)
		fSend = false;

	if (protoCaps & PF1_INVISLIST && wMyStatus == ID_STATUS_INVISIBLE && db_get_w(cache->getActiveContact(), szProto, "ApparentMode", 0) != ID_STATUS_ONLINE)
		fSend = false;

	if (!fSend) {
		CWarning::show(CWarning::WARN_SENDFILE, MB_OK | MB_ICONEXCLAMATION | CWarning::CWF_NOALLOWHIDE);
		return;
	}

	if (tempdirlen <= 0 || tempdirlen >= MAX_PATH - mir_wstrlen(mirandatempdir) - mir_wstrlen(filenametemplate) - 2) // -2 is because %Y takes 4 symbols
		filename[0] = 0;					// prompt for a new name
	else {
		mir_wstrcpy(filename + tempdirlen, mirandatempdir);
		if ((GetFileAttributes(filename) == INVALID_FILE_ATTRIBUTES || ((GetFileAttributes(filename) & FILE_ATTRIBUTE_DIRECTORY) == 0)) && CreateDirectory(filename, NULL) == 0)
			filename[0] = 0;
		else {
			tempdirlen = mir_wstrlen(filename);

			time_t rawtime;
			time(&rawtime);
			const tm* timeinfo;
			timeinfo = _localtime32((__time32_t *)&rawtime);
			wcsftime(filename + tempdirlen, MAX_PATH - tempdirlen, filenametemplate, timeinfo);
			size_t firstnumberpos = tempdirlen + 14;
			size_t lastnumberpos = tempdirlen + 20;
			while (GetFileAttributes(filename) != INVALID_FILE_ATTRIBUTES) {	// while it exists
				for (size_t pos = lastnumberpos; pos >= firstnumberpos; pos--)
					if (filename[pos]++ != '9')
						break;
					else
						if (pos == firstnumberpos)
							filename[0] = 0;	// all filenames exist => prompt for a new name
						else
							filename[pos] = '0';
			}
		}
	}

	if (filename[0] == 0) {	// prompting to save
		wchar_t filter[MAX_PATH];
		mir_snwprintf(filter, L"%s%c*.jpg%c%c", TranslateT("JPEG-compressed images"), 0, 0, 0);

		OPENFILENAME dlg;
		dlg.lStructSize = sizeof(dlg);
		dlg.lpstrFilter = filter;
		dlg.nFilterIndex = 1;
		dlg.lpstrFile = filename;
		dlg.nMaxFile = MAX_PATH;
		dlg.Flags = OFN_NOREADONLYRETURN | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
		dlg.lpstrDefExt = L"jpg";
		if (!GetSaveFileName(&dlg))
			return;
	}

	IMGSRVC_INFO ii;
	ii.cbSize = sizeof(ii);
	ii.hbm = hbmp;
	ii.wszName = filename;
	ii.dwMask = IMGI_HBITMAP;
	ii.fif = FIF_JPEG;
	CallService(MS_IMG_SAVE, (WPARAM)&ii, IMGL_WCHAR);

	int totalCount = 0;
	wchar_t **ppFiles = NULL;
	Utils::AddToFileList(&ppFiles, &totalCount, filename);

	wchar_t* _t = mir_wstrdup(filename);
	vTempFilenames.insert(_t);

	CallService(MS_FILE_SENDSPECIFICFILEST, (WPARAM)cache->getActiveContact(), (LPARAM)ppFiles);

	mir_free(ppFiles[0]);
	mir_free(ppFiles);
}

// remove all temporary files created by the "send clipboard as file" feature.
void TSAPI CleanTempFiles()
{
	for (int i = 0; i < vTempFilenames.getCount(); i++) {
		wchar_t* _t = vTempFilenames[i];
		DeleteFileW(_t);
		mir_free(_t);
	}
}

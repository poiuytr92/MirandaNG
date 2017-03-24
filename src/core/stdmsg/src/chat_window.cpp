/*
Chat module plugin for Miranda IM

Copyright 2000-12 Miranda IM, 2012-17 Miranda NG project,
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

static HKL hkl = nullptr;

static wchar_t szTrimString[] = L":;,.!?\'\"><()[]- \r\n";

int GetTextPixelSize(wchar_t *pszText, HFONT hFont, BOOL bWidth)
{
	if (!pszText || !hFont)
		return 0;

	HDC hdc = GetDC(nullptr);
	HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
	RECT rc = {};
	DrawText(hdc, pszText, -1, &rc, DT_CALCRECT);
	SelectObject(hdc, hOldFont);
	ReleaseDC(nullptr, hdc);
	return bWidth ? rc.right - rc.left : rc.bottom - rc.top;
}

static void __cdecl phase2(void *lParam)
{
	SESSION_INFO *si = (SESSION_INFO*)lParam;
	Sleep(30);
	if (si && si->pDlg)
		si->pDlg->RedrawLog2();
}

/////////////////////////////////////////////////////////////////////////////////////////

CChatRoomDlg::CChatRoomDlg(SESSION_INFO *si) :
	CSrmmBaseDialog(g_hInst, g_Settings.bTabsEnable ? IDD_CHANNEL_TAB : IDD_CHANNEL, si),
	m_btnOk(this, IDOK),
	
	m_splitterX(this, IDC_SPLITTERX),
	m_splitterY(this, IDC_SPLITTERY)
{
	m_autoClose = 0;
	m_forceResizable = true;

	m_btnOk.OnClick = Callback(this, &CChatRoomDlg::onClick_Ok);

	m_btnFilter.OnClick = Callback(this, &CChatRoomDlg::onClick_Filter);
	m_btnChannelMgr.OnClick = Callback(this, &CChatRoomDlg::onClick_Options);
	m_btnNickList.OnClick = Callback(this, &CChatRoomDlg::onClick_NickList);

	m_splitterX.OnChange = Callback(this, &CChatRoomDlg::onSplitterX);
	m_splitterY.OnChange = Callback(this, &CChatRoomDlg::onSplitterY);

	m_iSplitterX = g_Settings.iSplitterX;
	m_iSplitterY = g_Settings.iSplitterY;
}

void CChatRoomDlg::OnInitDialog()
{
	CSrmmBaseDialog::OnInitDialog();
	m_si->pDlg = this;
	SetWindowLongPtr(m_hwnd, GWLP_USERDATA, LPARAM(this));

	if (g_Settings.bTabsEnable)
		SetWindowLongPtr(m_hwnd, GWL_EXSTYLE, GetWindowLongPtr(m_hwnd, GWL_EXSTYLE) | WS_EX_APPWINDOW);

	// initialize toolbar icons
	Srmm_CreateToolbarIcons(m_hwnd, BBBF_ISCHATBUTTON);

	WindowList_Add(pci->hWindowList, m_hwnd, m_hContact);

	NotifyLocalWinEvent(m_hContact, m_hwnd, MSG_WINDOW_EVT_OPENING);

	m_log.SendMsg(EM_AUTOURLDETECT, 1, 0);

	int mask = (int)m_log.SendMsg(EM_GETEVENTMASK, 0, 0);
	m_log.SendMsg(EM_SETEVENTMASK, 0, mask | ENM_LINK | ENM_MOUSEEVENTS);
	m_log.SendMsg(EM_LIMITTEXT, sizeof(wchar_t) * 0x7FFFFFFF, 0);
	m_log.SendMsg(EM_SETOLECALLBACK, 0, (LPARAM)&reOleCallback);

	DWORD dwFlags = WS_CHILD | WS_VISIBLE | SBT_TOOLTIPS;
	if (!g_Settings.bTabsEnable)
		dwFlags |= SBARS_SIZEGRIP;

	m_hwndStatus = CreateWindowEx(0, STATUSCLASSNAME, nullptr, dwFlags, 0, 0, 0, 0, m_hwnd, nullptr, g_hInst, nullptr);
	SendMessage(m_hwndStatus, SB_SETMINHEIGHT, GetSystemMetrics(SM_CYSMICON), 0);

	m_log.SendMsg(EM_HIDESELECTION, TRUE, 0);

	UpdateOptions();
	UpdateStatusBar();
	UpdateTitle();
	SetWindowPosition();

	SendMessage(m_hwnd, WM_SIZE, 0, 0);

	NotifyLocalWinEvent(m_hContact, m_hwnd, MSG_WINDOW_EVT_OPEN);
}

void CChatRoomDlg::OnDestroy()
{
	NotifyLocalWinEvent(m_hContact, m_hwnd, MSG_WINDOW_EVT_CLOSING);

	SaveWindowPosition(true);
	if (!g_Settings.bTabsEnable) {
		if (db_get_b(0, CHAT_MODULE, "SavePosition", 0)) {
			db_set_dw(m_hContact, CHAT_MODULE, "roomx", m_si->iX);
			db_set_dw(m_hContact, CHAT_MODULE, "roomy", m_si->iY);
			db_set_dw(m_hContact, CHAT_MODULE, "roomwidth", m_si->iWidth);
			db_set_dw(m_hContact, CHAT_MODULE, "roomheight", m_si->iHeight);
		}
	}

	WindowList_Remove(pci->hWindowList, m_hwnd);

	m_si->pDlg = nullptr;
	m_si->wState &= ~STATE_TALK;
	DestroyWindow(m_hwndStatus); m_hwndStatus = nullptr;

	NotifyLocalWinEvent(m_hContact, m_hwnd, MSG_WINDOW_EVT_CLOSE);

	CSuper::OnDestroy();
}

void CChatRoomDlg::onClick_Filter(CCtrlButton *pButton)
{
	if (!pButton->Enabled())
		return;

	m_bFilterEnabled = !m_bFilterEnabled;
	m_btnFilter.SendMsg(BM_SETIMAGE, IMAGE_ICON, (LPARAM)LoadIconEx(m_bFilterEnabled ? "filter" : "filter2", FALSE));
	if (m_bFilterEnabled && db_get_b(0, CHAT_MODULE, "RightClickFilter", 0) == 0)
		ShowFilterMenu();
	else
		RedrawLog();
}

void CChatRoomDlg::onClick_NickList(CCtrlButton *pButton)
{
	if (!pButton->Enabled() || m_si->iType == GCW_SERVER)
		return;

	m_bNicklistEnabled = !m_bNicklistEnabled;
	pButton->SendMsg(BM_SETIMAGE, IMAGE_ICON, (LPARAM)LoadIconEx(m_bNicklistEnabled ? "nicklist" : "nicklist2", FALSE));

	ScrollToBottom();
	SendMessage(m_hwnd, WM_SIZE, 0, 0);
}

void CChatRoomDlg::onClick_Options(CCtrlButton *pButton)
{
	if (pButton->Enabled())
		DoEventHook(GC_USER_CHANMGR, nullptr, nullptr, 0);
}

void CChatRoomDlg::onClick_Ok(CCtrlButton *pButton)
{
	if (!pButton->Enabled())
		return;

	ptrA pszRtf(Message_GetFromStream(m_hwnd, m_si));
	if (pszRtf == nullptr)
		return;

	MODULEINFO *mi = pci->MM_FindModule(m_si->pszModule);
	if (mi == nullptr)
		return;

	pci->SM_AddCommand(m_si->ptszID, m_si->pszModule, pszRtf);

	CMStringW ptszText(ptrW(mir_utf8decodeW(pszRtf)));
	pci->DoRtfToTags(ptszText, mi->nColorCount, mi->crColors);
	ptszText.Trim();
	ptszText.Replace(L"%", L"%%");

	if (mi->bAckMsg) {
		EnableWindow(m_message.GetHwnd(), FALSE);
		m_message.SendMsg(EM_SETREADONLY, TRUE, 0);
	}
	else m_message.SetText(L"");

	EnableWindow(m_btnOk.GetHwnd(), FALSE);

	DoEventHook(GC_USER_MESSAGE, nullptr, ptszText, 0);

	SetFocus(m_message.GetHwnd());
}

void CChatRoomDlg::onSplitterX(CSplitter *pSplitter)
{
	RECT rc;
	GetClientRect(m_hwnd, &rc);

	m_iSplitterX = rc.right - pSplitter->GetPos() + 1;
	if (m_iSplitterX < 35)
		m_iSplitterX = 35;
	if (m_iSplitterX > rc.right - rc.left - 35)
		m_iSplitterX = rc.right - rc.left - 35;
	g_Settings.iSplitterX = m_iSplitterX;
}

void CChatRoomDlg::onSplitterY(CSplitter *pSplitter)
{
	RECT rc;
	GetClientRect(m_hwnd, &rc);

	m_iSplitterY = rc.bottom - pSplitter->GetPos() + 1;
	if (!IsWindowVisible(m_btnBold.GetHwnd()))
		m_iSplitterY += 19;

	if (m_iSplitterY < 63)
		m_iSplitterY = 63;
	if (m_iSplitterY > rc.bottom - rc.top - 40)
		m_iSplitterY = rc.bottom - rc.top - 40;
	g_Settings.iSplitterY = m_iSplitterY;
}

void CChatRoomDlg::SetWindowPosition()
{
	if (g_Settings.bTabsEnable)
		return;

	WINDOWPLACEMENT wp;
	wp.length = sizeof(wp);
	GetWindowPlacement(m_hwnd, &wp);

	RECT screen;
	SystemParametersInfo(SPI_GETWORKAREA, 0, &screen, 0);

	if (m_si->iX) {
		wp.rcNormalPosition.left = m_si->iX;
		wp.rcNormalPosition.top = m_si->iY;
		wp.rcNormalPosition.right = wp.rcNormalPosition.left + m_si->iWidth;
		wp.rcNormalPosition.bottom = wp.rcNormalPosition.top + m_si->iHeight;
		wp.showCmd = SW_HIDE;
		SetWindowPlacement(m_hwnd, &wp);
		return;
	}
	
	if (db_get_b(0, CHAT_MODULE, "SavePosition", 0)) {
		if (RestoreWindowPosition(m_hwnd, m_hContact, true)) {
			ShowWindow(m_hwnd, SW_HIDE);
			return;
		}
		SetWindowPos(m_hwnd, 0, (screen.right - screen.left) / 2 - (550) / 2, (screen.bottom - screen.top) / 2 - (400) / 2, (550), (400), SWP_NOZORDER | SWP_HIDEWINDOW | SWP_NOACTIVATE);
	}
	else SetWindowPos(m_hwnd, 0, (screen.right - screen.left) / 2 - (550) / 2, (screen.bottom - screen.top) / 2 - (400) / 2, (550), (400), SWP_NOZORDER | SWP_HIDEWINDOW | SWP_NOACTIVATE);

	SESSION_INFO *pActive = pci->GetActiveSession();
	if (pActive && pActive->pDlg && db_get_b(0, CHAT_MODULE, "CascadeWindows", 1)) {
		RECT rcThis, rcNew;
		int dwFlag = SWP_NOZORDER | SWP_NOACTIVATE;
		if (!IsWindowVisible(m_hwnd))
			dwFlag |= SWP_HIDEWINDOW;

		GetWindowRect(m_hwnd, &rcThis);
		GetWindowRect(pActive->pDlg->GetHwnd(), &rcNew);

		int offset = GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CYFRAME);
		SetWindowPos(m_hwnd, 0, rcNew.left + offset, rcNew.top + offset, rcNew.right - rcNew.left, rcNew.bottom - rcNew.top, dwFlag);
	}
}

void CChatRoomDlg::SaveWindowPosition(bool bUpdateSession)
{
	WINDOWPLACEMENT wp = {};
	wp.length = sizeof(wp);
	GetWindowPlacement(getCaptionWindow(), &wp);

	g_Settings.iX = wp.rcNormalPosition.left;
	g_Settings.iY = wp.rcNormalPosition.top;
	g_Settings.iWidth = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
	g_Settings.iHeight = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;

	if (bUpdateSession) {
		m_si->iX = g_Settings.iX;
		m_si->iY = g_Settings.iY;
		m_si->iWidth = g_Settings.iWidth;
		m_si->iHeight = g_Settings.iHeight;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////

void CChatRoomDlg::CloseTab()
{
	if (g_Settings.bTabsEnable)
		SendMessage(GetParent(m_hwndParent), GC_REMOVETAB, 0, (LPARAM)this);
	Close();
}

void CChatRoomDlg::LoadSettings()
{
	m_clrInputBG = db_get_dw(0, CHAT_MODULE, "ColorMessageBG", GetSysColor(COLOR_WINDOW));
	m_clrInputFG = g_Settings.MessageAreaColor;
}

void CChatRoomDlg::RedrawLog()
{
	m_si->LastTime = 0;
	if (m_si->pLog) {
		LOGINFO * pLog = m_si->pLog;
		if (m_si->iEventCount > 60) {
			int index = 0;
			while (index < 59) {
				if (pLog->next == nullptr)
					break;

				pLog = pLog->next;
				if (m_si->iType != GCW_CHATROOM || !m_bFilterEnabled || (m_iLogFilterFlags & pLog->iType) != 0)
					index++;
			}
			StreamInEvents(pLog, true);
			mir_forkthread(phase2, m_si);
		}
		else StreamInEvents(m_si->pLogEnd, true);
	}
	else ClearLog();
}

void CChatRoomDlg::ScrollToBottom()
{
	if ((GetWindowLongPtr(m_log.GetHwnd(), GWL_STYLE) & WS_VSCROLL) == 0)
		return;

	CHARRANGE sel;
	SCROLLINFO scroll = {};
	scroll.cbSize = sizeof(scroll);
	scroll.fMask = SIF_PAGE | SIF_RANGE;
	GetScrollInfo(m_log.GetHwnd(), SB_VERT, &scroll);

	scroll.fMask = SIF_POS;
	scroll.nPos = scroll.nMax - scroll.nPage + 1;
	SetScrollInfo(m_log.GetHwnd(), SB_VERT, &scroll, TRUE);

	sel.cpMin = sel.cpMax = GetRichTextLength(m_log.GetHwnd());
	m_log.SendMsg(EM_EXSETSEL, 0, (LPARAM)&sel);
	PostMessage(m_log.GetHwnd(), WM_VSCROLL, MAKEWPARAM(SB_BOTTOM, 0), 0);
}

void CChatRoomDlg::ShowFilterMenu()
{
	HWND hwnd = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_FILTER), m_hwnd, FilterWndProc, (LPARAM)this);
	TranslateDialogDefault(hwnd);

	RECT rc;
	GetWindowRect(m_btnFilter.GetHwnd(), &rc);
	SetWindowPos(hwnd, HWND_TOP, rc.left - 85, (IsWindowVisible(m_btnFilter.GetHwnd()) || IsWindowVisible(m_btnBold.GetHwnd())) ? rc.top - 206 : rc.top - 186, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
}

void CChatRoomDlg::UpdateNickList()
{
	int i = m_nickList.SendMsg(LB_GETTOPINDEX, 0, 0);
	m_nickList.SendMsg(LB_SETCOUNT, m_si->nUsersInNicklist, 0);
	m_nickList.SendMsg(LB_SETTOPINDEX, i, 0);

	UpdateTitle();
}

void CChatRoomDlg::UpdateOptions()
{
	m_btnNickList.SendMsg(BM_SETIMAGE, IMAGE_ICON, (LPARAM)LoadIconEx(m_bNicklistEnabled ? "nicklist" : "nicklist2", FALSE));
	m_btnFilter.SendMsg(BM_SETIMAGE, IMAGE_ICON, (LPARAM)LoadIconEx(m_bFilterEnabled ? "filter" : "filter2", FALSE));

	MODULEINFO *mi = pci->MM_FindModule(m_si->pszModule);
	EnableWindow(m_btnBold.GetHwnd(), mi->bBold);
	EnableWindow(m_btnItalic.GetHwnd(), mi->bItalics);
	EnableWindow(m_btnUnderline.GetHwnd(), mi->bUnderline);
	EnableWindow(m_btnColor.GetHwnd(), mi->bColor);
	EnableWindow(m_btnBkColor.GetHwnd(), mi->bBkgColor);
	if (m_si->iType == GCW_CHATROOM)
		EnableWindow(m_btnChannelMgr.GetHwnd(), mi->bChanMgr);

	HICON hIcon = m_si->wStatus == ID_STATUS_ONLINE ? mi->hOnlineIcon : mi->hOfflineIcon;
	if (!hIcon) {
		pci->MM_IconsChanged();
		hIcon = (m_si->wStatus == ID_STATUS_ONLINE) ? mi->hOnlineIcon : mi->hOfflineIcon;
	}

	if (g_Settings.bTabsEnable)
		pDialog->FixTabIcons(nullptr);

	SendMessage(m_hwndStatus, SB_SETICON, 0, (LPARAM)hIcon);

	Window_SetIcon_IcoLib(getCaptionWindow(), GetIconHandle("window"));

	m_log.SendMsg(EM_SETBKGNDCOLOR, 0, g_Settings.crLogBackground);

	CHARFORMAT2 cf;
	cf.cbSize = sizeof(CHARFORMAT2);
	cf.dwMask = CFM_COLOR | CFM_BOLD | CFM_UNDERLINE | CFM_BACKCOLOR;
	cf.dwEffects = 0;
	cf.crTextColor = g_Settings.MessageAreaColor;
	cf.crBackColor = m_clrInputBG;
	
	m_message.SendMsg(EM_SETBKGNDCOLOR, 0, m_clrInputBG);
	m_message.SendMsg(WM_SETFONT, (WPARAM)g_Settings.MessageAreaFont, MAKELPARAM(TRUE, 0));
	m_message.SendMsg(EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

	// nicklist
	int ih = GetTextPixelSize(L"AQGglo", g_Settings.UserListFont, FALSE);
	int ih2 = GetTextPixelSize(L"AQGglo", g_Settings.UserListHeadingsFont, FALSE);
	int height = db_get_b(0, CHAT_MODULE, "NicklistRowDist", 12);
	int font = ih > ih2 ? ih : ih2;

	// make sure we have space for icon!
	if (g_Settings.bShowContactStatus)
		font = font > 16 ? font : 16;

	m_nickList.SendMsg(LB_SETITEMHEIGHT, 0, height > font ? height : font);
	InvalidateRect(m_nickList.GetHwnd(), nullptr, TRUE);

	SendMessage(m_hwnd, WM_SIZE, 0, 0);
	RedrawLog2();
}

void CChatRoomDlg::UpdateStatusBar()
{
	MODULEINFO *mi = pci->MM_FindModule(m_si->pszModule);
	wchar_t *ptszDispName = mi->ptszModDispName;
	int x = 12;
	x += GetTextPixelSize(ptszDispName, (HFONT)SendMessage(m_hwndStatus, WM_GETFONT, 0, 0), TRUE);
	x += GetSystemMetrics(SM_CXSMICON);
	int iStatusbarParts[2] = { x, -1 };
	SendMessage(m_hwndStatus, SB_SETPARTS, 2, (LPARAM)&iStatusbarParts);

	// stupid hack to make icons show. I dunno why this is needed currently
	HICON hIcon = m_si->wStatus == ID_STATUS_ONLINE ? mi->hOnlineIcon : mi->hOfflineIcon;
	if (!hIcon) {
		pci->MM_IconsChanged();
		hIcon = m_si->wStatus == ID_STATUS_ONLINE ? mi->hOnlineIcon : mi->hOfflineIcon;
	}

	SendMessage(m_hwndStatus, SB_SETICON, 0, (LPARAM)hIcon);

	if (g_Settings.bTabsEnable)
		pDialog->FixTabIcons(nullptr);

	SendMessage(m_hwndStatus, SB_SETTEXT, 0, (LPARAM)ptszDispName);
	SendMessage(m_hwndStatus, SB_SETTEXT, 1, (LPARAM)(m_si->ptszStatusbarText ? m_si->ptszStatusbarText : L""));
	SendMessage(m_hwndStatus, SB_SETTIPTEXT, 1, (LPARAM)(m_si->ptszStatusbarText ? m_si->ptszStatusbarText : L""));
}

void CChatRoomDlg::UpdateTitle()
{
	wchar_t szTemp[100];
	switch (m_si->iType) {
	case GCW_CHATROOM:
		mir_snwprintf(szTemp,
			(m_si->nUsersInNicklist == 1) ? TranslateT("%s: chat room (%u user)") : TranslateT("%s: chat room (%u users)"),
			m_si->ptszName, m_si->nUsersInNicklist);
		break;
	case GCW_PRIVMESS:
		mir_snwprintf(szTemp,
			(m_si->nUsersInNicklist == 1) ? TranslateT("%s: message session") : TranslateT("%s: message session (%u users)"),
			m_si->ptszName, m_si->nUsersInNicklist);
		break;
	case GCW_SERVER:
		mir_snwprintf(szTemp, L"%s: Server", m_si->ptszName);
		break;
	}

	SetWindowText(getCaptionWindow(), szTemp);
}

/////////////////////////////////////////////////////////////////////////////////////////

int CChatRoomDlg::Resizer(UTILRESIZECONTROL *urc)
{
	SESSION_INFO *si = m_si;

	RECT rc;
	bool bControl = db_get_b(0, CHAT_MODULE, "ShowTopButtons", 1) != 0;
	bool bFormat = db_get_b(0, CHAT_MODULE, "ShowFormatButtons", 1) != 0;
	bool bToolbar = bFormat || bControl;
	bool bSend = db_get_b(0, CHAT_MODULE, "ShowSend", 0) != 0;
	bool bNick = si->iType != GCW_SERVER && m_bNicklistEnabled;

	switch (urc->wId) {
	case IDOK:
		GetWindowRect(m_hwndStatus, &rc);
		urc->rcItem.left = bSend ? 315 : urc->dlgNewSize.cx;
		urc->rcItem.top = urc->dlgNewSize.cy - m_iSplitterY + 23;
		urc->rcItem.bottom = urc->dlgNewSize.cy - (rc.bottom - rc.top) - 1;
		return RD_ANCHORX_RIGHT | RD_ANCHORY_CUSTOM;

	case IDC_SRMM_LOG:
		urc->rcItem.top = 2;
		urc->rcItem.left = 0;
		urc->rcItem.right = bNick ? urc->dlgNewSize.cx - m_iSplitterX : urc->dlgNewSize.cx;
	LBL_CalcBottom:
		urc->rcItem.bottom = urc->dlgNewSize.cy - m_iSplitterY;
		if (!bToolbar)
			urc->rcItem.bottom += 20;
		return RD_ANCHORX_CUSTOM | RD_ANCHORY_CUSTOM;

	case IDC_SRMM_NICKLIST:
		urc->rcItem.top = 2;
		urc->rcItem.right = urc->dlgNewSize.cx;
		urc->rcItem.left = urc->dlgNewSize.cx - m_iSplitterX + 2;
		goto LBL_CalcBottom;

	case IDC_SPLITTERX:
		urc->rcItem.top = 1;
		urc->rcItem.left = urc->dlgNewSize.cx - m_iSplitterX;
		urc->rcItem.right = urc->rcItem.left + 2;
		goto LBL_CalcBottom;

	case IDC_SPLITTERY:
		urc->rcItem.top = urc->dlgNewSize.cy - m_iSplitterY;
		if (!bToolbar)
			urc->rcItem.top += 20;
		urc->rcItem.bottom = urc->rcItem.top + 2;
		return RD_ANCHORX_WIDTH | RD_ANCHORY_CUSTOM;

	case IDC_SRMM_MESSAGE:
		GetWindowRect(m_hwndStatus, &rc);
		urc->rcItem.right = bSend ? urc->dlgNewSize.cx - 64 : urc->dlgNewSize.cx;
		urc->rcItem.top = urc->dlgNewSize.cy - m_iSplitterY + 22;
		urc->rcItem.bottom = urc->dlgNewSize.cy - (rc.bottom - rc.top) - 1;
		return RD_ANCHORX_LEFT | RD_ANCHORY_CUSTOM;
	}
	return RD_ANCHORX_LEFT | RD_ANCHORY_TOP;
}

/////////////////////////////////////////////////////////////////////////////////////////

INT_PTR CALLBACK CChatRoomDlg::FilterWndProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static CChatRoomDlg *pDlg = nullptr;
	switch (uMsg) {
	case WM_INITDIALOG:
		pDlg = (CChatRoomDlg*)lParam;
		CheckDlgButton(hwndDlg, IDC_1, pDlg->m_iLogFilterFlags & GC_EVENT_ACTION ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_2, pDlg->m_iLogFilterFlags & GC_EVENT_MESSAGE ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_3, pDlg->m_iLogFilterFlags & GC_EVENT_NICK ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_4, pDlg->m_iLogFilterFlags & GC_EVENT_JOIN ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_5, pDlg->m_iLogFilterFlags & GC_EVENT_PART ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_6, pDlg->m_iLogFilterFlags & GC_EVENT_TOPIC ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_7, pDlg->m_iLogFilterFlags & GC_EVENT_ADDSTATUS ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_8, pDlg->m_iLogFilterFlags & GC_EVENT_INFORMATION ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_9, pDlg->m_iLogFilterFlags & GC_EVENT_QUIT ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_10, pDlg->m_iLogFilterFlags & GC_EVENT_KICK ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_11, pDlg->m_iLogFilterFlags & GC_EVENT_NOTICE ? BST_CHECKED : BST_UNCHECKED);
		break;

	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORSTATIC:
		SetTextColor((HDC)wParam, RGB(60, 60, 150));
		SetBkColor((HDC)wParam, GetSysColor(COLOR_WINDOW));
		return (INT_PTR)GetSysColorBrush(COLOR_WINDOW);

	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE) {
			int iFlags = 0;

			if (IsDlgButtonChecked(hwndDlg, IDC_1) == BST_CHECKED)
				iFlags |= GC_EVENT_ACTION;
			if (IsDlgButtonChecked(hwndDlg, IDC_2) == BST_CHECKED)
				iFlags |= GC_EVENT_MESSAGE;
			if (IsDlgButtonChecked(hwndDlg, IDC_3) == BST_CHECKED)
				iFlags |= GC_EVENT_NICK;
			if (IsDlgButtonChecked(hwndDlg, IDC_4) == BST_CHECKED)
				iFlags |= GC_EVENT_JOIN;
			if (IsDlgButtonChecked(hwndDlg, IDC_5) == BST_CHECKED)
				iFlags |= GC_EVENT_PART;
			if (IsDlgButtonChecked(hwndDlg, IDC_6) == BST_CHECKED)
				iFlags |= GC_EVENT_TOPIC;
			if (IsDlgButtonChecked(hwndDlg, IDC_7) == BST_CHECKED)
				iFlags |= GC_EVENT_ADDSTATUS;
			if (IsDlgButtonChecked(hwndDlg, IDC_8) == BST_CHECKED)
				iFlags |= GC_EVENT_INFORMATION;
			if (IsDlgButtonChecked(hwndDlg, IDC_9) == BST_CHECKED)
				iFlags |= GC_EVENT_QUIT;
			if (IsDlgButtonChecked(hwndDlg, IDC_10) == BST_CHECKED)
				iFlags |= GC_EVENT_KICK;
			if (IsDlgButtonChecked(hwndDlg, IDC_11) == BST_CHECKED)
				iFlags |= GC_EVENT_NOTICE;

			if (iFlags & GC_EVENT_ADDSTATUS)
				iFlags |= GC_EVENT_REMOVESTATUS;

			pDlg->m_iLogFilterFlags = iFlags;
			if (pDlg->m_bFilterEnabled)
				pDlg->RedrawLog();
			PostMessage(hwndDlg, WM_CLOSE, 0, 0);
		}
		break;

	case WM_CLOSE:
		DestroyWindow(hwndDlg);
		break;
	}

	return(FALSE);
}

/////////////////////////////////////////////////////////////////////////////////////////

LRESULT CChatRoomDlg::WndProc_Message(UINT msg, WPARAM wParam, LPARAM lParam)
{
	CHARRANGE sel;

	switch (msg) {
	case WM_MOUSEWHEEL:
		m_log.SendMsg(WM_MOUSEWHEEL, wParam, lParam);
		m_iLastEnterTime = 0;
		return TRUE;

	case EM_REPLACESEL:
		PostMessage(m_message.GetHwnd(), EM_ACTIVATE, 0, 0);
		break;

	case EM_ACTIVATE:
		SetActiveWindow(m_hwnd);
		break;

	case WM_CHAR:
		{
			BOOL isCtrl = GetKeyState(VK_CONTROL) & 0x8000;
			BOOL isAlt = GetKeyState(VK_MENU) & 0x8000;

			if (GetWindowLongPtr(m_message.GetHwnd(), GWL_STYLE) & ES_READONLY)
				break;

			if (wParam == 9 && isCtrl && !isAlt) // ctrl-i (italics)
				return TRUE;

			if (wParam == VK_SPACE && isCtrl && !isAlt) // ctrl-space (paste clean text)
				return TRUE;

			if (wParam == '\n' || wParam == '\r') {
				if ((isCtrl != 0) ^ (0 != db_get_b(0, CHAT_MODULE, "SendOnEnter", 1))) {
					PostMessage(m_hwnd, WM_COMMAND, IDOK, 0);
					return 0;
				}
				if (db_get_b(0, CHAT_MODULE, "SendOnDblEnter", 0)) {
					if (m_iLastEnterTime + 2 < time(nullptr))
						m_iLastEnterTime = time(nullptr);
					else {
						m_message.SendMsg(WM_KEYDOWN, VK_BACK, 0);
						m_message.SendMsg(WM_KEYUP, VK_BACK, 0);
						PostMessage(m_hwnd, WM_COMMAND, IDOK, 0);
						return 0;
					}
				}
			}
			else m_iLastEnterTime = 0;

			if (wParam == 1 && isCtrl && !isAlt) {      //ctrl-a
				m_message.SendMsg(EM_SETSEL, 0, -1);
				return 0;
			}
		}
		break;

	case WM_KEYDOWN:
		{
			static int start, end;
			BOOL isShift = GetKeyState(VK_SHIFT) & 0x8000;
			BOOL isCtrl = GetKeyState(VK_CONTROL) & 0x8000;
			BOOL isAlt = GetKeyState(VK_MENU) & 0x8000;
			if (wParam == VK_RETURN) {
				szTabSave[0] = '\0';
				if ((isCtrl != 0) ^ (0 != db_get_b(0, CHAT_MODULE, "SendOnEnter", 1)))
					return 0;

				if (db_get_b(0, CHAT_MODULE, "SendOnDblEnter", 0))
					if (m_iLastEnterTime + 2 >= time(nullptr))
						return 0;

				break;
			}

			if (wParam == VK_TAB && isShift && !isCtrl) { // SHIFT-TAB (go to nick list)
				SetFocus(m_nickList.GetHwnd());
				return TRUE;
			}

			if (wParam == VK_TAB && isCtrl && !isShift) { // CTRL-TAB (switch tab/window)
				if (g_Settings.bTabsEnable)
					SendMessage(GetParent(GetParent(m_hwnd)), GC_SWITCHNEXTTAB, 0, 0);
				else
					pci->ShowRoom(SM_GetNextWindow(m_si));
				return TRUE;
			}

			if (wParam == VK_TAB && isCtrl && isShift) { // CTRL_SHIFT-TAB (switch tab/window)
				if (g_Settings.bTabsEnable)
					SendMessage(GetParent(GetParent(m_hwnd)), GC_SWITCHPREVTAB, 0, 0);
				else
					pci->ShowRoom(SM_GetPrevWindow(m_si));
				return TRUE;
			}

			if (wParam <= '9' && wParam >= '1' && isCtrl && !isAlt) // CTRL + 1 -> 9 (switch tab)
				if (g_Settings.bTabsEnable)
					SendMessage(m_hwnd, GC_SWITCHTAB, 0, (int)wParam - (int)'1');

			if (wParam <= VK_NUMPAD9 && wParam >= VK_NUMPAD1 && isCtrl && !isAlt) // CTRL + 1 -> 9 (switch tab)
				if (g_Settings.bTabsEnable)
					SendMessage(m_hwnd, GC_SWITCHTAB, 0, (int)wParam - (int)VK_NUMPAD1);

			if (wParam == VK_TAB && !isCtrl && !isShift) { // tab-autocomplete
				LRESULT lResult = (LRESULT)m_message.SendMsg(EM_GETSEL, 0, 0);

				m_message.SendMsg(WM_SETREDRAW, FALSE, 0);
				start = LOWORD(lResult);
				end = HIWORD(lResult);
				m_message.SendMsg(EM_SETSEL, end, end);

				GETTEXTLENGTHEX gtl = {};
				gtl.flags = GTL_PRECISE;
				gtl.codepage = CP_ACP;
				int iLen = m_message.SendMsg(EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);
				if (iLen > 0) {
					wchar_t *pszText = (wchar_t *)mir_alloc(sizeof(wchar_t)*(iLen + 100));

					GETTEXTEX gt = {};
					gt.cb = iLen + 99;
					gt.flags = GT_DEFAULT;
					gt.codepage = 1200;
					m_message.SendMsg(EM_GETTEXTEX, (WPARAM)&gt, (LPARAM)pszText);

					while (start > 0 && pszText[start - 1] != ' ' && pszText[start - 1] != 13 && pszText[start - 1] != VK_TAB)
						start--;
					while (end < iLen && pszText[end] != ' ' && pszText[end] != 13 && pszText[end - 1] != VK_TAB)
						end++;

					if (szTabSave[0] == '\0')
						mir_wstrncpy(szTabSave, pszText + start, end - start + 1);

					wchar_t *pszSelName = (wchar_t *)mir_alloc(sizeof(wchar_t)*(end - start + 1));
					mir_wstrncpy(pszSelName, pszText + start, end - start + 1);

					wchar_t *pszName = pci->UM_FindUserAutoComplete(m_si->pUsers, szTabSave, pszSelName);
					if (pszName == nullptr) {
						pszName = szTabSave;
						m_message.SendMsg(EM_SETSEL, start, end);
						if (end != start)
							m_message.SendMsg(EM_REPLACESEL, FALSE, (LPARAM)pszName);
						szTabSave[0] = '\0';
					}
					else {
						m_message.SendMsg(EM_SETSEL, start, end);
						if (end != start)
							m_message.SendMsg(EM_REPLACESEL, FALSE, (LPARAM)pszName);
					}
					mir_free(pszText);
					mir_free(pszSelName);
				}

				m_message.SendMsg(WM_SETREDRAW, TRUE, 0);
				RedrawWindow(m_message.GetHwnd(), nullptr, nullptr, RDW_INVALIDATE);
				return 0;
			}

			if (szTabSave[0] != '\0' && wParam != VK_RIGHT && wParam != VK_LEFT && wParam != VK_SPACE && wParam != VK_RETURN && wParam != VK_BACK && wParam != VK_DELETE) {
				if (g_Settings.bAddColonToAutoComplete && start == 0)
					SendMessageA(m_message.GetHwnd(), EM_REPLACESEL, FALSE, (LPARAM) ": ");

				szTabSave[0] = '\0';
			}

			if (ProcessHotkeys(wParam, isShift, isCtrl, isAlt))
				return TRUE;

			if (wParam == 0x46 && isCtrl && !isAlt) { // ctrl-f (paste clean text)
				onClick_Filter(&m_btnFilter);
				return TRUE;
			}

			if (wParam == 0x4e && isCtrl && !isAlt) { // ctrl-n (nicklist)
				onClick_NickList(&m_btnNickList);
				return TRUE;
			}

			if (wParam == 0x4f && isCtrl && !isAlt) { // ctrl-o (options)
				onClick_Options(&m_btnChannelMgr);
				return TRUE;
			}

			if ((wParam == 45 && isShift || wParam == 0x56 && isCtrl) && !isAlt) { // ctrl-v (paste clean text)
				m_message.SendMsg(EM_PASTESPECIAL, CF_TEXT, 0);
				return TRUE;
			}

			if (wParam == 0x57 && isCtrl && !isAlt) { // ctrl-w (close window)
				CloseTab();
				return TRUE;
			}

			if (wParam == VK_NEXT || wParam == VK_PRIOR) {
				m_log.SendMsg(msg, wParam, lParam);
				m_iLastEnterTime = 0;
				return TRUE;
			}

			if (wParam == VK_UP && isCtrl && !isAlt) {
				char* lpPrevCmd = pci->SM_GetPrevCommand(m_si->ptszID, m_si->pszModule);

				m_message.SendMsg(WM_SETREDRAW, FALSE, 0);

				if (lpPrevCmd) {
					SETTEXTEX ste;
					ste.flags = ST_DEFAULT;
					ste.codepage = CP_ACP;
					m_message.SendMsg(EM_SETTEXTEX, (WPARAM)&ste, (LPARAM)lpPrevCmd);
				}
				else m_message.SetText(L"");

				GETTEXTLENGTHEX gtl = {};
				gtl.flags = GTL_PRECISE;
				gtl.codepage = CP_ACP;
				int iLen = m_message.SendMsg(EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);
				m_message.SendMsg(EM_SCROLLCARET, 0, 0);
				m_message.SendMsg(WM_SETREDRAW, TRUE, 0);
				RedrawWindow(m_message.GetHwnd(), nullptr, nullptr, RDW_INVALIDATE);
				m_message.SendMsg(EM_SETSEL, iLen, iLen);
				m_iLastEnterTime = 0;
				return TRUE;
			}

			if (wParam == VK_DOWN && isCtrl && !isAlt) {
				char *lpPrevCmd = pci->SM_GetNextCommand(m_si->ptszID, m_si->pszModule);
				m_message.SendMsg(WM_SETREDRAW, FALSE, 0);

				if (lpPrevCmd) {
					SETTEXTEX ste;
					ste.flags = ST_DEFAULT;
					ste.codepage = CP_ACP;
					m_message.SendMsg(EM_SETTEXTEX, (WPARAM)&ste, (LPARAM)lpPrevCmd);
				}
				else m_message.SetText(L"");

				GETTEXTLENGTHEX gtl = {};
				gtl.flags = GTL_PRECISE;
				gtl.codepage = CP_ACP;
				int iLen = m_message.SendMsg(EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);
				m_message.SendMsg(EM_SCROLLCARET, 0, 0);
				m_message.SendMsg(WM_SETREDRAW, TRUE, 0);
				RedrawWindow(m_message.GetHwnd(), nullptr, nullptr, RDW_INVALIDATE);
				m_message.SendMsg(EM_SETSEL, iLen, iLen);
				m_iLastEnterTime = 0;
				return TRUE;
			}
		}

		// fall through
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_KILLFOCUS:
		m_iLastEnterTime = 0;
		break;

	case WM_RBUTTONDOWN:
		{
			HMENU hSubMenu = GetSubMenu(g_hMenu, 2);
			TranslateMenu(hSubMenu);
			m_message.SendMsg(EM_EXGETSEL, 0, (LPARAM)&sel);

			EnableMenuItem(hSubMenu, ID_MESSAGE_UNDO, m_message.SendMsg(EM_CANUNDO, 0, 0) ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(hSubMenu, ID_MESSAGE_REDO, m_message.SendMsg(EM_CANREDO, 0, 0) ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(hSubMenu, ID_MESSAGE_COPY, sel.cpMax != sel.cpMin ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(hSubMenu, ID_MESSAGE_CUT, sel.cpMax != sel.cpMin ? MF_ENABLED : MF_GRAYED);

			m_iLastEnterTime = 0;

			POINT pt;
			pt.x = (short)LOWORD(lParam);
			pt.y = (short)HIWORD(lParam);
			ClientToScreen(m_message.GetHwnd(), &pt);

			CHARRANGE all = { 0, -1 };
			UINT uID = TrackPopupMenu(hSubMenu, TPM_RETURNCMD, pt.x, pt.y, 0, m_message.GetHwnd(), nullptr);
			switch (uID) {
			case 0:
				break;

			case ID_MESSAGE_UNDO:
				m_message.SendMsg(EM_UNDO, 0, 0);
				break;

			case ID_MESSAGE_REDO:
				m_message.SendMsg(EM_REDO, 0, 0);
				break;

			case ID_MESSAGE_COPY:
				m_message.SendMsg(WM_COPY, 0, 0);
				break;

			case ID_MESSAGE_CUT:
				m_message.SendMsg(WM_CUT, 0, 0);
				break;

			case ID_MESSAGE_PASTE:
				m_message.SendMsg(EM_PASTESPECIAL, CF_TEXT, 0);
				break;

			case ID_MESSAGE_SELECTALL:
				m_message.SendMsg(EM_EXSETSEL, 0, (LPARAM)&all);
				break;

			case ID_MESSAGE_CLEAR:
				m_message.SetText(L"");
				break;
			}
			PostMessage(m_message.GetHwnd(), WM_KEYUP, 0, 0);
		}
		break;

	case WM_KEYUP:
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
		RefreshButtonStatus();
		break;
	}

	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

LRESULT CChatRoomDlg::WndProc_Log(UINT msg, WPARAM wParam, LPARAM lParam)
{
	CHARRANGE sel;

	switch (msg) {
	case WM_LBUTTONUP:
		m_log.SendMsg(EM_EXGETSEL, 0, (LPARAM)&sel);
		if (sel.cpMin != sel.cpMax) {
			m_log.SendMsg(WM_COPY, 0, 0);
			sel.cpMin = sel.cpMax;
			m_log.SendMsg(EM_EXSETSEL, 0, (LPARAM)&sel);
		}
		SetFocus(m_message.GetHwnd());
		break;

	case WM_KEYDOWN:
		if (wParam == 0x57 && GetKeyState(VK_CONTROL) & 0x8000) { // ctrl-w (close window)
			CloseTab();
			return TRUE;
		}
		break;

	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE) {
			m_log.SendMsg(EM_EXGETSEL, 0, (LPARAM)&sel);
			if (sel.cpMin != sel.cpMax) {
				sel.cpMin = sel.cpMax;
				m_log.SendMsg(EM_EXSETSEL, 0, (LPARAM)&sel);
			}
		}
		break;

	case WM_CHAR:
		SetFocus(m_message.GetHwnd());
		m_message.SendMsg(WM_CHAR, wParam, lParam);
		break;
	}

	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

LRESULT CChatRoomDlg::WndProc_Nicklist(UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_KEYDOWN:
		if (wParam == 0x57 && GetKeyState(VK_CONTROL) & 0x8000) { // ctrl-w (close window)
			CloseTab();
			return TRUE;
		}
		break;

	case WM_CONTEXTMENU:
		TVHITTESTINFO hti;
		{
			int height = 0;
			hti.pt.x = GET_X_LPARAM(lParam);
			hti.pt.y = GET_Y_LPARAM(lParam);
			if (hti.pt.x == -1 && hti.pt.y == -1) {
				int index = m_nickList.SendMsg(LB_GETCURSEL, 0, 0);
				int top = m_nickList.SendMsg(LB_GETTOPINDEX, 0, 0);
				height = m_nickList.SendMsg(LB_GETITEMHEIGHT, 0, 0);
				hti.pt.x = 4;
				hti.pt.y = (index - top)*height + 1;
			}
			else ScreenToClient(m_nickList.GetHwnd(), &hti.pt);

			int item = LOWORD(m_nickList.SendMsg(LB_ITEMFROMPOINT, 0, MAKELPARAM(hti.pt.x, hti.pt.y)));
			USERINFO *ui = pci->SM_GetUserFromIndex(m_si->ptszID, m_si->pszModule, item);
			if (ui) {
				USERINFO uinew;
				memcpy(&uinew, ui, sizeof(USERINFO));
				if (hti.pt.x == -1 && hti.pt.y == -1)
					hti.pt.y += height - 4;
				ClientToScreen(m_nickList.GetHwnd(), &hti.pt);

				HMENU hMenu = 0;
				UINT uID = CreateGCMenu(m_nickList.GetHwnd(), &hMenu, 0, hti.pt, m_si, uinew.pszUID, uinew.pszNick);
				switch (uID) {
				case 0:
					break;

				case ID_MESS:
					DoEventHook(GC_USER_PRIVMESS, ui, nullptr, 0);
					break;

				default:
					DoEventHook(GC_USER_NICKLISTMENU, ui, nullptr, uID);
					break;
				}
				DestroyGCMenu(&hMenu, 1);
				return TRUE;
			}
		}
		break;
	}

	return CSuper::WndProc_Nicklist(msg, wParam, lParam);
}

/////////////////////////////////////////////////////////////////////////////////////////

INT_PTR CChatRoomDlg::DlgProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	SESSION_INFO *s;
	CHARRANGE sel;

	switch (uMsg) {
	case WM_CBD_LOADICONS:
		Srmm_UpdateToolbarIcons(m_hwnd);
		break;

	case WM_CBD_UPDATED:
		SetButtonsPos(m_hwnd, true);
		break;

	case WM_SIZE:
		if (wParam == SIZE_MAXIMIZED)
			ScrollToBottom();

		if (!IsIconic(m_hwnd)) {
			SendMessage(m_hwndStatus, WM_SIZE, 0, 0);

			bool bSend = db_get_b(0, CHAT_MODULE, "ShowSend", 0) != 0;
			bool bFormat = db_get_b(0, CHAT_MODULE, "ShowFormatButtons", 1) != 0;
			bool bControl = db_get_b(0, CHAT_MODULE, "ShowTopButtons", 1) != 0;
			bool bNick = m_si->iType != GCW_SERVER && m_bNicklistEnabled;

			ShowWindow(m_btnBold.GetHwnd(), bFormat ? SW_SHOW : SW_HIDE);
			ShowWindow(m_btnItalic.GetHwnd(), bFormat ? SW_SHOW : SW_HIDE);
			ShowWindow(m_btnUnderline.GetHwnd(), bFormat ? SW_SHOW : SW_HIDE);
			
			ShowWindow(m_btnColor.GetHwnd(), bFormat ? SW_SHOW : SW_HIDE);
			ShowWindow(m_btnBkColor.GetHwnd(), bFormat ? SW_SHOW : SW_HIDE);
			ShowWindow(m_btnHistory.GetHwnd(), bControl ? SW_SHOW : SW_HIDE);
			ShowWindow(m_btnNickList.GetHwnd(), bControl ? SW_SHOW : SW_HIDE);
			ShowWindow(m_btnFilter.GetHwnd(), bControl ? SW_SHOW : SW_HIDE);
			ShowWindow(m_btnChannelMgr.GetHwnd(), bControl ? SW_SHOW : SW_HIDE);
			ShowWindow(m_btnOk.GetHwnd(), bSend ? SW_SHOW : SW_HIDE);
			ShowWindow(GetDlgItem(m_hwnd, IDC_SPLITTERX), bNick ? SW_SHOW : SW_HIDE);
			if (m_si->iType != GCW_SERVER)
				ShowWindow(m_nickList.GetHwnd(), m_bNicklistEnabled ? SW_SHOW : SW_HIDE);
			else
				ShowWindow(m_nickList.GetHwnd(), SW_HIDE);

			if (m_si->iType == GCW_SERVER) {
				EnableWindow(m_btnNickList.GetHwnd(), false);
				EnableWindow(m_btnFilter.GetHwnd(), false);
				EnableWindow(m_btnChannelMgr.GetHwnd(), false);
			}
			else {
				EnableWindow(m_btnNickList.GetHwnd(), true);
				EnableWindow(m_btnFilter.GetHwnd(), true);
				if (m_si->iType == GCW_CHATROOM)
					EnableWindow(m_btnChannelMgr.GetHwnd(), pci->MM_FindModule(m_si->pszModule)->bChanMgr);
			}

			CSrmmBaseDialog::DlgProc(uMsg, wParam, lParam); // call built-in resizer
			SetButtonsPos(m_hwnd, true);

			InvalidateRect(m_hwndStatus, nullptr, true);
			RedrawWindow(m_message.GetHwnd(), nullptr, nullptr, RDW_INVALIDATE);
			RedrawWindow(m_btnOk.GetHwnd(), nullptr, nullptr, RDW_INVALIDATE);
			SaveWindowPosition(false);
		}
		return TRUE;

	case WM_CTLCOLORLISTBOX:
		SetBkColor((HDC)wParam, g_Settings.crUserListBGColor);
		return (INT_PTR)pci->hListBkgBrush;

	case WM_MEASUREITEM:
		{
			MEASUREITEMSTRUCT *mis = (MEASUREITEMSTRUCT *)lParam;
			if (mis->CtlType == ODT_MENU)
				return Menu_MeasureItem(lParam);

			int ih = GetTextPixelSize(L"AQGgl'", g_Settings.UserListFont, FALSE);
			int ih2 = GetTextPixelSize(L"AQGg'", g_Settings.UserListHeadingsFont, FALSE);
			int font = ih > ih2 ? ih : ih2;
			int height = db_get_b(0, CHAT_MODULE, "NicklistRowDist", 12);

			// make sure we have space for icon!
			if (g_Settings.bShowContactStatus)
				font = font > 16 ? font : 16;

			mis->itemHeight = height > font ? height : font;
		}
		return TRUE;

	case WM_DRAWITEM:
		{
			DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lParam;
			if (dis->CtlType == ODT_MENU)
				return Menu_DrawItem(lParam);

			if (dis->CtlID == IDC_SRMM_NICKLIST) {
				int index = dis->itemID;
				USERINFO *ui = pci->SM_GetUserFromIndex(m_si->ptszID, m_si->pszModule, index);
				if (ui) {
					int x_offset = 2;

					int height = dis->rcItem.bottom - dis->rcItem.top;
					if (height & 1)
						height++;

					int offset = (height == 10) ? 0 : height / 2 - 4;
					HFONT hFont = (ui->iStatusEx == 0) ? g_Settings.UserListFont : g_Settings.UserListHeadingsFont;
					HFONT hOldFont = (HFONT)SelectObject(dis->hDC, hFont);
					SetBkMode(dis->hDC, TRANSPARENT);

					if (dis->itemAction == ODA_FOCUS && dis->itemState & ODS_SELECTED)
						FillRect(dis->hDC, &dis->rcItem, pci->hListSelectedBkgBrush);
					else //if (dis->itemState & ODS_INACTIVE)
						FillRect(dis->hDC, &dis->rcItem, pci->hListBkgBrush);

					if (g_Settings.bShowContactStatus && g_Settings.bContactStatusFirst && ui->ContactStatus) {
						HICON hIcon = Skin_LoadProtoIcon(m_si->pszModule, ui->ContactStatus);
						DrawIconEx(dis->hDC, x_offset, dis->rcItem.top + offset - 3, hIcon, 16, 16, 0, nullptr, DI_NORMAL);
						x_offset += 18;
					}
					DrawIconEx(dis->hDC, x_offset, dis->rcItem.top + offset, pci->SM_GetStatusIcon(m_si, ui), 10, 10, 0, nullptr, DI_NORMAL);
					x_offset += 12;
					if (g_Settings.bShowContactStatus && !g_Settings.bContactStatusFirst && ui->ContactStatus) {
						HICON hIcon = Skin_LoadProtoIcon(m_si->pszModule, ui->ContactStatus);
						DrawIconEx(dis->hDC, x_offset, dis->rcItem.top + offset - 3, hIcon, 16, 16, 0, nullptr, DI_NORMAL);
						x_offset += 18;
					}

					SetTextColor(dis->hDC, ui->iStatusEx == 0 ? g_Settings.crUserListColor : g_Settings.crUserListHeadingsColor);
					TextOut(dis->hDC, dis->rcItem.left + x_offset, dis->rcItem.top, ui->pszNick, (int)mir_wstrlen(ui->pszNick));
					SelectObject(dis->hDC, hOldFont);
				}
				return TRUE;
			}
		}
		break;

	case WM_TIMER:
		if (wParam == TIMERID_FLASHWND)
			FlashWindow(m_hwnd, TRUE);
		break;

	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE) {
			if (g_Settings.bTabsEnable) {
				m_si->wState &= ~GC_EVENT_HIGHLIGHT;
				m_si->wState &= ~STATE_TALK;
				pDialog->FixTabIcons(nullptr);
			}
			break;
		}
		if (LOWORD(wParam) != WA_ACTIVE)
			break;
		// fall through

	case WM_MOUSEACTIVATE:
		{
			WINDOWPLACEMENT wp = {};
			wp.length = sizeof(wp);
			GetWindowPlacement(m_hwnd, &wp);
			g_Settings.iX = wp.rcNormalPosition.left;
			g_Settings.iY = wp.rcNormalPosition.top;
			g_Settings.iWidth = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
			g_Settings.iHeight = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;

			if (uMsg != WM_ACTIVATE)
				SetFocus(m_message.GetHwnd());

			pci->SetActiveSession(m_si);

			if (KillTimer(m_hwnd, TIMERID_FLASHWND))
				FlashWindow(m_hwnd, FALSE);
			if (db_get_w(m_hContact, m_si->pszModule, "ApparentMode", 0) != 0)
				db_set_w(m_hContact, m_si->pszModule, "ApparentMode", 0);
			if (pcli->pfnGetEvent(m_hContact, 0))
				pcli->pfnRemoveEvent(m_hContact, GC_FAKE_EVENT);
		}
		break;

	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->code) {
		case EN_MSGFILTER:
			if (((LPNMHDR)lParam)->idFrom == IDC_SRMM_LOG && ((MSGFILTER *)lParam)->msg == WM_RBUTTONUP) {
				ENLINK *pLink = (ENLINK*)lParam;
				POINT pt = { GET_X_LPARAM(pLink->lParam), GET_Y_LPARAM(pLink->lParam) };
				ClientToScreen(((LPNMHDR)lParam)->hwndFrom, &pt);

				// fixing stuff for searches
				POINTL ptl = { (LONG)pt.x, (LONG)pt.y };
				ScreenToClient(m_log.GetHwnd(), (LPPOINT)&ptl);
				long iCharIndex = m_log.SendMsg(EM_CHARFROMPOS, 0, (LPARAM)&ptl);
				if (iCharIndex < 0)
					break;

				long start = m_log.SendMsg(EM_FINDWORDBREAK, WB_LEFT, iCharIndex);//-iChars;
				long end = m_log.SendMsg(EM_FINDWORDBREAK, WB_RIGHT, iCharIndex);//-iChars;

				wchar_t pszWord[4096]; pszWord[0] = '\0';
				if (end - start > 0) {
					TEXTRANGE tr;
					tr.lpstrText = pszWord;
					tr.chrg.cpMin = start;
					tr.chrg.cpMax = end;
					long iRes = m_log.SendMsg(EM_GETTEXTRANGE, 0, (LPARAM)&tr);
					if (pszWord[0] == 0)
						break;
					if (iRes > 0)
						for (size_t iLen = mir_wstrlen(pszWord) - 1; wcschr(szTrimString, pszWord[iLen]); iLen--)
							pszWord[iLen] = 0;
				}

				CHARRANGE all = { 0, -1 };
				HMENU hMenu = 0;
				UINT uID = CreateGCMenu(m_hwnd, &hMenu, 1, pt, m_si, nullptr, pszWord);
				switch (uID) {
				case 0:
					PostMessage(m_hwnd, WM_MOUSEACTIVATE, 0, 0);
					break;

				case ID_COPYALL:
					SendMessage(((LPNMHDR)lParam)->hwndFrom, EM_EXGETSEL, 0, (LPARAM)&sel);
					SendMessage(((LPNMHDR)lParam)->hwndFrom, EM_EXSETSEL, 0, (LPARAM)&all);
					SendMessage(((LPNMHDR)lParam)->hwndFrom, WM_COPY, 0, 0);
					SendMessage(((LPNMHDR)lParam)->hwndFrom, EM_EXSETSEL, 0, (LPARAM)&sel);
					PostMessage(m_hwnd, WM_MOUSEACTIVATE, 0, 0);
					break;

				case ID_CLEARLOG:
					s = pci->SM_FindSession(m_si->ptszID, m_si->pszModule);
					if (s) {
						ClearLog();
						pci->LM_RemoveAll(&s->pLog, &s->pLogEnd);
						s->iEventCount = 0;
						s->LastTime = 0;
						m_si->iEventCount = 0;
						m_si->LastTime = 0;
						m_si->pLog = s->pLog;
						m_si->pLogEnd = s->pLogEnd;
						PostMessage(m_hwnd, WM_MOUSEACTIVATE, 0, 0);
					}
					break;

				case ID_SEARCH_GOOGLE:
					if (pszWord[0])
						Utils_OpenUrlW(CMStringW(FORMAT, L"http://www.google.com/search?q=%s", pszWord));

					PostMessage(m_hwnd, WM_MOUSEACTIVATE, 0, 0);
					break;

				case ID_SEARCH_WIKIPEDIA:
					if (pszWord[0])
						Utils_OpenUrlW(CMStringW(FORMAT, L"http://en.wikipedia.org/wiki/%s", pszWord));

					PostMessage(m_hwnd, WM_MOUSEACTIVATE, 0, 0);
					break;

				default:
					PostMessage(m_hwnd, WM_MOUSEACTIVATE, 0, 0);
					DoEventHook(GC_USER_LOGMENU, nullptr, nullptr, uID);
					break;
				}
				DestroyGCMenu(&hMenu, 5);
			}
			break;

		case TTN_NEEDTEXT:
			if (((LPNMHDR)lParam)->idFrom == (UINT_PTR)m_nickList.GetHwnd()) {
				LPNMTTDISPINFO lpttd = (LPNMTTDISPINFO)lParam;
				SESSION_INFO* parentdat = (SESSION_INFO*)GetWindowLongPtr(m_hwnd, GWLP_USERDATA);
				POINT p;
				GetCursorPos(&p);
				ScreenToClient(m_nickList.GetHwnd(), &p);
				int item = LOWORD(m_nickList.SendMsg(LB_ITEMFROMPOINT, 0, MAKELPARAM(p.x, p.y)));
				USERINFO *ui = pci->SM_GetUserFromIndex(parentdat->ptszID, parentdat->pszModule, item);
				if (ui != nullptr) {
					static wchar_t ptszBuf[1024];
					mir_snwprintf(ptszBuf, L"%s: %s\r\n%s: %s\r\n%s: %s",
						TranslateT("Nickname"), ui->pszNick,
						TranslateT("Unique ID"), ui->pszUID,
						TranslateT("Status"), pci->TM_WordToString(parentdat->pStatuses, ui->Status));
					lpttd->lpszText = ptszBuf;
				}
			}
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_SRMM_MESSAGE:
			EnableWindow(m_btnOk.GetHwnd(), GetRichTextLength(m_message.GetHwnd()) != 0);
			break;
		}
		break;

	case WM_KEYDOWN:
		SetFocus(m_message.GetHwnd());
		break;

	case WM_MOVE:
		SaveWindowPosition(false);
		break;

	case WM_GETMINMAXINFO:
		{
			MINMAXINFO *mmi = (MINMAXINFO*)lParam;
			mmi->ptMinTrackSize.x = m_iSplitterX + 43;
			if (mmi->ptMinTrackSize.x < 350)
				mmi->ptMinTrackSize.x = 350;

			mmi->ptMinTrackSize.y = m_iSplitterY + 80;
		}
		break;

	case WM_LBUTTONDBLCLK:
		if (LOWORD(lParam) < 30)
			ScrollToBottom();
		break;
	}
	
	return CSrmmBaseDialog::DlgProc(uMsg, wParam, lParam);
}

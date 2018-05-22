/*
Miranda Crash Dumper Plugin
Copyright (C) 2008 - 2012 Boris Krasnovskiy All Rights Reserved

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2
of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "stdafx.h"

CMPlugin g_plugin;

DWORD mirandaVersion;
LCID packlcid;
//HANDLE hCrashLogFolder, hVerInfoFolder;
HANDLE hVerInfoFolder;
HMODULE hMsftedit;

wchar_t* vertxt;
wchar_t* profname;
wchar_t* profpath;

wchar_t CrashLogFolder[MAX_PATH], VersionInfoFolder[MAX_PATH];

bool servicemode, clsdates, dtsubfldr, catchcrashes, needrestart = 0;

extern HWND hViewWnd;

/////////////////////////////////////////////////////////////////////////////////////////

PLUGININFOEX pluginInfoEx = {
	sizeof(PLUGININFOEX),
	__PLUGIN_NAME,
	PLUGIN_MAKE_VERSION(__MAJOR_VERSION, __MINOR_VERSION, __RELEASE_NUM, __BUILD_NUM),
	__DESCRIPTION,
	__AUTHOR,
	__COPYRIGHT,
	__AUTHORWEB,
	UNICODE_AWARE,
	// {F62C1D7A-FFA4-4065-A251-4C9DD9101CC8}
	{ 0xf62c1d7a, 0xffa4, 0x4065, { 0xa2, 0x51, 0x4c, 0x9d, 0xd9, 0x10, 0x1c, 0xc8 } }
};

CMPlugin::CMPlugin() :
	PLUGIN<CMPlugin>(MODULENAME, pluginInfoEx)
{}

extern "C" __declspec(dllexport) PLUGININFOEX* MirandaPluginInfoEx(DWORD mirVersion)
{
	::mirandaVersion = mirVersion;
	return (PLUGININFOEX*)&pluginInfoEx;
}

/////////////////////////////////////////////////////////////////////////////////////////
// MirandaInterfaces - returns the protocol interface to the core

extern "C" __declspec(dllexport) const MUUID MirandaInterfaces[] = { MIID_SERVICEMODE, MIID_LAST };

/////////////////////////////////////////////////////////////////////////////////////////

INT_PTR StoreVersionInfoToFile(WPARAM, LPARAM lParam)
{
	CreateDirectoryTreeW(VersionInfoFolder);

	wchar_t path[MAX_PATH];
	mir_snwprintf(path, L"%s\\VersionInfo.txt", VersionInfoFolder);

	HANDLE hDumpFile = CreateFile(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hDumpFile != INVALID_HANDLE_VALUE) {
		CMStringW buffer;
		PrintVersionInfo(buffer, (unsigned int)lParam | VI_FLAG_PRNVAR);

		WriteUtfFile(hDumpFile, T2Utf(buffer.c_str()));
		CloseHandle(hDumpFile);

		ShowMessage(3, TranslateT("Version Info stored into file %s"), path);
	}
	else ShowMessage(2, TranslateT("Version Info file %s is inaccessible"), path);

	return 0;
}

INT_PTR StoreVersionInfoToClipboard(WPARAM, LPARAM lParam)
{
	CMStringW buffer;
	WriteBBFile(buffer, true);
	PrintVersionInfo(buffer, (unsigned int)lParam | VI_FLAG_PRNVAR | VI_FLAG_FORMAT);
	WriteBBFile(buffer, false);

	StoreStringToClip(buffer);

	return 0;
}

INT_PTR UploadVersionInfo(WPARAM, LPARAM lParam)
{
	CMStringW buffer;
	PrintVersionInfo(buffer);

	VerTrnsfr *trn = (VerTrnsfr*)mir_alloc(sizeof(VerTrnsfr));
	trn->buf = mir_utf8encodeW(buffer.c_str());
	trn->autot = lParam == 0xa1;

	mir_forkthread(VersionInfoUploadThread, trn);

	return 0;
}


INT_PTR ViewVersionInfo(WPARAM wParam, LPARAM)
{
	if (hViewWnd) {
		SetForegroundWindow(hViewWnd);
		SetFocus(hViewWnd);
	}
	else {
		DWORD dwFlags = wParam ? (VI_FLAG_PRNVAR | VI_FLAG_PRNDLL) : VI_FLAG_PRNVAR;
		hViewWnd = CreateDialogParam(g_plugin.getInst(), MAKEINTRESOURCE(IDD_VIEWVERSION), nullptr, DlgProcView, dwFlags);
	}

	return 0;
}

INT_PTR GetVersionInfo(WPARAM wParam, LPARAM lParam)
{
	int result = 1; //failure
	if (lParam != NULL) {
		CMStringW buffer;
		PrintVersionInfo(buffer, (unsigned int)wParam);
		char **retData = (char **)lParam;
		*retData = mir_utf8encodeW(buffer.c_str());
		if (*retData)
			result = 0; //success
	}
	return result;
}

INT_PTR OpenUrl(WPARAM wParam, LPARAM)
{
	switch (wParam) {
	case 0:
		ShellExecute(nullptr, L"explore", CrashLogFolder, nullptr, nullptr, SW_SHOW);
		break;

	case 1:
		OpenAuthUrl("https://vi.miranda-ng.org/detail/%s");
		break;
	}
	return 0;
}

INT_PTR CopyLinkToClipboard(WPARAM, LPARAM)
{
	ptrW tmp(db_get_wsa(NULL, MODULENAME, "Username"));
	if (tmp != NULL) {
		wchar_t buffer[MAX_PATH];
		mir_snwprintf(buffer, L"https://vi.miranda-ng.org/detail/%s", tmp);

		int bufLen = (sizeof(buffer) + 1) * sizeof(wchar_t);
		HANDLE hData = GlobalAlloc(GMEM_MOVEABLE, bufLen);
		LPSTR buf = (LPSTR)GlobalLock(hData);
		memcpy(buf, buffer, bufLen);

		OpenClipboard(nullptr);
		EmptyClipboard();

		if (SetClipboardData(CF_UNICODETEXT, hData) == nullptr)
			GlobalFree(hData);
		CloseClipboard();
	}
	return 0;
}

INT_PTR ServiceModeLaunch(WPARAM, LPARAM)
{
	servicemode = true;
	ViewVersionInfo(1, 0);
	return SERVICE_ONLYDB;
}

static int FoldersPathChanged(WPARAM, LPARAM)
{
	FOLDERSGETDATA fgd = {};
	fgd.cbSize = sizeof(FOLDERSGETDATA);
	fgd.nMaxPathSize = MAX_PATH;
	fgd.flags = FF_TCHAR;
	//	fgd.szPathT = CrashLogFolder;
	//	CallService(MS_FOLDERS_GET_PATH, (WPARAM)hCrashLogFolder, (LPARAM)&fgd);

	fgd.szPathT = VersionInfoFolder;
	CallService(MS_FOLDERS_GET_PATH, (WPARAM)hVerInfoFolder, (LPARAM)&fgd);
	return 0;
}

int OptionsInit(WPARAM wParam, LPARAM)
{
	OPTIONSDIALOGPAGE odp = {};
	odp.position = -790000000;
	odp.hInstance = g_plugin.getInst();
	odp.pszTemplate = MAKEINTRESOURCEA(IDD_OPTIONS);
	odp.szTitle.a = MODULENAME;
	odp.szGroup.a = LPGEN("Services");
	odp.flags = ODPF_BOLDGROUPS;
	odp.pfnDlgProc = DlgProcOptions;
	g_plugin.addOptions(wParam, &odp);
	return 0;
}

static int ToolbarModulesLoaded(WPARAM, LPARAM)
{
	TTBButton ttb = {};
	ttb.pszService = MS_CRASHDUMPER_STORETOCLIP;
	ttb.name = ttb.pszTooltipUp = LPGEN("Version Information To Clipboard");
	ttb.hIconHandleUp = GetIconHandle(IDI_VITOCLIP);
	ttb.dwFlags = TTBBF_VISIBLE;
	g_plugin.addTTB(&ttb);

	ttb.pszService = MS_CRASHDUMPER_STORETOFILE;
	ttb.name = ttb.pszTooltipUp = LPGEN("Version Information To File");
	ttb.hIconHandleUp = GetIconHandle(IDI_VITOFILE);
	ttb.dwFlags = 0;
	g_plugin.addTTB(&ttb);

	ttb.pszService = MS_CRASHDUMPER_VIEWINFO;
	ttb.name = ttb.pszTooltipUp = LPGEN("Show Version Information");
	ttb.hIconHandleUp = GetIconHandle(IDI_VISHOW);
	g_plugin.addTTB(&ttb);

	ttb.pszService = MS_CRASHDUMPER_UPLOAD;
	ttb.name = ttb.pszTooltipUp = LPGEN("Upload Version Information");
	ttb.hIconHandleUp = GetIconHandle(IDI_VIUPLOAD);
	g_plugin.addTTB(&ttb);
	return 0;
}

static int ModulesLoaded(WPARAM, LPARAM)
{
	char temp[MAX_PATH];
	Miranda_GetVersionText(temp, _countof(temp));
	crs_a2t(vertxt, temp);

	if (ServiceExists(MS_FOLDERS_REGISTER_PATH)) {
		replaceStrW(profpath, L"%miranda_userdata%");

		// Removed because it isn't available on Load()
		//		hCrashLogFolder = FoldersRegisterCustomPathT(MODULENAME, LPGEN("Crash Reports"), CrashLogFolder);
		hVerInfoFolder = FoldersRegisterCustomPathT(MODULENAME, LPGEN("Version Information"), VersionInfoFolder);

		HookEvent(ME_FOLDERS_PATH_CHANGED, FoldersPathChanged);
		FoldersPathChanged(0, 0);
	}

	CMenuItem mi(g_plugin);
	mi.root = g_plugin.addRootMenu(MO_MAIN, LPGENW("Version Information"), 2000089999, GetIconHandle(IDI_VI));
	Menu_ConfigureItem(mi.root, MCI_OPT_UID, "9A7A9C76-7FD8-4C05-B402-6C46060C2D78");

	SET_UID(mi, 0x52930e40, 0xb2ee, 0x4433, 0xad, 0x77, 0xf5, 0x42, 0xe, 0xf6, 0x57, 0xc1);
	mi.position = 2000089995;
	mi.name.a = LPGEN("Copy to clipboard");
	mi.hIcolibItem = GetIconHandle(IDI_VITOCLIP);
	mi.pszService = MS_CRASHDUMPER_STORETOCLIP;
	Menu_AddMainMenuItem(&mi);

	SET_UID(mi, 0x54109094, 0x494e, 0x4535, 0x9c, 0x3a, 0xf6, 0x9e, 0x9a, 0xf7, 0xcd, 0xbe);
	mi.position = 2000089996;
	mi.name.a = LPGEN("Store to file");
	mi.hIcolibItem = GetIconHandle(IDI_VITOFILE);
	mi.pszService = MS_CRASHDUMPER_STORETOFILE;
	Menu_AddMainMenuItem(&mi);

	SET_UID(mi, 0x4004f9ee, 0x2c5a, 0x420a, 0xb1, 0x54, 0x3e, 0x47, 0xc1, 0xde, 0x46, 0xec);
	mi.position = 2000089997;
	mi.name.a = LPGEN("Show");
	mi.hIcolibItem = GetIconHandle(IDI_VISHOW);
	mi.pszService = MS_CRASHDUMPER_VIEWINFO;
	Menu_AddMainMenuItem(&mi);

	SET_UID(mi, 0x8526469a, 0x8ab4, 0x4dd4, 0xad, 0xbf, 0x51, 0xfd, 0x71, 0x10, 0xd3, 0x3c);
	mi.position = 2000089998;
	mi.name.a = LPGEN("Show with DLLs");
	mi.hIcolibItem = GetIconHandle(IDI_VIUPLOAD);
	mi.pszService = MS_CRASHDUMPER_VIEWINFO;
	Menu_ConfigureItem(Menu_AddMainMenuItem(&mi), MCI_OPT_EXECPARAM, 1);

	SET_UID(mi, 0xc6e3b558, 0xe1e8, 0x4cce, 0x96, 0x8, 0xc6, 0x89, 0x1b, 0x79, 0xf3, 0x7e);
	mi.position = 2000089999;
	mi.name.a = LPGEN("Upload");
	mi.hIcolibItem = GetIconHandle(IDI_VIUPLOAD);
	mi.pszService = MS_CRASHDUMPER_UPLOAD;
	Menu_AddMainMenuItem(&mi);

	SET_UID(mi, 0xa23da95a, 0x7624, 0x4343, 0x8c, 0xc0, 0xa6, 0x16, 0xbc, 0x30, 0x13, 0x8c);
	mi.position = 2000089999;
	mi.name.a = LPGEN("Copy link to clipboard");
	mi.hIcolibItem = GetIconHandle(IDI_LINKTOCLIP);//need icon
	mi.pszService = MS_CRASHDUMPER_URLTOCLIP;
	Menu_AddMainMenuItem(&mi);

	if (catchcrashes && !needrestart) {
		SET_UID(mi, 0xecae52f2, 0xd601, 0x4f85, 0x87, 0x9, 0xec, 0x8e, 0x84, 0xfe, 0x1b, 0x3c);
		mi.position = 2000099990;
		mi.name.a = LPGEN("Open crash report directory");
		mi.hIcolibItem = Skin_GetIconHandle(SKINICON_EVENT_FILE);
		mi.pszService = MS_CRASHDUMPER_URL;
		Menu_AddMainMenuItem(&mi);
	}

	SET_UID(mi, 0x6b19be3, 0xfb7d, 0x457d, 0x85, 0xde, 0xe0, 0x26, 0x4c, 0x87, 0x35, 0xf4);
	mi.position = 2000099991;
	mi.name.a = LPGEN("Open online Version Info");
	mi.hIcolibItem = Skin_GetIconHandle(SKINICON_EVENT_URL);
	mi.pszService = MS_CRASHDUMPER_URL;
	Menu_ConfigureItem(Menu_AddMainMenuItem(&mi), MCI_OPT_EXECPARAM, 1);

	HOTKEYDESC hk = {};
	hk.szSection.a = MODULENAME;

	hk.szDescription.a = LPGEN("Copy Version Info to clipboard");
	hk.pszName = "CopyVerInfo";
	hk.pszService = MS_CRASHDUMPER_STORETOCLIP;
	g_plugin.addHotkey(&hk);

	hk.szDescription.a = LPGEN("Show Version Info");
	hk.pszName = "ShowVerInfo";
	hk.pszService = MS_CRASHDUMPER_VIEWINFO;
	g_plugin.addHotkey(&hk);

	UploadInit();

	if (catchcrashes && !needrestart)
		SetExceptionHandler();

	HookEvent(ME_TTB_MODULELOADED, ToolbarModulesLoaded);

	if (servicemode)
		ViewVersionInfo(0, 0);
	else if (db_get_b(NULL, MODULENAME, "UploadChanged", 0) && !ProcessVIHash(false))
		UploadVersionInfo(0, 0xa1);

	return 0;
}

static int PreShutdown(WPARAM, LPARAM)
{
	DestroyAllWindows();
	UploadClose();
	return 0;
}

extern "C" int __declspec(dllexport) Load(void)
{
	hMsftedit = LoadLibrary(L"Msftedit.dll");
	if (hMsftedit == nullptr)
		return 1;

	clsdates = db_get_b(NULL, MODULENAME, "ClassicDates", 1) != 0;
	dtsubfldr = db_get_b(NULL, MODULENAME, "SubFolders", 1) != 0;
	catchcrashes = db_get_b(NULL, MODULENAME, "CatchCrashes", 1) != 0;

	profname = Utils_ReplaceVarsW(L"%miranda_profilename%.dat");
	profpath = Utils_ReplaceVarsW(L"%miranda_userdata%");
	if (catchcrashes && !needrestart)
		mir_snwprintf(CrashLogFolder, L"%s\\CrashLog", profpath);
	wcsncpy_s(VersionInfoFolder, profpath, _TRUNCATE);

	HookEvent(ME_SYSTEM_MODULESLOADED, ModulesLoaded);
	HookEvent(ME_OPT_INITIALISE, OptionsInit);
	HookEvent(ME_SYSTEM_PRESHUTDOWN, PreShutdown);

	packlcid = (LCID)Langpack_GetDefaultLocale();

	InitIcons();

	if (catchcrashes && !needrestart)
		InitExceptionHandler();

	CreateServiceFunction(MS_CRASHDUMPER_STORETOFILE, StoreVersionInfoToFile);
	CreateServiceFunction(MS_CRASHDUMPER_STORETOCLIP, StoreVersionInfoToClipboard);
	CreateServiceFunction(MS_CRASHDUMPER_VIEWINFO, ViewVersionInfo);
	CreateServiceFunction(MS_CRASHDUMPER_GETINFO, GetVersionInfo);
	CreateServiceFunction(MS_CRASHDUMPER_UPLOAD, UploadVersionInfo);
	CreateServiceFunction(MS_CRASHDUMPER_URL, OpenUrl);
	CreateServiceFunction(MS_SERVICEMODE_LAUNCH, ServiceModeLaunch);
	CreateServiceFunction(MS_CRASHDUMPER_URLTOCLIP, CopyLinkToClipboard);
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

extern "C" int __declspec(dllexport) Unload(void)
{
	DestroyAllWindows();

	if ((catchcrashes && !needrestart) || (!catchcrashes && needrestart))
		DestroyExceptionHandler();

	mir_free(profpath);
	mir_free(profname);
	mir_free(vertxt);
	FreeLibrary(hMsftedit);
	return 0;
}

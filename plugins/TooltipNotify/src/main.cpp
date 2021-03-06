/*/////////////////////////////////////////////
	Tooltip Notify plugin for Miranda IM
*//////////////////////////////////////////////

#include "stdafx.h"

// Main global object
static CTooltipNotify *g_pTooltipNotify = nullptr;

CMPlugin g_plugin;

/////////////////////////////////////////////////////////////////////////////////////////

static PLUGININFOEX pluginInfoEx =
{
	sizeof(PLUGININFOEX),
	__PLUGIN_NAME,
	PLUGIN_MAKE_VERSION(__MAJOR_VERSION, __MINOR_VERSION, __RELEASE_NUM, __BUILD_NUM),
	__DESCRIPTION,
	__AUTHOR,
	__COPYRIGHT,
	__AUTHORWEB,
	UNICODE_AWARE,
	// {5906A545-F31A-4726-B48F-03A09F060318}
	{0x5906a545, 0xf31a, 0x4726, {0xb4, 0x8f, 0x3, 0xa0, 0x9f, 0x6, 0x3, 0x18}}
};

CMPlugin::CMPlugin() :
	PLUGIN<CMPlugin>(MODULENAME, pluginInfoEx)
{}

/////////////////////////////////////////////////////////////////////////////////////////

static int ProtoContactIsTyping(WPARAM wParam, LPARAM lParam)
{
	return CTooltipNotify::GetObjInstance()->ProtoContactIsTyping(wParam, lParam);
}

static int ProtoAck(WPARAM wParam, LPARAM lParam)
{
	return CTooltipNotify::GetObjInstance()->ProtoAck(wParam, lParam);
}

static int ContactSettingChanged(WPARAM wParam, LPARAM lParam)
{
	return CTooltipNotify::GetObjInstance()->ContactSettingChanged(wParam, lParam);
}

static int InitializeOptions(WPARAM wParam, LPARAM lParam)
{
	return CTooltipNotify::GetObjInstance()->InitializeOptions(wParam, lParam);
}

static int ModulesLoaded(WPARAM wParam, LPARAM lParam)
{
	return CTooltipNotify::GetObjInstance()->ModulesLoaded(wParam, lParam);
}

int CMPlugin::Load()
{
	g_pTooltipNotify = new CTooltipNotify();
	assert(g_pTooltipNotify != nullptr);
	
	HookEvent(ME_DB_CONTACT_SETTINGCHANGED, ContactSettingChanged);
	HookEvent(ME_OPT_INITIALISE, InitializeOptions);
	HookEvent(ME_PROTO_ACK, ProtoAck);
	HookEvent(ME_PROTO_CONTACTISTYPING, ProtoContactIsTyping);
	HookEvent(ME_SYSTEM_MODULESLOADED, ModulesLoaded);
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

int CMPlugin::Unload()
{
	delete g_pTooltipNotify;
	return 0;
}

/*
Copyright (c) 2013-18 Miranda NG team (https://miranda-ng.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2
of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "stdafx.h"
#include "version.h"

CMPlugin g_plugin;
CLIST_INTERFACE *pcli;

/////////////////////////////////////////////////////////////////////////////////////////

PLUGININFOEX pluginInfoEx =
{
	sizeof(PLUGININFOEX),
	__PLUGIN_NAME,
	PLUGIN_MAKE_VERSION(__MAJOR_VERSION,__MINOR_VERSION,__RELEASE_NUM,__BUILD_NUM),
	__DESCRIPTION,
	__AUTHOR,
	__COPYRIGHT,
	__AUTHORWEB,
	UNICODE_AWARE,
	// {32579908-724B-467F-ADAD-22B6359A749A}
	{ 0x32579908, 0x724b, 0x467f, {0xad, 0xad, 0x22, 0xb6, 0x35, 0x9a, 0x74, 0x9a}}
};

CMPlugin::CMPlugin() :
	ACCPROTOPLUGIN<CVkProto>("VKontakte", pluginInfoEx)
{
	SetUniqueId("ID");
}

extern "C" __declspec(dllexport) PLUGININFOEX* MirandaPluginInfoEx(DWORD)
{
	return &pluginInfoEx;
}

/////////////////////////////////////////////////////////////////////////////////////////

extern "C" __declspec(dllexport) const MUUID MirandaInterfaces[] = { MIID_PROTOCOL, MIID_LAST };

/////////////////////////////////////////////////////////////////////////////////////////
// OnLoad - initialize the plugin instance

extern "C" int __declspec(dllexport) Load()
{
	pcli = Clist_GetInterface();

	InitIcons();
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Unload - destroy the plugin instance

extern "C" int __declspec(dllexport) Unload(void)
{
	return 0;
}

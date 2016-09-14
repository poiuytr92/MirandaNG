#include "stdafx.h"

INT_PTR IsContactPassed(WPARAM hContact, LPARAM /*lParam*/)
{
	char *szProto = GetContactProto(hContact);
	if (szProto == NULL)
		return CS_PASSED;

	if (!plSets->ProtoDisabled(szProto))
		return CS_PASSED;

	if (db_get_b(hContact, pluginName, answeredSetting, 0))
		return CS_PASSED;

	if (!db_get_b(hContact, "CList", "NotOnList", 0) && db_get_w(hContact, szProto, "SrvGroupId", -1) != 1)
		return CS_PASSED;

	if (IsExistMyMessage(hContact))
		return CS_PASSED;

	return CS_NOTPASSED;
}

INT_PTR RemoveTempContacts(WPARAM, LPARAM lParam)
{
	for (MCONTACT hContact = db_find_first(); hContact;) {
		MCONTACT hNext = db_find_next(hContact);
		ptrW szGroup(db_get_wsa(hContact, "CList", "Group"));

		if (db_get_b(hContact, "CList", "NotOnList", 0) || (szGroup != NULL && (wcsstr(szGroup, L"Not In List") || wcsstr(szGroup, TranslateT("Not In List"))))) {
			char *szProto = GetContactProto(hContact);
			if (szProto != NULL) {
				// Check if protocol uses server side lists
				DWORD caps = CallProtoService(szProto, PS_GETCAPS, PFLAGNUM_1, 0);
				if (caps & PF1_SERVERCLIST) {
					int status = CallProtoService(szProto, PS_GETSTATUS, 0, 0);
					if (status == ID_STATUS_OFFLINE || IsStatusConnecting(status))
						// Set a flag so we remember to delete the contact when the protocol goes online the next time
						db_set_b(hContact, "CList", "Delete", 1);
					else
						db_delete_contact(hContact);
				}
			}
		}

		hContact = hNext;
	}

	int hGroup = 1;
	wchar_t *group_name;
	do {
		group_name = Clist_GroupGetName(hGroup, 0);
		if (group_name != NULL && wcsstr(group_name, TranslateT("Not In List"))) {
			BYTE ConfirmDelete = db_get_b(NULL, "CList", "ConfirmDelete", SETTING_CONFIRMDELETE_DEFAULT);
			if (ConfirmDelete)
				db_set_b(NULL, "CList", "ConfirmDelete", 0);

			Clist_GroupDelete(hGroup);
			if (ConfirmDelete)
				db_set_b(NULL, "CList", "ConfirmDelete", ConfirmDelete);
			break;
		}
		hGroup++;
	} while (group_name);
	if (!lParam)
		MessageBox(NULL, TranslateT("Complete"), TranslateT(pluginName), MB_ICONINFORMATION);

	return 0;
}

int OnSystemModulesLoaded(WPARAM, LPARAM)
{
	UnhookEvent(hLoadHook);
	if (plSets->RemTmpAll.Get())
		RemoveTempContacts(0, 1);
	return 0;
}

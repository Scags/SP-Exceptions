/**
 * vim: set ts=4 :
 * =============================================================================
 * SourceMod Sample Extension
 * Copyright (C) 2004-2008 AlliedModders LLC.  All rights reserved.
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, AlliedModders LLC gives you permission to link the
 * code of this program (as well as its derivative works) to "Half-Life 2," the
 * "Source Engine," the "SourcePawn JIT," and any Game MODs that run on software
 * by the Valve Corporation.  You must obey the GNU General Public License in
 * all respects for all other code used.  Additionally, AlliedModders LLC grants
 * this exception to all derivative works.  AlliedModders LLC defines further
 * exceptions, found in LICENSE.txt (as of this writing, version JULY-31-2007),
 * or <http://www.sourcemod.net/license.php>.
 *
 * Version: $Id$
 */

#include "extension.h"
#include "natives.h"
#include "exception.h"

SPExceptions g_SPExc;		/**< Global singleton for extension's main interface */
HandleType_t g_ExceptionHandle = NO_HANDLE_TYPE;

bool SPExceptions::SDK_OnLoad(char *error, size_t maxlen, bool late)
{
	sharesys->RegisterLibrary(myself, "SP-Exceptions");

	HandleError err;
	g_ExceptionHandle = handlesys->CreateType("Exception", this, 0, NULL, NULL, myself->GetIdentity(), &err);
	if (g_ExceptionHandle == BAD_HANDLE)
	{
		snprintf(error, maxlen, "Could not create Exception handle (err: %d)", err);
		return false;
	}

	sharesys->AddNatives(myself, g_Natives);
	return true;
}

void SPExceptions::SDK_OnUnload()
{
	
}

void SPExceptions::OnHandleDestroy(HandleType_t type, void *object)
{
	if (type == g_ExceptionHandle)
		delete (Exception *)object;
}

SMEXT_LINK(&g_SPExc);

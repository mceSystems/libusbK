/*!
#
# Copyright (c) 2012 Travis Robinson <libusbdotnet@gmail.com>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
# IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL TRAVIS LEE ROBINSON
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.
#
*/
#include "examples.h"

VOID KUSB_API OnHotPlug(
    KHOT_HANDLE Handle,
    KLST_DEVINFO_HANDLE DeviceInfo,
    KLST_SYNC_FLAG NotificationType)
{
	UNREFERENCED_PARAMETER(Handle);

	// Write arrival/removal event notifications to console output as they occur.
	printf(
	    "\n"
	    "[%s] %s (%s) [%s]\n"
	    "  InstanceID          : %s\n"
	    "  DeviceInterfaceGUID : %s\n"
	    "  DevicePath          : %s\n"
	    "  \n",
	    NotificationType == KLST_SYNC_FLAG_ADDED ? "ARRIVAL" : "REMOVAL",
	    DeviceInfo->DeviceDesc,
	    DeviceInfo->Mfg,
	    DeviceInfo->Service,
	    DeviceInfo->DeviceID,
	    DeviceInfo->DeviceInterfaceGUID,
	    DeviceInfo->DevicePath);
}

DWORD __cdecl main(int argc, char* argv[])
{
	DWORD errorCode = ERROR_SUCCESS;
	KHOT_HANDLE hotHandle = NULL;
	KHOT_PARAMS hotParams;
	CHAR chKey;

	memset(&hotParams, 0, sizeof(hotParams));
	hotParams.OnHotPlug = OnHotPlug;
	hotParams.Flags |= KHOT_FLAG_PLUG_ALL_ON_INIT;

	// A "real world" application should set a specific device interface guid if possible.
	// strcpy(hotParams.PatternMatch.DeviceInterfaceGUID, "{F676DCF6-FDFE-E0A9-FC12-8057DBE8E4B8}");
	strcpy(hotParams.PatternMatch.DeviceInterfaceGUID, "*");

	printf("Initialize a HotK device notification event monitor..\n");
	printf("Looking for 'DeviceInterfaceGUID's matching the pattern '%s'..\n", hotParams.PatternMatch.DeviceInterfaceGUID);

	// Initializes a new HotK handle.
	if (!HotK_Init(&hotHandle, &hotParams))
	{
		errorCode = GetLastError();
		printf("HotK_Init failed. ErrorCode: %08Xh\n",  errorCode);
		goto Done;
	}

	printf("HotK monitor initialized successfully!\n");
	printf("Press 'q' to exit...\n\n");

	for(;;)
	{
		if (_kbhit())
		{
			chKey = (CHAR)_getch();
			if (chKey == 'q' || chKey == 'Q')
				break;

			chKey = '\0';
			continue;
		}

		Sleep(100);
	}

	// Free the HotK handle.
	if (!HotK_Free(hotHandle))
	{
		errorCode = GetLastError();
		printf("HotK_Free failed. ErrorCode: %08Xh\n",  errorCode);
		goto Done;
	}

	printf("HotK monitor closed successfully!\n");

Done:
	return errorCode;
}

/*!
Initialize a HotK device notification event monitor..
Looking for devices with DeviceInterfaceGUIDs matching the pattern '*'..
Press 'q' to exit..

HotK monitor initialized. ErrorCode: 00000000h

[ARRIVAL] Benchmark Device (Microchip Technology, Inc.) [libusbK]
  InstanceID          : USB\VID_04D8&PID_FA2E\LUSBW1
  DeviceInterfaceGUID : {716cdf1f-418b-4b80-a07d-1311dffdc8b8}
  DevicePath          : \\?\USB#VID_04D8&PID_FA2E#LUSBW1#{716cdf1f-418b-4b80-a07d-1311dffdc8b8}


[REMOVAL] Benchmark Device (Microchip Technology, Inc.) [libusbK]
  InstanceID          : USB\VID_04D8&PID_FA2E\LUSBW1
  DeviceInterfaceGUID : {716cdf1f-418b-4b80-a07d-1311dffdc8b8}
  DevicePath          : \\?\USB#VID_04D8&PID_FA2E#LUSBW1#{716cdf1f-418b-4b80-a07d-1311dffdc8b8}

HotK monitor closed. ErrorCode: 00000000h
*/

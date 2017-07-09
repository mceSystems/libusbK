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

// Globals
KUSB_DRIVER_API Usb;

DWORD __cdecl main(int argc, char* argv[])
{
	KLST_HANDLE deviceList = NULL;
	KLST_DEVINFO_HANDLE deviceInfo = NULL;
	KUSB_HANDLE handle = NULL;
	DWORD errorCode = ERROR_SUCCESS;

	// Find the test device.  Uses "vid/pid=hhhh" arguments supplied
	// on the command line. (default is: vid=04D8 pid=FA2E)
	if (!Examples_GetTestDevice(&deviceList, &deviceInfo, argc, argv))
		return GetLastError();

	// load a dynamic driver api for this device.  The dynamic driver api
	// is more versatile because it adds support for winusb.sys devices.
	if (!LibK_LoadDriverAPI(&Usb, deviceInfo->DriverID))
	{
		errorCode = GetLastError();
		printf("LibK_LoadDriverAPI failed. ErrorCode: %08Xh\n",  errorCode);
		goto Done;
	}

	// Display some information on the driver api type.
	switch(deviceInfo->DriverID)
	{
	case KUSB_DRVID_LIBUSBK:
		printf("libusbK driver api loaded!\n");
		break;
	case KUSB_DRVID_LIBUSB0:
		printf("libusb0 driver api loaded!\n");
		break;
	case KUSB_DRVID_WINUSB:
		printf("WinUSB driver api loaded!\n");
		break;
	case KUSB_DRVID_LIBUSB0_FILTER:
		printf("libusb0/filter driver api loaded!\n");
		break;
	}

	/*
	From this point forth, do not use the exported "UsbK_" functions. Instead,
	use the functions in the driver api initialized above.
	*/

	// Initialize the device with the "dynamic" Open function
	if (!Usb.Init(&handle, deviceInfo))
	{
		errorCode = GetLastError();
		printf("Usb.Init failed. ErrorCode: %08Xh\n",  errorCode);
		goto Done;
	}
	printf("Device opened successfully!\n");

Done:
	// Close the device handle
	// if handle is invalid (NULL), has no effect
	UsbK_Free(handle);

	// Free the device list
	// if deviceList is invalid (NULL), has no effect
	LstK_Free(deviceList);

	return errorCode;
}

/*!
Console Output:

Looking for device vid/pid 04D8/FA2E..
Using 04D8:FA2E (LUSBW1): Benchmark Device - Microchip Technology, Inc.
libusbK driver api loaded!
Device opened successfully!
*/

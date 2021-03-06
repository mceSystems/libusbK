/*!********************************************************************
libusbK - kList descriptor diagnostic tool.
Copyright (C) 2012 Travis Lee Robinson. All Rights Reserved.
libusb-win32.sourceforge.net

Development : Travis Lee Robinson  (libusbdotnet@gmail.com)
Testing     : Xiaofan Chen         (xiaofanc@gmail.com)

At the discretion of the user of this library, this software may be
licensed under the terms of the GNU Public License v3 or a BSD-Style
license as outlined in the following files:
* LICENSE-gpl3.txt
* LICENSE-bsd.txt

License files are located in a license folder at the root of source and
binary distributions.
********************************************************************!*/

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <setupapi.h>

#include "lusbk_version.h"
#include "libusbk.h"
#include "UsbIds.h"

#pragma warning(disable:4296)
#pragma warning(disable:4201)
#include <PSHPACK1.h>

#define USB_HID_DESCRIPTOR_TYPE				0x21
#define USB_HID_REPORT_DESCRIPTOR_TYPE		0x22
#define USB_HID_PHYSICAL_DESCRIPTOR_TYPE	0x23

typedef struct _HID_DESCRIPTOR
{
	UCHAR   bLength;
	UCHAR   bDescriptorType;
	USHORT  bcdHID;
	UCHAR   bCountry;
	UCHAR   bNumDescriptors;

	/*
	 *  This is an array of one OR MORE descriptors.
	 */
	struct _HID_DESCRIPTOR_DESC_LIST
	{
		UCHAR   bReportType;
		USHORT  wReportLength;
	} DescriptorList [1];

}* PHID_DESCRIPTOR, HID_DESCRIPTOR;

typedef struct _UNI_DESCRIPTOR
{
	union
	{
		USB_COMMON_DESCRIPTOR Common;
		USB_DEVICE_DESCRIPTOR Device;
		USB_CONFIGURATION_DESCRIPTOR Config;
		USB_STRING_DESCRIPTOR String;
		USB_INTERFACE_DESCRIPTOR Interface;
		USB_ENDPOINT_DESCRIPTOR Endpoint;
		USB_INTERFACE_ASSOCIATION_DESCRIPTOR InterfaceAssociation;
		HID_DESCRIPTOR Hid;
	};
} UNI_DESCRIPTOR, *PUNI_DESCRIPTOR, ** PPUNI_DESCRIPTOR;
#include <POPPACK.h>
#pragma warning(default:4201)

static KUSB_DRIVER_API K;
static KUSB_HANDLE InterfaceHandle;

LONG WinError(__in_opt DWORD errorCode);

BOOL OpenDeviceFileHandle(__in LPCSTR deviceFileName,
                          __out HANDLE* fileHandle);

BOOL GetDescriptorReport(__in KLST_DEVINFO_HANDLE deviceElement,
                         __in BOOL detailed);

BOOL GetRealConfigDescriptor(__in UCHAR Index,
                             __out_opt PUCHAR Buffer,
                             __in ULONG BufferLength,
                             __out PUINT LengthTransferred);

LPCSTR LoadResourceUsbIds(void);

LPCSTR GetBmAttributes(UCHAR bmAttributes);
LPCSTR GetConfigBmAttributes(UCHAR bmAttributes);
CONST PCHAR GetDescriptorString(USHORT stringIndex);

VOID DumpDescriptorDevice(__in PUSB_DEVICE_DESCRIPTOR desc);

BOOL DumpDescriptorConfig(__inout PPUNI_DESCRIPTOR uniRef,
                          __inout PLONG remainingLength);

BOOL DumpDescriptorInterface(__inout PPUNI_DESCRIPTOR uniRef,
                             __inout PLONG remainingLength);

BOOL DumpDescriptorInterfaceAssociation(__inout PPUNI_DESCRIPTOR uniRef,
                                        __inout PLONG remainingLength);

BOOL DumpDescriptorEndpoint(__inout PPUNI_DESCRIPTOR uniRef,
                            __inout PLONG remainingLength);

BOOL DumpDescriptorCommon(__inout PPUNI_DESCRIPTOR uniRef,
                          __inout PLONG remainingLength);

BOOL DumpDescriptorHid(__inout PPUNI_DESCRIPTOR uniRef,
                       __in PUSB_INTERFACE_DESCRIPTOR currentInterface,
                       __inout PLONG remainingLength);

BOOL DumpDescriptorHidPhysical(__in PHID_DESCRIPTOR desc,
                               __in PUSB_INTERFACE_DESCRIPTOR currentInterface,
                               __in UCHAR descriptorPos);
BOOL DumpDescriptorHidReport(__in PHID_DESCRIPTOR desc,
                             __in PUSB_INTERFACE_DESCRIPTOR currentInterface,
                             __in UCHAR descriptorPos);

static LPCSTR DrvIdNames[8] = {"libusbK", "mpdppBusEnum", "WinUSB", "libusb0 filter", "Unknown", "Unknown", "Unknown"};
#define GetDrvIdString(DriverID)	(DrvIdNames[((((LONG)(DriverID))<0) || ((LONG)(DriverID)) >= KUSB_DRVID_COUNT)?KUSB_DRVID_COUNT:(DriverID)])

#define MAX_TAB 9
static LPCSTR gTabIndents[MAX_TAB] =
{
	"",
	" ",
	"   ",
	"     ",
	"       ",
	"         ",
	"           ",
	"             ",
	"               ",
};

static int gTab = 0;

LPCSTR UsbIdsText = NULL;

#define UNUSED_DESCRIPTOR_STRING "Unknown"
static LPCSTR DescriptorTypeString[] =
{
	"Device",
	"Configuration",
	"String",
	"Interface",
	"Endpoint",
	UNUSED_DESCRIPTOR_STRING,
	UNUSED_DESCRIPTOR_STRING,
	UNUSED_DESCRIPTOR_STRING,
	UNUSED_DESCRIPTOR_STRING,
	UNUSED_DESCRIPTOR_STRING,
	"Interface Association",
	UNUSED_DESCRIPTOR_STRING,
	UNUSED_DESCRIPTOR_STRING,
	UNUSED_DESCRIPTOR_STRING,
	UNUSED_DESCRIPTOR_STRING,
	UNUSED_DESCRIPTOR_STRING,
	UNUSED_DESCRIPTOR_STRING,
	UNUSED_DESCRIPTOR_STRING,
	UNUSED_DESCRIPTOR_STRING,
	UNUSED_DESCRIPTOR_STRING,
	UNUSED_DESCRIPTOR_STRING,
	UNUSED_DESCRIPTOR_STRING,
	UNUSED_DESCRIPTOR_STRING,
	UNUSED_DESCRIPTOR_STRING,
	UNUSED_DESCRIPTOR_STRING,
	UNUSED_DESCRIPTOR_STRING,
	UNUSED_DESCRIPTOR_STRING,
	UNUSED_DESCRIPTOR_STRING,
	UNUSED_DESCRIPTOR_STRING,
	UNUSED_DESCRIPTOR_STRING,
	UNUSED_DESCRIPTOR_STRING,
	UNUSED_DESCRIPTOR_STRING,
	"Hid",
	"Report",
	"Physical",
	UNUSED_DESCRIPTOR_STRING,
};
#define GetDescriptorTypeString(DescriptorId)							\
	(DescriptorTypeString[												\
	        (DescriptorId)-1>((sizeof(DescriptorTypeString)/sizeof(LPCSTR))) ?	\
	        ((sizeof(DescriptorTypeString)/sizeof(LPCSTR))-1) :					\
	        ((DescriptorId)-1)													\
	                     ])

#define KF_X "0x%X"
#define KF_X2 "0x%02X"
#define KF_X3 "0x%03X"
#define KF_X4 "0x%04X"

#define KF_U "%u"
#define KF_U2 "%02u"

#define KLIST_CATEGORY_SEP ":"
#define KLIST_LN "\n"
#define KLIST_HID_CAT_FORMAT "%-22s"
#define KLIST_CATEGORY_FORMAT "%-20s"
#define KLIST_CATEGORY_FORMAT_SEP KLIST_CATEGORY_FORMAT KLIST_CATEGORY_SEP

#define _TAB_INC() gTab=(gTab < MAX_TAB)?gTab+1:gTab
#define _TAB_DEC() gTab=(gTab > 0)?gTab-1:gTab

#define WRITERAW(format,...) printf("%s"format,gTabIndents[gTab],__VA_ARGS__)
#define WRITE(format,...)						WRITERAW(" "format,__VA_ARGS__)
#define WRITE_LN(format,...)					WRITE(format KLIST_LN,__VA_ARGS__)

#define WRITE_CAT(CategoryName,format,...)        WRITE(KLIST_CATEGORY_FORMAT format,CategoryName,__VA_ARGS__)
#define WRITE_CAT_LN(CategoryName,format,...)     WRITE_LN(KLIST_CATEGORY_FORMAT format,CategoryName,__VA_ARGS__)

#define WRITE_CAT_SEP(CategoryName,format,...)    WRITE(KLIST_CATEGORY_FORMAT_SEP format,CategoryName,__VA_ARGS__)
#define WRITE_CAT_SEP_LN(CategoryName,format,...) WRITE_LN(KLIST_CATEGORY_FORMAT_SEP format,CategoryName,__VA_ARGS__)

#define DESC_VALUE(Descriptor,FieldName,format,...) \
	WRITE_LN(KLIST_CATEGORY_FORMAT_SEP format,DEFINE_TO_STR(FieldName), Descriptor->FieldName,__VA_ARGS__)

#define DESC_HID_VALUE(Descriptor,DescriptorIndex,FieldName,format,...)	\
	WRITE_LN(KLIST_CATEGORY_FORMAT_SEP format,DEFINE_TO_STR(FieldName), (Descriptor)->DescriptorList[DescriptorIndex].FieldName,__VA_ARGS__)

#define DESC_iVALUE(Descriptor, StringIndex) \
	DESC_VALUE(Descriptor, StringIndex, KF_U"%s", GetDescriptorString(Descriptor->StringIndex))

#define DESC_MARK_DEVICE "-"
#define DESC_MARK_CONFIG "-"
#define DESC_MARK_HID "*"
#define DESC_MARK_END "!"
#define _DESC_BEGIN(DescId,DescMarkChar) \
	WRITERAW("%s%s%s\n",DescMarkChar,GetDescriptorTypeString(DescId),KLIST_CATEGORY_SEP); \
	_TAB_INC()

#define _DESC_SUB_BEGIN(DescId,DescMarkChar) \
	WRITERAW("%s%s%s\n",DescMarkChar,GetDescriptorTypeString(DescId),KLIST_CATEGORY_SEP); \
	_TAB_INC()

#define DESC_BEGIN_DEV(DescId) _DESC_BEGIN(DescId,DESC_MARK_DEVICE)
#define DESC_BEGIN_CFG(DescId) _DESC_SUB_BEGIN(DescId,DESC_MARK_CONFIG)

#define DESC_SUB_END(DescId) \
	_TAB_DEC(); \
	WRITERAW(DESC_MARK_END "End %s" KLIST_LN,GetDescriptorTypeString(DescId))


#define HID_BEGIN(CategoryName,format,...)  \
	WRITERAW("%s" KLIST_HID_CAT_FORMAT KLIST_CATEGORY_SEP format KLIST_LN,DESC_MARK_HID,CategoryName,__VA_ARGS__); \
	_TAB_INC()

#define HID_END(CategoryName,format,...)	\
	_TAB_DEC();								\
	WRITERAW(DESC_MARK_END KLIST_CATEGORY_FORMAT format KLIST_LN,CategoryName,__VA_ARGS__) \
 
#define PrintfDeviceElementEx(DeviceListFieldName,mFieldFormat) printf("    %-21s: " DEFINE_TO_STR(mFieldFormat) "\n",DEFINE_TO_STR(DeviceListFieldName),deviceElement->DeviceListFieldName)
#define PrintfDeviceElement(DeviceListFieldName) printf("    %-21s: %s\n",DEFINE_TO_STR(DeviceListFieldName),deviceElement->DeviceListFieldName)

#define IsDescValid(DescriptorPtr, RemainingTotalSize)\
	(((RemainingTotalSize) > sizeof(USB_COMMON_DESCRIPTOR)) && (RemainingTotalSize) >= (DescriptorPtr)->bLength)

#define AdvanceDescriptor(DescriptorPtr, RemainingLength)										\
	{																								\
		RemainingLength -= DescriptorPtr->bLength;													\
		DescriptorPtr = (PUSB_COMMON_DESCRIPTOR)(((PUCHAR)DescriptorPtr) + DescriptorPtr->bLength);	\
	}

#define IsUniDescriptorValid(DescriptorPtr, RemainingTotalSize)\
	(((RemainingTotalSize) > sizeof(USB_COMMON_DESCRIPTOR)) && (RemainingTotalSize) >= (DescriptorPtr)->Common.bLength)

#define AdvanceUniDescriptor(DescriptorPtr, RemainingLength)										\
	{																								\
		(RemainingLength) -= (DescriptorPtr)->Common.bLength;													\
		(DescriptorPtr) = (PUNI_DESCRIPTOR)(((PUCHAR)(DescriptorPtr)) + (DescriptorPtr)->Common.bLength);	\
	}

#define AdvanceDescriptorWithErrorBreak(DescriptorPtr, RemainingLength)							\
	AdvanceDescriptor(DescriptorPtr, RemainingLength);											\
	if (!IsDescValid(DescriptorPtr, RemainingLength))											\
		break

void ShowHelp(void);
void ShowCopyright(void);

int __cdecl main(int argc, char** argv)
{
	LONG ec = ERROR_SUCCESS;
	KLST_HANDLE deviceList = NULL;
	KLST_DEVINFO_HANDLE deviceElement;
	LONG devicePos = 0;
	LONG_PTR selection;
	UINT count = 0;
	int iArg;
	KLST_FLAG lstFlags = KLST_FLAG_NONE;
	KLST_PATTERN_MATCH patternMatch;
	PKLST_PATTERN_MATCH pPatternMatch = NULL;
	HANDLE hHeap = NULL;

	UNREFERENCED_PARAMETER(argc);
	UNREFERENCED_PARAMETER(argv);

	memset(&patternMatch, 0, sizeof(patternMatch));

	for (iArg = 1; iArg < argc; iArg++)
	{
		if (_stricmp(argv[iArg], "A") == 0)
			lstFlags |= KLST_FLAG_INCLUDE_DISCONNECT;
		else if (_stricmp(argv[iArg], "RAWGUID") == 0)
			lstFlags |= KLST_FLAG_INCLUDE_RAWGUID;
		else if ((iArg + 1 < argc) && _stricmp(argv[iArg], "/C") == 0)
			strcpy(patternMatch.ClassGUID, argv[++iArg]);
		else if ((iArg + 1 < argc) && _stricmp(argv[iArg], "/I") == 0)
			strcpy(patternMatch.DeviceInterfaceGUID, argv[++iArg]);
		else if ((iArg + 1 < argc) && _stricmp(argv[iArg], "/D") == 0)
			strcpy(patternMatch.DeviceID, argv[++iArg]);
		else
		{
			if (_stricmp(argv[iArg], "?") != 0 && _stricmp(argv[iArg], "/?") != 0 && _stricmp(argv[iArg], "HELP") != 0)
				printf("Invalid argument: '%s'\n", argv[iArg]);

			ShowHelp();
			return -1;
		}

	}
	printf("\n");
	printf("Loading USB ID's maintained by Stephen J. Gowdy <linux.usb.ids@gmail.com>..\n");
	UsbIdsText = LoadResourceUsbIds();
	printf("\n");

	pPatternMatch = &patternMatch;

	hHeap = HeapCreate(HEAP_NO_SERIALIZE, 0, 0);

	LibK_Context_Init(hHeap, NULL);

	if (!LstK_InitEx(&deviceList, lstFlags, pPatternMatch))
	{
		printf("failed getting device list.\n");
		ec = WinError(0);
		HeapDestroy(hHeap);
		return ec;
	}

	LstK_Count(deviceList, &count);
	if (count == 0)
	{
		printf("No devices found.\n");
		ec = -1;
		goto Done;
	}

	while(LstK_MoveNext(deviceList, &deviceElement))
	{
		printf("%2d. %s (%s) [%s]\n",
		       devicePos + 1,
		       deviceElement->DeviceDesc,
		       deviceElement->Mfg,
		       deviceElement->Connected ? "Connected" : "Not Connected");

		PrintfDeviceElement(Service);
		PrintfDeviceElement(ClassGUID);
		PrintfDeviceElement(DeviceID);
		PrintfDeviceElement(DeviceInterfaceGUID);
		PrintfDeviceElement(SymbolicLink);
		PrintfDeviceElement(DevicePath);
		PrintfDeviceElement(SerialNumber);
		PrintfDeviceElementEx(BusNumber, % u);
		PrintfDeviceElementEx(DeviceAddress, % u);

		printf("\n");

		devicePos++;

	}

	if (!devicePos)
	{
		printf("device list error\n");
		ec = -1;
		goto Done;
	}

	printf("Select device (1-%u) :", devicePos);
	while(_kbhit()) _getch();
	if (
	    ((((LONG)scanf_s("%u", &selection)) != 1) ||
	     (selection) > devicePos)
	)
	{
		printf("\ninvalid selection\n");
		ec = -1;
		goto Done;
	}
	printf("\n");

	devicePos = 1;
	while(LstK_MoveNext(deviceList, &deviceElement))
	{
		if (devicePos == selection)
			break;
		devicePos++;
	}
	if (!deviceElement)
	{
		printf("device list error\n");
		ec = -1;
		goto Done;
	}

	printf("Loading driver api..\n");
	if (!LibK_LoadDriverAPI(&K, deviceElement->DriverID))
	{
		ec = WinError(0);
		goto Done;
	}
	printf("Getting descriptors..\n");
	if (!GetDescriptorReport(deviceElement, TRUE))
	{
		ec = WinError(0);
		goto Done;
	}

Done:
	LstK_Free(deviceList);
	LibK_Context_Free();
	if (hHeap) HeapDestroy(hHeap);

	return ec;
}

BOOL GetRealConfigDescriptor(__in UCHAR Index,
                             __out_opt PUCHAR Buffer,
                             __in ULONG BufferLength,
                             __out PUINT LengthTransferred)
{
	WINUSB_SETUP_PACKET Pkt;
	KUSB_SETUP_PACKET* defPkt = (KUSB_SETUP_PACKET*)&Pkt;

	memset(&Pkt, 0, sizeof(Pkt));
	defPkt->BmRequest.Dir = BMREQUEST_DIR_DEVICE_TO_HOST;
	defPkt->Request = USB_REQUEST_GET_DESCRIPTOR;
	defPkt->ValueHi = USB_DESCRIPTOR_TYPE_CONFIGURATION;
	defPkt->ValueLo = Index;
	defPkt->Length = (USHORT)BufferLength;

	return K.ControlTransfer(InterfaceHandle, Pkt, Buffer, BufferLength, LengthTransferred, NULL);
}

BOOL OpenDeviceFileHandle(__in LPCSTR deviceFileName, __out HANDLE* fileHandle)
{
	HANDLE handle = CreateFileA(deviceFileName,
	                            GENERIC_READ | GENERIC_WRITE,
	                            FILE_SHARE_READ | FILE_SHARE_WRITE,
	                            NULL,
	                            OPEN_EXISTING,
	                            FILE_FLAG_OVERLAPPED,
	                            NULL);

	if (!handle || handle == INVALID_HANDLE_VALUE)
	{
		printf("failed creating device file handle:\n:%s\n", deviceFileName);
		return FALSE;
	}

	*fileHandle = handle;
	return TRUE;
}

BOOL GetDescriptorReport(__in KLST_DEVINFO_HANDLE deviceElement, __in BOOL detailed)
{
	HANDLE fileHandle = NULL;
	UINT length;
	USB_DEVICE_DESCRIPTOR deviceDescriptor;
	BOOL success = FALSE;
	PUSB_CONFIGURATION_DESCRIPTOR configDescriptor = NULL;

	if ((success = OpenDeviceFileHandle(deviceElement->DevicePath, &fileHandle)) == FALSE)
		goto Error;

	if (deviceElement)
	{

		if (!K.Initialize(fileHandle, &InterfaceHandle))
		{
			WinError(0);
			goto Error;
		}

		if (!K.GetDescriptor(InterfaceHandle,
		                     USB_DESCRIPTOR_TYPE_DEVICE,
		                     0, 0,
		                     (PUCHAR)&deviceDescriptor,
		                     sizeof(deviceDescriptor),
		                     &length))
		{
			WinError(0);
			goto Error;
		}

		WRITE_LN("");

		DESC_BEGIN_DEV(USB_DESCRIPTOR_TYPE_DEVICE);
		DumpDescriptorDevice(&deviceDescriptor);
		DESC_SUB_END(USB_DESCRIPTOR_TYPE_DEVICE);

		configDescriptor = malloc(4096);
		memset(configDescriptor, 0, 4096);

		if (detailed)
			success = GetRealConfigDescriptor(0, (PUCHAR)configDescriptor, 4096, &length);
		else
			success = K.GetDescriptor(InterfaceHandle, USB_DESCRIPTOR_TYPE_CONFIGURATION, 0, 0, (PUCHAR)configDescriptor, 4096, &length);

		if (!success)
		{
			printf("failed getting config descriptor.\n");
			goto Error;
		}
		else
		{
			LONG remainingLength = 0;
			PUNI_DESCRIPTOR hdr = (PUNI_DESCRIPTOR)configDescriptor;

			remainingLength = (LONG)length;

			success = DumpDescriptorConfig(&hdr, &remainingLength);
			if (!success && !remainingLength)
				success = TRUE;
		}
	}

Error:
	if (InterfaceHandle)
		K.Free(InterfaceHandle);

	if (fileHandle)
		CloseHandle(fileHandle);
	return success;
}


LONG WinError(__in_opt DWORD errorCode)
{
	LPSTR buffer = NULL;

	errorCode = errorCode ? labs(errorCode) : GetLastError();
	if (!errorCode) return errorCode;

	if (FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
	                   NULL, errorCode, 0, (LPSTR)&buffer, 0, NULL) > 0)
	{
		printf("%s\n", buffer);
	}
	else
	{
		printf("FormatMessage error!\n");
	}

	if (buffer)
		LocalFree(buffer);

	return -labs(errorCode);
}

CONST PCHAR GetDescriptorString(USHORT stringIndex)
{
	static CHAR rtn[128];

	BOOL success;
	UINT length;
	DWORD size = sizeof(USB_STRING_DESCRIPTOR) + sizeof(rtn) * 2;
	PUSB_STRING_DESCRIPTOR stringDesc = NULL;

	memset(rtn, 0, sizeof(rtn));
	if (!stringIndex) return rtn;

	stringDesc = malloc(size);
	memset(stringDesc, 0, size);

	success = K.GetDescriptor(InterfaceHandle, USB_DESCRIPTOR_TYPE_STRING, (UCHAR)stringIndex, 0, (PUCHAR)stringDesc, size, &length);
	if (success && length > sizeof(USB_COMMON_DESCRIPTOR))
	{
		PCHAR  dst = rtn;
		PWCHAR src = stringDesc->bString;
		length -= sizeof(USB_COMMON_DESCRIPTOR);
		length /= 2;
		dst[0] = ' ';
		dst[1] = '(';
		dst += 2;

		while(length-- && *src)
		{
			*dst = (CHAR) * src;
			dst++;
			src++;
		}
		dst[0] = ')';
		dst[1] = '\0';
	}

	if (stringDesc)
		free(stringDesc);

	return rtn;
}
LPCSTR AddGroupEndings(LPCSTR src)
{
	static CHAR buffer[MAX_PATH];
	memset(buffer, 0, sizeof(buffer));
	if (src[0])
	{
		if (src[0] != '(')
			strcat(buffer, "(");

		strcat(buffer, src);

		if (src[strlen(src) - 1] != ')')
			strcat(buffer, ")");
	}
	return buffer;
}

VOID DumpDescriptorDevice(PUSB_DEVICE_DESCRIPTOR desc)
{
	CHAR vendorName[MAX_PATH];
	CHAR productName[MAX_PATH];
	CHAR className[MAX_PATH];
	CHAR subClassName[MAX_PATH];
	CHAR protocolName[MAX_PATH];

	GetHwIdDisplayText(desc->idVendor, desc->idProduct, vendorName, productName);

	GetClassDisplayText(desc->bDeviceClass, desc->bDeviceSubClass, desc->bDeviceProtocol,
	                    className, subClassName, protocolName);


	DESC_VALUE(desc, bLength, KF_U);
	DESC_VALUE(desc, bDescriptorType, KF_X2);
	DESC_VALUE(desc, bcdUSB, KF_X4);
	DESC_VALUE(desc, bDeviceClass, KF_X2" %s", AddGroupEndings(className));
	DESC_VALUE(desc, bDeviceSubClass, KF_X2" %s", AddGroupEndings(subClassName));
	DESC_VALUE(desc, bDeviceProtocol, KF_X2" %s", AddGroupEndings(protocolName));
	DESC_VALUE(desc, bMaxPacketSize0, KF_U);
	DESC_VALUE(desc, idVendor, KF_X4" %s", AddGroupEndings(vendorName));
	DESC_VALUE(desc, idProduct, KF_X4" %s", AddGroupEndings(productName));
	DESC_VALUE(desc, bcdDevice, KF_X4);
	DESC_iVALUE(desc, iManufacturer);
	DESC_iVALUE(desc, iProduct);
	DESC_iVALUE(desc, iSerialNumber);
	DESC_VALUE(desc, bNumConfigurations, KF_U);
}

BOOL DumpDescriptorConfig(PPUNI_DESCRIPTOR uniRef, PLONG remainingLength)
{
	INT numInterfaces;
	BOOL success = TRUE;
	UCHAR interfaceNumberTrack[128];
	PUSB_CONFIGURATION_DESCRIPTOR desc = &((*uniRef)->Config);

	if (!IsUniDescriptorValid(*uniRef, *remainingLength))
		return FALSE;

	DESC_BEGIN_DEV(USB_DESCRIPTOR_TYPE_CONFIGURATION);

	memset(interfaceNumberTrack, 0, sizeof(interfaceNumberTrack));

	numInterfaces = (INT)(desc->bNumInterfaces);

	DESC_VALUE(desc, bLength, KF_U);
	DESC_VALUE(desc, bDescriptorType, KF_X2);
	DESC_VALUE(desc, wTotalLength, KF_U);
	DESC_VALUE(desc, bNumInterfaces, KF_U);
	DESC_VALUE(desc, bConfigurationValue, KF_X2);
	DESC_iVALUE(desc, iConfiguration);
	DESC_VALUE(desc, bmAttributes, KF_X2" %s", AddGroupEndings(GetConfigBmAttributes(desc->bmAttributes)));
	DESC_VALUE(desc, MaxPower, KF_U " (" KF_U "ma)", desc->MaxPower * 2);

	AdvanceUniDescriptor(*uniRef, *remainingLength);

	while ((success = IsUniDescriptorValid(*uniRef, *remainingLength)) == TRUE)
	{
		switch ((*uniRef)->Common.bDescriptorType)
		{
		case USB_DESCRIPTOR_TYPE_INTERFACE:
			interfaceNumberTrack[(*uniRef)->Interface.bInterfaceNumber]++;
			if (interfaceNumberTrack[(*uniRef)->Interface.bInterfaceNumber] == 1)
			{
				if ((--numInterfaces) < 0)
				{
					printf("Config descriptor is mis-reporting bNumInterfaces.\n");
					return FALSE;
				}
			}
			success = DumpDescriptorInterface(uniRef, remainingLength);
			break;
		case USB_DESCRIPTOR_TYPE_INTERFACE_ASSOCIATION:
			success = DumpDescriptorInterfaceAssociation(uniRef, remainingLength);
			break;
		default:
			success = DumpDescriptorCommon(uniRef, remainingLength);
			break;
		}
		if (!success)
			break;
	}

	DESC_SUB_END(USB_DESCRIPTOR_TYPE_CONFIGURATION);

	return success;
}

BOOL DumpDescriptorInterfaceAssociation(PPUNI_DESCRIPTOR uniRef, PLONG remainingLength)
{
	BOOL success = TRUE;
	PUSB_INTERFACE_ASSOCIATION_DESCRIPTOR desc = &((*uniRef)->InterfaceAssociation);


	if (!IsUniDescriptorValid(*uniRef, *remainingLength))
		return FALSE;

	DESC_BEGIN_CFG(USB_DESCRIPTOR_TYPE_INTERFACE_ASSOCIATION);

	DESC_VALUE(desc, bLength, KF_U);
	DESC_VALUE(desc, bDescriptorType, KF_X2);
	DESC_VALUE(desc, bFirstInterface, KF_X2);
	DESC_VALUE(desc, bInterfaceCount, KF_U);
	DESC_VALUE(desc, bFunctionClass, KF_X2);
	DESC_VALUE(desc, bFunctionSubClass, KF_X2);
	DESC_VALUE(desc, bFunctionProtocol, KF_X2);
	DESC_iVALUE(desc, iFunction);

	AdvanceUniDescriptor(*uniRef, *remainingLength);

	DESC_SUB_END(USB_DESCRIPTOR_TYPE_INTERFACE_ASSOCIATION);

	return success;
}

BOOL DumpDescriptorInterface(PPUNI_DESCRIPTOR uniRef, PLONG remainingLength)
{
	BOOL success = TRUE;
	CHAR className[MAX_PATH];
	CHAR subClassName[MAX_PATH];
	CHAR protocolName[MAX_PATH];
	PUSB_INTERFACE_DESCRIPTOR desc = &((*uniRef)->Interface);

	INT numEndpoints = (INT)(desc->bNumEndpoints);
	USB_INTERFACE_DESCRIPTOR currentInterface;

	if (!IsUniDescriptorValid(*uniRef, *remainingLength))
		return FALSE;

	DESC_BEGIN_CFG(USB_DESCRIPTOR_TYPE_INTERFACE);

	GetClassDisplayText(
	    desc->bInterfaceClass,
	    desc->bInterfaceSubClass,
	    desc->bInterfaceProtocol,
	    className,
	    subClassName,
	    protocolName);

	DESC_VALUE(desc, bLength, KF_U);
	DESC_VALUE(desc, bDescriptorType, KF_X2);
	DESC_VALUE(desc, bInterfaceNumber, KF_X2);
	DESC_VALUE(desc, bAlternateSetting, KF_X2);
	DESC_VALUE(desc, bNumEndpoints, KF_U);
	DESC_VALUE(desc, bInterfaceClass, KF_X2" %s", AddGroupEndings(className));
	DESC_VALUE(desc, bInterfaceSubClass, KF_X2" %s", AddGroupEndings(subClassName));
	DESC_VALUE(desc, bInterfaceProtocol, KF_X2" %s", AddGroupEndings(protocolName));
	DESC_iVALUE(desc, iInterface);

	memcpy(&currentInterface, desc, sizeof(currentInterface));

	AdvanceUniDescriptor(*uniRef, *remainingLength);

	while ((success = IsUniDescriptorValid(*uniRef, *remainingLength)) == TRUE)
	{
		switch ((*uniRef)->Common.bDescriptorType)
		{

		case USB_DESCRIPTOR_TYPE_ENDPOINT:
			if ((--numEndpoints) < 0)
			{
				printf("Interface descriptor is mis-reporting bNumEndpoints.\n");
				return FALSE;
			}
			success = DumpDescriptorEndpoint(uniRef, remainingLength);
			break;

		case USB_HID_DESCRIPTOR_TYPE:
			success = DumpDescriptorHid(uniRef, &currentInterface, remainingLength);
			break;

		case USB_DESCRIPTOR_TYPE_INTERFACE:
			DESC_SUB_END(USB_DESCRIPTOR_TYPE_INTERFACE);
			return TRUE;

		default:
			success = DumpDescriptorCommon(uniRef, remainingLength);
			break;

		}
		if (!success)
			break;
	}

	DESC_SUB_END(USB_DESCRIPTOR_TYPE_INTERFACE);

	return success;
}

int GetHidDescriptorItemValue(UCHAR itemType, PUCHAR data)
{
	switch(itemType & 0x3)
	{
	case 1:	// byte
		return (int)(((PUCHAR)data)[0]);
	case 2:	// word
		return (int)(((PUSHORT)data)[0]);
	case 3:	// dword ?
		return (int)(((PULONG)data)[0]);
	}
	return 0;
}

BOOL DumpDescriptorHidReport(__in PHID_DESCRIPTOR desc,
                             __in PUSB_INTERFACE_DESCRIPTOR currentInterface,
                             __in UCHAR descriptorPos)
{
	static CHAR usageName[MAX_PATH];
	static CHAR usagePage[MAX_PATH];
	static CHAR usageText[MAX_PATH];

	static const LPCSTR collectionNames[] = {"Physical", "Application", "Logical", "Vendor Defined"};
	BOOL success = TRUE;
	INT transferred;
	FIND_USBIDS_CONTEXT idContext;
	UCHAR reportType = desc->DescriptorList[descriptorPos].bReportType;
	USHORT reportLength = desc->DescriptorList[descriptorPos].wReportLength;
	PUCHAR reportBuffer = LocalAlloc(LPTR, reportLength);

	WINUSB_SETUP_PACKET Pkt;
	KUSB_SETUP_PACKET* defPkt = (KUSB_SETUP_PACKET*)&Pkt;

	memset(&idContext, 0, sizeof(idContext));
	memset(&Pkt, 0, sizeof(Pkt));

	defPkt->BmRequest.Dir = BMREQUEST_DIR_DEVICE_TO_HOST;
	defPkt->BmRequest.Recipient = BMREQUEST_RECIPIENT_INTERFACE;

	defPkt->Request = USB_REQUEST_GET_DESCRIPTOR;

	defPkt->ValueHi = reportType;
	defPkt->ValueLo = descriptorPos;

	defPkt->IndexLo = currentInterface->bInterfaceNumber;
	defPkt->Length = reportLength;

	success = K.ControlTransfer(InterfaceHandle, Pkt, reportBuffer, reportLength, (PUINT)&transferred, NULL);
	if (success)
	{
		PUCHAR data = reportBuffer;
		UCHAR usageID;
		BOOL WasHidValueHandled;
		while(transferred > 0)
		{
			int hidItemValue;

			WasHidValueHandled = FALSE;
			usageID = data[0];
			data++;
			transferred--;

			memset(usageName, 0, sizeof(usageName));
			GetHidDescriptorItemDisplayText(usageID, usageName);

			if ((transferred < (usageID & 0x3)) || ((usageID & 0x3) == 0x3))
			{
				printf("Hid report descriptor invalid near byte offset %d.\n", ((LONG)reportLength) - transferred);
				break;
			}

			hidItemValue = GetHidDescriptorItemValue(usageID, data);
			switch(usageID & 0xFC)
			{
			case 0x04: // Usage Page
				GetHidUsagePageText((UCHAR)hidItemValue, usagePage, &idContext);
				if (idContext.Found.MatchStart)
				{
					WasHidValueHandled = TRUE;
					WRITE_CAT_SEP_LN(usageName, KF_X2" %s", hidItemValue, AddGroupEndings(usagePage));
				}
				break;
			case 0x08: // Usage
				if (idContext.Found.MatchStart)
				{
					GetHidUsageText((USHORT)hidItemValue, usageText, &idContext);
					WRITE_CAT_SEP_LN(usageName, KF_X3" %s", hidItemValue, AddGroupEndings(usageText));
					WasHidValueHandled = TRUE;
				}
				break;
			case 0xA0: // Collection
				WasHidValueHandled = TRUE;
				HID_BEGIN(usageName, KF_X2" %s", hidItemValue, AddGroupEndings(collectionNames[hidItemValue & 0x3]));
				break;
			case 0xC0: // End Collection
				HID_END(usageName, "");
				WasHidValueHandled = TRUE;
				break;
			}

			switch(usageID & 0x3)
			{
			case 0:	// zero length data
				if (!WasHidValueHandled)
					WRITE_CAT(usageName, "\n");
				break;

			case 1:	// byte
				if (!WasHidValueHandled)
					WRITE_CAT_SEP_LN(usageName, KF_X2, hidItemValue);

				data++;
				transferred--;
				break;

			case 2:	// word
				if (!WasHidValueHandled)
					WRITE_CAT_SEP_LN(usageName, KF_X4, hidItemValue);

				data += 2;
				transferred -= 2;
				break;

			case 3:	// dword ?
				if (!WasHidValueHandled)
					WRITE_CAT_SEP_LN(usageName, KF_X, hidItemValue);

				data += 4;
				transferred -= 4;
				break;
			}
		}
	}

	LocalFree(reportBuffer);
	return success;
}

BOOL DumpDescriptorHidPhysical(__in PHID_DESCRIPTOR desc,
                               __in PUSB_INTERFACE_DESCRIPTOR currentInterface,
                               __in UCHAR descriptorPos)
{
	UNREFERENCED_PARAMETER(desc);
	UNREFERENCED_PARAMETER(currentInterface);
	UNREFERENCED_PARAMETER(descriptorPos);
	return TRUE;
}

BOOL DumpDescriptorHid(__inout PPUNI_DESCRIPTOR uniRef,
                       __in PUSB_INTERFACE_DESCRIPTOR currentInterface,
                       __inout PLONG remainingLength)
{
	UCHAR descriptorPos;
	BOOL success = TRUE;
	static CHAR countryName[MAX_PATH];

	PHID_DESCRIPTOR desc = &((*uniRef)->Hid);

	if (!IsUniDescriptorValid(*uniRef, *remainingLength))
		return FALSE;

	GetHidCountryCodeText(desc->bCountry, countryName);

	DESC_BEGIN_CFG(USB_HID_DESCRIPTOR_TYPE);

	DESC_VALUE(desc, bLength, KF_U);
	DESC_VALUE(desc, bDescriptorType, KF_X2);
	DESC_VALUE(desc, bcdHID, KF_X4);
	DESC_VALUE(desc, bCountry, KF_X2" %s", AddGroupEndings(countryName));
	DESC_VALUE(desc, bNumDescriptors, KF_U);

	for (descriptorPos = 0; descriptorPos < desc->bNumDescriptors; descriptorPos++)
	{
		switch (desc->DescriptorList[descriptorPos].bReportType)
		{
		case USB_HID_REPORT_DESCRIPTOR_TYPE:
			DESC_BEGIN_CFG(USB_HID_REPORT_DESCRIPTOR_TYPE);

			DESC_HID_VALUE(desc, descriptorPos, bReportType, KF_X2);
			DESC_HID_VALUE(desc, descriptorPos, wReportLength, KF_U);

			DumpDescriptorHidReport(desc, currentInterface, descriptorPos);

			DESC_SUB_END(USB_HID_REPORT_DESCRIPTOR_TYPE);
			break;
		case USB_HID_PHYSICAL_DESCRIPTOR_TYPE:
			DESC_BEGIN_CFG(USB_HID_PHYSICAL_DESCRIPTOR_TYPE);

			DESC_HID_VALUE(desc, descriptorPos, bReportType, KF_X2);
			DESC_HID_VALUE(desc, descriptorPos, wReportLength, KF_U);

			DumpDescriptorHidPhysical(desc, currentInterface, descriptorPos);

			DESC_SUB_END(USB_HID_PHYSICAL_DESCRIPTOR_TYPE);
			break;
		default:
			DESC_BEGIN_CFG(0xFF);

			DESC_HID_VALUE(desc, descriptorPos, bReportType, KF_X2);
			DESC_HID_VALUE(desc, descriptorPos, wReportLength, KF_U);

			DESC_SUB_END(0xFF);
			break;
		}
	}
	AdvanceUniDescriptor(*uniRef, *remainingLength);

	DESC_SUB_END(USB_HID_DESCRIPTOR_TYPE);

	return success;
}
LPCSTR GetBmAttributes(UCHAR bmAttributes)
{
	CONST PCHAR PipeTypeStrings[4] = {"Control", "Isochronous", "Bulk", "Interrupt"};
	CONST PCHAR IsoSynchronizationStrings[4] = {"No Synchonization", "Asynchronous", "Adaptive", "Synchronous"};
	CONST PCHAR IsoUsageTypeStrings[4] = {"Data Endpoint", "Feedback Endpoint", "Explicit Feedback Data Endpoint", "Reserved"};
#define GetPipeTypeString(PipeType) PipeTypeStrings[(PipeType) & 0x3]
#define GetIsoSyncronizationString(BmAttributes) IsoSynchronizationStrings[((BmAttributes)>>2) & 0x3]
#define GetIsoUsageTypeString(BmAttributes) IsoUsageTypeStrings[((BmAttributes)>>4) & 0x3]

	static CHAR rtn[128];

	memset(rtn, 0, sizeof(rtn));
	strcat_s(rtn, sizeof(rtn) - 1, GetPipeTypeString(bmAttributes));

	if ((bmAttributes & 0x03) == 0x01)
	{
		// isochronous
		strcat_s(rtn, sizeof(rtn) - 1, ", ");
		strcat_s(rtn, sizeof(rtn) - 1, GetIsoSyncronizationString(bmAttributes));
		strcat_s(rtn, sizeof(rtn) - 1, ", ");
		strcat_s(rtn, sizeof(rtn) - 1, GetIsoUsageTypeString(bmAttributes));
	}

	return rtn;
}

LPCSTR GetConfigBmAttributes(UCHAR bmAttributes)
{
	LPCSTR BmStringsConfig[4] = {"", "Remote-Wakeup", "Self-Powered", "Remote-Wakeup, Self-Powered"};

	return BmStringsConfig[(bmAttributes >> 4) & 0x3];
}

BOOL DumpDescriptorEndpoint(PPUNI_DESCRIPTOR uniRef, PLONG remainingLength)
{
	BOOL success = TRUE;
	PUSB_ENDPOINT_DESCRIPTOR desc = &((*uniRef)->Endpoint);

	if (!IsUniDescriptorValid(*uniRef, *remainingLength))
		return FALSE;

	DESC_BEGIN_CFG(USB_DESCRIPTOR_TYPE_ENDPOINT);

	DESC_VALUE(desc, bLength, KF_U);
	DESC_VALUE(desc, bDescriptorType, KF_X2);
	DESC_VALUE(desc, bEndpointAddress, KF_X2);
	DESC_VALUE(desc, bmAttributes, KF_X2" (%s)", GetBmAttributes(desc->bmAttributes));
	DESC_VALUE(desc, wMaxPacketSize, KF_U);
	DESC_VALUE(desc, bInterval, KF_X2);

	AdvanceUniDescriptor(*uniRef, *remainingLength);

	DESC_SUB_END(USB_DESCRIPTOR_TYPE_ENDPOINT);

	return success;
}
BOOL DumpDescriptorCommon(PPUNI_DESCRIPTOR uniRef, PLONG remainingLength)
{
	BOOL success = TRUE;
	PUSB_COMMON_DESCRIPTOR desc = &((*uniRef)->Common);


	if (!IsUniDescriptorValid(*uniRef, *remainingLength))
		return FALSE;

	DESC_BEGIN_CFG(0xff);

	DESC_VALUE(desc, bLength, KF_U);
	DESC_VALUE(desc, bDescriptorType, KF_X2);

	AdvanceUniDescriptor(*uniRef, *remainingLength);

	DESC_SUB_END(0xff);

	return success;
}

LPCSTR LoadResourceUsbIds(void)
{
#define ID_USBIDS_TEXT  10021
#define ID_DOS_TEXT   300

	LPCSTR src;
	LPSTR dst;
	DWORD src_count;
	HGLOBAL res_data;
	HRSRC hSrc;

	hSrc = FindResourceA(NULL, MAKEINTRESOURCEA(ID_USBIDS_TEXT), MAKEINTRESOURCEA(ID_DOS_TEXT));
	if (!hSrc)	return NULL;

	src_count = SizeofResource(NULL, hSrc);

	res_data = LoadResource(NULL, hSrc);
	if (!res_data)	return NULL;

	src = (char*) LockResource(res_data);
	if (!src) return NULL;
	dst = LocalAlloc(LPTR, src_count + 1);

	memcpy(dst, src, src_count);

	return dst;
}

//////////////////////////////////////////////////////////////////////////////
/* END OF PROGRAM                                                           */
//////////////////////////////////////////////////////////////////////////////
void ShowHelp(void)
{
#define ID_HELP_TEXT  10020
#define ID_DOS_TEXT   300

	CONST CHAR* src;
	DWORD src_count, charsWritten;
	HGLOBAL res_data;
	HANDLE handle;
	HRSRC hSrc;

	ShowCopyright();

	hSrc = FindResourceA(NULL, MAKEINTRESOURCEA(ID_HELP_TEXT), MAKEINTRESOURCEA(ID_DOS_TEXT));
	if (!hSrc)	return;

	src_count = SizeofResource(NULL, hSrc);

	res_data = LoadResource(NULL, hSrc);
	if (!res_data)	return;

	src = (char*) LockResource(res_data);
	if (!src) return;

	if ((handle = GetStdHandle(STD_ERROR_HANDLE)) != INVALID_HANDLE_VALUE)
		WriteConsoleA(handle, src, src_count, &charsWritten, NULL);
}

void ShowCopyright(void)
{
	printf("%s v%s (%s)\n",
	       RC_FILENAME_STR,
	       RC_VERSION_STR,
	       DEFINE_TO_STR(VERSION_DATE));

	printf("%s", "Copyright (c) 2011-2012 Travis Lee Robinson. <libusbdotnet@gmail.com>\n\n");
}

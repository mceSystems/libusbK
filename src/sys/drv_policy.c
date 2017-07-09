/*!********************************************************************
libusbK - WDF USB driver.
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

#include "drv_common.h"

// warning C4127: conditional expression is constant.
#pragma warning(disable: 4127)

#if (defined(ALLOC_PRAGMA) && defined(PAGING_ENABLED))
#pragma alloc_text(PAGE, Policy_InitPipe)
//#pragma alloc_text(PAGE, Policy_SetPipe)
//#pragma alloc_text(PAGE, Policy_GetPipe)
#pragma alloc_text(PAGE, Policy_InitPower)
#pragma alloc_text(PAGE, Policy_SetPower)
#pragma alloc_text(PAGE, Policy_GetPower)
#endif

#define mPipe_CheckValueLength(mStatus,mPolicyType,mPipeID,mMinValueLength,mCheckValueLength,mCheckValueUpdateLength,mErrorAction) do {	\
		if ((int)(mMinValueLength) > (int)(mCheckValueLength)) 																				\
		{  																																	\
			mStatus = STATUS_INVALID_BUFFER_SIZE;  																							\
			USBERRN("PipeID=%02Xh Invalid policy length %u for policy type %u. MinimumLength=%u",  											\
			        mPipeID,mCheckValueLength,mPolicyType,mMinValueLength);																		\
			\
			if (mCheckValueUpdateLength) ((PULONG)(mCheckValueUpdateLength))[0]=(ULONG)(mCheckValueLength); 								\
			mErrorAction;  																													\
		}  																																	\
		if (mCheckValueUpdateLength) ((PULONG)(mCheckValueUpdateLength))[0]=(ULONG)(mCheckValueLength); 									\
	}while(0)

#define mPower_CheckValueLength(mStatus,mPolicyType,mMinValueLength,mCheckValueLength,mCheckValueUpdateLength,mErrorAction) do {	\
		if ((int)(mMinValueLength) > (int)(mCheckValueLength)) 																				\
		{  																																	\
			mStatus = STATUS_INVALID_BUFFER_SIZE;  																							\
			USBERRN("Invalid policy length %u for power policy type %u. MinimumLength=%u",  											\
			        mCheckValueLength,mPolicyType,mMinValueLength);																		\
			\
			if (mCheckValueUpdateLength) ((PULONG)(mCheckValueUpdateLength))[0]=(ULONG)(mCheckValueLength); 								\
			mErrorAction;  																													\
		}  																																	\
		if (mCheckValueUpdateLength) ((PULONG)(mCheckValueUpdateLength))[0]=(ULONG)(mCheckValueLength); 									\
	}while(0)

#define mDevice_CheckValueLength(mStatus,mPolicyType,mMinValueLength,mCheckValueLength,mCheckValueUpdateLength,mErrorAction) do {	\
		if ((int)(mMinValueLength) > (int)(mCheckValueLength)) 																				\
		{  																																	\
			mStatus = STATUS_INVALID_BUFFER_SIZE;  																							\
			USBERRN("Invalid length %u for device information type %u. MinimumLength=%u",  											\
			        mCheckValueLength,mPolicyType,mMinValueLength);																		\
			\
			if (mCheckValueUpdateLength) ((PULONG)(mCheckValueUpdateLength))[0]=(ULONG)(mCheckValueLength); 								\
			mErrorAction;  																													\
		}  																																	\
		if (mCheckValueUpdateLength) ((PULONG)(mCheckValueUpdateLength))[0]=(ULONG)(mCheckValueLength); 									\
	}while(0)


VOID Policy_SetAllPipesToDefault(__in PDEVICE_CONTEXT deviceContext)
{
	PPIPE_CONTEXT pipeContext = NULL;
	UCHAR interfaceIndex, pipeIndex;
	UCHAR interfaceCount, pipeCount;

	PAGED_CODE();

	interfaceCount = deviceContext->InterfaceCount;
	for (interfaceIndex = 0; interfaceIndex < interfaceCount; interfaceIndex++)
	{
		pipeCount = deviceContext->InterfaceContext[interfaceIndex].PipeCount;
		for (pipeIndex = 0; pipeIndex < pipeCount; pipeIndex++)
		{
			pipeContext = deviceContext->InterfaceContext[interfaceIndex].PipeContextByIndex[pipeIndex];
			Policy_InitPipe(deviceContext, pipeContext);
		}
	}
}

VOID Policy_InitPipe(
    __in PDEVICE_CONTEXT deviceContext,
    __inout PPIPE_CONTEXT pipeContext)
{
	if (pipeContext && pipeContext->PipePoliciesInitialized == FALSE)
	{
		PIPE_POLICIES defaultPolicies;

		defaultPolicies.values = 0;
		defaultPolicies.AllowPartialReads = TRUE;
		defaultPolicies.IsoStartLatency = IsHighSpeedDevice(deviceContext) ? 32 : 8;

		defaultPolicies.IsoAlwaysStartAsap = USB_ENDPOINT_DIRECTION_IN(pipeContext->PipeInformation.EndpointAddress) ? FALSE : TRUE;
		pipeContext->Policies.values = defaultPolicies.values;
		pipeContext->PipePoliciesInitialized = TRUE;
	}
}

NTSTATUS Policy_GetPipe(__in PDEVICE_CONTEXT deviceContext,
                                   __in UCHAR pipeID,
                                   __in ULONG policyType,
                                   __out PVOID value,
                                   __inout PULONG valueLength)
{
	PPIPE_CONTEXT pipeContext = NULL;
	NTSTATUS status = STATUS_SUCCESS;
	PIPE_POLICIES pipePolicies;

	pipeContext = GetPipeContextByID(deviceContext, pipeID);
	if (!pipeContext->IsValid || !pipeContext->Pipe || !pipeContext->Queue)
	{
		status = STATUS_INVALID_PARAMETER;
		USBERRN("Invalid PipeID=%02Xh", pipeID);
		return status;
	}

	if (!valueLength)
	{
		status = STATUS_INVALID_BUFFER_SIZE;
		USBERRN("PipeID=%02Xh Invalid BufferLength.", pipeID);
		return status;
	}

	RtlCopyMemory(&pipePolicies, &pipeContext->Policies, sizeof(pipePolicies));

	switch(policyType)
	{
	case SHORT_PACKET_TERMINATE:	// 0x01
		mPipe_CheckValueLength(status, policyType, pipeID, 1, valueLength[0], valueLength, goto Done);
		if (value) ((PUCHAR)value)[0] = (UCHAR)pipePolicies.ShortPacketTerminate;
		break;

	case AUTO_CLEAR_STALL:			// 0x02
		mPipe_CheckValueLength(status, policyType, pipeID, 1, valueLength[0], valueLength, goto Done);
		if (value) ((PUCHAR)value)[0] = (UCHAR)pipePolicies.AutoClearStall;
		break;

	case PIPE_TRANSFER_TIMEOUT:		// 0x03
		mPipe_CheckValueLength(status, policyType, pipeID, sizeof(ULONG), valueLength[0], valueLength, goto Done);
		if (value) ((PULONG)value)[0] = pipeContext->TimeoutPolicy;
		break;

	case IGNORE_SHORT_PACKETS:		// 0x04
		mPipe_CheckValueLength(status, policyType, pipeID, 1, valueLength[0], valueLength, goto Done);
		if (value) ((PUCHAR)value)[0] = (UCHAR)pipePolicies.IgnoreShortPackets;
		break;

	case ALLOW_PARTIAL_READS:		// 0x05
		mPipe_CheckValueLength(status, policyType, pipeID, 1, valueLength[0], valueLength, goto Done);
		if (value) ((PUCHAR)value)[0] = (UCHAR)pipePolicies.AllowPartialReads;
		break;

	case AUTO_FLUSH:				// 0x06
		mPipe_CheckValueLength(status, policyType, pipeID, 1, valueLength[0], valueLength, goto Done);
		if (value) ((PUCHAR)value)[0] = (UCHAR)pipePolicies.AutoFlush;
		break;

	case RAW_IO:					// 0x07
		mPipe_CheckValueLength(status, policyType, pipeID, 1, valueLength[0], valueLength, goto Done);
		if (value) ((PUCHAR)value)[0] = (UCHAR)pipePolicies.RawIO;
		break;

	case MAXIMUM_TRANSFER_SIZE:		// 0x08
		mPipe_CheckValueLength(status, policyType, pipeID, sizeof(ULONG), valueLength[0], valueLength, goto Done);
		if (value) ((PULONG)value)[0] = pipeContext->PipeInformation.MaximumTransferSize;
		break;

	case RESET_PIPE_ON_RESUME:		// 0x09
		mPipe_CheckValueLength(status, policyType, pipeID, 1, valueLength[0], valueLength, goto Done);
		if (value) ((PUCHAR)value)[0] = (UCHAR)pipePolicies.ResetPipeOnResume;
		break;

	case ISO_START_LATENCY:			// 0x20
		mPipe_CheckValueLength(status, policyType, pipeID, 2, valueLength[0], valueLength, goto Done);
		if (value) ((PUSHORT)value)[0] = (USHORT)pipePolicies.IsoStartLatency;
		break;

	case ISO_ALWAYS_START_ASAP:		// 0x21
		mPipe_CheckValueLength(status, policyType, pipeID, 1, valueLength[0], valueLength, goto Done);
		if (value) ((PUCHAR)value)[0] = (UCHAR)pipePolicies.IsoAlwaysStartAsap;
		break;
	case ISO_NUM_FIXED_PACKETS:		// 0x22
		mPipe_CheckValueLength(status, policyType, pipeID, 2, valueLength[0], valueLength, goto Done);
		if (value) ((PUSHORT)value)[0] = (USHORT)pipePolicies.IsoNumFixedPackets;
		break;
	case SIMUL_PARALLEL_REQUESTS:	// 0x30
		mPipe_CheckValueLength(status, policyType, pipeID, sizeof(ULONG), valueLength[0], valueLength, goto Done);
		if (value) ((PULONG)value)[0] = pipeContext->SimulParallelRequests;
		break;
	default:
		status = STATUS_INVALID_PARAMETER;
	}

Done:
	if (NT_SUCCESS(status))
	{
		if (value && valueLength)
		{
			USBMSGN("PipeID=%02Xh returned value of length %u for policy type %u",
			        pipeContext->PipeInformation.EndpointAddress, valueLength[0], policyType);
		}
	}
	else
	{
		USBERRN("PipeID=%02Xh Failed appying policy type %u. Status=%08Xh",
		        pipeID, policyType, status);
	}

	return status;
}

NTSTATUS Policy_InitPower(__in PDEVICE_CONTEXT deviceContext)
{

	NTSTATUS status = STATUS_SUCCESS;
	WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS idleSettings;
	WDF_DEVICE_POWER_POLICY_WAKE_SETTINGS wakeSettings;

	PAGED_CODE();

	/* DeviceIdleIgnoreWakeEnable
	Use this value if your USB-connected device supports both idling and
	waking itself while the computer is in its working state. If your USB
	device supports only idling, use IdleCannotWakeFromS0. (Drivers for USB
	devices must not specify IdleCanWakeFromS0.)
	*/
	if (deviceContext->UsbVersionInfo.USBDI_Version < 0x600)
		deviceContext->DeviceRegSettings.DeviceIdleIgnoreWakeEnable = FALSE;

	/* IdleSettings.IdleCaps
	For Windows XP, the framework supports USB selective suspend only if the
	device's USB_CONFIGURATION_DESCRIPTOR structure shows that the device
	supports remote wakeup. For Windows Vista and later versions of Windows,
	the framework supports USB selective suspend whether or not the device
	supports remote wakeup.
	*/
	if (deviceContext->RemoteWakeCapable || deviceContext->DeviceRegSettings.DeviceIdleIgnoreWakeEnable)
		WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS_INIT(&idleSettings, IdleUsbSelectiveSuspend);
	else
		WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS_INIT(&idleSettings, IdleCannotWakeFromS0);

	// IdleSettings.UserControlOfIdleSettings
	if (!deviceContext->DeviceRegSettings.UserSetDeviceIdleEnabled)
		idleSettings.UserControlOfIdleSettings = IdleDoNotAllowUserControl;
	else
		idleSettings.UserControlOfIdleSettings = IdleAllowUserControl;

	// IdleSettings.Enabled
	if (deviceContext->DeviceRegSettings.DeviceIdleEnabled && deviceContext->DeviceRegSettings.DefaultIdleState)
		idleSettings.Enabled = WdfTrue;
	else
		idleSettings.Enabled = WdfFalse;

	idleSettings.IdleTimeout = deviceContext->DeviceRegSettings.DefaultIdleTimeout;

	/*
	Assign idle settings.
	NOTE: WdfDeviceAssignS0IdleSettings is not called until idle settings have been enabled for the first time.
	*/
	if (idleSettings.Enabled || (!idleSettings.Enabled && deviceContext->IsIdleSettingsInitialized))
	{
		USBMSGN("Assigning IdleSettings. Enabled=%u IdleTimeout=%u UserControl=%s IdleCaps=%s",
		        idleSettings.Enabled,
		        idleSettings.IdleTimeout,
		        (idleSettings.UserControlOfIdleSettings == IdleAllowUserControl) ? "Allow" : "DoNotAllow",
		        (idleSettings.IdleCaps == IdleUsbSelectiveSuspend) ? "SelectiveSuspend" : "CannotWakeFromS0");

		status = WdfDeviceAssignS0IdleSettings(deviceContext->WdfDevice, &idleSettings);
		if ( !NT_SUCCESS(status))
		{
			USBERR("WdfDeviceSetPowerPolicyS0IdlePolicy failed. status=%Xh\n", status);
			return status;
		}
		deviceContext->IsIdleSettingsInitialized = TRUE;
	}

	WDF_DEVICE_POWER_POLICY_WAKE_SETTINGS_INIT(&wakeSettings);

	// WakeSettings.Enabled
	wakeSettings.Enabled = deviceContext->DeviceRegSettings.SystemWakeEnabled ? WdfTrue : WdfFalse;

	// WakeSettings.UserControlOfWakeSettings
	wakeSettings.UserControlOfWakeSettings = wakeSettings.Enabled;

	/*
	Assign wake settings.
	*/
	if (wakeSettings.Enabled)
	{
		USBMSGN("Assigning WakeSettings. Enabled=%u UserControl=%u",
		        wakeSettings.Enabled, wakeSettings.UserControlOfWakeSettings);

		status = WdfDeviceAssignSxWakeSettings(deviceContext->WdfDevice, &wakeSettings);
		if (!NT_SUCCESS(status))
		{
			USBERR("WdfDeviceAssignSxWakeSettings failed. status=%Xh\n", status);
			return status;
		}
	}

	return status;
}

NTSTATUS Policy_SetPower(__inout PDEVICE_CONTEXT deviceContext,
                         __in ULONG policyType,
                         __in PVOID value,
                         __in ULONG valueLength)
{

	ULONG polBackup;
	NTSTATUS status = STATUS_SUCCESS;

	switch(policyType)
	{
	case AUTO_SUSPEND:

		mPower_CheckValueLength(status, policyType, 1, valueLength, NULL, goto Done);
		polBackup = deviceContext->DeviceRegSettings.DefaultIdleState;
		deviceContext->DeviceRegSettings.DefaultIdleState = ((PUCHAR)value)[0] ? 1 : 0;

		status = Policy_InitPower(deviceContext);

		if (!NT_SUCCESS(status))
			deviceContext->DeviceRegSettings.DefaultIdleState = polBackup;
		break;
	case SUSPEND_DELAY:
		mPower_CheckValueLength(status, policyType, 4, valueLength, NULL, goto Done);
		polBackup = deviceContext->DeviceRegSettings.DefaultIdleTimeout;
		deviceContext->DeviceRegSettings.DefaultIdleTimeout = ((PULONG)value)[0];

		status = Policy_InitPower(deviceContext);

		if (!NT_SUCCESS(status))
			deviceContext->DeviceRegSettings.DefaultIdleTimeout = polBackup;
		break;
	default:
		USBERR("unknown power policy %d\n", policyType);
		status = STATUS_INVALID_PARAMETER;

	}

Done:
	return status;
}

NTSTATUS Policy_GetPower(__in PDEVICE_CONTEXT deviceContext,
                         __in ULONG policyType,
                         __out PVOID value,
                         __inout PULONG valueLength)
{
	NTSTATUS status = STATUS_SUCCESS;

	switch(policyType)
	{
	case AUTO_SUSPEND:
		mPower_CheckValueLength(status, policyType, 1, valueLength[0], valueLength, goto Done);
		if (value) ((PUCHAR)value)[0] = (UCHAR)deviceContext->DeviceRegSettings.DefaultIdleState;
		break;
	case SUSPEND_DELAY:
		mPower_CheckValueLength(status, policyType, 4, valueLength[0], valueLength, goto Done);
		if (value) ((PULONG)value)[0] = deviceContext->DeviceRegSettings.DefaultIdleTimeout;
		break;
	default:
		USBERR("unknown power policy %d\n", policyType);
		status = STATUS_INVALID_PARAMETER;

	}

Done:
	return status;
}

NTSTATUS Policy_GetDevice(__in PDEVICE_CONTEXT deviceContext,
                          __in ULONG policyType,
                          __out PVOID value,
                          __inout PULONG valueLength)
{
	NTSTATUS status = STATUS_SUCCESS;

	switch(policyType)
	{
	case DEVICE_SPEED:
		mPower_CheckValueLength(status, policyType, 1, valueLength[0], valueLength, goto Done);
		if (value) ((PUCHAR)value)[0] = (UCHAR)deviceContext->DeviceSpeed + 1;
		break;
	default:
		USBERR("unknown device information type %d\n", policyType);
		status = STATUS_INVALID_PARAMETER;

	}

Done:
	return status;
}

/*
NTSTATUS Policy_ApplyIsoAutoPacketTemplate(
    __inout PDEVICE_CONTEXT deviceContext,
    __in PPIPE_CONTEXT pipeContext,
    __in PVOID value,
    __in ULONG valueLength)
{
	BOOLEAN isHS;
	SHORT minPackets;
	SHORT maxPackets;
	SHORT numPackets;
	int lastOffset, iPacket;
	WDF_OBJECT_ATTRIBUTES memAttributes;
	NTSTATUS status;
	WDFMEMORY sharedMemory;
	KISO_PACKET* isoPackets;


	if (pipeContext->PipeInformation.PipeType != WdfUsbPipeTypeIsochronous || !pipeContext->Queue)
	{
		USBERRN("PipeID=%02Xh Invalid PipeType=%s",
		        pipeContext->PipeInformation.EndpointAddress, GetPipeTypeString(pipeContext->PipeInformation.PipeType));
		return STATUS_INVALID_PARAMETER;
	}

	if (valueLength == 0)
	{
		if (pipeContext->SharedAutoIsoExPacketMemory)
		{
			WdfObjectAcquireLock(pipeContext->Queue);

			WdfObjectDelete(pipeContext->SharedAutoIsoExPacketMemory);
			pipeContext->SharedAutoIsoExPacketMemory = NULL;

			WdfObjectReleaseLock(pipeContext->Queue);
		}

		return STATUS_SUCCESS;
	}

	numPackets = (SHORT)((valueLength - sizeof(KISO_CONTEXT)) / sizeof(KISO_PACKET));
	if ((valueLength - sizeof(KISO_CONTEXT)) % sizeof(KISO_PACKET))
	{
		USBERRN("PipeID=%02Xh BufferLength not an interval of %u sizeof(KISO_PACKET)",
		        pipeContext->PipeInformation.EndpointAddress, sizeof(KISO_PACKET));
		return STATUS_INVALID_PARAMETER;
	}

	isHS = IsHighSpeedDevice(deviceContext);
	if (isHS)
	{
		minPackets = 8;
		maxPackets = 1024;
	}
	else
	{
		minPackets = 1;
		maxPackets = 255;
	}

	if (numPackets % minPackets || numPackets > maxPackets || numPackets < minPackets)
	{
		USBERRN("PipeID=%02Xh NumberOfPackets must be an interval of %u and less than or equal to %u",
		        pipeContext->PipeInformation.EndpointAddress, minPackets, maxPackets);
		return STATUS_INVALID_PARAMETER;
	}

	lastOffset	= 0;
	isoPackets	= (KISO_PACKET*) & (((PUCHAR)value)[sizeof(KISO_CONTEXT)]);
	for (iPacket = 0; iPacket < (int)numPackets; iPacket++)
	{
		KISO_PACKET* nextIsoPacket = &isoPackets[iPacket];

		if ((int)nextIsoPacket->Offset < lastOffset || (int)nextIsoPacket->Offset - lastOffset > (int)pipeContext->PipeInformation.MaximumPacketSize)
		{
			USBERRN("PipeID=%02Xh IsoPacket[%u] to long. Length=%u",
			        pipeContext->PipeInformation.EndpointAddress, iPacket - 1, (int)nextIsoPacket->Offset - lastOffset);
			return STATUS_INVALID_PARAMETER;
		}
	}

	WDF_OBJECT_ATTRIBUTES_INIT(&memAttributes);
	memAttributes.ParentObject = deviceContext->WdfDevice;
	status = WdfMemoryCreate(&memAttributes, NonPagedPool, POOL_TAG, valueLength, &sharedMemory, NULL);
	if (!NT_SUCCESS(status))
	{
		USBERRN("WdfMemoryCreate failed. Status=%08Xh", status);
		return status;
	}

	status = WdfMemoryCopyFromBuffer(sharedMemory, 0, value, valueLength);
	if (!NT_SUCCESS(status))
	{
		USBERRN("WdfMemoryCopyFromBuffer failed. Status=%08Xh", status);
		return status;
	}

	WdfObjectAcquireLock(pipeContext->Queue);

	sharedMemory = InterlockedExchangePointer(&pipeContext->SharedAutoIsoExPacketMemory, sharedMemory);
	if (sharedMemory) WdfObjectDelete(sharedMemory);

	WdfObjectReleaseLock(pipeContext->Queue);

	USBMSGN("PipeID=%02Xh Assigned %u iso packet descriptors.", pipeContext->PipeInformation.EndpointAddress, numPackets);
	return STATUS_SUCCESS;
}
*/

NTSTATUS Policy_SetPipe(
    __inout PDEVICE_CONTEXT deviceContext,
    __in UCHAR pipeID,
    __in ULONG policyType,
    __in PVOID value,
    __in ULONG valueLength)
{
	PPIPE_CONTEXT pipeContext = NULL;
	NTSTATUS status = STATUS_SUCCESS;
	PIPE_POLICIES pipePolicies;
	ULONG tempVal;

	pipeContext = GetPipeContextByID(deviceContext, pipeID);
	if (!pipeContext->IsValid)
	{
		status = STATUS_INVALID_PARAMETER;
		USBERRN("Invalid PipeID=%02Xh", pipeID);
		return status;
	}

	pipePolicies.values = pipeContext->Policies.values;

	switch(policyType)
	{
	case SHORT_PACKET_TERMINATE:	// 0x01
		mPipe_CheckValueLength(status, policyType, pipeID, 1, valueLength, NULL, goto Done);
		pipePolicies.ShortPacketTerminate = ((PUCHAR)value)[0] ? 1 : 0;
		break;

	case AUTO_CLEAR_STALL:			// 0x02
		mPipe_CheckValueLength(status, policyType, pipeID, 1, valueLength, NULL, goto Done);
		pipePolicies.AutoClearStall = ((PUCHAR)value)[0] ? 1 : 0;
		break;

	case PIPE_TRANSFER_TIMEOUT:		// 0x03
		mPipe_CheckValueLength(status, policyType, pipeID,  sizeof(ULONG), sizeof(ULONG), NULL, goto Done);
		pipeContext->TimeoutPolicy = ((PULONG)value)[0];
		break;

	case IGNORE_SHORT_PACKETS:		// 0x04
		mPipe_CheckValueLength(status, policyType, pipeID, 1, valueLength, NULL, goto Done);
		pipePolicies.IgnoreShortPackets = ((PUCHAR)value)[0] ? 1 : 0;
		break;

	case ALLOW_PARTIAL_READS:		// 0x05
		mPipe_CheckValueLength(status, policyType, pipeID, 1, valueLength, NULL, goto Done);
		pipePolicies.AllowPartialReads = ((PUCHAR)value)[0] ? 1 : 0;
		break;

	case AUTO_FLUSH:				// 0x06
		mPipe_CheckValueLength(status, policyType, pipeID, 1, valueLength, NULL, goto Done);
		pipePolicies.AutoFlush = ((PUCHAR)value)[0] ? 1 : 0;
		break;

	case RAW_IO:					// 0x07
		if (pipeContext->PipeInformation.PipeType != WdfUsbPipeTypeBulk &&
		        pipeContext->PipeInformation.PipeType != WdfUsbPipeTypeInterrupt)
		{
			status = STATUS_INVALID_PARAMETER;
			break;
		}
		mPipe_CheckValueLength(status, policyType, pipeID, 1, valueLength, NULL, goto Done);
		tempVal = ((PUCHAR)value)[0];

		if ((tempVal & 0x1) != pipePolicies.RawIO)
		{
			pipePolicies.RawIO = ((PUCHAR)value)[0] ? 1 : 0;
			pipeContext->IsQueueDirty = TRUE;
		}
		break;

	case RESET_PIPE_ON_RESUME:		// 0x09
		mPipe_CheckValueLength(status, policyType, pipeID, 1, valueLength, NULL, goto Done);
		pipePolicies.ResetPipeOnResume = ((PUCHAR)value)[0] ? 1 : 0;
		break;

	case ISO_START_LATENCY:			// 0x20
		mPipe_CheckValueLength(status, policyType, pipeID, 2, valueLength, NULL, goto Done);
		pipePolicies.IsoStartLatency = ((PUSHORT)value)[0];
		break;

	case ISO_ALWAYS_START_ASAP:		// 0x21
		mPipe_CheckValueLength(status, policyType, pipeID, 1, valueLength, NULL, goto Done);
		pipePolicies.IsoAlwaysStartAsap = ((PUCHAR)value)[0] ? 1 : 0;
		break;

	case ISO_NUM_FIXED_PACKETS:		// 0x22
		mPipe_CheckValueLength(status, policyType, pipeID, 2, valueLength, NULL, goto Done);
		pipePolicies.IsoNumFixedPackets = ((PUSHORT)value)[0] & 0x7FF;
		if (pipePolicies.IsoNumFixedPackets)
		{
			if (IsHighSpeedDevice(deviceContext))
			{
				if (pipePolicies.IsoNumFixedPackets % 8)
				{
					USBERRN("Number of fixed packets must be an interval of 8.");
					status = STATUS_INVALID_PARAMETER;
					break;
				}
				if (pipePolicies.IsoNumFixedPackets > 1024)
				{
					USBERRN("Number of fixed packets cannot be greater than 1024.");
					status = STATUS_INVALID_PARAMETER;
					break;
				}
			}
			else
			{
				if (pipePolicies.IsoNumFixedPackets > 256)
				{
					USBERRN("Number of fixed packets cannot be greater than 256.");
					status = STATUS_INVALID_PARAMETER;
					break;
				}
			}
		}
		break;

	case SIMUL_PARALLEL_REQUESTS:
		mPipe_CheckValueLength(status, policyType, pipeID, 4, valueLength, NULL, goto Done);
		if (pipeContext->SimulParallelRequests != ((PULONG)value)[0])
		{
			pipeContext->SimulParallelRequests = ((PULONG)value)[0];
			pipeContext->IsQueueDirty = TRUE;
		}
		break;
		/*
		case ISO_AUTO_PACKET_TEMPLATE:	// 0x10
		status = Policy_ApplyIsoAutoPacketTemplate(deviceContext, pipeContext, value, valueLength);
		break;
		*/


	default:
		status = STATUS_INVALID_PARAMETER;
	}

Done:
	if (NT_SUCCESS(status))
	{
		USBMSGN("PipeID=%02Xh Applied value of length %u to policy type %u",
		        pipeContext->PipeInformation.EndpointAddress, valueLength, policyType);

		pipeContext->Policies.values = pipePolicies.values;

	}
	else
	{
		USBERRN("PipeID=%02Xh Failed appying policy type %u. Status=%08Xh",
		        pipeID, policyType, status);
	}

	return status;

}
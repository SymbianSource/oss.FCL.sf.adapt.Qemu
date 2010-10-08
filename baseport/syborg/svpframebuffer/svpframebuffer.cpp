/*
* Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
* All rights reserved.
* This component and the accompanying materials are made available
* under the terms of the License "Eclipse Public License v1.0"
* which accompanies this distribution, and is available
* at the URL "http://www.eclipse.org/legal/epl-v10.html".
*
* Initial Contributors:
* Nokia Corporation - initial contribution.
*
* Contributors:
*
* Accenture Ltd - Syborg framebuffer improvements, now auto determines frame size from board model, performance and memory improvements
*
* Description: Minimalistic frame buffer driver
*
*/
#include <videodriver.h>
#include "platform.h"
#include <nkern.h>
#include <kernel/kernel.h>
#include <kernel/kern_priv.h>
#include <kernel/kpower.h>

#include <syborg_priv.h>
#include "svpframebuffer.h"

DLcdPowerHandler	*  DLcdPowerHandler::pLcd			= NULL;

TPhysAddr Syborg::VideoRamPhys;
TPhysAddr Syborg::VideoRamPhysSecure;		// Secure display memory

TPhysAddr TSyborg::VideoRamPhys()
{
  __KTRACE_OPT(KEXTENSION,Kern::Printf("TSyborg::VideoRamPhys: VideoRamPhys=0x%x", Syborg::VideoRamPhys));
  return Syborg::VideoRamPhys;
}

TPhysAddr TSyborg::VideoRamPhysSecure()
{
  return Syborg::VideoRamPhysSecure;
}

LOCAL_C TInt DoHalFunction(TAny* aPtr, TInt aFunction, TAny* a1, TAny* a2)
{
  DLcdPowerHandler* pH=(DLcdPowerHandler*)aPtr;
  return pH->HalFunction(aFunction,a1,a2);
}

static void rxMsg(TAny* aPtr)
{
  DLcdPowerHandler& h=*(DLcdPowerHandler*)aPtr;
  TMessageBase* pM = h.iMsgQ.iMessage;
  if(pM)
	h.HandleMsg(pM);
}

static void power_up_dfc(TAny* aPtr)
{
  ((DLcdPowerHandler*)aPtr)->PowerUpDfc();
}

void power_down_dfc(TAny* aPtr)
{
  ((DLcdPowerHandler*)aPtr)->PowerDownDfc();
}

void DLcdPowerHandler::DisplayOn()
{ 
  PowerUpLcd(iSecureDisplay);
  iDisplayOn = ETrue;
}

void DLcdPowerHandler::DisplayOff()
{
  PowerDownLcd();
  iDisplayOn = EFalse;
}

void DLcdPowerHandler::SwitchDisplay(TBool aSecure)
 {
   if(aSecure)
	 {
	   if(!iSecureDisplay)
		 {
		   DisplayOff();
		   iSecureDisplay = ETrue;
		   DisplayOn();
		 }
	 }
   else
	 {
	   if(iSecureDisplay)
		 {
		   DisplayOff();
		   iSecureDisplay = EFalse;
		   DisplayOn();
		 }
	 }
 }

void DLcdPowerHandler::PowerUpDfc()
{
  DisplayOn();
  PowerUpDone();
}

void DLcdPowerHandler::PowerDownDfc()
{
  DisplayOff();

  PowerDownDone();
}

void DLcdPowerHandler::PowerDown(TPowerState)
{
  iPowerDownDfc.Enque();
}

void DLcdPowerHandler::PowerUp()
{
  iPowerUpDfc.Enque();
}

void DLcdPowerHandler::PowerUpLcd(TBool aSecure)
{

  WriteReg(iPortAddr, FB_ENABLED, 0);
  WriteReg(iPortAddr, FB_BASE, aSecure ? iSecurevRamPhys : ivRamPhys);

  WriteReg(iPortAddr, FB_BLANK, 0);
  WriteReg(iPortAddr, FB_BPP, 32);
  WriteReg(iPortAddr, FB_COLOR_ORDER, 0);
  WriteReg(iPortAddr, FB_BYTE_ORDER, 0);
  WriteReg(iPortAddr, FB_PIXEL_ORDER, 0);
  WriteReg(iPortAddr, FB_INT_MASK, 0);
  WriteReg(iPortAddr, FB_ENABLED, 1);

// We don't write the Height and Width of the framebuffer, this is controlled by the board model

}

void DLcdPowerHandler::PowerDownLcd()
{
  WriteReg(iPortAddr, FB_BLANK, 1);
}

DLcdPowerHandler::DLcdPowerHandler()
	:	DPowerHandler(KLitLcd),
		iMsgQ(rxMsg,this,NULL,1),
		iPowerUpDfc(&power_up_dfc,this,6),
		iPowerDownDfc(&power_down_dfc,this,7)
{
}

void DLcdPowerHandler::ScreenInfo(TScreenInfoV01& anInfo)
{
  anInfo.iWindowHandleValid = EFalse;
  anInfo.iWindowHandle = NULL;
  anInfo.iScreenAddressValid = ETrue;
  anInfo.iScreenAddress = (TAny *)(iChunk->LinearAddress());
  anInfo.iScreenSize.iWidth = iVideoInfo.iSizeInPixels.iWidth;
  anInfo.iScreenSize.iHeight = iVideoInfo.iSizeInPixels.iHeight;
}

void DLcdPowerHandler::HandleMsg(TMessageBase* aMsg)
{
  if(aMsg->iValue)
	DisplayOn();
  else
	DisplayOff();
  aMsg->Complete(KErrNone,ETrue);
}

void DLcdPowerHandler::WsSwitchOnScreen()
{
  TThreadMessage& m = Kern::Message();
  m.iValue = ETrue;
  m.SendReceive(&iMsgQ);
}

void DLcdPowerHandler::WsSwitchOffScreen()
{
  TThreadMessage& m = Kern::Message();
  m.iValue = EFalse;
  m.SendReceive(&iMsgQ);
}

TInt DLcdPowerHandler::GetCurrentDisplayModeInfo(TVideoInfoV01& aInfo, TBool aSecure)
{
  NKern::FMWait(&iLock);
  if(aSecure)
	aInfo = iSecureVideoInfo;
  else
	aInfo = iVideoInfo;
  NKern::FMSignal(&iLock);
  return KErrNone;
}

TInt DLcdPowerHandler::GetSpecifiedDisplayModeInfo(TInt aMode, TVideoInfoV01& aInfo)
{
  if(aMode < 0 || aMode >= KConfigLcdNumberOfDisplayModes)
	return KErrArgument;
  
  NKern::FMWait(&iLock);
  aInfo = iVideoInfo;
  NKern::FMSignal(&iLock);

  if(aMode != aInfo.iDisplayMode)
	{
	  aInfo.iOffsetToFirstPixel = KCOnfigOffsetToFirstPixel;
	  aInfo.iIsPalettized       = KConfigIsPalettized;
	  aInfo.iOffsetBetweenLines = iVideoInfo.iSizeInPixels.iWidth*4; //Offset depends on width of framebuffer
	  aInfo.iBitsPerPixel       = KConfigBitsPerPixel;
	}
  return KErrNone;
}

TInt DLcdPowerHandler::AllocateFrameBuffer()
{
	// Allocate physical RAM for video
	
	//read width and height of display from board model and allocate size
	TInt width = ReadReg(iPortAddr, FB_WIDTH);
	TInt height = ReadReg(iPortAddr, FB_HEIGHT);
	
	iSize = 4*width*height; //*4 as 32bits per pixel

	NKern::ThreadEnterCS();
	TInt r = Epoc::AllocPhysicalRam(iSize,Syborg::VideoRamPhys);
	if (r != KErrNone)
	{
	        NKern::ThreadLeaveCS();
		Kern::Fault("AllocVideoRam",r);
	}

	// Map the video RAM
	ivRamPhys = TSyborg::VideoRamPhys();

	r = DPlatChunkHw::New(iChunk,ivRamPhys,iSize,EMapAttrUserRw|EMapAttrBufferedC);

	NKern::ThreadLeaveCS();

	if(r != KErrNone)
	  return r;

	TUint* pV = (TUint*)iChunk->LinearAddress();

	// Allocate physical RAM for secure display
	NKern::ThreadEnterCS();
	r = Epoc::AllocPhysicalRam(iSize,Syborg::VideoRamPhysSecure);
	if (r != KErrNone)
	{
	        NKern::ThreadLeaveCS();
		Kern::Fault("AllocVideoRam 2",r);
	}
	iSecurevRamPhys = ivRamPhys + iSize;
	TInt r2 = DPlatChunkHw::New(iSecureChunk,iSecurevRamPhys,iSize,EMapAttrUserRw|EMapAttrBufferedC);

	NKern::ThreadLeaveCS();

	if(r2 != KErrNone)
	  return r2;

	TUint* pV2 = (TUint*)iSecureChunk->LinearAddress();

	//width and height set by reading board model
	iVideoInfo.iSizeInPixels.iWidth  = width;
	iVideoInfo.iSizeInPixels.iHeight = height;

	//offset between lines depends on width of screen
	iVideoInfo.iOffsetBetweenLines = width*4;

	iVideoInfo.iDisplayMode = KConfigLcdDisplayMode;
	iVideoInfo.iOffsetToFirstPixel = KConfigOffsetToFirstPixel;
	
	iVideoInfo.iIsPalettized = KConfigIsPalettized;
	iVideoInfo.iBitsPerPixel = KConfigBitsPerPixel;
	iVideoInfo.iSizeInTwips.iWidth = KConfigLcdWidthInTwips;
	iVideoInfo.iSizeInTwips.iHeight = KConfigLcdHeightInTwips;
	iVideoInfo.iIsMono = KConfigIsMono;
	iVideoInfo.iVideoAddress = (TInt)pV;
	iVideoInfo.iIsPixelOrderLandscape = KConfigPixelOrderLandscape;
	iVideoInfo.iIsPixelOrderRGB = KConfigPixelOrderRGB;

	iSecureVideoInfo = iVideoInfo;
	iSecureVideoInfo.iVideoAddress = (TInt)pV2;

	// Alloc Physical RAM for the Composition Buffers used by OpenWF
	// double and round the page size
	TUint round = 2*Kern::RoundToPageSize(iSize);

	r=Epoc::AllocPhysicalRam(round , iCompositionPhysical);
	if(r!=KErrNone)
		{
		return r;
		}
	
	return KErrNone;
}


TInt DLcdPowerHandler::SetDisplayMode(TInt aMode)
{
  if(aMode < 0 || aMode >= KConfigLcdNumberOfDisplayModes)
	return KErrArgument;

  // store the current mode
  iVideoInfo.iDisplayMode = aMode;

  // store the current mode for secure screen
  iSecureVideoInfo.iDisplayMode = aMode;

  return KErrNone;
}

TInt DLcdPowerHandler::HalFunction(TInt aFunction, TAny* a1, TAny* a2)
{
  TInt r=KErrNone;
  switch(aFunction)
	{
	case EDisplayHalScreenInfo:
	  {
		TPckgBuf<TScreenInfoV01> vPckg;
		ScreenInfo(vPckg());
		Kern::InfoCopy(*(TDes8*)a1,vPckg);
		break;
	  }
	case EDisplayHalWsRegisterSwitchOnScreenHandling:
	  {
		iWsSwitchOnScreen=(TBool)a1;
		break;
	  }
	case EDisplayHalWsSwitchOnScreen:
	  {
		WsSwitchOnScreen();
		break;
	  }
	case EDisplayHalModeCount:
	  {
		TInt ndm = KConfigLcdNumberOfDisplayModes;
		kumemput32(a1, &ndm, sizeof(ndm));
		break;
	  }
	case EDisplayHalSetMode:
	  {
		__KTRACE_OPT(KEXTENSION,Kern::Printf("EDisplayHalSetMode"));
		__SECURE_KERNEL(
						if(!Kern::CurrentThreadHasCapability(ECapabilityMultimediaDD,__PLATSEC_DIAGNOSTIC_STRING("Checked by Hal function EDisplayHalSetMode")))
						return KErrPermissionDenied;
						)
		  r = SetDisplayMode((TInt)a1);
		break;
	  }
	case EDisplayHalMode:
	  {
		kumemput32(a1, &iVideoInfo.iDisplayMode, sizeof(iVideoInfo.iDisplayMode));
		r = KErrNone;
		break;
	  }
	case EDisplayHalSetPaletteEntry:
	  {
		__SECURE_KERNEL(
						if(!Kern::CurrentThreadHasCapability(ECapabilityMultimediaDD,__PLATSEC_DIAGNOSTIC_STRING("Checked by Hal function EDisplayHalSetPaletteEntry")))
						return KErrPermissionDenied;
						)
		  r = KErrNotSupported;
		break;
	  }
	case EDisplayHalPaletteEntry:
	  {
		TInt entry;
		kumemget32(&entry, a1, sizeof(TInt));
		r = KErrNotSupported;
		break;
	  }
	case EDisplayHalSetState:
	  {
		__SECURE_KERNEL(
						if(!Kern::CurrentThreadHasCapability(ECapabilityPowerMgmt,__PLATSEC_DIAGNOSTIC_STRING("Checked by Hal function EDisplayHalSetState")))
						return KErrPermissionDenied;
						)
		  if((TBool)a1)
			WsSwitchOnScreen();
		  else
			WsSwitchOffScreen();
		break;
	  }
	case EDisplayHalState:
	  {
		kumemput32(a1, &iDisplayOn, sizeof(TBool));
		break;
	  }
	case EDisplayHalColors:
	  {
		TInt mdc = 1<<24;
		kumemput32(a1, &mdc, sizeof(mdc));
		break;
	  }
	case EDisplayHalCurrentModeInfo:
	  {
		TPckgBuf<TVideoInfoV01> vPckg;
		r = GetCurrentDisplayModeInfo(vPckg(), (TBool)a2);
		if(KErrNone == r)
		  Kern::InfoCopy(*(TDes8*)a1,vPckg);
		break;
	  }
	case EDisplayHalSpecifiedModeInfo:
	  {
		TPckgBuf<TVideoInfoV01> vPckg;
		TInt mode;
		kumemget32(&mode, a1, sizeof(mode));
		r = GetSpecifiedDisplayModeInfo(mode, vPckg());
		if(KErrNone == r)
		  Kern::InfoCopy(*(TDes8*)a2,vPckg);
		break;
	  }	
	case EDisplayHalSecure:
	  {
		kumemput32(a1, &iSecureDisplay, sizeof(TBool));
		break;
	  }
	case EDisplayHalSetSecure:
	  {
		__SECURE_KERNEL(
						if(!Kern::CurrentThreadHasCapability(ECapabilityMultimediaDD,__PLATSEC_DIAGNOSTIC_STRING("Checked by Hal function EDisplayHalSetSecure")))
						return KErrPermissionDenied;
						)
		  SwitchDisplay((TBool)a1);
		break;
	  }
	default:
	  {
		r = KErrNotSupported;
		break;
	  }			
	}
  return r;
}

TInt DLcdPowerHandler::Create()
{
	__KTRACE_OPT(KEXTENSION ,Kern::Printf("DLcdPowerHandler::Create") );
	pLcd			= this;

	iPortAddr = KHwBaseClcd;

	TInt r = AllocateFrameBuffer();
	if(r == KErrNone)
		{
		r = Kern::AddHalEntry(EHalGroupDisplay,DoHalFunction,this);
		}
	    
	if(r != KErrNone)
		{
		__KTRACE_OPT(KEXTENSION ,Kern::Printf("DLcdPowerHandler::Create failed %d", r) );
		return r;
		}

	iPowerUpDfc.SetDfcQ(iDfcQ);
	iPowerDownDfc.SetDfcQ(iDfcQ);
	iMsgQ.SetDfcQ(iDfcQ);
	iMsgQ.Receive();

	Add();
	DisplayOn();

	return KErrNone;
}

/**
 * Register the call back function.
 * Components interested in receiving notification of the Vsync interrupt should register a callback function.
 */
EXPORT_C TInt DLcdPowerHandler::RegisterCallback(TLcdUserCallBack* aCbPtr)
{
	__KTRACE_OPT(KEXTENSION ,Kern::Printf("DLcdPowerHandler::RegisterCallBack %08x\n",aCbPtr->iCbFn) );

	TInt irq=__SPIN_LOCK_IRQSAVE(callbackLock);

	if(aCbPtr != NULL)
		{
		if ( pLcd->iAppCallBk[0] == NULL  )
			{
			pLcd->iAppCallBk[0] = aCbPtr;
			}
		else
			{
			if((pLcd->iAppCallBk[1] == NULL) && (pLcd->iAppCallBk[0]->iCbFn != aCbPtr->iCbFn))
				{
				pLcd->iAppCallBk[1] = aCbPtr;
				}
			else
				{
				__SPIN_UNLOCK_IRQRESTORE(callbackLock,irq);
				return KErrInUse;
				}
			}

		__SPIN_UNLOCK_IRQRESTORE(callbackLock,irq);
		__KTRACE_OPT(KEXTENSION ,Kern::Printf("<DLcdPowerHandler::RegisterCallBack ok %08x\n",aCbPtr->iCbFn) );
		return KErrNone;
		}
	else
		{
		__SPIN_UNLOCK_IRQRESTORE(callbackLock,irq);
		__KTRACE_OPT(KEXTENSION, Kern::Printf("Error: The supplied listener's callback is NULL"));
		return KErrArgument;
		}
}


/**
 *DeRegister the call back function
 */
EXPORT_C void DLcdPowerHandler::DeRegisterCallback(TLcdUserCallBack* aCbPtr)
{
	__KTRACE_OPT(KEXTENSION ,Kern::Printf("DLcdPowerHandler::DeRegisterCallBack %08x\n ",aCbPtr->iCbFn)  );

	TInt irq=__SPIN_LOCK_IRQSAVE(callbackLock);
	if(aCbPtr != NULL)
		{
	    if( pLcd->iAppCallBk[0] != NULL)
			{
			if ( (pLcd->iAppCallBk[0]->iDataPtr == aCbPtr->iDataPtr) && (pLcd->iAppCallBk[0]->iCbFn == aCbPtr->iCbFn) )
				{
				pLcd->iAppCallBk[0] = NULL;
				}
			}

		if( pLcd->iAppCallBk[1] != NULL)
			{
			if ( (pLcd->iAppCallBk[1]->iDataPtr == aCbPtr->iDataPtr) && (pLcd->iAppCallBk[1]->iCbFn == aCbPtr->iCbFn) )
				{
				pLcd->iAppCallBk[1] = NULL;
				}
			}
		}
	__SPIN_UNLOCK_IRQRESTORE(callbackLock,irq);
	__KTRACE_OPT(KEXTENSION ,Kern::Printf("<DLcdPowerHandler::DeRegisterCallBack %08x\n ",aCbPtr->iCbFn)  );
}

/**
	Constructor
*/
DDisplayPddSyborg::DDisplayPddSyborg():
	iPendingBuffer(NULL),
	iActiveBuffer(NULL),
	iChunk(NULL),
	iLcdCallback(NULL),
	iVSyncDfc(&VSyncDfcFn, this, KVSyncDfcPriority)
	{
	__GCE_DEBUG_PRINT("DDisplayPddSyborg::DDisplayPddSyborg\n");

	iPostFlag = EFalse;
	}

/**
	Destructor
*/
DDisplayPddSyborg::~DDisplayPddSyborg()
	{
	__GCE_DEBUG_PRINT("DDisplayPddSyborg::~DDisplayPddSyborg()  \n");

	if(iLcdCallback)
		{
		DLcdPowerHandler::pLcd->DeRegisterCallback(iLcdCallback) ;
		delete iLcdCallback;
		iLcdCallback = NULL;
		}

	//The DFC Queue is owned by DLcdPowerHandler so we shouldn't call Destroy() at this point.
	if (iDfcQ)
		{
		iDfcQ=NULL;
		}

	DChunk* chunk = (DChunk*) __e32_atomic_swp_ord_ptr(&iChunk, 0);

	if(chunk)
		{
		Kern::ChunkClose(chunk);
		}

	}

/**
    Set the Legacy Mode by setting the appropriate Frame control value.

*/
TInt DDisplayPddSyborg::SetLegacyMode()
	{
	__GCE_DEBUG_PRINT("DDisplayPddSyborg::SetLegacyMode()\n");

    return KErrNone;
	}

/**
    Set the GCE mode by posting a composition buffer.

*/
TInt DDisplayPddSyborg::SetGceMode()
	{
	__GCE_DEBUG_PRINT("DDisplayPddSyborg::SetGceMode()\n");

    PostCompositionBuffer(&iLdd->iCompositionBuffer[0]);
    return KErrNone;
	}

/**
	@param	aDegOfRot  The requested rotation
	@return KErrNone
*/
TInt DDisplayPddSyborg::SetRotation(RDisplayChannel::TDisplayRotation aDegOfRot)
	{
	return KErrNone;
	}

/**
    Remove any previous post operations, set the appropriate layer as the next layer to be displayed( This value is updated in synchronization
    with V Sync so it will take affect in the next V Sync after that) and also set the buffer provided as the buffer to
    be posted next. Layer 3 is associated with user buffers.

	@param	aNode  Pointer to the User buffer to post.
*/
TInt DDisplayPddSyborg::PostUserBuffer(TBufferNode* aNode)
	{

	__GCE_DEBUG_PRINT2("DDisplayPddSyborg::PostUserBuffer :  aNode->iAddress = %08x\n", aNode->iAddress);

	if(iPendingBuffer)
		{
		iPendingBuffer->iState = EBufferFree;
		if (!(iPendingBuffer->iType == EBufferTypeUser) )
			{
			iPendingBuffer->iFree  = ETrue;
			}
		}

	aNode->iState   = EBufferPending;
	iPendingBuffer	= aNode;
	iPostFlag		= ETrue;
	
  	// Activate the posted buffer
	TUint32 physicalAddress = Epoc::LinearToPhysical( aNode->iAddress );
  	WriteReg(DLcdPowerHandler::pLcd->iPortAddr, DLcdPowerHandler::FB_BASE, physicalAddress );
	/* Queue a DFC to complete the request*/
	iVSyncDfc.Enque();

	return KErrNone;
	}

/**
    Remove any previous post operations, set the appropriate layer as the next layer to be displayed( This value is updated in synchronization
    with V Sync so it will take affect in the next V Sync after that) and also set the buffer provided as the buffer to
    be posted next. Layer 1 and 2 are associated with composition buffers 0 and 1 respectively.

	@param	aNode  Pointer to the Composition buffer to post.
*/
TInt DDisplayPddSyborg::PostCompositionBuffer(TBufferNode* aNode)
	{

	__GCE_DEBUG_PRINT2("DDisplayPddSyborg::PostCompositionBuffer :  aNode->iAddress = %08x\n", aNode->iAddress);

	if(iPendingBuffer)
		{
		iPendingBuffer->iState = EBufferFree;
		if (iPendingBuffer->iType == EBufferTypeUser)
			{
			RequestComplete(RDisplayChannel::EReqPostUserBuffer, KErrCancel);
			}
		else
			{
			iPendingBuffer->iFree  = ETrue;
			}
		}
	aNode->iState	= EBufferPending;
	aNode->iFree	= EFalse;
	iPendingBuffer	= aNode;
	iPostFlag		= ETrue;

  	// Activate the posted buffer
	TUint32 physicalAddress = Epoc::LinearToPhysical( aNode->iAddress );
  	WriteReg(DLcdPowerHandler::pLcd->iPortAddr, DLcdPowerHandler::FB_BASE, physicalAddress );
	
	/* Queue a DFC to complete the request*/
	iVSyncDfc.Enque();

	return KErrNone;
	}

/**
    Remove any previous post operations, set the appropriate layer as the next layer to be displayed( This value is updated in synchronization
    with V Sync so it will take affect in the next V Sync after that) and also set the Legacy Buffer as the buffer to
    be posted next.Layer 0 is associated with legacy buffer.

	@param	aNode  Pointer to the Composition buffer to post.
*/
TInt DDisplayPddSyborg::PostLegacyBuffer()
	{
	__GCE_DEBUG_PRINT("DDisplayPddSyborg::PostLegacyBuffer() \n");

	if(iPendingBuffer)
		{
		iPendingBuffer->iState = EBufferFree;
		if (iPendingBuffer->iType == EBufferTypeUser)
			{

			RequestComplete(RDisplayChannel::EReqPostUserBuffer, KErrCancel);
			}
		else
			{
			iPendingBuffer->iFree  = ETrue;
			}
		}


	iLdd->iLegacyBuffer[0].iState		= EBufferPending;
	iLdd->iLegacyBuffer[0].iFree		= EFalse;
	iPendingBuffer						= &iLdd->iLegacyBuffer[0];
	iPostFlag		= ETrue;

  	// Activate the posted buffer
  	WriteReg(DLcdPowerHandler::pLcd->iPortAddr, DLcdPowerHandler::FB_BASE, TSyborg::VideoRamPhys() );

	/* Queue a DFC to complete the request*/
	iVSyncDfc.Enque();

	return KErrNone;
	}

/**
    Handles device specific operations when a close message has been sent to the Logical Channel.

*/
TInt DDisplayPddSyborg::CloseMsg()
	{
	__GCE_DEBUG_PRINT("DDisplayPddSyborg::CloseMsg()\n");

	iPendingBuffer  = NULL;
	iActiveBuffer	= NULL;

	iVSyncDfc.Cancel();
    return KErrNone;
	}

/**
    Called by the LDD's DoCreate function to handle the device specific part of opening the channel.
    (DoCreate is called by RDisplayChannel::Open)

	@param aUnit	The screen unit

    @return KErrNone if successful; or one of the other system wide error codes.
*/
TInt DDisplayPddSyborg::CreateChannelSetup(TInt aUnit)
	{
	__GCE_DEBUG_PRINT("DDisplayPddSyborg::CreateChannelSetup\n");

	iScreenInfo = DLcdPowerHandler::pLcd->iVideoInfo;
	iLdd->iUnit = aUnit;

	iLdd->iDisplayInfo.iAvailableRotations			= RDisplayChannel::ERotationNormal;
	iLdd->iDisplayInfo.iNormal.iOffsetBetweenLines	= iScreenInfo.iOffsetBetweenLines;
	iLdd->iDisplayInfo.iNormal.iHeight				= iScreenInfo.iSizeInPixels.iHeight;
	iLdd->iDisplayInfo.iNormal.iWidth				= iScreenInfo.iSizeInPixels.iWidth;
	iLdd->iDisplayInfo.iNumCompositionBuffers		= KDisplayCBMax;
	iLdd->iDisplayInfo.iBitsPerPixel				= iScreenInfo.iBitsPerPixel;
    iLdd->iDisplayInfo.iRefreshRateHz = 60;


	switch (iScreenInfo.iBitsPerPixel)
		{
		case 16:
			iLdd->iDisplayInfo.iPixelFormat = EUidPixelFormatRGB_565;
			break;
		case 24:
			iLdd->iDisplayInfo.iPixelFormat = EUidPixelFormatRGB_888;
			break;
		case 32:
			iLdd->iDisplayInfo.iPixelFormat = EUidPixelFormatXRGB_8888;
			break;
		default:
			iLdd->iDisplayInfo.iPixelFormat = EUidPixelFormatUnknown;
			break;
		}

	iLdd->iCurrentRotation = RDisplayChannel::ERotationNormal;

	// Open shared chunk to the composition framebuffer

	DChunk* chunk = 0;
	TLinAddr chunkKernelAddr  = 0;
	TUint32 chunkMapAttr = 0;

	// round to twice the page size
	TUint round  =  2*Kern::RoundToPageSize(DLcdPowerHandler::pLcd->iSize);

	__GCE_DEBUG_PRINT2("DDisplayPddSyborg::CreateChannelSetup DLcdPowerHandler::pLcd->iSize  = %d\n", DLcdPowerHandler::pLcd->iSize );

	TChunkCreateInfo info;
	info.iType					 = TChunkCreateInfo::ESharedKernelMultiple;
	info.iMaxSize				 = round;
	info.iMapAttr				 = EMapAttrFullyBlocking;
	info.iOwnsMemory			 = EFalse;
	info.iDestroyedDfc			 = 0;

	TInt r = Kern::ChunkCreate(info, chunk, chunkKernelAddr, chunkMapAttr);

	__GCE_DEBUG_PRINT2("CreateChannelSetup:ChunkCreate called for composition chunk. Set iChunkKernelAddr  = %08x\n", chunkKernelAddr );

	if( r == KErrNone)
		{
		// map our chunk
		r = Kern::ChunkCommitPhysical(chunk, 0,round , DLcdPowerHandler::pLcd->iCompositionPhysical);
		__GCE_DEBUG_PRINT2("Mapping chunk %d", r);
		if(r != KErrNone)
			{
			Kern::ChunkClose(chunk);
			}
		}

	if ( r!= KErrNone)
		{
		return r;
		}

	iChunk	= chunk;

	// init CB 0
	iLdd->iCompositionBuffer[0].iType			= EBufferTypeComposition;
	iLdd->iCompositionBuffer[0].iBufferId		= 0;
	iLdd->iCompositionBuffer[0].iFree			= ETrue;
	iLdd->iCompositionBuffer[0].iState			= EBufferFree;
	iLdd->iCompositionBuffer[0].iAddress		= chunkKernelAddr;
	iLdd->iCompositionBuffer[0].iPhysicalAddress = Epoc::LinearToPhysical(chunkKernelAddr);
	iLdd->iCompositionBuffer[0].iChunk			= chunk;
	iLdd->iCompositionBuffer[0].iHandle			= 0;
	iLdd->iCompositionBuffer[0].iOffset			= 0;
	iLdd->iCompositionBuffer[0].iSize			= DLcdPowerHandler::pLcd->iSize;
	iLdd->iCompositionBuffer[0].iPendingRequest = 0;

	// init CB 1
	iLdd->iCompositionBuffer[1].iType			= EBufferTypeComposition;
	iLdd->iCompositionBuffer[1].iBufferId		= 1;
	iLdd->iCompositionBuffer[1].iFree			= ETrue;
	iLdd->iCompositionBuffer[1].iState			= EBufferFree;
	iLdd->iCompositionBuffer[1].iAddress		= chunkKernelAddr + DLcdPowerHandler::pLcd->iSize;
	iLdd->iCompositionBuffer[1].iPhysicalAddress = Epoc::LinearToPhysical(chunkKernelAddr + DLcdPowerHandler::pLcd->iSize);
	iLdd->iCompositionBuffer[1].iChunk			= chunk;
	iLdd->iCompositionBuffer[1].iHandle			= 0;
	iLdd->iCompositionBuffer[1].iOffset			= DLcdPowerHandler::pLcd->iSize;
	iLdd->iCompositionBuffer[1].iSize			= DLcdPowerHandler::pLcd->iSize;
	iLdd->iCompositionBuffer[1].iPendingRequest = 0;

	iLdd->iCompositionBuffIdx					= 0;
	//Use the same DFC queue created by the DLcdPowerHandler so all hardware accesses are executed under the same DFC thread.
	iDfcQ= DLcdPowerHandler::pLcd->iDfcQ;

	// Set the Post DFC.
	iVSyncDfc.SetDfcQ(iDfcQ);


	return KErrNone;
	}

/**
Detect whether a post operation is pending
*/
TBool DDisplayPddSyborg::PostPending()
	{
	return (iPendingBuffer != NULL);
	}

/**
	Return the DFC queue to be used for this device.
 */
TDfcQue * DDisplayPddSyborg::DfcQ(TInt aUnit)
	{
	return iDfcQ;
	}

void DDisplayPddSyborg::VSyncDfcFn(TAny* aChannel)
	{
	DDisplayPddSyborg * channel =(DDisplayPddSyborg*)aChannel;

	if (channel->iPostFlag)
		{
		 channel->iPostFlag = EFalse;

		if (channel->iActiveBuffer)
			{
			//When a User buffer is registered its iFree member becomes EFalse and Deregister sets it
			//back to ETrue. Composition and Legacy buffers are not free when they are in the pending or
			//active state.
			if (channel->iActiveBuffer->iType == EBufferTypeUser)
				{
				channel->RequestComplete(RDisplayChannel::EReqPostUserBuffer, KErrNone);
				}
			else
				{
				channel->iActiveBuffer->iFree	= ETrue;
				}

			channel->iActiveBuffer->iState		= EBufferFree;


			//If no buffer was available during a call to GetCompositionBuffer the active buffer has
			//been returned as the next available one, so we must set the buffer to the proper state before we
			//send the notification.
			TInt pendingIndex = channel->iLdd->iPendingIndex[RDisplayChannel::EReqGetCompositionBuffer];
			if(channel->iLdd->iPendingReq[RDisplayChannel::EReqGetCompositionBuffer][pendingIndex].iTClientReq)
			{
				if(channel->iLdd->iPendingReq[RDisplayChannel::EReqGetCompositionBuffer][pendingIndex].iTClientReq->IsReady())
				{
				channel->iActiveBuffer->iState = EBufferCompose;
				channel->RequestComplete(RDisplayChannel::EReqGetCompositionBuffer,KErrNone);
				}

			}

			channel->iActiveBuffer				= NULL;
			}

		if (channel->iPendingBuffer)
			{
			__GCE_DEBUG_PRINT2("DDisplayPddSyborg::VSyncDfcFn moving pending buffer at address %08x to the active state\n", channel->iPendingBuffer->iAddress);
			channel->iActiveBuffer			= channel->iPendingBuffer;
			channel->iActiveBuffer->iState	= EBufferActive;
			channel->iPendingBuffer			= NULL;

			channel->RequestComplete(RDisplayChannel::EReqWaitForPost,  KErrNone);
			}
		}
	}
//*****************************************************************
//DDisplayPddFactory
//*****************************************************************/


/**
	Constructor
*/
DDisplayPddFactory::DDisplayPddFactory()
	{
	__GCE_DEBUG_PRINT("DDisplayPddFactory::DDisplayPddFactory()\n");

	iVersion		= TVersion(KDisplayChMajorVersionNumber,
                      KDisplayChMinorVersionNumber,
                      KDisplayChBuildVersionNumber);
	}

/**
	PDD factory function. Creates a PDD object.

	@param  aChannel  A pointer to an PDD channel object which will be initialised on return.

	@return KErrNone  if object successfully allocated, KErrNoMemory if not.
*/
TInt DDisplayPddFactory::Create(DBase*& aChannel, TInt aUnit, const TDesC8* aInfo, const TVersion& aVer)
	{
	DDisplayPddSyborg *device= new DDisplayPddSyborg() ;
	aChannel=device;
	if (!device)
		{
		return KErrNoMemory;
		}
	return KErrNone;
	}


/**
    Set the Pdd name and return error code
*/
TInt DDisplayPddFactory::Install()
	{
	__GCE_DEBUG_PRINT("DDisplayPddFactory::Install() \n");

	TBuf<32> name(RDisplayChannel::Name());
	_LIT(KPddExtension,".pdd");
	name.Append(KPddExtension);
	return SetName(&name);
	}


void DDisplayPddFactory::GetCaps(TDes8& /*aDes*/) const
	{
	//Not supported
	}


/**
    Validate version and number of units.
*/
TInt DDisplayPddFactory::Validate(TInt aUnit, const TDesC8* /*anInfo*/, const TVersion& aVer)
	{
	if (!Kern::QueryVersionSupported(iVersion,aVer))
		{
		return KErrNotSupported;
		}

	if (aUnit != 0)
		{
		return KErrNotSupported;
		}

	return KErrNone;
	}

DECLARE_EXTENSION_PDD()
/**
	"Standard PDD" entrypoint.Creates PDD factory when Kern::InstallPhysicalDevice is called

	@return pointer to the PDD factory object.
*/
	{
	__GCE_DEBUG_PRINT("DECLARE_EXTENSION_PDD()\n");
	return new DDisplayPddFactory ;
	}


DECLARE_STANDARD_EXTENSION()
{
  TInt r = KErrNoMemory;
  DLcdPowerHandler* pH=new DLcdPowerHandler;
  if(pH)
	{
	r = pH->Create();
	if ( r == KErrNone)
		{
		TInt r = Kern::DfcQCreate(pH->iDfcQ, 29 , &KLitLcd);

		if(r!=KErrNone)
		{
			return r;
		}

		DDisplayPddFactory * device = new DDisplayPddFactory;

		if (device==NULL)
			{
			r=KErrNoMemory;
			}
		else
			{
			r=Kern::InstallPhysicalDevice(device);
			}

		#ifdef CPU_AFFINITY_ANY
        NKern::ThreadSetCpuAffinity((NThread*) pH->iDfcQ->iThread, KCpuAffinityAny);
		#endif

		__KTRACE_OPT(KEXTENSION,Kern::Printf("Installing the display device from the kernel extension returned with error code %d",r));

		}
	}
  
  return r;
}

//
// Copyright (c) 2013 MOTU, Inc. All rights reserved.
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file.
//
/**********************************************************************************

	AudioUnitUtils

	Utilities for interacting with AudioUnits

**********************************************************************************/

#include "AudioUnitUtils.h"
#include <CoreAudio/AudioHardware.h>
#include <AudioToolbox/AudioUnitUtilities.h>
#include <AudioUnit/AudioUnitCarbonView.h>
#include "AUValStatus.h"
#include <memory>


#define kMusicDeviceProperty_DualSchedulingMode 1013


namespace AudioUnits
{

bool GetGlobalPropertyInfo(  AudioUnit  ci,  AudioUnitPropertyID inID,
                                UInt32& outDataSize, Boolean& outWritable, bool* negOneErrorCode = NULL)
{
    int32_t err = AudioUnitGetPropertyInfo(  ci, inID, kAudioUnitScope_Global, 0, &outDataSize, &outWritable );
    if ( negOneErrorCode ) *negOneErrorCode = err == -1;
    return err == noErr;
}

void GetGlobalProperty( AudioUnit ci, AudioUnitPropertyID  inID,
                                void* outData, UInt32&  ioDataSize, const FuncDesc& desc, bool* negOneErrorCode = NULL)
{
    FailAudioUnitError( AudioUnitGetProperty( ci, inID, kAudioUnitScope_Global, 0, outData, &ioDataSize), desc, negOneErrorCode );
}

void SetGlobalProperty( AudioUnit ci, AudioUnitPropertyID  inID,
                                const void* inData, UInt32  inDataSize, const FuncDesc& desc, bool* negOneErrorCode = NULL)
{
    FailAudioUnitError( AudioUnitSetProperty(ci, inID, kAudioUnitScope_Global, 0, const_cast<void*>(inData), inDataSize), desc, negOneErrorCode );
}

//--------------------------------
bool GetPropertyInfo(  AudioUnit  ci,  AudioUnitPropertyID inID, AudioUnitScope scope, AudioUnitElement elem,
                                UInt32& outDataSize, Boolean& outWritable )
{
    int32_t error = AudioUnitGetPropertyInfo(  ci, inID, scope, elem, &outDataSize, &outWritable );

    if ( error == kAudioUnitErr_Uninitialized)
    {
        DCL_AU_FUNC(GetPropertyInfo)
        FailAudioUnitError( kAudioUnitErr_Uninitialized, AU_DESC );
    }

    return error == noErr;
}

void GetProperty( AudioUnit ci, AudioUnitPropertyID  inID, AudioUnitScope scope, AudioUnitElement elem,
                                void* outData, UInt32&  ioDataSize, const FuncDesc& desc )
{
    FailAudioUnitError( AudioUnitGetProperty( ci, inID, scope, elem, outData, &ioDataSize), desc );
}

void SetProperty( AudioUnit ci, AudioUnitPropertyID  inID, AudioUnitScope scope, AudioUnitElement elem,
                                const void* inData, UInt32  inDataSize, const FuncDesc& desc )
{
    FailAudioUnitError( AudioUnitSetProperty(ci, inID, scope, elem, const_cast<void*>(inData), inDataSize), desc );
}

//--------------------------------
Base::Base( const AudioComponentDescription& desc ) :
    fComponentDesc(desc),
    fIsInitialized(false),
    fSupportsPrioritizedMIDI(false),
    fParamListenerRef(NULL)
#if SUPPORT_AU_VERSION_1
	,fRenderProc(NULL)
#endif
{
    DCL_AU_FUNC(Base::Base)

    AudioComponent auComp;
    AudioComponentDescription nonConst = desc;
    auComp = AudioComponentFindNext (NULL, &nonConst);
    if ( auComp == NULL )
		FailAudioUnitError( kAudioUnitErr_FailedInitialization, AU_DESC );

    AudioUnit newAudioUnit;
	FailAudioUnitError( AudioComponentInstanceNew (auComp, &newAudioUnit), AU_DESC );

	if ( newAudioUnit  == NULL )
		FailAudioUnitError( kAudioUnitErr_FailedInitialization, AU_DESC );

    fCi = newAudioUnit;
}

namespace {
std::string handleToUTF8Str( Handle h )
{
    std::string s;
	int handleSize = GetHandleSize( h );
	if ( handleSize > 0 )
	{
		int bufSize = (handleSize + 1) * 6;
		std::vector<char> buffer( bufSize );	// should be worst case for UTF8.

		int32_t strLen = *(unsigned char*)*h;
		if ( strLen > handleSize )
		{
			strLen = handleSize - 1;
		}

		CFStringRef newStr = CFStringCreateWithBytes( NULL, (UInt8*) &((*h)[1]), strLen,  GetApplicationTextEncoding(), false );

		if ( newStr == NULL )
		{
			// try fall-back to roman.
			newStr = CFStringCreateWithBytes( NULL, (UInt8*) &((*h)[1]), strLen,  kCFStringEncodingMacRoman, false );

			if ( newStr == NULL )
			{
				// weird. Just hand back the bytes.
				memmove( buffer.data(),  &((*h)[1]), strLen );
				buffer[strLen] = 0;
				char* p = buffer.data();	//note: must be signed char
				while ( *p )
				{
 					if ( *p <= 31 ) *p = '*';
 					++p;
				}
				s = buffer.data();
				return s;
			}
		}

 		CFStringGetCString( newStr, buffer.data(), bufSize,  kCFStringEncodingUTF8 );
		CFRelease( newStr );
  		s = buffer.data();
	}
	else
		s = "";

    return s;
}
} // unnamed namespace

Base::~Base()
{
	for ( int i = 0; i < fEventListeners.size(); ++i )
		fEventListeners[i]->BaseDied();
	fEventListeners.clear();

	if ( fParamListenerRef != NULL )
		AUListenerDispose( fParamListenerRef );

	CloseComponent( fCi );
	fCi = NULL;
}
//---------
int32_t Base::getComponentVersion()
{
	return CallComponentVersion( fCi );
}

//---------
void Base::SetHostInfo( CFStringRef host, const AUNumVersion& version )
{
	if ( host )
	{
		AUHostIdentifier hi;
		hi.hostName = host;
		hi.hostVersion = version;

		AudioUnitSetProperty(fCi, kAudioUnitProperty_AUHostIdentifier, kAudioUnitScope_Global, 0, &hi, sizeof(AUHostIdentifier) );
	}
}

//---------
void Base::Initialize()
{
	DCL_AU_FUNC(Initialize)
	assert( not fIsInitialized );
	FailAudioUnitError( AudioUnitInitialize( fCi ), AU_DESC );

	fIsInitialized = true;

 	UInt32 dataSize;
	Boolean writable;
	if ( GetGlobalPropertyInfo( fCi,  kAudioUnitProperty_ParameterList, dataSize, writable) && dataSize > 0 )
		createListener();
}

void Base::Uninitialize()
{
	DCL_AU_FUNC(Uninitialize)

	assert( fIsInitialized );

	if ( fIsInitialized )
	{
		if ( fParamListenerRef != NULL )
		{
			AUListenerDispose( fParamListenerRef );
			fParamListenerRef = NULL;
		}

		AudioUnitUninitialize( fCi );
		fIsInitialized = false;
	}
}

void Base::Reset()
{
	DCL_AU_FUNC(Reset)

 	if ( fIsInitialized )
	{
		AudioUnitReset (fCi, kAudioUnitScope_Global, 0);
	}
}


//---------
PropertyList Base::getClassInfo()
{
	DCL_AU_FUNC(getClassInfo)

	if ( fCi == NULL )
		FailAudioUnitError( kAudioHardwareUnspecifiedError, AU_DESC );

	CFPropertyListRef dict = NULL;
	UInt32 dataSize;
	Boolean writable;

	bool negOneErr;
	if ( GetGlobalPropertyInfo( fCi,  kAudioUnitProperty_ClassInfo, dataSize, writable, &negOneErr ) )
	{
		dataSize = sizeof( CFPropertyListRef );
		GetGlobalProperty( fCi, kAudioUnitProperty_ClassInfo, &dict, dataSize, AU_DESC, &negOneErr );

		if ( negOneErr )
			dict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

		if ( dict != NULL )
			return PropertyList(dict);
	}
	if ( negOneErr )
	{
		dict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		return PropertyList(dict);
	}

	FailAudioUnitError( kAudioHardwareUnspecifiedError, AU_DESC );
	return PropertyList();
}

namespace
{
    // this is used for the presets
    const CFStringRef kUntitledString = CFSTR("Untitled");
    //these are the current keys for the class info document

    const CFStringRef kVersionString = CFSTR("version");
    const CFStringRef kTypeString = CFSTR("type");
    const CFStringRef kSubtypeString = CFSTR("subtype");
    const CFStringRef kManufacturerString = CFSTR("manufacturer");
    const CFStringRef kDataString = CFSTR("data");
    const CFStringRef kNameString = CFSTR("name");
}

#define kCurrentSavedStateVersion 0

OSStatus Base::testPropList( CFPropertyListRef plist )
{
	#pragma unused( plist )
#if 0
// first step -> check the saved version in the data ref
// at this point we're only dealing with version==0
	CFNumberRef cfnum = reinterpret_cast<CFNumberRef>(CFDictionaryGetValue((CFDictionaryRef)plist, kVersionString));
	mSInt32 value;
	CFNumberGetValue (cfnum, kCFNumberSInt32Type, &value);
	if (value != kCurrentSavedStateVersion) return kAudioUnitErr_InvalidPropertyValue;

// second step -> check that this data belongs to this kind of audio unit
// by checking the component type, subtype and manuID
// they MUST all match
	cfnum = reinterpret_cast<CFNumberRef>(CFDictionaryGetValue((CFDictionaryRef)plist, kTypeString));
	CFNumberGetValue (cfnum, kCFNumberSInt32Type, &value);
	if (UInt32(value) != fComponentDesc.componentType) return kAudioUnitErr_InvalidPropertyValue;

	cfnum = reinterpret_cast<CFNumberRef>(CFDictionaryGetValue((CFDictionaryRef)plist, kSubtypeString));
	CFNumberGetValue (cfnum, kCFNumberSInt32Type, &value);
	if (UInt32(value) != fComponentDesc.componentSubType) return kAudioUnitErr_InvalidPropertyValue;

	cfnum = reinterpret_cast<CFNumberRef>(CFDictionaryGetValue((CFDictionaryRef)plist, kManufacturerString));
	CFNumberGetValue (cfnum, kCFNumberSInt32Type, &value);
	if (UInt32(value) != fComponentDesc.componentManufacturer) return kAudioUnitErr_InvalidPropertyValue;

// fourth step -> restore the state of all of the parameters for each scope and element
	CFDataRef data = reinterpret_cast<CFDataRef>(CFDictionaryGetValue((CFDictionaryRef)plist, kDataString));
	if (data == NULL) return kAudioUnitErr_InvalidPropertyValue;
#endif

	return 0;
}

void Base::setClassInfo( const PropertyList& dict )
{
	DCL_AU_FUNC(FInfo)
	CFPropertyListRef propList = dict.getPropList();
	OSStatus err = testPropList( propList );
	assert( err == noErr );

	if ( propList )
	{
		bool negOneErr;
		SetGlobalProperty( fCi, kAudioUnitProperty_ClassInfo, &propList, sizeof( CFPropertyListRef ), AU_DESC, &negOneErr );

		if ( not negOneErr )
		{
				// now that we've potentially changed the values of the parameter we
				// should notify any listeners of this change:
				// We're only taking care of units in the global scope at this point.
			AudioUnitParameter changedUnit;
			changedUnit.mAudioUnit = fCi;
			changedUnit.mParameterID = kAUParameterListener_AnyParameter;
			AUParameterListenerNotify( NULL, NULL, &changedUnit );
		}
	}
}

void Base::setFromMASData( char* buffer, int32_t len )
{
	PropertyList propList( buffer, len, fComponentDesc );
	setClassInfo( propList );
}
//---------
void Base::getPresetList( CFArrayRef& array )
{
	DCL_AU_FUNC(getPresetList)

	array = NULL;
	UInt32 dataSize;
	dataSize = sizeof( CFArrayRef );

	OSStatus err = AudioUnitGetProperty( fCi, kAudioUnitProperty_FactoryPresets, kAudioUnitScope_Global, 0, &array, &dataSize);

	if ( err != noErr )
	{
		array = NULL;
	}
}

bool Base::presetsRequireInit()
{
	DCL_AU_FUNC(presetsRequireInit)

	CFArrayRef array;
	array = NULL;
	UInt32 dataSize;
	dataSize = sizeof( CFArrayRef );

	OSStatus err = AudioUnitGetProperty( fCi, kAudioUnitProperty_FactoryPresets, kAudioUnitScope_Global, 0, &array, &dataSize);

	if ( err == kAudioUnitErr_Uninitialized )
		return true;

	if ( array != NULL )
		CFRelease( array );

	return false;
}

void Base::testSupportsPrioritizedMIDI()
{
 	UInt32 prioritizeMIDI = 1;
 	OSStatus err = AudioUnitSetProperty( fCi, kMusicDeviceProperty_DualSchedulingMode, kAudioUnitScope_Global, 0, &prioritizeMIDI, sizeof( UInt32 ) );	// ignore error. Many AUs may not have this prop.
	fSupportsPrioritizedMIDI = ( err == noErr );
}

void Base::getCurrentPreset( AUPreset& preset )
{
	DCL_AU_FUNC(getCurrentPreset)

	UInt32 dataSize;
	dataSize = sizeof( AUPreset );
	ComponentResult result = AudioUnitGetProperty ( fCi,
									kAudioUnitProperty_PresentPreset,
									kAudioUnitScope_Global,
									0,
									&preset,
									&dataSize);

	if (result != noErr)
	{	// if the PresentPreset property is not implemented, fall back to the CurrentPreset property
		GetGlobalProperty( fCi, kAudioUnitProperty_CurrentPreset, &preset, dataSize, AU_DESC );

		if ( preset.presetName )
			CFRetain (preset.presetName);
	}
}

void Base::setCurrentPreset( AUPreset& preset )
{
	DCL_AU_FUNC(setCurrentPreset)

	OSStatus result = AudioUnitSetProperty ( fCi, kAudioUnitProperty_PresentPreset,
								kAudioUnitScope_Global, 0,
								&preset, sizeof (AUPreset));

	if (result != noErr)
	{
		SetGlobalProperty( fCi, kAudioUnitProperty_PresentPreset, &preset, sizeof (AUPreset), AU_DESC );
	}


		// now that we've potentially changed the values of the parameter we
		// should notify any listeners of this change:
		// We're only taking care of units in the global scope at this point.
	AudioUnitParameter changedUnit;
	changedUnit.mAudioUnit = fCi;
	changedUnit.mParameterID = kAUParameterListener_AnyParameter;
	AUParameterListenerNotify( NULL, NULL, &changedUnit );
}
//-------------
std::vector<AudioUnitParameterID> Base::getGlobalParameterList()
{
	DCL_AU_FUNC(getParameterList)

 	UInt32 dataSize;
	Boolean writable;
	if ( GetGlobalPropertyInfo( fCi,  kAudioUnitProperty_ParameterList, dataSize, writable) )
	{
		int32_t num =  dataSize / sizeof( AudioUnitParameterID );
		if ( num > 0 )
		{
			std::vector<AudioUnitParameterID> buffer( num );
			GetGlobalProperty( fCi, kAudioUnitProperty_ParameterList, buffer.data(), dataSize, AU_DESC );
			return buffer;
		}
	}
	return std::vector<AudioUnitParameterID>();
}

void	Base::getGlobalParameterInfo( AudioUnitParameterID id, AudioUnitParameterInfo& info )
{
	DCL_AU_FUNC(getParameterInfo)

 	UInt32 dataSize;
	Boolean writable;
	if ( GetPropertyInfo( fCi,  kAudioUnitProperty_ParameterInfo, kAudioUnitScope_Global, id, dataSize, writable) )
	{
		dataSize = sizeof( AudioUnitParameterInfo );
 		GetProperty( fCi, kAudioUnitProperty_ParameterInfo, kAudioUnitScope_Global, id, &info, dataSize, AU_DESC );
	}
}

void	Base::getGlobalParameterValueStrings( AudioUnitParameterID id, CFArrayRef& strings )
{
	DCL_AU_FUNC(getParameterValueStrings)

 	UInt32 dataSize;
	Boolean writable;
	strings = NULL;
	if ( GetPropertyInfo( fCi,  kAudioUnitProperty_ParameterValueStrings, kAudioUnitScope_Global, id, dataSize, writable) )
	{
		dataSize = sizeof( CFArrayRef );
		if ( AudioUnitGetProperty( fCi, kAudioUnitProperty_ParameterValueStrings, kAudioUnitScope_Global, id, &strings, &dataSize) != noErr )
			strings = NULL;
	}
}

float	Base::getGlobalParameterValue( AudioUnitParameterID id )
{
	DCL_AU_FUNC(getGlobalParameterValue)
	Float32 retVal = 0;
	if ( AudioUnitGetParameter ( fCi, id, kAudioUnitScope_Global, 0, &retVal ) == noErr )
		return retVal;

	return 0;
}

//---------

std::vector<AudioUnitParameterID> Base::getParameterList( int scope )
{
	DCL_AU_FUNC(getParameterList)

 	UInt32 dataSize;
	Boolean writable;
	if ( GetPropertyInfo( fCi,  kAudioUnitProperty_ParameterList, scope, 0, dataSize, writable) )
	{
		int32_t num =  dataSize / sizeof( AudioUnitParameterID );
		if ( num > 0 )
		{
			std::vector<AudioUnitParameterID> buffer( num );
			GetProperty( fCi, kAudioUnitProperty_ParameterList, scope, 0, buffer.data(), dataSize, AU_DESC );
            return buffer;
		}
	}
	return std::vector<AudioUnitParameterID>();
}

void	Base::getParameterInfo( int scope, AudioUnitParameterID id, AudioUnitParameterInfo& info )
{
	DCL_AU_FUNC(getParameterInfo)

 	UInt32 dataSize;
	Boolean writable;
	if ( GetPropertyInfo( fCi,  kAudioUnitProperty_ParameterInfo, scope, id, dataSize, writable) )
	{
		dataSize = sizeof( AudioUnitParameterInfo );
 		GetProperty( fCi, kAudioUnitProperty_ParameterInfo, scope, id, &info, dataSize, AU_DESC );
	}
}

void	Base::getParameterValueStrings( int scope, AudioUnitParameterID id, CFArrayRef& strings )
{
	DCL_AU_FUNC(getParameterValueStrings)

 	UInt32 dataSize;
	Boolean writable;
	strings = NULL;
	if ( GetPropertyInfo( fCi,  kAudioUnitProperty_ParameterValueStrings, scope, id, dataSize, writable) )
	{
		dataSize = sizeof( CFArrayRef );
		if ( AudioUnitGetProperty( fCi, kAudioUnitProperty_ParameterValueStrings, scope, id, &strings, &dataSize) != noErr )
			strings = NULL;
 	}
}
float	Base::getParameterValue( int scope, int32_t element, AudioUnitParameterID id )
{
	DCL_AU_FUNC(getParameterValue)
	Float32 retVal = 0;
	if ( AudioUnitGetParameter ( fCi, id, scope, element, &retVal ) == noErr )
		return retVal;

	return 0;
}

//---------
#if 0
std::vector<AudioUnitMIDIControlMapping> Base::getMIDIControlMapping()
{
	DCL_AU_FUNC(getMIDIControlMapping)

 	UInt32 dataSize;
	Boolean writable;
	if ( GetGlobalPropertyInfo( fCi,  kAudioUnitProperty_MIDIControlMapping, dataSize, writable) )
	{
		int32_t num =  dataSize / sizeof( AudioUnitMIDIControlMapping );
		AutoBuffer<AudioUnitMIDIControlMapping> buffer( num );
		GetGlobalProperty( fCi, kAudioUnitProperty_MIDIControlMapping, buffer.get(), dataSize, AU_DESC );
		std::vector<AudioUnitMIDIControlMapping> ret;

		for ( int i = 0; i < num; ++i )
			ret.push_back(buffer[i]);
		return ret;
	}
	return std::vector<AudioUnitMIDIControlMapping>();
}
#endif

//---------
void Base::setStreamFormat( int scope, int busNumber, const AudioStreamBasicDescription& desc )
{
	DCL_AU_FUNC(setStreamFormat)
	SetProperty( fCi, kAudioUnitProperty_StreamFormat, scope, busNumber, &desc, sizeof( AudioStreamBasicDescription ), AU_DESC );
}
void Base::getStreamFormat( int scope, int busNumber, AudioStreamBasicDescription& desc )
{
	DCL_AU_FUNC(getStreamFormat)

 	UInt32 dataSize;
	Boolean writable;
	if ( GetGlobalPropertyInfo( fCi,  kAudioUnitProperty_StreamFormat, dataSize, writable) )
	{
		dataSize = sizeof(AudioStreamBasicDescription);
 		GetProperty( fCi, kAudioUnitProperty_StreamFormat, scope, busNumber, &desc, dataSize, AU_DESC );
	}
}

int32_t	Base::getNumBusses(int scope, bool& writable )
{

	DCL_AU_FUNC(getNumBusses)
 	UInt32 dataSize;
	UInt32 ret = 0;
	Boolean writ;
	if ( GetPropertyInfo( fCi,  kAudioUnitProperty_ElementCount, scope, 0, dataSize, writ ) )
	{
		writable = writ;
		dataSize = sizeof( UInt32 );
 		GetProperty( fCi, kAudioUnitProperty_ElementCount, scope, 0, &ret, dataSize, AU_DESC );
	}
	return ret;
}

void	Base::setNumBusses(int scope, int num )
{
	DCL_AU_FUNC(setNumBusses)
 	UInt32 dataSize;
	UInt32 numBusses = num;
	Boolean writ;
	if ( GetPropertyInfo( fCi,  kAudioUnitProperty_ElementCount, scope, 0, dataSize, writ ) )
	{
 		dataSize = sizeof( UInt32 );
 		SetProperty( fCi, kAudioUnitProperty_ElementCount, scope, 0, &numBusses, dataSize, AU_DESC );
	}
}

std::vector<AUChannelInfo>	Base::getSupportedNumChannels()
{
	DCL_AU_FUNC(getSupportedNumChannels)


 	UInt32 dataSize;
	Boolean writable;
	if ( GetGlobalPropertyInfo( fCi,  kAudioUnitProperty_SupportedNumChannels, dataSize, writable ))
	{
		int32_t num =  dataSize / sizeof( AUChannelInfo );
		std::vector<AUChannelInfo> buffer( num );
		GetGlobalProperty( fCi, kAudioUnitProperty_SupportedNumChannels, buffer.data(), dataSize, AU_DESC );
        return buffer;
	}
	return std::vector<AUChannelInfo>();
}

Float64	Base::getLatency()
{
	DCL_AU_FUNC(getLatency)

 	UInt32 dataSize;
	Boolean writable;
	Float64 ret = 0;
	if ( GetPropertyInfo( fCi,  kAudioUnitProperty_Latency, kAudioUnitScope_Global, 0, dataSize, writable ) )
	{
		dataSize = sizeof( Float64 );
 		GetProperty( fCi, kAudioUnitProperty_Latency, kAudioUnitScope_Global, 0, &ret, dataSize, AU_DESC );
	}
	return ret;
}
optional<Float64> Base::getTail()
{
	DCL_AU_FUNC(getTail)

 	UInt32 dataSize;
	Boolean writable;
	if ( GetPropertyInfo( fCi,  kAudioUnitProperty_TailTime, kAudioUnitScope_Global, 0, dataSize, writable ) )
	{
        Float64 tail;
		dataSize = sizeof( Float64 );
 		GetProperty( fCi, kAudioUnitProperty_TailTime, kAudioUnitScope_Global, 0, &tail, dataSize, AU_DESC );
 		return tail;
	}
	return nullptr;
}

void	Base::setMaxFramesPerSlice( uint32_t max )
{
	DCL_AU_FUNC(setMaxFramesPerSlice)
	SetGlobalProperty( fCi, kAudioUnitProperty_MaximumFramesPerSlice, &max, sizeof( UInt32 ), AU_DESC );
}

void	Base::setRealtimeHint( bool realtime )
{
 	UInt32 offlineRender = not realtime;
 	AudioUnitSetProperty( fCi, kAudioUnitProperty_OfflineRender, kAudioUnitScope_Global, 0, &offlineRender, sizeof( UInt32 ) );	// ignore error. Many AUs may not have this prop.
}

GUIInfo
Base::getGUIInfo()
{
    std::vector<AudioComponentDescription> carbonDescs = getCarbonUIComponentList();
    optional<AudioComponentDescription> carbonDesc;
    bool carbonDescWasGeneric = true;

	for ( const AudioComponentDescription& c : carbonDescs)
	{
		if ( c.componentSubType != 'gnrc' )
		{
            carbonDesc = c;
            carbonDescWasGeneric = false;
			break;
		}
	}

    if(not carbonDesc.hasValue() and (carbonDescs.size() > 0))
    {
        carbonDesc = carbonDescs[0];
    }

    return GUIInfo(getCocoaUIInfo(), carbonDesc, carbonDescWasGeneric);
}

optional<CocoaUIInfo>
Base::getCocoaUIInfo()
{
	DCL_AU_FUNC(getCocoaUIInfo)

 	UInt32 dataSize;
	Boolean writable;

	if ( GetGlobalPropertyInfo( fCi,  kAudioUnitProperty_CocoaUI, dataSize, writable) )
	{
		UInt32 numberOfClasses = (dataSize - sizeof(CFURLRef)) / sizeof(CFStringRef);
		if ( numberOfClasses >= 1 )
		{
            CocoaUIInfo info;
			std::vector<char> buffer( dataSize );
			GetProperty( fCi, kAudioUnitProperty_CocoaUI, kAudioUnitScope_Global, 0, buffer.data(), dataSize, AU_DESC );
			AudioUnitCocoaViewInfo* infoPtr = reinterpret_cast<AudioUnitCocoaViewInfo*>(buffer.data());
			info.bundleLocation.reset(infoPtr->mCocoaAUViewBundleLocation);
			for ( int i = 0; i < numberOfClasses; ++i )
				info.classes.push_back(ScopedCFTypeRef<CFStringRef>(infoPtr->mCocoaAUViewClass[i]));
			return info;
		}
 	}

	return nullptr;
}

std::vector<AudioComponentDescription> Base::getCarbonUIComponentList()
{
	DCL_AU_FUNC(getUIComponentList)

 	UInt32 dataSize;
	Boolean writable;
	std::vector<AudioComponentDescription> ret;

	if ( GetGlobalPropertyInfo( fCi,  kAudioUnitProperty_GetUIComponentList, dataSize, writable) )
	{
		int32_t num =  dataSize / sizeof( AudioComponentDescription );
		std::vector<AudioComponentDescription> buffer( num );
		GetGlobalProperty( fCi, kAudioUnitProperty_GetUIComponentList, buffer.data(), dataSize, AU_DESC );
		ret = std::move(buffer);
	}

	if ( ret.size() == 0 )
	{
		AudioComponentDescription genericDesc;

		genericDesc.componentType = kAudioUnitCarbonViewComponentType;
		genericDesc.componentSubType = 'gnrc';
		genericDesc.componentManufacturer = 'appl';
		genericDesc.componentFlags = 0;
		genericDesc.componentFlagsMask = 0;

		ret.push_back( genericDesc );
	}

	return ret;
}

std::vector<AudioUnitOtherPluginDesc> Base::getReplacementList()
{
	DCL_AU_FUNC(getMASReplacementList)

 	UInt32 dataSize;
	Boolean writable;
	std::vector<AudioUnitOtherPluginDesc> ret;
	CFArrayRef array = NULL;

	if ( GetGlobalPropertyInfo( fCi,  kAudioUnitMigrateProperty_FromPlugin, dataSize, writable) )
	{

		dataSize = sizeof( CFArrayRef );
		try
		{
			GetGlobalProperty( fCi, kAudioUnitMigrateProperty_FromPlugin, &array, dataSize, AU_DESC);
		}
		catch(...)
		{
			array = NULL;
		}

		if ( array != NULL )
		{

			int num = CFArrayGetCount(array);
			for ( int j = 0; j < num; ++j )
			{
				AudioUnitOtherPluginDesc* otherDesc = (AudioUnitOtherPluginDesc*)CFArrayGetValueAtIndex(array, j);
				if ( otherDesc != NULL )
					ret.push_back( *otherDesc );
			}

			CFRelease( array );
		}
	}


	return ret;
}

bool	Base::getIsBypassed()
{
	DCL_AU_FUNC(getIsBypassed)
 	UInt32 dataSize;
	Boolean writable;
	UInt32 ret = 0;
	if ( GetPropertyInfo( fCi,  kAudioUnitProperty_BypassEffect, kAudioUnitScope_Global, 0, dataSize, writable ) )
	{
		dataSize = sizeof( UInt32 );
 		GetProperty( fCi, kAudioUnitProperty_BypassEffect, kAudioUnitScope_Global, 0, &ret, dataSize, AU_DESC );
	}
	return ret;
}

void	Base::setIsBypassed( bool is )
{
	DCL_AU_FUNC(setIsBypassed)
	UInt32 isBypassed = is;
	SetGlobalProperty( fCi, kAudioUnitProperty_BypassEffect, &isBypassed, sizeof( UInt32 ), AU_DESC );
}

#if 0
void	Base::setContextName( CFStringRef ref )
{
	DCL_AU_FUNC(setContextName)

	SetGlobalProperty( fCi, kAudioUnitProperty_ContextName, &ref, sizeof( CFStringRef ), AU_DESC );
}

void	Base::getContextName( CFStringRef& ref )
{
	DCL_AU_FUNC(getContextName)
 	UInt32 dataSize;
	Boolean writable;
	ref = 0;
 	if ( GetPropertyInfo( fCi,  kAudioUnitProperty_ContextName, kAudioUnitScope_Global, 0, dataSize, writable ) )
	{
		dataSize = sizeof( CFStringRef );
 		GetProperty( fCi, kAudioUnitProperty_ContextName, kAudioUnitScope_Global, 0, &ref, dataSize, AU_DESC );
	}
 }
#endif

optional<CFStringRef> Base::getBusName( int scope, int bus )
{
	DCL_AU_FUNC(getBusName)
 	UInt32 dataSize;
	Boolean writable;
 	if ( GetPropertyInfo( fCi,  kAudioUnitProperty_ElementName, scope, bus, dataSize, writable ) )
	{
		dataSize = sizeof( CFStringRef );
        CFStringRef name;
		try
		{
			GetProperty( fCi, kAudioUnitProperty_ElementName, scope, bus, &name, dataSize, AU_DESC );
		}
		catch(...)
		{
			name = 0;
			return nullptr;
		}
		return name;
	}
	return nullptr;
}

//---------
void Base::setRenderCallback( AURenderCallback ci, void* data, int bus )
{
	DCL_AU_FUNC(setRenderNotification)

	AURenderCallbackStruct callStruct;
	callStruct.inputProc = ci;
	callStruct.inputProcRefCon = data;

	SetProperty( fCi, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, bus, &callStruct, sizeof( AURenderCallbackStruct ), AU_DESC );
}
void Base::removeRenderCallback(int bus)
{
	DCL_AU_FUNC(removeRenderCallback)

	AURenderCallbackStruct callStruct;
	callStruct.inputProc = NULL;
	callStruct.inputProcRefCon = NULL;

	AudioUnitSetProperty(fCi, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, bus, &callStruct, sizeof( AURenderCallbackStruct ));
}

void Base::scheduleParameters(const AudioUnitParameterEvent* inParameterEvent, uint32_t inNumParamEvents )
{
	DCL_AU_FUNC(scheduleParameters)
	FailAudioUnitError( AudioUnitScheduleParameters( fCi, inParameterEvent, inNumParamEvents), AU_DESC );
}

void Base::setParameter( AudioUnitParameterID inID,
						  AudioUnitScope         inScope,
						  AudioUnitElement       inElement,
						  Float32                inValue,
						  uint32_t                 inBufferOffsetInFrames )
{
	DCL_AU_FUNC(setParameter)

	AudioUnitParameter param;
	param.mAudioUnit = fCi;
	param.mParameterID = inID;
	param.mScope = inScope;
	param.mElement = inElement;

	AUParameterSet ( fParamListenerRef, NULL, &param, inValue, inBufferOffsetInFrames);
}

//---------
void Base::broadcastGesture(   AudioUnitParameterID inID,
						  AudioUnitScope         inScope,
						  AudioUnitElement       inElement,
						  bool	beginning )
{
	DCL_AU_FUNC(broadcastGesture)

	AudioUnitEvent param;
	param.mEventType = beginning ? kAudioUnitEvent_BeginParameterChangeGesture :  kAudioUnitEvent_EndParameterChangeGesture;
	param.mArgument.mParameter.mAudioUnit = fCi;
	param.mArgument.mParameter.mParameterID = inID;
	param.mArgument.mParameter.mScope = inScope;
	param.mArgument.mParameter.mElement = inElement;

	AUEventListenerNotify ( fParamListenerRef, NULL, &param );
}

//---------
void Base::render( AudioUnitRenderActionFlags& ioActionFlags,
 	 			 const AudioTimeStamp& inTimeStamp,
  					uint32_t inOutputBusNumber,
  					uint32_t inNumberFrames,
  					AudioBufferList* ioData )
{
	DCL_AU_FUNC(renderSlice)
	FailAudioUnitError(  AudioUnitRender( fCi, &ioActionFlags, &inTimeStamp, inOutputBusNumber, inNumberFrames, ioData ), AU_DESC );
}

//---------

namespace
{

bool HasValidName( const AudioComponentDescription& desc )
{
	optional<UTF8ComponentInfo> info = GetUTF8ComponentInfo( desc );
	return info.hasValue() and (info->name.length() > 5);
}

void AddComponentsToVector( std::vector<AudioComponentDescription>& vec, uint32_t component_type  )
{
	int mNumUnits = 0;

	AudioComponentDescription	cd2;

 	cd2.componentType = component_type;
	cd2.componentFlags = 0;
	cd2.componentFlagsMask = 0;
	cd2.componentSubType = 0;
	cd2.componentManufacturer = 0;

	AudioComponent comp = NULL;

	// find all the v2 effect AUs
 	comp = AudioComponentFindNext (NULL, &cd2);
	while (comp != NULL)
	{
		AudioComponentDescription desc;
		AudioComponentGetDescription (comp, &desc);
		if ( HasValidName( desc ) )
			vec.push_back( desc );

		comp = AudioComponentFindNext (comp, &cd2);
	}
}
} // unnamed namespace
std::vector<AudioComponentDescription> GetEffectList()
{
	std::vector<AudioComponentDescription> ret;
	AddComponentsToVector( ret, kAudioUnitType_Effect );
	AddComponentsToVector( ret, kAudioUnitType_MusicEffect );
	return ret;
}

std::vector<AudioComponentDescription> GetSynthList()
{
	std::vector<AudioComponentDescription> ret;
	AddComponentsToVector( ret, kAudioUnitType_MusicDevice );
	return ret;
}
std::vector<AudioComponentDescription> GetCompleteList()
{
	std::vector<AudioComponentDescription> ret;
	AddComponentsToVector( ret, kAudioUnitType_Effect );
	AddComponentsToVector( ret, kAudioUnitType_MusicEffect );
	AddComponentsToVector( ret, kAudioUnitType_MusicDevice );
	return ret;
}

void FailAudioUnitError( int32_t error, const FuncDesc& desc, bool* negOneErrorCode )
{
	if ( error != noErr )
	{
		if ( negOneErrorCode )
			*negOneErrorCode = (error == -1);

		char errorStr[500];
		sprintf( errorStr, "Failure: %s %d %d", desc.name, desc.line, error );
	}
	else
	{
		if ( negOneErrorCode )
			*negOneErrorCode = false;
	}
}

optional<UTF8ComponentInfo> GetUTF8ComponentInfo( const AudioComponentDescription& desc )
{
    AudioComponent auComp = AudioComponentFindNext (NULL, &desc);

	if ( !auComp )
        return nullptr;
    
    UTF8ComponentInfo info;
    
    ScopedCFTypeRef<CFStringRef> name;
    CFStringRef rawName;
    
    if(AudioComponentCopyName(auComp, &rawName) != noErr)
        return nullptr;
    
    name.reset(rawName); // adopt the cfstringref

    std::vector<char> cStringBuffer(CFStringGetLength(name.get()) * 6 + 1);
    if(not CFStringGetCString(name.get(), cStringBuffer.data(), cStringBuffer.size(), kCFStringEncodingUTF8))
        return nullptr;

    // convert to c++ string
    info.name = cStringBuffer.data();

	return info;
}

optional<uint32_t> GetComponentVersion( const AudioComponentDescription& desc )
{
    AudioComponent auComp = AudioComponentFindNext (NULL, &desc);
    if( !auComp )
        return nullptr;
    
    UInt32 version = 0;
    if(AudioComponentGetVersion(auComp, &version) != noErr)
        return nullptr;
    
    return version;
}

CFURLRef Base::getMIDIXMLDoc()
{
	DCL_AU_FUNC(getMIDIXMLDoc)

	CFURLRef url = NULL;
	UInt32 dataSize;
	Boolean writable;

	try
	{
	 	if ( GetGlobalPropertyInfo( fCi,  kMusicDeviceProperty_MIDIXMLNames, dataSize, writable  ) )
		{
			dataSize = sizeof( CFURLRef );
			GetGlobalProperty( fCi, kMusicDeviceProperty_MIDIXMLNames, &url, dataSize, AU_DESC );
		}
	}
	catch(...)
	{
		return NULL;
	}
	return url;
}

void Base::translateAutomation( const AudioUnitOtherPluginDesc& desc,
								int32_t& id, float& value )
{
	AudioUnitParameterValueTranslation transStruct;
	transStruct.otherDesc = desc;
 	transStruct.otherParamID = id;
	transStruct.otherValue = value;
	transStruct.auParamID = id;
	transStruct.auValue = value;

 	UInt32 dataSize = sizeof( AudioUnitParameterValueTranslation );
	if ( AudioUnitGetProperty( fCi, kAudioUnitMigrateProperty_OldAutomation, kAudioUnitScope_Global, 0, &transStruct, &dataSize) == noErr )
	{
		id = transStruct.auParamID;
		value = transStruct.auValue;
	}
}


void Base::getSupportedLayouts( bool input,  int bus, std::vector<AudioChannelLayoutTag>& layouts )
{
	layouts.clear();

	DCL_AU_FUNC(getSupportedLayouts)

 	UInt32 dataSize;
	Boolean writable;

	try
	{
	 	if ( GetPropertyInfo( fCi,  kAudioUnitProperty_SupportedChannelLayoutTags, input ? kAudioUnitScope_Input : kAudioUnitScope_Output, bus, dataSize, writable  ) )
		{
			int32_t num =  dataSize / sizeof( AudioChannelLayoutTag );
			if ( num > 0 )
			{
				std::vector<AudioChannelLayoutTag> buffer( num );
				GetProperty( fCi, kAudioUnitProperty_SupportedChannelLayoutTags, input ? kAudioUnitScope_Input : kAudioUnitScope_Output, bus, buffer.data(), dataSize, AU_DESC );
				layouts = std::move(buffer);
			}
		}
	}
	catch(...)
	{
	}
}

Base::GetLayoutRetVals Base::getCurrentChannelLayout( bool input, int bus, AudioChannelLayoutTag& cur, bool& writable )
{
	DCL_AU_FUNC(getCurrentChannelLayout)

 	UInt32 dataSize;
	Boolean writableX = false;

	try
	{
	 	if ( GetPropertyInfo( fCi,  kAudioUnitProperty_AudioChannelLayout, input ? kAudioUnitScope_Input : kAudioUnitScope_Output, bus, dataSize, writableX  ) )
		{
			writable = writableX;
			std::vector<char> buffer( dataSize );
			ComponentResult result = AudioUnitGetProperty( fCi, kAudioUnitProperty_AudioChannelLayout, input ? kAudioUnitScope_Input : kAudioUnitScope_Output, bus, buffer.data(), &dataSize );

			if ( result == kAudioUnitErr_PropertyNotInUse )
				return kNotCurrentlySet;

			if ( result == noErr )
			{
				AudioChannelLayout* layout = reinterpret_cast<AudioChannelLayout*>( buffer.data() );
				if ( layout->mChannelLayoutTag != kAudioChannelLayoutTag_UseChannelDescriptions and layout->mChannelLayoutTag != kAudioChannelLayoutTag_UseChannelBitmap )
				{
					cur = layout->mChannelLayoutTag;
					return kNoError;
				}
				return kNotCurrentlySet;
			}
		}
	}
	catch(...)
	{
	}
	return kNotSupported;
}

bool Base::setChannelLayout(  bool input, int bus, const AudioChannelLayout* layout, int byteSize )
{
	DCL_AU_FUNC(setChannelLayout)

	try
	{
		SetProperty( fCi,  kAudioUnitProperty_AudioChannelLayout, input ? kAudioUnitScope_Input : kAudioUnitScope_Output, bus, layout, byteSize, AU_DESC  );
		return true;
	}
	catch(...)
	{
	}
	return false;
}

namespace
{
void EventListenerProc( void* , void* , const AudioUnitParameter*	, Float32  )
{
}
} // unnamed namespace

void Base::makeAndDeleteListener()
{
	DCL_AU_FUNC(createListener)
	AUParameterListenerRef paramListenerRef;

	FailAudioUnitError( AUListenerCreate( EventListenerProc, this,
						NULL, NULL, 0, &paramListenerRef ), AU_DESC );

	std::vector<AudioUnitParameterID> params = getGlobalParameterList();

	for ( int i = 0; i < params.size(); ++i )
	{
		AudioUnitParameter param;
		param.mAudioUnit = fCi;
		param.mParameterID = params[i];
		param.mScope = kAudioUnitScope_Global;
		param.mElement = 0;
		AUListenerAddParameter( paramListenerRef, NULL, &param);
	}
	AUListenerDispose( paramListenerRef );
}

namespace
{
    void AddNumToDictionary (CFMutableDictionaryRef dict, CFStringRef key, UInt32 value)
    {
        CFNumberRef num = CFNumberCreate (NULL, kCFNumberSInt32Type, &value);
        CFDictionarySetValue (dict, key, num);
        CFRelease (num);
    }
    bool GetNumFromDictionary (CFDictionaryRef dict, CFStringRef key, UInt32& val )
    {
        CFNumberRef num;
        if ( CFDictionaryGetValueIfPresent( dict, key, (const void**) &num) and CFGetTypeID(num) == CFNumberGetTypeID() )
            return CFNumberGetValue( num, kCFNumberSInt32Type, &val);

        return false;
    }
}

AudioUnits::PropertyList::PropertyList( char* buffer, int32_t len, const AudioComponentDescription& cd  )
{
	CFMutableDictionaryRef mutableRef = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	CFDataRef dataRef = CFDataCreate( kCFAllocatorDefault, (unsigned char*)buffer, len );
	CFStringRef key = CFStringCreateWithCString( kCFAllocatorDefault, kAUPresetMASDataKey, kCFStringEncodingASCII );

	// save the data into the dictionary
	CFDictionaryAddValue( mutableRef, key, dataRef );

	// add the additional key/value pairs to make this a 'legitimate' class info.
	AddNumToDictionary (mutableRef, kVersionString,  0 );
	AddNumToDictionary (mutableRef, kTypeString, cd.componentType);
	AddNumToDictionary (mutableRef, kSubtypeString, cd.componentSubType);
	AddNumToDictionary (mutableRef, kManufacturerString, cd.componentManufacturer);
	CFDictionarySetValue (mutableRef, kNameString, CFSTR("Untitled"));


	fDictionary = mutableRef;
}

optional<AudioComponentDescription>
AudioUnits::PropertyList::getComponentDescriptionFromPreset( CFDictionaryRef dict)
{
    AudioComponentDescription d;
	bool worked = GetNumFromDictionary ( dict, kTypeString, d.componentType ) and
        GetNumFromDictionary ( dict, kSubtypeString, d.componentSubType ) and
		GetNumFromDictionary ( dict, kManufacturerString, d.componentManufacturer );
    if(not worked) return nullptr;
    return d;
}

AudioUnits::PropertyList::PropertyList( const PropertyList& propList ):
	fDictionary( propList.fDictionary )
{
	CFRetain( fDictionary );
}

AudioUnits::PropertyList::PropertyList( CFPropertyListRef pl ):
	fDictionary( (CFDictionaryRef) pl )
{
}

AudioUnits::PropertyList::PropertyList( CFDictionaryRef dict ):
	fDictionary( dict )
{
}

AudioUnits::PropertyList::~PropertyList()
{
	if ( fDictionary )
		CFRelease( fDictionary );
}

enum DictIOTypes
{
	kDIO_StringType,
	kDIO_DoubleType,
	kDIO_LongLongType,
	kDIO_DataType,
	kDIO_EndOfListType,
	kDIO_UnknownType,
	kDIO_XMLBlob
};


int32_t	AudioUnits::PropertyList::getVersion()
{
	int32_t retVal;
	CFNumberRef cfnum = reinterpret_cast<CFNumberRef>(CFDictionaryGetValue(fDictionary, CFSTR("version")));
 	CFNumberGetValue( cfnum, kCFNumberSInt32Type, &retVal );
 	return retVal;
}

namespace
{
void demandMutableDict( CFMutableDictionaryRef& dict )
{
	if ( dict == NULL )
	{
		dict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	}
}
} // unnamed namespace

void Base::createListener()
{
	DCL_AU_FUNC(createListener)
	FailAudioUnitError( AUEventListenerCreate( ListenerProc, this,
						CFRunLoopGetCurrent(), kCFRunLoopDefaultMode, 0.001, 0.001, &fParamListenerRef ), AU_DESC );

	std::vector<AudioUnitParameterID> params = getGlobalParameterList();

	for ( int i = 0; i < params.size(); ++i )
	{
		AudioUnitParameter param;
		param.mAudioUnit = fCi;
		param.mParameterID = params[i];
		param.mScope = kAudioUnitScope_Global;
		param.mElement = 0;

		AudioUnitEvent e;

		e.mArgument.mProperty.mAudioUnit = fCi;
		e.mArgument.mProperty.mPropertyID = params[i];
		e.mArgument.mProperty.mScope = kAudioUnitScope_Global;
		e.mArgument.mProperty.mElement = 0;

		e.mEventType = kAudioUnitEvent_ParameterValueChange;
		AUEventListenerAddEventType( fParamListenerRef, NULL, &e);

		e.mEventType = kAudioUnitEvent_BeginParameterChangeGesture;
		AUEventListenerAddEventType( fParamListenerRef, NULL, &e);

		e.mEventType = kAudioUnitEvent_EndParameterChangeGesture;
		AUEventListenerAddEventType( fParamListenerRef, NULL, &e);

		e.mEventType = kAudioUnitEvent_PropertyChange;
		e.mArgument.mProperty.mPropertyID = kAudioUnitProperty_PresentPreset;

		AUEventListenerAddEventType( fParamListenerRef, NULL, &e);

		e.mEventType = kAudioUnitEvent_PropertyChange;
		e.mArgument.mProperty.mPropertyID = kAudioUnitProperty_CurrentPreset;

		AUEventListenerAddEventType( fParamListenerRef, NULL, &e);

	}
}
void Base::ListenerProc( void* inCallbackRefCon, void* inObject,
								const AudioUnitEvent* inEvent, UInt64 inEventHostTime, Float32	inParameterValue )
{
	((Base*)inCallbackRefCon)->ListenToEvent( inEvent, inEventHostTime, inParameterValue );
}
void Base::ListenToEvent( const AudioUnitEvent* inEvent, UInt64 inEventHostTime, Float32 inParameterValue )
{
	if ( inEvent->mEventType == kAudioUnitEvent_ParameterValueChange )
	{
		for ( int i = 0; i < fEventListeners.size(); ++i )
			fEventListeners[i]->ListenToChange( inEventHostTime, &(inEvent->mArgument.mParameter), inParameterValue );
	}
	else if ( inEvent->mEventType == kAudioUnitEvent_BeginParameterChangeGesture )
	{
		for ( int i = 0; i < fEventListeners.size(); ++i )
			fEventListeners[i]->ListenToGesture( true, inEventHostTime, &(inEvent->mArgument.mParameter) );
	}
	else if ( inEvent->mEventType ==kAudioUnitEvent_EndParameterChangeGesture )
	{
		for ( int i = 0; i < fEventListeners.size(); ++i )
			fEventListeners[i]->ListenToGesture( false, inEventHostTime, &(inEvent->mArgument.mParameter) );
	}
	else if ( inEvent->mEventType ==kAudioUnitEvent_PropertyChange )
	{
		if (inEvent->mArgument.mProperty.mPropertyID == kAudioUnitProperty_PresentPreset or inEvent->mArgument.mProperty.mPropertyID == kAudioUnitProperty_CurrentPreset )
		{
			AUPreset preset;
			preset.presetName = NULL;
			try
			{
				getCurrentPreset( preset );

				if ( preset.presetNumber != -1 )	// don't notify on user preset changes, since that's definitely us making the change
				{
					for ( int i = 0; i < fEventListeners.size(); ++i )
						fEventListeners[i]->ListenToPresetChange( preset );
				}
			}
			catch(...)
			{
			}
			if ( preset.presetName )
				CFRelease( preset.presetName );
		}
	}
}
void Base::RegisterEventListener( EventListener* listener )
{
	std::vector<EventListener*>::iterator iter = find( fEventListeners.begin(), fEventListeners.end(), listener );
	if ( iter == fEventListeners.end() )
		fEventListeners.push_back( listener );
}
void Base::UnregisterEventListener( EventListener* listener )
{
	std::vector<EventListener*>::iterator iter = find( fEventListeners.begin(), fEventListeners.end(), listener );
	if ( iter != fEventListeners.end() )
	{
		fEventListeners.erase( iter );
	}
}
void Base::setCallbacks( const struct AUHostCallbackStruct& callbacks )
{
 	UInt32 dataSize;
	Boolean writable;
	if ( GetGlobalPropertyInfo( fCi,  kAudioUnitProperty_HostCallbacks, dataSize, writable) )
	{
		// ignore errors
		dataSize = std::min( dataSize, static_cast<UInt32>(sizeof(AUHostCallbackStruct)) );
		AudioUnitSetProperty( fCi, kAudioUnitProperty_HostCallbacks, kAudioUnitScope_Global, 0, (void*)&callbacks, dataSize );
	}
}

void Base::clearCallbacks()
{
	AUHostCallbackStruct info;
    memset (&info, 0, sizeof ( AUHostCallbackStruct ));
	setCallbacks( info );
}

void PropertyList::drizzleDataMember()
{
#if IS_DRIZZLING
	CFDataRef cfdata = reinterpret_cast<CFDataRef>(CFDictionaryGetValue(fDictionary, CFSTR("data")));
	int32_t len = CFDataGetLength(cfdata);
	const mUInt8* data = CFDataGetBytePtr(cfdata);
	drizzlef(("CFDataRef: "));
	bool firstTime = true;
	for ( int i = 0; i < len; ++i )
	{
		if ( not firstTime )
			drizzlef((", "));
		drizzlef(( "%2lX", data[i]  ));
		firstTime = false;
	}
	drizzlef(("\n"));
#endif
}

bool cd_equal( const AudioComponentDescription& a, const AudioComponentDescription& b )
{
	return	a.componentType == b.componentType and
				a.componentSubType == b.componentSubType and
				a.componentManufacturer == b.componentManufacturer;
}

} // namespace AudioUnits
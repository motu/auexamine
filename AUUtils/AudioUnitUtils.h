//
// Copyright (c) 2013 MOTU, Inc. All rights reserved.
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file.
//
#ifndef _AUDIOUNITUTILS_H_
#define _AUDIOUNITUTILS_H_

/**********************************************************************************

	AudioUnitUtils

	Utilities for interacting with AudioUnits

**********************************************************************************/

#include <vector>
#include <string>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioUnitUtilities.h>
#include <Carbon/Carbon.h>
#include <CoreFoundation/CFURL.h>
#include "optional.h"
#include "scoped_cftyperef.h"

typedef OSStatus (*HostCallback_GetTransportState) (void 	*inHostUserData,
										Boolean 			*outIsPlaying,
										Boolean 			*outTransportStateChanged,
										Float64 			*outCurrentSampleInTimeLine,
										Boolean 			*outIsCycling,
										Float64 			*outCycleStartBeat,
										Float64 			*outCycleEndBeat);

struct AUHostCallbackStruct
{
	void *									hostUserData;
	HostCallback_GetBeatAndTempo 			beatAndTempoProc;
    HostCallback_GetMusicalTimeLocation     musicalTimeLocationProc;
	HostCallback_GetTransportState			transportStateProc;
};

namespace AudioUnits
{

struct CocoaUIInfo
{
	CocoaUIInfo() {}

	ScopedCFTypeRef<CFURLRef> bundleLocation;
	std::vector<ScopedCFTypeRef<CFStringRef> >	classes;
};

class Base;
struct GUIInfo
{
    optional<CocoaUIInfo> getCocoa() {return fCocoa;}
    optional<AudioComponentDescription> getCarbon() {return fCarbon;}

    bool useCocoa( bool hostIsCocoa ) const
	{
        // if the host is cocoa, then prefer cocoa.
        if(hostIsCocoa)
            return fCocoa.hasValue();

        // otherwise, prefer non-generic carbon
        return fCocoa.hasValue() and (not fGeneric);
    }

private:
    // this can only be created by "base".
   	GUIInfo(optional<CocoaUIInfo> cocoa, optional<AudioComponentDescription> carbon, bool genericCarbon)
        : fCocoa(cocoa), fCarbon(carbon), fGeneric(genericCarbon) {}

	friend class Base;

    optional<CocoaUIInfo> fCocoa;
    optional<AudioComponentDescription> fCarbon;
    bool fGeneric;
};

class PropertyList;

class Base
{
public:
    Base(const AudioComponentDescription& desc);

	virtual ~Base();
	void SetHostInfo( CFStringRef host, const AUNumVersion& version );
	void Initialize();
	void Uninitialize();
	bool IsInitialized() const { return fIsInitialized; }
	void Reset();

	void setFromMASData( char* buffer, int32_t len );

	bool presetsRequireInit();
	void testSupportsPrioritizedMIDI();
	bool supportsPrioritizedMIDI() { return fSupportsPrioritizedMIDI; }
	void getPresetList( CFArrayRef& );

	void getCurrentPreset( AUPreset& preset );
	void setCurrentPreset( AUPreset& preset );

	std::vector<AudioUnitParameterID> getGlobalParameterList( );
	void	getGlobalParameterInfo( AudioUnitParameterID id, AudioUnitParameterInfo& info );
	void	getGlobalParameterValueStrings( AudioUnitParameterID id, CFArrayRef& strings );
	float	getGlobalParameterValue( AudioUnitParameterID id );

	std::vector<AudioUnitParameterID> getParameterList( int scope );
	void	getParameterInfo( int scope, AudioUnitParameterID id, AudioUnitParameterInfo& info );
	void	getParameterValueStrings( int scope, AudioUnitParameterID id, CFArrayRef& strings );
	float	getParameterValue( int scope, int32_t element, AudioUnitParameterID id );

/*  TODO: rewrite using standard-compliant types.
	bool	parameterValueToString( int scope, int32_t element, AudioUnitParameterID id, float val, MotuString* str );
	bool	stringToParameterValue( int scope, int32_t element, AudioUnitParameterID id, ConstMotuString* str, float& val );
*/
	//std::vector<AudioUnitMIDIControlMapping> getMIDIControlMapping();

	void setStreamFormat( int scope, int busNumber, const AudioStreamBasicDescription& desc );
	void getStreamFormat( int scope, int busNumber, AudioStreamBasicDescription& desc );

	int32_t	getNumBusses(int scope, bool& writable );
	void	setNumBusses(int scope, int num );
	optional<CFStringRef>	getBusName( int scope, int bus );
	std::vector<AUChannelInfo>	getSupportedNumChannels();

	Float64	getLatency();
	optional<Float64> getTail();
	void	setMaxFramesPerSlice( uint32_t );
	void	setRealtimeHint( bool realtime );

	GUIInfo getGUIInfo();
 	optional<CocoaUIInfo> getCocoaUIInfo();

	std::vector<AudioComponentDescription> getCarbonUIComponentList();

	std::vector<AudioUnitOtherPluginDesc> getReplacementList();

	bool	getIsBypassed();
	void	setIsBypassed( bool is );

	void setRenderCallback( AURenderCallback ci, void* data, int bus );
	void removeRenderCallback(int bus);

	void scheduleParameters(const AudioUnitParameterEvent* inParameterEvent, uint32_t inNumParamEvents );

	void setParameter(   AudioUnitParameterID inID,
						  AudioUnitScope         inScope,
						  AudioUnitElement       inElement,
						  Float32                inValue,
						  uint32_t                 inBufferOffsetInFrames );

	void broadcastGesture(   AudioUnitParameterID inID,
						  AudioUnitScope         inScope,
						  AudioUnitElement       inElement,
						  bool	beginning );

	void render( AudioUnitRenderActionFlags& ioActionFlags,
 	 			 const AudioTimeStamp& inTimeStamp,
  					uint32_t inOutputBusNumber,
  					uint32_t inNumberFrames,
  					AudioBufferList* ioData);

  	const AudioComponentDescription& getComponentDescription()
  		{ return fComponentDesc; }

  	AudioUnit getInternalAudioUnit() { return fCi; }

  	class EventListener
  	{
  		Base* fOurBase;
  	public:
  		EventListener( Base* b ): fOurBase(b){}

  		void SetAU( Base* b ) { if ( fOurBase ) fOurBase->UnregisterEventListener( this ); fOurBase = b; }

  		void Register() { if ( fOurBase != NULL ) fOurBase->RegisterEventListener( this ); }
  		void BaseDied() { fOurBase = NULL; }

  		virtual void ListenToGesture( bool begin, UInt64 inEventHostTime, const AudioUnitParameter* inParameter ) = 0;
  		virtual void ListenToChange( UInt64 inEventHostTime, const AudioUnitParameter* inParameter, Float32 inValue ) = 0;
  		virtual void ListenToPresetChange( AUPreset& preset ) = 0;

  		virtual ~EventListener() { if ( fOurBase ) fOurBase->UnregisterEventListener( this ); }
  	};
  	void RegisterEventListener( EventListener* listener );
  	void UnregisterEventListener( EventListener* listener );

  	void setCallbacks( const struct AUHostCallbackStruct& callbacks );
  	void clearCallbacks();

  	bool IsASynth() { return fComponentDesc.componentType == kAudioUnitType_MusicDevice; }

  	int32_t getComponentVersion();

  	CFURLRef getMIDIXMLDoc();

  	void translateAutomation( const AudioUnitOtherPluginDesc& desc,
								int32_t& id, float& value );

	void getSupportedLayouts( bool input, int bus, std::vector<AudioChannelLayoutTag>& layouts );

	enum GetLayoutRetVals
	{
		kNoError,
		kNotCurrentlySet,
		kNotSupported
	};
	GetLayoutRetVals getCurrentChannelLayout( bool input, int bus, AudioChannelLayoutTag& cur, bool& writable );

	bool setChannelLayout( bool input, int bus, const AudioChannelLayout* layout, int byteSize );

  	void makeAndDeleteListener();
	PropertyList getClassInfo();

protected:
	AudioUnit fCi;
	bool fIsInitialized;
	bool fSupportsPrioritizedMIDI;
 	AudioComponentDescription fComponentDesc;
	AUEventListenerRef fParamListenerRef;

#if SUPPORT_AU_VERSION_1
	AudioUnitRenderSliceProc	fRenderProc;
#endif

	void setClassInfo( const PropertyList& propList );

private:
	void createListener();
	static void ListenerProc( void* inCallbackRefCon, void* inObject,
								const AudioUnitEvent* inEvent, UInt64 inEventHostTime, Float32 inParameterValue );

	void ListenToEvent( const AudioUnitEvent*	inEvent, UInt64 inEventHostTime, Float32 inParameterValue );
	std::vector<EventListener*> fEventListeners;
	OSStatus testPropList( CFPropertyListRef plist );
};

struct UTF8ComponentInfo
{
    std::string name;
    std::string info;
};

optional<UTF8ComponentInfo> GetUTF8ComponentInfo( const AudioComponentDescription& desc );
optional<uint32_t> GetComponentVersion( const AudioComponentDescription& desc );

std::vector<AudioComponentDescription> GetEffectList();
std::vector<AudioComponentDescription> GetSynthList();
std::vector<AudioComponentDescription> GetCompleteList();

struct FuncDesc
{
	const char* name;
	int line;

	FuncDesc( const char* n, int l ):
		name(n), line(l){}
};
#define DCL_AU_FUNC(x) const char* x_name = #x; //FuncTimer t(x_name);
#define AU_DESC FuncDesc( x_name, __LINE__ )
#define AU_FILEDESC FuncDesc( __FILE__, __LINE__ )
void FailAudioUnitError( int32_t error, const FuncDesc& desc, bool* negOneErrorCode = NULL );


class PropertyList
{
	CFDictionaryRef fDictionary;

	const PropertyList& operator = (const PropertyList&);
public:
	PropertyList():fDictionary(NULL){}
	PropertyList( const PropertyList& propList );
	PropertyList( CFPropertyListRef pl );
	PropertyList( CFDictionaryRef dict );
	PropertyList( char* buffer, int32_t len, const AudioComponentDescription& cd );
	~PropertyList();

	int32_t	getVersion();

	CFDictionaryRef	getDict() { return fDictionary; }
	CFDictionaryRef releaseDictionary()
	{
		CFDictionaryRef dict = fDictionary;
		fDictionary = NULL;

		return dict;
	}

	static optional<AudioComponentDescription> getComponentDescriptionFromPreset( CFDictionaryRef dict );
private:
	CFPropertyListRef	getPropList() const { return (CFPropertyListRef)fDictionary; }
	void drizzleDataMember();

	friend class Base;
};

bool cd_equal( const AudioComponentDescription& a, const AudioComponentDescription& b );

} // AudioUnits namespace

#endif // _AUDIOUNITUTILS_H_

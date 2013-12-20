//
// Copyright (c) 2013 MOTU, Inc. All rights reserved.
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file.
//

#include "AUTortureTest.h"
#include "gtest/gtest.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#define DP_VERSION 0

bool gRequiresInit = false;
using namespace std;


namespace
{
    const int kTimesToRepeatTests = 5;
    
    class InitializedAudioUnit : public AudioUnits::Base
    {
        public:
            InitializedAudioUnit(ComponentDescription d) :
                AudioUnits::Base(move(d)),
                wasInitialized(false)
            {
                if(gRequiresInit)
                {
                    Initialize();
                    wasInitialized = true;
                }
            }
            
            ~InitializedAudioUnit()
            {
                if(wasInitialized)
                    Uninitialize();
            }
        
            InitializedAudioUnit(const InitializedAudioUnit&) = delete;
            const InitializedAudioUnit& operator=(const InitializedAudioUnit&) = delete;
            bool wasInitialized;
    };

    // we keep a AU instance in globals so we don't have to recreate it for every test case.
    // (recreating a AU can be prohibitively slow for some AUs)
    class Globals : public ::testing::Environment
    {
        public:
            Globals(ComponentDescription cd) :
                cd(std::move(cd)),
                unauthorized(false)
            {}
        
            void SetUp()
            {
                // force "requires init" if it's a non-apple version one component.
                if ( (not gRequiresInit) and AudioUnits::GetComponentVersion(cd) == 1 && cd.componentManufacturer != 'appl')
                    gRequiresInit = true;

                audioUnit.reset(new InitializedAudioUnit(cd));
            }
        
            void TearDown()
            {
                // we have to delete everything that we created in SetUp here.
                audioUnit.reset();
            }
        
            ComponentDescription cd;
            bool unauthorized;
            shared_ptr<InitializedAudioUnit> audioUnit;
    };
    
    // owned by gtest
    Globals* globals;
    
    void HandleErrors(std::function<void ()> f)
    {
        try
        {
            f();
        }
        catch(int32_t errorCode)
        {
            if ( errorCode == kAudioUnitErr_Uninitialized )
            {
                // try again, with initialization.
                if ( not gRequiresInit )
                {
                    gRequiresInit = true;
                    globals->audioUnit.reset(new InitializedAudioUnit(globals->cd));
                    HandleErrors(f);
                }
            }

            if ( errorCode == kAudioUnitErr_Unauthorized )
            {
                globals->unauthorized = true;
                FAIL() << " unauthorized.";
            }
            
            FAIL()<<"AU Error:"<<errorCode;
        }
        catch(...)
        {
            FAIL()<<"unknown exception";
        }
    }
    
    class AUTest : public ::testing::TestWithParam<int>
    {
    public:
        // this is just a convenience to make the global object
        // accessible to each test.
        AUTest() :  cd(globals->cd),
                    audioUnit(globals->audioUnit)
        {}

    protected:
        ComponentDescription& cd;
        shared_ptr<InitializedAudioUnit>& audioUnit;
    };

    #define BEGIN_AUTEST(x) TEST_P(AUTest, x) { HandleErrors([&](){
    #define END_AUTEST });}
    
    BEGIN_AUTEST(ReinitializeInstance)
        audioUnit.reset(new InitializedAudioUnit(cd));
    END_AUTEST
    
    struct OSTypeName
    {
        char name[5];
    };

    OSTypeName GetOSTypeName( int32_t osType )
    {
        OSTypeName ret;
    #if TARGET_RT_BIG_ENDIAN
        memcpy( ret.name, &osType, 4 );
    #elif TARGET_RT_LITTLE_ENDIAN
        char* namePtr = (char*)&osType;
        ret.name[0] = namePtr[3];
        ret.name[1] = namePtr[2];
        ret.name[2] = namePtr[1];
        ret.name[3] = namePtr[0];
    #endif
        ret.name[4] = 0;
        return ret;
    }
    
    void inspectOneElement( const void* it  )
    {
        char str[100];
        std::string type;
        CFTypeID valueType = CFGetTypeID( it );
        
        if ( valueType == CFStringGetTypeID() )
        {
            CFStringGetCString( (CFStringRef)it, str, 99,  kCFStringEncodingUTF8 );
            type = "cfstr";
        }
        else if ( valueType == CFNumberGetTypeID() )
        {
            double d;
            CFNumberGetValue( (CFNumberRef)it, kCFNumberFloat64Type, &d);
            if ( CFNumberIsFloatType( (CFNumberRef)it) )
                sprintf( str, "flt %.3f", d );
            else
            {
                int32_t num = d;
                sprintf( str, "int %d : %s", num, GetOSTypeName(num).name );
            }
            
            type = "cfnum";
        }
        else if ( valueType == CFDataGetTypeID() )
        {
            int len = CFDataGetLength((CFDataRef)it);
            const UInt8* data = CFDataGetBytePtr((CFDataRef)it);
            
            sprintf( str, "%d bytes begining: ", len);
            int index = 0;
            while ( len > 0 && strlen( str ) < 50 )
            {
                char num[10];
                sprintf( num, "%02X ", data[index] );
                strcat( str, num );
                --len;
                ++index;            
            }
            type = "cfdata";
        }
    }

    void inspectOneEntry(const void *key, const void *value, void *)
    {
        inspectOneElement( key );
        inspectOneElement( value );
    }
    
    BEGIN_AUTEST(InspectClassInfo)
        AudioUnits::PropertyList propList = audioUnit->getClassInfo();
        if ( propList.getDict() != NULL )
            CFDictionaryApplyFunction( propList.getDict(), inspectOneEntry, NULL );
    END_AUTEST
    
    BEGIN_AUTEST(InspectPresetInfo)
        char name[100];
        CFArrayRef presetList;
        audioUnit->getPresetList( presetList );
        if ( presetList )
        {
            int num = CFArrayGetCount(presetList);
            for ( int i = 0 ; i < num; ++i )
            {
                AUPreset* preset = (AUPreset*) CFArrayGetValueAtIndex(presetList, i);
                if ( preset )
                {
                    CFStringGetCString( preset->presetName, name, 99,  kCFStringEncodingUTF8 );
                }
                else
                {
                }
            }
            CFRelease( presetList );
        }
        else
        {
        }

        AUPreset curPreset;
        audioUnit->getCurrentPreset( curPreset );
        CFStringGetCString( curPreset.presetName, name, 99,  kCFStringEncodingUTF8 );
        CFRelease( curPreset.presetName );
    END_AUTEST
    
    BEGIN_AUTEST(InspectParameterInfo)
        std::vector<AudioUnitParameterID> ids = audioUnit->getParameterList( kAudioUnitScope_Global );
        
        int32_t numBlank = 0;
        
        for ( const AudioUnitParameterID& id : ids)
        {
            char nameStr[100];
            AudioUnitParameterInfo info;
            info.cfNameString = NULL;
            audioUnit->getParameterInfo( kAudioUnitScope_Global, id, info );
     
            if ( info.flags & kAudioUnitParameterFlag_HasCFNameString )
            {
                CFStringGetCString( info.cfNameString, nameStr, 99,  kCFStringEncodingUTF8 );
            }
            else
            {
                strcpy( nameStr, info.name );
            }
            
            if ( strlen( nameStr) == 0 )
            {
                numBlank++;
            }

            if ( info.unit == kAudioUnitParameterUnit_Indexed )
            {
                CFArrayRef strings;
                audioUnit->getParameterValueStrings( kAudioUnitScope_Global, id, strings );
                if ( strings )
                {
                    int num = CFArrayGetCount(strings);
                    char presetvalstr[100];
                    for ( int j = 0; j < num; ++j )
                    {
                        CFStringGetCString( (CFStringRef)CFArrayGetValueAtIndex(strings, j), presetvalstr, 99,  kCFStringEncodingUTF8 );
                    }
                    CFRelease( strings );
                }
                else
                {
                }
            }
        }
        if ( numBlank > 0 )
        {
        }
    END_AUTEST
    
    BEGIN_AUTEST(InspectBusAndChannelInfo)
        std::vector<AUChannelInfo> supChans = audioUnit->getSupportedNumChannels();
    END_AUTEST
    
    BEGIN_AUTEST(InspectLatency)
        audioUnit->getLatency();
    END_AUTEST
    
    BEGIN_AUTEST(InspectUIComponentList)
        AudioUnits::GUIInfo info = audioUnit->getGUIInfo();
        if ( not info.useCocoa( true ) and info.getCarbon().hasValue())
        {
            AudioUnits::GetUTF8ComponentInfo( *info.getCarbon() );
        }
    END_AUTEST

    static void inspectOneStreamFormat( shared_ptr<InitializedAudioUnit>& base, int scope  )
    {
        AudioStreamBasicDescription desc;
        base->getStreamFormat( scope, 0, desc );
        if ( base->IsInitialized() )
            base->Uninitialize();
        base->setStreamFormat( scope, 0, desc );
        base->Initialize();
    }
    
    BEGIN_AUTEST(InspectStreamFormat)
        if ( not audioUnit->IsASynth() )
            inspectOneStreamFormat( audioUnit, kAudioUnitScope_Input );
        inspectOneStreamFormat( audioUnit, kAudioUnitScope_Output );
    END_AUTEST
    
    BEGIN_AUTEST(MakeAndDeleteListener)
        audioUnit->makeAndDeleteListener();
    END_AUTEST
    
    BEGIN_AUTEST(TestComponentVersion)
        audioUnit->getComponentVersion();
    END_AUTEST
    
    // this somewhat complicated test ensures that we can schedule correctly.
    OSStatus renderCallback(void *, AudioUnitRenderActionFlags *, const AudioTimeStamp *,
                                UInt32 , UInt32 , AudioBufferList *ioData)
    {
        for ( int i = 0; i < ioData->mNumberBuffers; ++i )
        {
            if ( ioData->mBuffers[i].mData )
                memset ( ioData->mBuffers[i].mData, 0, ioData->mBuffers[i].mDataByteSize );
        }
        return noErr;
    }

    const int kTestFrames = 2048;

    void setupTestStreamFormat( shared_ptr<InitializedAudioUnit>& aunt, int32_t& numIn, int32_t& numOut )
    {
        AudioStreamBasicDescription description;
        
        int32_t numIns;

        if ( not aunt->IsASynth() )
        {
            aunt->getStreamFormat( kAudioUnitScope_Input, 0, description );
            numIns = description.mChannelsPerFrame;
        }
        
        aunt->getStreamFormat( kAudioUnitScope_Output, 0, description );
        
        description.mSampleRate = 44100;
        description.mFormatID = kAudioFormatLinearPCM;
        description.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kLinearPCMFormatFlagIsNonInterleaved;       //  flags specific to each format
        description.mBytesPerPacket = sizeof ( float ); 
        description.mFramesPerPacket = 1;
        description.mBytesPerFrame = sizeof ( float ); 
        description.mBitsPerChannel = sizeof ( float ) * 8;
        numOut = description.mChannelsPerFrame;
        
        aunt->setStreamFormat( kAudioUnitScope_Output, 0, description );
        
        if ( not aunt->IsASynth() )
        {
            description.mChannelsPerFrame = numIns;
            numIn = numIns;
            aunt->setStreamFormat( kAudioUnitScope_Input, 0, description );
        
            aunt->setRenderCallback( renderCallback, NULL, false );
        }
        else
            numIn = 0;
        
        aunt->setMaxFramesPerSlice( kTestFrames );
    }

    bool getAParamForScheduleTest( std::vector<AudioUnitParameterID>& ids, shared_ptr<InitializedAudioUnit>& audioUnit, AudioUnitParameterID& id, Float32& minVal, Float32& maxVal, bool& ramps )
    {
        int32_t firstAuto = -1;
        ramps = false;
        try
        {
            for ( int i = 0; i < ids.size(); ++i )
            {
                AudioUnitParameterInfo itsInfo;
                itsInfo.cfNameString = NULL;
                itsInfo.name[0] = 0;
                itsInfo.flags = 0;

                audioUnit->getParameterInfo( kAudioUnitScope_Global, ids[i], itsInfo );

                if ( (itsInfo.flags & kAudioUnitParameterFlag_CanRamp) != 0 )
                {
                    id = ids[i];
                    minVal = itsInfo.minValue;
                    maxVal = itsInfo.maxValue;
                    ramps = true;
                    return true;
                }
                else if ( firstAuto == -1 )
                {
                    firstAuto = id = ids[i];
                    minVal = itsInfo.minValue;
                    maxVal = itsInfo.maxValue;
                }           
            }
        }   
        catch(...)
        {
        }
        return firstAuto != -1;
    }
    
    // Dummy defaults for our host callbacks. Turns out some plug-ins (such as
    // Audio Damage's Axon) don't manage correctly without any callbacks
    // specified. Axon hangs, for instance, when rendering.
    namespace
    {
        struct HostData
        {
            char unused[16];
        } gHostData;
        
        OSStatus GetBeatAndTempoProc(
            void* inHostUserData,
            Float64* outCurrentBeat,
            Float64* outCurrentTempo)
        {
            if ( inHostUserData )
            {
                if (outCurrentBeat)
                    *outCurrentBeat = 0;
                
                if (outCurrentTempo)
                    *outCurrentTempo = 120;
                return noErr;
            }

            return kAudioUnitErr_InvalidParameter;        
        }
        OSStatus GetMusicalTimeLocationProc(
            void* inHostUserData,
            UInt32* outDeltaSampleOffsetToNextBeat,
            Float32* outTimeSig_Numerator,
            UInt32* outTimeSig_Denominator,
            Float64*  outCurrentMeasureDownBeat)
        {
            if ( inHostUserData )
            {
                if (outDeltaSampleOffsetToNextBeat)
                    *outDeltaSampleOffsetToNextBeat = 0;
                
                if (outTimeSig_Numerator)
                    *outTimeSig_Numerator = 4;
                
                if (outTimeSig_Denominator)
                    *outTimeSig_Denominator = 4;
                
                if (outCurrentMeasureDownBeat)
                    *outCurrentMeasureDownBeat = 0;
            }

            return kAudioUnitErr_InvalidParameter;        
        }            

        OSStatus GetTransportStateProc(
            void* inHostUserData,
            Boolean* outIsPlaying,
            Boolean* outTransportStateChanged,
            Float64* outCurrentSampleInTimeLine,
            Boolean* outIsCycling,
            Float64* outCycleStartBeat,
            Float64* outCycleEndBeat)
        {
            if ( inHostUserData )
            {
                if (outIsPlaying)
                    *outIsPlaying = true;
                
                if (outTransportStateChanged)
                    *outTransportStateChanged = false;
                
                if (outCurrentSampleInTimeLine)
                    *outCurrentSampleInTimeLine = 0;
                
                if (outIsCycling)
                    *outIsCycling = false;
                
                if (outCycleStartBeat)
                    *outCycleStartBeat = 0;
                
                if (outCycleEndBeat)
                    *outCycleEndBeat = 0;
            }

            return kAudioUnitErr_InvalidParameter;        
        }
    } // anonymous namespace

    BEGIN_AUTEST(TestSchedulingAbility)
        std::vector<AudioUnitParameterID> ids = audioUnit->getParameterList( kAudioUnitScope_Global );
        if ( ids.size() == 0 ) return;
        
        bool wasInited = audioUnit->IsInitialized();
        if ( audioUnit->IsInitialized() )
            audioUnit->Uninitialize();
        
        int32_t numIn, numOuts;
        setupTestStreamFormat( audioUnit, numIn, numOuts );

        audioUnit->Initialize();
        
        AUHostCallbackStruct info = { &gHostData, GetBeatAndTempoProc, GetMusicalTimeLocationProc, GetTransportStateProc };
        audioUnit->setCallbacks(info);
        
        vector<char> fBufferListBuffer(sizeof( AudioBufferList ) + (sizeof(AudioBuffer) * numOuts));
        AudioBufferList* fBufferList = reinterpret_cast<AudioBufferList*>(fBufferListBuffer.data());

        fBufferList->mNumberBuffers = numOuts;
        for ( int i = 0; i < numOuts; ++i )
        {
            fBufferList->mBuffers[i].mNumberChannels = 1;
            fBufferList->mBuffers[i].mDataByteSize = sizeof( float ) * kTestFrames; 
            fBufferList->mBuffers[i].mData = new float[kTestFrames];
        }

        try
        {
            AudioUnitRenderActionFlags actionFlags = 0;
            AudioTimeStamp timestamp;
            timestamp.mSampleTime = 0;
            timestamp.mFlags = kAudioTimeStampSampleTimeValid;

            audioUnit->render( actionFlags, timestamp, 0, kTestFrames, fBufferList );   

            int32_t smallBuf = sizeof( float ) * (kTestFrames / 4);
            for ( int i = 0; i < numOuts; ++i )
                fBufferList->mBuffers[i].mDataByteSize = smallBuf;  

            AudioUnitParameterID id;
            Float32 minVal, maxVal;
            bool ramps;
            if ( getAParamForScheduleTest( ids, audioUnit, id, minVal,  maxVal, ramps ) )
            {
                AudioUnitParameterEvent thisParam;
                thisParam.scope = kAudioUnitScope_Global;
                thisParam.element = 0;
                if ( ramps )
                {
                    thisParam.eventType = kParameterEvent_Ramped;
                    thisParam.parameter = id;
                    thisParam.eventValues.ramp.startBufferOffset = ((kTestFrames / 4) / 2);
                    thisParam.eventValues.ramp.durationInFrames = (kTestFrames / 4);
                    thisParam.eventValues.ramp.startValue = minVal;
                    thisParam.eventValues.ramp.endValue = maxVal;
                }
                else
                {
                    thisParam.eventType = kParameterEvent_Immediate;
                    thisParam.parameter = id;
                    thisParam.eventValues.immediate.bufferOffset = ((kTestFrames / 4) / 2);
                    thisParam.eventValues.immediate.value = minVal;
                }       
                
                audioUnit->scheduleParameters( &thisParam, 1 );
            }
            
            timestamp.mSampleTime = kTestFrames;
            timestamp.mFlags = kAudioTimeStampSampleTimeValid;
            actionFlags = 0;
            audioUnit->render( actionFlags, timestamp, 0, kTestFrames / 4, fBufferList );   
        }
        catch(...)
        {
        }
        for ( int i = 0; i < numOuts; ++i )
            delete [] (float*) fBufferList->mBuffers[i].mData;
            
        audioUnit->clearCallbacks();

        audioUnit->Uninitialize();

        if ( not audioUnit->IsASynth() )
            audioUnit->removeRenderCallback(false);
        
        if ( wasInited )
            audioUnit->Initialize();
    END_AUTEST

    INSTANTIATE_TEST_CASE_P(AUTest, AUTest, ::testing::Range(0, kTimesToRepeatTests));
    
        // this is the test printer that works with Digital Performer
    class DigitalPerformerPrinter : public ::testing::EmptyTestEventListener
    {
    public:
        DigitalPerformerPrinter() : testNumber(0) {}
        virtual void OnTestProgramStart(const ::testing::UnitTest& testInfo)
        {
            cout<<"total, "<<testInfo.total_test_count()<<endl;
        }
        virtual void OnTestStart(const ::testing::TestInfo& testInfo)
        {
            string testName = testInfo.name();
            // since we do each test multiple times, each test has "/N" appended to it
            // to indicate which iteration it is.  let's strip that out for the output.
            testName = testName.substr(0, testName.find_last_of('/'));
            cout<<++testNumber<<", "<<testName<<endl;
        }
    private:
        int testNumber;
    };
}

namespace AudioUnits
{
    void SetupTest(ComponentDescription cd)
    {
        globals = new Globals(std::move(cd));
        // transfer ownership to gtest (we no longer control lifetime)
        ::testing::AddGlobalTestEnvironment(globals);
        
        // we use a minimal test printing style for use with Digital Performer
        #if DP_VERSION
            ::testing::TestEventListeners& listeners = ::testing::UnitTest::GetInstance()->listeners();
            listeners.Append(new DigitalPerformerPrinter); //owned by gtest.
            delete listeners.Release(listeners.default_result_printer());
        #endif
    }
    
    bool IsAuthorized()
    {
        return not globals->unauthorized;
    }
}

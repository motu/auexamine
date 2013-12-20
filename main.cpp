//
// Copyright (c) 2013 MOTU, Inc. All rights reserved.
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file.
//

#include "AUTortureTest.h"
#include "gtest/gtest.h"
#include "AUValExcptList.h"

#include <stdio.h>
#include <pthread.h>
#include <syslog.h>
#include <fcntl.h>
#include <Carbon/Carbon.h>
#include "MotuTargetFlags.h"

#include "CPPAutoReleasePool.h"
#include "AUValStatus.h"

extern "C"
{
	#include <mach/thread_act.h>
	#include <mach/mach_init.h>
	#include <mach/semaphore.h>
	#include <mach/task.h>
}

extern bool gRequiresInit;

namespace {
uint32_t charStrToLong( const char* str )
{
	uint32_t retVal = str[0];
	retVal = (retVal << 8) | str[1];
	retVal = (retVal << 8) | str[2];
	retVal = (retVal << 8) | str[3];
	return retVal;
}
uint32_t str2Long( const char* str )
{
	uint32_t l;
	sscanf( str, "%d", &l );
	return l;
}
int successRet()
{
    return gRequiresInit
           ? kAUValStatusSuccessRequiresInit
           : kAUValStatusSuccessDoesNotRequireInit;
}
}

int main( int argc, char** argv)
{
    // we want to shuffle the order of the tests
    testing::GTEST_FLAG(shuffle) = true;

    // this removes any google test options
    ::testing::InitGoogleTest(&argc, argv);

#if !MOTU_TARGET_RT_64_BIT
  	FlushEvents(everyEvent, 0);
 	EventRecord	classicEvent;
	WaitNextEvent( everyEvent, &classicEvent, 0, NULL);
#endif
	CPPAutoReleasePool autoRelPool(kAUValStatusCrashed);

 	bool charMode = false;

	ComponentDescription cd;
	cd.componentFlags = 0;
	cd.componentFlagsMask = 0;

	if ( argc < 4 )
	{
		printf ("!too few arguments\n");
 		return kAUValStatusNotFound;
	}
	else
	{
        bool charMode = (argc <= 6);

		if (charMode)
		{
			cd.componentType = charStrToLong(argv[1]);
	  		cd.componentSubType = charStrToLong(argv[2]);
	 		cd.componentManufacturer = charStrToLong(argv[3]);
            if(argc > 4)
                gRequiresInit = str2Long(argv[4]);
            else
                gRequiresInit = true;
		}
		else
		{
			cd.componentType = str2Long(argv[1]);
	  		cd.componentSubType = str2Long(argv[2]);
	 		cd.componentManufacturer = str2Long(argv[3]);
	 		gRequiresInit = str2Long(argv[4]);
		}
	}

	optional<AudioUnits::UTF8ComponentInfo> info = AudioUnits::GetUTF8ComponentInfo(cd);
    if (not info.hasValue())
    {
        printf("!Audio Unit not found\n");
        return kAUValStatusNotFound;
    }

    bool masDuplicate = false;
    if ( IsBlackListed( cd, info->name, masDuplicate) )
    {
        if ( masDuplicate )
            return kAUValStatusDuplicateFormat;
        else
            return kAUValStatusIncompatibleVersion;
    }
    if ( IsWhiteListed( cd ) )
        return successRet();

    AudioUnits::SetupTest(cd);
    if(not AudioUnits::IsAuthorized())
        return kAUValStatusNotAuthorized;

    bool success = (RUN_ALL_TESTS() == 0);
    if(not success)
        return kAUValStatusFailure;

    return successRet();
}

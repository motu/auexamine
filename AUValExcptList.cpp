//
// Copyright (c) 2013 MOTU, Inc. All rights reserved.
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file.
//

/****************************************************************************

	AUValExceptionList

	This module defines a 'black list' and a 'white list' for the
	validator... in other words, AUs that will either always be
	rejected without testing, and AUs that will always be passed
	without testing.

	This is necessary for a number of reasons:
		*Some AUs cause kernel panics when tested.
		*Some AUs pass validation, but crash code not touched by
			the validator (UI, Render, etc)
		*Some AUs are duplicated by MAS counterparts, and take a long
			time to validate (Waves)
		*Some AUs work fine, but take a long time to validate (Zoyd)
		*Some AUs are so bizare, that even though they pass validation,
			we shouldn't load them (Reaktor).

****************************************************************************/

#include "AUValExcptList.h"
#include "sorted_vector_map.h"
#include "AudioUnitUtils.h"
#include "AUTortureTest.h"
#include "MotuTargetFlags.h"
#include <stdio.h>

struct AU_Ref
{
	ComponentDescription fDesc;

	AU_Ref(){}
	AU_Ref(const ComponentDescription& desc ):
		fDesc(desc) {}
	AU_Ref( int32_t auType, int32_t auSubT, int32_t auManu )
	{
 		fDesc.componentType = auType;
 		fDesc.componentSubType = auSubT;
 		fDesc.componentManufacturer = auManu;
	}


	bool operator < ( const AU_Ref& p ) const;
};

bool AU_Ref::operator < ( const AU_Ref& p ) const
{
	if ( fDesc.componentType < p.fDesc.componentType )
		return true;
	if ( fDesc.componentType > p.fDesc.componentType )
		return false;

	if ( fDesc.componentManufacturer < p.fDesc.componentManufacturer )
		return true;
	if ( fDesc.componentManufacturer > p.fDesc.componentManufacturer )
		return false;

	if ( fDesc.componentSubType < p.fDesc.componentSubType )
		return true;
	if ( fDesc.componentSubType > p.fDesc.componentSubType )
		return false;

	return false;
}

enum ValidVal
{
	kAllVersionsValid,
	kAllVersionsInvalid,
	kVersionsLTorEQ_Invalid
};

struct ValidState
{
	ValidState(){}
	ValidState( ValidVal v, const char* msg, int32_t version = 0 ):
		valid(v), componentVersion(version), message(msg){}

	ValidVal valid;
	int32_t componentVersion;
	const char* message;
};

typedef sorted_vector_map< AU_Ref, ValidState > ExceptionList;

static const ExceptionList& getExceptionList();

bool IsBlackListed( const ComponentDescription& cd, const std::string& name, bool& masDuplicate )
{
	masDuplicate = false;
	//if ( cd.componentManufacturer == 'ksWV' )
	//{
	//	printf("!Use the MAS version of Waves plug-ins\n");
	//	return true;
	//}

	// we allow Apple AUs that have a -1 version. This is their internal debug vers
	if ( cd.componentManufacturer == 1634758764 and  AudioUnits::GetComponentVersion( cd ) == -1)
		return false;

	char tempStr[500];
	const ExceptionList& excptList = getExceptionList();
	ExceptionList::const_iterator iter = excptList.find( AU_Ref( cd ) );
	bool isInvalid = false;
	if ( iter != excptList.end() )
	{
		const ValidState& vs = (*iter).second;
		if ( vs.valid == kAllVersionsInvalid )
		{
			isInvalid = true;
			masDuplicate = true;
		}
		else if ( vs.valid == kVersionsLTorEQ_Invalid && (AudioUnits::GetComponentVersion( cd ) <= vs.componentVersion) )
			isInvalid = true;

		if ( isInvalid )
		{
			if ( vs.message )
			{
				printf("!%s, %s\n", name.c_str(), vs.message );
			}

			return true;
		}
	}
	return false;
}

bool IsWhiteListed(  const ComponentDescription& cd  )
{
	char tempStr[500];
	const ExceptionList& excptList = getExceptionList();
	ExceptionList::const_iterator iter = excptList.find( AU_Ref( cd ) );
	if ( iter != excptList.end() )
	{
		const ValidState& vs = (*iter).second;
		if ( vs.valid == kAllVersionsValid )
		{
			if ( vs.message )
			{
				printf("!%s\n", vs.message);
			}

			return true;
		}
	}
	return false;
}

static const ExceptionList& getExceptionList()
{
	static bool inited = false;
	static ExceptionList exceptionList;
	if ( not inited )
	{
		inited = true;

		// Black list -----------------
#if !MOTU_TARGET_RT_64_BIT

		// Volta
		exceptionList[ AU_Ref( 'aumu', 'Volt', 'Motu' ) ] = ValidState( kAllVersionsInvalid, "Use the MAS version of Volta" );

 		// Mach-5
		exceptionList[ AU_Ref( 1635085685, 1163555664, 1297044565 ) ] = ValidState( kAllVersionsInvalid, "Use the MAS version of Mach-5" );

 		// ElectricKeys
		exceptionList[ AU_Ref( 'aumu', 'EKEY', 'MOTU' ) ] = ValidState( kAllVersionsInvalid, "Use the MAS version of ElectricKeys" );

 		// Mach-5 2
		exceptionList[ AU_Ref( 'aumu', 'M5II', 'MOTU' ) ] = ValidState( kAllVersionsInvalid, "Use the MAS version of Mach-5" );

		// MX-4
		exceptionList[ AU_Ref( 'aumu', 'MX4!', 'Motu' ) ] = ValidState( kAllVersionsInvalid, "Use the MAS version of MX4" );

		// Ethno
		exceptionList[ AU_Ref( 'aumu', 'ETHN', 'MOTU' ) ] = ValidState( kAllVersionsInvalid, "Use the MAS version of Ethno" );

		// MSI
		exceptionList[ AU_Ref( 'aumu', 'MVO ', 'MOTU' ) ] = ValidState( kAllVersionsInvalid, "Use the MAS version of MSI" );

		// UniversalUVIPlayer
		exceptionList[ AU_Ref( 'aumu', 'UPLA', 'USB ' ) ] = ValidState( kAllVersionsInvalid, "Use the MAS version of this AU" );
		exceptionList[ AU_Ref( 'aumu', 'UVIW', 'UVI ' ) ] = ValidState( kAllVersionsInvalid, "Use the MAS version of this AU" );


		// MOTU: BeatInc & BeatSampler
		exceptionList[ AU_Ref( 'aumu', 'BPM ', 'MOTU' ) ] = ValidState( kAllVersionsInvalid, "Use the MAS version of this AU" );
		exceptionList[ AU_Ref( 'aufx', 'BPMS', 'MOTU' ) ] = ValidState( kAllVersionsInvalid, "Use the MAS version of this AU" );

		// TC
		exceptionList[ AU_Ref( 1635083896, 842282819, 1448301600 ) ] = ValidState( kVersionsLTorEQ_Invalid, "Contact the manufacturer for an updated version of this Audio Unit", 500);
		exceptionList[ AU_Ref( 1635083896, 842282861, 1448301600 ) ] = ValidState( kVersionsLTorEQ_Invalid, "Contact the manufacturer for an updated version of this Audio Unit", 500);
		exceptionList[ AU_Ref( 1635083896, 1129738850, 1448301600) ] = ValidState( kVersionsLTorEQ_Invalid, "Contact the manufacturer for an updated version of this Audio Unit", 500);
		exceptionList[ AU_Ref( 1635083896, 1346585715, 1448301600) ] = ValidState( kVersionsLTorEQ_Invalid, "Contact the manufacturer for an updated version of this Audio Unit", 500);
		exceptionList[ AU_Ref( 1635083896, 1346587757, 1448301600) ] = ValidState( kVersionsLTorEQ_Invalid, "Contact the manufacturer for an updated version of this Audio Unit", 500);
		exceptionList[ AU_Ref( 1635083896, 1346587763, 1448301600) ] = ValidState( kVersionsLTorEQ_Invalid, "Contact the manufacturer for an updated version of this Audio Unit", 500);
		exceptionList[ AU_Ref( 1635083896, 1346596723, 1448301600) ] = ValidState( kVersionsLTorEQ_Invalid, "Contact the manufacturer for an updated version of this Audio Unit", 500);
		exceptionList[ AU_Ref( 1635083896, 1346720115, 1448301600) ] = ValidState( kVersionsLTorEQ_Invalid, "Contact the manufacturer for an updated version of this Audio Unit", 500);
		exceptionList[ AU_Ref( 1635083896, 1347236723, 1448301600) ] = ValidState( kVersionsLTorEQ_Invalid, "Contact the manufacturer for an updated version of this Audio Unit", 500);
		exceptionList[ AU_Ref( 1635083896, 1347244659, 1448301600) ] = ValidState( kVersionsLTorEQ_Invalid, "Contact the manufacturer for an updated version of this Audio Unit", 500);
		exceptionList[ AU_Ref( 1635083896, 1347834733, 1448301600) ] = ValidState( kVersionsLTorEQ_Invalid, "Contact the manufacturer for an updated version of this Audio Unit", 500);
		exceptionList[ AU_Ref( 1635083896, 1347834739, 1448301600) ] = ValidState( kVersionsLTorEQ_Invalid, "Contact the manufacturer for an updated version of this Audio Unit", 500);
		exceptionList[ AU_Ref( 1635085685, 1345335667, 1448301600) ] = ValidState( kVersionsLTorEQ_Invalid, "Contact the manufacturer for an updated version of this Audio Unit", 500);


		// Unity
		exceptionList[ AU_Ref( 1635085685, 1433302137, 1114207304) ] = ValidState( kVersionsLTorEQ_Invalid, "Contact the manufacturer for an updated version of this Audio Unit", 65536);

		// Rumblence
		exceptionList[ AU_Ref( 1635083896, 1383422001, 1430808152) ] = ValidState( kVersionsLTorEQ_Invalid, "Contact the manufacturer for an updated version of this Audio Unit", 65536);

		// Apple's DLSMusicDevice
		exceptionList[ AU_Ref( 1635085685, 1684828960, 1634758764) ] = ValidState( kVersionsLTorEQ_Invalid, "Contact the manufacturer for an updated version of this Audio Unit", 65536);
#endif
	}
	return exceptionList;
}

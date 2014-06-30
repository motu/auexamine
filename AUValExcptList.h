//
// Copyright (c) 2013 MOTU, Inc. All rights reserved.
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file.
//

#ifndef _AUVAL_EXCEPTIONLIST_H_
#define _AUVAL_EXCEPTIONLIST_H_
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

#include <AudioUnit/AudioUnit.h>
#include <string>
struct ComponentDescription;

bool IsBlackListed( const AudioComponentDescription& cd, const std::string& name, bool& masDuplicate );
bool IsWhiteListed(  const AudioComponentDescription& cd );

#endif // _AUVAL_EXCEPTIONLIST_H_

//
// Copyright (c) 2013 MOTU, Inc. All rights reserved.
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file.
//

#ifndef _CPPAUTORELEASEPOOL_H_
#define _CPPAUTORELEASEPOOL_H_


void initCocoa();

class CPPAutoReleasePool
{
	void* autoRelPool;
	void* exceptionTrap;
public:
	explicit CPPAutoReleasePool( int crashCode );
	~CPPAutoReleasePool();
};


#endif // _CPPAUTORELEASEPOOL_H_

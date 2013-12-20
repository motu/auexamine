//
// Copyright (c) 2013 MOTU, Inc. All rights reserved.
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file.
//

#ifndef _AUVALSTATUS_H_
#define _AUVALSTATUS_H_

enum AUValStatus
{
	kAUValStatusNotRunning = 0,
	kAUValStatusRunning,
	kAUValStatusCouldNotRun,
	kAUValStatusCrashed,
	kAUValStatusNotFound,
	kAUValStatusFailure,
	kAUValStatusIncompatibleVersion,
	kAUValStatusDuplicateFormat,
	kAUValStatusSuccessDoesNotRequireInit,
	kAUValStatusSuccessRequiresInit,
	kAUValStatusNotAuthorized,

	kAUValStatusLastCode // always last
};


#endif // _AUVALSTATUS_H_

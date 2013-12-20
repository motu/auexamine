//
// Copyright (c) 2013 MOTU, Inc. All rights reserved.
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file.
//

#include "CPPAutoReleasePool.h"
#import <Cocoa/Cocoa.h>
#import <ExceptionHandling/NSExceptionHandler.h>

void initCocoa()
{
	NSApplicationLoad();
}

int gCrashCode = 0;

extern "C" void
signal_callback_handler(int signum)
{
	exit( gCrashCode );
}

@interface motu_VST_Support_ExceptionHandlerDelegate : NSObject
{
}
-(motu_VST_Support_ExceptionHandlerDelegate*)init;
- (void)dealloc;
- (BOOL)shouldDisplayException:(NSException *)exception;
- (BOOL)exceptionHandler:(NSExceptionHandler *)sender shouldLogException:(NSException *)exception mask:(unsigned int)aMask;
- (BOOL)exceptionHandler:(NSExceptionHandler *)sender shouldHandleException:(NSException *)exception mask:(unsigned int)aMask;
@end

@implementation motu_VST_Support_ExceptionHandlerDelegate

-(motu_VST_Support_ExceptionHandlerDelegate*)init
{
    self = [super init];

    [[NSExceptionHandler defaultExceptionHandler] setDelegate:self];
    [[NSExceptionHandler defaultExceptionHandler] setExceptionHandlingMask: NSLogAndHandleEveryExceptionMask];

	signal(SIGINT, signal_callback_handler);
	signal(SIGABRT, signal_callback_handler);
	signal(SIGHUP, signal_callback_handler);
	signal(SIGKILL, signal_callback_handler);
	signal(SIGSEGV, signal_callback_handler);
	signal(SIGTERM, signal_callback_handler);
	signal(SIGBUS, signal_callback_handler);

    return self;
}
- (void)dealloc;
{
    [[NSExceptionHandler defaultExceptionHandler] setDelegate:nil];
    [super dealloc];
}
// used to filter out some common harmless exceptions
- (BOOL)shouldDisplayException:(NSException *)exception;
{
    return NO;
}
- (BOOL)exceptionHandler:(NSExceptionHandler *)sender shouldLogException:(NSException *)exception mask:(unsigned int)aMask;
{
    // this controls whether the exception shows up in the console, just return YES
    return YES;
}
- (BOOL)exceptionHandler:(NSExceptionHandler *)sender shouldHandleException:(NSException *)exception mask:(unsigned int)aMask;
{
	exit( gCrashCode );
    return NO;
}
@end

CPPAutoReleasePool::CPPAutoReleasePool( int crashCode ): autoRelPool(NULL)
{
	gCrashCode = crashCode;
	autoRelPool = [[NSAutoreleasePool alloc] init];
	exceptionTrap = [[motu_VST_Support_ExceptionHandlerDelegate alloc] init];
}
CPPAutoReleasePool::~CPPAutoReleasePool()
{
	[ (NSAutoreleasePool*)autoRelPool release];
	[ (motu_VST_Support_ExceptionHandlerDelegate*)exceptionTrap release];
}

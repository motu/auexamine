//
// Copyright (c) 2013 MOTU, Inc. All rights reserved.
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file.
//
#ifndef _MOTUTARGETFLAGS_H_
#define _MOTUTARGETFLAGS_H_
//
//
//
//		MotuBuildFlags
//
//
//		Flags for determining target architecture and OS
//			at compile time
//
//
//

#if defined(__GNUC__) && ( defined(__APPLE_CPP__) || defined(__APPLE_CC__) || defined(__MACOS_CLASSIC__) )

    #define MOTU_TARGET_OS_MAC						1
	#define MOTU_TARGET_RT_MAC_MACHO				1
    #define MOTU_TARGET_OS_WIN32					0

    #if defined(__ppc__)
        #define MOTU_TARGET_CPU_PPC					1
        #define MOTU_TARGET_CPU_X86					0
        #define MOTU_TARGET_CPU_X86_64				0
		#define MOTU_TARGET_RT_LITTLE_ENDIAN		0
        #define MOTU_TARGET_RT_BIG_ENDIAN			1
        #define MOTU_TARGET_RT_64_BIT				0

     #elif defined(__i386__)
        #define MOTU_TARGET_CPU_PPC					0
        #define MOTU_TARGET_CPU_X86					1
        #define MOTU_TARGET_CPU_X86_64				0
		#define MOTU_TARGET_RT_LITTLE_ENDIAN		1
        #define MOTU_TARGET_RT_BIG_ENDIAN			0
        #define MOTU_TARGET_RT_64_BIT				0

     #elif defined(__x86_64__)
        #define MOTU_TARGET_CPU_PPC					0
        #define MOTU_TARGET_CPU_X86         		0
        #define MOTU_TARGET_CPU_X86_64      		1
        #define MOTU_TARGET_RT_LITTLE_ENDIAN		1
        #define MOTU_TARGET_RT_BIG_ENDIAN   		0
        #define MOTU_TARGET_RT_64_BIT       		1
    #endif


	#if __GNUC__ >= 4
		#define MOTU_PRAGMA_STRUCT_PACK				1
		#define MOTU_PRAGMA_STRUCT_PACKPUSH			1
	#else
		#define MOTU_PRAGMA_STRUCT_PACK				0
		#define MOTU_PRAGMA_STRUCT_PACKPUSH			0
	#endif

	#if __LP64__
		#define MOTU_PRAGMA_STRUCT_ALIGN			0
	#else
		#define MOTU_PRAGMA_STRUCT_ALIGN			1
	#endif

	#define MOTU_FOUR_CHAR_CODE(x)					(x)

#elif defined(_MSC_VER)

    #define MOTU_TARGET_OS_MAC						0
    #define MOTU_TARGET_OS_WIN32					1

	#if defined(_M_X64)
		#define MOTU_TARGET_CPU_PPC					0
        #define MOTU_TARGET_CPU_X86         		0
        #define MOTU_TARGET_CPU_X86_64      		1
        #define MOTU_TARGET_RT_LITTLE_ENDIAN		1
        #define MOTU_TARGET_RT_BIG_ENDIAN   		0
        #define MOTU_TARGET_RT_64_BIT       		1

	#elif defined (_M_IX86)
        #define MOTU_TARGET_CPU_PPC					0
        #define MOTU_TARGET_CPU_X86					1
        #define MOTU_TARGET_CPU_X86_64				0
		#define MOTU_TARGET_RT_LITTLE_ENDIAN		1
        #define MOTU_TARGET_RT_BIG_ENDIAN			0
        #define MOTU_TARGET_RT_64_BIT				0
	#else
		#error Unsupported CPU architecture
    #endif

	#define MOTU_PRAGMA_STRUCT_ALIGN				0
	#define MOTU_PRAGMA_STRUCT_PACK					1
	#define MOTU_PRAGMA_STRUCT_PACKPUSH				1
	#define MOTU_FOUR_CHAR_CODE(x)					(x)

#endif

#endif // _MOTUTARGETFLAGS_H_

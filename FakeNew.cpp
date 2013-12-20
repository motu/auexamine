//
// Copyright (c) 2013 MOTU, Inc. All rights reserved.
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file.
//
////////////////////////////////////////////////////////////////////////////////
//
//	FakeNew
//
//
//	Description: Checks that plug-ins do not incorrectly mix new and delete with
//    malloc/free.
//
////////////////////////////////////////////////////////////////////////////////

#include <new>
#include <stdlib.h>
#include <stdint.h>

// If we're using microsoft's compiler, we export this from a
// .def file to avoid compiler errors, hence we don't want to use
// the export statement.
#ifdef _MSC_VER
	#undef EXPORT
	#define EXPORT
#endif

#if (__GNUC__) || (__clang__)
	#define EXPORT __attribute__((used, __visibility__("default")))
#endif


namespace std {
	extern new_handler __new_handler;
	#ifdef __GNUC__
	extern const nothrow_t nothrow;
	#elif _MSC_VER
	#else
	EXPORT nothrow_t nothrow;
	#endif
}

#define MAGIC_NUMERO	0xdeceaced
#define HEADER_SIZE		16

static void* doAllocate( std::size_t size )
{
	char* ret = reinterpret_cast<char*>(malloc( size + HEADER_SIZE ));
	if ( ret )
	{
		*((int32_t*)ret) = MAGIC_NUMERO;
		ret += HEADER_SIZE;
	}
	return ret;
}

static void doDeallocate( void* mem )
{
	if ( mem )
	{
		char* test = reinterpret_cast<char*>(mem) - HEADER_SIZE;
		if ( *((int32_t*)test) == MAGIC_NUMERO )
			free( test );
		else
			free( mem );
	}
}

EXPORT void *operator new(std::size_t size) throw(std::bad_alloc)
{
	void* result = doAllocate(size);

	if (result == NULL)
		throw std::bad_alloc();

	return result;
}

EXPORT void *operator new(std::size_t size, const std::nothrow_t&) throw()
{
	return doAllocate(size);
}

EXPORT void *operator new[](std::size_t size) throw(std::bad_alloc)
{
	void* result = doAllocate(size);

	if (result == NULL)
		throw std::bad_alloc();

	return result;
}

EXPORT void *operator new[](std::size_t size, const std::nothrow_t&) throw()
{
	return doAllocate(size);
}

EXPORT void operator delete(void *address) throw()
{
	doDeallocate( address );
}

EXPORT void operator delete[](void *address) throw()
{
	doDeallocate( address );
}

EXPORT void operator delete(void *address, const std::nothrow_t&) throw()
{
	doDeallocate( address );
}

EXPORT void operator delete[](void *address, const std::nothrow_t&) throw()
{
	doDeallocate( address );
}

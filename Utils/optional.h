//
// Copyright (c) 2013 MOTU, Inc. All rights reserved.
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file.
//
#ifndef optional_h
#define optional_h

#include <memory>

// this loosely emulates the api of boost/optional with a unique_ptr.
// unlike boost/optional, it requires an extra heap allocation.
template<typename t>
struct optional
{
public:
	optional() {}
	optional(const t& o) : m(new t(o)) {}
	optional(const optional<t>& o)
	{
		if(o.m) m.reset(new t(*o.m.get()));
	}
	optional(optional<t>&& o) : m(std::move(o.m)) {}
	optional(std::nullptr_t) {}

	const optional<t>& operator= (const t& o)
	{
		m.reset(new t(o));
		return *this;
	}
	const optional<t>& operator= (const optional<t>& o)
	{
		if(o.m)
			m.reset(new t(*o.m.get()));
		else
			m.reset();
		return *this;
	}
	const optional<t>& operator= (optional<t>&& o)
	{
		m = std::move(o.m);
		return *this;
	}
	const optional<t>& operator=(std::nullptr_t)
	{
		m.reset();
        return *this;
	}

	bool operator< (const optional<t>& o) const
	{
		if(m and o.hasValue())
			return *(*this) < *o;
		if(o.hasValue())
			return true;
		return false;
	}

	// if visual studio support this, wouldn't it be awesome!?!
	// explicit operator bool() const {return m;}

    // explicit cast is a workaround for clang bug....
	bool hasValue() const {return static_cast<bool>(m);}

	const t& operator*() const {assert(m); return *m.get();}
	t* operator->() {assert(m); return m.get();}
	const t* operator->() const {assert(m); return m.get();}
private:
	std::unique_ptr<t> m;
};

#endif
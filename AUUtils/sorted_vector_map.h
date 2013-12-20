//
// Copyright (c) 2013 MOTU, Inc. All rights reserved.
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file.
//
//
// sorted_vector_map.h
//
// This template implements the STL map interface using a sorted vector instead of a tree.
// In many cases, especially when using relatively small sets of data, it can be faster
// than the standard STL map.
//
// This reproduces the std::map interface as closely as possible, so it can be used as
// a drop-in replacement, with no other changes needed.
//
// EXCEPTION: map has the property that inserting or removing items does not
//	invalidate any existing iterators.  sorted_vector_map does not have this property, so
//	make sure you're not relying on it.
//

#ifndef sorted_vector_map_DJS061902_h__
#define sorted_vector_map_DJS061902_h__

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

template <class Key, class T,
		class Compare = std::less<Key>,
		class Allocator = std::allocator<std::pair<Key, T> > >
class sorted_vector_map
{
private:
	typedef std::vector<std::pair<Key, T>, Allocator>			container_type;

public:
	// DEFINITIONS:
	
	typedef Key										key_type;
	typedef T											mapped_type;
	typedef typename container_type::value_type				value_type;
	typedef Compare									key_compare;
	typedef typename container_type::allocator_type			allocator_type;
	typedef typename container_type::reference				reference;
	typedef typename container_type::const_reference			const_reference;
	typedef typename container_type::size_type				size_type;
	typedef typename container_type::difference_type			difference_type;
	typedef typename container_type::pointer					pointer;
	typedef typename container_type::const_pointer			const_pointer;
	typedef typename container_type::iterator				iterator;
	typedef typename container_type::const_iterator			const_iterator;
	typedef typename container_type::reverse_iterator			reverse_iterator;
	typedef typename container_type::const_reverse_iterator	const_reverse_iterator;
		
	class value_compare : public std::binary_function<value_type, value_type, bool>
	{
	public:
		bool operator() (const value_type& x, const value_type& y)
		{
			return comp (x.first, y.first);
		}
		
	protected:
		friend class sorted_vector_map;
		value_compare (Compare c) : comp (c)	{}
	
		Compare comp;
	};
	
	// CONSTRUCTORS, DESTRUCTORS, AND ASSIGNMENT:
	
	explicit sorted_vector_map (const Compare& comp = Compare(), const Allocator& = Allocator());
	template<class InputIterator>
		sorted_vector_map (InputIterator first, InputIterator last,
			   	   		const Compare& comp = Compare(), const Allocator& = Allocator());	   
	sorted_vector_map (const sorted_vector_map& rhs);
	~sorted_vector_map () {}
	sorted_vector_map& operator= (const sorted_vector_map& rhs);
	
	// ITERATOR ACCESS:
	
	iterator				begin ()					{ return fContainer.begin (); }
	const_iterator			begin () const				{ return fContainer.begin (); }
	iterator				end ()					{ return fContainer.end (); }
	const_iterator			end () const				{ return fContainer.end(); }
	reverse_iterator		rbegin ()					{ return fContainer.rbegin (); }
	const_reverse_iterator	rbegin () const				{ return fContainer.rbegin (); }
	reverse_iterator		rend ()					{ return fContainer.rend (); }
	const_reverse_iterator	rend () const				{ return fContainer.rend (); }
	
	// CAPACITY:
	
	bool					empty () const				{ return fContainer.empty (); }
	size_type				size () const				{ return fContainer.size(); }
	size_type				max_size () const			{ return fContainer.max_size(); }
	allocator_type			get_allocator () const		{ return fContainer.get_allocator(); }
	
	// MAP OPERATIONS:
	
	iterator				find (const key_type& x);
	const_iterator			find (const key_type& x) const;
	size_type				count (const key_type& x) const;
	
	iterator				lower_bound (const key_type& x);
	const_iterator			lower_bound (const key_type& x) const;
	iterator				upper_bound (const key_type& x);
	const_iterator			upper_bound (const key_type& x) const;
	
	std::pair<iterator, iterator> 			equal_range (const key_type& x);
	std::pair<const_iterator, const_iterator>	equal_range (const key_type& x) const;
	
	// ELEMENT ACCESS:
	
	mapped_type&			operator[] (const key_type& key);
	
	// MODIFYING:
	
	std::pair<iterator, bool>		insert (const value_type& value);
	iterator					insert (iterator position, const value_type& value);
	template<class InputIterator>
		void					insert (InputIterator first, InputIterator last);
				
	iterator	erase (iterator pos)					{ return fContainer.erase (pos); }
	size_type	erase (const key_type& key);
	iterator	erase (iterator first, iterator last)		{ return fContainer.erase (first, last); }
	
	void		swap (sorted_vector_map& rhs)		{ fContainer.swap (rhs.fContainer); }
	void		clear ()							{ fContainer.clear (); }
	
	// COMPARISON FUNCTIONS:
	
	key_compare		key_comp () const			{ return fComp; }
	value_compare		value_comp () const			{ return value_compare (fComp); }
	
private:
	class value_key_comp : public std::binary_function<value_type, key_type, bool>
	{
	public:
		value_key_comp (key_compare c) : fComp (c) {}
		bool operator() (const value_type& x, const key_type& y) const 
		{
			return fComp (x.first, y);
		}
		bool operator() (const key_type& x, const value_type& y) const
		{
			return fComp (x, y.first);
		}
	private:
		key_compare fComp;
	};

	container_type	fContainer;
	key_compare		fComp;
};

template <class Key, class T, class Compare, class Allocator>
inline
sorted_vector_map<Key, T, Compare, Allocator>::sorted_vector_map (const Compare& comp, const Allocator& allocator) :
	fContainer (allocator),
	fComp (comp)
{
}

template <class Key, class T, class Compare, class Allocator>
template <class InputIterator>
inline
sorted_vector_map<Key, T, Compare, Allocator>::sorted_vector_map (InputIterator first, InputIterator last,
												  	     const Compare& comp, const Allocator& allocator) :
	fContainer (first, last, allocator),
	fComp (comp)
{
	std::sort (fContainer.begin(), fContainer.end(), value_comp ());
}
	
template <class Key, class T, class Compare, class Allocator>
inline
sorted_vector_map<Key, T, Compare, Allocator>::sorted_vector_map (const sorted_vector_map& rhs) :
	fContainer (rhs.fContainer),
	fComp (rhs.fComp)
{
}

template <class Key, class T, class Compare, class Allocator>
inline sorted_vector_map<Key, T, Compare, Allocator>&
sorted_vector_map<Key, T, Compare, Allocator>::operator= (const sorted_vector_map& rhs)
{
	fContainer = rhs.fContainer;
	fComp = rhs.fComp;
	return *this;
}

template <class Key, class T, class Compare, class Allocator>
inline typename sorted_vector_map<Key, T, Compare, Allocator>::iterator
sorted_vector_map<Key, T, Compare, Allocator>::lower_bound (const key_type& key)
{
	return std::lower_bound (begin(), end(), key, value_key_comp (fComp));
}

template <class Key, class T, class Compare, class Allocator>
inline typename sorted_vector_map<Key, T, Compare, Allocator>::const_iterator
sorted_vector_map<Key, T, Compare, Allocator>::lower_bound (const key_type& key) const
{
	return std::lower_bound (begin(), end(), key, value_key_comp (fComp));
}

template <class Key, class T, class Compare, class Allocator>
inline typename sorted_vector_map<Key, T, Compare, Allocator>::iterator
sorted_vector_map<Key, T, Compare, Allocator>::upper_bound (const key_type& key)
{
	return std::upper_bound (begin(), end(), key, value_key_comp (fComp));
}

template <class Key, class T, class Compare, class Allocator>
inline typename sorted_vector_map<Key, T, Compare, Allocator>::const_iterator
sorted_vector_map<Key, T, Compare, Allocator>::upper_bound (const key_type& key) const
{
	return std::upper_bound (begin(), end(), key, value_key_comp (fComp));
}

template <class Key, class T, class Compare, class Allocator>
inline typename sorted_vector_map<Key, T, Compare, Allocator>::iterator
sorted_vector_map<Key, T, Compare, Allocator>::find (const key_type& x)
{
	iterator iter = lower_bound (x);
	return (iter == end() || fComp (x, iter->first)) ? end() : iter;
}

template <class Key, class T, class Compare, class Allocator>
inline typename sorted_vector_map<Key, T, Compare, Allocator>::const_iterator
sorted_vector_map<Key, T, Compare, Allocator>::find (const key_type& x) const
{
	const_iterator iter = lower_bound (x);
	return (iter == end() || fComp (x, iter->first)) ? end () : iter;
}

template <class Key, class T, class Compare, class Allocator>
inline std::pair<typename sorted_vector_map<Key, T, Compare, Allocator>::iterator,
			typename sorted_vector_map<Key, T, Compare, Allocator>::iterator>
sorted_vector_map<Key, T, Compare, Allocator>::equal_range (const key_type& x)
{
	return std::equal_range (begin(), end(), x, value_key_comp (fComp));
}

template <class Key, class T, class Compare, class Allocator>
inline std::pair<typename sorted_vector_map<Key, T, Compare, Allocator>::const_iterator,
			typename sorted_vector_map<Key, T, Compare, Allocator>::const_iterator>
sorted_vector_map<Key, T, Compare, Allocator>::equal_range (const key_type& x) const
{
	return std::equal_range (begin(), end(), x, value_key_comp (fComp));
}

template <class Key, class T, class Compare, class Allocator>
inline typename sorted_vector_map<Key, T, Compare, Allocator>::size_type
sorted_vector_map<Key, T, Compare, Allocator>::count (const key_type& x) const
{
	return (find (x) == end()) ? 0 : 1;
}

template <class Key, class T, class Compare, class Allocator>
inline typename sorted_vector_map<Key, T, Compare, Allocator>::mapped_type&
sorted_vector_map<Key, T, Compare, Allocator>::operator[] (const key_type& key)
{
	iterator iter = lower_bound (key);
	if ((iter == end()) || fComp (key, iter->first))
		iter = fContainer.insert (iter, value_type (key, mapped_type ()));
	
	return iter->second;
}

template <class Key, class T, class Compare, class Allocator>
std::pair<typename sorted_vector_map<Key, T, Compare, Allocator>::iterator, bool>
sorted_vector_map<Key, T, Compare, Allocator>::insert (const value_type& value)
{
	value_compare valComp = value_comp();

	iterator iter = std::lower_bound (begin(), end(), value, valComp);
	bool doInsert = (iter == end()) || valComp (value, *iter);
	
	if (doInsert)
		iter = fContainer.insert (iter, value);
	return std::make_pair (iter, doInsert);
}

template <class Key, class T, class Compare, class Allocator>
typename sorted_vector_map<Key, T, Compare, Allocator>::iterator
sorted_vector_map<Key, T, Compare, Allocator>::insert (iterator iter, const value_type& value)
{
	// The iterator is a hint: First check if it's the correct place to insert.  If not, fall back to the
	// regular insert method:
	value_compare valComp = value_comp ();
	
	if ((iter == end()) || valComp (value, *iter))
	{
		iterator before = iter;
		if ((before == begin()) || (valComp (* (--before), value)))
			return fContainer.insert (iter, value);
	}
	
	return insert (value).first;
}

template <class Key, class T, class Compare, class Allocator>
template <class InputIterator>
void
sorted_vector_map<Key, T, Compare, Allocator>::insert (InputIterator first, InputIterator last)
{
	value_compare valComp = value_comp ();
	
	for (; first != last; ++first)
	{
		iterator iter = std::lower_bound (begin(), end(), *first, valComp);
		if ((iter == end()) || valComp (*first, *iter))
			fContainer.insert (iter, *first);
	}
}

template <class Key, class T, class Compare, class Allocator>
inline typename sorted_vector_map<Key, T, Compare, Allocator>::size_type
sorted_vector_map<Key, T, Compare, Allocator>::erase (const key_type& key)
{
	iterator iter = find (key);
	if (iter != end ())
	{
		fContainer.erase (iter);
		return 1;
	}
	return 0;
}

template <class Key, class T, class Compare, class Allocator>
inline bool
operator== (const sorted_vector_map<Key, T, Compare, Allocator>& x,
		   const sorted_vector_map<Key, T, Compare, Allocator>& y)
{
	return (x.size() == y.size()) && std::equal (x.begin(), x.end(), y.begin());
}

template <class Key, class T, class Compare, class Allocator>
inline bool
operator!= (const sorted_vector_map<Key, T, Compare, Allocator>& x,
		  const sorted_vector_map<Key, T, Compare, Allocator>& y)
{
	return ! (x == y);
}

template <class Key, class T, class Compare, class Allocator>
inline bool
operator< (const sorted_vector_map<Key, T, Compare, Allocator>& x,
		 const sorted_vector_map<Key, T, Compare, Allocator>& y)
{
	return std::lexicographical_compare (x.begin(), x.end(), y.begin(), y.end ());
}

template <class Key, class T, class Compare, class Allocator>
inline bool
operator> (const sorted_vector_map<Key, T, Compare, Allocator>& x,
		 const sorted_vector_map<Key, T, Compare, Allocator>& y)
{
	return y < x;
}

template <class Key, class T, class Compare, class Allocator>
inline bool
operator<= (const sorted_vector_map<Key, T, Compare, Allocator>& x,
		   const sorted_vector_map<Key, T, Compare, Allocator>& y)
{
	return ! (y < x);
}

template <class Key, class T, class Compare, class Allocator>
inline bool
operator>= (const sorted_vector_map<Key, T, Compare, Allocator>& x,
		   const sorted_vector_map<Key, T, Compare, Allocator>& y)
{
	return ! (x < y);
}

#endif // sorted_vector_map_DJS061902_h__



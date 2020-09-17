#pragma once

// clang-format off
#include "BoostRedirect.h"
#include PLD_BOOST_INCLUDE(/optional.hpp)
// clang-format on

#include <list>
#include <map>
#include <mutex>

// shamelessly copied from boost (houdini's hboost does not include this header)
// http://www.boost.org/doc/libs/1_66_0/boost/compute/detail/lru_cache.hpp

//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

// a cache which evicts the least recently used item when it is full
template <class Key, class Value>
class lru_cache {
public:
	typedef Key key_type;
	typedef Value value_type;
	typedef std::list<key_type> list_type;
	typedef std::map<key_type, std::pair<value_type, typename list_type::iterator>> map_type;

	lru_cache(size_t capacity) : m_capacity(capacity) {}

	~lru_cache() {}

	size_t size() const {
		return m_map.size();
	}

	size_t capacity() const {
		return m_capacity;
	}

	bool empty() const {
		return m_map.empty();
	}

	bool contains(const key_type& key) {
		return m_map.find(key) != m_map.end();
	}

	void insert(const key_type& key, const value_type& value) {
		typename map_type::iterator i = m_map.find(key);
		if (i == m_map.end()) {
			// insert item into the mCache, but first check if it is full
			if (size() >= m_capacity) {
				// mCache is full, evict the least recently used item
				evict();
			}

			// insert the new item
			m_list.push_front(key);
			m_map[key] = std::make_pair(value, m_list.begin());
		}
	}

	PLD_BOOST_NS::optional<value_type> get(const key_type& key) {
		// lookup value in the mCache
		typename map_type::iterator i = m_map.find(key);
		if (i == m_map.end()) {
			// value not in mCache
			return PLD_BOOST_NS::none;
		}

		// return the value, but first update its place in the most
		// recently used list
		typename list_type::iterator j = i->second.second;
		if (j != m_list.begin()) {
			// move item to the front of the most recently used list
			m_list.erase(j);
			m_list.push_front(key);

			// update iterator in map
			j = m_list.begin();
			const value_type& value = i->second.first;
			m_map[key] = std::make_pair(value, j);

			// return the value
			return value;
		}
		else {
			// the item is already at the front of the most recently
			// used list so just return it
			return i->second.first;
		}
	}

	void clear() {
		m_map.clear();
		m_list.clear();
	}

private:
	void evict() {
		// evict item from the end of most recently used list
		typename list_type::iterator i = --m_list.end();
		m_map.erase(*i);
		m_list.erase(i);
	}

private:
	map_type m_map;
	list_type m_list;
	size_t m_capacity;
};

/*
 * Copyright 2014-2020 Esri R&D Zurich and VRBN
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#undef LOCKED_LRU_CACHE_STATS

template <typename K, typename V>
class LockedLRUCache : public lru_cache<K, V> {
private:
	using Base = lru_cache<K, V>;

	std::mutex mMutex;

#ifdef LOCKED_LRU_CACHE_STATS
	size_t hits = 0;
	size_t misses = 0;
#endif

public:
	LockedLRUCache(size_t capacity) : Base{capacity} {}

	~LockedLRUCache() {
#ifdef LOCKED_LRU_CACHE_STATS
		std::wcout << L"~LockedLRUCache: capacity = " << this->capacity() << L", size = " << this->size()
		           << L", hits = " << hits << L", misses = " << misses << std::endl;
#endif
	}

	void insert(const K& key, const V& value) {
		std::lock_guard<std::mutex> guard(mMutex);
		Base::insert(key, value);
	}

	PLD_BOOST_NS::optional<V> get(const K& key) {
		std::lock_guard<std::mutex> guard(mMutex);
		auto v = Base::get(key);

#ifdef LOCKED_LRU_CACHE_STATS
		if (v)
			hits++;
		else
			misses++;
#endif

		return v;
	}
};

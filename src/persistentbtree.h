
#pragma once

#include <algorithm>
#include <functional>
#include <utility>

#include "MemoryPage.h"

#ifdef BTREE_DEBUG

#include <iostream>

/// Assertion only if BTREE_DEBUG is defined. This is not used in verify().
#define BTREE_ASSERT(x)         do { assert(x); } while(0)

#else

/// Assertion only if BTREE_DEBUG is defined. This is not used in verify().
#define BTREE_ASSERT(x)         do { } while(0)

#endif

/// The maximum of a and b. Used in some compile-time formulas.
#define BTREE_MAX(a,b)          ((a) < (b) ? (b) : (a))

#define PAGESIZE 0x1000

/** Generates default traits for a B+ tree used as a map. It estimates leaf and
* inner node sizes by assuming a cache line size of 256 bytes. */
template <typename _Key, typename _Data>
struct btree_default_map_traits
{
	/// If true, the tree will self verify it's invariants after each insert()
	/// or erase(). The header must have been compiled with BTREE_DEBUG defined.
	static const bool   selfverify = false;

	/// If true, the tree will print out debug information and a tree dump
	/// during insert() or erase() operation. The header must have been
	/// compiled with BTREE_DEBUG defined and key_type must be std::ostream
	/// printable.
	static const bool   debug = false;

	/// Number of slots in each node of the tree. Estimated so that each node
	/// has a size of about 65536 bytes.
	static const int    maxslots = (PAGESIZE - sizeof(MemoryPage<_Key,_Data>)) / (sizeof(_Key) + BTREE_MAX(sizeof(int), sizeof(_Data)));

};

template <typename _Key, typename _Data, typename _Traits = btree_default_map_traits<_Key, _Data>>
class PersistentBTree
{
public:

	typedef _Key key_type;

	typedef _Data data_type;

	typedef std::pair<key_type, data_type> pair_type;

	typedef std::pair<key_type, data_type> value_type;

	typedef PersistentBTree < key_type, data_type > btree_self;

	typedef _Traits	traits;

public:

	// The number key/data slots in each node
	unsigned int nodeslotmax;

	// The minimum number of key/data slots used
	// in a node. If fewer slots are used, the leaf will be merged or slots
	// shifted from it's siblings.
	// minnodeslots = (nodeslotmax / 2)
	unsigned int minnodeslots;

	MemoryPageManager<_Key, _Data> m_memMgr;

private:

	class node : public MemoryNode<_Key, _Data>
	{
	public:
		node() : MemoryNode<_Key, _Data>() {}

		node(const MemoryNode<_Key, _Data>& n) : MemoryNode<_Key, _Data>(n) {
	    }

		inline void initialize(const unsigned short l)
		{
		    (*this)->level = l;
		    (*this)->slotuse = 0;
		    (*this)->isInit = true;
		}

		inline int level()
		{
			return (*this)->level;
		}

		inline int nslots()
		{
			return (*this)->nSlots;
		}

		inline bool isleafnode()
		{
			return  (level() == 0);
		}
	};

	class inner_node : public node
	{
    public:
	    inner_node() : node() {}

	    inner_node(const MemoryNode<_Key, _Data>& n) : node(n) {
        }

		key_type & key(unsigned int slot) 
		{
			return ((key_type*)this->slotsKeys + slot);
		}

	};

	class leaf_node : public node
	{
    public:
	    leaf_node() : node() {}

	    leaf_node(const MemoryNode<_Key, _Data>& n) : node(n) {
        }

		inline void initialize()
		{
			node::initialize(0);
			(*this)->prevleaf = -1;
			(*this)->nextleaf = -1;
		}

		key_type & key(unsigned int slot)
		{
			return ((key_type*)this->slotsKeys + slot);
		}

		data_type & data(unsigned int slot)
		{
			return ((data_type*)this->slotsData + slot);
		}

		bool hasprevleaf()
		{
			int nPage = this->prevLeaf;
			return nPage != -1;
		}

		bool hasnextleaf()
		{
			int nPage = this->nextLeaf;
			return nPage != -1;
		}

		/// Set the (key,data) pair in slot. Overloaded function used by
		/// bulk_load().
		inline void set_slot(unsigned int slot, const pair_type& value)
		{
			BTREE_ASSERT(slot < node::slotuse);
			((key_type*)this->slotsKeys + slot) = value.first;
			((data_type*)this->slotsData + slot) = value.second;
		}
	};

	inline bool isfull(node n) const
	{
		return (n.nslots() == nodeslotmax);
	}

	inline bool isfew(node n) const
	{
		return (n.nslots() <= minnodeslots);
	}

	inline bool isunderflow(node n) const
	{
		return (n.nslots() < minnodeslots);
	}

	node child(inner_node _node, unsigned int slot)
	{
		int nPage = _node->slotsData + slot;
		return (node) get_node(nPage);
	}

	leaf_node nextleaf(inner_node node)
	{
		int nPage = node->nextLeaf;
		return (leaf_node) get_node(nPage);
	}

	leaf_node prevleaf(inner_node node)
	{
		int nPage = node->prevLeaf;
		return (leaf_node)get_node(nPage);
	}

public:
	class iterator;
	class const_iterator;
	class reverse_iterator;
	class const_reverse_iterator;

	class iterator
	{
	public:

		typedef typename PersistentBTree::key_type	key_type;

		typedef typename PersistentBTree::data_type	data_type;

		typedef typename PersistentBTree::pair_type	pair_type;

		typedef typename PersistentBTree::value_type	value_type;

	private:

		typename PersistentBTree::leaf_node currnode;

		unsigned int currslot;

		friend class PersistentBTree < key_type, data_type > ;

		mutable value_type temp_value;

	public:

		inline iterator()
			: currnode(NULL), currslot(0)
		{}

		inline iterator(typename PersistentBTree::leaf_node l, unsigned int s)
			: currnode(l), currslot(s)
		{}

		inline value_type& operator*() const
		{
			temp_value = pair_type(key(), data());
			return temp_value;
		}

		inline value_type* operator->() const
		{
			temp_value = pair_type(key(), data());
			return &temp_value;
		}

		inline const key_type& key() const
		{
			return currnode->slotkey[currslot];
		}

		inline data_type& data() const
		{
			return currnode->data.slotdata[currslot];
		}

		inline iterator& operator++()
		{
			if (currslot + 1 < currnode->slotuse) {
				++currslot;
			}
			else if (currnode->nextleaf != NULL) {
				currnode = currnode->nextleaf;
				currslot = 0;
			}
			else {
				// this is end()
				currslot = currnode->slotuse;
			}

			return *this;
		}

		inline iterator operator++(int)
		{
			iterator tmp = *this;   // copy ourselves

			if (currslot + 1 < currnode->slotuse) {
				++currslot;
			}
			else if (currnode->nextleaf != NULL) {
				currnode = currnode->nextleaf;
				currslot = 0;
			}
			else {
				// this is end()
				currslot = currnode->slotuse;
			}

			return tmp;
		}

		inline iterator& operator--()
		{
			if (currslot > 0) {
				--currslot;
			}
			else if (currnode->prevleaf != NULL) {
				currnode = currnode->prevleaf;
				currslot = currnode->slotuse - 1;
			}
			else {
				// this is begin()
				currslot = 0;
			}

			return *this;
		}

		inline iterator operator--(int)
		{
			iterator tmp = *this;   // copy ourselves

			if (currslot > 0) {
				--currslot;
			}
			else if (currnode->prevleaf != NULL) {
				currnode = currnode->prevleaf;
				currslot = currnode->slotuse - 1;
			}
			else {
				// this is begin()
				currslot = 0;
			}

			return tmp;
		}

		inline bool operator==(const iterator& x) const
		{
			return (x.currnode == currnode) && (x.currslot == currslot);
		}

		inline bool operator!=(const iterator& x) const
		{
			return (x.currnode != currnode) || (x.currslot != currslot);
		}

	};

public:

	struct tree_stats
	{
		size_t	itemcount;

		size_t	leaves;

		size_t	innernodes;

		inline tree_stats()
			: itemcount(0),
			leaves(0), innernodes(0)
		{
		}

		inline size_t nodes() const
		{
			return innernodes + leaves;
		}
	};

private:

	int m_rootId;
	int m_headleafId;
	int m_tailleafId;

	tree_stats  m_stats;

public:

	inline PersistentBTree()
		: m_headleafId(-1), m_tailleafId(-1)
	{
		nodeslotmax = traits::maxslots;
		minnodeslots = nodeslotmax / 2;

		m_memMgr.Open("data");

		m_rootId = m_memMgr.GetRootId();
		m_headleafId = m_memMgr.GetHeadLeafId();
		m_tailleafId = m_memMgr.GetTailLeafId();
	}

	template <class InputIterator>
	inline PersistentBTree(InputIterator first, InputIterator last)
		: m_headleafId(-1), m_tailleafId(-1)
	{
		insert(first, last);

		nodeslotmax = traits::maxslots;
		minnodeslots = nodeslotmax / 2;

		m_memMgr.Open("data");

		m_rootId = m_memMgr.GetRootId();
		m_headleafId = m_memMgr.GetHeadLeafId();
		m_tailleafId = m_memMgr.GetTailLeafId();
	}

	inline ~PersistentBTree()
	{
		clear();
	}

	void setNodeSize(unsigned int _nodeslotmax = traits::nodeslots)
	{
		nodeslotmax = _nodeslotmax;
		minnodeslots = nodeslotmax / 2;
	}

public:

	class value_compare
	{
	protected:

		inline value_compare()
		{ }

		friend class PersistentBTree < key_type, data_type >;

	public:
		inline bool operator()(const value_type& x, const value_type& y) const
		{
			return x.first < y.first;
		}
	};

	inline value_compare value_comp() const
	{
		return value_compare();
	}

private:

	inline bool key_less(const key_type &a, const key_type b) const
	{
		return a < b;
	}

	inline bool key_lessequal(const key_type &a, const key_type b) const
	{
		return a <= b;
	}

	inline bool key_greater(const key_type &a, const key_type &b) const
	{
		return b < a;
	}

	inline bool key_greaterequal(const key_type &a, const key_type b) const
	{
		return b <= a;
	}

	inline bool key_equal(const key_type &a, const key_type &b) const
	{
		return !key_less(a, b) && !key_less(b, a);
	}

private:

	inline node get_node(int np)
    {
	    node n = (node) m_memMgr.GetMemoryPage(np);

	    if (n.isleafnode()) {
	        n->slotkey = (key_type*) &(((MemoryPage<int,int>*) n.getData())[1]);
	        n->data.slotdata = (data_type*) &((key_type*) n->slotkey)[nodeslotmax];
	    }
	    else {
	        n->slotkey = (key_type*) &((MemoryPage<int,int>*) n.getData())[1];
	        n->data.childid = (int*) &((key_type*) n->slotkey)[nodeslotmax];
	    }
        return n;
    }

	inline leaf_node allocate_leaf()
	{
		leaf_node n = (leaf_node) m_memMgr.InsertPage();
		n.initialize();
        n->slotkey = (key_type*) &((MemoryPage<int,int>*) n.getData())[1];
        n->data.slotdata = (data_type*) &((key_type*) n->slotkey)[nodeslotmax];
		m_stats.leaves++;
		return n;
	}

	inline inner_node allocate_inner(unsigned short level)
	{
		inner_node n = (inner_node) m_memMgr.InsertPage();
		n.initialize(level);
        n->slotkey = (key_type*) &((MemoryPage<int,int>*) n.getData())[1];
        n->data.childid = (int*) &((key_type*) n->slotkey)[nodeslotmax];
		m_stats.innernodes++;
		return n;
	}

	inline void free_node(node * n)
	{
		m_memMgr.DeletePage(n->id);
// 		if (n->isleafnode()) {
// 			leaf_node * ln = static_cast<leaf_node*>(n);
// 			free(ln->slotkey);
// 			free(ln->slotdata);
// 			delete ln;
// 			m_stats.leaves--;
// 		}
// 		else {
// 			inner_node * in = static_cast<inner_node*>(n);
// 			free(in->slotkey);
// 			free(in->childid);
// 			delete in;
// 			m_stats.innernodes--;
// 		}
	}

 	/// Convenient template function for conditional copying of slotdata. This
 	/// should be used instead of std::copy for all slotdata manipulations.
 	template<class InputIterator, class OutputIterator>
 	static OutputIterator data_copy(InputIterator first, InputIterator last,
 		OutputIterator result)
 	{
 		return std::copy(first, last, result);
 	}

 	/// Convenient template function for conditional copying of slotdata. This
 	/// should be used instead of std::copy for all slotdata manipulations.
 	template<class InputIterator, class OutputIterator>
 	static OutputIterator data_copy_backward(InputIterator first, InputIterator last,
 		OutputIterator result)
 	{
 		return std::copy_backward(first, last, result);
 	}

public:

	void clear()
	{
		m_memMgr.Close();
// 		if (m_root)
// 		{
// 			clear_recursive(m_root);
// 			free_node(m_root);
// 
// 			m_root = NULL;
// 			m_headleaf = m_tailleaf = NULL;
// 
// 			m_stats = tree_stats();
// 		}

	}

private:

// 	void clear_recursive(node * n)
// 	{
// 		if (n->isleafnode())
// 		{
// 			leaf_node * leafnode = static_cast<leaf_node*>(n);
// 
// 			for (unsigned int slot = 0; slot < leafnode->slotuse; ++slot)
// 			{
// 				// data objects are deleted by leaf_node's destructor
// 			}
// 		}
// 		else
// 		{
// 			inner_node * innernode = static_cast<inner_node*>(n);
// 
// 			for (unsigned short slot = 0; slot < innernode->slotuse + 1; ++slot)
// 			{
// 				clear_recursive(innernode->childid[slot]);
// 				free_node(innernode->childid[slot]);
// 			}
// 		}
// 	}

 public:

 	inline iterator Begin()
 	{
 		return iterator(get_node(m_headleafId), 0);
 	}

 	inline iterator End()
 	{
 		return iterator(get_node(m_tailleafId), m_tailleafId!=-1 ? m_memMgr.GetMemoryPage(m_tailleafId)->slotuse : 0);
 	}

private:

	/// Searches for the first key in the node n greater or equal to key. Uses
	/// binary search with an optional linear self-verification. This is a
	/// template function, because the slotkey array is located at different
	/// places in leaf_node and inner_node.
	template <typename node_type>
	inline int find_lower(node_type n, const key_type& key)
	{
		if (n->slotuse == 0) return 0;

		int lo = 0, hi = n->slotuse;

		while (lo < hi)
		{
			int mid = (lo + hi) >> 1;

			if (key_lessequal(key, n->slotkey[mid])) {
				hi = mid; // key <= mid
			}
			else {
				lo = mid + 1; // key > mid
			}
		}

		return lo;
	}

	/// Searches for the first key in the node n greater than key. Uses binary
	/// search with an optional linear self-verification. This is a template
	/// function, because the slotkey array is located at different places in
	/// leaf_node and inner_node.
	template <typename node_type>
	inline int find_upper(const node_type *n, const key_type& key) const
	{
		if (n->slotuse == 0) return 0;

		int lo = 0, hi = n->slotuse;

		while (lo < hi)
		{
			int mid = (lo + hi) >> 1;

			if (key_less(key, n->slotkey[mid])) {
				hi = mid; // key < mid
			}
			else {
				lo = mid + 1; // key >= mid
			}
		}

		return lo;
	}

public:

	inline size_t size() const
	{
		return m_stats.itemcount;
	}

	inline bool empty() const
	{
		return (size() == size_t(0));
	}

	inline size_t max_size() const
	{
		return size_t(-1);
	}

public:

	bool exists(const key_type &key)
	{
		node n = (node)get_node(m_rootId);
		if (!n) return false;

		while (!n.isleafnode())
		{
			const inner_node inner = static_cast<const inner_node>(n);
			int slot = find_lower(inner, key);

			n = (const node)get_node(inner->childid[slot]);
		}

		const leaf_node leaf = static_cast<const leaf_node>(n);

		unsigned int slot = find_lower(leaf, key);
		return (slot < leaf->slotuse && key_equal(key, leaf->slotkey[slot]));
	}


	iterator find(const key_type &key)
	{
		node n = (node)get_node(m_rootId);
		if (!n) return End();

		while (!n.isleafnode())
		{
			inner_node inner = static_cast<inner_node>(n);
			int slot = find_lower(inner, key);

			n = (node)get_node(inner->data.childid[slot]);
		}

		leaf_node leaf = static_cast<leaf_node>(n);

		int slot = find_lower(leaf, key);
		return (slot < leaf->slotuse && key_equal(key, leaf->slotkey[slot]))
			? iterator(leaf, slot) : End();
	}

	size_t count(const key_type &key)
	{
		node n = (node)get_node(m_rootId);
		if (!n) return 0;

		while (!n->isleafnode())
		{
			const inner_node inner = static_cast<const inner_node>(n);
			int slot = find_lower(inner, key);

			n = (node)get_node(inner->childid[slot]);
		}

		const leaf_node leaf = static_cast<const leaf_node>(n);

		int slot = find_lower(leaf, key);
		size_t num = 0;

		while (leaf && slot < leaf->slotuse && key_equal(key, leaf->slotkey[slot]))
		{
			++num;
			if (++slot >= leaf->slotuse)
			{
				leaf = (const leaf_node)get_node(leaf->nextleaf);
				slot = 0;
			}
		}

		return num;
	}

	iterator lower_bound(const key_type& key)
	{
		node n = (node)get_node(m_rootId);
		if (!n) return End();

		while (!n->isleafnode())
		{
			const inner_node inner = static_cast<const inner_node>(n);
			int slot = find_lower(inner, key);

			n = (node)get_node(inner->childid[slot]);
		}

		leaf_node leaf = static_cast<leaf_node>(n);

		int slot = find_lower(leaf, key);
		return iterator(leaf, slot);
	}


	iterator upper_bound(const key_type& key)
	{
		node n = (node)get_node(m_rootId);
		if (!n) return End();

		while (!n->isleafnode())
		{
			const inner_node inner = static_cast<const inner_node>(n);
			int slot = find_upper(inner, key);

			n = (node)get_node(inner->childid[slot]);
		}

		leaf_node leaf = static_cast<leaf_node>(n);

		int slot = find_upper(leaf, key);
		return iterator(leaf, slot);
	}

public:

//	inline bool operator==(const btree_self &other) const
//	{
//		return (size() == other.size()) && std::equal(Begin(), End(), other.Begin());
//	}

	inline bool operator!=(const btree_self &other) const
	{
		return !(*this == other);
	}

public:

// 	inline btree_self& operator= (const btree_self &other)
// 	{
// 		if (this != &other)
// 		{
// 			clear();
// 
// 			if (other.size() != 0)
// 			{
// 				m_stats.leaves = m_stats.innernodes = 0;
// 				if (other.m_root) {
// 					m_root = copy_recursive(other.m_root);
// 				}
// 				m_stats = other.m_stats;
// 			}
// 
// 		}
// 		return *this;
// 	}

	/// Copy constructor. The newly initialized B+ tree object will contain a
	/// copy of all key/data pairs.
// 	inline btree(const btree_self &other)
// 		: m_root(NULL), m_headleaf(NULL), m_tailleaf(NULL),
// 		m_stats(other.m_stats)
// 	{
// 		if (size() > 0)
// 		{
// 			m_stats.leaves = m_stats.innernodes = 0;
// 			if (other.m_root) {
// 				m_root = copy_recursive(other.m_root);
// 			}
// 		}
// 	}

private:

// 	struct node* copy_recursive(const node *n)
// 	{
// 		if (n->isleafnode())
// 		{
// 			const leaf_node * leaf = static_cast<const leaf_node*>(n);
// 			leaf_node * newleaf = allocate_leaf();
// 
// 			newleaf->slotuse = leaf->slotuse;
// 			std::copy(leaf->slotkey, leaf->slotkey + leaf->slotuse, newleaf->slotkey);
// 			data_copy(leaf->slotdata, leaf->slotdata + leaf->slotuse, newleaf->slotdata);
// 
// 			if (m_headleaf == NULL)
// 			{
// 				m_headleaf = m_tailleaf = newleaf;
// 				newleaf->prevleaf = newleaf->nextleaf = NULL;
// 			}
// 			else
// 			{
// 				newleaf->prevleaf = m_tailleaf;
// 				m_tailleaf->nextleaf = newleaf;
// 				m_tailleaf = newleaf;
// 			}
// 
// 			return newleaf;
// 		}
// 		else
// 		{
// 			const inner_node *inner = static_cast<const inner_node*>(n);
// 			inner_node *newinner = allocate_inner(inner->level);
// 
// 			newinner->slotuse = inner->slotuse;
// 			std::copy(inner->slotkey, inner->slotkey + inner->slotuse, newinner->slotkey);
// 
// 			for (unsigned short slot = 0; slot <= inner->slotuse; ++slot)
// 			{
// 				newinner->childid[slot] = copy_recursive(inner->childid[slot]);
// 			}
// 
// 			return newinner;
// 		}
// 	}

public:

	inline std::pair<iterator, bool> insert(const pair_type& x)
	{
		return insert_start(x.first, x.second);
	}

	inline std::pair<iterator, bool> insert(const key_type& key, const data_type& data)
	{
		return insert_start(key, data);
	}

private:

	std::pair<iterator, bool> insert_start(const key_type& key, const data_type& value)
	{
		node newchild;
		key_type newkey = key_type();

		if (m_rootId == -1)
		{
			node n = allocate_leaf();
			m_rootId = m_headleafId = m_tailleafId = n->id;
			m_memMgr.SetRootId(n->id);
			m_memMgr.SetHeadLeafId(n->id);
			m_memMgr.SetTailLeafId(n->id);
		}

		node root = (node) get_node(m_rootId);

		std::pair<iterator, bool> r = insert_descend(root, key, value, &newkey, &newchild);

		if (newchild)
		{
			inner_node newroot = (inner_node) allocate_inner(root.level() + 1);
			newroot->slotkey[0] = newkey;

			newroot->data.childid[0] = m_rootId;
			newroot->data.childid[1] = newchild->id;

			newroot->slotuse = 1;

			m_rootId = newroot->id;
			m_memMgr.SetRootId(newroot->id);
		}

		// increment itemcount if the item was inserted
		if (r.second) ++m_stats.itemcount;

		return r;
	}

	std::pair<iterator, bool> insert_descend(node n, const key_type& key, const data_type& value,
		key_type* splitkey, node* splitnode)
	{
		if (!n.isleafnode())
		{
			inner_node inner = static_cast<inner_node>(n);

			key_type newkey = key_type();
			node newchild;

			unsigned int slot = find_lower(inner, key);

			std::pair<iterator, bool> r = insert_descend((node)get_node(inner->data.childid[slot]),
				key, value, &newkey, &newchild);

			if (newchild)
			{
				if (isfull(inner))
				{
					split_inner_node(inner, splitkey, splitnode, slot);

					if (slot == inner->slotuse + 1 && inner->slotuse < (*splitnode)->slotuse)
					{
						// special case when the insert slot matches the split
						// place between the two nodes, then the insert key
						// becomes the split key.

						BTREE_ASSERT(inner->slotuse + 1 < innerslotmax);

						inner_node splitinner = static_cast<inner_node>(*splitnode);

						// move the split key and it's datum into the left node
						inner->slotkey[inner->slotuse] = *splitkey;
						inner->data.childid[inner->slotuse + 1] = ((inner_node) get_node(splitinner->data.childid[0]))->id;
						inner->slotuse++;

						// set new split key and move corresponding datum into right node
						splitinner->data.childid[0] = newchild->id;
						*splitkey = newkey;

						return r;
					}
					else if (slot >= inner->slotuse + 1)
					{
						// in case the insert slot is in the newly create split
						// node, we reuse the code below.

						slot -= inner->slotuse + 1;
						inner = static_cast<inner_node>(*splitnode);
					}

				}

				// move items and put pointer to child node into correct slot
				BTREE_ASSERT(slot >= 0 && slot <= inner->slotuse);

				std::copy_backward(inner->slotkey + slot, inner->slotkey + inner->slotuse,
					inner->slotkey + inner->slotuse + 1);
				std::copy_backward(inner->data.childid + slot, inner->data.childid + inner->slotuse + 1,
					inner->data.childid + inner->slotuse + 2);

				inner->slotkey[slot] = newkey;
				inner->data.childid[slot + 1] = newchild->id;
				inner->slotuse++;

			}

			return r;
		}
		else
		{
			leaf_node leaf = static_cast<leaf_node>(n);

			unsigned int slot = find_lower(leaf, key);

			// 			if (!allow_duplicates && slot < leaf->slotuse && key_equal(key, leaf->slotkey[slot])) {
			// 				return std::pair<iterator, bool>(iterator(leaf, slot), false);
			// 			}

			if (isfull(leaf))
			{
				split_leaf_node(leaf, splitkey, splitnode);

				// check if insert slot is in the split sibling node
				if (slot >= leaf->slotuse)
				{
					slot -= leaf->slotuse;
					leaf = static_cast<leaf_node>(*splitnode);
				}
			}

			// move items and put data item into correct data slot
			BTREE_ASSERT(slot >= 0 && slot <= leaf->slotuse);

			std::copy_backward(leaf->slotkey + slot, leaf->slotkey + leaf->slotuse,
				leaf->slotkey + leaf->slotuse + 1);
			data_copy_backward(leaf->data.slotdata + slot, leaf->data.slotdata + leaf->slotuse,
				leaf->data.slotdata + leaf->slotuse + 1);

			leaf->slotkey[slot] = key;
			leaf->data.slotdata[slot] = value;
			leaf->slotuse++;

			if (splitnode && leaf != *splitnode && slot == leaf->slotuse - 1)
			{
				// special case: the node was split, and the insert is at the
				// last slot of the old node. then the splitkey must be
				// updated.
				*splitkey = key;
			}

			return std::pair<iterator, bool>(iterator(leaf, slot), true);
		}
	}

	/// Split up a leaf node into two equally-filled sibling leaves. Returns
	/// the new nodes and it's insertion key in the two parameters.
	void split_leaf_node(leaf_node leaf, key_type* _newkey, node* _newleaf)
	{
		BTREE_ASSERT(isfull(leaf));

		unsigned int mid = (leaf->slotuse >> 1);

		leaf_node newleaf = allocate_leaf();

		newleaf->slotuse = leaf->slotuse - mid;

		newleaf->nextleaf = leaf->nextleaf;
		if (newleaf->nextleaf == -1) {
			BTREE_ASSERT(leaf->id == m_tailleafId);
			m_tailleafId = newleaf->id;
		}
		else {
			(get_node(newleaf->nextleaf))->prevleaf = newleaf->id;
		}

		std::copy(leaf->slotkey + mid, leaf->slotkey + leaf->slotuse,
			newleaf->slotkey);
		data_copy(leaf->data.slotdata + mid, leaf->data.slotdata + leaf->slotuse,
			newleaf->data.slotdata);

		leaf->slotuse = mid;
		leaf->nextleaf = newleaf->id;
		newleaf->prevleaf = leaf->id;

		*_newkey = leaf->slotkey[leaf->slotuse - 1];
		*_newleaf = newleaf;
	}

	/// Split up an inner node into two equally-filled sibling nodes. Returns
	/// the new nodes and it's insertion key in the two parameters. Requires
	/// the slot of the item will be inserted, so the nodes will be the same
	/// size after the insert.
	void split_inner_node(inner_node inner, key_type* _newkey, node* _newinner, unsigned int addslot)
	{
		BTREE_ASSERT(isfull(inner));

		unsigned int mid = (inner->slotuse >> 1);

		// if the split is uneven and the overflowing item will be put into the
		// larger node, then the smaller split node may underflow
		if (addslot <= mid && mid > inner->slotuse - (mid + 1))
			mid--;

		inner_node newinner = allocate_inner(inner->level);

		newinner->slotuse = inner->slotuse - (mid + 1);

		std::copy(inner->slotkey + mid + 1, inner->slotkey + inner->slotuse,
			newinner->slotkey);
		std::copy(inner->data.childid + mid + 1, inner->data.childid + inner->slotuse + 1,
			newinner->data.childid);

		inner->slotuse = mid;

		*_newkey = inner->slotkey[mid];
		*_newinner = newinner;
	}

private:

	enum result_flags_t
	{
		btree_ok = 0,
		btree_not_found = 1,
		btree_update_lastkey = 2,
		btree_fixmerge = 4
	};

	struct result_t
	{
		result_flags_t flags;

		key_type lastkey;

		inline result_t(result_flags_t f = btree_ok)
			: flags(f), lastkey()
		{}

		inline result_t(result_flags_t f, const key_type &k)
			: flags(f), lastkey(k)
		{ }

		inline bool has(result_flags_t f) const
		{
			return (flags & f) != 0;
		}

		inline result_t& operator|= (const result_t &other)
		{
			flags = result_flags_t(flags | other.flags);

			// we overwrite existing lastkeys on purpose
			if (other.has(btree_update_lastkey))
				lastkey = other.lastkey;

			return *this;
		}
	};

public:

	bool erase_one(const key_type & key)
	{
		if (m_rootId == -1) return false;

		node root = get_node(m_rootId);

		result_t result = erase_one_descend(key, root, NULL, NULL, NULL, NULL, NULL, 0);

		if (!result.has(btree_not_found))
			--m_stats.itemcount;

		return !result.has(btree_not_found);
	}

	size_t erase(const key_type &key)
	{
		size_t c = 0;

		while (erase_one(key))
		{
			++c;
		}

		return c;
	}

	void erase(iterator iter)
	{
		if (m_rootId == -1) return;

		node root = get_node(m_rootId);

		result_t result = erase_iter_descend(iter, root, NULL, NULL, NULL, NULL, NULL, 0);

		if (!result.has(btree_not_found))
			--m_stats.itemcount;

	}

private:

	/** @brief Erase one (the first) key/data pair in the B+ tree matching key.
	*
	* Descends down the tree in search of key. During the descent the parent,
	* left and right siblings and their parents are computed and passed
	* down. Once the key/data pair is found, it is removed from the leaf. If
	* the leaf underflows 6 different cases are handled. These cases resolve
	* the underflow by shifting key/data pairs from adjacent sibling nodes,
	* merging two sibling nodes or trimming the tree.
	*/
	result_t erase_one_descend(const key_type& key,
		node curr,
		node left, node right,
		inner_node leftparent, inner_node rightparent,
		inner_node parent, unsigned int parentslot)
	{
		if (curr->isleafnode())
		{
			leaf_node leaf = static_cast<leaf_node>(curr);
			leaf_node leftleaf = static_cast<leaf_node>(left);
			leaf_node rightleaf = static_cast<leaf_node>(right);

			int slot = find_lower(leaf, key);

			if (slot >= leaf->slotuse || !key_equal(key, leaf->slotkey[slot]))
			{
				return btree_not_found;
			}

			std::copy(leaf->slotkey + slot + 1, leaf->slotkey + leaf->slotuse,
				leaf->slotkey + slot);
			data_copy(leaf->slotdata + slot + 1, leaf->slotdata + leaf->slotuse,
				leaf->slotdata + slot);

			leaf->slotuse--;

			result_t myres = btree_ok;

			// if the last key of the leaf was changed, the parent is notified
			// and updates the key of this leaf
			if (slot == leaf->slotuse)
			{
				if (parent && parentslot < parent->slotuse)
				{
					BTREE_ASSERT(parent->childid[parentslot] == curr);
					parent->slotkey[parentslot] = leaf->slotkey[leaf->slotuse - 1];
				}
				else
				{
					if (leaf->slotuse >= 1)
					{
						myres |= result_t(btree_update_lastkey, leaf->slotkey[leaf->slotuse - 1]);
					}
					else
					{
						BTREE_ASSERT(leaf == m_root);
					}
				}
			}

			if (isunderflow(leaf) && !(leaf->id == m_rootId && leaf->slotuse >= 1))
			{
				// determine what to do about the underflow

				// case : if this empty leaf is the root, then delete all nodes
				// and set root to NULL.
				if (!leftleaf && !rightleaf)
				{
					BTREE_ASSERT(leaf == m_root);
					BTREE_ASSERT(leaf->slotuse == 0);

					free_node(m_rootId);

					m_rootId = -1;
					m_headleafId = m_tailleafId = -1;

					// will be decremented soon by insert_start()
					BTREE_ASSERT(m_stats.itemcount == 1);
					BTREE_ASSERT(m_stats.leaves == 0);
					BTREE_ASSERT(m_stats.innernodes == 0);

					return btree_ok;
				}
				// case : if both left and right leaves would underflow in case of
				// a shift, then merging is necessary. choose the more local merger
				// with our parent
				else if ((!leftleaf || isfew(leftleaf)) && (!rightleaf || isfew(rightleaf)))
				{
					if (leftparent == parent)
						myres |= merge_leaves(leftleaf, leaf, leftparent);
					else
						myres |= merge_leaves(leaf, rightleaf, rightparent);
				}
				// case : the right leaf has extra data, so balance right with current
				else if ((leftleaf && isfew(leftleaf)) && (rightleaf && !isfew(rightleaf)))
				{
					if (rightparent == parent)
						myres |= shift_left_leaf(leaf, rightleaf, rightparent, parentslot);
					else
						myres |= merge_leaves(leftleaf, leaf, leftparent);
				}
				// case : the left leaf has extra data, so balance left with current
				else if ((leftleaf && !isfew(leftleaf)) && (rightleaf && isfew(rightleaf)))
				{
					if (leftparent == parent)
						shift_right_leaf(leftleaf, leaf, leftparent, parentslot - 1);
					else
						myres |= merge_leaves(leaf, rightleaf, rightparent);
				}
				// case : both the leaf and right leaves have extra data and our
				// parent, choose the leaf with more data
				else if (leftparent == rightparent)
				{
					if (leftleaf->slotuse <= rightleaf->slotuse)
						myres |= shift_left_leaf(leaf, rightleaf, rightparent, parentslot);
					else
						shift_right_leaf(leftleaf, leaf, leftparent, parentslot - 1);
				}
				else
				{
					if (leftparent == parent)
						shift_right_leaf(leftleaf, leaf, leftparent, parentslot - 1);
					else
						myres |= shift_left_leaf(leaf, rightleaf, rightparent, parentslot);
				}
			}

			return myres;
		}
		else // !curr->isleafnode()
		{
			inner_node inner = static_cast<inner_node>(curr);
			inner_node leftinner = static_cast<inner_node>(left);
			inner_node rightinner = static_cast<inner_node>(right);

			node myleft, myright;
			inner_node myleftparent, myrightparent;

			int slot = find_lower(inner, key);

			if (slot == 0) {
				myleft = (left == NULL) ? NULL : (node) get_node((static_cast<inner_node>(left))->childid[left->slotuse - 1]);
				myleftparent = leftparent;
			}
			else {
				myleft = (node)get_node(inner->childid[slot - 1]);
				myleftparent = inner;
			}

			if (slot == inner->slotuse) {
				myright = (right == NULL) ? NULL : (node)get_node((static_cast<inner_node*>(right))->childid[0]);
				myrightparent = rightparent;
			}
			else {
				myright = (node)get_node(inner->childid[slot + 1]);
				myrightparent = inner;
			}

			result_t result = erase_one_descend(key,
				(node)get_node(inner->childid[slot]),
				myleft, myright,
				myleftparent, myrightparent,
				inner, slot);

			result_t myres = btree_ok;

			if (result.has(btree_not_found))
			{
				return result;
			}

			if (result.has(btree_update_lastkey))
			{
				if (parent && parentslot < parent->slotuse)
				{
					BTREE_ASSERT(parent->childid[parentslot] == curr);
					parent->slotkey[parentslot] = result.lastkey;
				}
				else
				{
					myres |= result_t(btree_update_lastkey, result.lastkey);
				}
			}

			if (result.has(btree_fixmerge))
			{
				// either the current node or the next is empty and should be removed
				if (inner->childid[slot]->slotuse != 0)
					slot++;

				// this is the child slot invalidated by the merge
				BTREE_ASSERT(inner->childid[slot]->slotuse == 0);

				free_node(inner->childid[slot]);

				std::copy(inner->slotkey + slot, inner->slotkey + inner->slotuse,
					inner->slotkey + slot - 1);
				std::copy(inner->childid + slot + 1, inner->childid + inner->slotuse + 1,
					inner->childid + slot);

				inner->slotuse--;

				if (inner->level == 1)
				{
					// fix split key for children leaves
					slot--;
					leaf_node child = (leaf_node)get_node(static_cast<leaf_node*>(inner->childid[slot]));
					inner->slotkey[slot] = child->slotkey[child->slotuse - 1];
				}
			}

			if (isunderflow(inner) && !(inner->id == m_rootId && inner->slotuse >= 1))
			{
				// case: the inner node is the root and has just one child. that child becomes the new root
				if (leftinner == NULL && rightinner == NULL)
				{
					BTREE_ASSERT(inner == m_root);
					BTREE_ASSERT(inner->slotuse == 0);

					m_rootId = inner->childid[0];

					inner->slotuse = 0;
					free_node(inner);

					return btree_ok;
				}
				// case : if both left and right leaves would underflow in case of
				// a shift, then merging is necessary. choose the more local merger
				// with our parent
				else if ((!leftinner || isfew(leftinner)) && (!rightinner || isfew(rightinner)))
				{
					if (leftparent == parent)
						myres |= merge_inner(leftinner, inner, leftparent, parentslot - 1);
					else
						myres |= merge_inner(inner, rightinner, rightparent, parentslot);
				}
				// case : the right leaf has extra data, so balance right with current
				else if ((leftinner && isfew(leftinner)) && (rightinner && !isfew(rightinner)))
				{
					if (rightparent == parent)
						shift_left_inner(inner, rightinner, rightparent, parentslot);
					else
						myres |= merge_inner(leftinner, inner, leftparent, parentslot - 1);
				}
				// case : the left leaf has extra data, so balance left with current
				else if ((leftinner && !isfew(leftinner)) && (rightinner && isfew(rightinner)))
				{
					if (leftparent == parent)
						shift_right_inner(leftinner, inner, leftparent, parentslot - 1);
					else
						myres |= merge_inner(inner, rightinner, rightparent, parentslot);
				}
				// case : both the leaf and right leaves have extra data and our
				// parent, choose the leaf with more data
				else if (leftparent == rightparent)
				{
					if (leftinner->slotuse <= rightinner->slotuse)
						shift_left_inner(inner, rightinner, rightparent, parentslot);
					else
						shift_right_inner(leftinner, inner, leftparent, parentslot - 1);
				}
				else
				{
					if (leftparent == parent)
						shift_right_inner(leftinner, inner, leftparent, parentslot - 1);
					else
						shift_left_inner(inner, rightinner, rightparent, parentslot);
				}
			}

			return myres;
		}
	}

	/** @brief Erase one key/data pair referenced by an iterator in the B+
	* tree.
	*
	* Descends down the tree in search of an iterator. During the descent the
	* parent, left and right siblings and their parents are computed and
	* passed down. The difficulty is that the iterator contains only a pointer
	* to a leaf_node, which means that this function must do a recursive depth
	* first search for that leaf node in the subtree containing all pairs of
	* the same key. This subtree can be very large, even the whole tree,
	* though in practice it would not make sense to have so many duplicate
	* keys.
	*
	* Once the referenced key/data pair is found, it is removed from the leaf
	* and the same underflow cases are handled as in erase_one_descend.
	*/
	result_t erase_iter_descend(const iterator& iter,
		node curr,
		node left, node right,
		inner_node leftparent, inner_node rightparent,
		inner_node parent, unsigned int parentslot)
	{
		if (curr->isleafnode())
		{
			leaf_node leaf = static_cast<leaf_node>(curr);
			leaf_node leftleaf = static_cast<leaf_node>(left);
			leaf_node rightleaf = static_cast<leaf_node>(right);

			// if this is not the correct leaf, get next step in recursive
			// search
			if (leaf != iter.currnode)
			{
				return btree_not_found;
			}

			if (iter.currslot >= leaf->slotuse)
			{
				return btree_not_found;
			}

			int slot = iter.currslot;

			std::copy(leaf->slotkey + slot + 1, leaf->slotkey + leaf->slotuse,
				leaf->slotkey + slot);
			data_copy(leaf->slotdata + slot + 1, leaf->slotdata + leaf->slotuse,
				leaf->slotdata + slot);

			leaf->slotuse--;

			result_t myres = btree_ok;

			// if the last key of the leaf was changed, the parent is notified
			// and updates the key of this leaf
			if (slot == leaf->slotuse)
			{
				if (parent && parentslot < parent->slotuse)
				{
					BTREE_ASSERT(parent->childid[parentslot] == curr);
					parent->slotkey[parentslot] = leaf->slotkey[leaf->slotuse - 1];
				}
				else
				{
					if (leaf->slotuse >= 1)
					{
						myres |= result_t(btree_update_lastkey, leaf->slotkey[leaf->slotuse - 1]);
					}
					else
					{
						BTREE_ASSERT(leaf == m_root);
					}
				}
			}

			if (isunderflow(leaf) && !(leaf->id == m_rootId && leaf->slotuse >= 1))
			{
				// determine what to do about the underflow

				// case : if this empty leaf is the root, then delete all nodes
				// and set root to NULL.
				if (!leftleaf && !rightleaf)
				{
					BTREE_ASSERT(leaf == m_root);
					BTREE_ASSERT(leaf->slotuse == 0);

					free_node(m_rootId);

					m_rootId = -1;
					m_headleafId = m_tailleafId = -1;

					// will be decremented soon by insert_start()
					BTREE_ASSERT(m_stats.itemcount == 1);
					BTREE_ASSERT(m_stats.leaves == 0);
					BTREE_ASSERT(m_stats.innernodes == 0);

					return btree_ok;
				}
				// case : if both left and right leaves would underflow in case of
				// a shift, then merging is necessary. choose the more local merger
				// with our parent
				else if ((!leftleaf || isfew(leftleaf)) && (!rightleaf || isfew(rightleaf)))
				{
					if (leftparent == parent)
						myres |= merge_leaves(leftleaf, leaf, leftparent);
					else
						myres |= merge_leaves(leaf, rightleaf, rightparent);
				}
				// case : the right leaf has extra data, so balance right with current
				else if ((leftleaf && isfew(leftleaf)) && (rightleaf && !isfew(rightleaf)))
				{
					if (rightparent == parent)
						myres |= shift_left_leaf(leaf, rightleaf, rightparent, parentslot);
					else
						myres |= merge_leaves(leftleaf, leaf, leftparent);
				}
				// case : the left leaf has extra data, so balance left with current
				else if ((leftleaf && !isfew(leftleaf)) && (rightleaf && isfew(rightleaf)))
				{
					if (leftparent == parent)
						shift_right_leaf(leftleaf, leaf, leftparent, parentslot - 1);
					else
						myres |= merge_leaves(leaf, rightleaf, rightparent);
				}
				// case : both the leaf and right leaves have extra data and our
				// parent, choose the leaf with more data
				else if (leftparent == rightparent)
				{
					if (leftleaf->slotuse <= rightleaf->slotuse)
						myres |= shift_left_leaf(leaf, rightleaf, rightparent, parentslot);
					else
						shift_right_leaf(leftleaf, leaf, leftparent, parentslot - 1);
				}
				else
				{
					if (leftparent == parent)
						shift_right_leaf(leftleaf, leaf, leftparent, parentslot - 1);
					else
						myres |= shift_left_leaf(leaf, rightleaf, rightparent, parentslot);
				}
			}

			return myres;
		}
		else // !curr->isleafnode()
		{
			inner_node inner = static_cast<inner_node>(curr);
			inner_node leftinner = static_cast<inner_node>(left);
			inner_node rightinner = static_cast<inner_node>(right);

			// find first slot below which the searched iterator might be
			// located.

			result_t result;
			int slot = find_lower(inner, iter.key());

			while (slot <= inner->slotuse)
			{
				node myleft, myright;
				inner_node myleftparent, myrightparent;

				if (slot == 0) {
					myleft = (!left) ? NULL : (node)get_node((static_cast<inner_node*>(left))->childid[left->slotuse - 1]);
					myleftparent = leftparent;
				}
				else {
					myleft = inner->childid[slot - 1];
					myleftparent = inner;
				}

				if (slot == inner->slotuse) {
					myright = (!right) ? NULL : (node)get_node((static_cast<inner_node*>(right))->childid[0]);
					myrightparent = rightparent;
				}
				else {
					myright = (node)get_node(inner->childid[slot + 1]);
					myrightparent = inner;
				}

				result = erase_iter_descend(iter,
					(node)get_node(inner->childid[slot]),
					myleft, myright,
					myleftparent, myrightparent,
					inner, slot);

				if (!result.has(btree_not_found))
					break;

				// continue recursive search for leaf on next slot

				if (slot < inner->slotuse && key_less(inner->slotkey[slot], iter.key()))
					return btree_not_found;

				++slot;
			}

			if (slot > inner->slotuse)
				return btree_not_found;

			result_t myres = btree_ok;

			if (result.has(btree_update_lastkey))
			{
				if (parent && parentslot < parent->slotuse)
				{
					BTREE_ASSERT(parent->childid[parentslot] == curr);
					parent->slotkey[parentslot] = result.lastkey;
				}
				else
				{
					myres |= result_t(btree_update_lastkey, result.lastkey);
				}
			}

			if (result.has(btree_fixmerge))
			{
				// either the current node or the next is empty and should be removed
				if (inner->childid[slot]->slotuse != 0)
					slot++;

				// this is the child slot invalidated by the merge
				BTREE_ASSERT(inner->childid[slot]->slotuse == 0);

				free_node(inner->childid[slot]);

				std::copy(inner->slotkey + slot, inner->slotkey + inner->slotuse,
					inner->slotkey + slot - 1);
				std::copy(inner->childid + slot + 1, inner->childid + inner->slotuse + 1,
					inner->childid + slot);

				inner->slotuse--;

				if (inner->level == 1)
				{
					// fix split key for children leaves
					slot--;
					leaf_node child = (leaf_node)get_node(static_cast<leaf_node*>(inner->childid[slot]));
					inner->slotkey[slot] = child->slotkey[child->slotuse - 1];
				}
			}

			if (isunderflow(inner) && !(inner->id == m_rootId && inner->slotuse >= 1))
			{
				// case: the inner node is the root and has just one
				// child. that child becomes the new root
				if (!leftinner && !rightinner)
				{
					BTREE_ASSERT(inner == m_root);
					BTREE_ASSERT(inner->slotuse == 0);

					m_rootId = inner->childid[0];

					inner->slotuse = 0;
					free_node(inner);

					return btree_ok;
				}
				// case : if both left and right leaves would underflow in case of
				// a shift, then merging is necessary. choose the more local merger
				// with our parent
				else if ((!leftinner || isfew(leftinner)) && (!rightinner || isfew(rightinner)))
				{
					if (leftparent == parent)
						myres |= merge_inner(leftinner, inner, leftparent, parentslot - 1);
					else
						myres |= merge_inner(inner, rightinner, rightparent, parentslot);
				}
				// case : the right leaf has extra data, so balance right with current
				else if ((leftinner && isfew(leftinner)) && (rightinner && !isfew(rightinner)))
				{
					if (rightparent == parent)
						shift_left_inner(inner, rightinner, rightparent, parentslot);
					else
						myres |= merge_inner(leftinner, inner, leftparent, parentslot - 1);
				}
				// case : the left leaf has extra data, so balance left with current
				else if ((leftinner && !isfew(leftinner)) && (rightinner && isfew(rightinner)))
				{
					if (leftparent == parent)
						shift_right_inner(leftinner, inner, leftparent, parentslot - 1);
					else
						myres |= merge_inner(inner, rightinner, rightparent, parentslot);
				}
				// case : both the leaf and right leaves have extra data and our
				// parent, choose the leaf with more data
				else if (leftparent == rightparent)
				{
					if (leftinner->slotuse <= rightinner->slotuse)
						shift_left_inner(inner, rightinner, rightparent, parentslot);
					else
						shift_right_inner(leftinner, inner, leftparent, parentslot - 1);
				}
				else
				{
					if (leftparent == parent)
						shift_right_inner(leftinner, inner, leftparent, parentslot - 1);
					else
						shift_left_inner(inner, rightinner, rightparent, parentslot);
				}
			}

			return myres;
		}
	}

	/// Merge two leaf nodes. The function moves all key/data pairs from right
	/// to left and sets right's slotuse to zero. The right slot is then
	/// removed by the calling parent node.
	result_t merge_leaves(leaf_node left, leaf_node right, inner_node parent)
	{
		(void)parent;

		BTREE_ASSERT(left->isleafnode() && right->isleafnode());
		BTREE_ASSERT(parent->level == 1);

		BTREE_ASSERT(left->slotuse + right->slotuse < leafslotmax);

		std::copy(right->slotkey, right->slotkey + right->slotuse,
			left->slotkey + left->slotuse);
		data_copy(right->slotdata, right->slotdata + right->slotuse,
			left->slotdata + left->slotuse);

		left->slotuse += right->slotuse;

		left->nextleaf = right->nextleaf;
		if (left->nextleaf)
			left->nextleaf->prevleaf = left->id;
		else
			m_tailleafId = left->id;

		right->slotuse = 0;

		return btree_fixmerge;
	}

	/// Merge two inner nodes. The function moves all key/childid pairs from
	/// right to left and sets right's slotuse to zero. The right slot is then
	/// removed by the calling parent node.
	static result_t merge_inner(inner_node left, inner_node right, inner_node parent, unsigned int parentslot)
	{
		BTREE_ASSERT(left->level == right->level);
		BTREE_ASSERT(parent->level == left->level + 1);

		BTREE_ASSERT(parent->childid[parentslot] == left);

		BTREE_ASSERT(left->slotuse + right->slotuse < innerslotmax);

		// retrieve the decision key from parent
		left->slotkey[left->slotuse] = parent->slotkey[parentslot];
		left->slotuse++;

		// copy over keys and children from right
		std::copy(right->slotkey, right->slotkey + right->slotuse,
			left->slotkey + left->slotuse);
		std::copy(right->childid, right->childid + right->slotuse + 1,
			left->childid + left->slotuse);

		left->slotuse += right->slotuse;
		right->slotuse = 0;

		return btree_fixmerge;
	}

	/// Balance two leaf nodes. The function moves key/data pairs from right to
	/// left so that both nodes are equally filled. The parent node is updated
	/// if possible.
	static result_t shift_left_leaf(leaf_node left, leaf_node right, inner_node parent, unsigned int parentslot)
	{
		BTREE_ASSERT(left->isleafnode() && right->isleafnode());
		BTREE_ASSERT(parent->level == 1);

		BTREE_ASSERT(left->nextleaf == right);
		BTREE_ASSERT(left == right->prevleaf);

		BTREE_ASSERT(left->slotuse < right->slotuse);
		BTREE_ASSERT(parent->childid[parentslot] == left);

		unsigned int shiftnum = (right->slotuse - left->slotuse) >> 1;

		BTREE_ASSERT(left->slotuse + shiftnum < leafslotmax);

		// copy the first items from the right node to the last slot in the left node.

		std::copy(right->slotkey, right->slotkey + shiftnum,
			left->slotkey + left->slotuse);
		data_copy(right->slotdata, right->slotdata + shiftnum,
			left->slotdata + left->slotuse);

		left->slotuse += shiftnum;

		// shift all slots in the right node to the left

		std::copy(right->slotkey + shiftnum, right->slotkey + right->slotuse,
			right->slotkey);
		data_copy(right->slotdata + shiftnum, right->slotdata + right->slotuse,
			right->slotdata);

		right->slotuse -= shiftnum;

		// fixup parent
		if (parentslot < parent->slotuse) {
			parent->slotkey[parentslot] = left->slotkey[left->slotuse - 1];
			return btree_ok;
		}
		else { // the update is further up the tree
			return result_t(btree_update_lastkey, left->slotkey[left->slotuse - 1]);
		}
	}

	/// Balance two inner nodes. The function moves key/data pairs from right
	/// to left so that both nodes are equally filled. The parent node is
	/// updated if possible.
	static void shift_left_inner(inner_node left, inner_node right, inner_node parent, unsigned int parentslot)
	{
		BTREE_ASSERT(left->level == right->level);
		BTREE_ASSERT(parent->level == left->level + 1);

		BTREE_ASSERT(left->slotuse < right->slotuse);
		BTREE_ASSERT(parent->childid[parentslot] == left);

		unsigned int shiftnum = (right->slotuse - left->slotuse) >> 1;

		BTREE_ASSERT(left->slotuse + shiftnum < innerslotmax);

		// copy the parent's decision slotkey and childid to the first new key on the left
		left->slotkey[left->slotuse] = parent->slotkey[parentslot];
		left->slotuse++;

		// copy the other items from the right node to the last slots in the left node.

		std::copy(right->slotkey, right->slotkey + shiftnum - 1,
			left->slotkey + left->slotuse);
		std::copy(right->childid, right->childid + shiftnum,
			left->childid + left->slotuse);

		left->slotuse += shiftnum - 1;

		// fixup parent
		parent->slotkey[parentslot] = right->slotkey[shiftnum - 1];

		// shift all slots in the right node

		std::copy(right->slotkey + shiftnum, right->slotkey + right->slotuse,
			right->slotkey);
		std::copy(right->childid + shiftnum, right->childid + right->slotuse + 1,
			right->childid);

		right->slotuse -= shiftnum;
	}

	/// Balance two leaf nodes. The function moves key/data pairs from left to
	/// right so that both nodes are equally filled. The parent node is updated
	/// if possible.
	static void shift_right_leaf(leaf_node left, leaf_node right, inner_node parent, unsigned int parentslot)
	{
		BTREE_ASSERT(left->isleafnode() && right->isleafnode());
		BTREE_ASSERT(parent->level == 1);

		BTREE_ASSERT(left->nextleaf == right);
		BTREE_ASSERT(left == right->prevleaf);
		BTREE_ASSERT(parent->childid[parentslot] == left);

		BTREE_ASSERT(left->slotuse > right->slotuse);

		unsigned int shiftnum = (left->slotuse - right->slotuse) >> 1;

		// shift all slots in the right node

		BTREE_ASSERT(right->slotuse + shiftnum < leafslotmax);

		std::copy_backward(right->slotkey, right->slotkey + right->slotuse,
			right->slotkey + right->slotuse + shiftnum);
		data_copy_backward(right->slotdata, right->slotdata + right->slotuse,
			right->slotdata + right->slotuse + shiftnum);

		right->slotuse += shiftnum;

		// copy the last items from the left node to the first slot in the right node.
		std::copy(left->slotkey + left->slotuse - shiftnum, left->slotkey + left->slotuse,
			right->slotkey);
		data_copy(left->slotdata + left->slotuse - shiftnum, left->slotdata + left->slotuse,
			right->slotdata);

		left->slotuse -= shiftnum;

		parent->slotkey[parentslot] = left->slotkey[left->slotuse - 1];
	}

	/// Balance two inner nodes. The function moves key/data pairs from left to
	/// right so that both nodes are equally filled. The parent node is updated
	/// if possible.
	static void shift_right_inner(inner_node left, inner_node right, inner_node parent, unsigned int parentslot)
	{
		BTREE_ASSERT(left->level == right->level);
		BTREE_ASSERT(parent->level == left->level + 1);

		BTREE_ASSERT(left->slotuse > right->slotuse);
		BTREE_ASSERT(parent->childid[parentslot] == left);

		unsigned int shiftnum = (left->slotuse - right->slotuse) >> 1;

		// shift all slots in the right node

		BTREE_ASSERT(right->slotuse + shiftnum < innerslotmax);

		std::copy_backward(right->slotkey, right->slotkey + right->slotuse,
			right->slotkey + right->slotuse + shiftnum);
		std::copy_backward(right->childid, right->childid + right->slotuse + 1,
			right->childid + right->slotuse + 1 + shiftnum);

		right->slotuse += shiftnum;

		// copy the parent's decision slotkey and childid to the last new key on the right
		right->slotkey[shiftnum - 1] = parent->slotkey[parentslot];

		// copy the remaining last items from the left node to the first slot in the right node.
		std::copy(left->slotkey + left->slotuse - shiftnum + 1, left->slotkey + left->slotuse,
			right->slotkey);
		std::copy(left->childid + left->slotuse - shiftnum + 1, left->childid + left->slotuse + 1,
			right->childid);

		// copy the first to-be-removed key from the left node to the parent's decision slot
		parent->slotkey[parentslot] = left->slotkey[left->slotuse - shiftnum];

		left->slotuse -= shiftnum;
	}

};

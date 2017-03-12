
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

class PersistentBTree
{
public:

	typedef DataType key_type;

	typedef DataType data_type;

	typedef std::pair<key_type, data_type> pair_type;

	typedef std::pair<key_type, data_type> value_type;

	typedef PersistentBTree btree_self;

public:

	// The number key/data slots in each node
	unsigned int nodeslotmax;

	// The minimum number of key/data slots used
	// in a node. If fewer slots are used, the leaf will be merged or slots
	// shifted from it's siblings.
	// minnodeslots = (nodeslotmax / 2)
	unsigned int minnodeslots;

	MemoryPageManager m_memMgr;

private:

	class node : public MemoryNode
	{
	public:
		node() : MemoryNode() {}

		node(const MemoryNode& n) : MemoryNode(n) {
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

	    inner_node(const MemoryNode& n) : node(n) {
        }

		key_type key(unsigned int slot)
		{
			return GetKey(slot);
		}

        void set_key(unsigned int slot, key_type key)
        {
            SetKey(slot, key);
        }

        int child(unsigned int slot)
        {
            return GetChild(slot);
        }

        void set_child(unsigned int slot, int c)
        {
            SetChild(slot, c);
        }

	};

	class leaf_node : public node
	{
    public:
	    leaf_node() : node() {}

	    leaf_node(const MemoryNode& n) : node(n) {
        }

		inline void initialize()
		{
			node::initialize(0);
			(*this)->prevleaf = -1;
			(*this)->nextleaf = -1;
		}

		key_type key(unsigned int slot)
		{
			return GetKey(slot);
		}

		data_type  data(unsigned int slot)
		{
			return GetData(slot);
		}

		void set_key(unsigned int slot, key_type key)
		{
		    SetKey(slot, key);
		}

		void set_data(unsigned int slot, data_type data)
        {
            SetData(slot, data);
        }

		bool hasprevleaf()
		{
			int nPage = (*this)->prevleaf;
			return nPage != -1;
		}

		bool hasnextleaf()
		{
			int nPage = (*this)->nextleaf;
			return nPage != -1;
		}

		/// Set the (key,data) pair in slot. Overloaded function used by
		/// bulk_load().
//		inline void set_slot(unsigned int slot, const pair_type& value)
//		{
//			BTREE_ASSERT(slot < node::slotuse);
//			((key_type*)this->slotsKeys + slot) = value.first;
//			((data_type*)this->slotsData + slot) = value.second;
//		}
	};

	inline bool isfull(node n) const
	{
		return (n.nslots() == (int) nodeslotmax);
	}

	inline bool isfew(node n) const
	{
		return (n.nslots() <= (int) minnodeslots);
	}

	inline bool isunderflow(node n) const
	{
		return ((int) n.nslots() < (int) minnodeslots);
	}

	node child(inner_node _node, unsigned int slot)
	{
		int nPage = _node.GetChild(slot);
		return (node) get_node(nPage);
	}

	void set_child(inner_node _node, unsigned int slot)
    {
        _node.SetChild(slot, slot);
    }

	leaf_node nextleaf(inner_node node)
	{
		int nPage = node->nextleaf;
		return (leaf_node) get_node(nPage);
	}

	leaf_node prevleaf(inner_node node)
	{
		int nPage = node->prevleaf;
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

		friend class PersistentBTree;

		mutable value_type temp_value;

		PersistentBTree * m_parent;

	public:

		inline iterator()
			: currslot(0), m_parent(NULL)
		{}

		inline iterator(PersistentBTree * parent, typename PersistentBTree::leaf_node l, unsigned int s)
			: currnode(l), currslot(s), m_parent(parent)
		{}

		inline value_type& operator*()
		{
			temp_value = pair_type(key(), data());
			return temp_value;
		}

		inline value_type* operator->()
		{
			temp_value = pair_type(key(), data());
			return &temp_value;
		}

		inline key_type key()
		{
			return currnode.GetKey(currslot);
		}

		inline data_type data()
		{
			return currnode.GetData(currslot);
		}

		inline iterator& operator++();

		inline iterator operator++(int);

		inline iterator& operator--();

		inline iterator operator--(int);

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
        nodeslotmax = 0;
        minnodeslots = 0;

        m_rootId = -1;
        m_headleafId = -1;
        m_tailleafId = -1;
    }

	inline PersistentBTree(std::string & name)
		: m_headleafId(-1), m_tailleafId(-1)
	{
		open(name);
	}

//	template <class InputIterator>
//	inline PersistentBTree(InputIterator first, InputIterator last)
//		: m_headleafId(-1), m_tailleafId(-1)
//	{
//		insert(first, last);
//
//		nodeslotmax = traits::maxslots;
//		minnodeslots = nodeslotmax / 2;
//
//		m_memMgr.Open("data");
//
//		m_rootId = m_memMgr.GetRootId();
//		m_headleafId = m_memMgr.GetHeadLeafId();
//		m_tailleafId = m_memMgr.GetTailLeafId();
//	}

	inline ~PersistentBTree()
	{
		clear();
	}

	void setNodeSize(unsigned int _nodeslotmax = 0)
	{
		nodeslotmax = _nodeslotmax;
		minnodeslots = nodeslotmax / 2;
	}

	void create(const std::string & name, const DataStructure & keyStruct, const DataStructure & dataStruct)
	{
        m_memMgr.Create(name, keyStruct, dataStruct);
	}

	void open(const std::string & name)
	{
	    m_memMgr.Open(name);

        nodeslotmax = m_memMgr.GetNSlots();
        minnodeslots = nodeslotmax / 2;

        m_rootId = m_memMgr.GetRootId();
        m_headleafId = m_memMgr.GetHeadLeafId();
        m_tailleafId = m_memMgr.GetTailLeafId();
	}

	bool is_open() {
	    return m_memMgr.IsOpen();
	}

public:

	DataStructure * GetKeyStructure() {
	    return m_memMgr.KeyType();
	}

	DataStructure * GetDataStructure() {
        return m_memMgr.DataType();
    }

public:

	class value_compare
	{
	protected:

		inline value_compare()
		{ }

		friend class PersistentBTree;

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

	inline bool key_less(key_type a, key_type b) const
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
	        n->slotkey = (char*) &((MemoryPage*) n.getData())[1];
	        n->data.slotdata = (char*) ((char*) n->slotkey) + m_memMgr.GetNSlots();
	    }
	    else {
	        n->slotkey = (char*) &((MemoryPage*) n.getData())[1];
	        n->data.childid = (int*) ((char*) n->slotkey) + m_memMgr.GetNSlots();
	    }
        return n;
    }

	inline leaf_node allocate_leaf()
	{
		leaf_node n = (leaf_node) m_memMgr.InsertPage();
		n.initialize();
        n->slotkey = (char*) &((MemoryPage*) n.getData())[1];
        n->data.slotdata = (char*) ((char*) n->slotkey) + m_memMgr.GetNSlots();
		m_stats.leaves++;
		return n;
	}

	inline inner_node allocate_inner(unsigned short level)
	{
		inner_node n = (inner_node) m_memMgr.InsertPage();
		n.initialize(level);
        n->slotkey = (char*) &((MemoryPage*) n.getData())[1];
        n->data.childid = (int*) ((char*) n->slotkey) + m_memMgr.GetNSlots();
		m_stats.innernodes++;
		return n;
	}

	inline void free_node(node n)
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

    inline void free_node(int n)
    {
        m_memMgr.DeletePage(n);
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
 		return iterator(this, get_node(m_headleafId), 0);
 	}

 	inline iterator End()
 	{
 		return iterator(this, get_node(m_tailleafId), m_tailleafId!=-1 ? m_memMgr.GetMemoryPage(m_tailleafId)->slotuse : 0);
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

			if (key_lessequal(key, n.key(mid))) {
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
	inline int find_upper(node n, key_type& key) const
	{
		if (n->slotuse == 0) return 0;

		int lo = 0, hi = n->slotuse;

		while (lo < hi)
		{
			int mid = (lo + hi) >> 1;

			if (key_less(key, n.GetKey(mid))) {
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

			n = (const node)get_node(child(inner, slot));
		}

		leaf_node leaf = static_cast<const leaf_node>(n);

		unsigned int slot = find_lower(leaf, key);
		return (slot < leaf->slotuse && key_equal(key, leaf.key(slot)));
	}


	iterator find(key_type &key)
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
		return (slot < leaf->slotuse && key_equal(key, leaf.key(slot)))
			? iterator(this, leaf, slot) : End();
	}

	size_t count(key_type &key)
	{
		node n = (node)get_node(m_rootId);
		if (!n) return 0;

		while (!n.isleafnode())
		{
			const inner_node inner = static_cast<const inner_node>(n);
			int slot = find_lower(inner, key);

			n = (node)get_node(child(inner, slot));
		}

		leaf_node leaf = static_cast<const leaf_node>(n);

		int slot = find_lower(leaf, key);
		size_t num = 0;

		while (leaf && slot < leaf->slotuse && key_equal(key, leaf.key(slot)))
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

	iterator lower_bound(key_type& key)
	{
		node n = (node)get_node(m_rootId);
		if (!n) return End();

		while (!n.isleafnode())
		{
			inner_node inner = static_cast<const inner_node>(n);
			int slot = find_lower(inner, key);

			n = (node)get_node(inner.child(slot));
		}

		leaf_node leaf = static_cast<leaf_node>(n);

		int slot = find_lower(leaf, key);
		return iterator(this, leaf, slot);
	}


	iterator upper_bound(key_type& key)
	{
		node n = (node)get_node(m_rootId);
		if (!n) return End();

		while (!n.isleafnode())
		{
			const inner_node inner = static_cast<const inner_node>(n);
			int slot = find_upper(inner, key);

			n = (node)get_node(child(inner, slot));
		}

		leaf_node leaf = static_cast<leaf_node>(n);

		int slot = find_upper(leaf, key);
		return iterator(this, leaf, slot);
	}

public:

//	inline bool operator==(const btree_self &other) const
//	{
//		return (size() == other.size()) && std::equal(Begin(), End(), other.Begin());
//	}

//	inline bool operator!=(const btree_self &other) const
//	{
//		return !(*this == other);
//	}

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

		std::pair<iterator, bool> r = insert_descend(root, key, value, newkey, newchild);

		if (newchild)
		{
			inner_node newroot = (inner_node) allocate_inner(root.level() + 1);
			newroot.set_key(0, newkey);

			newroot.set_child(0, m_rootId);
			newroot.set_child(1, newchild->id);

			newroot->slotuse = 1;

			m_rootId = newroot->id;
			m_memMgr.SetRootId(newroot->id);
		}

		// increment itemcount if the item was inserted
		if (r.second) ++m_stats.itemcount;

		return r;
	}

	std::pair<iterator, bool> insert_descend(node n, const key_type& key, const data_type& value,
		key_type& splitkey, node& splitnode)
	{
		if (!n.isleafnode())
		{
			inner_node inner = static_cast<inner_node>(n);

			key_type newkey = key_type();
			node newchild;

			unsigned int slot = find_lower(inner, key);

			std::pair<iterator, bool> r = insert_descend((node)get_node(inner.child(slot)),
				key, value, newkey, newchild);

			if (newchild)
			{
				if (isfull(inner))
				{
					split_inner_node(inner, splitkey, splitnode, slot);

					if (slot == inner->slotuse + 1 && inner->slotuse < (splitnode)->slotuse)
					{
						// special case when the insert slot matches the split
						// place between the two nodes, then the insert key
						// becomes the split key.

						BTREE_ASSERT(inner->slotuse + 1 < innerslotmax);

						inner_node splitinner = static_cast<inner_node>(splitnode);

						// move the split key and it's datum into the left node
						inner.set_key(inner->slotuse, splitkey);
						inner.set_child(inner->slotuse + 1, ((inner_node) get_node(child(splitinner, 0)))->id);
						inner->slotuse++;

						// set new split key and move corresponding datum into right node
						splitinner->data.childid[0] = newchild->id;
						splitkey = newkey;

						return r;
					}
					else if (slot >= (int)inner->slotuse + 1)
					{
						// in case the insert slot is in the newly create split
						// node, we reuse the code below.

						slot -= inner->slotuse + 1;
						inner = static_cast<inner_node>(splitnode);
					}

				}

				// move items and put pointer to child node into correct slot
				BTREE_ASSERT(slot >= 0 && slot <= inner->slotuse);

				copy_inner_keys(inner, inner, slot, inner->slotuse, inner->slotuse + 1);
				copy_inner_childs(inner, inner, slot, inner->slotuse + 1, inner->slotuse + 2);

				inner.set_key(slot, newkey);
				inner.set_child(slot + 1, newchild->id);
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
				if (slot >= (int) leaf->slotuse)
				{
					slot -= leaf->slotuse;
					leaf = static_cast<leaf_node>(splitnode);
				}
			}

			// move items and put data item into correct data slot
			BTREE_ASSERT(slot >= 0 && slot <= leaf->slotuse);

			copy_leaf_keys(leaf, leaf, slot, leaf->slotuse, leaf->slotuse + 1);
			copy_leaf_data(leaf, leaf, slot, leaf->slotuse, leaf->slotuse + 1);

			leaf.set_key(slot, key);
			leaf.set_data(slot, value);
			leaf->slotuse++;

			if (splitnode && leaf != splitnode && slot == (int)leaf->slotuse - 1)
			{
				// special case: the node was split, and the insert is at the
				// last slot of the old node. then the splitkey must be
				// updated.
				splitkey = key;
			}

			return std::pair<iterator, bool>(iterator(this, leaf, slot), true);
		}
	}

	/// Split up a leaf node into two equally-filled sibling leaves. Returns
	/// the new nodes and it's insertion key in the two parameters.
	void split_leaf_node(leaf_node leaf, key_type& _newkey, node& _newleaf)
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

		copy_leaf_keys(leaf, newleaf, mid, leaf->slotuse, 0);
		copy_leaf_keys(leaf, newleaf, mid, leaf->slotuse, 0);

		leaf->slotuse = mid;
		leaf->nextleaf = newleaf->id;
		newleaf->prevleaf = leaf->id;

		_newkey = leaf.key(leaf->slotuse - 1);
		_newleaf = newleaf;
	}

	/// Split up an inner node into two equally-filled sibling nodes. Returns
	/// the new nodes and it's insertion key in the two parameters. Requires
	/// the slot of the item will be inserted, so the nodes will be the same
	/// size after the insert.
	void split_inner_node(inner_node inner, key_type& _newkey, node& _newinner, unsigned int addslot)
	{
		BTREE_ASSERT(isfull(inner));

		unsigned int mid = (inner->slotuse >> 1);

		// if the split is uneven and the overflowing item will be put into the
		// larger node, then the smaller split node may underflow
		if (addslot <= mid && mid > inner->slotuse - (mid + 1))
			mid--;

		inner_node newinner = allocate_inner(inner->level);

		newinner->slotuse = inner->slotuse - (mid + 1);

		copy_inner_keys(inner, newinner, mid + 1, inner->slotuse, 0);
		copy_inner_childs(inner, newinner, mid + 1, inner->slotuse + 1, 0);

		inner->slotuse = mid;

		_newkey = inner.key(mid);
		_newinner = newinner;
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

		result_t result = erase_one_descend(key, root, node(), node(), node(), node(), node(), 0);

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

		result_t result = erase_iter_descend(iter, root, node(), node(), node(), node(), node(), 0);

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
		if (curr.isleafnode())
		{
			leaf_node leaf = static_cast<leaf_node>(curr);
			leaf_node leftleaf = static_cast<leaf_node>(left);
			leaf_node rightleaf = static_cast<leaf_node>(right);

			int slot = find_lower(leaf, key);

			if (slot >= leaf->slotuse || !key_equal(key, leaf.key(slot)))
			{
				return btree_not_found;
			}

			copy_leaf_keys(leaf, leaf, slot + 1, leaf->slotuse, slot);
			copy_leaf_data(leaf, leaf, slot + 1, leaf->slotuse, slot);

			leaf->slotuse--;

			result_t myres = btree_ok;

			// if the last key of the leaf was changed, the parent is notified
			// and updates the key of this leaf
			if (slot == leaf->slotuse)
			{
				if (parent && parentslot < (int)parent->slotuse)
				{
					BTREE_ASSERT(parent->childid[parentslot] == curr);
					parent.set_key(parentslot, leaf.key(leaf->slotuse - 1));
				}
				else
				{
					if (leaf->slotuse >= 1)
					{
						myres |= result_t(btree_update_lastkey, leaf.key(leaf->slotuse - 1));
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
				myleft = (!left) ? node() : (node) get_node((static_cast<inner_node>(left)).child(left->slotuse - 1));
				myleftparent = leftparent;
			}
			else {
				myleft = (node)get_node(inner.child(slot - 1));
				myleftparent = inner;
			}

			if (slot == inner->slotuse) {
				myright = (!right) ? node() : (node)get_node((static_cast<inner_node>(right)).child(0));
				myrightparent = rightparent;
			}
			else {
				myright = (node)get_node(inner.child(slot + 1));
				myrightparent = inner;
			}

			result_t result = erase_one_descend(key,
				(node)get_node(inner.child(slot)),
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
					parent.set_key(parentslot, result.lastkey);
				}
				else
				{
					myres |= result_t(btree_update_lastkey, result.lastkey);
				}
			}

			if (result.has(btree_fixmerge))
			{
				// either the current node or the next is empty and should be removed
				if (get_node(inner.child(slot))->slotuse != 0)
					slot++;

				// this is the child slot invalidated by the merge
				BTREE_ASSERT(inner->childid[slot]->slotuse == 0);

				free_node(inner.child(slot));

				copy_inner_keys(inner, inner, slot, inner->slotuse, slot - 1);
				copy_inner_childs(inner, inner, slot+1, inner->slotuse+1, slot);

				inner->slotuse--;

				if (inner->level == 1)
				{
					// fix split key for children leaves
					slot--;
					leaf_node child = (leaf_node)get_node(inner.child(slot));
					inner.set_key(slot, child.key(child->slotuse - 1));
				}
			}

			if (isunderflow(inner) && !(inner->id == m_rootId && inner->slotuse >= 1))
			{
				// case: the inner node is the root and has just one child. that child becomes the new root
				if (leftinner && rightinner)
				{
					BTREE_ASSERT(inner == m_root);
					BTREE_ASSERT(inner->slotuse == 0);

					m_rootId = inner.child(0);

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
	result_t erase_iter_descend(iterator& iter,
		node curr,
		node left, node right,
		inner_node leftparent, inner_node rightparent,
		inner_node parent, unsigned int parentslot)
	{
		if (curr.isleafnode())
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

			if (iter.currslot >= (int) leaf->slotuse)
			{
				return btree_not_found;
			}

			int slot = iter.currslot;

			copy_leaf_keys(leaf, leaf, slot + 1, leaf->slotuse, slot);
			copy_leaf_data(leaf, leaf, slot + 1, leaf->slotuse, slot);

			leaf->slotuse--;

			result_t myres = btree_ok;

			// if the last key of the leaf was changed, the parent is notified
			// and updates the key of this leaf
			if (slot == leaf->slotuse)
			{
				if (parent && parentslot < (int) parent->slotuse)
				{
					BTREE_ASSERT(parent->childid[parentslot] == curr);
					parent.set_key(parentslot, leaf.key(leaf->slotuse - 1));
				}
				else
				{
					if (leaf->slotuse >= 1)
					{
						myres |= result_t(btree_update_lastkey, leaf.key(leaf->slotuse - 1));
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
					myleft = (!left) ? node() : (node)get_node((static_cast<inner_node>(left)).child(left->slotuse - 1));
					myleftparent = leftparent;
				}
				else {
					myleft = (node)get_node(inner.child(slot - 1));
					myleftparent = inner;
				}

				if (slot == inner->slotuse) {
					myright = (!right) ? node() : (node)get_node((static_cast<inner_node>(right)).child(0));
					myrightparent = rightparent;
				}
				else {
					myright = (node)get_node(inner.child(slot + 1));
					myrightparent = inner;
				}

				result = erase_iter_descend(iter,
					(node)get_node(inner.child(slot)),
					myleft, myright,
					myleftparent, myrightparent,
					inner, slot);

				if (!result.has(btree_not_found))
					break;

				// continue recursive search for leaf on next slot

				if (slot < inner->slotuse && key_less(inner.key(slot), iter.key()))
					return btree_not_found;

				++slot;
			}

			if (slot > inner->slotuse)
				return btree_not_found;

			result_t myres = btree_ok;

			if (result.has(btree_update_lastkey))
			{
				if (parent && parentslot < (int) parent->slotuse)
				{
					BTREE_ASSERT(parent->childid[parentslot] == curr);
					parent.set_key(parentslot, result.lastkey);
				}
				else
				{
					myres |= result_t(btree_update_lastkey, result.lastkey);
				}
			}

			if (result.has(btree_fixmerge))
			{
				// either the current node or the next is empty and should be removed
				if (get_node(inner.child(slot))->slotuse != 0)
					slot++;

				// this is the child slot invalidated by the merge
				BTREE_ASSERT(inner->childid[slot]->slotuse == 0);

				free_node(inner.child(slot));

				copy_inner_keys(inner, inner, slot, inner->slotuse, slot - 1);
				copy_inner_childs(inner, inner, slot + 1, inner->slotuse + 1, slot);

				inner->slotuse--;

				if (inner->level == 1)
				{
					// fix split key for children leaves
					slot--;
					leaf_node child = (leaf_node)(get_node(inner.child(slot)));
					inner.set_key(slot, child.key(child->slotuse - 1));
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

					m_rootId = inner.child(0);

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

		copy_leaf_keys(right, left, 0, right->slotuse, left->slotuse);
		copy_leaf_data(right, left, 0, right->slotuse, left->slotuse);

		left->slotuse += right->slotuse;

		left->nextleaf = right->nextleaf;
		if (left->nextleaf)
		    (get_node(left->nextleaf))->prevleaf = left->id;
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
		left.set_key(left->slotuse, parent.key(parentslot));
		left->slotuse++;

		// copy over keys and children from right
		copy_inner_keys(right, left, 0, right->slotuse, left->slotuse);
		copy_inner_childs(right, left, 0, right->slotuse+1, left->slotuse);

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

		copy_leaf_keys(right, left, 0, shiftnum, left->slotuse);
		copy_leaf_data(right, left, 0, shiftnum, left->slotuse);

		left->slotuse += shiftnum;

		// shift all slots in the right node to the left
		copy_leaf_keys(right, right, shiftnum, right->slotuse, 0);
		copy_leaf_data(right, right, shiftnum, right->slotuse, 0);

		right->slotuse -= shiftnum;

		// fixup parent
		if (parentslot < (int) parent->slotuse) {
			parent.set_key(parentslot, left.key(left->slotuse - 1));
			return btree_ok;
		}
		else { // the update is further up the tree
			return result_t(btree_update_lastkey, left.key(left->slotuse - 1));
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
		left.set_key(left->slotuse, parent.key(parentslot));
		left->slotuse++;

		// copy the other items from the right node to the last slots in the left node.

		copy_inner_keys(right, left, 0, shiftnum - 1, left->slotuse);
		copy_inner_childs(right, left, 0, shiftnum, left->slotuse);

		left->slotuse += shiftnum - 1;

		// fixup parent
		parent.set_key(parentslot, right.key(shiftnum - 1));

		// shift all slots in the right node

		copy_inner_keys(right, right, shiftnum, right->slotuse, 0);
		copy_inner_childs(right, right, shiftnum, right->slotuse + 1, 0);

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
		BTREE_ASSERT(parent.child(parentslot) == left);

		BTREE_ASSERT(left->slotuse > right->slotuse);

		unsigned int shiftnum = (left->slotuse - right->slotuse) >> 1;

		// shift all slots in the right node

		BTREE_ASSERT(right->slotuse + shiftnum < leafslotmax);

		copy_backwards_leaf_keys(right, right, 0, right->slotuse, right->slotuse + shiftnum);
		copy_backwards_leaf_data(right, right, 0, right->slotuse, right->slotuse + shiftnum);

		right->slotuse += shiftnum;

		// copy the last items from the left node to the first slot in the right node.
		copy_leaf_keys(left, left, left->slotuse - shiftnum, left->slotuse, 0);
		copy_leaf_data(left, left, left->slotuse - shiftnum, left->slotuse, 0);

		left->slotuse -= shiftnum;

		parent.set_key(parentslot, left.key(left->slotuse - 1));
	}

	/// Balance two inner nodes. The function moves key/data pairs from left to
	/// right so that both nodes are equally filled. The parent node is updated
	/// if possible.
	static void shift_right_inner(inner_node left, inner_node right, inner_node parent, unsigned int parentslot)
	{
		BTREE_ASSERT(left->level == right->level);
		BTREE_ASSERT(parent->level == left->level + 1);

		BTREE_ASSERT(left->slotuse > right->slotuse);
		BTREE_ASSERT(parent.child(parentslot) == left);

		unsigned int shiftnum = (left->slotuse - right->slotuse) >> 1;

		// shift all slots in the right node

		BTREE_ASSERT(right->slotuse + shiftnum < innerslotmax);

		copy_backwards_inner_keys(right, right, 0, right->slotuse, right->slotuse + shiftnum);
		copy_backwards_inner_childs(right, right, 0, right->slotuse + 1, right->slotuse + 1 + shiftnum);

		right->slotuse += shiftnum;

		// copy the parent's decision slotkey and childid to the last new key on the right
		right.set_key(shiftnum - 1, parent.key(parentslot));

		// copy the remaining last items from the left node to the first slot in the right node.
		copy_inner_keys(left, left, left->slotuse - shiftnum + 1,  + left->slotuse, 0);
		copy_inner_childs(left, left, left->slotuse - shiftnum + 1,  + left->slotuse+1, 0);

		// copy the first to-be-removed key from the left node to the parent's decision slot
		parent.set_key(parentslot, left.key(left->slotuse - shiftnum));

		left->slotuse -= shiftnum;
	}

	static void copy_inner_keys(inner_node node_from, inner_node node_to, int l, int r, int to)
	{
	    for (int i=l; i<r; i++) {
	        node_to.set_key(to+i-l, node_from.key(i));
	    }
	}

	static void copy_inner_childs(inner_node node_from, inner_node node_to, int l, int r, int to)
    {
        for (int i=l; i<r; i++) {
            node_to.set_child(to+i-l, node_from.child(i));
        }
    }

	static void copy_backwards_inner_keys(inner_node node_from, inner_node node_to, int l, int r, int to)
    {
        for (int i=r-1; i>=l; i--) {
            node_to.set_key(to-(r-i), node_from.key(i));
        }
    }

	static void copy_backwards_inner_childs(inner_node node_from, inner_node node_to, int l, int r, int to)
    {
        for (int i=r-1; i>=l; i--) {
            node_to.set_child(to-(r-i), node_from.child(i));
        }
    }

	static void copy_leaf_keys(leaf_node node_from, leaf_node node_to, int l, int r, int to)
    {
        for (int i=l; i<r; i++) {
            node_to.set_key(to+i-l, node_from.key(i));
        }
    }

	static void copy_leaf_data(leaf_node node_from, leaf_node node_to, int l, int r, int to)
    {
        for (int i=l; i<r; i++) {
            node_to.set_data(to+i-l, node_from.data(i));
        }
    }

	static void copy_backwards_leaf_keys(leaf_node node_from, leaf_node node_to, int l, int r, int to)
    {
        for (int i=r-1; i>=l; i--) {
            node_to.set_key(to-(r-i), node_from.key(i));
        }
    }

	static void copy_backwards_leaf_data(leaf_node node_from, leaf_node node_to, int l, int r, int to)
    {
        for (int i=r-1; i>=l; i--) {
            node_to.set_data(to-(r-i), node_from.data(i));
        }
    }

};

inline PersistentBTree::iterator & PersistentBTree::iterator::operator++()
{
    if (currslot + 1 < currnode->slotuse) {
        ++currslot;
    }
    else if (currnode->nextleaf != -1) {
        currnode = m_parent->get_node(currnode->nextleaf);
        currslot = 0;
    }
    else {
        // this is end()
        currslot = currnode->slotuse;
    }

    return *this;
}

inline PersistentBTree::iterator PersistentBTree::iterator::operator++(int)
{
    iterator tmp = *this;   // copy ourselves

    if (currslot + 1 < currnode->slotuse) {
        ++currslot;
    }
    else if (currnode->nextleaf != -1) {
        currnode = m_parent->get_node(currnode->nextleaf);
        currslot = 0;
    }
    else {
        // this is end()
        currslot = currnode->slotuse;
    }

    return tmp;
}

inline PersistentBTree::iterator & PersistentBTree::iterator::operator--()
{
    if (currslot > 0) {
        --currslot;
    }
    else if (currnode->prevleaf != -1) {
        currnode = m_parent->get_node(currnode->prevleaf);
        currslot = currnode->slotuse - 1;
    }
    else {
        // this is begin()
        currslot = 0;
    }

    return *this;
}

inline PersistentBTree::iterator PersistentBTree::iterator::operator--(int)
{
    iterator tmp = *this;   // copy ourselves

    if (currslot > 0) {
        --currslot;
    }
    else if (currnode->prevleaf != -1) {
        currnode = m_parent->get_node(currnode->prevleaf);
        currslot = currnode->slotuse - 1;
    }
    else {
        // this is begin()
        currslot = 0;
    }

    return tmp;
}

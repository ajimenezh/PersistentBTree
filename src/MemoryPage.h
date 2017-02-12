#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <set>
#include <map>
#include <fstream>

#ifdef __unix__
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#else
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/filesystem.hpp>
#endif

#include <iostream>
#include <iterator>
#include <algorithm>

template <typename _Key, typename _Data>
struct MemoryPage {
	bool isInit;
	int id;
	int level;
	int nSlots;
	int slotuse;
	_Key * slotkey;
	union udata
	{
	    int * childid;
	    _Data * slotdata;
	} data;
	int prevleaf;
	int nextleaf;
};

struct MemoryHeader {
	bool init;
	int nPages;
	int rootPage;
	int headLeaf;
	int tailLeaf;
	int usedPages;
	int size;
};

struct mmap_params {
    int size;
    int offset;
    std::string path;
};

template <typename _Key, typename _Data>
class MemoryPageManager;

template <typename _Key, typename _Data>
struct MemoryNodeImpl {

#ifdef __unix__
	int m_fd;
	mmap_params m_fileParams;
#else
	boost::iostreams::mapped_file m_fileMap;
	boost::iostreams::mapped_file_params m_fileParams;
#endif
	MemoryPage<_Key, _Data> * m_page;
	MemoryPageManager<_Key, _Data> * m_mgr;
    int m_count;

#ifdef __unix__
	MemoryNodeImpl(MemoryPageManager<_Key, _Data> * mgr, mmap_params & params);
#else
    MemoryNodeImpl(MemoryPageManager<_Key, _Data> * mgr, boost::iostreams::mapped_file_params & params);
#endif

	~MemoryNodeImpl();

	void AddRef() {
		m_count++;
	}
	int Release() {
		return --m_count;
	}
};

template <typename _Key, typename _Data>
class MemoryNode {

public:
	MemoryNodeImpl<_Key, _Data> * m_memNodeImpl;

public:
	MemoryNode() : m_memNodeImpl(NULL) {}

#ifdef __unix__
    MemoryNode(MemoryPageManager<_Key, _Data> * mgr, mmap_params & params) {
        m_memNodeImpl = new MemoryNodeImpl<_Key, _Data>(mgr, params);
        m_memNodeImpl->AddRef();
    }
#else
    MemoryNode(MemoryPageManager<_Key, _Data> * mgr, boost::iostreams::mapped_file_params & params) {
        m_memNodeImpl = new MemoryNodeImpl<_Key, _Data>(mgr, params);
        m_memNodeImpl->AddRef();
    }
#endif

	MemoryNode(const MemoryNode& n) : m_memNodeImpl(n.m_memNodeImpl) {
		m_memNodeImpl->AddRef();
	}

	~MemoryNode() {

		if (m_memNodeImpl != NULL && m_memNodeImpl->Release() == 0) {
			delete m_memNodeImpl;
		}
	}

	MemoryPage<_Key, _Data>& operator* () {
		return *m_memNodeImpl->m_page;
	}

	MemoryPage<_Key, _Data>& operator* () const {
	        return *m_memNodeImpl->m_page;
	    }

	MemoryPage<_Key, _Data>* operator->() {
		return m_memNodeImpl->m_page;
	}

	void * getData() {
        return (void*) m_memNodeImpl->m_page;
    }

	MemoryNode & operator=(const MemoryNode & n) {

		if (this != &n) {

			if (m_memNodeImpl != NULL && m_memNodeImpl->Release() == 0) {
				delete m_memNodeImpl;
			}

			m_memNodeImpl = n.m_memNodeImpl;
			m_memNodeImpl->AddRef();
		}
		return *this;
	}

	operator bool() const {
		return m_memNodeImpl != NULL;
	}

};

template <typename _Key, typename _Data>
class MemoryPageManager {
public:
	MemoryPageManager() : m_header(NULL), m_headerFD(-1), activePage(-1) {
	}

	~MemoryPageManager() {
	}

	bool Open(const char * file) {

		if (file != NULL) {

			m_fileName = std::string(file);
			m_headerFile = m_fileName + "_header";

			if (!FileExists( m_headerFile ))
			{
				CreateHeader( );
			}

			Init( );

			return true;
		}
		
		return false;
	}

	bool Close() {
		m_deletePages.clear();

		return true;
	}

	bool CreateHeader( ) {

		bool good = ResizeFile(m_headerFile, sizeof(MemoryHeader));

		good = good && InitHeader( );

		return good;

	}

	bool ResizeFile(const std::string & file, size_t nbytes) {

		std::ofstream outfile;

		if (FileExists(file)) {
		    outfile.open(file, std::ios::in | std::ios::out | std::ios::ate);
		}
		else {
		    outfile.open(file);
		}

		bool res = false;

		if (outfile.is_open()) {

			outfile.seekp(nbytes - 1);
			char buf[] = "\0";
			outfile.write(buf, 1);

			outfile.close();

			res = true;

		}

		return res;
	}

	bool InitHeader( ) {

		assert(FileExists( m_headerFile ));

		bool res = OpenHeaderMap( );

		if (res) {

			assert(!m_header->init);

			m_header->init = true;
			m_header->nPages = 0;
			m_header->usedPages = 0;
			m_header->rootPage = -1;
			m_header->headLeaf = -1;
			m_header->tailLeaf = -1;
			m_header->size = 0;

			CloseHeaderMap( );

		}

		return res;

	}

	bool ReadHeader( ) {

		assert(FileExists( m_headerFile ));

		bool res = OpenHeaderMap( );

		return res;

	}

	bool FileExists( const std::string & file ) {
#ifdef __unix__
        struct stat buffer;
        return (stat (file.c_str(), &buffer) == 0);
#else
        return boost::filesystem::exists(file);
#endif
	}

	bool Init( ) {
		
		bool res = ReadHeader( );

		res = res && InitUsedPages();

		return res;
	}

	bool InitUsedPages() {

		for (int i = 0; i < m_header->nPages; i++) {
			MemoryNode<_Key, _Data> page = GetMemoryPage(i);
			if (!page->isInit) {
				m_deletePages.insert(i);
			}
		}
		return true;
	}

	MemoryNode<_Key,_Data> InsertPage( ) {

		MemoryNode<_Key, _Data> page;

		int nPage = 0;

		if (m_deletePages.size() > 0) {

			nPage = *m_deletePages.begin();
			m_deletePages.erase(m_deletePages.begin());

			page = GetMemoryPage(nPage);

			page->isInit = true;

		}
		else {

			nPage = m_header->nPages;

			int siz = m_header->size + PAGE_SIZE;

			ResizeFile(m_fileName, siz);

			m_header->size = siz;
			m_header->nPages++;
			m_header->usedPages++;

			page = GetMemoryPage(nPage);

			page->isInit = true;
			page->id = nPage;
		}

		activePage = nPage;

		return page;

	}

	MemoryPage<_Key, _Data> * GetPage(int n) {
		return GetMemoryPage(n);
	}

	bool DeletePage(int n) {

		if (n < m_header->nPages && m_deletePages.find(n) == m_deletePages.end()) {
			MemoryNode<_Key, _Data> page = GetMemoryPage(n);
			page->isInit = false;
			m_deletePages.insert(n);
		}
		return true;
	}

	bool OpenHeaderMap( ) {

	    bool res = false;

#ifdef __unix__
	    int m_headerFD = open(m_headerFile.c_str(), O_RDWR|O_CREAT, (mode_t)0700);
	    m_header = (MemoryHeader *) mmap(NULL, sizeof(MemoryHeader), PROT_WRITE | PROT_READ, MAP_SHARED|MAP_POPULATE, m_headerFD, 0);

	    res = m_header != NULL;

#else
		boost::iostreams::mapped_file_params fileParams;

		fileParams.path = m_headerFile;
		fileParams.flags = boost::iostreams::mapped_file_base::readwrite;
		fileParams.length = sizeof(MemoryHeader);

		if (m_headerFileMap.is_open()) {
			m_headerFileMap.close();
		}

		m_headerFileMap.open(fileParams);

		res = m_headerFileMap.is_open();

		if (res) {
		    m_header = (MemoryHeader *) m_headerFileMap.data();
		}
#endif

		return res;

	}

    bool CloseHeaderMap( ) {

        bool res = false;

#ifdef __unix__
        munmap((void*) m_header, sizeof(MemoryHeader));
        close(m_headerFD);

        m_headerFD = -1;

#else
        if (m_headerFileMap.is_open())
            m_headerFileMap.close();
#endif

        m_header = NULL;

        return res;

    }

	MemoryNode<_Key, _Data> GetMemoryPage(int n) {

		MemoryNode<_Key, _Data> nd;

		if (n >= 0 && n < m_header->nPages && m_deletePages.find(n) == m_deletePages.end()) {

			if (m_memoryPageCache.find(n) != m_memoryPageCache.end()) {
				nd = m_memoryPageCache[n];
			}
			else {
#ifdef __unix__
			    mmap_params fileParams;

			    fileParams.size = PAGE_SIZE;
			    fileParams.offset = n*PAGE_SIZE;
			    fileParams.path = m_fileName;
#else
				boost::iostreams::mapped_file_params fileParams;

				fileParams.path = m_fileName;
				fileParams.flags = boost::iostreams::mapped_file_base::readwrite;
				fileParams.length = PAGE_SIZE;
				fileParams.offset = n*PAGE_SIZE;
#endif

				nd = MemoryNode<_Key, _Data>(this, fileParams);

				m_memoryPageCache[n] = nd;

				(*nd.m_memNodeImpl).m_count--;

			}
		}
		
		return nd;

	}

	int GetRootId( ) {
		int id = -1;
		if (m_header != NULL) {
			id = m_header->rootPage;
		}
		return id;
	}

	void SetRootId(int id) {
		if (m_header != NULL) {
			m_header->rootPage = id;
		}
	}

	int GetHeadLeafId() {
		int id = -1;
		if (m_header != NULL) {
			id = m_header->headLeaf;
		}
		return id;
	}

	void SetHeadLeafId(int id) {
		if (m_header != NULL) {
			m_header->headLeaf = id;
		}
	}

	int GetTailLeafId() {
		int id = -1;
		if (m_header != NULL) {
			id = m_header->tailLeaf;
		}
		return id;
	}

	void SetTailLeafId(int id) {
		if (m_header != NULL) {
			m_header->tailLeaf = id;
		}
	}

	void DeleteFromCache(int id) {
		assert(m_memoryPageCache.find(id) != m_memoryPageCache.end());
		m_memoryPageCache.erase(m_memoryPageCache.find(id));
	}

private:

	std::string m_fileName;
	std::string m_headerFile;
	
	MemoryHeader * m_header;

#ifdef __unix__
	const int PAGE_SIZE = 0x1000;
	int m_headerFD;
#else
	boost::iostreams::mapped_file m_headerFileMap;
	const int PAGE_SIZE = boost::iostreams::mapped_file::alignment();
#endif

	std::set<int> m_deletePages;

	int activePage;

	std::map<int, MemoryNode<_Key, _Data> > m_memoryPageCache;

};

#ifdef __unix__
template <typename _Key, typename _Data>
MemoryNodeImpl<_Key,_Data>::MemoryNodeImpl(MemoryPageManager<_Key, _Data> * mgr, mmap_params & params) : m_mgr(mgr), m_count(0) {

    m_fd = open(params.path.c_str(), O_RDWR|O_CREAT, (mode_t)0700);

    m_fileParams = params;

    m_page = (MemoryPage<_Key, _Data> *) mmap(NULL, params.size, PROT_WRITE | PROT_READ, MAP_SHARED|MAP_POPULATE, m_fd, params.offset);

}
#else
template <typename _Key, typename _Data>
MemoryNodeImpl<_Key,_Data>::MemoryNodeImpl(MemoryPageManager<_Key, _Data> * mgr, boost::iostreams::mapped_file_params & params) : m_mgr(mgr), m_count(0) {

    if (m_fileMap.is_open()) {
        m_fileMap.close();
    }

    m_fileMap.open(params);
    m_fileParams = params;

    m_page = (MemoryPage<_Key, _Data> *) m_fileMap.data();

}
#endif

#ifdef __unix__
template <typename _Key, typename _Data>
MemoryNodeImpl<_Key, _Data>::~MemoryNodeImpl() {

    m_mgr->DeleteFromCache(m_page->id);
    munmap((void *) m_page, m_fileParams.size);
    close(m_fd);

}
#else
template <typename _Key, typename _Data>
MemoryNodeImpl<_Key, _Data>::~MemoryNodeImpl() {

    m_mgr->DeleteFromCache(m_page->id);
    m_fileMap.close();

}
#endif


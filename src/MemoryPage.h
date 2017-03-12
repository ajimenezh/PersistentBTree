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
#include <math.h>

#include "data_structures.h"

struct MemoryPage {
	bool isInit;
	int id;
	int level;
	int nSlots;
	int slotuse;
	char * slotkey;
	union udata
	{
	    int * childid;
	    char * slotdata;
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
	int nKeyTypes;
	int nDataTypes;
	int key_type[64];
	size_t key_sizes[64];
	int data_type[64];
	size_t data_sizes[64];
	size_t memPageSize;
	int dataSize;
	int keySize;
	int nSlots;
};

struct mmap_params {
    int size;
    int offset;
    std::string path;
};

class MemoryPageManager;

struct MemoryNodeImpl {

#ifdef __unix__
	int m_fd;
	mmap_params m_fileParams;
#else
	boost::iostreams::mapped_file m_fileMap;
	boost::iostreams::mapped_file_params m_fileParams;
#endif
	MemoryPage * m_page;
	MemoryPageManager * m_mgr;
    int m_count;

#ifdef __unix__
	MemoryNodeImpl(MemoryPageManager * mgr, mmap_params & params);
#else
    MemoryNodeImpl(MemoryPageManager * mgr, boost::iostreams::mapped_file_params & params);
#endif

	~MemoryNodeImpl();

	void AddRef() {
		m_count++;
	}
	int Release() {
		return --m_count;
	}

	DataType GetKey(int slot);

	DataType GetData(int slot);

	int GetChild(int slot);

	void SetKey(int slot, DataType & data);

	void SetData(int slot, DataType & data);

	void SetChild(int slot, int c);
};

class MemoryNode {

public:
	MemoryNodeImpl * m_memNodeImpl;

public:
	MemoryNode() : m_memNodeImpl(NULL) {}

#ifdef __unix__
    MemoryNode(MemoryPageManager * mgr, mmap_params & params) {
        m_memNodeImpl = new MemoryNodeImpl(mgr, params);
        m_memNodeImpl->AddRef();
    }
#else
    MemoryNode(MemoryPageManager * mgr, boost::iostreams::mapped_file_params & params) {
        m_memNodeImpl = new MemoryNodeImpl(mgr, params);
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

	MemoryPage& operator* () {
		return *m_memNodeImpl->m_page;
	}

	MemoryPage& operator* () const {
	        return *m_memNodeImpl->m_page;
	    }

	MemoryPage* operator->() {
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

	DataType GetKey(int slot);

	DataType GetData(int slot);

	int GetChild(int slot);

    void SetKey(int slot, DataType & data);

    void SetData(int slot, DataType & data);

    void SetChild(int slot, int c);
};

class MemoryPageManager {
public:
	MemoryPageManager() : m_header(NULL), m_headerFD(-1), activePage(-1) {
	}

	~MemoryPageManager() {
	}

	bool Open(const std::string & name) {

        m_fileName = name;
        m_headerFile = m_fileName + "_header";

        bool res = FileExists( m_headerFile );

        res = res && Init();

        if (!res)
        {
            m_fileName = "";
            m_headerFile = "";
            m_header = NULL;
        }

		return res;
	}

	bool IsOpen() {
	    return m_header != NULL;
	}

	void Create(const std::string & name, const DataStructure & keyStruct, const DataStructure & dataStruct) {

	    Clear();

	    m_fileName = name;
	    m_headerFile = m_fileName + "_header";

	    if (CreateHeader( )) {

	        InitHeader(keyStruct, dataStruct);

	    }
	}

	bool Close() {

		Clear();

		return true;
	}

	void Clear() {
	    m_deletePages.clear();
	    m_memoryPageCache.clear();
	    m_header = NULL;
	}

	bool CreateHeader( ) {

		bool good = ResizeFile(m_headerFile, sizeof(MemoryHeader));

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

	bool InitHeader( const DataStructure & keyStruct, const DataStructure & dataStruct, int limit = 65536 ) {

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

			m_header->nKeyTypes = keyStruct.NTypes();
			m_header->nDataTypes = dataStruct.NTypes();

			m_header->dataSize = 0;
			m_header->keySize = 0;

			for (int i=0; i<m_header->nKeyTypes; i++) {
			    m_header->key_type[i] = (int) keyStruct.GetType(i);
			    m_header->key_sizes[i] = keyStruct.GetTypeSize(i);
			    m_header->keySize += keyStruct.GetTypeSize(i);
			}

			for (int i=0; i<m_header->nDataTypes; i++) {
                m_header->data_type[i] = (int) dataStruct.GetType(i);
                m_header->data_sizes[i] = dataStruct.GetTypeSize(i);
                m_header->dataSize += dataStruct.GetTypeSize(i);
            }



			m_header->memPageSize = limit;
			m_header->nSlots = (limit - sizeof(MemoryPage)) / ( m_header->keySize + std::max(m_header->dataSize, (int)sizeof(int)));

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

		if (res) {
		    m_keyType = DataStructure(m_header->nKeyTypes, &m_header->key_type[0], &m_header->key_sizes[0]);
		    m_dataType = DataStructure(m_header->nDataTypes, &m_header->data_type[0], &m_header->data_sizes[0]);
		}

		res = res && InitUsedPages();

		return res;
	}

	bool InitUsedPages() {

		for (int i = 0; i < m_header->nPages; i++) {
			MemoryNode page = GetMemoryPage(i);
			if (!page->isInit) {
				m_deletePages.insert(i);
			}
		}
		return true;
	}

	MemoryNode InsertPage( ) {

		MemoryNode page;

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

	MemoryNode GetPage(int n) {
		return GetMemoryPage(n);
	}

	bool DeletePage(int n) {

		if (n < m_header->nPages && m_deletePages.find(n) == m_deletePages.end()) {
			MemoryNode page = GetMemoryPage(n);
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

	MemoryNode GetMemoryPage(int n) {

		MemoryNode nd;

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

				nd = MemoryNode(this, fileParams);

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

	int GetNSlots() const {
	    return m_header->nSlots;
	}

	size_t KeySize() { return m_header->keySize; }

	size_t DataSize() { return m_header->dataSize; }

	DataStructure * KeyType() { return &m_keyType; }

	DataStructure * DataType() { return &m_dataType; }

private:

	std::string m_fileName;
	std::string m_headerFile;
	
	MemoryHeader * m_header;

	DataStructure m_keyType;
	DataStructure m_dataType;

#ifdef __unix__
	const int PAGE_SIZE = 0x1000;
	int m_headerFD;
#else
	boost::iostreams::mapped_file m_headerFileMap;
	const int PAGE_SIZE = boost::iostreams::mapped_file::alignment();
#endif

	std::set<int> m_deletePages;

	int activePage;

	std::map<int, MemoryNode > m_memoryPageCache;

};

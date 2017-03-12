/*
 * MemoryPage.cpp
 *
 *  Created on: Feb 26, 2017
 *      Author: alex
 */

#include "MemoryPage.h"

#ifdef __unix__
MemoryNodeImpl::MemoryNodeImpl(MemoryPageManager * mgr, mmap_params & params) : m_mgr(mgr), m_count(0) {

    m_fd = open(params.path.c_str(), O_RDWR|O_CREAT, (mode_t)0700);

    m_fileParams = params;

    m_page = (MemoryPage *) mmap(NULL, params.size, PROT_WRITE | PROT_READ, MAP_SHARED|MAP_POPULATE, m_fd, params.offset);

}
#else
MemoryNodeImpl::MemoryNodeImpl(MemoryPageManager * mgr, boost::iostreams::mapped_file_params & params) : m_mgr(mgr), m_count(0) {

    if (m_fileMap.is_open()) {
        m_fileMap.close();
    }

    m_fileMap.open(params);
    m_fileParams = params;

    m_page = (MemoryPage *) m_fileMap.data();

}
#endif

#ifdef __unix__
MemoryNodeImpl::~MemoryNodeImpl() {

    m_mgr->DeleteFromCache(m_page->id);
    munmap((void *) m_page, m_fileParams.size);
    close(m_fd);

}
#else
MemoryNodeImpl::~MemoryNodeImpl() {

    m_mgr->DeleteFromCache(m_page->id);
    m_fileMap.close();

}
#endif

inline DataType MemoryNodeImpl::GetKey(int slot) {

    char * ptr = ((char *)m_page->slotkey) + m_mgr->KeySize()*slot;
    return DataType(m_mgr->KeyType(), ptr);

}

inline DataType MemoryNodeImpl::GetData(int slot) {

    char * ptr = ((char *)m_page->data.slotdata) + m_mgr->DataSize()*slot;
    return DataType(m_mgr->DataType(), ptr);
}

inline int MemoryNodeImpl::GetChild(int slot) {

    char * ptr = ((char *)m_page->data.childid) + sizeof(int)*slot;
    return *(int*)ptr;
}

inline void MemoryNodeImpl::SetKey(int slot, DataType & data) {
    char * ptr = ((char *)m_page->data.slotdata) + m_mgr->DataSize()*slot;
    memcpy(ptr, data.Data(), m_mgr->KeySize());
}

inline void MemoryNodeImpl::SetData(int slot, DataType & data) {
    char * ptr = ((char *)m_page->data.childid) + sizeof(int)*slot;
    memcpy(ptr, data.Data(), m_mgr->DataSize());
}

inline void MemoryNodeImpl::SetChild(int slot, int c) {
    char * ptr = ((char *)m_page->data.childid) + sizeof(int)*slot;
    *(int*)ptr = c;
}

DataType MemoryNode::GetKey(int slot) {
    return m_memNodeImpl->GetKey(slot);
}

DataType MemoryNode::GetData(int slot) {
    return m_memNodeImpl->GetData(slot);
}

int MemoryNode::GetChild(int slot) {
    return m_memNodeImpl->GetChild(slot);
}

void MemoryNode::SetKey(int slot, DataType & data) {
    m_memNodeImpl->SetKey(slot, data);
}

void MemoryNode::SetData(int slot, DataType & data) {
    m_memNodeImpl->SetData(slot, data);
}

void MemoryNode::SetChild(int slot, int c) {
    m_memNodeImpl->SetChild(slot, c);
}




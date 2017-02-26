/*
 * data_structures.h
 *
 *  Created on: Feb 19, 2017
 *      Author: alex
 */

#ifndef SRC_DATA_STRUCTURES_H_
#define SRC_DATA_STRUCTURES_H_

#include <string.h>
#include <assert.h>

enum t_dataTypes {
    t_short_type = 0,
    t_int_type,
    t_longlong_type,
    t_double_type,
    t_bool_type,
    t_string_type,
    t_ntypes
};

// String only used in variant which stores the data aligned with the object
class VariantString {
public:
    VariantString(std::string & s) {
        n = s.length() + 1;
        buf = (char*)(((VariantString*)this)+1);
        for (int i=0; i<(int)n; i++) buf[i] = s[i];
        buf[n] = '\0';
    }
    char operator[](int i) const {
        return buf[i];
    }
    size_t size() const { return n; }
    char * data() { return buf; }
private:
    size_t n;
    char * buf;
};

class CVariant {
public:

//    CVariant(short val) {
//        m_data = (void *) malloc(sizeof(short));
//        *(short*)m_data = val;
//        m_type = t_short_type;
//        m_size = sizeof(short);
//    }
//
//    CVariant(int val) {
//        m_data = (void *) malloc(sizeof(int));
//        *(int*)m_data = val;
//        m_type = t_int_type;
//        m_size = sizeof(int);
//    }
//
//    CVariant(long long val) {
//        m_data = (void *) malloc(sizeof(long long));
//        *(long long*)m_data = val;
//        m_type = t_longlong_type;
//        m_size = sizeof(long long);
//    }
//
//    CVariant(double val) {
//        m_data = (void *) malloc(sizeof(double));
//        *(double*)m_data = val;
//        m_type = t_double_type;
//        m_size = sizeof(double);
//    }
//
//    CVariant(bool val) {
//        m_data = (void *) malloc(sizeof(bool));
//        *(bool*)m_data = val;
//        m_type = t_bool_type;
//        m_size = sizeof(bool);
//    }
//
//    CVariant(std::string & buf) {
//        m_data = (void *) malloc(sizeof(VariantString) + (int)buf.length() + 1);
//        new (m_data) VariantString(buf);
//        m_type = t_string_type;
//        m_size = sizeof(VariantString) + (size_t)buf.length() + 1;
//    }

    CVariant(char * data, t_dataTypes type, size_t s) {
        m_data = data;
        m_type = type;
        m_size = s;
    }

    ~CVariant() {
        //free (m_data);
    }

    operator short() const          { assert(m_type == t_short_type); return *(short*)m_data; }
    operator int() const            { assert(m_type == t_int_type); return *(int*)m_data; }
    operator long long() const      { assert(m_type == t_longlong_type); return *(long long*)m_data; }
    operator double() const         { assert(m_type == t_double_type); return *(double*)m_data; }
    operator bool() const           { assert(m_type == t_bool_type); return *(bool*)m_data; }
    operator std::string() const    {
        return std::string(((VariantString*)m_data)->data());
    }

    bool operator<(const CVariant & other) {
        assert(m_type == other.m_type);
        if (m_type != other.m_type) {
            return true;
        }
        switch (m_type) {
        case t_short_type:
            return (short)(*this) < (short)other;
            break;
        case t_int_type:
            return (int)(*this) < (int)other;
            break;
        case t_longlong_type:
            return (long long)(*this) < (long long)other;
            break;
        case t_double_type:
            return (double)(*this) < (double)other;
            break;
        case t_bool_type:
            return (bool)(*this) < (bool)other;
            break;
        case t_string_type:
        {
            VariantString * lhs = (VariantString *)m_data;
            const VariantString * rhs = (const VariantString *)other.m_data;

            int n = std::min((int)lhs->size(), (int)rhs->size());

            for (int i=0; i<n; i++) {
                if ((*lhs)[i] != (*rhs)[i]) {
                    return (*lhs)[i] < (*rhs)[i];
                }
            }

            return lhs->size() < rhs->size();
        }
            break;
        default:
            break;
        }
        return true;
    }

    bool operator!=(const CVariant & other) {
        assert(m_type == other.m_type);
        if (m_type != other.m_type) {
            return true;
        }
        switch (m_type) {
        case t_short_type:
            return (short)(*this) != (short)other;
            break;
        case t_int_type:
            return (int)(*this) != (int)other;
            break;
        case t_longlong_type:
            return (long long)(*this) != (long long)other;
            break;
        case t_double_type:
            return (double)(*this) != (double)other;
            break;
        case t_bool_type:
            return (bool)(*this) !=(bool)other;
            break;
        case t_string_type:
        {
            VariantString * lhs = (VariantString *)m_data;
            const VariantString * rhs = (const VariantString *)other.m_data;

            int n = std::min((int)lhs->size(), (int)rhs->size());

            for (int i=0; i<n; i++) {
                if ((*lhs)[i] != (*rhs)[i]) {
                    return true;
                }
            }

            return false;
        }
            break;
        default:
            break;
        }
        return true;
    }

    static t_dataTypes GetType(const std::string & type) {

        if (type == "SHORT") {
            return t_short_type;
        }
        if (type == "INT") {
            return t_int_type;
        }
        if (type == "INT64") {
             return t_longlong_type;
        }
        if (type == "DOUBLE") {
            return t_double_type;
        }
        if (type == "BOOL") {
            return t_bool_type;
        }
        else {
            if (type.substr(0, 6) == "STRING") {
                return t_string_type;
            }
        }

        return t_ntypes;
    }

    static size_t GetSize(const std::string & type) {

        if (type == "SHORT") {
            return sizeof(short);
        }
        if (type == "INT") {
            return sizeof(int);
        }
        if (type == "INT64") {
            return sizeof(long long);
        }
        if (type == "DOUBLE") {
            return sizeof(double);
        }
        if (type == "BOOL") {
            return sizeof(bool);
        }
        else {
            if (type.substr(0, 6) == "STRING") {
                int siz = atoi(type.substr(6, type.length() - 6).c_str());
                return siz*sizeof(char) + sizeof(VariantString);
            }
        }

        return 0;
    }

private:
    t_dataTypes m_type;
    size_t m_size;
    void * m_data;
};

class DataStructure {
public:

    DataStructure(int _n, int * _types, size_t * _sizes) {
        n = _n;
        types = new t_dataTypes[n];
        sizes = new size_t[n];

        memcpy(types, _types, n*sizeof(t_dataTypes));
        memcpy(sizes, _sizes, n*sizeof(size_t));
    }
    DataStructure(std::vector<std::string> & _types) {
        n = _types.size();
        types = new t_dataTypes[n];
        sizes = new size_t[n];

        for (int i=0; i<n; i++) {
            types[i] = CVariant::GetType(_types[i]);
            sizes[i] = CVariant::GetSize(_types[i]);
        }
    }
    DataStructure(std::vector<std::string> _types) {
        n = _types.size();
        types = new t_dataTypes[n];
        sizes = new size_t[n];

        for (int i=0; i<n; i++) {
            types[i] = CVariant::GetType(_types[i]);
            sizes[i] = CVariant::GetSize(_types[i]);
        }
    }

    DataStructure() {
        n = 0;
        types = NULL;
        sizes = NULL;
    }

    ~DataStructure() {
        Destroy();
    }

    void Destroy() {
        delete[] types;
        delete[] sizes;
    }

    DataStructure & operator=(const DataStructure & other) {
        if (this != &other) {
            Destroy();

            n = other.n;
            types = new t_dataTypes[n];
            sizes = new size_t[n];

            memcpy(types, other.types, n*sizeof(t_dataTypes));
            memcpy(sizes, other.sizes, n*sizeof(size_t));
        }
        return *this;
    }

    DataStructure(const DataStructure & other) {
        n = other.n;
        types = new t_dataTypes[n];
        sizes = new size_t[n];

        memcpy(types, other.types, n*sizeof(t_dataTypes));
        memcpy(sizes, other.sizes, n*sizeof(size_t));
    }

    int NTypes() const { return n; }

    t_dataTypes GetType(int i) const { return types[i]; }

    size_t GetTypeSize(int i) const { return sizes[i]; }

private:
    int n;
    t_dataTypes * types;
    size_t * sizes;
};

class DataType {
public:
    DataType(DataStructure * dataStruct, char * data) : m_dataStruct(dataStruct), m_data(data) {
    }

    DataType() : m_dataStruct(NULL), m_data(NULL) {}

    DataType(const DataType & other) : m_dataStruct(other.m_dataStruct), m_data(other.m_data) {
    }

    int NParams() const { return m_dataStruct != NULL ? m_dataStruct->NTypes() : 0; }

    bool operator<(const DataType & other) {

        char * cur = m_data;
        char * curOther = other.Data();

        for (int i=0; i<m_dataStruct->NTypes(); i++) {

            CVariant lhs(cur, m_dataStruct->GetType(i), m_dataStruct->GetTypeSize(i));
            CVariant rhs(cur, m_dataStruct->GetType(i), m_dataStruct->GetTypeSize(i));

            if (lhs != rhs) {
                return lhs < rhs;
            }

            cur += m_dataStruct->GetTypeSize(i);
            curOther += m_dataStruct->GetTypeSize(i);

        }

        return false;
    }

    bool operator<=(const DataType & other) {
        // TODO
        return true;
    }

    char * Data() { return m_data; }

private:
    DataStructure * m_dataStruct;
    char * m_data;
};

#endif /* SRC_DATA_STRUCTURES_H_ */

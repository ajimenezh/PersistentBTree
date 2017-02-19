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

    CVariant(short val) {
        m_data = (void *) malloc(sizeof(short));
        *(short*)m_data = val;
        m_type = t_short_type;
        m_size = sizeof(short);
    }

    CVariant(int val) {
        m_data = (void *) malloc(sizeof(int));
        *(int*)m_data = val;
        m_type = t_int_type;
        m_size = sizeof(int);
    }

    CVariant(long long val) {
        m_data = (void *) malloc(sizeof(long long));
        *(long long*)m_data = val;
        m_type = t_longlong_type;
        m_size = sizeof(long long);
    }

    CVariant(double val) {
        m_data = (void *) malloc(sizeof(double));
        *(double*)m_data = val;
        m_type = t_double_type;
        m_size = sizeof(double);
    }

    CVariant(bool val) {
        m_data = (void *) malloc(sizeof(bool));
        *(bool*)m_data = val;
        m_type = t_bool_type;
        m_size = sizeof(bool);
    }

    CVariant(std::string & buf) {
        m_data = (void *) malloc(sizeof(VariantString) + (int)buf.length() + 1);
        new (m_data) VariantString(buf);
        m_type = t_string_type;
        m_size = sizeof(VariantString) + (size_t)buf.length() + 1;
    }

    ~CVariant() {
        free (m_data);
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
            VariantString * lhs = (VariantString *)m_data;
            const VariantString * rhs = (const VariantString *)other.m_data;

            int n = std::min((int)lhs->size(), (int)rhs->size());

            for (int i=0; i<n; i++) {
                if ((*lhs)[i] != (*rhs)[i]) {
                    return (*lhs)[i] < (*rhs)[i];
                }
            }

            return lhs->size() < rhs->size();

            break;
        }
        return true;
    }

private:
    t_dataTypes m_type;
    size_t m_size;
    void * m_data;
};

class DataStructure {
public:

    DataStructure(int _n, t_dataTypes * _types, size_t * _sizes) {
        n = _n;
        types = new t_dataTypes[n];
        sizes = new size_t[n];

        memcpy(types, _types, n*sizeof(t_dataTypes));
        memcpy(sizes, _sizes, n*sizeof(size_t));
    }
    ~DataStructure() {
        Destroy();
    }

    void Destroy() {
        delete[] types;
        delete[] sizes;
    }

    DataStructure & DataStructure(const DataStructure & other) {
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

private:
    int n;
    t_dataTypes * types;
    size_t * sizes;
};

#endif /* SRC_DATA_STRUCTURES_H_ */

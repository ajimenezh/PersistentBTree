/*
 * string_parser.h
 *
 *  Created on: Feb 25, 2017
 *      Author: alex
 */

#ifndef SRC_STRING_UTILS_H_
#define SRC_STRING_UTILS_H_

#include <string>
#include <stdio.h>
#include <ctype.h>
#include <vector>

std::string to_upper(std::string s);

class StringParser {

public:
    StringParser(std::string s) : m_str(s), m_idx(0) {}

    bool hasNext() {
        while (m_idx < (int)m_str.length() && m_str[m_idx] == ' ') m_idx++;
        return m_idx < (int)m_str.length();
    }

    std::string next() {

        if (hasNext()) {

            int nxtIdx = m_idx;
            char searchChar = ' ';
            if (m_str[nxtIdx] == '\'') {
                searchChar = '\'';
                nxtIdx++;
                m_idx++;
            }
            while (nxtIdx < (int)m_str.length() && m_str[nxtIdx] != searchChar) nxtIdx++;

            std::string s = m_str.substr(m_idx, nxtIdx - m_idx);

            m_idx = nxtIdx + 1;

            return s;
        }
        else {
            return "";
        }
    }

    std::vector<std::string> tokenize() {
        m_idx = 0;
        for (int i=0; i<(int)m_str.length(); i++) {
            if (!isalnum(m_str[i])) {
                m_str[i] = ' ';
            }
        }
        std::vector<std::string> v;
        while (hasNext()) {
            v.push_back(next());
        }

        return v;
    }

private:
    std::string m_str;
    int m_idx;

};



#endif /* SRC_STRING_UTILS_H_ */

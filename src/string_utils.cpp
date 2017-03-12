#include "string_utils.h"

std::string to_upper(std::string s) {
    for (int i=0; i<(int)s.length(); i++) s[i] = toupper(s[i]);
    return s;
}





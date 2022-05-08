#include "string_processing.h"
#include <algorithm>

using namespace std;
list<string_view> SplitIntoWords(string_view text) {
    std::list<std::string_view> words;
    
    int i = 0;
    int len = 0;
    std::for_each(text.begin(), text.end(),
        [&](const char& c) {
            if (c == ' ') {
                if (len) {
                    words.push_back(text.substr(i, len));
                    i += len;
                    len = 0;
                }
                ++i;

            } else {
                ++len;
            }
        });
    
    if (len) {
        words.push_back(text.substr(i, len));
    }
    
    return words;
}
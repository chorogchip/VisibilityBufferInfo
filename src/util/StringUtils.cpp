#include "util/StringUtils.h"

#include <cwctype>

namespace util {

    bool StringUtils::equals_ignore_case(
        std::wstring_view lhs,
        std::wstring_view rhs) {
        if (lhs.size() != rhs.size()) return false;

        for (size_t i = 0; i < lhs.size(); ++i) {
            if (std::towlower(lhs[i]) != std::towlower(rhs[i])) return false;
        }
        return true;
    }

}

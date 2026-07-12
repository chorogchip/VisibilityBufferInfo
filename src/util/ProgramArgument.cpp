#include "util/ProgramArgument.h"

#include <sstream>

#include "util/StringUtils.h"

namespace util {


    ProgramArgument ProgramArgument::from_args(const std::vector<std::string>& args) {
        util::ProgramArgument ret{};

        for (size_t i = 0; i < args.size(); ++i) {

#define X(type, name, defl, arg) \
        if (args[i] == std::string("--" #arg)) { \
            if (i + 1 >= args.size()) throw std::runtime_error("Missing value for --" #arg); \
            ret.name = util::parse_value<type>(args[i + 1]); \
            ++i; \
            continue; \
        }
            ProgramArgument_MAC
#undef X
        };

        return ret;
    }

    std::string ProgramArgument::get_header_string() {
        std::ostringstream stream;
#define X(type, name, defl, argname) \
        stream << (#argname) << ',';
        ProgramArgument_MAC;
#undef X
        std::string ret = stream.str();
        if (!ret.empty()) ret.pop_back();
        return ret;
    }

    std::string ProgramArgument::to_string() const {
        std::ostringstream stream;
#define X(type, name, defl, argname) \
        stream << (this->name) << ',';
        ProgramArgument_MAC;
#undef X
        std::string ret = stream.str();
        if (!ret.empty()) ret.pop_back();
        return ret;
    }


    std::string ProgramResult::get_header_string() {
        std::ostringstream stream;
#define X(type, name, argname) \
        stream << (#argname) << ',';
        ProgramResult_MAC;
#undef X
        std::string ret = stream.str();
        if (!ret.empty()) ret.pop_back();
        return ret;
    }

    std::string ProgramResult::to_string() const {
        std::ostringstream stream;
#define X(type, name, argname) \
        stream << (this->name) << ',';
        ProgramResult_MAC;
#undef X
        std::string ret = stream.str();
        if (!ret.empty()) ret.pop_back();
        return ret;
    }


    std::string ProgramResultPerFrame::get_header_string() {
        std::ostringstream stream;
#define X(type, name, argname) \
        stream << (#argname) << ',';
        ProgramResultPerFrame_MAC;
#undef X
        std::string ret = stream.str();
        if (!ret.empty()) ret.pop_back();
        return ret;
    }

    std::string ProgramResultPerFrame::to_string() const {
        std::ostringstream stream;
#define X(type, name, argname) \
        stream << (this->name) << ',';
        ProgramResultPerFrame_MAC;
#undef X
        std::string ret = stream.str();
        if (!ret.empty()) ret.pop_back();
        return ret;
    }
}

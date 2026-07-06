#include "ArgParser.h"

#include <algorithm>
#include <string>
#include <sstream>
#include <stdexcept>
#include <type_traits>

#include "util/ProgramArgument.h"

template <typename T>
T parse_value(const std::string& s) {
    if constexpr (std::is_same_v<T, std::string>) return s;

    std::istringstream iss{ s };

    T value{};
    if (!(iss >> value)) throw std::runtime_error("parse value error: " + s);

    return value;
}

ProgramArgument ArgParser::parse(const std::vector<std::string>& args) {
    ProgramArgument ret{};

    for (size_t i = 0; i < args.size(); ++i) {

#define X(type, name, defl, arg) \
        if (args[i] == std::string("--" #arg)) { \
            if (i + 1 >= args.size()) throw std::runtime_error("Missing value for --" #arg); \
            ret.name = parse_value<type>(args[i + 1]); \
            ++i;\
            continue;\
        }

        ProgramArgument_MAC

#undef X

    };

    return ret;
}
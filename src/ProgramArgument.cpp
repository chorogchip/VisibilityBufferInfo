#include "ProgramArgument.h"

#include <iomanip>
#include <sstream>

#include "util/Constants.h"
#include "util/StringUtils.h"
#include "util/TimeUtils.h"

namespace util {


    ProgramArgument ProgramArgument::from_args(const std::vector<std::string>& args) {
        util::ProgramArgument ret{};

        for (size_t i = 0; i < args.size(); ++i) {

#define X(type, name, defl, arg) \
        if (args[i] == std::string("--" #arg)) { \
            if (i + 1 >= args.size()) { \
                Logger::g_logger.assert_with_log(false, "Missing value for --" #arg); \
            } \
            ret.name = util::StringUtils::parse_value<type>(args[i + 1]); \
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

    ProgramResult ProgramResult::from_args(const ProgramArgument& arg) {

        ProgramResult ret{};

        ret.camera_mode_name =
            arg.camera_mode == 0 ? "free" :
            arg.camera_mode == 1 ? "record" :
            arg.camera_mode == 2 ? "playback" :
            "unknown";
        ret.run_current_time = util::TimeUtils::make_current_time_string();

        return ret;
    }

    std::string ProgramResult::get_header_string() {
        std::ostringstream stream;

        for (size_t i = 0; i < util::Constants::TIMER_SLOT_COUNT; ++i) {
            stream << "pass_name_" << i << ',';
            stream << "pass_" << i << "_time_avg_ms,";
        }

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
        stream << std::fixed << std::setprecision(5);

        for (size_t i = 0; i < util::Constants::TIMER_SLOT_COUNT; ++i) {
            stream << pass_names[i] << ',';
            stream << pass_time_avg_ms[i] << ',';
        }

#define X(type, name, argname) \
        stream << (this->name) << ',';
        ProgramResult_MAC;
#undef X
        std::string ret = stream.str();
        if (!ret.empty()) ret.pop_back();
        return ret;
    }
}

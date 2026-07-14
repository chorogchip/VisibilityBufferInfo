#include "util/DummyTextureGen.h"

namespace util {

    std::vector<unsigned char> create_dummy_texture_data(
        unsigned int width,
        unsigned int height,
        unsigned int seed) {

        unsigned int texture_index = seed;

        constexpr unsigned int channels = 4;
        constexpr unsigned int checker_size = 16;
        constexpr unsigned int border_width = 4;
        constexpr unsigned int line_width = 2;

        if (width == 0 || height == 0) return {};

        std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * channels);

        // texture_index마다 서로 다른 기본 색상
        const auto base_r =
            static_cast<unsigned char>((texture_index * 97u + 53u) & 0xffu);
        const auto base_g =
            static_cast<unsigned char>((texture_index * 57u + 101u) & 0xffu);
        const auto base_b =
            static_cast<unsigned char>((texture_index * 31u + 193u) & 0xffu);

        const unsigned int safe_border =
            std::min(border_width, std::min(width, height) / 2u);

        for (unsigned int y = 0; y < height; ++y) {
            for (unsigned int x = 0; x < width; ++x) {
                const std::size_t offset =
                    (static_cast<std::size_t>(y) * width + x) * channels;

                const bool bright_checker =
                    ((x / checker_size) + (y / checker_size)) % 2u == 0u;

                unsigned char r =
                    bright_checker ? base_r
                    : static_cast<unsigned char>(base_r / 4u);
                unsigned char g =
                    bright_checker ? base_g
                    : static_cast<unsigned char>(base_g / 4u);
                unsigned char b =
                    bright_checker ? base_b
                    : static_cast<unsigned char>(base_b / 4u);

                // 흰색 테두리
                const bool is_border =
                    x < safe_border ||
                    y < safe_border ||
                    x >= width - safe_border ||
                    y >= height - safe_border;

                if (is_border) {
                    r = 255;
                    g = 255;
                    b = 255;
                }

                // 좌상단 -> 우하단 자홍색 대각선
                const std::int64_t main_diagonal =
                    static_cast<std::int64_t>(x) * height -
                    static_cast<std::int64_t>(y) * width;

                if (std::llabs(main_diagonal) <
                    static_cast<std::int64_t>(line_width) *
                    std::max(width, height)) {
                    r = 255;
                    g = 0;
                    b = 255;
                }

                // 우상단 -> 좌하단 청록색 대각선
                const std::int64_t opposite_diagonal =
                    static_cast<std::int64_t>(x) * height +
                    static_cast<std::int64_t>(y) * width -
                    static_cast<std::int64_t>(width) * height;

                if (std::llabs(opposite_diagonal) <
                    static_cast<std::int64_t>(line_width) *
                    std::max(width, height)) {
                    r = 0;
                    g = 255;
                    b = 255;
                }

                // 중앙 노란색 십자선
                const bool center_line =
                    std::abs(
                        static_cast<int>(x) -
                        static_cast<int>(width / 2u)) <
                    static_cast<int>(line_width) ||
                    std::abs(
                        static_cast<int>(y) -
                        static_cast<int>(height / 2u)) <
                    static_cast<int>(line_width);

                if (center_line) {
                    r = 255;
                    g = 255;
                    b = 0;
                }

                // 좌상단 빨간색 방향 마커
                const unsigned int marker_width = std::max(1u, width / 8u);
                const unsigned int marker_height = std::max(1u, height / 8u);

                if (x < marker_width && y < marker_height) {
                    r = 255;
                    g = 0;
                    b = 0;
                }

                pixels[offset + 0] = r;
                pixels[offset + 1] = g;
                pixels[offset + 2] = b;
                pixels[offset + 3] = 255;
            }
        }

        return pixels;
    }
}
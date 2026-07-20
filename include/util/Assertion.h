#pragma once

#include <concepts>
#include <type_traits>

namespace util {

    template<auto A, auto B, auto... Rest>
        requires (
            std::is_enum_v<decltype(A)>&&
            std::same_as<decltype(A), decltype(B)> &&
            (std::same_as<decltype(A), decltype(Rest)> && ...)
        )
    consteval void assure_contiguous() {
        using Underlying = std::underlying_type_t<decltype(A)>;

        static_assert(
            static_cast<Underlying>(A) + Underlying{ 1 } ==
            static_cast<Underlying>(B),
            "Enum values must be contiguous"
        );

        if constexpr (sizeof...(Rest) > 0) {
            assure_contiguous<B, Rest...>();
        }
    }
}
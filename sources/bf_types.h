#pragma once
#include <stdint.h>

using uint = unsigned int;
using u8   = uint8_t;
using i8   = int8_t;
using u16  = uint16_t;
using i16  = int16_t;
using u32  = uint32_t;
using i32  = int32_t;
using b32  = uint32_t;
using u64  = uint64_t;
using i64  = int64_t;
using f32  = float;
using f64  = double;
using ptrd = ptrdiff_t;

constexpr u8     u8_max     = std::numeric_limits<u8>::max();
constexpr u16    u16_max    = std::numeric_limits<u16>::max();
constexpr u32    u32_max    = std::numeric_limits<u32>::max();
constexpr u64    u64_max    = std::numeric_limits<u64>::max();
constexpr size_t size_t_max = std::numeric_limits<size_t>::max();

constexpr i8  i8_max  = std::numeric_limits<i8>::max();
constexpr i16 i16_max = std::numeric_limits<i16>::max();
constexpr i32 i32_max = std::numeric_limits<i32>::max();
constexpr i64 i64_max = std::numeric_limits<i64>::max();

constexpr i8  i8_min  = std::numeric_limits<i8>::min();
constexpr i16 i16_min = std::numeric_limits<i16>::min();
constexpr i32 i32_min = std::numeric_limits<i32>::min();
constexpr i64 i64_min = std::numeric_limits<i64>::min();

constexpr f32 f32_inf = std::numeric_limits<f32>::infinity();

// template <typename T>
// struct bf_uptr {
//     bf_uptr() noexcept {}
//
//     explicit bf_uptr(T* ptr) noexcept {
//         value = ptr;
//     }
//
//     bf_uptr(bf_uptr<T>&& other) {
//         value       = other.value;
//         other.value = nullptr;
//     }
//
// private:
//     T* value = nullptr;
// }

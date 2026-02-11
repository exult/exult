/*
 *  Copyright (C) 2020 Marzo Sette Torres Junior
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#ifndef ENDIANIO_H
#define ENDIANIO_H

#include <array>
#include <climits>
#include <cstdint>
#include <cstring>
#include <ios>
#include <limits>
#include <type_traits>

#if __cplusplus >= 202002L && __has_include(<bit>)
#	include <bit>
#endif

#if defined(__cpp_lib_endian) && __cpp_lib_endian >= 201907L
namespace endian_internal { namespace detail {
	using std::endian;
}}    // namespace endian_internal::detail
#elif defined(__GNUG__)
// Both libstdc++ and libc++ essentially do this.
namespace endian_internal { namespace detail {
	enum class endian : uint16_t {
		big    = __ORDER_BIG_ENDIAN__,
		little = __ORDER_LITTLE_ENDIAN__,
		native = __BYTE_ORDER__,
	};
}}    // namespace endian_internal::detail
#elif defined(_MSC_VER)
// MS-STL does essentially this.
namespace endian_internal { namespace detail {
	enum class endian : uint16_t {
		little = 1234,
		big    = 4321,
		native = little
	};
}}    // namespace endian_internal::detail
#endif

#ifdef __clang__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wunused-macros"
#	pragma GCC diagnostic ignored "-Wunused-template"
#endif

#ifndef __has_builtin
#	define ENDIANIO_HAS_BUILTIN
#	define __has_builtin(x) 0
#endif

#if defined(_MSC_VER)
#	define ENDIANIO_INLINE          __forceinline
#	define ENDIANIO_CONSTEXPR       __forceinline constexpr
#	define ENDIANIO_CONST_CONSTEXPR __forceinline constexpr
#	define ENDIANIO_PURE_CONSTEXPR  __forceinline constexpr
#elif defined(__GNUG__)
#	define ENDIANIO_INLINE          [[gnu::always_inline]] inline
#	define ENDIANIO_CONSTEXPR       [[gnu::always_inline]] constexpr
#	define ENDIANIO_CONST_CONSTEXPR [[using gnu: const, always_inline]] constexpr
#	define ENDIANIO_PURE_CONSTEXPR  [[using gnu: pure, always_inline]] constexpr
#else
#	define ENDIANIO_INLINE          inline
#	define ENDIANIO_CONSTEXPR       constexpr
#	define ENDIANIO_CONST_CONSTEXPR constexpr
#	define ENDIANIO_PURE_CONSTEXPR  constexpr
#endif

#if defined(_MSC_VER)
#	include <intrin.h>
#	include <stdlib.h>
#	pragma intrinsic(_byteswap_ushort)
#	pragma intrinsic(_byteswap_ulong)
#	pragma intrinsic(_byteswap_uint64)
#	if (defined(_M_ARM64) || defined(_M_ARM64EC) || defined(_M_HYBRID_X86_ARM64)) && !defined(_M_CEE_PURE) \
			&& !defined(__CUDACC__) && !defined(__INTEL_COMPILER) && !defined(__clang__)
#		define HAS_NEON_INTRINSICS 1
#		include <arm64_neon.h>
#	else
#		define HAS_NEON_INTRINSICS 0
#	endif
#	if ((defined(_M_IX86) && !defined(_M_HYBRID_X86_ARM64)) || (defined(_M_X64) && !defined(_M_ARM64EC))) \
			&& !defined(_M_CEE_PURE) && !defined(__CUDACC__) && !defined(__INTEL_COMPILER)
#		define HAS_POPCNT_INTRINSICS 1
#	else
#		define HAS_POPCNT_INTRINSICS 0
#	endif
#endif

namespace endian_internal { namespace detail {
	// is_detected and related (ported from Library Fundamentals v2).
	namespace inner {
		template <typename Default, typename AlwaysVoid, template <typename...> class Op, typename... Args>
		struct detector {
			using value_t = std::false_type;
			using type    = Default;
		};

		template <typename Default, template <typename...> class Op, typename... Args>
		struct detector<Default, std::void_t<Op<Args...>>, Op, Args...> {
			using value_t = std::true_type;
			using type    = Op<Args...>;
		};

		struct nonesuch final {
			nonesuch(const nonesuch&)                    = delete;
			nonesuch(nonesuch&&)                         = delete;
			nonesuch()                                   = delete;
			~nonesuch()                                  = delete;
			auto operator=(const nonesuch&) -> nonesuch& = delete;
			auto operator=(nonesuch&&) -> nonesuch&      = delete;
		};
	}    // namespace inner

	template <template <typename...> class Op, typename... Args>
	using is_detected = typename inner::detector<inner::nonesuch, void, Op, Args...>::value_t;

	template <template <typename...> class Op, typename... Args>
	constexpr bool is_detected_v = is_detected<Op, Args...>::value;

	template <template <typename...> class Op, typename... Args>
	constexpr bool exists = is_detected_v<Op, Args...>;

	namespace functions {
		template <typename T>
		using read = decltype(std::declval<T>().read(std::declval<char*>(), std::declval<std::streamsize>()));

		template <typename T>
		using write = decltype(std::declval<T>().write(std::declval<const char*>(), std::declval<std::streamsize>()));
	}    // namespace functions

	// Robust SFINAE idiom by Jonathan Boccara
	// https://www.fluentcpp.com/2019/08/23/how-to-make-sfinae-pretty-and-robust/
	template <typename T>
	using has_read_function_t = std::enable_if_t<exists<functions::read, T>, bool>;

	template <typename T>
	using has_write_function_t = std::enable_if_t<exists<functions::write, T>, bool>;

	template <typename T>
	using is_an_integer_t = std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, bool>;

	template <typename T>
	struct is_byte_type_impl : std::integral_constant<bool, false> {};

	template <>
	struct is_byte_type_impl<char> : std::integral_constant<bool, true> {};

	template <>
	struct is_byte_type_impl<unsigned char> : std::integral_constant<bool, true> {};

	template <>
	struct is_byte_type_impl<std::byte> : std::integral_constant<bool, true> {};

	template <typename T>
	using is_byte_type_t = std::enable_if_t<is_byte_type_impl<std::remove_cv_t<T>>::value, bool>;

	// Porting is_constant_evaluated from c++20
#if defined(__cpp_lib_is_constant_evaluated) && __cpp_lib_is_constant_evaluated >= 201811L
	[[nodiscard]] ENDIANIO_CONST_CONSTEXPR bool is_constant_evaluated() noexcept {
		return std::is_constant_evaluated();
	}
#elif __has_builtin(__builtin_is_constant_evaluated) || (defined(_MSC_VER) && _MSC_VER >= 1925)
	[[nodiscard]] ENDIANIO_CONST_CONSTEXPR bool is_constant_evaluated() noexcept {
		return __builtin_is_constant_evaluated();
	}
#else
	[[nodiscard]] ENDIANIO_CONST_CONSTEXPR bool is_constant_evaluated() noexcept {
		return false;
	}
#endif

	template <typename T, is_an_integer_t<T> = true>
	constexpr auto select_unsigned_impl() {
		constexpr const size_t Size = sizeof(T);
		if constexpr (Size == sizeof(uint8_t)) {
			return uint8_t{};
		} else if constexpr (Size == sizeof(uint16_t)) {
			return uint16_t{};
		} else if constexpr (Size == sizeof(uint32_t)) {
			return uint32_t{};
		} else if constexpr (Size == sizeof(uint64_t)) {
			return uint64_t{};
		} else {
			static_assert(
					Size == sizeof(uint8_t) || Size == sizeof(uint16_t) || Size == sizeof(uint32_t) || Size == sizeof(uint64_t));
		}
	}

	template <typename T>
	using select_unsigned_for_t = decltype(select_unsigned_impl<T>());

	template <class... Ts>
	struct overloaded : public Ts... {
		using Ts::operator()...;
	};
	template <class... Ts>
	overloaded(Ts...) -> overloaded<Ts...>;

#if defined(__cpp_lib_byteswap) && __cpp_lib_byteswap >= 202110L
	template <typename Int, std::enable_if_t<std::is_integral_v<Int>, bool> = true>
	ENDIANIO_CONST_CONSTEXPR Int byteswap(Int value) noexcept {
		return std::byteswap(value);
	}
#else
	template <typename UInt, std::enable_if_t<std::is_unsigned_v<UInt>, bool> = true>
	ENDIANIO_CONST_CONSTEXPR UInt byteswap_impl(UInt value) noexcept {
		if constexpr (sizeof(UInt) == 1) {
			return value;
		}
#	if defined(__GNUG__) || defined(_MSC_VER)
		if (!is_constant_evaluated()) {
			constexpr const auto builtin_byteswap = overloaded{
#		ifdef __GNUG__
					[](const uint16_t val) {
						return __builtin_bswap16(val);
					},
					[](const uint32_t val) {
						return __builtin_bswap32(val);
					},
					[](const uint64_t val) {
						return __builtin_bswap64(val);
					}
#		elif defined(_MSC_VER)
					[](const uint16_t val) {
						return _byteswap_ushort(val);
					},
					[](const uint32_t val) {
						return _byteswap_ulong(val);
					},
					[](const uint64_t val) {
						return _byteswap_uint64(val);
					}
#		endif
			};
			if constexpr (sizeof(UInt) == 2 || sizeof(UInt) == 4 || sizeof(UInt) == 8) {
				return builtin_byteswap(value);
			}
			if constexpr (sizeof(UInt) == 16) {
#		if __has_builtin(__builtin_bswap128)
				return __builtin_bswap128(value);
#		else
				return (builtin_byteswap(static_cast<uint64_t>(value >> 64))
						| (static_cast<UInt>(builtin_byteswap(static_cast<uint64_t>(value))) << 64));
#		endif
			}
		}
#	endif

		// Fallback implementation from libstdc++.
		size_t diff  = CHAR_BIT * (sizeof(UInt) - 1);
		UInt   mask1 = std::numeric_limits<unsigned char>::max();
		UInt   mask2 = mask1 << diff;
		UInt   val   = value;
		diff += size_t{2} * CHAR_BIT;
		for (size_t i = 0; i < sizeof(UInt) / 2; ++i) {
			diff -= size_t{2} * CHAR_BIT;
			UInt byte1 = val & mask1;
			UInt byte2 = val & mask2;
			val        = (val ^ byte1 ^ byte2 ^ (byte1 << diff) ^ (byte2 >> diff));
			mask1 <<= CHAR_BIT;
			mask2 >>= CHAR_BIT;
		}
		return val;
	}

	template <typename Int, std::enable_if_t<std::is_integral_v<Int>, bool> = true>
	ENDIANIO_CONST_CONSTEXPR Int byteswap(Int value) noexcept {
		using UInt = select_unsigned_for_t<Int>;
		if constexpr (std::is_same_v<UInt, std::remove_cv_t<Int>>) {
			return byteswap_impl(value);
		}
		return static_cast<Int>(byteswap_impl(static_cast<UInt>(value)));
	}

#endif

	struct EndianBaseImpl {
		template <endian FromEndian, typename Int, typename Byte, is_an_integer_t<Int> = true, is_byte_type_t<Byte> = true>
		ENDIANIO_CONSTEXPR static Int Read(Byte*&& source) {
			Int val;
			std::memcpy(&val, std::move(source), sizeof(Int));
			if constexpr (FromEndian != endian::native) {
				val = byteswap(val);
			}
			return val;
		}

		template <endian FromEndian, typename Int, typename Byte, is_an_integer_t<Int> = true, is_byte_type_t<Byte> = true>
		ENDIANIO_CONSTEXPR static Int Read(Byte*& source) {
			Int val;
			std::memcpy(&val, source, sizeof(Int));
			std::advance(source, sizeof(Int));
			if constexpr (FromEndian != endian::native) {
				val = byteswap(val);
			}
			return val;
		}

		template <endian FromEndian, typename Int, typename Stream, has_read_function_t<Stream> = true>
		ENDIANIO_INLINE static auto Read(Stream& in) {
			using UC = typename Stream::char_type;
			alignas(alignof(Int)) std::array<UC, sizeof(Int)> buffer;
			in.read(buffer.data(), sizeof(Int));
			return Read<FromEndian, Int>(buffer.data());
		}

		template <endian FromEndian, typename Int, typename Stream, has_read_function_t<Stream> = true>
		ENDIANIO_INLINE static auto Read(Stream* in) {
			using UC = typename Stream::char_type;
			alignas(alignof(Int)) std::array<UC, sizeof(Int)> buffer;
			in->read(buffer.data(), sizeof(Int));
			return Read<FromEndian, Int>(buffer.data());
		}

		template <endian ToEndian, typename Int, typename Byte, is_an_integer_t<Int> = true, is_byte_type_t<Byte> = true>
		ENDIANIO_CONSTEXPR static void Write(Byte*&& dest, Int val) {
			if constexpr (ToEndian != endian::native) {
				val = byteswap(val);
			}
			std::memcpy(std::move(dest), &val, sizeof(Int));
		}

		template <endian ToEndian, typename Int, typename Byte, is_an_integer_t<Int> = true, is_byte_type_t<Byte> = true>
		ENDIANIO_CONSTEXPR static void Write(Byte*& dest, Int val) {
			if constexpr (ToEndian != endian::native) {
				val = byteswap(val);
			}
			std::memcpy(dest, &val, sizeof(Int));
			std::advance(dest, sizeof(Int));
		}

		template <endian ToEndian, typename Int, typename Stream, has_write_function_t<Stream> = true>
		ENDIANIO_INLINE static auto Write(Stream& out, Int val) {
			using UC = typename Stream::char_type;
			alignas(alignof(Int)) std::array<UC, sizeof(Int)> buffer;
			Write<ToEndian>(buffer.data(), val);
			out.write(buffer.data(), sizeof(Int));
		}

		template <endian ToEndian, typename Int, typename Stream, has_write_function_t<Stream> = true>
		ENDIANIO_INLINE static auto Write(Stream* out, Int val) {
			using UC = typename Stream::char_type;
			alignas(alignof(Int)) std::array<UC, sizeof(Int)> buffer;
			Write<ToEndian>(buffer.data(), val);
			out->write(buffer.data(), sizeof(Int));
		}
	};

	template <endian type>
	struct endian_base {
		template <typename Src>
		ENDIANIO_CONSTEXPR static auto Read2(Src&& in) {
			return EndianBaseImpl::Read<type, uint16_t>(std::forward<Src>(in));
		}

		template <typename Src>
		ENDIANIO_CONSTEXPR static auto Read2s(Src&& in) {
			return EndianBaseImpl::Read<type, int16_t>(std::forward<Src>(in));
		}

		template <typename Src>
		ENDIANIO_CONSTEXPR static auto Read4(Src&& in) {
			return EndianBaseImpl::Read<type, uint32_t>(std::forward<Src>(in));
		}

		template <typename Src>
		ENDIANIO_CONSTEXPR static auto Read4s(Src&& in) {
			return EndianBaseImpl::Read<type, int32_t>(std::forward<Src>(in));
		}

		template <typename Src>
		ENDIANIO_CONSTEXPR static auto Read8(Src&& in) {
			return EndianBaseImpl::Read<type, uint64_t>(std::forward<Src>(in));
		}

		template <typename Src>
		ENDIANIO_CONSTEXPR static auto Read8s(Src&& in) {
			return EndianBaseImpl::Read<type, int64_t>(std::forward<Src>(in));
		}

		template <typename Src>
		ENDIANIO_CONSTEXPR static auto ReadPtr(Src&& in) {
			return EndianBaseImpl::Read<type, uintptr_t>(std::forward<Src>(in));
		}

		template <typename Int, typename Src, is_an_integer_t<Int> = true>
		ENDIANIO_CONSTEXPR static auto ReadN(Src&& in) {
			return EndianBaseImpl::Read<type, Int>(std::forward<Src>(in));
		}

		template <typename Dst>
		ENDIANIO_CONSTEXPR static void Write2(Dst&& out, uint16_t val) {
			EndianBaseImpl::Write<type>(std::forward<Dst>(out), val);
		}

		template <typename Dst>
		ENDIANIO_CONSTEXPR static void Write2s(Dst&& out, int16_t val) {
			EndianBaseImpl::Write<type>(std::forward<Dst>(out), val);
		}

		template <typename Dst>
		ENDIANIO_CONSTEXPR static void Write4(Dst&& out, uint32_t val) {
			EndianBaseImpl::Write<type>(std::forward<Dst>(out), val);
		}

		template <typename Dst>
		ENDIANIO_CONSTEXPR static void Write4s(Dst&& out, int32_t val) {
			EndianBaseImpl::Write<type>(std::forward<Dst>(out), val);
		}

		template <typename Dst>
		ENDIANIO_CONSTEXPR static void Write8(Dst&& out, uint64_t val) {
			EndianBaseImpl::Write<type>(std::forward<Dst>(out), val);
		}

		template <typename Dst>
		ENDIANIO_CONSTEXPR static void Write8s(Dst&& out, int64_t val) {
			EndianBaseImpl::Write<type>(std::forward<Dst>(out), val);
		}

		template <typename Dst>
		ENDIANIO_CONSTEXPR static void WritePtr(Dst&& out, uintptr_t val) {
			EndianBaseImpl::Write<type>(std::forward<Dst>(out), val);
		}

		template <typename Int, typename Dst, is_an_integer_t<Int> = true>
		ENDIANIO_CONSTEXPR static auto WriteN(Dst&& out, Int val) {
			return EndianBaseImpl::Write<type>(std::forward<Dst>(out), val);
		}
	};
}}    // namespace endian_internal::detail

using big_endian    = endian_internal::detail::endian_base<endian_internal::detail::endian::big>;
using little_endian = endian_internal::detail::endian_base<endian_internal::detail::endian::little>;
using native_endian = endian_internal::detail::endian_base<endian_internal::detail::endian::native>;

template <typename Src>
ENDIANIO_CONSTEXPR static auto Read1(Src&& in) {
	return endian_internal::detail::EndianBaseImpl::Read<endian_internal::detail::endian::native, uint8_t>(std::forward<Src>(in));
}

template <typename Src>
ENDIANIO_CONSTEXPR static auto Read1s(Src&& in) {
	return endian_internal::detail::EndianBaseImpl::Read<endian_internal::detail::endian::native, int8_t>(std::forward<Src>(in));
}

template <typename Dst>
ENDIANIO_CONSTEXPR static void Write1(Dst&& out, uint8_t val) {
	endian_internal::detail::EndianBaseImpl::Write<endian_internal::detail::endian::native>(std::forward<Dst>(out), val);
}

template <typename Dst>
ENDIANIO_CONSTEXPR static void Write1s(Dst&& out, int8_t val) {
	endian_internal::detail::EndianBaseImpl::Write<endian_internal::detail::endian::native>(std::forward<Dst>(out), val);
}

#if defined(__cpp_lib_bitops) && __cpp_lib_bitops >= 201907L
ENDIANIO_CONSTEXPR uint32_t bitcount(uint32_t val) {
	return static_cast<uint32_t>(std::popcount(val));
}
#elif defined(__GNUG__)
ENDIANIO_CONSTEXPR uint32_t bitcount(uint32_t val) {
	return static_cast<uint32_t>(__builtin_popcount(val));
}
#else
namespace endian_internal { namespace detail {
	ENDIANIO_CONSTEXPR uint32_t bitcount_fallback(uint32_t val) {
		val = val - ((val >> 1) & static_cast<uint32_t>(0x5555'5555'5555'5555ull));
		val = (val & static_cast<uint32_t>(0x3333'3333'3333'3333ull))
			  + ((val >> 2) & static_cast<uint32_t>(0x3333'3333'3333'3333ull));
		val = (val + (val >> 4)) & static_cast<uint32_t>(0x0F0F'0F0F'0F0F'0F0Full);
		// Multiply by one in each byte, so that it will have the sum of all
		// source bytes in the highest byte
		val = val * static_cast<uint32_t>(0x0101'0101ull);
		// Extract highest byte
		return val >> (std::numeric_limits<uint32_t>::digits - 8);
	}
}}    // namespace endian_internal::detail

ENDIANIO_CONSTEXPR uint32_t bitcount(uint32_t val) noexcept {
	if (!endian_internal::detail::is_constant_evaluated()) {
#	if defined(_MSC_VER) && HAS_NEON_INTRINSICS
		const __n64 _Temp = neon_cnt(__uint64ToN64_v(val));
		return neon_addv8(_Temp).n8_i8[0];
#	elif defined(_MSC_VER) && HAS_POPCNT_INTRINSICS
		return __popcnt(val);
#	endif
	}
	return endian_internal::detail::bitcount_fallback(val);
}
#endif

#ifdef __clang__
#	pragma GCC diagnostic pop
#endif

#undef ENDIANIO_INLINE
#undef ENDIANIO_CONSTEXPR
#undef ENDIANIO_CONST_CONSTEXPR
#undef ENDIANIO_PURE_CONSTEXPR

#ifdef ENDIANIO_HAS_BUILTIN
#	undef ENDIANIO_HAS_BUILTIN
#	undef __has_builtin
#endif

#if defined(_MSC_VER)
#	undef HAS_NEON_INTRINSICS
#	undef HAS_POPCNT_INTRINSICS
#endif

#endif    // ENDIANIO_H

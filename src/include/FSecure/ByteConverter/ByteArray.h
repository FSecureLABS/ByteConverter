#pragma once

#include <array>

namespace FSecure
{
	/// Owning container with size known at compilation time.
	template <size_t N>
	using ByteArray = std::array<uint8_t, N>;

	/// Idiom for detecting tuple ByteArray.
	template <typename T>
	struct IsByteArray : std::false_type {};
	template<size_t N>
	struct IsByteArray<ByteArray<N>> : std::true_type {};
}
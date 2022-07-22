#pragma once

#include "ByteVector.h"
#include "ByteArray.h"
#include <stdexcept>

namespace FSecure
{
	/// Non owning container.
	class ByteView : std::basic_string_view<ByteVector::value_type>
	{
	public:
		/// Privately inherited Type.
		using Super = std::basic_string_view<ByteVector::value_type>;

		/// Type of stored values.
		using ValueType = Super::value_type;

		/// Create from ByteVector.
		/// @param data. Data from which view will be constructed.
		/// @param offset. Offset from first byte of data collection.
		/// @return ByteView. View of data.
		/// @throw std::out_of range. If offset is greater than data.size().
		ByteView(ByteVector const& data, size_t offset = 0)
			: Super([&]()
			{
				if (offset > data.size())
					BYTE_CONVERTER_THROW(std::out_of_range{ OBF("Out of range. Data size: ") + std::to_string(data.size()) + OBF(" offset: ") + std::to_string(offset) });

				return data.data() + offset;
			}(), data.size() - offset)
		{

		}

		/// Create from ByteVector iterators.
		/// @param begin. Iterator to first element of data.
		/// @param end. Iterator to past the last element of data.
		/// @return ByteView. View of data.
		/// @throw std::out_of range. If begin is greater than end.
		ByteView(ByteVector::const_iterator begin, ByteVector::const_iterator end)
			: Super([&]()
			{
				if (begin > end)
					BYTE_CONVERTER_THROW(std::out_of_range{ OBF("Out of range by: ") + std::to_string(begin - end) + OBF(" elements.") });

				return begin != end ? &*begin : nullptr;
			}(), static_cast<size_t>(end - begin))
		{

		}

		/// Create from std::basic_string_view<uint8_t>.
		/// @param data. Data from which view will be constructed.
		/// @return ByteView. View of data.
		ByteView(std::basic_string_view<ByteVector::value_type> data)
			: Super(data)
		{

		}

		/// Create from ByteArray.
		/// @param data. Data from which view will be constructed.
		/// @return ByteView. View of data.
		template <size_t N>
		ByteView(ByteArray<N> const& data)
			: Super(data.data(), data.size())
		{

		}

		/// Create from std::string_view.
		/// @param data. Data from which view will be constructed.
		/// @return ByteView. View of data.
		ByteView(std::string_view data)
			: Super{ reinterpret_cast<const ByteVector::value_type*>(data.data()), data.size() }
		{
		}

		/// Allow cast to ByteVector.
		operator ByteVector() const
		{
			return { begin(), end() };
		}

		/// Allow cast to std::string().
		operator std::string() const
		{
			return { reinterpret_cast<const char*>(data()), size() };
		}

		/// Allow cast to std::string_view.
		operator std::string_view() const
		{
			return { reinterpret_cast<const char*>(data()), size() };
		}

		/// Create a sub-string from this ByteView.
		/// @param offset. 	Position of the first byte.
		/// @param count. Requested length
		/// @returns ByteView. View of the substring
		ByteView SubString(const size_type offset = 0, size_type count = npos) const
		{
			return Super::substr(offset, count);
		}

		// Enable methods.
		using std::basic_string_view<ByteVector::value_type>::basic_string_view;
		using Super::operator=;
		using Super::begin;
		using Super::cbegin;
		using Super::end;
		using Super::cend;
		using Super::rbegin;
		using Super::crbegin;
		using Super::rend;
		using Super::crend;
		using Super::operator[];
		using Super::at;
		using Super::front;
		using Super::back;
		using Super::data;
		using Super::size;
		using Super::length;
		using Super::max_size;
		using Super::empty;
		using Super::remove_prefix;
		using Super::remove_suffix;
		using Super::swap;
		using Super::copy;
		using Super::compare;
		using Super::find;
		using Super::rfind;
		using Super::find_first_of;
		using Super::find_last_of;
		using Super::find_first_not_of;
		using Super::find_last_not_of;
		using Super::npos;
		using Super::value_type;
		friend inline bool operator==(ByteView const& lhs, ByteView const& rhs);
		friend inline bool operator!=(ByteView const& lhs, ByteView const& rhs);

		/// Read bytes and move ByteView to position after parsed data.
		/// @param byteCount. How many bytes should be read.
		/// @returns ByteVector. Owning container with the read bytes.
		/// @throws std::out_of_range. If byteCount > size().
		ByteVector Read(size_t byteCount)
		{
			if (byteCount > size())
				BYTE_CONVERTER_THROW(std::out_of_range{ OBF(": Size: ") + std::to_string(size()) + OBF(". Cannot read ") + std::to_string(byteCount) + OBF(" bytes.") });

			auto retVal = ByteVector{ begin(), begin() + byteCount };
			remove_prefix(byteCount);
			return retVal;
		}

		/// Read object and move ByteView to position after parsed data.
		/// @tparam T. Mandatory type to be retrieved from ByteView.
		/// @tparam Ts. Optional types to be retrieved in one call.
		/// @note Works with types and templates that have specialized ByteConverter.
		/// Include ByteConverter.h to add support for arithmetic, iterable, tuple and other standard types.
		/// Create new specializations for custom types.
		/// @returns one type if Ts was empty, std::tuple with all types otherwise.
		/// Simple usage:
		/// @code auto [a, b, c] = someByteView.Read<int, float, std::string>(); @endcode
		/// @note Returned types does not have to match exactly with Read template parameters list.
		/// It is possible to create tags that will be used to retrieve different type.
		template<typename T, typename ...Ts>
		requires requires(ByteView& bv) { ByteConverter<std::remove_cvref_t<T>>::From(bv); }
		auto Read()
		{
			auto copy = *this;
			BYTE_CONVERTER_TRY
			{
				auto current = ByteConverter<std::remove_cvref_t<T>>::From(*this);
				if constexpr (sizeof...(Ts) == 0)
					return current;
				else if constexpr (sizeof...(Ts) == 1)
					return std::make_tuple(std::move(current), Read<Ts...>());
				else
					return std::tuple_cat(std::make_tuple(std::move(current)), Read<Ts...>());
			}
			BYTE_CONVERTER_CATCH(...)
			{
				*this = copy;
				BYTE_CONVERTER_THROW();
			}
		}
	};

	/// Helper class for Reading data from ByteView.
	class ByteReader
	{
		/// Reference to ByteView with data.
		/// ByteReader will modify ByteView used for it construction.
		ByteView& m_byteView;

	public:
		/// Create ByteReader.
		/// @param bv, ByteView with data to read.
		ByteReader(ByteView& bv)
			: m_byteView{bv}
		{}

		/// Read each of arguments from ByteView.
		/// @param ts, arguments to tie, and read.
		template <typename ...Ts>
		void Read(Ts&... ts)
		{
			((ts = m_byteView.Read<decltype(ts)>()), ...);
		}
	};

	namespace Literals
	{
		/// Create ByteView with syntax ""_bvec
		inline FSecure::ByteView operator "" _bv(const char* data, size_t size)
		{
			return { reinterpret_cast<const uint8_t*>(data), size };
		}

		/// Create ByteView with syntax L""_bvec
		inline FSecure::ByteView operator "" _bv(const wchar_t* data, size_t size)
		{
			return { reinterpret_cast<const uint8_t*>(data), size * sizeof(wchar_t) };
		}
	}

	/// Checks if the contents of lhs and rhs are equal.
	/// @param lhs. Left hand side of operator.
	/// @param rhs. Right hand side of operator.
	inline bool operator==(FSecure::ByteView const& lhs, FSecure::ByteView const& rhs)
	{
		if (lhs.size() != rhs.size())
			return false;

		return !memcmp(lhs.data(), rhs.data(), lhs.size());
	}

	/// Checks if the contents of lhs and rhs are equal.
	/// @param lhs. Left hand side of operator.
	/// @param rhs. Right hand side of operator.
	inline bool operator!=(FSecure::ByteView const& lhs, FSecure::ByteView const& rhs)
	{
		return !(lhs == rhs);
	}
}

namespace std
{
	/// Add hashing function for ByteView.
	template <>
	struct hash<FSecure::ByteView>
	{
		size_t operator()(FSecure::ByteView const& bv) const
		{
			return std::hash<std::string_view>{}(bv);
		}
	};
}

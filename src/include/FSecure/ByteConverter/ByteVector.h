#pragma once

#include "Utils.h"

#include <limits>
#include <cstring>
#include <string>
#include <cstring>
#include <string_view>
#include <vector>

namespace FSecure
{
	/// Forward declaration
	class ByteView;
	class ByteVector;

	/// Empty template of class that can convert data from and to byte form.
	/// Look for example in ByteConventer.h
	template <typename T, typename = void>
	struct ByteConverter {};

	namespace Detail
	{
		enum class SizeFunction
		{
			absent,
			compileTime,
			runTime,
		};

		enum class ToFunction
		{
			absent,
			createsContainer,
			expandsContainer,
		};

		namespace Impl
		{
			namespace SizeConcept
			{
				template <typename T, typename = void>
				struct CompileTime
					: std::false_type {};

				template <typename T>
				struct CompileTime<T, decltype(void(ByteConverter<T>::Size()))>
					: std::true_type {};

				template <typename T, typename = void>
				struct RunTime
					: std::false_type {};

				template <typename T>
				struct RunTime<T, decltype(void(ByteConverter<T>::Size(std::declval<T>())))>
					: std::true_type {};
			}

			namespace ToConcept
			{
				template <typename T, typename = void>
				struct Create
					: std::false_type {};

				template <typename T>
				struct Create<T, decltype(void(ByteConverter<T>::To(std::declval<T>())))>
					: std::true_type {};

				template <typename T, typename = void>
				struct Expand
					: std::false_type {};

				template <typename T>
				struct Expand<T, decltype(void(ByteConverter<T>::To(std::declval<T>(), std::declval<ByteVector&>())))>
					: std::true_type {};
			}

			template<SizeFunction V>
			using SizeFunctionConstant = std::integral_constant<SizeFunction, V>;

			template<ToFunction V>
			using ToFunctionConstant = std::integral_constant<ToFunction, V>;

			template <typename T, typename = void>
			struct FunctionSize
				: SizeFunctionConstant<SizeFunction::absent> {};

			template <typename T>
			struct FunctionSize<T, std::enable_if_t<SizeConcept::CompileTime<T>::value>>
				: SizeFunctionConstant<SizeFunction::compileTime> {};

			template <typename T>
			struct FunctionSize<T, std::enable_if_t<!SizeConcept::CompileTime<T>::value && SizeConcept::RunTime<T>::value>>
				: SizeFunctionConstant<SizeFunction::runTime> {};

			template <typename T, typename = void>
			struct FunctionTo
				: ToFunctionConstant<ToFunction::absent> {};

			template <typename T>
			struct FunctionTo<T, std::enable_if_t<!ToConcept::Expand<T>::value && ToConcept::Create<T>::value>>
				: ToFunctionConstant<ToFunction::createsContainer> {};

			template <typename T>
			struct FunctionTo<T, std::enable_if_t<ToConcept::Expand<T>::value>>
				: ToFunctionConstant<ToFunction::expandsContainer> {};
		}

		/// Class detecting specifics of ByteConverter specialization for given type.
		template <typename T>
		struct ConverterDeduction
		{
			struct FunctionSize
			{
				using type = SizeFunction;
				static constexpr auto value = Impl::FunctionSize<T>::value;
			};

			struct FunctionTo
			{
				using type = ToFunction;
				static constexpr auto value = Impl::FunctionTo<T>::value;
			};

			static_assert(!((FunctionTo::value == FunctionTo::type::expandsContainer) && (FunctionSize::value == FunctionSize::type::absent)),
				"ByteConverter::Size must be defined, if function ByteConverter::To expands already allocated container.");
		};

		/// Checks if ByteConverter is defined for all types.
		template <typename ...Ts>
		struct WriteCondition
		{
			static constexpr bool value = ((ConverterDeduction<Ts>::FunctionTo::value != ToFunction::absent) && ...);
		};

		/// Check if type can be concatenated
		template <typename ...Ts>
		struct ConcatCondition
		{
			static constexpr bool value = (sizeof...(Ts) > 0) && ((Utils::IsOneOf<Ts, ByteView, ByteVector>::value && ...));
		};
	}

	/// An owning container.
	class ByteVector : std::vector<std::uint8_t>
	{
	public:
		/// Privately inherited Type.
		using Super = std::vector<std::uint8_t>;

		/// Type of stored values.
		using ValueType = Super::value_type;


		/// Destructor.
		~ByteVector()
		{
#if defined BYTEVECTOR_ZERO_MEMORY_DESTRUCTION
			Clear();
#endif
		}


		/// Copy constructor.
		/// @param other. Object to copy.
		ByteVector(ByteVector const& other)
			: Super(static_cast<Super const&>(other))
		{

		}

		/// Move constructor.
		/// @param other. Object to move.
		ByteVector(ByteVector&& other)
			: Super(static_cast<Super&&>(other))
		{

		}

		/// Copy assignment operator.
		/// @param other. Object to copy.
		ByteVector& operator=(ByteVector const& other)
		{
			auto tmp = Super{ static_cast<Super const&>(other) };
			std::swap(static_cast<Super&>(*this), tmp);
			return *this;
		}

		/// Move assignment operator.
		/// @param other. Object to move.
		ByteVector& operator=(ByteVector&& other)
		{
			std::swap(static_cast<Super&>(*this), static_cast<Super&>(other));
			return *this;
		}

		/// Create from std::vector.
		/// @param other. Object to copy.
		ByteVector(std::vector<uint8_t> other)
			: Super(std::move(other))
		{

		}

		// Enable methods.
		using Super::vector;
		using Super::value_type;
		using Super::allocator_type;
		using Super::size_type;
		using Super::difference_type;
		using Super::reference;
		using Super::const_reference;
		using Super::pointer;
		using Super::const_pointer;
		using Super::iterator;
		using Super::const_iterator;
		using Super::reverse_iterator;
		using Super::const_reverse_iterator;
		using Super::operator=;
		using Super::assign;
		using Super::get_allocator;
		using Super::at;
		using Super::operator[];
		using Super::front;
		using Super::back;
		using Super::data;
		using Super::begin;
		using Super::cbegin;
		using Super::end;
		using Super::cend;
		using Super::rbegin;
		using Super::crbegin;
		using Super::rend;
		using Super::crend;
		using Super::empty;
		using Super::size;
		using Super::max_size;
		using Super::reserve;
		using Super::capacity;
		using Super::shrink_to_fit;
		using Super::clear;
		using Super::insert;
		using Super::emplace;
		using Super::erase;
		using Super::push_back;
		using Super::emplace_back;
		using Super::pop_back;
		using Super::resize;
		friend inline bool operator==(ByteVector const& lhs, ByteVector const& rhs);
		friend inline bool operator!=(ByteVector const& lhs, ByteVector const& rhs);

		/// Write content of of provided objects.
		/// Supports arithmetic types, and basic iterable types.
		/// Include ByteConverter.h to add support for common types like enum, std::tuple and others.
		/// Create specialization on ByteConverter for custom types or template types to expand existing serialization functionality.
		/// @param arg. Object to be stored.
		/// @param args. Optional other objects to be stored.
		/// @return itself to allow chaining.
		template <typename T, typename ...Ts, typename std::enable_if_t<Detail::WriteCondition<T, Ts...>::value, int> = 0>
		ByteVector& Write(T const& arg, Ts const& ...args)
		{
			reserve(size() + Size<T, Ts...>(arg, args...));
			Store<T, Ts...>(arg, args...);
			return *this;
		}

		/// Write content of of provided objects.
		/// Supports ByteView and ByteVector.
		/// Does not write header with size..
		/// @param args. Objects to be stored.
		/// @return itself to allow chaining.
		template <typename ...Ts, typename std::enable_if_t<Detail::ConcatCondition<Ts...>::value, int> = 0>
		ByteVector& Concat(Ts const& ...args)
		{
			auto oldSize = size();
			BYTE_CONVERTER_TRY
			{
				resize(oldSize + (args.size() + ...));
				auto ptr = data() + oldSize;
				((memcpy(ptr, args.data(), args.size()), (ptr += args.size())), ...);
				return *this;
			}
			BYTE_CONVERTER_CATCH (...)
			{
				resize(oldSize);
				BYTE_CONVERTER_THROW();
			}
		}

		/// Create new ByteVector with Variadic list of parameters.
		/// This function cannot be constructor, because it would be ambiguous with super class constructors.
		/// @param arg. Object to be stored.
		/// @param args. Optional other objects to be stored.
		/// @see ByteVector::Write for more informations.
		template <typename T, typename ...Ts, typename std::enable_if_t<Detail::WriteCondition<T, Ts...>::value, int> = 0>
		static ByteVector Create(T const& arg, Ts const& ...args)
		{
			return std::move(ByteVector{}.Write(arg, args...));
		}

		/// Calculate the size that the argument will take in memory
		/// @param arg. Argument to be stored.
		/// @param args. Rest of types that will be handled with recursion.
		/// @return size_t number of bytes needed.
		template<typename T, typename ...Ts>
		static size_t Size([[maybe_unused]] T const& arg, Ts const& ...args)
		{
			using Deduction = typename Detail::ConverterDeduction<T>::FunctionSize;
			if constexpr (sizeof...(args) != 0)
				return Size(arg) + Size(args...);
			else if constexpr (Deduction::value == Deduction::type::compileTime)
				return ByteConverter<T>::Size();
			else if constexpr (Deduction::value == Deduction::type::runTime)
				return ByteConverter<T>::Size(arg);
			else if constexpr (Deduction::value == Deduction::type::absent)
				return ByteConverter<T>::To(arg).size();
		}

		/// Zero memory. Not optimized out during destruction.
		void Clear()
		{
			Utils::SecureMemzero(data(), size());
		}

	private:
		/// Store custom type.
		/// @param arg. Object to be stored. There must exsist FSecure::ByteConverter<T>::To method avalible to store custom type.
		/// @param args. Rest of objects that will be handled with recursion.
		/// @return itself to allow chaining.
		template<typename T, typename ...Ts, typename std::enable_if_t<Detail::WriteCondition<T, Ts...>::value, int> = 0>
		ByteVector& Store(T const& arg, Ts const& ...args)
		{
			auto oldSize = size();
			BYTE_CONVERTER_TRY
			{
				using Deduction = typename Detail::ConverterDeduction<T>::FunctionTo;
				if constexpr (Deduction::value == Deduction::type::createsContainer)
					Concat(ByteConverter<T>::To(arg));
				else
					ByteConverter<T>::To(arg, *this);

				if constexpr (sizeof...(Ts) != 0)
					Store(args...);

				return *this;
			}
			BYTE_CONVERTER_CATCH (...)
			{
				resize(oldSize);
				BYTE_CONVERTER_THROW();
			}
		}

		/// Declaration of friendship.
		template <typename , typename>
		friend struct ByteConverter;

		/// Declaration of friendship.
		template <typename>
		friend struct TupleConverter;

		/// Declaration of friendship.
		template <typename>
		friend struct PointerTupleConverter;
	};

	namespace Literals
	{
		/// Create ByteVector with syntax ""_bvec
		inline ByteVector operator "" _b(const char* data, size_t size)
		{
			ByteVector ret;
			ret.resize(size);
			std::memcpy(ret.data(), data, size);
			return ret;
		}

		/// Create ByteVector with syntax L""_bvec
		inline ByteVector operator "" _b(const wchar_t* data, size_t size)
		{
			ByteVector ret;
			ret.resize(size * sizeof(wchar_t));
			std::memcpy(ret.data(), data, size * sizeof(wchar_t));
			return ret;
		}
	}

	/// Checks if the contents of lhs and rhs are equal.
	/// @param lhs. Left hand side of operator.
	/// @param rhs. Right hand side of operator.
	inline bool operator==(ByteVector const& lhs, ByteVector const& rhs)
	{
		if (lhs.size() != rhs.size())
			return false;

		return !memcmp(lhs.data(), rhs.data(), lhs.size());
	}

	/// Checks if the contents of lhs and rhs are equal.
	/// @param lhs. Left hand side of operator.
	/// @param rhs. Right hand side of operator.
	inline bool operator!=(ByteVector const& lhs, ByteVector const& rhs)
	{
		return !(lhs == rhs);
	}
}

namespace std
{
	/// Add hashing function for ByteVector.
	template <>
	struct hash<FSecure::ByteVector>
	{
		size_t operator()(FSecure::ByteVector const& bv) const
		{
			return std::hash<std::string_view>{}(std::string_view{ reinterpret_cast<const char*>(bv.data()), bv.size() });
		}
	};
}

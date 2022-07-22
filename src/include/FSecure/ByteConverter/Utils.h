#pragma once

#include <string_view>
#include <type_traits>
#include <utility>
#include <functional>
#include <array>
#include <vector>
#include <tuple>
#include <concepts>


/// Define BYTE_CONVERTER_NO_EXCEPTIONS to disable exception definitions manually.
#if (defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)) && !defined(BYTE_CONVERTER_NO_EXCEPTIONS)
	#define BYTE_CONVERTER_HAS_EXCEPTIONS 1
#else
	#define BYTE_CONVERTER_HAS_EXCEPTIONS 0
#endif

/// Allows compilation with exceptions turned off.
/// std::abort() will be called on throw.
#if (BYTE_CONVERTER_HAS_EXCEPTIONS)
	#include <stdexcept>
	#define BYTE_CONVERTER_THROW(...) throw __VA_ARGS__
	#define BYTE_CONVERTER_TRY try
	#define BYTE_CONVERTER_CATCH(exception) catch(exception)
#else
	#include <cstdlib>
	#define BYTE_CONVERTER_THROW(...) std::abort()
	#define BYTE_CONVERTER_TRY if constexpr (true)
	#define BYTE_CONVERTER_CATCH(exception) if constexpr (false)
#endif

/// Allow normal string if obfuscation macro is not defined.
#ifndef OBF
#	define OBF(x) x
#endif // !OBF

namespace FSecure::Utils
{
	/// Prevents compiler from optimizing out call.
	/// @param ptr pointer to memory to be cleared.
	/// @param n number of bytes to overwrite.
	inline void* SecureMemzero(void* ptr, size_t n)
	{
		if (ptr) for (auto p = reinterpret_cast<volatile char*>(ptr); n--; *p++ = 0);
		return ptr;
	}

	/// Concept to evaluate if T is one of Ts types.
	template <typename T, typename ...Ts>
	concept IsOneOf = ((std::same_as<T, Ts> || ...));

	/// Concept to evaluate all of Ts are equal to T.
	template <typename T, typename ...Ts>
	concept IsSame = ((std::same_as<T, Ts> && ...));

	/// Namespace for internal implementation.
	namespace Impl
	{
		template <template <typename ...> class Tmp, typename T>
		struct IsTemplateInstance : std::false_type {};

		template<template <typename ...> class Tmp, typename ...T>
		struct IsTemplateInstance<Tmp, Tmp<T...>> : std::true_type {};
	}

	/// Concept for detecting template instance.
	/// @example IsTemplateInstance<std::vector, MyType> will check if MyType is a vector.
	/// @note Works only when all template arguments are types.
	/// Template like std::array<typename, size_t> will fail to match arguments.
	template<template <typename ...> class Tmp, class T>
	concept IsTemplateInstance = Impl::IsTemplateInstance<Tmp, T>::value;

	/// Concept for detecting pair.
	template<class T>
	concept IsPair = IsTemplateInstance<std::pair, T>;

	/// Concept for detecting tuple.
	template<class T>
	concept IsTuple = IsPair<T> || IsTemplateInstance<std::tuple, T>;

	/// Concept for detecting types that are basic_string_view.
	template<typename T>
	concept IsBasicView = std::same_as<T, std::basic_string_view<typename T::value_type>>;

	/// Concept for detecting types that publicly inherit from basic_string_view.
	template<typename T>
	concept DerivesFromBasicView = std::derived_from<T, std::basic_string_view<typename T::value_type>>;

	/// Concept for detecting view.
	template<class T>
	concept IsView = IsBasicView<T> || DerivesFromBasicView<T>;

	/// Namespace full of helpers for containers template programing.
	namespace Container
	{
		/// Get type stored by container that uses iterators.
		template <typename T>
		using StoredValue = std::remove_cvref_t<decltype(*begin(std::declval<T>()))>;

		/// Check if type can be iterated with begin() and end().
		template <typename T>
		concept IsIterable = requires(T t) { begin(t); end(t); };

		/// Check if type have own implementation of size method.
		template <typename T>
		concept HasDedicatedSize = requires(T t) { size(t); };

		/// Check if type have insert(iterator, value) function.
		template <typename T>
		concept HasInsert = requires(T t) { t.insert(begin(t), *end(t)); };

		/// Check if type can reserve some space to avoid reallocations.
		template <typename T>
		concept HasReserve = requires(T t) { t.reserve(size_t{}); };

		/// Returns number of elements in container, if size(T const&), or pair of begin(T const&), end(T const&) functions can be found.
		struct Size
		{
			template <typename T>
			requires HasDedicatedSize<T> || IsIterable<T>
			size_t operator () (T const& obj) const
			{
				size_t count = 0;
				if constexpr (HasDedicatedSize<T>) count = size(obj);
				else for (auto it = obj.begin(); it != obj.end(); ++it, ++count);

				return count;
			}
		};

		/// Struct to generalize container construction.
		/// Defines one of allowed operators that will return requested container.
		/// @tparam T. Type to be constructed.
		/// @tparam unnamed typename with default value, to allow easy creation of partial specializations.
		template <typename T, typename = void>
		struct Generator
		{
			/// Form with queued access to each of container values.
			/// @param size. Defines numbers of elements in constructed container.
			/// @param next. Functor returning one of container values at a time.
			// T operator()(uint32_t size, std::function<StoredValue<T>()> next);

			/// Form with direct access to memory.
			/// @param size. Defines numbers of elements in constructed container.
			/// @param data. Allows access to data used to container generation.
			/// Dereferenced pointer should be changed, to represent number of bytes consumed for container generation.
			// T operator()(uint32_t size, const char** data);
		};

		/// Generator for any container that have insert method.
		template <typename T>
		requires HasInsert<T>
		struct Generator<T>
		{
			/// Form with queued access to each of container values.
			/// @param size. Defines numbers of elements in constructed container.
			/// @param next. Functor returning one of container values at a time.
			T operator()(uint32_t size, std::function<StoredValue<T>()> next)
			{
				T ret;
				if constexpr (HasReserve<T>)
					ret.reserve(size);

				for (auto i = 0u; i < size; ++i)
					ret.insert(ret.end(), next());

				return ret;
			}
		};

		/// Generator for any container that is simmilar to std::basic_string_view.
		template <typename T>
		requires IsView<T>
		struct Generator<T>
		{
			/// Form with direct access to memory.
			/// @param size. Defines numbers of elements in constructed container.
			/// @param data. Allows access to data used to container generation.
			/// Dereferenced pointer should be changed, to represent number of bytes consumed for container generation.
			T operator()(uint32_t size, const char** data)
			{
				auto ptr = reinterpret_cast<const typename T::value_type*>(*data);
				*data += size * sizeof(typename T::value_type);
				return { ptr, size };
			}
		};

		/// Generator for any array types.
		template <typename T, size_t N>
		struct Generator<std::array<T, N>>
		{
			/// Form with queued access to each of container values.
			/// @param size. Defines numbers of elements in constructed container.
			/// @param next. Functor returning one of container values at a time.
			std::array<T, N> operator()(uint32_t size, std::function<T()> next)
			{
				if (size != N)
					BYTE_CONVERTER_THROW(std::runtime_error{ OBF("Array size does not match declaration") });

				std::vector<T> temp;
				temp.reserve(N);
				for (auto i = 0u; i < N; ++i)
					temp.push_back(next());

				return MakeArray(std::move(temp), std::make_index_sequence<N>());
			}

		private:
			/// Create array with size N from provided vector.
			/// This helper function is required because T might not be default constructible, but array must be filled like aggregator.
			/// Reading data to vector first will ensure order of elements, independent of calling convention.
			/// @param obj. Temporary vector with data.
			/// @returns std::array with all elements.
			template<size_t... Is>
			std::array<T, N> MakeArray(std::vector<T>&& obj, std::index_sequence<Is...>)
			{
				return { std::move(obj[Is])... };
			}
		};

		/// Concepts to check signature of generator for provided type.
		namespace Impl::Deduction::GeneratorSignature
		{
			template<typename T>
			concept Direct = requires { Generator<T>{}(uint32_t{}, std::declval<const char**>()); };

			template<typename T>
			concept Queued = requires { Generator<T>{}(uint32_t{}, std::function<StoredValue<T>()>{}); };
		}

		/// Determines how data should be accessed to create container.
		enum class AccessType
		{
			unknown,
			directMemory,
			queued,
		};

		/// Used to detect form of Generator operator() at compile time.
		/// Direct access to memory is preferred over Queued.
		template <typename T>
		struct GeneratorSignature
		{
			using type = AccessType;
			static constexpr auto value = Impl::Deduction::GeneratorSignature::Direct<T> ?
				type::directMemory :
				Impl::Deduction::GeneratorSignature::Queued<T> ? type::queued : type::unknown;
		};
	}

	/// Concept checking if it is worth to take reference or copy by value.
	template<typename T>
	concept WorthAddingConstRef = !std::is_rvalue_reference_v<T> && (!std::is_trivial_v<std::remove_reference_t<T>> || sizeof(T) > sizeof(long long));

	/// Namespace for internal implementation
	namespace Impl
	{
		/// @brief Default implementation of class declaring simpler type to use based on T type.
		/// Represents copy by value.
		template<typename T>
		struct AddConstRefToNonTrivial
		{
			using type = std::remove_reference_t<T>;
		};

		/// @brief Specialization of class declaring simpler type to use based on T type.
		/// Represents copy by reference.
		template<typename T>
		requires WorthAddingConstRef<T>
		struct AddConstRefToNonTrivial<T>
		{
			using type = std::remove_reference_t<T> const&;
		};
	}

	/// @brief Class declaring simpler type to use based on T type.
	template<typename T>
	struct  AddConstRefToNonTrivial : Impl::AddConstRefToNonTrivial<T> {};

	/// @brief Simplified AddConstRefToNonTrivial<T>::type.
	template<typename T>
	using AddConstRefToNonTrivialT = typename AddConstRefToNonTrivial<T>::type;

	/// @brief Helper allowing transformation of provided arguments to tuple of values/references that are used for serialization.
	/// @param ...args arguments to be stored in tuple
	/// @return tuple with references to non trivial types, and values of simple ones.
	template<typename ...Args>
	auto MakeConversionTuple(Args&& ...args)
	{
		return std::tuple<AddConstRefToNonTrivialT<Args&&>...>(std::forward<Args>(args)...);
	}

	/// Construction with parentheses and with braces is not interchangeable.
	/// std::make_from_tuple must use constructor and is unable to create trivial type.
	/// Types implemented in this namespace, alongside with std::apply, allows us choose method used for construction.
	/// For more information look at:
	/// https://groups.google.com/a/isocpp.org/forum/#!topic/std-discussion/aQQzL0JoXLg
	namespace Construction
	{
		/// @brief Type used for construction with braces.
		template <typename T>
		struct Braces
		{
			template <typename ... As>
			constexpr auto operator () (As&& ... as) const
			{
				return T{ std::forward<As>(as)... };
			}
		};

		/// @brief Type used for construction with parentheses.
		template <typename T>
		struct Parentheses
		{
			template <typename ... As>
			constexpr auto operator () (As&& ... as) const
			{
				return T(std::forward<As>(as)...);
			}
		};
	}

	/// @brief Constexpr helper to perform logic on tuple types. Evaluation result is assigned to value member.
	/// @tparam T Class with function to be applied. Must define template<typename...> constexpr auto Apply(). Tuple types will be passed by parameter pack.
	/// @tparam Tpl Tuple with types on which logic will be applied.
	template <typename T, typename Tpl>
	class Apply
	{
		template <size_t ...Is>
		constexpr static auto ApplyImpl(std::index_sequence<Is...>)
		{
			return T::template Apply<std::tuple_element_t<Is, Tpl>...>();
		}

	public:
		constexpr static auto value = ApplyImpl(std::make_index_sequence<std::tuple_size<Tpl>::value>{});
	};
}

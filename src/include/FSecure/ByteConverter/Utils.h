#pragma once

#include <string_view>
#include <type_traits>
#include <utility>
#include <functional>
#include <array>
#include <vector>
#include <tuple>


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

	/// Template to evaluate if T is one of Ts types.
	template <typename T, typename ...Ts>
	struct IsOneOf
	{
		constexpr static bool value = [](bool ret) { return ret; }((std::is_same_v<T, Ts> || ...));
	};

	/// Template to evaluate all of Ts are equal to T.
	template <typename T, typename ...Ts>
	struct IsSame
	{
		constexpr static bool value = [](bool ret) { return ret; }((std::is_same_v<T, Ts> && ...));
	};

	/// Template to strip type out of const, volatile and reference.
	template <typename T>
	using RemoveCVR = std::remove_cv_t<std::remove_reference_t<T>>;

	/// Namespace for internal implementation.
	namespace Impl
	{
		template <typename T>
		struct IsTuple : std::false_type {};

		template<typename ...T>
		struct IsTuple<std::tuple<T...>> : std::true_type {};

		template<typename ...T>
		struct IsTuple<std::pair<T...>> : std::true_type {};

		template<typename T>
		struct IsPair : std::false_type {};

		template<typename ...T>
		struct IsPair<std::pair<T...>> : std::true_type {};

		template <typename T, typename = void>
		struct IsView : std::false_type {};

		template <typename T>
		struct IsView<std::basic_string_view<T>, void> : std::true_type {};
	}

	/// Idiom for detecting tuple.
	template <typename T>
	struct IsTuple : Impl::IsTuple<T> {};

	/// Idiom for detecting pair.
	template<typename T>
	struct IsPair : Impl::IsPair<T> {};

	/// Check if type is designed to view data owned by other container.
	template <typename T>
	struct IsView : Impl::IsView<T> {};

	/// Namespace full of helpers for containers template programing.
	namespace Container
	{
		/// Get type stored by container that uses iterators.
		template <typename T>
		using StoredValue = RemoveCVR<decltype(*begin(std::declval<T>()))>;

		/// Namespace for internal implementation.
		namespace Impl
		{
			template <typename T, typename = void>
			struct IsIterable : std::false_type {};

			template <typename T>
			struct IsIterable<T, std::void_t<decltype(begin(std::declval<T>()), end(std::declval<T>()))>> : std::true_type {};

			template <typename T, typename = void>
			struct HasDedicatedSize : std::false_type {};

			template <typename T>
			struct HasDedicatedSize<T, std::void_t<decltype(size(std::declval<T>()))>> : std::true_type {};

			template <typename T, typename = void>
			struct HasInsert : std::false_type {};

			template <typename T>
			struct HasInsert<T, std::void_t<decltype(std::declval<T>().insert(begin(std::declval<T>()), *end(std::declval<T>())))>> : std::true_type {};

			template <typename T, typename = void>
			struct HasReserve : std::false_type {};

			template <typename T>
			struct HasReserve<T, std::void_t<decltype(std::declval<T>().reserve(size_t{}))>> : std::true_type {};
		}

		/// Check if type can be iterated with begin() and end().
		template <typename T>
		struct IsIterable : Impl::IsIterable<T> {};

		/// Check if type have own implementation of size method.
		template <typename T>
		struct HasDedicatedSize : Impl::HasDedicatedSize<T> {};

		/// Check if type have insert(iterator, value) function.
		template <typename T>
		struct HasInsert : Impl::HasInsert<T> {};

		/// Check if type can reserve some space to avoid reallocations.
		template <typename T>
		struct HasReserve : Impl::HasReserve<T> {};

		/// Returns number of elements in container, if size(T const&), or pair of begin(T const&), end(T const&) functions can be found.
		struct Size
		{
			template <typename T>
			auto operator () (T const& obj) const -> std::enable_if_t<HasDedicatedSize<T>::value || IsIterable<T>::value, size_t>
			{
				size_t count = 0;
				if constexpr (HasDedicatedSize<T>::value) count = size(obj);
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
		struct Generator<T, std::enable_if_t<HasInsert<T>::value>>
		{
			/// Form with queued access to each of container values.
			/// @param size. Defines numbers of elements in constructed container.
			/// @param next. Functor returning one of container values at a time.
			T operator()(uint32_t size, std::function<StoredValue<T>()> next)
			{
				T ret;
				if constexpr (HasReserve<T>::value)
					ret.reserve(size);

				for (auto i = 0u; i < size; ++i)
					ret.insert(ret.end(), next());

				return ret;
			}
		};

		/// Generator for any container that is simmilar to std::basic_string_view.
		template <typename T>
		struct Generator<T, std::enable_if_t<IsView<T>::value>>
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

		/// Determines how data is accessed to create container.
		enum class AccessType
		{
			unknown,
			directMemory,
			queued,
		};

		/// Namespace for internal implementation.
		namespace Impl
		{
			namespace SignatureConcept
			{
				template <typename T, typename = void>
				struct Direct
					: std::false_type {};

				template <typename T>
				struct Direct<T, decltype(void(Generator<T>{}(uint32_t{}, std::declval<const char**>())))>
					: std::true_type {};

				template <typename T, typename = void>
				struct Queued
					: std::false_type {};

				template <typename T>
				struct Queued<T, decltype(void(Generator<T>{}(uint32_t{}, std::function<StoredValue<T>()>{})))>
					: std::true_type {};
			}

			template<AccessType V>
			using AccessTypeConstant = std::integral_constant<AccessType, V>;

			template <typename T, typename = void>
			struct GeneratorSignature
				: AccessTypeConstant<AccessType::unknown> {};

			template <typename T>
			struct GeneratorSignature<T, std::enable_if_t<SignatureConcept::Direct<T>::value>>
				: AccessTypeConstant<AccessType::directMemory> {};

			template <typename T>
			struct GeneratorSignature<T, std::enable_if_t<!SignatureConcept::Direct<T>::value && SignatureConcept::Queued<T>::value>>
				: AccessTypeConstant<AccessType::queued> {};
		}

		/// Used to detect form of Generator operator() at compile time.
		template <typename T>
		struct GeneratorSignature
		{
			using type = AccessType;
			static constexpr auto value = Impl::GeneratorSignature<T>::value;
		};
	}

	/// Namespace for internal implementation
	namespace Impl
	{
		/// @brief Default implementation of class checking if it is worth to take reference or copy by value
		/// Represents false.
		template<typename T, typename = void>
		struct WorthAddingConstRef : std::false_type {};

		/// @brief Specialization of class checking if it is worth to take reference or copy by value.
		/// Represents true.
		template<typename T>
		struct WorthAddingConstRef<T,
			std::enable_if_t<!std::is_rvalue_reference_v<T> && (!std::is_trivial_v<std::remove_reference_t<T>> || (sizeof(T) > sizeof(long long)))>>
			: std::true_type {};

		/// @brief Default implementation of class declaring simpler type to use based on T type.
		/// Represents copy by value.
		template<typename T, typename = void>
		struct AddConstRefToNonTrivial
		{
			using type = std::remove_reference_t<T>;
		};

		/// @brief Specialization of class declaring simpler type to use based on T type.
		/// Represents copy by reference.
		template<typename T>
		struct  AddConstRefToNonTrivial<T, std::enable_if_t<WorthAddingConstRef<T>::value>>
		{
			using type = std::remove_reference_t<T> const&;
		};
	}

	/// @brief Class checking if it is worth to take reference or copy by value.
	template<typename T>
	struct WorthAddingConstRef : Impl::WorthAddingConstRef<T> {};

	/// @brief Simplified WorthAddingConstRef<T>::value.
	template<typename T>
	static constexpr auto WorthAddingConstRefV = WorthAddingConstRef<T>::value;

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
	struct Apply
	{
	private:
		template <size_t ...Is>
		constexpr static auto ApplyImpl(std::index_sequence<Is...>)
		{
			return T::template Apply<std::tuple_element_t<Is, Tpl>...>();
		}
	public:
		constexpr static auto value = ApplyImpl(std::make_index_sequence<std::tuple_size<Tpl>::value>{});
	};
}

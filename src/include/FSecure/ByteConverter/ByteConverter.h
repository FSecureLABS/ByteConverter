#pragma once

#include <filesystem>
#include <variant>

#include "ByteView.h"

/// specializations for ByteConverter for common types.
namespace FSecure
{
	/// ByteConverter specialization for arithmetic types.
	template <typename T>
	requires std::is_arithmetic_v<T>
	struct ByteConverter<T>
	{
		/// Serialize arithmetic type to ByteVector.
		/// @param obj. Object to be serialized.
		/// @param bv. ByteVector to be expanded.
		static void To(T obj, ByteVector& bv)
		{
			auto oldSize = bv.size();
			bv.resize(oldSize + Size());
			*reinterpret_cast<T*>(bv.data() + oldSize) = obj;
		}

		/// Get size required after serialization.
		/// @return size_t. Number of bytes used after serialization.
		constexpr static size_t Size()
		{
			return sizeof(T);
		}

		/// Deserialize from ByteView.
		/// @param bv. Buffer with serialized data.
		/// @return arithmetic type.
		static T From(ByteView& bv)
		{
			if (sizeof(T) > bv.size())
				BYTE_CONVERTER_THROW(std::out_of_range{ OBF(": Cannot read size from ByteView ") });

			T ret;
			memcpy(&ret, bv.data(), Size());
			bv.remove_prefix(Size());
			return ret;
		}
	};

	/// ByteConverter specialization for enum.
	template <typename T>
	requires std::is_enum_v<T>
	struct ByteConverter<T>
	{
		/// Serialize enum type to ByteVector.
		/// @param enumInstance. Object to be serialized.
		/// @param bv. ByteVector to be expanded.
		static void To(T enumInstance, ByteVector& bv)
		{
			bv.Store(static_cast<std::underlying_type_t<T>>(enumInstance));
		}

		/// Get size required after serialization.
		/// @return size_t. Number of bytes used after serialization.
		constexpr static size_t Size()
		{
			return sizeof(std::underlying_type_t<T>);
		}

		/// Deserialize from ByteView.
		/// @param bv. Buffer with serialized data.
		/// @return enum.
		static T From(ByteView& bv)
		{
			return static_cast<T>(bv.Read<std::underlying_type_t<T>>());
		}
	};

	/// ByteConverter specialization for iterable types.
	template <typename T>
	requires Utils::Container::IsIterable<T>
	struct ByteConverter<T>
	{
		/// Serialize iterable type to ByteVector.
		/// @param obj. Object to be serialized.
		/// @param bv. ByteVector to be expanded.
		static void To(T const& obj, ByteVector& bv)
		{
			if (auto numberOfElements = Utils::Container::Size{}(obj); numberOfElements <= std::numeric_limits<uint32_t>::max())
				bv.Write(static_cast<uint32_t>(numberOfElements));
			else
				BYTE_CONVERTER_THROW(std::out_of_range{ OBF(": Cannot write size to ByteVector ") });

			for (auto&& e : obj)
				bv.Write(e);
		}

		/// Get size required after serialization.
		/// @return size_t. Number of bytes used after serialization.
		static size_t Size(T const& obj)
		{
			using Element = Utils::Container::StoredValue<T>;
			using Deduction = typename Detail::ConverterDeduction<Element>::FunctionSize;
			auto ret = sizeof(uint32_t);
			if constexpr (Deduction::value == Deduction::type::compileTime)
				ret += ByteConverter<Element>::Size() * obj.size(); // avoid extra calls when size of stored type is known at compile time
			else
				for (auto const& e : obj)
					ret += ByteVector::Size(e);

			return ret;
		}

		/// Deserialize from ByteView.
		/// Writing to ByteVector is similar for any iterable type, but container construction can heavily differ.
		/// Utils::Container::Generator is used for unification of this process.
		/// @param bv. Buffer with serialized data.
		/// @return iterable type.
		static T From(ByteView& bv)
		{
			using Signature = Utils::Container::GeneratorSignature<T>;
			using Generator = Utils::Container::Generator<T>;
			using Element = Utils::Container::StoredValue<T>;

			if (sizeof(uint32_t) > bv.size())
				BYTE_CONVERTER_THROW(std::out_of_range{ OBF(": Cannot read size from ByteView ") });

			if constexpr (Signature::value == Signature::type::queued)
			{
				return Generator{}(bv.Read<uint32_t>(), [&bv] { return bv.Read<Element>(); } );
			}
			else if constexpr (Signature::value == Signature::type::directMemory)
			{
				auto size = bv.Read<uint32_t>();
				auto data = bv.data();
				auto ret = Generator{}(size, reinterpret_cast<const char**>(&data));
				bv.remove_prefix(data - bv.data());
				return ret;
			}

			static_assert(Signature::value != Signature::type::unknown, "Unable to find container generator for provided type");
		}
	};

	/// ByteConverter specialization for std::filesystem::path.
	template <>
	struct ByteConverter<std::filesystem::path>
	{
		/// Serialize path type to ByteVector.
		/// @param pathInstance. Object to be serialized.
		/// @param bv. ByteVector to be expanded.
		static void To(std::filesystem::path const& pathInstance, ByteVector& bv)
		{
			bv.Store(pathInstance.wstring());
		}

		/// Get size required after serialization.
		/// @param pathInstance. Instance for which size should be found.
		/// @return size_t. Number of bytes used after serialization.
		static size_t Size(std::filesystem::path const& pathInstance)
		{
			return ByteVector::Size(pathInstance.wstring());
		}

		/// Deserialize from ByteView.
		/// @param bv. Buffer with serialized data.
		/// @return std::filesystem::path.
		static std::filesystem::path From(ByteView& bv)
		{
			return { bv.Read<std::wstring>() };
		}
	};

	/// ByteConverter specialization for std::variant
	template <typename ...Ts>
	class ByteConverter<std::variant<Ts...>>
	{
		/// @brief Variant type.
		using VarT = std::variant<Ts...>;

		/// @brief compilation time size constant.
		template <size_t size>
		using ConstSize = std::integral_constant<size_t, size>;

		/// @brief Calls provided function with dynamic index transformed into integral constant.
		/// Using Index template will generate series of functions, from Idx to MaxIdx,
		/// that will recursively call next one, until dynamic index matches static one.
		/// @tparam Idx static index.
		/// @param idx dynamic index.
		/// @param func function that will perform actual logic with matched index.
		/// @throws std::runtime_error if dynamic index is greater than MaxIdx.
		template<size_t Idx = 0, typename Func>
		static auto Index([[maybe_unused]] size_t idx, Func&& func) -> decltype(func(ConstSize<0>{}))
		{
			if constexpr (Idx < std::variant_size_v<VarT>)
				return idx == Idx ? func(ConstSize<Idx>{}) : Index<Idx + 1>(idx, std::forward<Func>(func));
			else
				BYTE_CONVERTER_THROW(std::runtime_error{ OBF("Index out of bounds") });
		}

	public:
		/// Serialize variant type to ByteVector.
		/// @param var. Object to be serialized.
		/// @param bv. ByteVector to be expanded.
		static void To(VarT const& var, ByteVector& bv)
		{
			bv.Store(var.index());
			bv.Concat(std::visit([](auto&& arg) { return ByteVector::Create(arg); }, var));
		}

		/// Get size required after serialization.
		/// @param var. Instance for which size should be found.
		/// @return size_t. Number of bytes used after serialization.
		static size_t Size(VarT const& var)
		{
			return sizeof(size_t) + std::visit([](auto&& arg) { return ByteVector::Size(arg); }, var);
		}

		/// Deserialize from ByteView.
		/// @param bv. Buffer with serialized data.
		/// @return std::variant.
		static VarT From(ByteView& bv)
		{
			auto idx = bv.Read<size_t>();
			return Index(idx, [&](auto idx) { return VarT(std::in_place_index<idx()>, bv.Read<std::variant_alternative_t<idx(), VarT>>()); });
		}
	};

	/// Tag allowing reading N bytes from ByteView.
	/// Provides simpler way of joining multiple reads than ByteView::Reed(size_t).
	/// @code someByteViewObject.Read<int, int, Bytes<7>, std::string> @endCode
	/// Size must be known at compile time.
	/// Using Bytes class in read will return ByteView, or ByteVector, depending on HardCopy template argument.
	template<size_t N, bool HardCopy = false>
	class Bytes
	{
		/// This class should never be instantiated.
		Bytes() = delete;
		static constexpr bool Copy = HardCopy;
	};

	/// Tag always coping data.
	template<size_t N>
	using BytesCopy = Bytes<N, true>;

	/// ByteConverter specialization for FSecure::Bytes.
	template <size_t N, bool HardCopy>
	struct ByteConverter<Bytes<N, HardCopy>>
	{
		/// Retrieve ByteView substring with requested size.
		/// @param bv. Buffer with serialized data. Will be moved by N bytes.
		/// @return ByteView new view with size equal to N.
		static auto From(ByteView& bv) -> std::conditional_t<HardCopy, ByteVector, ByteView>
		{
			if (N > bv.size())
				BYTE_CONVERTER_THROW(std::out_of_range{ OBF(": Cannot read data from ByteView") });

			auto retVal = bv.SubString(0, N);
			bv.remove_prefix(N);
			return retVal;
		}
	};

	/// Contains internal logic of FSecure classes and functions.
	namespace Detail
	{
		/// Concepts determining if type provides generic access to members.
		namespace MemberGenericAccess
		{
			/// Check if type provides generic access to one element by index.
			template <typename T, size_t N>
			concept OneElement = requires(T t)
			{
				typename std::tuple_element<N, T>::type;
				std::get<N>(t);
			};

			/// Function checking if type provides generic access to each element specified by index sequence.
			template<typename T, size_t... Is>
			constexpr bool CanAccessEachElement(std::index_sequence<Is...>)
			{
				return (OneElement<T, Is> && ...);
			}

			/// Check if type provides generic access to members, by supporting std::get and std::tuple_size.
			template <typename T>
			concept EachElement = requires(T t)
			{
				std::tuple_size<T>::value;
				requires CanAccessEachElement<T>(std::make_index_sequence<std::tuple_size<T>::value>{});
			};
		}

		/// Helper class responsible for handling type with generic access like tuple.
		/// @tparam T. Tuple like type to read/write.
		/// @tparam N. How many elements should be handled. Functions will use recursion, decrementing N with each call.
		template <typename T, size_t N = std::tuple_size_v<T>>
		struct TupleHandler
		{
			/// Function responsible for recursively packing data to ByteVector.
			/// @param bv. Reference to ByteVector object using TupleHandler.
			/// @param t. reference to tuple.
			static void Write([[maybe_unused]] ByteVector& bv, [[maybe_unused]] T const& tpl)
			{
				if constexpr (N != 0)
				{
					bv.Write(std::get<std::tuple_size_v<T> - N>(tpl));
					TupleHandler<T, N - 1>::Write(bv, tpl);
				}
			}

			/// Function responsible for recursively calculating buffer size needed for call with tuple argument.
			/// @param t. reference to tuple.
			/// @return size_t number of bytes needed.
			static size_t Size([[maybe_unused]] T const& tpl)
			{
				if constexpr (N != 0)
					return ByteVector::Size(std::get<std::tuple_size_v<T> - N>(tpl)) + TupleHandler<T, N - 1>::Size(tpl);
				else
					return 0;
			}

			/// Function responsible for recursively packing data to tuple.
			/// @param bv. Reference to ByteView object using generate method.
			static auto Read(ByteView& bv)
			{
				if constexpr (N != 0)
				{
					auto current = std::make_tuple(bv.Read<std::remove_cvref_t<std::tuple_element_t<std::tuple_size_v<T> - N, T>>>());
					auto rest = TupleHandler<T, N - 1>::Read(bv);
					return std::tuple_cat(std::move(current), std::move(rest));
				}
				else
				{
					return std::tuple<>{};
				}
			}
		};

		/// Tuple handler is using recursion and tuple concatenation to work.
		/// Concepts within this namespace can determine, if tuple constructed by TupleHandler::From, can be transformed back to desired type.
		namespace TupleHandlerCompatible
		{
			/// Desired type allows conversion from tuple.
			template <typename T>
			concept Convertible = requires(ByteView bv)
			{
				{ TupleHandler<T>::Read(bv) } -> std::convertible_to<T>;
			};

			/// Desired type provides constructor, taking all tuple members as arguments.
			template <typename T>
			concept Constructible = requires(ByteView bv)
			{
				{ make_from_tuple<T>(TupleHandler<T>::Read(bv)) } -> std::same_as<T>;
			};

			/// Desired type can be transformed form tuple.
			template <typename T>
			concept Transformable = Convertible<T> || Constructible<T>;
		}
	}

	/// ByteConverter specialization for tuple.
	template <typename T>
	requires (!Utils::Container::IsIterable<T>) && Detail::MemberGenericAccess::EachElement<T> && Detail::TupleHandlerCompatible::Transformable<T>
	struct ByteConverter<T>
	{
		/// Serialize tuple type to ByteVector.
		/// @param tupleInstance. Object to be serialized.
		/// @param bv. ByteVector to be expanded.
		static void To(T const& tupleInstance, ByteVector& bv)
		{
			Detail::TupleHandler<T>::Write(bv, tupleInstance);
		}

		/// Get size required after serialization.
		/// @param tupleInstance. Instance for which size should be found.
		/// @return size_t. Number of bytes used after serialization.
		static size_t Size(T const& tupleInstance)
		{
			return Detail::TupleHandler<T>::Size(tupleInstance);
		}

		/// Deserialize from ByteView.
		/// @param bv. Buffer with serialized data.
		/// @return T.
		/// This function must be capable to transform tuple back to T type.
		/// For example C++ allows cast from pair to tuple of two, but not other way around.
		/// If type cannot be converted, function will use tuple members to initialize new object.
		///
		/// @note make_from_tuple requires constructor.
		/// If author specified tuple-like interface for their class, it is safe to assume constructor is also present.
		/// This less restrictive form might be used if the assumption above turns out to be incorrect.
		/// return apply(Utils::Construction::Braces<T>{}, std::move(tmp));
		static T From(ByteView& bv)
		{
			auto tmp = Detail::TupleHandler<T>::Read(bv);
			if constexpr (std::is_convertible_v<decltype(tmp), T>)
				return tmp;
			else
				return make_from_tuple<T>(std::move(tmp));
		}
	};

	/// @brief Class providing simple way of generating ByteConverter of custom types by treating them as tuple.
	/// ByteConverter can use this functionality by inheriting from TupleConverter and providing
	/// public static std::tuple<...> Convert(T const&) method.
	/// Use Utils::MakeConversionTuple to create efficient tuple of value or references to members.
	/// ByteVector can declare its own versions of To/From/Size methods if it needs dedicated logic to serialize type.
	/// @tparam T Type for serialization.
	template <typename T>
	class TupleConverter
	{
		/// @brief Type returned by ByteConverter<C>::Convert.
		/// ByteConverter<C> specialization may not yet be defined.
		/// This type must be deduced late in instantiation procedure.
		/// @tparam C Type for serialization.
		template <typename C>
		using ConvertType = decltype(ByteConverter<C>::Convert(std::declval<C>()));

		/// @brief Type responsible for removing qualifiers from tuple template parameters.
		/// This structure is only defined for tuple types.
		template <typename C>
		struct EraseQualifiers;

		/// @brief Type responsible for removing qualifiers from tuple template parameters.
		/// @tparam ...Cs tuple template parameters.
		template <typename ...Cs>
		struct EraseQualifiers<std::tuple<Cs...>>
		{
			using type = std::tuple<std::remove_cvref_t<Cs>...>;
		};

		/// @brief Helper class compatible with Utils::Apply. Checks if size of all types have constexpr Size() function.
		class IsSizeConstexpr
		{
			template <typename C>
			using Deduction = typename Detail::ConverterDeduction<C>::FunctionSize;

		public:
			template <typename ...Ts>
			static constexpr auto Apply()
			{
				return ((Deduction<Ts>::value == Deduction<Ts>::type::compileTime) && ...);
			}
		};

		/// @brief Chceck if TupleConverter<C>::Convert return type, have Size that can be determined at compilation time.
		/// @tparam C Type to be tested.
		template <typename C>
		using ConvertHaveConstexprSize = Utils::Apply<IsSizeConstexpr, ConvertType<C>>;

		/// @brief Helper class compatible with Utils::Apply. Determines size after serialization, of all tuple types.
		struct GetConstexprSize
		{
			template <typename ...Ts>
			static constexpr auto Apply() -> size_t
			{
				if constexpr (sizeof...(Ts) != 0)
					return (ByteConverter<Ts>::Size() + ...);
				else
					return 0;
			}
		};

	public:
		/// @brief Use it to convert raw data into tuple.
		/// Allows splitting deserialization into two phases.
		/// 1. Retrieve data as tuple. ByteView internal pointer will be correctly moved in the process.
		/// 2. Create dedicated logic of transforming tuple into desired type.
		/// @param bv. Buffer with serialized data.
		/// @return tuple of retrieved data for type construction.
		static auto Convert(ByteView& bv)
		{
			return bv.Read<typename EraseQualifiers<ConvertType<T>>::type>();
		}

		// From this point forward will be implemented ByteConverter standard interface methods.

		/// @brief Default implementation of To method.
		/// Serializes data treating it as tuple generated by Convert.
		/// @param obj Object for serialization.
		/// @param bv output ByteVector with already allocated memory for data.
		static void To(T const& obj, ByteVector& bv)
		{
			bv.Store(ByteConverter<T>::Convert(obj));
		}

		/// @brief Default implementation of From method.
		/// Retrieves data from view and creates new object by passing tuple as arguments for T{...} construction.
		/// This implementation uses brace enclosed construction, becouse std::make_from_tuple does not support trivial types.
		/// Bear in mind that construction with parentheses, and with braces, is not interchangeable.
		/// @param bv. Buffer with serialized data.
		/// @return constructed type.
		static T From(ByteView& bv)
		{
			return std::apply(Utils::Construction::Braces<T>{}, Convert(bv));
		}

		/// @brief Default implementation of Size method with compile time evaluation.
		/// @note All template parameters are used only to determine if function should be defined.
		/// @return size_t. Number of bytes used after serialization.
		template <typename C = T>
		requires ConvertHaveConstexprSize<C>::value
		static constexpr size_t Size()
		{
			return Utils::Apply<GetConstexprSize, ConvertType<T>>::value;
		}

		/// @brief Default implementation of Size method.
		/// @param obj Object for serialization.
		/// @note All template parameters are used only to determine if function should be defined.
		/// @return size_t. Number of bytes used after serialization.
		template <typename C = T>
		requires (!ConvertHaveConstexprSize<C>::value)
		static size_t Size(T const& obj)
		{
			return ByteVector::Size(ByteConverter<T>::Convert(obj));
		}
	};

	/// @brief Class providing simple way of generating ByteConverter listing only necessary members once.
	/// ByteConverter can use this functionality by inheriting from PointerTupleConverter and providing
	/// public static std::tuple<T::*...> MemberPointers() method.
	/// Use std::make_tuple to create return value out of member list.
	/// ByteVector can declare its own versions of To/From/Size methods if it needs dedicated logic to serialize type.
	/// @tparam T Type for serialization.
	template <typename T>
	struct PointerTupleConverter : TupleConverter<T>
	{
	private:
		/// @brief Class applying pointers to members to object.
		/// Compatible with std::apply.
		/// Creates TupleConverter<T>::ConvertType object.
		class ReferenceMembers
		{
			T const& m_Obj;
		public:
			ReferenceMembers(T const& obj) : m_Obj{ obj } {}

			template <typename ... Ts>
			auto operator () (Ts /*T::**/...ts) const
			{
				return Utils::MakeConversionTuple(m_Obj.*ts ...);
			}
		};

		/// @brief Function assigning object members with tuple of values using tuple member pointers.
		/// @tparam PtrTpl Tuple of pointers to members type.
		/// @tparam ValueTpl Tuple of values type.
		/// @tparam Is Index sequence used to match tuples one to one.
		/// @param obj Object to be assigned.
		/// @param ptrTpl Tuple of pointers to members.
		/// @param valueTpl Tuple of values.
		template <typename PtrTpl, typename ValueTpl, size_t ...Is>
		static void AssignToMembers(T& obj, PtrTpl const& ptrTpl, ValueTpl valueTpl, std::index_sequence<Is...>)
		{
			((obj.*(std::get<Is>(ptrTpl)) = std::move(std::get<Is>(valueTpl))), ...);
		}

	public:
		/// @brief Default implementation of Convert method used by TupleConverter for serialization.
		/// Function uses ByteConverter<T>::MemberPointers() to reference T object members.
		/// @param obj object to be serialized.
		/// @return TupleConverter<T>::ConvertType values to be serialized.
		static auto Convert(T const& obj)
		{
			auto ptrTpl = ByteConverter<T>::MemberPointers();
			return std::apply(ReferenceMembers{ obj }, ptrTpl);
		}

		/// @brief Shadowed TupleConverter<T>::From initalizing only selected members, skipped ones will be default initialized.
		/// @note T must have default constructor.
		/// @param bv. Buffer with serialized data.
		/// @return constructed type.
		static T From(ByteView& bv)
		{
			auto ret = T{};
			auto ptrTpl = ByteConverter<T>::MemberPointers();
			auto valueTpl = TupleConverter<T>::Convert(bv);
			AssignToMembers(ret, ptrTpl, std::move(valueTpl), std::make_index_sequence<std::tuple_size<decltype(ptrTpl)>::value>{});
			return ret;
		}
	};
}

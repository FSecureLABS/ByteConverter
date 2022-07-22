#include "FSecure/ByteConverter/ByteConverter.h"

#include <iostream>
#include <string>

using namespace FSecure::Literals;

struct A
{
	uint16_t m_a;
	uint32_t m_b;
};

namespace FSecure
{
	/// Example code of specializing ByteConverter for custom type A.
	template <>
	struct ByteConverter<A>
	{
		/// Function transforming data to ByteVector.
		static ByteVector To(A const& a)
		{
			return ByteVector::Create(a.m_a, a.m_b);
		}

		/// Informs how many bytes object will take after serialization.
		static size_t Size(A const& a)
		{
			return ByteVector::Size(a.m_a, a.m_b);
		}

		/// Function deserializing data from ByteView and generating new object of desired type.
		static A From(ByteView& bv)
		{
			auto [a, b] = bv.Read<uint16_t, uint32_t>();
			return A{ a, b };
		}
	};
}

struct B : A {};

namespace FSecure
{
	/// Example code of specializing ByteConverter for custom type B.
	/// B have the same members as A, but this ByteConverter will use alternative function versions.
	template <>
	struct ByteConverter<B>
	{
		/// Void versions of function can be defined to avoid data reallocation.
		/// ByteVector& will have enough capacity to write object into it.
		/// This form requires function Size to be defined.
		static void To(B const& b, ByteVector& bv)
		{
			/// ByteConverter have access to function Store as a friend of ByteVector. Function should be used in place of Write, to increase performance when memory is already allocated.
			bv.Store(b.m_a, b.m_b);
		}

		/// Constexpr parameterless version of function can be used if Size is known at compile time
		constexpr static size_t Size()
		{
			return sizeof(uint16_t) + sizeof(uint32_t);
		}

		/// ByteReader will tie arguments, and assign values from reading their type from ByteView.
		static B From(ByteView& bv)
		{
			/// Object must be created before Read call.
			auto b = B{};
			ByteReader{ bv }.Read(b.m_a, b.m_b);
			return b;
		}
	};
}

struct C
{
	uint16_t m_a;
	std::string m_b;
};

namespace FSecure
{
	/// Example code for specializing ByteConverter using TupleConverter helper.
	/// TupleConverter uses void version of serializing method to avoid data reallocation.
	/// Versions of To/Size/From provided by TupleConverter can be shadowed by dedicated version.
	template <>
	struct ByteConverter<C> : TupleConverter<C>
	{
		/// @brief In order for TupleConverter to work only Convert method must be defined.
		/// It allows quick creation of ByteConverter by listing all data needed to be serialized.
		/// @param obj Object to be serialized.
		/// @return auto tuple type allowing already declared ByteConverter for tuple to handle serialization.
		static auto Convert(C const& obj)
		{
			/// Function detecting if tuple should use value or reference, to avoid costly copies, or counterproductive reference to trivial types.
			/// Please remember that ByteView::Read<std::tuple<T1, T2 const&>>() -> std::tuple<T1, T2> for correct data menagment.
			return Utils::MakeConversionTuple(obj.m_a, obj.m_b);
		}
	};
}

struct D
{
	uint16_t m_a;
	std::string m_b = "String we would like to not serialize.";
	uint32_t m_c;
};

namespace FSecure
{
	/// Example code for specializing ByteConverter using PointerTupleConverter helper.
	/// This class is designed to provide Convert and modified From function for TupleConverter using member pointers.
	/// This approach will serialize/initialize only desired members, leaving skipped to default initialization.
	/// Versions of To/Size/From provided by PointerTupleConverter can be shadowed by dedicated version.
	template <>
	struct ByteConverter<D> : PointerTupleConverter<D>
	{
		static auto MemberPointers()
		{
			return std::make_tuple(&D::m_a, &D::m_c);
		}
	};
}

template <typename ...Ts>
void Print(Ts ...ts)
{
	((std::cout << ts << ' '), ...) << std::endl;
}

int main()
{
	// Create ByteVector with two serialized objects.
	auto bv = FSecure::ByteVector::Create(A{ 7, 13 }, std::string{ "foo bar" }, C{ 17, "suf" });

	// Write some more.
	bv.Write(std::filesystem::current_path());

	// Read directly from ByteVector.
	auto [a, string, c, path] = FSecure::ByteView{ bv }.Read<A, std::string, C, std::filesystem::path>();

	// Create ByteView. Underlying memory is owned by ByteVector.
	auto view = FSecure::ByteView{ bv };

	// Read from view. This operation moves ByteView to next object, allowing chaining Read operations.
	auto [a_a, a_b, string2] = view.Read<uint16_t, uint32_t, std::string>();
	auto b2 = view.Read<C>();
	auto path2 = view.Read<std::filesystem::path>();

	// Write data, skipping "Useless string".
	auto bv2 = FSecure::ByteVector::Create(D{ 17, "Useless string", 111 });

	// Read data, skipped member will be initialized with default value.
	auto d = FSecure::ByteView{ bv2 }.Read<D>();

	Print(a.m_a, a.m_b, string2, c.m_a, b2.m_b, path2.string(), d.m_a, d.m_c);
}

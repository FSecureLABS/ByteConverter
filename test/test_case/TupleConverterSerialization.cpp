#include "catch2/catch.hpp"

#include "CustomType.h"
#include "FSecure/ByteConverter/ByteConverter.h"

using namespace FSecure;

namespace TupleConverterSerialization
{
	struct SimpleType
	{
		uint16_t a;
		uint32_t b;
	};

	struct AdvancedType : TestFixture::CustomType
	{
		std::string extraMember = RndStr();

		bool operator==(AdvancedType const& other) const
		{
			return extraMember == other.extraMember
				&& static_cast<TestFixture::CustomType const&>(*this) == static_cast<TestFixture::CustomType const&>(other);
		}
	};

	struct SuperAdvancedType
	{
		AdvancedType advancedType;

		bool operator==(SuperAdvancedType const& other) const
		{
			return advancedType == other.advancedType;
		}
	};

	static uint8_t ToFunctionCalls = 0;
	static uint8_t FromFunctionCalls = 0;
}

namespace FSecure
{
	using namespace TupleConverterSerialization;

	template <>
	struct ByteConverter<SimpleType> : TupleConverter<SimpleType>
	{
		static auto Convert(SimpleType const& obj)
		{
			return Utils::MakeConversionTuple(
				obj.a,
				obj.b
			);
		}
	};

	template <>
	struct ByteConverter<AdvancedType> : TupleConverter<AdvancedType>
	{
		static auto Convert(AdvancedType const& obj)
		{
			return Utils::MakeConversionTuple(
				obj.number,
				obj.enumerable,
				obj.string,
				obj.wstring,
				obj.path,
				obj.tuple,
				obj.array,
				obj.hashmap,
				obj.vector,
				obj.variant,
				obj.extraMember
			);
		}
	};

	template <>
	struct ByteConverter<SuperAdvancedType> : TupleConverter<SuperAdvancedType>
	{
		static auto Convert(SuperAdvancedType const& obj)
		{
			return Utils::MakeConversionTuple(
				obj.advancedType
			);
		}

		static void To(SuperAdvancedType const& obj, ByteVector& bv)
		{
			++ToFunctionCalls;
			TupleConverter<SuperAdvancedType>::To(obj, bv);
		}

		static SuperAdvancedType From(ByteView& bv)
		{
			++FromFunctionCalls;
			return TupleConverter<SuperAdvancedType>::From(bv);
		}
	};
}

TEST_CASE("TupleConverter serialization.")
{
	AdvancedType advancedType[2];
	SuperAdvancedType superAdvancedType;
	constexpr static size_t simpleTypeExpectedSize = sizeof(SimpleType::a) + sizeof(SimpleType::b);

	SECTION("Serialized size is constexpr when possible.")
	{
		constexpr auto size = ByteConverter<SimpleType>::Size();
		REQUIRE(size == simpleTypeExpectedSize);
	}

	SECTION("Serialization makes no size overhead.")
	{
		auto simpleType = SimpleType{};
		auto bv = ByteVector::Create(simpleType);

		CHECK(bv.size() == simpleTypeExpectedSize);
		CHECK(bv.capacity() == simpleTypeExpectedSize);
	}

	SECTION("Serialized data are not corrupted.")
	{
		ByteVector serialized[] = { ByteVector::Create(advancedType[0]), ByteVector::Create(advancedType[1]) };
		ByteView view[] = { serialized[0], serialized[1] };
		REQUIRE_FALSE(view[0] == view[1]);
		AdvancedType deserialized[] = { view[0].Read<AdvancedType>(), view[1].Read<AdvancedType>() };
		CHECK(deserialized[0] == advancedType[0]);
		CHECK(deserialized[1] == advancedType[1]);
	}

	SECTION("Override serialization functions.")
	{
		auto serialized = ByteVector::Create(superAdvancedType);
		auto deserialized = ByteView{ serialized }.Read<SuperAdvancedType>();

		CHECK(superAdvancedType == deserialized);
		CHECK(ToFunctionCalls == 1);
		CHECK(FromFunctionCalls == 1);
	}
};

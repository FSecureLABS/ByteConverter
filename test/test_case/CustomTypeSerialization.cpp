#include "catch2/catch.hpp"

#include "FSecure/ByteConverter/ByteConverter.h"
#include "CustomType.h"
#include "Tools.h"

using namespace FSecure;

namespace CustomTypeSerialization
{
	struct CustomType : TestFixture::CustomType {};
}

namespace FSecure
{
	using namespace CustomTypeSerialization;

	template <>
	struct ByteConverter<CustomType>
	{
		static void To(CustomType const& obj, ByteVector& bv)
		{
			 bv.Store(
				obj.number,
				obj.enumerable,
				obj.string,
				obj.wstring,
				obj.path,
				obj.tuple,
				obj.array,
				obj.hashmap,
				obj.vector,
				obj.variant
			);
		}

		static size_t Size(CustomType const& obj)
		{
			return ByteVector::Size(
				obj.number,
				obj.enumerable,
				obj.string,
				obj.wstring,
				obj.path,
				obj.tuple,
				obj.array,
				obj.hashmap,
				obj.vector,
				obj.variant
			);
		}

		static CustomType From(ByteView& bv)
		{
			auto obj = CustomType{};
			ByteReader{ bv }.Read(
				obj.number,
				obj.enumerable,
				obj.string,
				obj.wstring,
				obj.path,
				obj.tuple,
				obj.array,
				obj.hashmap,
				obj.vector,
				obj.variant
			);
			return obj;
		}
	};
}

TEST_CASE("Custom type serialization.")
{
	CustomType testObject[2];

	SECTION("Test objects are valid.")
	{
		REQUIRE_FALSE(testObject[0] == testObject[1]);
	}

	SECTION("Serialized data are not corrupted.")
	{
		ByteVector serialized[] = { ByteVector::Create(testObject[0]), ByteVector::Create(testObject[1]) };
		ByteView view[] = { serialized[0], serialized[1] };
		REQUIRE_FALSE(view[0] == view[1]);
		CustomType deserialized[] = { view[0].Read<CustomType>(), view[1].Read<CustomType>() };
		REQUIRE(deserialized[0] == testObject[0]);
		REQUIRE(deserialized[1] == testObject[1]);
	}

	SECTION("One object serialization makes no size overhead.")
	{
		ByteVector serialized = ByteVector::Create(testObject[0]);
		REQUIRE(serialized.size() == ByteVector::Size(testObject[0]));
	}

	SECTION("Two objects serialization makes no size overhead.")
	{
		ByteVector serializedSeparately[] = { ByteVector::Create(testObject[0]), ByteVector::Create(testObject[1]) };
		ByteVector serializedTogether = ByteVector::Create(testObject[0], testObject[1]);
		CHECK((serializedSeparately[0].size() + serializedSeparately[1].size()) == serializedTogether.size());
	}
};

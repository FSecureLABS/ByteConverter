#include "catch2/catch.hpp"

#include "FSecure/ByteConverter/ByteConverter.h"

using namespace FSecure;

TEST_CASE("Simple type serialization.")
{
	int testObject[2] = { -44, 776 };

	SECTION("Test objects are valid.")
	{
		REQUIRE_FALSE(testObject[0] == testObject[1]);
	}

	SECTION("Serialized data are not corrupted.")
	{
		ByteVector serialized[] = { ByteVector::Create(testObject[0]), ByteVector::Create(testObject[1]) };
		ByteView view[] = { serialized[0], serialized[1] };
		REQUIRE_FALSE(view[0] == view[1]);
		int deserialized[] = { view[0].Read<int>(), view[1].Read<int>() };
		CHECK(deserialized[0] == testObject[0]);
		CHECK(deserialized[1] == testObject[1]);
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
		REQUIRE((serializedSeparately[0].size() + serializedSeparately[1].size()) == serializedTogether.size());
	}
};

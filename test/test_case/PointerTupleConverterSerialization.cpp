#include "catch2/catch.hpp"

#include "FSecure/ByteConverter/ByteConverter.h"

using namespace FSecure;

namespace PointerTupleConverterSerialization
{
	const uint32_t defaultNumber = 111;
	const std::string defaultString = "Default string";

	struct SimpleType
	{
		uint16_t a;
		uint32_t b = defaultNumber;
		uint64_t c;

		bool operator==(SimpleType const& other) const
		{
			// b can be different
			return a == other.a && c == other.c;
		}
	};

	struct AdvancedType : SimpleType
	{
		std::string b = defaultString;
	};
}

namespace FSecure
{
	using namespace PointerTupleConverterSerialization;

	template <typename T>
	struct MyConverter : PointerTupleConverter<T>
	{
		static auto MemberPointers()
		{
			return std::make_tuple(&T::a, &T::c);
		}
	};

	template <>
	struct ByteConverter<SimpleType> : MyConverter<SimpleType>
	{};

	template <>
	struct ByteConverter<AdvancedType> : MyConverter<AdvancedType>
	{};
}

TEST_CASE("PointerTupleConverter serialization.")
{
	SECTION("Serialized size is constexpr when possible.")
	{
		constexpr auto size = ByteConverter<SimpleType>::Size();
		constexpr auto simpleTypeExpectedSize = sizeof(SimpleType::a) /*+ sizeof(SimpleType::b)*/ + sizeof(SimpleType::c);
		REQUIRE(size == simpleTypeExpectedSize);
	}

	SECTION("Serialized data are not corrupted.")
	{
		auto simpleType = SimpleType{ 3, 30, 300 };
		auto advancedType = AdvancedType{ { 7, 70, 700 }, "7000" };
		auto bv = ByteVector::Create(simpleType, advancedType);
		auto [simple, advanced] = ByteView{ bv }.Read<SimpleType, AdvancedType>();

		CHECK(simpleType == simple);
		CHECK(advancedType == advanced);
		CHECK(simple.b == defaultNumber);
		CHECK(static_cast<SimpleType&>(advanced).b == defaultNumber);
		CHECK(advanced.b == defaultString);
	}
};

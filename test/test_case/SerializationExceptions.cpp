#include "catch2/catch.hpp"

#include "FSecure/ByteConverter/ByteConverter.h"
#include "Tools.h"

using namespace FSecure;

namespace SerializationExceptions
{
	struct BrokenType
	{
		bool throwAtSizeCalculation;
	};

	BrokenType BrokenSizeCalculation()
	{
		return { true };
	}

	BrokenType BrokenWrite()
	{
		return { false };
	}

	struct Exception : std::exception {};
}

namespace FSecure
{
	using namespace SerializationExceptions;

	template <>
	struct ByteConverter<BrokenType>
	{
		static ByteVector To(BrokenType const&)
		{
			throw Exception{};
		}

		static size_t Size(BrokenType const& obj)
		{
			if (obj.throwAtSizeCalculation)
				throw Exception{};

			return 0;
		}

		static BrokenType From(ByteView&)
		{
			throw Exception{};
		}
	};
}

#if (BYTE_CONVERTER_HAS_EXCEPTIONS)
	TEST_CASE("Serialization exceptions.")
	{
		SECTION("Throws during out-of-range read.")
		{
			REQUIRE_THROWS_AS( ByteView{}.Read<uint8_t>(), std::out_of_range);
		}

		SECTION("Throws correct exceptions.")
		{
			REQUIRE_THROWS_AS( ByteVector::Create(BrokenSizeCalculation()), Exception);
			REQUIRE_THROWS_AS( ByteVector::Create(BrokenWrite()), Exception);
			REQUIRE_THROWS_AS( ByteView{}.Read<BrokenType>(), Exception);
		}

		SECTION("Exception during write does not invalidate data.")
		{
			auto bv = ByteVector::Create(RndNum(), RndNum(), RndStr(), RndStr());
			auto bvAddress = &bv[0];
			auto bvCopy = bv;
			auto ensureRealocationAtWrite = ByteVector{};
			ensureRealocationAtWrite.resize(bv.capacity() + 1);

			// If exception occurs at size calculation, reallocation will not take place, leaving iterators and views valid.
			REQUIRE_THROWS_AS( bv.Write(ensureRealocationAtWrite, BrokenSizeCalculation()), Exception);
			CHECK(bv == bvCopy);
			CHECK(&bv[0] == bvAddress);

			// If exception occurs at write, iterators and views are invalidated.
			REQUIRE_THROWS_AS( bv.Write(ensureRealocationAtWrite, BrokenWrite()), Exception);
			CHECK(bv == bvCopy);
		}

		SECTION("Exception during read does not invalidate view.")
		{
			auto bv = ByteVector::Create(RndNum(), RndNum(), RndStr(), RndStr());
			auto view = ByteView{ bv };
			auto viewCopy = view;

			// View address can be modified by Read, but pointer to underlying data stays unchanged, leaving iterators valid.
			REQUIRE_THROWS_AS( (view.Read<decltype(RndNum()), BrokenType>()), Exception);

			CHECK(view == viewCopy);
			CHECK(view.begin() == viewCopy.begin());
		}
	};
#endif

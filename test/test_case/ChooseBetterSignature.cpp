#include "catch2/catch.hpp"

#include "FSecure/ByteConverter/ByteConverter.h"

using namespace FSecure;

namespace ChooseBetterSignature
{
	struct SimpleType
	{
		int number = 0;
	};
}

namespace FSecure
{
	using namespace ChooseBetterSignature;

	template <>
	struct ByteConverter<SimpleType>
	{
		static void To(SimpleType const& obj, ByteVector& bv)
		{
			bv.Store(obj.number);
		}

		static ByteVector To(SimpleType const& obj)
		{
			return ByteVector::Create(obj.number);
		}

		constexpr static size_t Size()
		{
			return sizeof(SimpleType::number);
		}

		static size_t Size(SimpleType const& obj)
		{
			return ByteVector::Size(obj.number);
		}

		static SimpleType From(ByteView& bv)
		{
			return { bv.Read<decltype(SimpleType::number)>() };
		}
	};
}

TEST_CASE("Choose better signature")
{
	using Deduction = FSecure::Detail::ConverterDeduction<SimpleType>;

	SECTION("Constexpr Size is preferred.")
	{
		[[maybe_unused]] constexpr auto size = ByteConverter<SimpleType>::Size();
		REQUIRE(Deduction::FunctionSize::value == Deduction::FunctionSize::type::compileTime);
	}

	SECTION("Avoid realocation by default")
	{
		REQUIRE(Deduction::FunctionTo::value == Deduction::FunctionTo::type::expandsContainer);
	}
}

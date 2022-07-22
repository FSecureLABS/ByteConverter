#include "catch2/catch.hpp"

#include "FSecure/ByteConverter/ByteConverter.h"
#include "Tools.h"

using namespace FSecure;

namespace QualifiersErasure
{
	using TypeA = int;
	using TypeB = std::string;

	struct SimpleType
	{
		TypeA m_a = GenerateRandomValue<TypeA>();
		TypeB m_b = GenerateRandomString(16);
	};

	struct QualifiedType
	{
		const TypeA m_a = GenerateRandomValue<TypeA>();
		const TypeB m_b = GenerateRandomString(16);
	};
}

namespace FSecure
{
	using namespace QualifiersErasure;

	template <typename T>
	struct MyConverter : TupleConverter<T>
	{
		static auto Convert(T const& obj)
		{
			return Utils::MakeConversionTuple(obj.m_a, obj.m_b);
		}
	};

	template <>
	struct ByteConverter<SimpleType> : MyConverter<SimpleType>
	{};

	template <>
	struct ByteConverter<QualifiedType> : MyConverter<QualifiedType>
	{};
}

namespace QualifiersErasure
{
	struct Fixture
	{
		SimpleType m_simple;
		QualifiedType m_qualified = { m_simple.m_a, m_simple.m_b };
		ByteVector m_serialized = ByteVector::Create(m_simple);

		template <typename ...Ts>
		void Serialize(Ts&& ...ts)
		{
			auto bv = ByteVector::Create(std::forward<Ts>(ts)...);
			REQUIRE(m_serialized == bv);
		}

		template <typename T1, typename T2>
		void Deserialize()
		{
			auto [a, b] = ByteView{ m_serialized }.Read<T1, T2>();
			static_assert(std::is_same_v<decltype(a), TypeA>, "Type deduction failed.");
			static_assert(std::is_same_v<decltype(b), TypeB>, "Type deduction failed.");
			CHECK(a == m_simple.m_a);
			CHECK(b == m_simple.m_b);
		}
	};

	TEST_CASE_METHOD(Fixture, "Serialization from qualified types.")
	{
		Serialize(m_qualified.m_a, m_qualified.m_b);
	}

	TEST_CASE_METHOD(Fixture, "Serialization is reference agnostic.")
	{
		auto copy = m_simple;
		Serialize(copy.m_a, std::move(copy.m_b));
	}

	TEST_CASE_METHOD(Fixture, "Deserialization from qualified types.")
	{
		Deserialize<decltype(m_qualified.m_a), decltype(m_qualified.m_b)>();
	}

	TEST_CASE_METHOD(Fixture, "Deserialization is reference agnostic.")
	{
		Deserialize<TypeA&, TypeB&&>();
	}

	TEST_CASE_METHOD(Fixture, "TupleConverter is compatible with qualified types.")
	{
		auto bv = ByteVector::Create(m_qualified);
		auto qualified = ByteView{ bv }.Read<QualifiedType>();
		CHECK(bv == m_serialized);
		CHECK(qualified.m_a == m_simple.m_a);
		CHECK(qualified.m_b == m_simple.m_b);
	}
};

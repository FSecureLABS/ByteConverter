#pragma once

#include "Tools.h"

#include <array>
#include <vector>
#include <unordered_map>
#include <tuple>
#include <utility>
#include <filesystem>
#include <variant>

namespace TestFixture
{

	enum class CustomEnum
	{
		foo,
		bar
	};

	struct CustomType
	{
		int number = GenerateRandomValue<int>();
		CustomEnum enumerable = CustomEnum::bar;
		std::string string = RndStr();
		std::wstring wstring = RndStr<std::wstring>();
		std::filesystem::path path = std::filesystem::current_path() / RndStr();
		std::tuple<std::string, size_t, std::string_view> tuple = { RndStr(),  RndNum(), "Known at compile time" };
		std::array<std::byte, 12> array = { std::byte{ 2 }, std::byte{ 3 }, std::byte{ 5 }, std::byte{ 7 }, std::byte{ 13 }, std::byte{ 17 }, std::byte{ 19 }, std::byte{ 31 }, std::byte{ 61 }, std::byte{ 89 }, std::byte{ 107 }, std::byte{ 127 } };
		std::unordered_map<std::string, std::string> hashmap = { { RndStr(), RndStr() }, { RndStr(), RndStr() }, { RndStr(), RndStr() }, { RndStr(), RndStr() }, { RndStr(), RndStr() }, { RndStr(), RndStr() } };
		std::vector<uint32_t> vector = { 01000100, 01101001, 01100100, 00100000, 01111001, 01101111, 01110101, 00100000, 01100101, 01110110, 01100101, 01110010, 00100000, 01101000, 01100101, 01100001 };
		using TestVariant = std::variant<size_t, std::string, std::string>;
		std::array<TestVariant, 4> variant = { RndNum(), TestVariant{std::in_place_index<1>, RndStr()}, TestVariant{std::in_place_index<2>, RndStr()}, RndNum() };

		bool operator==(CustomType const& other) const
		{
			return number == other.number
				&& enumerable == other.enumerable
				&& string == other.string
				&& wstring == other.wstring
				&& path == other.path
				&& tuple == other.tuple
				&& array == other.array
				&& hashmap == other.hashmap
				&& vector == other.vector
				&& variant == other.variant;
		}
	};
}

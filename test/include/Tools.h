#pragma once

#include <string>
#include <random>

#include "FSecure/ByteConverter/Utils.h"

template <typename T = std::string, std::enable_if_t<FSecure::Utils::IsOneOf<T, std::string, std::wstring>::value, int> = 0>
T GenerateRandomString(size_t size)
{
	constexpr std::string_view charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
	static std::random_device rd;
	static std::mt19937 gen(rd());
	static std::uniform_int_distribution<int> uni(0, static_cast<int>(charset.size() - 1));

	T randomString;
	randomString.resize(size);
	for (auto& e : randomString)
		e = static_cast<typename T::value_type>(charset[uni(gen)]);

	return randomString;
}

template <typename T>
T GenerateRandomValue(T rangeFrom = std::numeric_limits<T>::min(), T rangeTo = std::numeric_limits<T>::max())
{
	static std::random_device rd;
	static std::mt19937 eng(rd());
	std::uniform_int_distribution<T> distr(rangeFrom, rangeTo);

	return distr(eng);
}



template <typename T = size_t>
auto RndNum()
{
	return GenerateRandomValue(T{ 8 }, T{ 64 });
}

template <typename T = std::string>
auto RndStr()
{
	return GenerateRandomString<T>(RndNum());
}
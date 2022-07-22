# ByteConverter

This is a header-only library providing classes allowing for serialization, storage, and viewing of data. It also aids the compiler in finding the correct converters for types passed to Read/Write methods. The library itself comes with templates for many standard data structures, as well as allowing for the definition of custom specializations. By recursively searching for the correct converter for a given type, complex containers can be unfolded into their basic components, leaving each definition relatively simple. Matching is performed during compilation time, producing efficient machine code.

## Compilation

To add ByteConverter to a new project, copy the directory `src/include/FSecure/ByteConverter`.

This project can be used on Linux and Windows. It is compatible with three major compilers: MSVC, GCC and Clang.

## Conan

The code is available as a Conan package. Conan is also used for unit tests, to pull the `Catch2` dependency, if the `build_tests` flag is set.

Example of building and running unit tests:
```
conan install . -if out/build/Debug -s build_type=Debug -o build_tests=True
cmake -G Ninja -S . -B out/build/Debug
cmake --build out/build/Debug --config Debug
./out/build/Debug/bin/UnitTest -d yes
```

Generating and testing the conan package by running an example file depending on ByteConverter:
```
set CONAN_CMAKE_GENERATOR=Ninja
conan install . -if out/build/
conan build . --build-folder=out/build/
conan package . --build-folder=out/build/ --package-folder=out/package
conan export-pkg . ByteConverter/0.0.1@test/test --build-folder=out/build/
conan test test_package ByteConverter/0.0.1@test/test
```

## C++20

ByteConverter was first developed and released alongside [C3](https://github.com/FSecureLABS/C3) using the C++17 standard. It utilizes [SFINAE](https://en.cppreference.com/w/cpp/language/sfinae) to detect the correct converter to be applied. C++20's introduction of concepts allows for more direct specification of requirements on template types and functions. As a result, we've been able to replace detection tricks with syntax designed to perform compile time validation, which in turn should reduce the time and resources required during building. Switch to the `cpp20` branch if your project including ByteConverter is already using the current standard, in order to make the most of this.

## Usage

The library provides three types:  
`ByteVector` - Stores the buffer with data, which can be expanded using the `Write` method.  
`ByteView` - Lightweight type allowing access to data stored inside `ByteVector`. Data can be retrieved using the `Read` method.  
`ByteConverter` - Performs conversion of types to, and from, ByteVector. User can create dedicated specializations to add support for custom types.

ByteVector and ByteView are designed as extensions of std::vector and std::basic_string_view of std::uint8_t. They expose many methods of underlying types and will work with standard algorithms.

`ByteConverter.h` is a set of specializations for commonly used standard types. It is recommended to use it as the only include file for the whole library.

### Writing data

Pass objects to the variadic function `Write`. 
```
auto bv = ByteVector{};
bv.Write(std::string{ "foo" }, true);
```

Chain multiple `Write` calls to expand the buffer.
```
bv.Write(0, 1);
bv.Write(1, 2);
```

Use the shorthand `Create` static function to construct and fill a new container in one line.
```
auto bv2 = ByteVector::Create(3, 5);
auto bv3 = ByteVector::Create(8, 13);
```

Join existing containers using `Concat`. Object `bv` will be expanded, while the others will remain unchanged.
```
bv.Concat(bv2).Concat(bv3);
```

### Reading data

Use a temporary `ByteView` to call the `Read` method.
```
auto [arg1, arg2] = ByteView{ bv }.Read<std::string, bool>();
```

Use the same view object for many `Read` calls. This operation moves ByteView to the next object, allowing for chaining of operations. The underlying memory is owned by ByteVector.
```
auto view = ByteView{ bv };
// Operations on the temporary ByteView object had no effect on ByteVector.
// The new view starts at the beginning of the buffer.
auto [sameAsArg1, sameAsArg2] = view.Read<std::string, bool>(); 
auto [fib1, fib2] = view.Read<int, int>();
// Operations on the same view affect future function calls.
auto [fib3, fib4] = view.Read<int, int>();
// Read can be called with the number of bytes to be processed. The returned type will be ByteVector.
auto fib5 = view.Read(sizeof(int));
// Read(size_t) moves ByteView's starting pointer similarly to the Read<T>() template.
auto fib6 = view.Read(sizeof(int)); 
```

### Converting types

Dedicated specializations of ByteConverter for custom types will add serialization support for them to ByteVector and ByteView. ByteConverter must provide the static functions `To/From`. The static function `Size`, which informs how much memory the serialized object requires, is optional. To avoid reallocation, size for all arguments is calculated at an earlier stage of execution, so a converter without a known `Size` will call the function `To` twice.


Example code of specializing ByteConverter for custom type A.
```
struct A
{
	uint16_t m_a;
	uint32_t m_b;
};

// A correct namespace is required for converter lookup.
namespace FSecure
{
	// Declaration of specialization
	template <>
	struct ByteConverter<A>
	{
		// Function transforming data to ByteVector.
		static ByteVector To(A const& a)
		{
			return ByteVector::Create(a.m_a, a.m_b);
		}

		// Informs how many bytes the object will take after serialization.
		// Optional, but recommended.
		static size_t Size(A const& a)
		{
			return ByteVector::Size(a.m_a, a.m_b);
		}

		// Function deserializing data from ByteView and generating new object of the desired type.
		static A From(ByteView& bv)
		{
			auto [a, b] = bv.Read<uint16_t, uint32_t>();
			return A{ a, b };
		}
	};
}
```

## Helpers

Alongside three main classes, some additional types are provided for ease of use.

### Bytes

Bytes is a tag allowing for reading the specified number of bytes from ByteView. It provides a simpler way of joining multiple reads than `Read(size_t)`.
```
template<size_t N, bool HardCopy = false>
class Bytes;
```

The number of bytes must be known at compile time. Using the Bytes class in `Read` will return ByteView, or ByteVector, depending on the `HardCopy` template argument.
```
ByteView{ bv }.Read<std::string, bool, Bytes<4>, Bytes<4, true>>(); 
```


### ByteReader

ByteReader can be used to read data and assign it to already existing variables e.g. class members outside of the constructor.

```
std::string existingString;
bool existingBool;
int sameAsFib1, sameAsFib2;

ByteView viewForReader = bv;
// Read and assign into members
ByteReader{ viewForReader }.Read(existingString, existingBool);
// ByteReader will move ByteView to point at the data of the next object in the buffer
std::tie(sameAsFib1, sameAsFib2) = viewForReader.Read<int, int>();
```

### TupleConverter

This class provides a simple way of generating ByteConverter for custom types by treating them as a tuple. It allows quick creation of ByteConverter by listing all data that is needed to be serialized. ByteConverter can use this functionality by inheriting from TupleConverter and providing a public static `Convert` method. Versions of `To/Size/From` provided by TupleConverter can be shadowed by a dedicated version if additional logic is required.

You can use `Utils::MakeConversionTuple` to easily create a tuple. The function will detect if a tuple should use of a value or reference in order to avoid costly copies, or else if it is counterproductively referencing a trivial type. Please remember that `Read<std::tuple<T1, T2 const&>>()` will return `std::tuple<T1, T2>` for correct data management.

Example of specializing ByteConverter using the TupleConverter helper.
```
struct C
{
	uint16_t m_a;
	std::string m_b;
};

namespace FSecure
{
	// Declare ByteConverter for class C using TupleConverter
	template <>
	struct ByteConverter<C> : TupleConverter<C>
	{

		// Returned type will be handled by the already declared ByteConverter for std::tuple.
		static auto Convert(C const& obj)
		{
			return Utils::MakeConversionTuple(obj.m_a, obj.m_b);
		}
	};
}
```

### PointerTupleConverter

The PointerTupleConverter class is designed to provide `Convert` and modified `From` functions for TupleConverter using member pointers. This approach will only serialize/initialize desired members, leaving the remainder to default initialization. Versions of `To/Size/From` provided by PointerTupleConverter can be shadowed by a dedicated version if necessary.

Example code for specializing ByteConverter using the PointerTupleConverter helper.
```
struct D
{
	uint16_t m_a;
	std::string m_b = "String we would like to not serialize.";
	uint32_t m_c;
};

namespace FSecure
{
	template <>
	struct ByteConverter<D> : PointerTupleConverter<D>
	{
		static auto MemberPointers()
		{
			return std::make_tuple(&D::m_a, &D::m_c);
		}
	};
}
```

## Additional topics

### Recursive resolution

Given complex classes, ByteConverter, if not provided custom specialization, will be generated by recursive lookup and assembled from converters for most fundamental classes. For example, there is no custom specialization for `std::unordered_map<Key,Value>`. Instead, the first general template for iterable containers will be used. Then, a converter for tuple-like types will be chosen for each node, and as long as both `Key` and `Value` can be converted, the code will compile. Both `Key` and `Value` can generate their own cascading template lookup for each of their members.

### constexpr resolution

For many types, a specific object reference is not required to determine how much size will be needed after serialization. The function `Size` can be declared as `constexpr`, without taking any arguments, in order to calculate the required space during compilation time.

```
// Inside ByteConverter<ClassWithFixedSize>
constexpr static size_t Size()
{
	return sizeof(ClassWithFixedSize);
}
```

### Tags

Tag classes can be used to change how serialization will be handled for types that already have defined behavior e.g. std::string will write a header with a size before its content is written into a buffer. Tags can be used to prevent that.

```
struct TwiceTag
{
	// Tags can take a reference to an original object to allow writing to the buffer.
	int const& m_value;
};

namespace FSecure
{
	template <>
	struct ByteConverter<TwiceTag>
	{
		static ByteVector To(TwiceTag const& obj)
		{
			return ByteVector::Create(2 * obj.m_value);
		}

		// The function `From` does not have to return the same type that ByteConverter was specialized for. 
		// Use it to read from the buffer using tags, while returning the desired type.
		static auto From(ByteView& bv)
		{
			return bv.Read<int>() / 2;
		}
	};
}

auto TwiceTagWorks()
{
	auto one = 1;
	auto bv = ByteVector::Create(TwiceTag{ one });
	auto two = ByteView{ bv }.Read<int>();
	one = ByteView{ bv }.Read<TwiceTag>();
	return two == 2 * one;
}
```

### Additional ByteConverter typename

The declaration of ByteConverter takes an extra template argument with a default value. This feature can be used to create conditions for specializations.
```
template <typename T, typename = void>
struct ByteConverter {};
```

ByteConverter specialization for arithmetic types.
```
template <typename T>
struct ByteConverter<T, std::enable_if_t<std::is_arithmetic_v<T>>>;
```

In C++20 Concepts can be used instead of SFINAE in most cases.

### Avoiding temporary buffers

A new ByteVector is created as the result of each call of function `To`.
```
// Inside ByteConverter<A>
static ByteVector To(A const& a)
{
	return ByteVector::Create(a.m_a, a.m_b);
}
```

In most cases data is copied into a different buffer in order to serialize a parent object, and the temporary object is destructed. The parent object already has the capacity to store all required data, as a result of calling `Size` on all objects before memory is first allocated. The function `To` can be modified to take a parent ByteVector parameter, and avoid unnecessary operations.
```
// Inside ByteConverter for arithmetic types.
static void To(T obj, ByteVector& bv)
{
	auto oldSize = bv.size();
	// buffer is guaranteed to have enough capacity to avoid reallocation.
	bv.resize(oldSize + Size());
	*reinterpret_cast<T*>(bv.data() + oldSize) = obj;
}
```

### Literals

This library provides literal operators for creating ByteVector `""_b`, and ByteView `""_bv`.

```
using namespace FSecure::Literals;
...
auto bVector = "\x11\x12"_b;
auto bView = "\x13\x14"_bv;
```

### Exceptions

The errors with handling ByteView and ByteVector should be resolved through use of exceptions.  `Read` and `Size` function have strong exception safety, while `Write` have basic exception safety, and can leave buffer with undefined data.

Exception macros are created to allow compilation inside project with disabled exception handling. Project settings should be detected automatically, but exceptions can be also disabled manually by defining `BYTE_CONVERTER_NO_EXCEPTIONS`. In case of error `std::abort` will be called.
```
BYTE_CONVERTER_TRY
{
	BYTE_CONVERTER_THROW(std::out_of_range{ "Cannot read size from ByteView " });
}
BYTE_CONVERTER_CATCH(...)
{
	// Rethrow
	BYTE_CONVERTER_THROW();
}
```


### Variant

ByteConverter can be used to create a lightweight communication protocols. With `std::variant` and `std::visit` communication layer goes from strong type safety, to data transmission, and back to calling the functions with correct arguments.
```
// helper type for overloaded lambdas
template<class... Ts> struct Overloaded : Ts... { using Ts::operator()...; };
// explicit deduction guide (not needed as of C++20)
template<class... Ts> Overloaded(Ts...)->Overloaded<Ts...>;

namespace FSecure {
	// Possible packets for transmission carrying different data.
	struct PacketA { std::string m_data; };
	struct PacketB { int m_data; };

	// Common interface.
	using Packet = std::variant<PacketA, PacketB>;

	// This example will use same converter for both types.
	// In real scenarios packets and converters would be defined independently in separate files.
	template <typename T> struct Converter : TupleConverter<T>
	{
		static auto Convert(T const& obj)
		{
			return std::tuple{obj.m_data};
		}
	};

	// Declaration for converter lookup.
	template <> struct ByteConverter<PacketA> : Converter<PacketA> {};
	template <> struct ByteConverter<PacketB> : Converter<PacketB> {};

	// Function simulating receiving a buffer from communication medium.
	// Only size and binary data is known.
	void Receive(ByteView bv)
	{
		// Variant type is recognized and execution is passed to function with correct signature.
		std::visit(Overloaded{
				[&](PacketA obj) { std::cout << "Packet A " << obj.m_data << std::endl; },
				[&](PacketB obj) { std::cout << "Packet B " << obj.m_data << std::endl; },
				[&](auto&&) { throw std::runtime_error{ "Packet not supported" }; },
			}, bv.Read<Packet>());
	}

	// Function taking an argument as variant, and transmitting to the other side as data.
	void Send(Packet packet)
	{
		Receive(ByteVector::Create(packet));
	}

	// Demonstration of sending a packet without knowledge of underlying transmission.
	void SimpleExampleOfCommunication()
	{
		Send(PacketA{ "string message" });
	}
}
```

## More Information

Special thanks for:
* [@grzryc](https://github.com/grzryc) for reviewing code, providing ideas, and implementing some of ByteConverter specializations.
* [@aaron.joslyn](https://github.com/aaronjoslyn) for his help in creating usage manual.


## License

BSD 3-Clause License

Copyright (c) 2019-2022, F-Secure

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

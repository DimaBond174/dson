#include <dson/dson.h>
#include <dson/from_dson_converters.h>
#include <dson/to_dson_converters.h>

#include <iostream>

enum class Keys : std::int32_t
{
	x,
	y,
	z
};

void store_numbers()
{
	hi::Dson dson;
	dson.insert(Keys::z, std::uint32_t{12345});
	dson.insert(Keys::x, 1);
	dson.insert(Keys::y, -2);
	dson.insert(Keys::z, std::uint32_t{3});

	for (const auto & [key, value] : dson.map())
	{
		std::cout << "dson[" << key << "]=" << hi::to_int64(value.get()) << std::endl;
	}
	std::cout << "dson[Keys::y]=" << hi::to_uint32(dson[Keys::y]) << std::endl;
	std::cout << "dson[Keys::y]=" << hi::to_int32(dson[Keys::y]) << std::endl;
}

int main(int /* argc */, char ** /* argv */)
{
	store_numbers();
	std::cout << "Tests finished" << std::endl;
	return 0;
}

#include <dson/dson.h>
#include <dson/from_dson_converters.h>

#include <iostream>

enum class Keys : std::int32_t
{
	x,
	y,
	z,
	string_message,
	string_view_message
};

void print(std::string_view header, hi::Dson & dson)
{
	std::cout << header << std::endl;
	for (const auto & [key, value] : dson.map())
	{
		std::cout << "dson[" << key << "]=" << hi::to_string(value) << std::endl;
	}
	std::cout << std::endl;
}

void save_and_load()
{
	hi::Dson dson1;
	dson1.insert(Keys::x, 4);
	dson1.insert(Keys::y, 2);
	dson1.insert(Keys::z, std::uint32_t(3));
	dson1.insert(Keys::string_message, std::string{"Mother washed the frame"});
	dson1.insert(Keys::string_view_message, std::string_view{"Father eats banana"});
	std::cout << "dson1[Keys::z]=" << hi::to_string(dson1[Keys::z]) << std::endl;
	print("dson1:", dson1);
	{
		std::ofstream output("dson.bin", std::ios::binary);
		output << dson1;
	}

	print("dson1 after copy:", dson1);

	hi::Dson dson2;
	{
		std::ifstream input("dson.bin", std::ios::binary);
		input >> dson2;
	}
	std::cout << "dson2[Keys::string_message]=" << hi::to_string_view(dson2[Keys::string_message]) << std::endl;
	std::cout << "dson2[Keys::z]=" << hi::to_string(dson2[Keys::z]) << std::endl;
	print("dson2:", dson2);
}

int main(int /* argc */, char ** /* argv */)
{
	std::cout << "types_map<std::uint32_t>::value= " << hi::types_map<std::uint32_t>::value << std::endl;
	std::cout << "types_map<std::string>::value= " << hi::types_map<std::string>::value << std::endl;
	save_and_load();
	std::cout << "Tests finished" << std::endl;
	return 0;
}

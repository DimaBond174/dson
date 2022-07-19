#define USE_USER_DSON_TYPES
#include <dson/dson.h>

#include <iostream>

extern void save_and_load_advanced();
extern void very_big_struct_converters(
	hi::Dson::Converters::ConvertersMap & to_host_order,
	hi::Dson::Converters::ConvertersMap & to_network_order);
extern void save_and_load_advanced_composite();

struct MyStruct;
template <>
struct hi::types_map<MyStruct> : register_id<MyStruct, 54321>
{
};

struct MyStruct
{
	std::string name;
	std::uint16_t age;
	bool male;
	operator hi::Dson()
	{
		hi::Dson re;
		const auto all_size = sizeof(std::uint32_t) /* для name.size() */ + name.size() + sizeof(age) + sizeof(male);
		char * buf = static_cast<char *>(re.init(hi::types_map<MyStruct>::value, static_cast<std::int32_t>(all_size)));
		if (!buf)
			return re;
		*std::launder(reinterpret_cast<std::uint32_t *>(buf)) = name.size();
		buf += sizeof(std::uint32_t);
		std::memcpy(buf, name.data(), name.size());
		buf += name.size();
		*std::launder(reinterpret_cast<std::uint16_t *>(buf)) = age;
		buf += sizeof(std::uint16_t);
		*std::launder(reinterpret_cast<bool *>(buf)) = male;
		return re;
	}
};

// std::unique_ptr<hi::DsonObj> to_json_obj(std::int32_t key, MyStruct val)
//{
//     auto re = std::make_unique<hi::Dson>();
//     const auto all_size = sizeof(std::uint32_t) /* для name.size() */ + val.name.size() + sizeof (val.age) + sizeof
//     (val.male); char *buf = static_cast<char *>(re->init(key, hi::types_map<MyStruct>::value,
//     static_cast<std::int32_t>(all_size))); if (!buf) return {}; *std::launder(reinterpret_cast<std::uint32_t *>(buf))
//     = val.name.size(); buf += sizeof(std::uint32_t); std::memcpy(buf, val.name.data(), val.name.size()); buf +=
//     val.name.size(); *std::launder(reinterpret_cast<std::uint16_t *>(buf)) = val.age; buf += sizeof(std::uint16_t);
//     *std::launder(reinterpret_cast<bool *>(buf)) = val.male;
//     return re;
// }

void hi::Dson::Converters::dson_user_defined_converters(ConvertersMap & to_host_order, ConvertersMap & to_network_order)
{
	// Подгружаем network order конвертеры с других компилируемых файлов
	very_big_struct_converters(to_host_order, to_network_order);

	const auto dummy_key = types_map<MyStruct>::value;
	to_host_order.emplace(
		dummy_key,
		[](hi::Dson::Header &, char * buf)
		{
			std::uint32_t * size = std::launder(reinterpret_cast<std::uint32_t *>(buf));
			*size = ntohl(*size);
			buf += sizeof(std::uint32_t) + *size;
			std::uint16_t * age = std::launder(reinterpret_cast<std::uint16_t *>(buf));
			*age = ntohs(*age);
		});

	to_network_order.emplace(
		dummy_key,
		[](hi::Dson::Header &, char * buf)
		{
			std::uint32_t * size = std::launder(reinterpret_cast<std::uint32_t *>(buf));
			buf += sizeof(std::uint32_t) + *size;
			*size = htonl(*size);
			std::uint16_t * age = std::launder(reinterpret_cast<std::uint16_t *>(buf));
			*age = ntohs(*age);
		});
}

MyStruct to_my_struct(hi::Dson * dson)
{
	char * buf = static_cast<char *>(dson->data());
	if (!buf)
		return {};
	auto size = *std::launder(reinterpret_cast<std::uint32_t *>(buf));
	if (!size || size > 1024)
		return {};
	buf += sizeof(std::uint32_t);
	MyStruct re;
	re.name = std::string{buf, size};
	buf += size;
	re.age = *std::launder(reinterpret_cast<std::uint16_t *>(buf));
	buf += sizeof(std::uint16_t);
	re.male = *std::launder(reinterpret_cast<bool *>(buf));
	return re;
}

void save_and_load_simple()
{
	hi::Dson dson1;
	MyStruct my_struct_before{"Dima", 45, true};

	dson1.emplace(1, my_struct_before);
	{
		std::ofstream output("dson.bin", std::ios::binary);
		output << dson1;
	}

	hi::Dson dson2;
	{
		std::ifstream input("dson.bin", std::ios::binary);
		input >> dson2;
	}
	auto my_struct_obj = dson2.get(1);
	assert(my_struct_obj);
	auto my_struct_dson_obj = dynamic_cast<hi::Dson *>(my_struct_obj);
	assert(my_struct_dson_obj);

	auto my_struct_after = to_my_struct(my_struct_dson_obj);
	assert(
		my_struct_before.name == my_struct_after.name && my_struct_before.age == my_struct_after.age
		&& my_struct_before.male == my_struct_after.male);

	std::cout << "Loaded:" << std::endl
			  << "MyStruct {" << std::endl
			  << "\t name = " << my_struct_after.name << std::endl
			  << "\t age = " << my_struct_after.age << std::endl
			  << "\t male = " << my_struct_after.male << std::endl
			  << "}" << std::endl;
}

int main(int /* argc */, char ** /* argv */)
{
	save_and_load_simple();
	save_and_load_advanced();
	save_and_load_advanced_composite();
	std::cout << "Tests finished" << std::endl;
	return 0;
}

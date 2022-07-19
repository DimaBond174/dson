#ifndef DSON_TOOLS_H
#define DSON_TOOLS_H

#include <dson/os/system_switch.h>

#include <cstdint>
#include <limits>
#include <new>

namespace hi
{

#if __BIG_ENDIAN__
#	define htonll(x) (x)
#	define ntohll(x) (x)
#else
#	define htonll(x) (((uint64_t)htonl((x)&0xFFFFFFFF) << 32) | htonl((x) >> 32))
#	define ntohll(x) (((uint64_t)ntohl((x)&0xFFFFFFFF) << 32) | ntohl((x) >> 32))
#endif

/*
  Количество знаков после запятой достаточное для
  передачи GPS координат
*/
constexpr double div100000007 = 10000000.0;

constexpr std::uint64_t zero{0};
constexpr std::int32_t size_of_uint64{static_cast<std::int32_t>(sizeof(std::uint64_t))};

/*
  Преобразование double в host|network проблемно
  https://stackoverflow.com/questions/10616883/how-to-convert-double-between-host-and-network-byte-order
  https://docs.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-htond
  https://docs.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-ntohd

  поэтому варианты:
  1) добавить свои кастомные преобразователи to_json_obj своего кастомного типа
  (например через стандартные функции Microsoft)
  2) слать в виде std::ratio
  3) пересылать double в виде строки
  4) использовать хак ниже
  //todo юнит тесты на пограничные и отрицательные
*/
inline std::uint64_t double_to_network(double data)
{
	data *= div100000007;
	if (std::numeric_limits<std::int64_t>::min() < data && data < std::numeric_limits<std::int64_t>::max())
	{
		std::int64_t i64 = static_cast<std::int64_t>(data);
		return ntohll(*std::launder(reinterpret_cast<std::uint64_t *>(&i64)));
	}
	return ntohll(zero);
}

inline double double_to_host(std::uint64_t data)
{
	data = ntohll(data);
	return static_cast<double>(*std::launder(reinterpret_cast<std::int64_t *>(&data))) / div100000007;
}

inline std::uint64_t int64_to_network(std::int64_t data)
{
	return ntohll(*std::launder(reinterpret_cast<std::uint64_t *>(&data)));
}

inline std::int64_t int64_to_host(std::uint64_t data)
{
	data = ntohll(data);
	return (*std::launder(reinterpret_cast<std::int64_t *>(&data)));
}

inline std::uint32_t int32_to_network(std::int32_t data)
{
	return htonl(*std::launder(reinterpret_cast<std::uint32_t *>(&data)));
}

inline std::int32_t int32_to_host(std::uint32_t data)
{
	data = ntohl(data);
	return (*std::launder(reinterpret_cast<std::int32_t *>(&data)));
}

} // namespace hi

#endif // DSON_TOOLS_H

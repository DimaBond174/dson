#ifndef DSON_TOOLS_H
#define DSON_TOOLS_H

#include <dson/os/system_switch.h>

#include <cfloat>
#include <cmath>
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

enum class FloatPointType : std::uint8_t
{
	// Normal negative number
	NormalNegative,
	// Normal positive number
	NormalPositive,
	// Not-a-number (NaN) value
	NaN,
	// Negative infinity
	InfNegative,
	// Positive infinity.
	InfPositive
};

constexpr std::int32_t buf_size_for_double =
	static_cast<std::int32_t>(sizeof(FloatPointType) + sizeof(std::uint64_t) + sizeof(std::uint32_t));

constexpr int double_mantissa_bits{53};

inline void double_in_buf_to_host_order(char * data)
{
	FloatPointType type = *std::launder(reinterpret_cast<FloatPointType *>(data));
	double * res = std::launder(reinterpret_cast<double *>(data));
	switch (type)
	{
	case FloatPointType::NaN:
		*res = std::numeric_limits<double>::quiet_NaN();
		return;
	case FloatPointType::InfNegative:
		*res = -std::numeric_limits<double>::infinity();
		return;
	case FloatPointType::InfPositive:
		*res = std::numeric_limits<double>::infinity();
		return;
	case FloatPointType::NormalNegative:
		break;
	case FloatPointType::NormalPositive:
		break;
	}
	std::uint64_t * mantissa_ptr = std::launder(reinterpret_cast<std::uint64_t *>(data + sizeof(FloatPointType)));
	std::uint32_t * exp_ptr = std::launder(reinterpret_cast<std::uint32_t *>(mantissa_ptr + 1));
	*res = std::ldexp(static_cast<double>(ntohll(*mantissa_ptr)), int32_to_host(*exp_ptr));
	if (type == FloatPointType::NormalNegative)
	{
		*res = -*res;
	}
} // double_in_buf_to_host_order

inline void double_in_buf_to_network_order(char * data)
{
	const double from = *std::launder(reinterpret_cast<double *>(data));
	FloatPointType * type = std::launder(reinterpret_cast<FloatPointType *>(data));
	if (std::isnan(from))
	{
		*type = FloatPointType::NaN;
		return;
	}
	if (std::isinf(from))
	{
		*type = (from < 0) ? FloatPointType::InfNegative : FloatPointType::InfPositive;
		return;
	}
	*type = (from < 0) ? FloatPointType::NormalNegative : FloatPointType::NormalPositive;
	++type;

	int exp{0};
	double frac = std::frexp(std::fabs(from), &exp);

	// матиссу делаем целочисленной
	std::uint64_t * mantissa = std::launder(reinterpret_cast<std::uint64_t *>(type));
	*mantissa = htonll(static_cast<std::uint64_t>(std::ldexp(frac, double_mantissa_bits)));
	++mantissa;

	// корректируем степень
	exp -= double_mantissa_bits;
	*std::launder(reinterpret_cast<std::uint32_t *>(mantissa)) = int32_to_network(exp);
} // double_in_buf_to_network_order

} // namespace hi

#endif // DSON_TOOLS_H

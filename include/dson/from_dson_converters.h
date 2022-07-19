#ifndef FROM_DSON_CONVERTERS_H
#define FROM_DSON_CONVERTERS_H

#include <dson/dson.h>
#include <dson/custom_dson_objs/dson_string_obj.h>

#include <string>
#include <string_view>

/**
 * Набор преобразователей DsonObj в стандартные типы данных
 */
namespace hi
{

// TODO float, double, long double as ratio:
// https://stackoverflow.com/questions/50962041/how-can-i-get-numerator-and-denominator-from-a-fractional-number
inline std::uint32_t to_uint32(Dson * obj, const std::uint32_t def = 0)
{
	if (!obj)
		return {};
	switch (obj->data_type())
	{
	case types_map<std::uint32_t>::value:
		return *(static_cast<std::uint32_t *>(obj->data()));
	case types_map<std::int32_t>::value:
		{
			const std::int32_t re = *(static_cast<std::int32_t *>(obj->data()));
			if (0 <= re)
			{
				return static_cast<std::uint32_t>(re);
			}
			return def;
		}
	case types_map<std::uint64_t>::value:
		{
			const std::uint64_t re = *(static_cast<std::uint64_t *>(obj->data()));
			if (re <= std::numeric_limits<std::uint32_t>::max())
			{
				return static_cast<std::uint32_t>(re);
			}
			return def;
		}
	case types_map<std::int64_t>::value:
		{
			const std::int64_t re = *(static_cast<std::int64_t *>(obj->data()));
			if (0 <= re && re <= std::numeric_limits<std::uint32_t>::max())
			{
				return static_cast<std::uint32_t>(re);
			}
			return def;
		}
	default:
		break;
	}
	return {};
}

std::uint32_t to_uint32(DsonObj * obj, const std::uint32_t def = 0)
{
	if (!obj)
		return {};
	if (auto dson = dynamic_cast<Dson *>(obj))
	{
		return to_uint32(dson, def);
	}
	return {};
}

inline std::int32_t to_int32(Dson * obj, const std::int32_t def = 0)
{
	if (!obj)
		return {};
	switch (obj->data_type())
	{
	case types_map<std::int32_t>::value:
		return *(static_cast<std::int32_t *>(obj->data()));
	case types_map<std::uint32_t>::value:
		{
			const std::uint32_t re = *(static_cast<std::uint32_t *>(obj->data()));
			if (re <= std::numeric_limits<std::int32_t>::max())
			{
				return static_cast<std::int32_t>(re);
			}
			return def;
		}
	case types_map<std::uint64_t>::value:
		{
			const std::uint64_t re = *(static_cast<std::uint64_t *>(obj->data()));
			if (re <= std::numeric_limits<std::int32_t>::max())
			{
				return static_cast<std::int32_t>(re);
			}
			return def;
		}
	case types_map<std::int64_t>::value:
		{
			const std::int64_t re = *(static_cast<std::int64_t *>(obj->data()));
			if (std::numeric_limits<std::int32_t>::lowest() <= re && re <= std::numeric_limits<std::int32_t>::max())
			{
				return static_cast<std::int32_t>(re);
			}
			return def;
		}
	default:
		break;
	}
	return {};
}

inline std::int32_t to_int32(DsonObj * obj, const std::int32_t def = 0)
{
	if (!obj)
		return {};
	if (auto dson = dynamic_cast<Dson *>(obj))
	{
		return to_int32(dson, def);
	}
	return {};
}

inline std::uint64_t to_uint64(Dson * obj, const std::uint64_t def = 0)
{
	if (!obj)
		return {};
	switch (obj->data_type())
	{
	case types_map<std::uint32_t>::value:
		return *(static_cast<std::uint32_t *>(obj->data()));
	case types_map<std::int32_t>::value:
		{
			const std::int32_t re = *(static_cast<std::int32_t *>(obj->data()));
			if (0 <= re)
			{
				return static_cast<std::uint64_t>(re);
			}
			return def;
		}
	case types_map<std::uint64_t>::value:
		return *(static_cast<std::uint64_t *>(obj->data()));
	case types_map<std::int64_t>::value:
		{
			const std::int64_t re = *(static_cast<std::int64_t *>(obj->data()));
			if (0 <= re && re <= std::numeric_limits<std::uint64_t>::max())
			{
				return static_cast<std::uint64_t>(re);
			}
			return def;
		}
	default:
		break;
	}
	return {};
}

inline std::uint64_t to_uint64(DsonObj * obj, const std::uint64_t def = 0)
{
	if (!obj)
		return {};
	if (auto dson = dynamic_cast<Dson *>(obj))
	{
		return to_uint64(dson, def);
	}
	return {};
}

inline std::int64_t to_int64(Dson * obj, const std::int64_t def = 0)
{
	if (!obj)
		return {};
	switch (obj->data_type())
	{
	case types_map<std::int32_t>::value:
		return *(static_cast<std::int32_t *>(obj->data()));
	case types_map<std::uint32_t>::value:
		return static_cast<std::int64_t>(*(static_cast<std::uint32_t *>(obj->data())));
	case types_map<std::uint64_t>::value:
		{
			const std::uint64_t re = *(static_cast<std::uint64_t *>(obj->data()));
			if (re <= std::numeric_limits<std::int64_t>::max())
			{
				return static_cast<std::int64_t>(re);
			}
			return def;
		}
	case types_map<std::int64_t>::value:
		return *(static_cast<std::int64_t *>(obj->data()));
	default:
		break;
	}
	return {};
}

inline std::int64_t to_int64(DsonObj * obj, const std::int64_t def = 0)
{
	if (!obj)
		return {};
	if (auto dson = dynamic_cast<Dson *>(obj))
	{
		return to_int64(dson, def);
	}
	return {};
}

inline std::string_view to_string_view(Dson * obj)
{
	if (!obj)
		return {};
	if (obj->data_type() != types_map<std::string>::value)
		return {};
	return std::string_view{static_cast<char *>(obj->data()), static_cast<std::uint32_t>(obj->data_size())};
}

inline std::string_view to_string_view(DsonObj * obj)
{
	if (!obj)
		return {};
	if (auto dson = dynamic_cast<Dson *>(obj))
	{
		return to_string_view(dson);
	}
	if (auto dson = dynamic_cast<DsonStringObj *>(obj))
	{
		return dson->object();
	}
	return {};
}

/**
 * @brief to_string
 * Преобразование объектов в строку (чисел и др.).
 * Например для вывода в std::cout.
 * Для получения именно строк лучше использовать to_string_view
 * @param obj - что преобразовать к строке
 * @return объект строки
 */
inline std::string to_string(DsonObj * obj)
{
	if (!obj)
		return {};
	switch (obj->data_type())
	{
	case types_map<std::uint32_t>::value:
		return std::to_string(to_uint32(obj));
	case types_map<std::int32_t>::value:
		return std::to_string(to_int32(obj));
	case types_map<std::uint64_t>::value:
		return std::to_string(to_uint64(obj));
	case types_map<std::int64_t>::value:
		return std::to_string(to_int64(obj));
	case types_map<std::string>::value:
		return std::string{to_string_view(obj)};
	}

	return std::string{"unknown dson data_type():"}.append(std::to_string(obj->data_type()));
}

inline std::string to_string(const std::unique_ptr<DsonObj> & obj)
{
	return to_string(obj.get());
}

} // namespace hi

#endif // FROM_DSON_CONVERTERS_H

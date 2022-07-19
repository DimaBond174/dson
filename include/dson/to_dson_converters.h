#ifndef DSON_CONVERTERS_H
#define DSON_CONVERTERS_H

#include <dson/custom_dson_objs/dson_string_obj.h>

#include <memory>

/**
 * Набор преобразователей стандартных типов данных в DsonObj
 */
namespace hi
{

inline std::unique_ptr<DsonObj> to_json_obj(std::int32_t key, std::string val)
{
	return std::make_unique<DsonStringObj>(key, std::move(val));
}

template <typename K>
inline std::unique_ptr<DsonObj> to_json_obj(K key, std::string val)
{
	return to_json_obj(static_cast<std::int32_t>(key), std::move(val));
}

} // namespace hi

#endif // DSON_CONVERTERS_H

#ifndef TYPES_MAP_H
#define TYPES_MAP_H

#include <cstdint>
#include <string>
#include <vector>

namespace hi
{

template <int N>
struct marker_id
{
	static std::int32_t const value = N;
};

template <typename T>
struct marker_type
{
	typedef T type;
};

template <typename T, int N>
struct register_id
	: marker_id<N>
	, marker_type<T>
{
private:
	friend marker_type<T> marked_id(marker_id<N>)
	{
		return marker_type<T>();
	}
};

template <typename T>
struct types_map;

/**
 * Заполнение справочника идентификаторов типов данных.
 * Для отправки через сеть типам данных назначается std::int32_t идентификатор.
 * (знаковый для совместимости с другими языками)
 * Данный справочник нужен в качестве реестра вроде Enum.
 * Enum не используется чтобы дать возможность дополнять справочник
 * пользовательскими типами данных в разных местах программы.
 */

// Структура - метка ошибки
struct Empty
{
};
template <>
struct types_map<Empty> : register_id<Empty, 0>
{
};

// Структура - метка контейнера, содержащего другие объекты
class DsonContainer
{
};
template <>
struct types_map<DsonContainer> : register_id<DsonContainer, 1>
{
};

template <>
struct types_map<std::string> : register_id<std::string, 2>
{
};
template <>
struct types_map<std::int32_t> : register_id<std::int32_t, 3>
{
};
template <>
struct types_map<std::uint32_t> : register_id<std::uint32_t, 4>
{
};
template <>
struct types_map<std::int64_t> : register_id<std::int64_t, 5>
{
};
template <>
struct types_map<std::uint64_t> : register_id<std::uint64_t, 6>
{
};
template <>
struct types_map<std::vector<std::uint32_t>> : register_id<std::vector<std::uint32_t>, 7>
{
};

} // namespace hi

#endif // TYPES_MAP_H

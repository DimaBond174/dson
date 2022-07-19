#ifndef ROUTER_H
#define ROUTER_H

#include <dson/custom_dson_objs/dson_route_obj.h>
#include <dson/dson.h>

#include <functional>
#include <map>

/*
 Каждый кто хочет чтобы его нашли оставляет свой ID -> функция как послать сообщение.
 Для того чтобы пример оставался простым не отслеживается время жизни колбэков.
*/
class Router
{
public:
	template <typename K>
	Router(K route_address_key)
		: route_address_key_{static_cast<std::int32_t>(route_address_key)}
	{
	}

	std::int32_t route_address_key()
	{
		return route_address_key_;
	}

	using Callback = std::function<void(hi::Dson &&)>;
	void add_route(std::uint32_t id, Callback callback)
	{
		route_table_.insert_or_assign(id, std::move(callback));
	}

	void route(hi::Dson && dson)
	{
		const auto * address = hi::to_address(dson.get(route_address_key_));
		if (auto it{route_table_.find(address->to_cli_id)}; it != std::end(route_table_))
		{
			const auto & [key, callback]{*it};
			callback(std::move(dson));
		}
	}

private:
	const std::int32_t route_address_key_;
	std::map<std::uint32_t, Callback> route_table_;
};

#endif // ROUTER_H

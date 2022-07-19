#include "cout_scope.h"
#include "socket_client.h"

#include <dson/include_all.h>

#include <future>

/*
  В этом примере имитируется что к серверу подключено несколько клиентов,
  и между ними происходит общение.
  Сервер занимается маршрутизацией.
  Для упрощения примера только одно из нескольких подключение через UNIX socket.
  Впрочем роутеру всё равно чей колбэк - с соккета/потока/IPC.

  Так как все идут через маршрутизатор - то это звезда.
*/
void star_routing()
{
	hi::CoutScope scope("star_routing");
	enum class Key : std::int32_t
	{
		RouteAddress,
		WantAuth,
		Authed,
		SimpleData,
		Password,
		ChunkID,
		StringData
	};

	Router router{Key::RouteAddress};

	const std::string_view password{"password"};

	const auto echo_service = [&](std::uint32_t & chunk_id, hi::Dson & dson)
	{
		++chunk_id;
		hi::Dson answer;
		auto sender_address = to_address(dson.get(Key::RouteAddress));
		answer.insert(std::make_unique<hi::DsonRouteObj>(Key::RouteAddress, sender_address));

		switch (dson.typed_key<Key>())
		{
		case Key::WantAuth:
			if (password == hi::to_string_view(dson.get(Key::Password)))
			{
				answer.set_key(Key::Authed);
				router.route(std::move(answer));
			}
			break;
		case Key::SimpleData:
			answer.set_key(Key::SimpleData);
			answer.insert(Key::ChunkID, chunk_id);
			answer.insert(
				Key::StringData,
				std::to_string(sender_address->to_cli_id)
					.append(" received from ")
					.append(std::to_string(sender_address->from_cli_id))
					.append(" string:")
					.append(hi::to_string_view(dson.get(Key::StringData))));
			router.route(std::move(answer));
			break;
		default:
			break;
		}
	};

	for (std::uint32_t service_id = 1; service_id < 100; ++service_id)
	{
		router.add_route(
			service_id,
			[&, chunk_id = std::uint32_t{}](hi::Dson && dson) mutable
			{
				echo_service(chunk_id, dson);
			});
	}

	SocketServer server{router, scope};
	SocketClient client;

	std::uint32_t cnt{0};

	const auto send_dson = [](const std::int32_t fd, hi::Dson & dson) -> bool
	{
		hi::Result result{hi::Result::InProcess};
		while (hi::Result::InProcess == result)
		{
			result = dson.copy_to_fd_network_order(fd);
		}
		return (hi::Result::Error != result);
	};

	auto client_callback = [&](std::int32_t fd, hi::Dson & dson)
	{
		hi::Dson answer;
		auto sender_address = to_address(dson.get(Key::RouteAddress));
		answer.insert(std::make_unique<hi::DsonRouteObj>(Key::RouteAddress, sender_address));
		switch (dson.typed_key<Key>())
		{
		case Key::Authed:
			{
				scope.print(std::string{"Message::Authed from "}.append(std::to_string(sender_address->from_cli_id)));
				answer.set_key(Key::SimpleData);
				answer.insert(
					Key::StringData,
					std::string{"echo message for "}.append(std::to_string(sender_address->from_cli_id)));
				send_dson(fd, answer);
			}
			break;
		case Key::SimpleData:
			{
				const auto c = hi::to_uint32(dson.get(Key::ChunkID));
				scope.print(std::string{"Message::SimpleData from "}
								.append(std::to_string(sender_address->from_cli_id))
								.append(" chunk{")
								.append(std::to_string(hi::to_uint32(dson.get(Key::ChunkID))))
								.append("}, string{")
								.append(hi::to_string_view(dson.get(Key::StringData))));
				++cnt;
			}
			break;
		default:
			break;
		}
	};

	std::promise<bool> can_finish_promise;
	const auto can_finish_future = can_finish_promise.get_future();

	client.execute(
		[&](std::int32_t fd)
		{
			const std::uint32_t client_certificate_id{12345};
			hi::Dson dson;
			hi::Dson answer_dson;
			dson.set_key(Key::WantAuth);
			dson.insert(Key::Password, password);
			dson.insert(std::make_unique<hi::DsonRouteObj>(Key::RouteAddress));
			auto address = dynamic_cast<hi::DsonRouteObj *>(dson.get(Key::RouteAddress));
			address->address()->from_cli_id = client_certificate_id;
			bool was_answers{false};
			for (std::uint32_t service_id = 0; service_id < 500; ++service_id)
			{
				address->address()->to_cli_id = service_id;
				// send via network
				if (!send_dson(fd, dson))
				{
					scope.print(std::string{"ERROR: client failed to send_dson, error:"}.append(
						std::to_string(errno).append(strerror(errno))));
					return;
				}
				// receive from network
				if (hi::Result::Ready == answer_dson.load_from_fd(fd))
				{
					client_callback(fd, answer_dson);
					answer_dson.clear();
					if (!was_answers)
					{
						was_answers = true;
						can_finish_promise.set_value(true);
					}
				}
			}
			// Дождёмся чтобы ответил хоть кто-то
			while (!was_answers)
			{
				// receive from network
				if (hi::Result::Ready == answer_dson.load_from_fd(fd))
				{
					client_callback(fd, answer_dson);
					answer_dson.clear();
					was_answers = true;
					can_finish_promise.set_value(true);
					break;
				}
			}
		});

	can_finish_future.wait();
	server.stop();
}

int main(int /* argc */, char ** /* argv */)
{
	star_routing();
	std::cout << "Tests finished" << std::endl;
	return 0;
}

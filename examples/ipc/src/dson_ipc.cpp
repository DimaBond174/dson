#include "cout_scope.h"

#include <dson/dson.h>

#include <vector>

void empty_messages()
{
	hi::CoutScope scope("empty_messages");
	/*
	 * https://man7.org/linux/man-pages/man2/pipe.2.html
	 * The array pipefd is used to
	   return two file descriptors referring to the ends of the pipe.
	   pipefd[0] refers to the read end of the pipe.  pipefd[1] refers
	   to the write end of the pipe.
	*/
	int pipe_client_send_to_server[2];
	int pipe_server_send_to_client[2];
	if (pipe(pipe_client_send_to_server) == -1 || pipe(pipe_server_send_to_client))
	{
		scope.print("ERROR: failed to open pipe");
		return;
	}

	enum class Message : std::int32_t
	{
		IAmClient,
		IAmServer,
		AlarmEnemyDetected,
		AttackEnemy,
		EnemyDestroyed,
		GoShutdown,
		WorkFinished
	};

	const auto load_dson = [](const std::int32_t fd, hi::Dson & dson) -> bool
	{
		dson.clear();
		hi::Result result{hi::Result::InProcess};
		while (hi::Result::InProcess == result)
		{
			result = dson.load_from_fd(fd);
		}
		return (hi::Result::Error != result);
	};

	const auto send_dson = [](const std::int32_t fd, hi::Dson & dson) -> bool
	{
		hi::Result result{hi::Result::InProcess};
		while (hi::Result::InProcess == result)
		{
			result = dson.copy_to_fd_host_order(fd);
		}
		return (hi::Result::Error != result);
	};

	const auto server = [&]
	{
		hi::Dson read_dson;
		hi::Dson write_dson;
		enum class State
		{
			WaitClientAuth,
			WaitEnemyDetection,
			WaitEnemyDestroyed,
			WaitClientShutdowned
		};
		// Гарант очерёдности сообщений
		State state_{State::WaitClientAuth};
		while (true)
		{
			if (!load_dson(pipe_client_send_to_server[0], read_dson))
			{
				scope.print(std::string{"Server:FAIL: read_dson.load_from_fd, errno="}.append(std::to_string(errno)));
				continue;
			}
			switch (read_dson.typed_key<Message>())
			{
			case Message::IAmClient:
				{
					if (state_ != State::WaitClientAuth)
					{
						scope.print("Server:ERROR:state sequence is broken.");
						return;
					}
					scope.print("Server:received: Message::IAmClient");
					write_dson.clear();
					write_dson.set_key(Message::IAmServer);
					send_dson(pipe_server_send_to_client[1], write_dson);
					state_ = State::WaitEnemyDetection;
				}
				break;
			case Message::AlarmEnemyDetected:
				{
					if (state_ != State::WaitEnemyDetection)
					{
						scope.print("Server:ERROR:state sequence is broken.");
						return;
					}
					scope.print("Server:received: Message::AlarmEnemyDetected");
					write_dson.clear();
					write_dson.set_key(Message::AttackEnemy);
					send_dson(pipe_server_send_to_client[1], write_dson);
					state_ = State::WaitEnemyDestroyed;
				}
			case Message::EnemyDestroyed:
				{
					if (state_ != State::WaitEnemyDestroyed)
					{
						scope.print("Server:ERROR:state sequence is broken.");
						return;
					}
					scope.print("Server:received: Message::EnemyDestroyed");
					write_dson.clear();
					write_dson.set_key(Message::GoShutdown);
					send_dson(pipe_server_send_to_client[1], write_dson);
					state_ = State::WaitClientShutdowned;
				}
			case Message::WorkFinished:
				{
					if (state_ != State::WaitClientShutdowned)
					{
						scope.print("Server:ERROR:state sequence is broken.");
						return;
					}
					scope.print("Server:received: Message::WorkFinished");
					return;
				}
				break;
			default:
				scope.print(std::string{"Server:UNKNOWN message:"}.append(std::to_string(read_dson.key())));
				break;
			}
		}
		scope.print("Server:Finished");
	}; // server

	const auto client = [&]
	{
		hi::Dson read_dson;
		hi::Dson write_dson;
		write_dson.set_key(Message::IAmClient);
		send_dson(pipe_client_send_to_server[1], write_dson);
		enum class State
		{
			WaitServerAuth,
			WaitServerAtackOrder,
			WaitServerShutdownOrder,
		};
		// Гарант очерёдности сообщений
		State state_{State::WaitServerAuth};
		while (true)
		{
			if (!load_dson(pipe_server_send_to_client[0], read_dson))
			{
				scope.print(std::string{"Client:FAIL: read_dson.load_from_fd, errno="}.append(std::to_string(errno)));
				continue;
			}
			switch (read_dson.typed_key<Message>())
			{
			case Message::IAmServer:
				{
					if (state_ != State::WaitServerAuth)
					{
						scope.print("Client:ERROR:state sequence is broken.");
						return;
					}
					scope.print("Client:received: Message::IAmServer");
					write_dson.clear();
					write_dson.set_key(Message::AlarmEnemyDetected);
					send_dson(pipe_client_send_to_server[1], write_dson);
					state_ = State::WaitServerAtackOrder;
				}
				break;
			case Message::AttackEnemy:
				{
					if (state_ != State::WaitServerAtackOrder)
					{
						scope.print("Client:ERROR:state sequence is broken.");
						return;
					}
					scope.print("Client:received: Message::AttackEnemy");
					write_dson.clear();
					write_dson.set_key(Message::EnemyDestroyed);
					send_dson(pipe_client_send_to_server[1], write_dson);
					state_ = State::WaitServerShutdownOrder;
				}
				break;
			case Message::GoShutdown:
				{
					if (state_ != State::WaitServerShutdownOrder)
					{
						scope.print("Client:ERROR:state sequence is broken.");
						return;
					}
					scope.print("Client:received: Message::GoShutdown");
					write_dson.clear();
					write_dson.set_key(Message::WorkFinished);
					send_dson(pipe_client_send_to_server[1], write_dson);
					return;
				}
				break;
			default:
				scope.print(std::string{"Client:UNKNOWN message:"}.append(std::to_string(read_dson.key())));
				break;
			}
		}
		scope.print("Client:Finished");
	}; // client

	std::thread server_thread(server);
	std::thread client_thread(client);

	client_thread.join();
	server_thread.join();

	close(pipe_client_send_to_server[0]);
	close(pipe_client_send_to_server[1]);
	close(pipe_server_send_to_client[0]);
	close(pipe_server_send_to_client[1]);
}

/*
  1) Генерируем Dson сложной иерархии.
  2) Передаём по pipe в host byte order.
  3) Записываем на диск в network byte order.
  4) Грузим с диска, проверяем что идентичен.
*/
void ram_pipe_disk_ram()
{
	hi::CoutScope scope("ram_pipe_disk_ram");
	int pipe_client_send_to_server[2];
	// if (pipe(pipe_client_send_to_server) == -1)
	//{
	//     scope.print("ERROR: failed to open pipe");
	//     return;
	// }

	enum class Key : std::int32_t
	{
		IntVal,
		StringVal,
		VectorVal,
		DsonVal
	};

	std::vector<hi::Dson> data_vec;
	// for (int i = 0; i < 10; ++i)
	//{
	//   const std::int32_t val = std::rand();
	//   hi::Dson dson;
	//   dson.insert(Key::IntVal, val);
	//   dson.insert(Key::StringVal, std::to_string(val));
	//   data_vec.emplace_back(std::move(dson));
	// }

	// hi::Dson dson;
	// dson.insert(Key::VectorVal, std::move(data_vec));

	// hi::DsonObjMoved<std::vector<hi::Dson>> test{0, std::move(data_vec)};
	// hi::DsonObjMoved<std::vector<hi::Dson>> *test = new hi::DsonObjMoved<std::vector<hi::Dson>>(0,
	// std::move(data_vec));

	// close(pipe_client_send_to_server[0]);
	// close(pipe_client_send_to_server[1]);
}

int main(int /* argc */, char ** /* argv */)
{
	empty_messages();
	// ram_pipe_disk_ram();
	std::cout << "Tests finished" << std::endl;
	return 0;
}

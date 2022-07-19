#ifndef SOCKET_SERVER_H
#define SOCKET_SERVER_H

#include "raii_thread.h"
#include "router.h"
#include "cout_scope.h"

#include <dson/dson.h>

#include <deque>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>
#include <unistd.h>

#include <atomic>

class SocketServer
{
public:
	static constexpr const char * socket_name{"socket_name"};
	SocketServer(Router & router, hi::CoutScope & scope)
		: router_{router}
		, scope_{scope}
	{
		int socket = create_socket();
		worker_thread_ = hi::RAIIthread(std::thread(
			[&, socket]
			{
				server_loop(socket);
			}));
	}

	void stop()
	{
		keep_run_.store(false, std::memory_order_release);
		worker_thread_.join();
	}

private:
	void server_loop(int socket)
	{
		scope_.print("server_loop started");
		int connection = accept_connection(socket);
		scope_.print(std::string{"server_loop accept_connection:"}.append(std::to_string(connection)));
		if (connection < 0)
			return;
		hi::Dson dson;
		bool need_set_route{true};
		std::deque<hi::Dson> write_deque;

		const auto send_dson_from_deque = [&]()
		{
			while (!write_deque.empty())
			{
				const auto result = write_deque.front().copy_to_fd_network_order(connection);
				switch (result)
				{
				case hi::Result::Ready:
					write_deque.pop_front();
					break;
				case hi::Result::InProcess:
					return;
				case hi::Result::Error:
					keep_run_ = false;
					return;
				}
			}
		};

		const auto send_dson = [&](hi::Dson && dson)
		{
			write_deque.emplace_back(std::move(dson));
			send_dson_from_deque();
		};

		while (keep_run_.load(std::memory_order_acquire))
		{
			hi::Result res = dson.load_from_fd(connection);
			if (res == hi::Result::Error)
				return;
			if (res == hi::Result::Ready)
			{
				if (need_set_route)
				{
					auto address_obj = dson.get(router_.route_address_key());
					const auto address = hi::to_address(address_obj);
					assert(address);

					/*
					 * Чтобы пример оставался простым предполагаю
					 * что вызовы колбэка будут в одном потоке, поэтому добавление маршрута без мьютексов
					 */
					router_.add_route(
						address->from_cli_id,
						[&](hi::Dson && dson)
						{
							send_dson(std::move(dson));
						});
					need_set_route = false;
				}
				router_.route(std::move(dson));
				dson.clear();
			}
			send_dson_from_deque();
		} // while loop

		close(connection);
		scope_.print("server_loop finished");
	} // server_loop

	int accept_connection(int socket_descriptor)
	{
		struct sockaddr_un client;
		int connection;
		socklen_t length = sizeof client;

		// Start accepting connections on this socket and
		// receive a connection-specific socket for any
		// incoming socket
		// clang-format off
      connection = accept(
          socket_descriptor,
          (struct sockaddr*)&client,
          &length
      );
		// clang-format on

		if (connection == -1)
		{
			throw("Error accepting connection");
		}

		// adjust_socket_blocking_timeout(connection, 0, 1);
		if (set_io_flag(connection, O_NONBLOCK) == -1)
		{
			throw("Error setting socket to non-blocking on server-side");
		}

		// Don't need this one anymore (because we only have one connection)
		close(socket_descriptor);

		return connection;
	}

	int set_io_flag(int socket_fd, int flag)
	{
		int old_flags;

		// Get the old flags, because we must bitwise-OR our flag to add it
		// fnctl takes as arguments:
		// 1. The file fd to modify
		// 2. The command (e.g. F_GETFL, F_SETFL, ...)
		// 3. Arguments to that command (variadic)
		// For F_GETFL, the arguments are ignored (that's why we pass 0)
		if ((old_flags = fcntl(socket_fd, F_GETFL, 0)) == -1)
		{
			return -1;
		}

		if (fcntl(socket_fd, F_SETFL, old_flags | flag))
		{
			return -1;
		}

		return 0;
	}

	int create_socket()
	{
		// File descriptor for the socket
		int socket_descriptor;

		// Get a new socket from the OS
		// Arguments:
		// 1. The family of the socket (AF_UNIX for UNIX-domain sockets)
		// 2. The socket type, either stream-oriented (TCP) or
		//    datagram-oriented (UDP)
		// 3. The protocol for the given socket type. By passing 0, the
		//    OS will pick the right protocol for the job (TCP/UDP)
		socket_descriptor = socket(AF_UNIX, SOCK_STREAM, 0);

		if (socket_descriptor == -1)
		{
			throw("Error opening socket on server-side");
		}

		setup_socket(socket_descriptor);

		// Notify the client that it can connect to the socket now
		// server_once(NOTIFY);

		return socket_descriptor;
	}

	void setup_socket(int socket_descriptor)
	{
		int return_code;

		// The main datastructure for a UNIX-domain socket.
		// It only has two members:
		// 1. sun_family: The family of the socket. Should be AF_UNIX
		//                for UNIX-domain sockets (AF_LOCAL is the same,
		//                but AF_UNIX is POSIX).
		// 2. sun_path: Noting that a UNIX-domain socket ist just a
		//              file in the file-system, it also has a path.
		//              This may be any path the program has permission
		//              to create, read and write files in. The maximum
		//              size of such a path is 108 bytes.
		struct sockaddr_un address;

		// Set the family of the address struct
		address.sun_family = AF_UNIX;
		// Copy in the path
		strcpy(address.sun_path, socket_name);
		// Remove the socket if it already exists
		remove(address.sun_path);

		// Bind the socket to an address.
		// Arguments:
		// 1. The socket file-descriptor.
		// 2. A sockaddr struct, which we get by casting our address struct.
		// 3. The length of the struct, as computed by the SUN_LEN macro.
		// clang-format off
        return_code = bind(
            socket_descriptor,
            (struct sockaddr*)&address,
            SUN_LEN(&address)
        );
		// clang-format on

		if (return_code == -1)
		{
			throw("Error binding socket to address");
		}

		// Enable listening on this socket
		return_code = listen(socket_descriptor, 10);

		if (return_code == -1)
		{
			throw("Could not start listening on socket");
		}
	}

private:
	Router & router_;
	hi::CoutScope & scope_;
	hi::RAIIthread worker_thread_;
	std::atomic_bool keep_run_{true};
};

#endif // SOCKET_SERVER_H

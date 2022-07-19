#ifndef SOCKET_CLIENT_H
#define SOCKET_CLIENT_H

#include "socket_server.h"

class SocketClient
{
public:
	SocketClient()
		: connection_{create_connection()}
	{
	}

	~SocketClient()
	{
		if (connection_ >= 0)
		{
			close(connection_);
		}
	}

	void execute(std::function<void(std::int32_t)> logic)
	{
		logic(connection_);
	}

private:
	void setup_socket(int connection)
	{
		int return_code;

		// The main datastructure for a UNIX-domain socket.
		// It only has two members:
		// 1. sun_family: The family of the socket. Should be AF_UNIX
		//                for UNIX-domain sockets.
		// 2. sun_path: Noting that a UNIX-domain socket ist just a
		//              file in the file-system, it also has a path.
		//              This may be any path the program has permission
		//              to create, read and write files in. The maximum
		//              size of such a path is 108 bytes.

		struct sockaddr_un address;

		// For domain sockets blocking or not seems to make no
		// difference at all in terms of speed. Neither setting
		// the timeout nor not blocking at all.
		// adjust_socket_blocking_timeout(connection, 0, 1);
		if (set_io_flag(connection, O_NONBLOCK) == -1)
		{
			throw("Error setting socket to non-blocking on client-side");
		}

		// Set the family of the address struct
		address.sun_family = AF_UNIX;
		// Copy in the path
		strcpy(address.sun_path, SocketServer::socket_name);

		// Connect the socket to an address.
		// Arguments:
		// 1. The socket file-descriptor.
		// 2. A sockaddr struct describing the socket address to connect to.
		// 3. The length of the struct, as computed by the SUN_LEN macro.
		// clang-format off
      // Blocks until the connection is accepted by the other end.
      return_code = connect(
          connection,
          (struct sockaddr*)&address,
          SUN_LEN(&address)
      );
		// clang-format on

		if (return_code == -1)
		{
			throw("Error connecting to server");
		}
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

	int create_connection()
	{
		// The connection socket (file descriptor) that we will return
		int connection;

		// Get a new socket from the OS
		// Arguments:
		// 1. The family of the socket (AF_UNIX for UNIX-domain sockets)
		// 2. The socket type, either stream-oriented (TCP) or
		//    datagram-oriented (UDP)
		// 3. The protocol for the given socket type. By passing 0, the
		//    OS will pick the right protocol for the job (TCP/UDP)
		connection = socket(AF_UNIX, SOCK_STREAM, 0);

		if (connection == -1)
		{
			throw("Error opening socket on client-side");
		}

		setup_socket(connection);

		return connection;
	}

private:
	int connection_;
};

#endif // SOCKET_CLIENT_H

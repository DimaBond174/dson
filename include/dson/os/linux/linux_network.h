/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#ifndef LINUX_NETWORK_H
#define LINUX_NETWORK_H

#include <arpa/inet.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>

namespace hi
{

inline std::int64_t write_to_fd(std::int32_t fd, const void * buf, std::int64_t size)
{
	// todo обработать кейс когда int64_t больше чем может write
	const auto re = ::write(fd, buf, static_cast<size_t>(size));
	if (re < 0)
	{
		const auto err = errno;
		if (EAGAIN == err || EWOULDBLOCK == err)
		{
			return 0;
		}
		else
		{
			return -1;
		}
	}
	return re;
}

inline std::int64_t read_from_fd(std::int32_t fd, void * buf, std::int64_t buf_size)
{
	// todo обработать кейс когда int64_t больше чем может read
	const auto re = ::read(fd, buf, static_cast<size_t>(buf_size));
	if (re < 0)
	{
		const auto err = errno;
		if (EAGAIN == err || EWOULDBLOCK == err)
		{
			return 0;
		}
		else
		{
			return -1;
		}
	}
	return re;
}

} // namespace hi

#endif // LINUX_NETWORK_H

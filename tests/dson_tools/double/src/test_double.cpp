/*
 * This is the source code of thread_highways library
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: bondarenkoda@gmail.com
 */

#include <dson/custom_dson_objs/dson_route_obj.h>
#include <dson/dson.h>
#include <dson/from_dson_converters.h>

#include <gtest/gtest.h>

#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace hi
{
namespace
{

std::vector<double> test_data{
	0.0,
	-1.0,
	1.0,
	-12345.56789,
	12345.56789,
	std::numeric_limits<double>::min(),
	std::numeric_limits<double>::max(),
	std::numeric_limits<double>::lowest()};

TEST(TestDouble, DsonToolsToNetworkToHost)
{
	char buf[buf_size_for_double];
	const auto to_network_to_host = [&](const double val) -> double
	{
		auto data = std::launder(reinterpret_cast<double *>(buf));
		*data = val;
		double_in_buf_to_network_order(buf);
		double_in_buf_to_host_order(buf);
		return *data;
	};

	EXPECT_TRUE(std::isnan(to_network_to_host(0.0 / 0.0)));
	EXPECT_TRUE(std::isinf(to_network_to_host(-std::numeric_limits<double>::infinity())));
	EXPECT_TRUE(std::isinf(to_network_to_host(std::numeric_limits<double>::infinity())));

	for (const auto it : test_data)
	{
		EXPECT_EQ(it, to_network_to_host(it));
	}
}

TEST(TestDouble, DsonFdSendHostOrder)
{
	int pipe_client_send_to_server[2];
	EXPECT_TRUE(pipe(pipe_client_send_to_server) != -1);

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

	std::thread sender(
		[&]
		{
			for (const auto it : test_data)
			{
				Dson dson(it);
				send_dson(pipe_client_send_to_server[1], dson);
			}
		});

	for (const auto it : test_data)
	{
		Dson dson;
		load_dson(pipe_client_send_to_server[0], dson);
		EXPECT_EQ(it, to_double(&dson));
	}

	sender.join();

	close(pipe_client_send_to_server[0]);
	close(pipe_client_send_to_server[1]);
}

TEST(TestDouble, DsonFdSendNetworkOrder)
{
	int pipe_client_send_to_server[2];
	EXPECT_TRUE(pipe(pipe_client_send_to_server) != -1);

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
			result = dson.copy_to_fd_network_order(fd);
		}
		return (hi::Result::Error != result);
	};

	std::thread sender(
		[&]
		{
			for (const auto it : test_data)
			{
				Dson dson(it);
				send_dson(pipe_client_send_to_server[1], dson);
			}
		});

	for (const auto it : test_data)
	{
		Dson dson;
		load_dson(pipe_client_send_to_server[0], dson);
		EXPECT_EQ(it, to_double(&dson));
	}

	sender.join();

	close(pipe_client_send_to_server[0]);
	close(pipe_client_send_to_server[1]);
}

TEST(TestDouble, DsonFdSendHostOrderComposite)
{
	int pipe_client_send_to_server[2];
	EXPECT_TRUE(pipe(pipe_client_send_to_server) != -1);

	enum class Key : std::int32_t
	{
		RouteAddress,
		TestDouble
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

	std::thread sender(
		[&]
		{
			Dson dson;
			dson.emplace(std::make_unique<hi::DsonRouteObj>(Key::RouteAddress));
			for (const auto it : test_data)
			{
				dson.emplace(Key::TestDouble, it);
				send_dson(pipe_client_send_to_server[1], dson);
			}
		});

	for (const auto it : test_data)
	{
		Dson dson;
		load_dson(pipe_client_send_to_server[0], dson);
		EXPECT_EQ(it, to_double(dson.get(Key::TestDouble)));
	}

	sender.join();

	close(pipe_client_send_to_server[0]);
	close(pipe_client_send_to_server[1]);
}

TEST(TestDouble, DsonFdSendNetworkOrderComposite)
{
	int pipe_client_send_to_server[2];
	EXPECT_TRUE(pipe(pipe_client_send_to_server) != -1);

	enum class Key : std::int32_t
	{
		RouteAddress,
		TestDouble
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
			result = dson.copy_to_fd_network_order(fd);
		}
		return (hi::Result::Error != result);
	};

	std::thread sender(
		[&]
		{
			Dson dson;
			dson.emplace(std::make_unique<hi::DsonRouteObj>(Key::RouteAddress));
			for (const auto it : test_data)
			{
				dson.emplace(Key::TestDouble, it);
				send_dson(pipe_client_send_to_server[1], dson);
			}
		});

	for (const auto it : test_data)
	{
		Dson dson;
		load_dson(pipe_client_send_to_server[0], dson);
		EXPECT_EQ(it, to_double(dson.get(Key::TestDouble)));
	}

	sender.join();

	close(pipe_client_send_to_server[0]);
	close(pipe_client_send_to_server[1]);
}

TEST(TestDouble, DsonBufSendHostOrder)
{
	const std::int32_t buf_size{10};
	char buf[buf_size];
	bool buf_ready{false};
	std::mutex mut;
	std::condition_variable cv;

	const auto load_dson = [&](hi::Dson & dson) -> bool
	{
		dson.clear();
		std::unique_lock lk{mut};
		hi::Result result{hi::Result::InProcess};
		while (hi::Result::InProcess == result)
		{
			cv.wait_for(
				lk,
				std::chrono::milliseconds(10),
				[&]
				{
					return buf_ready;
				});
			if (!buf_ready)
				continue;
			result = dson.load_from_buf(buf, buf_size);
			buf_ready = false;
		}
		cv.notify_all();
		return (hi::Result::Error != result);
	};

	const auto send_dson = [&](hi::Dson & dson) -> bool
	{
		hi::Result result{hi::Result::InProcess};
		std::unique_lock lk{mut};
		while (hi::Result::InProcess == result)
		{
			cv.wait_for(
				lk,
				std::chrono::milliseconds(10),
				[&]
				{
					return !buf_ready;
				});
			if (buf_ready)
				continue;
			char * ptr = buf;
			std::int32_t writed{buf_size};
			result = dson.copy_to_buf_host_order(ptr, writed);
			buf_ready = true;
		}
		cv.notify_all();
		return (hi::Result::Error != result);
	};

	std::thread sender(
		[&]
		{
			for (const auto it : test_data)
			{
				Dson dson(it);
				send_dson(dson);
			}
		});

	Dson dson;
	for (const auto it : test_data)
	{
		load_dson(dson);
		EXPECT_EQ(it, to_double(&dson));
	}

	sender.join();
}

TEST(TestDouble, DsonBufSendNetworkOrder)
{
	const std::int32_t buf_size{10};
	char buf[buf_size];
	bool buf_ready{false};
	std::mutex mut;
	std::condition_variable cv;

	const auto load_dson = [&](hi::Dson & dson) -> bool
	{
		dson.clear();
		std::unique_lock lk{mut};
		hi::Result result{hi::Result::InProcess};
		while (hi::Result::InProcess == result)
		{
			cv.wait_for(
				lk,
				std::chrono::milliseconds(10),
				[&]
				{
					return buf_ready;
				});
			if (!buf_ready)
				continue;
			result = dson.load_from_buf(buf, buf_size);
			buf_ready = false;
		}
		cv.notify_all();
		return (hi::Result::Error != result);
	};

	const auto send_dson = [&](hi::Dson & dson) -> bool
	{
		hi::Result result{hi::Result::InProcess};
		std::unique_lock lk{mut};
		while (hi::Result::InProcess == result)
		{
			cv.wait_for(
				lk,
				std::chrono::milliseconds(10),
				[&]
				{
					return !buf_ready;
				});
			if (buf_ready)
				continue;
			char * ptr = buf;
			std::int32_t writed{buf_size};
			result = dson.copy_to_buf_network_order(ptr, writed);
			buf_ready = true;
		}
		cv.notify_all();
		return (hi::Result::Error != result);
	};

	std::thread sender(
		[&]
		{
			for (const auto it : test_data)
			{
				Dson dson(it);
				send_dson(dson);
			}
		});

	Dson dson;
	for (const auto it : test_data)
	{
		load_dson(dson);
		EXPECT_EQ(it, to_double(&dson));
	}

	sender.join();
}

} // namespace
} // namespace hi

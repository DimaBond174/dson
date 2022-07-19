#define USE_USER_DSON_TYPES

#include <dson/include_all.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iostream>

class VeryBigStruct;
template <>
struct hi::types_map<VeryBigStruct> : register_id<VeryBigStruct, 54322>
{
};

/*
  Структура с большим объёмом данных, копирование в Dson стоит дорого.
  Поэтому не копируем.
*/
class VeryBigStruct : public hi::DsonObj
{
public:
	VeryBigStruct()
	{
		setup_header();
	}

public:
	std::string very_big_data;
	std::uint16_t param1;
	std::uint16_t param2;
	std::uint16_t param3;

public: // hi::DsonObj
	bool is_host_order() const noexcept override
	{
		return header()->mark_byte_order_ == hi::mark_host_order;
	}

	bool is_network_order() const noexcept override
	{
		return header()->mark_byte_order_ == hi::mark_network_order;
	}

	std::int32_t data_size() const noexcept override
	{
		return network_ordered_buf_size + static_cast<std::int32_t>(very_big_data.size());
	}

	std::int32_t key() const noexcept override
	{
		Header * _header = header();
		if (_header->mark_byte_order_ == hi::mark_host_order)
			return _header->key_;
		return hi::int32_to_host(header_as_array()[2]);
	}

	void set_key(std::int32_t _key) noexcept override
	{
		Header * _header = header();
		if (_header->mark_byte_order_ == hi::mark_host_order)
		{
			_header->key_ = _key;
		}
		else
		{
			header_as_array()[2] = hi::int32_to_network(_key);
		}
	}

	std::int32_t data_type() const noexcept override
	{
		return hi::types_map<VeryBigStruct>::value;
	}

	void copy_to_stream_host_order(std::ofstream & out) override
	{
		to_host_order();
		copy_to_stream_local(out);
	}

	void copy_to_stream_network_order(std::ofstream & out) override
	{
		to_network_order();
		copy_to_stream_local(out);
	}

	hi::Result copy_to_fd_host_order(std::int32_t fd) override
	{
		if (offset_ == 0)
		{
			to_host_order();
			state_ = State::CopyingHeader;
			offset_ = 0;
		}
		return copy_to_fd_local(fd);
	}

	hi::Result copy_to_fd_network_order(std::int32_t fd) override
	{
		if (state_ == State::Ready)
		{
			to_network_order();
			state_ = State::CopyingHeader;
			offset_ = 0;
		}
		return copy_to_fd_local(fd);
	}

	hi::Result copy_to_buf_host_order(char *& buf, std::int32_t & buf_size) override
	{
		if (state_ == State::Ready)
		{
			to_host_order();
			state_ = State::CopyingHeader;
			offset_ = 0;
		}
		return copy_to_buf_local(buf, buf_size);
	}

	hi::Result copy_to_buf_network_order(char *& buf, std::int32_t & buf_size) override
	{
		if (state_ == State::Ready)
		{
			to_network_order();
			state_ = State::CopyingHeader;
			offset_ = 0;
		}
		return copy_to_buf_local(buf, buf_size);
	}

	State state() const noexcept override
	{
		return state_;
	}
	void reset_state() noexcept override
	{
		state_ = State::Ready;
	}

private:
	Header * header() const noexcept
	{
		return std::launder(reinterpret_cast<Header *>(buf_));
	}

	void setup_header()
	{
		Header * _header = header();
		_header->mark_byte_order_ = hi::mark_host_order;
		_header->data_size_ = 0;
		_header->key_ = 0;
		_header->data_type_ = hi::types_map<VeryBigStruct>::value;
	}

	std::uint32_t * header_as_array() const noexcept
	{
		return std::launder(reinterpret_cast<std::uint32_t *>(buf_));
	}

	std::uint32_t * address_as_array() const noexcept
	{
		return std::launder(reinterpret_cast<std::uint32_t *>(buf_ + header_size));
	}

	void to_host_order()
	{
		if (!is_host_order())
		{
			std::uint32_t * array = std::launder(reinterpret_cast<std::uint32_t *>(buf_));
			for (std::int32_t i = 0; i < header_array_len; ++i)
			{
				array[i] = ntohl(array[i]);
			}
		}
		prepare_data();
	}

	void to_network_order()
	{
		if (!is_network_order())
		{
			std::uint32_t * array = std::launder(reinterpret_cast<std::uint32_t *>(buf_));
			for (std::int32_t i = 0; i < header_array_len; ++i)
			{
				array[i] = htonl(array[i]);
			}
		}
		prepare_data();
	}

	void copy_to_stream_local(std::ofstream & out) const
	{
		std::copy(buf_, buf_ + buf_size, std::ostream_iterator<char>(out));
		std::copy(very_big_data.begin(), very_big_data.end(), std::ostream_iterator<char>(out));
	}

	hi::Result copy_to_fd_local(std::int32_t fd)
	{
		switch (state_)
		{
		case State::CopyingHeader:
			{
				if (offset_ >= buf_size)
				{
					assert(false);
					return hi::Result::Error;
				}
				const std::int32_t writed = hi::write_to_fd(fd, buf_ + offset_, buf_size - offset_);
				switch (writed)
				{
				case -1:
					return hi::Result::Error;
				case 0:
					return hi::Result::InProcess;
				default:
					break;
				}
				offset_ += writed;
				if (offset_ < buf_size)
					return hi::Result::InProcess;
				state_ = State::CopyingData;
				offset_ = 0;
			}
			[[fallthrough]];
		case State::CopyingData:
			{
				std::int32_t size = static_cast<std::int32_t>(very_big_data.size());
				if (offset_ >= size)
				{
					assert(false);
					return hi::Result::Error;
				}
				const std::int32_t writed = hi::write_to_fd(fd, very_big_data.data() + offset_, size - offset_);
				switch (writed)
				{
				case -1:
					return hi::Result::Error;
				case 0:
					return hi::Result::InProcess;
				default:
					break;
				}
				offset_ += writed;
				if (offset_ < size)
					return hi::Result::InProcess;
				state_ = State::Ready;
				return hi::Result::Ready;
			}
		default:
			break;
		}
		return hi::Result::Error;
	}

	hi::Result copy_to_buf_local(char *& buf, std::int32_t & buf_size)
	{
		switch (state_)
		{
		case State::CopyingHeader:
			{
				if (offset_ >= buf_size)
				{
					assert(false);
					return hi::Result::Error;
				}
				std::int32_t writed = buf_size - offset_;
				if (writed > buf_size)
					writed = buf_size;
				std::memcpy(buf, buf_ + offset_, writed);
				offset_ += writed;
				buf += writed;
				buf_size -= writed;
				if (offset_ < buf_size)
					return hi::Result::InProcess;
				state_ = State::CopyingData;
				offset_ = 0;
			}
			[[fallthrough]];
		case State::CopyingData:
			{
				std::int32_t size = static_cast<std::int32_t>(very_big_data.size());
				if (offset_ >= size)
				{
					assert(false);
					return hi::Result::Error;
				}
				std::int32_t writed = size - offset_;
				if (writed > buf_size)
					writed = buf_size;
				std::memcpy(buf, very_big_data.data() + offset_, writed);
				offset_ += writed;
				buf += writed;
				buf_size -= writed;
				if (offset_ < size)
					return hi::Result::InProcess;
				state_ = State::Ready;
				return hi::Result::Ready;
			}
		default:
			break;
		}
		return hi::Result::Error;
	}

	/*
	 * Можно менять данные на месте в хранимых полях - но тогда остальная логика
	 * работы с VeryBigStruct должна действовать с оглядкой на network order.
	 *
	 * Так как объём данных чувствительных к network order небольшой, их сразу копируем в буфер отправки buf_.
	 */
	void prepare_data()
	{
		std::uint16_t * ptr = std::launder(reinterpret_cast<std::uint16_t *>(buf_ + sizeof(Header)));
		if (is_host_order())
		{
			*ptr = param1;
			++ptr;
			*ptr = param2;
			++ptr;
			*ptr = param3;
			++ptr;
			*std::launder(reinterpret_cast<std::uint32_t *>(ptr)) = very_big_data.size();
			header()->data_size_ = static_cast<std::int32_t>(very_big_data.size()) + network_ordered_buf_size;
		}
		else
		{
			*ptr = htons(param1);
			++ptr;
			*ptr = htons(param2);
			++ptr;
			*ptr = htons(param3);
			++ptr;
			*std::launder(reinterpret_cast<std::uint32_t *>(ptr)) =
				htonl(static_cast<std::uint32_t>(very_big_data.size()));
			header_as_array()[1] =
				hi::int32_to_network(static_cast<std::int32_t>(very_big_data.size()) + network_ordered_buf_size);
		}
	}

private:
	static constexpr std::int32_t network_ordered_buf_size{
		sizeof(param1) + sizeof(param2) + sizeof(param3) + sizeof(std::uint32_t)};
	static constexpr std::int32_t buf_size{network_ordered_buf_size + sizeof(Header)};
	mutable char buf_[buf_size];
};

void very_big_struct_converters(
	hi::Dson::Converters::ConvertersMap & to_host_order,
	hi::Dson::Converters::ConvertersMap & to_network_order)
{
	const auto dummy_key = hi::types_map<VeryBigStruct>::value;
	to_host_order.emplace(
		dummy_key,
		[](hi::Dson::Header &, char * buf)
		{
			std::uint16_t * ptr = std::launder(reinterpret_cast<std::uint16_t *>(buf));
			*ptr = ntohs(*ptr);
			++ptr;
			*ptr = ntohs(*ptr);
			++ptr;
			*ptr = ntohs(*ptr);
			++ptr;
			std::uint32_t * ptr2 = std::launder(reinterpret_cast<std::uint32_t *>(ptr));
			*ptr2 = ntohl(*ptr2);
		});

	to_network_order.emplace(
		dummy_key,
		[](hi::Dson::Header &, char * buf)
		{
			std::uint16_t * ptr = std::launder(reinterpret_cast<std::uint16_t *>(buf));
			*ptr = htons(*ptr);
			++ptr;
			*ptr = htons(*ptr);
			++ptr;
			*ptr = htons(*ptr);
			++ptr;
			std::uint32_t * ptr2 = std::launder(reinterpret_cast<std::uint32_t *>(ptr));
			*ptr2 = htonl(*ptr2);
		});
}

VeryBigStruct to_my_struct(hi::DsonObj * dson_obj)
{
	VeryBigStruct re;
	if (!dson_obj)
		return re;
	auto dson = dynamic_cast<hi::Dson *>(dson_obj);
	if (!dson)
		return re;
	char * buf = static_cast<char *>(dson->data());
	if (!buf)
		return re;
	std::uint16_t * ptr = std::launder(reinterpret_cast<std::uint16_t *>(buf));
	re.param1 = *ptr;
	++ptr;
	re.param2 = *ptr;
	++ptr;
	re.param3 = *ptr;
	++ptr;
	std::uint32_t * ptr2 = std::launder(reinterpret_cast<std::uint32_t *>(ptr));
	std::uint32_t size = *ptr2;
	++ptr2;
	re.very_big_data = std::string{std::launder(reinterpret_cast<char *>(ptr2)), size};
	return re;
}

void save_and_load_advanced()
{
	VeryBigStruct struct_before;
	struct_before.very_big_data = "very_big_data_!_!_!_!_!_!_!_!_...";
	struct_before.param1 = 9;
	struct_before.param2 = 8;
	struct_before.param3 = 7;
	struct_before.set_key(12345);

	{
		std::ofstream output("dson.bin", std::ios::binary);
		output << struct_before;
	}

	hi::Dson dson_from_stream;
	{
		std::ifstream input("dson.bin", std::ios::binary);
		input >> dson_from_stream;
	}
	auto struct_after = to_my_struct(dson_from_stream.get(12345));
	assert(
		struct_before.very_big_data == struct_after.very_big_data && struct_before.param1 == struct_after.param1
		&& struct_before.param2 == struct_after.param2 && struct_before.param3 == struct_after.param3);

	std::cout << "Loaded from stream:" << std::endl
			  << "VeryBigStruct {" << std::endl
			  << "\t very_big_data = " << struct_after.very_big_data << std::endl
			  << "\t param1 = " << struct_after.param1 << std::endl
			  << "\t param2 = " << struct_after.param2 << std::endl
			  << "\t param3 = " << struct_after.param3 << std::endl
			  << "}" << std::endl;

	// Попробуем POSIX загрузку
	int fd = open("./dson.bin", O_RDONLY);
	if (-1 == fd)
	{
		perror("Open Failed");
		return;
	}
	hi::Dson dson_from_fd;
	while (true)
	{
		auto result = dson_from_fd.load_from_fd(fd);
		if (result == hi::Result::Error)
		{
			perror("Read Failed");
			return;
		}
		if (result == hi::Result::Ready)
			break;
	}
	auto struct_after2 = to_my_struct(dson_from_fd.get(12345));
	assert(
		struct_before.very_big_data == struct_after2.very_big_data && struct_before.param1 == struct_after2.param1
		&& struct_before.param2 == struct_after2.param2 && struct_before.param3 == struct_after2.param3);

	std::cout << "Loaded from fd:" << std::endl
			  << "VeryBigStruct {" << std::endl
			  << "\t very_big_data = " << struct_after2.very_big_data << std::endl
			  << "\t param1 = " << struct_after2.param1 << std::endl
			  << "\t param2 = " << struct_after2.param2 << std::endl
			  << "\t param3 = " << struct_after2.param3 << std::endl
			  << "}" << std::endl;
} // save_and_load_advanced

void save_and_load_advanced_composite()
{
	enum class Key
	{
		ChunkId,
		VeryBigStruct,
		StringAsDson,
		StringAsDsonObj,
		StringView
	};
	hi::Dson composite;
	VeryBigStruct very_big_struct;
	very_big_struct.very_big_data = "very_big_data_!_!_!_!_!_!_!_!_...";
	very_big_struct.param1 = 9;
	very_big_struct.param2 = 8;
	very_big_struct.param3 = 7;
	composite.emplace(Key::VeryBigStruct, std::move(very_big_struct));
	composite.emplace(Key::ChunkId, 12345);
	composite.emplace(Key::StringAsDson, std::string{"Small Ad hoc string cheap to copy"});
	composite.emplace(hi::to_dson_obj(Key::StringAsDsonObj, std::string{"Big String want to move"}));
	static std::string_view mit_license{"The above copyright notice and this permission notice shall be included in "
										"all copies or substantial portions of the Software. ..."};
	composite.emplace(Key::StringView, mit_license);

	int fd = open("./big_dson.bin", O_CREAT | O_TRUNC | O_WRONLY);
	if (-1 == fd)
	{
		perror("Open Failed");
		return;
	}

	while (true)
	{
		auto result = composite.copy_to_fd_network_order(fd);
		if (result == hi::Result::Error)
		{
			perror("Write Failed");
			return;
		}
		if (result == hi::Result::Ready)
			break;
	}
	close(fd);

	hi::Dson dson_from_stream;
	{
		std::ifstream input("big_dson.bin", std::ios::binary);
		input >> dson_from_stream;
	}
	auto struct_after = to_my_struct(dson_from_stream.get(Key::VeryBigStruct));
	std::cout << "Composite Loaded from stream:" << std::endl
			  << "VeryBigStruct {" << std::endl
			  << "\t very_big_data = " << struct_after.very_big_data << std::endl
			  << "\t param1 = " << struct_after.param1 << std::endl
			  << "\t param2 = " << struct_after.param2 << std::endl
			  << "\t param3 = " << struct_after.param3 << std::endl
			  << "}," << std::endl
			  << "ChunkId = " << hi::to_string(dson_from_stream.get(Key::ChunkId)) << std::endl
			  << "StringAsDson = " << hi::to_string(dson_from_stream.get(Key::StringAsDson)) << std::endl
			  << "StringAsDsonObj = " << hi::to_string(dson_from_stream.get(Key::StringAsDsonObj)) << std::endl
			  << "StringView = " << hi::to_string(dson_from_stream.get(Key::StringView)) << std::endl;
} // save_and_load_advanced_composite

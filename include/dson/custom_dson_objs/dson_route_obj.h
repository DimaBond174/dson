#ifndef DSON_ROUTE_OBJ_H
#define DSON_ROUTE_OBJ_H

#include <dson/dson.h>

namespace hi
{

class DsonRouteObj : public DsonObj
{
public:
	template <typename K>
	DsonRouteObj(K key)
	{
		setup_header(static_cast<std::int32_t>(key));
	}

	struct Address
	{
		/*
		 * Откуда сообщение (обратный адрес сервера к которому подключен адресат (кто может доставить))
		 * Сервер если видит from_serv_id == 0, то проставляет себя
		 */
		uint32_t from_serv_id{0};

		/*
		 * Откуда сообщение (обратный адресат)
		 * Сервер если видит from_cli_id_ == 0, то проставляет
		 * в исходящих себя
		 * во входящих id подключившегося (id сертификата авторизации)
		 * Уровень авторизации (админ/юзер/..) понятен по id сертификата авторизации
		 */
		uint32_t from_cli_id{0};

		/*
		 * Куда сообщение (адрес сервера который может смаршрутизировать доставку)
		 * Сервер если видит to_serv_id_ == 0, то проставляет себя (внутренняя маршрутизация)
		 */
		uint32_t to_serv_id{0};

		/*
		 * Куда сообщение (адресат)
		 * Сервер если видит to_cli_id_ == 0, то проставляет
		 * во входящих себя (например команда серверу или сообщение об инциденте)
		 * в исходящих удаляет сообщение как ошибочное
		 */
		uint32_t to_cli_id{0};
	};
	static constexpr std::int32_t address_size{sizeof(Address)};
	static constexpr std::int32_t header_size_address_size{sizeof(Address) + sizeof(Header)};
	static constexpr std::int32_t buf_array_len{(sizeof(Address) + sizeof(Header)) / sizeof(std::uint32_t)};

	/**
	 * @brief object
	 * Доступ к хранящемуся объекту
	 * @return объект
	 */
	Address * address()
	{
		to_host_order();
		return std::launder(reinterpret_cast<Address *>(buf_ + header_size));
	}

	/**
	 * @brief set_reverse_address
	 * Проставить обратный адрес
	 * @param from адрес отправителя
	 */
	void set_reverse_address(Address * from)
	{
		if (!from)
			return;
		auto _address = address();
		_address->to_cli_id = from->from_cli_id;
		_address->to_serv_id = from->from_serv_id;

		_address->from_cli_id = from->to_cli_id;
		_address->from_serv_id = from->to_serv_id;
	}

	/**
	 * @brief DsonRouteObj
	 * @param key ключ с которым сохраняется маршрут
	 * @param from адрес отправителя с которого будет скопирован
	 * обратный адрес
	 */
	template <typename K>
	DsonRouteObj(K key, Address * from)
	{
		setup_header(static_cast<std::int32_t>(key));
		set_reverse_address(from);
	}

public: // DsonObj
	bool is_host_order() const noexcept override
	{
		return header()->mark_byte_order_ == mark_host_order;
	}

	bool is_network_order() const noexcept override
	{
		return header()->mark_byte_order_ == mark_network_order;
	}

	std::int32_t data_size() const noexcept override
	{
		return static_cast<std::int32_t>(sizeof(Address));
	}

	std::int32_t key() const noexcept override
	{
		Header * _header = header();
		if (_header->mark_byte_order_ == mark_host_order)
			return _header->key_;
		return int32_to_host(header_as_array()[2]);
	}

	void set_key(std::int32_t _key) noexcept override
	{
		Header * _header = header();
		if (_header->mark_byte_order_ == mark_host_order)
		{
			_header->key_ = _key;
		}
		else
		{
			header_as_array()[2] = int32_to_network(_key);
		}
	}

	std::int32_t data_type() const noexcept override
	{
		return types_map<std::vector<std::uint32_t>>::value;
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

	Result copy_to_fd_host_order(std::int32_t fd) override
	{
		if (offset_ == 0)
		{
			to_host_order();
			state_ = State::CopyingHeader;
			offset_ = 0;
		}
		return copy_to_fd_local(fd);
	}

	Result copy_to_fd_network_order(std::int32_t fd) override
	{
		if (state_ == State::Ready)
		{
			to_network_order();
			state_ = State::CopyingHeader;
			offset_ = 0;
		}
		return copy_to_fd_local(fd);
	}

	Result copy_to_buf_host_order(char *& buf, std::int32_t & buf_size) override
	{
		if (state_ == State::Ready)
		{
			to_host_order();
			state_ = State::CopyingHeader;
			offset_ = 0;
		}
		return copy_to_buf_local(buf, buf_size);
	}

	Result copy_to_buf_network_order(char *& buf, std::int32_t & buf_size) override
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

	void setup_header(const std::int32_t key)
	{
		Header * _header = header();
		_header->mark_byte_order_ = mark_host_order;
		_header->data_size_ = static_cast<std::int32_t>(sizeof(Address));
		_header->key_ = key;
		_header->data_type_ = types_map<std::vector<std::uint32_t>>::value;
		Address * _address = address();
		_address->from_serv_id = 0;
		_address->from_cli_id = 0;
		_address->to_serv_id = 0;
		_address->to_cli_id = 0;
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
		if (is_host_order())
			return;
		// Сразу преобразую весь буфер
		std::uint32_t * array = std::launder(reinterpret_cast<std::uint32_t *>(buf_));
		for (std::int32_t i = 0; i < buf_array_len; ++i)
		{
			array[i] = ntohl(array[i]);
		}
	}

	void to_network_order()
	{
		if (is_network_order())
			return;
		// Сразу преобразую весь буфер
		std::uint32_t * array = std::launder(reinterpret_cast<std::uint32_t *>(buf_));
		for (std::int32_t i = 0; i < buf_array_len; ++i)
		{
			array[i] = htonl(array[i]);
		}
	}

	void copy_to_stream_local(std::ofstream & out) const
	{
		std::copy(buf_, buf_ + header_size_address_size, std::ostream_iterator<char>(out));
	}

	Result copy_to_fd_local(std::int32_t fd)
	{
		switch (state_)
		{
		case State::CopyingHeader:
			{
				if (offset_ >= header_size_address_size)
				{
					assert(false);
					return Result::Error;
				}
				const std::int32_t writed = write_to_fd(fd, buf_ + offset_, header_size_address_size - offset_);
				switch (writed)
				{
				case -1:
					return Result::Error;
				case 0:
					return Result::InProcess;
				default:
					break;
				}
				offset_ += writed;
				if (offset_ < header_size_address_size)
					return Result::InProcess;
				state_ = State::Ready;
				return Result::Ready;
			}
		default:
			break;
		}
		return Result::Error;
	}

	Result copy_to_buf_local(char *& buf, std::int32_t & buf_size)
	{
		switch (state_)
		{
		case State::CopyingHeader:
			{
				if (offset_ >= header_size_address_size)
				{
					assert(false);
					return Result::Error;
				}
				std::int32_t writed = header_size_address_size - offset_;
				if (writed > buf_size)
					writed = buf_size;
				std::memcpy(buf, buf_ + offset_, writed);
				offset_ += writed;
				buf += writed;
				buf_size -= writed;
				if (offset_ < header_size_address_size)
					return Result::InProcess;
				state_ = State::Ready;
				return Result::Ready;
			}
		default:
			break;
		}
		return Result::Error;
	}

private:
	alignas(std::uint32_t) mutable char buf_[sizeof(Header) + sizeof(Address)];
};

DsonRouteObj::Address * to_address(DsonObj * obj)
{
	if (!obj)
		return nullptr;
	if (auto dson = dynamic_cast<Dson *>(obj))
	{
		Dson::converters().to_host(*dson);
		return static_cast<DsonRouteObj::Address *>(dson->data());
	}
	if (auto dson = dynamic_cast<DsonRouteObj *>(obj))
	{
		return dson->address();
	}
	return nullptr;
}

} // namespace hi

#endif // DSON_ROUTE_OBJ_H

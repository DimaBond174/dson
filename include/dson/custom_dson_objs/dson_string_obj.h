#ifndef DSON_STRING_OBJ_H
#define DSON_STRING_OBJ_H

#include <dson/impl/dson_obj.h>

#include <memory>

namespace hi
{

class DsonStringObj : public DsonObj
{
public:
	DsonStringObj(const DsonKey key, std::string && object)
		: object_{std::move(object)}
	{
		setup_header(key);
	}

	/**
	 * @brief object
	 * Доступ к хранящемуся объекту
	 * @return объект
	 */
	const std::string & object()
	{
		return object_;
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
		Header * _header = header();
		if (_header->mark_byte_order_ == mark_host_order)
			return _header->data_size_;
		return int32_to_host(header_as_array()[1]);
	}

	DsonKey key() const noexcept override
	{
		Header * _header = header();
		if (_header->mark_byte_order_ == mark_host_order)
			return _header->key_;
		return int32_to_host(header_as_array()[2]);
	}

	void set_key(DsonKey _key) noexcept override
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

	TypeMarker data_type() const noexcept override
	{
		Header * _header = header();
		if (_header->mark_byte_order_ == mark_host_order)
			return _header->data_type_;
		return int32_to_host(header_as_array()[3]);
	}

	void copy_to_stream_host_order(std::ofstream & out) override
	{
		prepare_header_host_order();
		copy_to_stream_local(out);
	}

	void copy_to_stream_network_order(std::ofstream & out) override
	{
		prepare_header_network_order();
		copy_to_stream_local(out);
	}

	Result copy_to_fd_host_order(std::int32_t fd) override
	{
		if (state_ == State::Ready)
		{
			prepare_header_host_order();
			state_ = State::CopyingHeader;
			offset_ = 0;
		}
		return copy_to_fd_local(fd);
	}

	Result copy_to_fd_network_order(std::int32_t fd) override
	{
		if (state_ == State::Ready)
		{
			prepare_header_network_order();
			state_ = State::CopyingHeader;
			offset_ = 0;
		}
		return copy_to_fd_local(fd);
	}

	Result copy_to_buf_host_order(char *& buf, std::int32_t & buf_size) override
	{
		if (state_ == State::Ready)
		{
			prepare_header_host_order();
			state_ = State::CopyingHeader;
			offset_ = 0;
		}
		return copy_to_buf_local(buf, buf_size);
	}

	Result copy_to_buf_network_order(char *& buf, std::int32_t & buf_size) override
	{
		if (state_ == State::Ready)
		{
			prepare_header_network_order();
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
		return std::launder(reinterpret_cast<Header *>(header_));
	}

	void setup_header(const std::int32_t key)
	{
		Header * _header = header();
		_header->mark_byte_order_ = mark_host_order;
		_header->data_size_ = static_cast<std::int32_t>(object_.size());
		_header->key_ = key;
		_header->data_type_ = types_map<std::string>::value;
	}

	std::uint32_t * header_as_array() const noexcept
	{
		return std::launder(reinterpret_cast<std::uint32_t *>(header_));
	}

	void prepare_header_host_order()
	{
		if (is_host_order())
			return;
		std::uint32_t * _header = header_as_array();
		for (std::int32_t i = 0; i < header_array_len; ++i)
		{
			_header[i] = ntohl(_header[i]);
		}
	}

	void prepare_header_network_order()
	{
		if (is_network_order())
			return;
		std::uint32_t * _header = header_as_array();
		for (std::int32_t i = 0; i < header_array_len; ++i)
		{
			_header[i] = htonl(_header[i]);
		}
	}

	void copy_to_stream_local(std::ofstream & out) const
	{
		std::copy(header_, header_ + header_size, std::ostream_iterator<char>(out));
		std::copy(object_.begin(), object_.end(), std::ostream_iterator<char>(out));
	}

	Result copy_to_fd_local(std::int32_t fd)
	{
		switch (state_)
		{
		case State::CopyingHeader:
			{
				if (offset_ >= header_size)
				{
					assert(false);
					return Result::Error;
				}
				const std::int32_t writed = write_to_fd(fd, header_ + offset_, header_size - offset_);
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
				if (offset_ < header_size)
					return Result::InProcess;
				state_ = State::CopyingData;
				offset_ = 0;
			}
			[[fallthrough]];
		case State::CopyingData:
			{
				std::int32_t size = static_cast<std::int32_t>(object_.size());
				if (offset_ >= size)
				{
					assert(false);
					return Result::Error;
				}
				const std::int32_t writed = write_to_fd(fd, object_.data() + offset_, size - offset_);
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
				if (offset_ < size)
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
				if (offset_ >= header_size)
				{
					assert(false);
					return Result::Error;
				}
				std::int32_t writed = header_size - offset_;
				if (writed > buf_size)
					writed = buf_size;
				std::memcpy(buf, header_ + offset_, writed);
				offset_ += writed;
				buf += writed;
				buf_size -= writed;
				if (offset_ < header_size)
					return Result::InProcess;
				state_ = State::CopyingData;
				offset_ = 0;
			}
			[[fallthrough]];
		case State::CopyingData:
			{
				std::int32_t size = static_cast<std::int32_t>(object_.size());
				if (offset_ >= size)
				{
					assert(false);
					return Result::Error;
				}
				std::int32_t writed = size - offset_;
				if (writed > buf_size)
					writed = buf_size;
				std::memcpy(buf, object_.data() + offset_, writed);
				offset_ += writed;
				buf += writed;
				buf_size -= writed;
				if (offset_ < size)
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
	alignas(Header) mutable char header_[sizeof(Header)];
	std::string object_;
};

inline std::unique_ptr<DsonObj> to_dson_obj(std::int32_t key, std::string val)
{
	return std::make_unique<DsonStringObj>(key, std::move(val));
}

template <typename K>
inline std::unique_ptr<DsonObj> to_dson_obj(K key, std::string val)
{
	return to_dson_obj(static_cast<std::int32_t>(key), std::move(val));
}

} // namespace hi
#endif // DSON_STRING_OBJ_H

#ifndef DSON_H
#define DSON_H

#include <dson/impl/dson_obj.h>

#include <cstdlib> // malloc
#include <cstring> // memcpy
#include <functional>
#include <map>
#include <memory>

#ifndef MAX_DSON_RAM_SIZE
#	define MAX_DSON_RAM_SIZE (1024 * 1024 * 1024)
#endif

namespace hi
{

class Dson : public DsonObj
{
public:
	Dson()
	{
		clear_header();
	}

	~Dson()
	{
		clear();
	}

	/**
	 * @brief Dson
	 * Работа ака std::string_view - окно на буфер.
	 * Буфер представляется в виде Dson.
	 * Если попытаться смувать, то буфер скопируется в новую аллокацию.
	 * (например для передачи Dson в библиотеки лучше использовать to_buf_host_order())
	 * @param buf
	 */
	explicit Dson(char * buf)
		: buf_{buf}
		, dson_kind_{DsonKind::DataBufNeedParse}
	{
		pre_parse_buf();
	}

	Dson(const Dson & other) = delete;
	Dson(Dson && other) noexcept
	{
		move_from_other(std::move(other));
	}

	/**
	 * @brief Dson
	 * Когда необходимо DsonKind::OneObjectInDataBuf
	 * перекинуть в key_to_val_map_
	 * @param header
	 * @param buf
	 */
	Dson(char * header, char * buf)
		: buf_{buf}
		, dson_kind_{DsonKind::OneObjectInDataBuf}
	{
		std::memcpy(header_, header, header_size);
	}

	Dson & operator=(const Dson & other) = delete;
	Dson & operator=(Dson && other) noexcept
	{
		if (&other == this)
			return *this;
		move_from_other(std::move(other));
		return *this;
	}

	template <typename T>
	Dson(T data);

	void * init(std::int32_t data_type, std::int32_t data_size)
	{
		clear();
		char * ptr = allocate(data_size);
		if (!ptr)
			return {};
		Header * _header = header();
		_header->mark_byte_order_ = mark_host_order;
		_header->data_type_ = data_type;
		_header->data_size_ = data_size;
		dson_kind_ = DsonKind::OneObjectInDataBuf;
		return ptr;
	}

	/**
	 * @brief init
	 * Аллокация буфера под хранение объекта.
	 * (как получить память из malloc и работать с ней)
	 * @param key - назначенный ключ для поиска этих данных в Dson
	 * @param data_type_id - идентификатор типа данных, назначается
	 * в <dson/impl/types_map.h> как
		template<>
		struct types_map<std::uint32_t> : register_id<std::uint32_t, Идентификатор data_type_id> { };
	 * или в любом ином месте где разработчик сможет контроллировать уникальность data_type_id
	 * @param data_size - аллоцируемый под данные размер
	 * @return буфер для заполнения
	 *
	 * @note std::int32_t size знаковый чтобы при работе с POSIX read|write
	 * не преобразовывать беззнаковое в знаковое
	 */
	void * init(std::int32_t key, std::int32_t data_type, std::int32_t data_size)
	{
		clear();
		char * ptr = allocate(data_size);
		if (!ptr)
			return {};
		Header * _header = header();
		_header->mark_byte_order_ = mark_host_order;
		_header->key_ = key;
		_header->data_type_ = data_type;
		_header->data_size_ = data_size;
		dson_kind_ = DsonKind::OneObjectInDataBuf;
		return ptr;
	}

	/**
	 * @brief map
	 * Доступ к содержимому Dson для поиска и итерации
	 * @return map ключ->объект
	 */
	const std::map<std::int32_t, std::unique_ptr<DsonObj>> & map()
	{
		if (state_ != State::Ready)
			return key_to_val_map_;
		switch (dson_kind_)
		{
		case DsonKind::DataBufNeedParse:
			parse_buf();
			break;
		case DsonKind::OneObjectInDataBuf:
			one_object_buf_to_container();
			break;
		default:
			break;
		}
		// Внешний доступ всегда предполагает host order для всех элементов
		auto & _converters = converters();
		for (auto & it : key_to_val_map_)
		{
			if (auto dson = dynamic_cast<Dson *>(it.second.get()))
			{
				_converters.to_host(*dson);
			}
		}

		return key_to_val_map_;
	}

	void clear()
	{
		if (was_buf_allocation_)
		{
			std::free(buf_);
			buf_ = nullptr;
			was_buf_allocation_ = false;
		}
		buf_size_ = 0;
		dson_kind_ = DsonKind::DsonContainer;

		copy_iter_ = {};
		key_to_val_map_.clear();

		state_ = State::Ready;
	}

	template <typename K>
	K typed_key() const noexcept
	{
		return static_cast<K>(key());
	}

	/**
	 * @brief insert
	 * Добавление объекта.
	 * Если объект с таким ключём уже есть, то он будет заменён на новый
	 * @param obj
	 */
	void insert(std::unique_ptr<DsonObj> obj)
	{
		if (!obj)
			return;
		const auto key = obj->key();
		insert_internal(key, std::move(obj));
	}

	template <typename K>
	void insert(K key, Dson obj)
	{
		const std::int32_t k = static_cast<std::int32_t>(key);
		obj.set_key(k);
		auto ptr = std::make_unique<Dson>(std::move(obj));
		insert_internal(k, std::move(ptr));
	}

	/**
	 * @brief get
	 * Получение значения по ключу:
	 * если значение по ключу будет найдено, то вернёт указатель на значение
	 * @param _key - искомый ключ
	 * @return - nullptr если не найдено, иначе указатель на значение
	 * @note предполагается что должен быть реализован метод from_dson_obj
	 * преобразующий значение DsonObj в пользовательский тип
	 */
	template <typename K>
	DsonObj * get(K _key)
	{
		if (DsonKind::DataBufNeedParse == dson_kind_)
		{
			parse_buf();
		}
		if (state_ != State::Ready)
			return {};
		const std::int32_t k = static_cast<std::int32_t>(_key);
		switch (dson_kind_)
		{
		case DsonKind::OneObjectInDataBuf:
			if (key() == k)
			{
				converters().to_host(*this);
				return this;
			}
			return nullptr;

		case DsonKind::DsonContainer:
			return get_internal(k);
		default:
			break;
		}
		return nullptr;
	}

	template <typename K>
	DsonObj * operator[](K key)
	{
		return get(key);
	}

	/**
	 * @brief load_from_stream
	 * Загрузка из ifstream
	 * @param input - ifstream
	 * @note загрузка происходит разом всего объёма
	 */
	void load_from_stream(std::ifstream & input)
	{
		clear();
		if (!input.read(header_, header_size))
		{
			state_ = State::Error;
			return;
		}
		dson_kind_ = DsonKind::DataBufNeedParse;
		std::int32_t size = data_size();
		if (size < 0 || size > MAX_DSON_RAM_SIZE)
		{
			state_ = State::Error;
			return;
		}
		if (size == 0)
		{
			state_ = State::Ready;
			dson_kind_ = DsonKind::DsonContainer;
			return;
		}
		char * buf = allocate(size);
		if (!buf)
		{
			state_ = State::Error;
			return;
		}
		if (!input.read(buf, size))
		{
			state_ = State::Error;
			return;
		}
		state_ = State::Ready;
	}

	/**
	 * @brief load_from_fd
	 * POSIX чтение из fd (сеть/файл/pipe/..)
	 * @param fd дескриптор
	 * @return Result
	 * @note загрузка происходит по мере возможности fd
	 * @note итераторы хранятся внутри - чтобы сбросить: reset()
	 */
	Result load_from_fd(const std::int32_t fd)
	{
		switch (state_)
		{
		case State::Error:
			[[fallthrough]];
		case State::CopyingHeader:
			[[fallthrough]];
		case State::CopyingData:
			[[fallthrough]];
		case State::Ready:
			{
				clear();
				offset_ = 0;
				state_ = State::LoadingHeader;
				dson_kind_ = DsonKind::DataBufNeedParse;
			}
			[[fallthrough]];
		case State::LoadingHeader:
			{
				if (offset_ >= header_size)
				{
					assert(false);
					state_ = State::Error;
					return Result::Error;
				}
				const auto readed = read_from_fd(fd, header_ + offset_, header_size - offset_);
				if (readed < 0)
				{
					state_ = State::Error;
					return Result::Error;
				}
				offset_ += readed;
				if (offset_ < header_size)
				{
					return Result::InProcess;
				}
				const std::int32_t size = data_size();
				if (size < 0 || size > MAX_DSON_RAM_SIZE)
				{
					state_ = State::Error;
					return Result::Error;
				}
				if (size == 0)
				{
					state_ = State::Ready;
					dson_kind_ = DsonKind::DsonContainer;
					return Result::Ready;
				}
				char * buf = allocate(size);
				if (!buf)
				{
					state_ = State::Error;
					return Result::Error;
				}
				offset_ = 0;
				state_ = State::LoadingData;
			}
			[[fallthrough]];
		case State::LoadingData:
			{
				if (offset_ >= buf_size_)
				{
					assert(false);
					return Result::Error;
				}
				const auto readed = read_from_fd(fd, buf_ + offset_, buf_size_ - offset_);
				if (readed < 0)
				{
					state_ = State::Error;
					return Result::Error;
				}
				offset_ += readed;
				if (offset_ == buf_size_)
				{
					state_ = State::Ready;
					return Result::Ready;
				}
				return Result::InProcess;
			}
		} // switch

		return Result::Error;
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
		switch (dson_kind_)
		{
		case DsonKind::DataBufNeedParse:
			[[fallthrough]];
		case DsonKind::OneObjectInDataBuf:
			{
				Header * _header = header();
				if (_header->mark_byte_order_ == mark_host_order)
					return _header->data_size_;
				return int32_to_host(header_as_array()[1]);
			}
		case DsonKind::DsonContainer:
			{
				std::int32_t re{0};
				for (auto & it : key_to_val_map_)
				{
					re += it.second->data_size() + header_size;
				}
				return re;
			}
		}
		return {};
	}

	std::int32_t key() const noexcept override
	{
		Header * _header = header();
		if (_header->mark_byte_order_ == mark_host_order)
			return _header->key_;
		return int32_to_host(header_as_array()[2]);
	}

	void set_key(const std::int32_t _key) noexcept override
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

	template <typename K>
	void set_key(K key)
	{
		set_key(static_cast<std::int32_t>(key));
	}

	std::int32_t data_type() const noexcept override
	{
		if (state_ == State::Error)
			return types_map<Empty>::value;
		Header * _header = header();
		if (_header->mark_byte_order_ == mark_host_order)
			return _header->data_type_;
		return int32_to_host(header_as_array()[3]);
	}

	void copy_to_stream_host_order(std::ofstream & out) override
	{
		if (state_ != State::Ready)
			return;
		if (dson_kind_ == DsonKind::DataBufNeedParse)
		{ // Данные только приехали и не менялись => можно сразу отправить
			if (is_host_order())
			{
				const auto _data_size = data_size();
				copy_to_stream_header_internal(out, _data_size);
				copy_to_stream_data_internal(out, _data_size);
				return;
			}
			parse_buf();
		}
		switch (dson_kind_)
		{
		case DsonKind::OneObjectInDataBuf:
			{
				converters().to_host(*this);
				const auto _data_size = data_size();
				copy_to_stream_header_internal(out, _data_size);
				copy_to_stream_data_internal(out, _data_size);
				break;
			}
		case DsonKind::DsonContainer:
			{
				header_to_host();
				const auto _data_size = data_size();
				copy_to_stream_header_internal(out, _data_size);

				for (auto & it : key_to_val_map_)
				{
					it.second->copy_to_stream_host_order(out);
				}
				break;
			}
		default:
			break;
		}
	}

	void copy_to_stream_network_order(std::ofstream & out) override
	{
		if (state_ != State::Ready)
			return;
		if (dson_kind_ == DsonKind::DataBufNeedParse)
		{ // Данные только приехали и не менялись => можно сразу отправить
			if (is_network_order())
			{
				const auto _data_size = data_size();
				copy_to_stream_header_internal(out, _data_size);
				copy_to_stream_data_internal(out, _data_size);
				return;
			}
			parse_buf();
		}
		switch (dson_kind_)
		{
		case DsonKind::OneObjectInDataBuf:
			{
				const auto _data_size = data_size();
				converters().to_network(*this);
				copy_to_stream_header_internal(out, _data_size);
				copy_to_stream_data_internal(out, _data_size);
				break;
			}
		case DsonKind::DsonContainer:
			{
				const auto _data_size = data_size();
				header_to_network();
				copy_to_stream_header_internal(out, _data_size);

				for (auto & it : key_to_val_map_)
				{
					it.second->copy_to_stream_network_order(out);
				}
				break;
			}
		default:
			break;
		}
	}

	/**
	 * @brief copy_to_fd
	 * POSIX запись в fd (сеть/файл/pipe/..)
	 * @param fd дескриптор
	 * @return Result
	 * @note итераторы хранятся внутри - чтобы сбросить: reset_state()
	 * @note copy_to_fd_*  и load_from_fd несоместимы - чтобы начать надо сбросить итераторы reset_state()
	 */
	Result copy_to_fd_host_order(std::int32_t fd) override
	{
		return copy_to_fd_internal<false>(fd);
	}

	/**
	 * @brief copy_to_fd
	 * POSIX запись в fd (сеть/файл/pipe/..)
	 * @param fd дескриптор
	 * @return Result
	 * @note итераторы хранятся внутри - чтобы сбросить: reset_state()
	 * @note copy_to_fd_*  и load_from_fd несоместимы - чтобы начать надо сбросить итераторы reset_state()
	 */
	Result copy_to_fd_network_order(std::int32_t fd) override
	{
		return copy_to_fd_internal<true>(fd);
	}

	Result copy_to_buf_host_order(char *& buf, std::int32_t & buf_size) override
	{
		return copy_to_buf_internal<false>(buf, buf_size);
	}

	Result copy_to_buf_network_order(char *& buf, std::int32_t & buf_size) override
	{
		return copy_to_buf_internal<true>(buf, buf_size);
	}

	State state() const noexcept override
	{
		return state_;
	}

	void reset_state() noexcept override
	{
		if (state_ == State::Error)
		{
			clear();
			return;
		}
		state_ = State::Ready;
		// Итераторы сбросятся при переключении состояний
	}

public: // Converters
	/**
	 * Данные в буффере необходимо уметь преобразовывать в
	 * host | network byte order.
	 * В данном разделе конструируется таблица преобразователей byte order.
	 */
	using Converter = std::function<void(Header &, char *)>;
	struct Converters
	{
		using ConvertersMap = std::map<std::uint32_t, std::function<void(Header &, char *)>>;
		/**
		 * @brief dson_lib_defined_converters
		 * Заполнение таблицы преобразователей стандартными методами библиотеки
		 * @param to_host_order
		 * @param to_network_order
		 */
		static void dson_lib_defined_converters(ConvertersMap & to_host_order, ConvertersMap & to_network_order);
#ifdef USE_USER_DSON_TYPES
		/**
		 * @brief dson_lib_defined_converters
		 * Заполнение таблицы преобразователей методами пользователя.
		 * Запускается после dson_lib_defined_converters => можно переопределять библиотечные методы.
		 * @param to_host_order
		 * @param to_network_order
		 */
		static void dson_user_defined_converters(ConvertersMap & to_host_order, ConvertersMap & to_network_order);
#endif

		Converters()
		{
			dson_lib_defined_converters(to_host_order_, to_network_order_);
#ifdef USE_USER_DSON_TYPES
			dson_user_defined_converters(to_host_order_, to_network_order_);
#endif
		}

		void to_host(Dson & obj) noexcept
		{
			if (obj.is_host_order())
				return;
			auto converter = to_host_order_.find(obj.data_type());
			if (converter == to_host_order_.end())
			{
				// Для некоторых типов, например std::string, преобразования не требуются
				return;
			}
			obj.to_host_internal(converter->second);
		}

		void to_network(Dson & obj) noexcept
		{
			if (obj.is_network_order())
				return;
			auto converter = to_network_order_.find(obj.data_type());
			if (converter == to_network_order_.end())
			{
				// Для некоторых типов, например std::string, преобразования не требуются
				return;
			}
			obj.to_network_internal(converter->second);
		}

		ConvertersMap to_host_order_;
		ConvertersMap to_network_order_;
	};

	static inline Converters & converters()
	{
		static Converters converters;
		return converters;
	}

	void * data()
	{
		if (state_ == State::Error || !buf_)
			return nullptr;
		if (was_buf_allocation_)
			return buf_;
		return (buf_ + header_size);
	}

private:
	Header * header() const noexcept
	{
		if (was_buf_allocation_ || !buf_)
		{
			/*
			 *  Если блок данных был аллоцирован, значит это не вьюха на внешний буффер
			 *  и буффер под заголовки локальный.
			 */
			return std::launder(reinterpret_cast<Header *>(header_));
		}
		return std::launder(reinterpret_cast<Header *>(buf_));
	}

	std::uint32_t * header_as_array() const noexcept
	{
		if (was_buf_allocation_ || !buf_)
		{
			return std::launder(reinterpret_cast<std::uint32_t *>(header_));
		}
		return std::launder(reinterpret_cast<std::uint32_t *>(buf_));
	}

	char * header_as_char_buf() const noexcept
	{
		if (was_buf_allocation_ || !buf_)
		{
			return std::launder(reinterpret_cast<char *>(header_));
		}
		return buf_;
	}

	void pre_parse_buf() noexcept
	{
		const std::int32_t _data_size = data_size();
		if (_data_size < 0 || _data_size > MAX_DSON_RAM_SIZE)
		{
			state_ = State::Error;
			return;
		}
		if (data_type() == types_map<DsonContainer>::value)
		{
			dson_kind_ = DsonKind::DataBufNeedParse;
			return;
		}
		dson_kind_ = DsonKind::OneObjectInDataBuf;
	}

	void parse_buf()
	{
		assert(dson_kind_ == DsonKind::DataBufNeedParse);
		const auto type = data_type();
		if (type != types_map<DsonContainer>::value)
		{
			dson_kind_ = DsonKind::OneObjectInDataBuf;
			return;
		}

		std::int32_t not_used = buf_size_;
		char * ptr = static_cast<char *>(data());

		while (not_used >= header_size)
		{
			auto obj = std::make_unique<Dson>(ptr);
			const std::int32_t key = obj->key();
			if (key < 0)
				break;
			std::int32_t buf_size = obj->data_size();
			if (buf_size < 0)
				break;
			buf_size += header_size;
			key_to_val_map_.insert_or_assign(key, std::move(obj));
			not_used -= buf_size;
			ptr += buf_size;
		}
		assert(not_used == 0);
		if (not_used != 0)
		{
			state_ = State::Error;
			return;
		}
		dson_kind_ = DsonKind::DsonContainer;
	}

	/**
	 * @brief one_object_buf_to_container
	 * Пока был только 1 загруженный извне объект - key_to_val_map_ не использовался
	 * Когда добавляются новые объекты поиск уже идёт по key_to_val_map_
	 * и теперь необходимо первый объект также отразить в key_to_val_map_
	 */
	void one_object_buf_to_container()
	{
		assert(dson_kind_ == DsonKind::OneObjectInDataBuf);
		assert(data_type() != types_map<DsonContainer>::value);
		char * ptr = static_cast<char *>(data());
		if (!ptr)
		{
			state_ = State::Error;
			return;
		}
		auto obj = std::make_unique<Dson>(header_as_char_buf(), ptr);
		const std::int32_t key = obj->key();
		if (key < 0)
		{
			state_ = State::Error;
			return;
		}
		key_to_val_map_.insert_or_assign(key, std::move(obj));
		dson_kind_ = DsonKind::DsonContainer;
		set_data_type_internal(types_map<DsonContainer>::value);
	}

	void insert_internal(const std::int32_t key, std::unique_ptr<DsonObj> obj)
	{
		assert(obj);
		if (!obj)
			return;
		if (state_ != State::Ready)
			reset_state();

		switch (dson_kind_)
		{
		case DsonKind::DataBufNeedParse:
			parse_buf();
			break;
		case DsonKind::OneObjectInDataBuf:
			// Добавление последующих объектов превращает Dson в контейнер
			one_object_buf_to_container();
			break;
		default:
			break;
		}
		if (state_ != State::Ready)
			return;
		key_to_val_map_.insert_or_assign(key, std::move(obj));
		set_data_type_internal(types_map<DsonContainer>::value);
	}

	DsonObj * get_internal(const std::int32_t _key)
	{
		assert(dson_kind_ == DsonKind::DsonContainer);
		if (state_ != State::Ready)
			reset_state();
		auto find_it = key_to_val_map_.find(_key);
		if (find_it == key_to_val_map_.end())
			return {};
		DsonObj * obj = find_it->second.get();
		if (!obj)
			return {};
		// Если объект - Dson, то можно сразу попробовать конвертировать в host byte order
		if (auto dson = dynamic_cast<Dson *>(obj))
		{
			converters().to_host(*dson);
		}
		return obj;
	}

	void to_host_internal(const Converter & converter)
	{
		header_to_host();
		auto dson_header = header();
		auto dson_data = static_cast<char *>(data());
		if (!dson_data)
			return;
		converter(*dson_header, dson_data);
	}

	void to_network_internal(const Converter & converter)
	{
		auto dson_header = header();
		auto dson_data = static_cast<char *>(data());
		if (!dson_data)
			return;
		converter(*dson_header, dson_data);
		header_to_network();
	}

	void header_to_host() const noexcept
	{
		std::uint32_t * header = header_as_array();
		for (std::uint32_t i = 0; i < header_array_len; ++i)
		{
			header[i] = ntohl(header[i]);
		}
	}

	void header_to_network() const noexcept
	{
		std::uint32_t * header = header_as_array();
		if (!header)
			return;
		for (std::uint32_t i = 0; i < header_array_len; ++i)
		{
			header[i] = htonl(header[i]);
		}
	}

	void move_from_other(Dson && other)
	{
		if (!was_buf_allocation_ && buf_)
		{
			/*
			 * Если не было аллокаций, то это вьюха на чужой буфер => гарантировать его существование невозможно.
			 * Поэтому просто всё копируем.
			 */
			buf_size_ = data_size();
			buf_ = static_cast<char *>(std::malloc(static_cast<size_t>(buf_size_)));
			if (!buf_)
				return;
			was_buf_allocation_ = true;
			// Копируем header
			char * buf = header_;
			std::int32_t buf_size = header_size;
			auto copy_result = other.copy_to_buf_host_order(buf, buf_size);
			if (copy_result == Result::Error || buf_size)
			{
				state_ = State::Error;
				return;
			}
			// Копируем данные
			buf = buf_;
			buf_size = buf_size_;
			copy_result = other.copy_to_buf_host_order(buf, buf_size);
			if (copy_result != Result::Ready || buf_size)
			{
				state_ = State::Error;
			}
			return;
		}

		// Обычный move всего
		std::memcpy(header_, other.header_, static_cast<size_t>(header_size));
		state_ = other.state_;
		other.state_ = State::Error;
		buf_ = other.buf_;
		other.buf_ = nullptr;
		buf_size_ = other.buf_size_;
		was_buf_allocation_ = other.was_buf_allocation_;
		dson_kind_ = other.dson_kind_;
		std::swap(key_to_val_map_, other.key_to_val_map_);
		offset_ = other.offset_;
		std::swap(copy_iter_, other.copy_iter_);
	}

	void set_data_type_internal(const std::int32_t data_type)
	{
		Header * _header = header();
		if (_header->mark_byte_order_ == mark_host_order)
		{
			_header->data_type_ = data_type;
		}
		else
		{
			header_as_array()[3] = int32_to_network(data_type);
		}
	}

	void set_data_size_internal(const std::int32_t data_size)
	{
		Header * _header = header();
		if (_header->mark_byte_order_ == mark_host_order)
		{
			_header->data_size_ = data_size;
		}
		else
		{
			header_as_array()[1] = int32_to_network(data_size);
		}
	}

	/**
	 * @brief copy_to_fd_internal
	 * @param fd
	 * @return
	 * @note network_order нужен ли network byte order
	 */
	template <bool network_order>
	Result copy_to_fd_internal(std::int32_t fd)
	{
		switch (state_)
		{
		case State::Ready:
			{
				if (dson_kind_ == DsonKind::DataBufNeedParse)
				{
					if ((is_host_order() && network_order) || (is_network_order() && !network_order))
					{
						/*
						 * Если существующий byte order требует преобразований, то придётся
						 * разобрать на отдельные объекты и преобразовать каждый
						 */
						parse_buf();
					}
				}

				if (dson_kind_ == DsonKind::DsonContainer)
				{
					set_data_size_internal(data_size());
				}

				if constexpr (network_order)
				{
					if (dson_kind_ == DsonKind::OneObjectInDataBuf)
					{
						converters().to_network(*this);
					}
					else
					{
						header_to_network();
					}
				}
				else
				{
					if (dson_kind_ == DsonKind::OneObjectInDataBuf)
					{
						converters().to_host(*this);
					}
					else
					{
						header_to_host();
					}
				}
				offset_ = 0;
				state_ = State::CopyingHeader;
			}
			[[fallthrough]];
		case State::CopyingHeader:
			{
				if (offset_ >= header_size)
				{
					assert(false);
					return Result::Error;
				}
				char * _header = header_as_char_buf();
				const auto writed = write_to_fd(fd, _header + offset_, header_size - offset_);
				if (writed < 0)
				{
					state_ = State::Ready;
					return Result::Error;
				}

				offset_ += writed;
				if (offset_ < header_size)
				{
					return Result::InProcess;
				}

				if (dson_kind_ == DsonKind::DsonContainer)
				{
					copy_iter_ = key_to_val_map_.begin();
				}
				else
				{
					offset_ = 0;
				}
				state_ = State::CopyingData;
			}
			[[fallthrough]];
		case State::CopyingData:
			{
				if (dson_kind_ == DsonKind::DsonContainer)
				{
					if constexpr (network_order)
					{
						return copy_to_fd_internal_map_network(fd);
					}
					else
					{
						return copy_to_fd_internal_map_host(fd);
					}
				}
				else
				{
					return copy_to_fd_internal_buf(fd);
				}
			}

		default:
			break;
		}
		assert(false);
		return Result::Error;
	} // copy_to_fd_internal

	std::int32_t buf_size_without_header()
	{
		if (was_buf_allocation_)
			return buf_size_;
		return (buf_size_ - header_size);
	}

	Result copy_to_fd_internal_buf(std::int32_t fd)
	{
		char * buf = static_cast<char *>(data());
		const std::int32_t size = buf_size_without_header();
		if (offset_ >= size)
		{
			assert(false);
			return Result::Error;
		}
		const auto writed = write_to_fd(fd, buf + offset_, size - offset_);
		if (writed < 0)
		{
			state_ = State::Ready;
			return Result::Error;
		}
		offset_ += writed;
		if (size == offset_)
		{
			state_ = State::Ready;
			return Result::Ready;
		}
		return Result::InProcess;
	}

	Result copy_to_fd_internal_map_network(std::int32_t fd)
	{
		while (copy_iter_ != key_to_val_map_.end())
		{
			const auto res = copy_iter_->second->copy_to_fd_network_order(fd);
			if (res != Result::Ready)
				return res;
			++copy_iter_;
		}
		state_ = State::Ready;
		return Result::Ready;
	}

	Result copy_to_fd_internal_map_host(std::int32_t fd)
	{
		while (copy_iter_ != key_to_val_map_.end())
		{
			const auto res = copy_iter_->second->copy_to_fd_host_order(fd);
			if (res != Result::Ready)
				return res;
			++copy_iter_;
		}
		state_ = State::Ready;
		return Result::Ready;
	}

	/**
	 * @brief copy_to_fd_internal
	 * @param buf курсор по буферу
	 * @param buf_size счётчик оставшегося объёма
	 * @return Result
	 * @note network_order нужен ли network byte order
	 */
	template <bool network_order>
	Result copy_to_buf_internal(char *& buf, std::int32_t & buf_size)
	{
		switch (state_)
		{
		case State::Ready:
			{
				if (dson_kind_ == DsonKind::DataBufNeedParse)
				{
					if ((is_host_order() && network_order) || (is_network_order() && !network_order))
					{
						/*
						 * Если существующий byte order требует преобразований, то придётся
						 * разобрать на отдельные объекты и преобразовать каждый
						 */
						parse_buf();
					}
				}

				if (dson_kind_ == DsonKind::DsonContainer)
				{
					set_data_size_internal(data_size());
				}

				if constexpr (network_order)
				{
					if (dson_kind_ == DsonKind::OneObjectInDataBuf)
					{
						converters().to_network(*this);
					}
					else
					{
						header_to_network();
					}
				}
				else
				{
					if (dson_kind_ == DsonKind::OneObjectInDataBuf)
					{
						converters().to_host(*this);
					}
					else
					{
						header_to_host();
					}
				}
				offset_ = 0;
				state_ = State::CopyingHeader;
			}
			[[fallthrough]];
		case State::CopyingHeader:
			{
				if (offset_ >= header_size)
				{
					assert(false);
					return Result::Error;
				}

				char * _header = header_as_char_buf();
				const auto writed = std::min(buf_size, header_size - offset_);
				std::memcpy(buf, _header + offset_, writed);
				buf += writed;
				offset_ += writed;
				buf_size -= writed;
				if (offset_ < header_size)
				{
					return Result::InProcess;
				}

				if (dson_kind_ == DsonKind::DsonContainer)
				{
					copy_iter_ = key_to_val_map_.begin();
				}
				else
				{
					offset_ = 0;
				}
				state_ = State::CopyingData;
			}
			[[fallthrough]];
		case State::CopyingData:
			{
				if (dson_kind_ == DsonKind::DsonContainer)
				{
					if constexpr (network_order)
					{
						return copy_to_buf_internal_map_network(buf, buf_size);
					}
					else
					{
						return copy_to_buf_internal_map_host(buf, buf_size);
					}
				}
				else
				{
					return copy_to_buf_internal_buf(buf, buf_size);
				}
			}

		default:
			break;
		}
		assert(false);
		return Result::Error;
	} // copy_to_buf_internal

	Result copy_to_buf_internal_buf(char *& buf, std::int32_t & buf_size)
	{
		char * local_buf = static_cast<char *>(data());
		const std::int32_t size = buf_size_without_header();
		if (offset_ >= size)
		{
			assert(false);
			return Result::Error;
		}

		const auto writed = std::min(buf_size, size - offset_);
		std::memcpy(buf, local_buf + offset_, writed);
		buf += writed;
		offset_ += writed;
		buf_size -= writed;
		if (offset_ < size)
		{
			return Result::InProcess;
		}

		state_ = State::Ready;
		return Result::Ready;
	}

	Result copy_to_buf_internal_map_network(char *& buf, std::int32_t & buf_size)
	{
		while (copy_iter_ != key_to_val_map_.end())
		{
			const auto res = copy_iter_->second->copy_to_buf_network_order(buf, buf_size);
			if (res != Result::Ready)
				return res;
			++copy_iter_;
		}
		state_ = State::Ready;
		return Result::Ready;
	}

	Result copy_to_buf_internal_map_host(char *& buf, std::int32_t & buf_size)
	{
		while (copy_iter_ != key_to_val_map_.end())
		{
			const auto res = copy_iter_->second->copy_to_buf_host_order(buf, buf_size);
			if (res != Result::Ready)
				return res;
			++copy_iter_;
		}
		state_ = State::Ready;
		return Result::Ready;
	}

	char * allocate(std::int32_t buf_size)
	{
		if (buf_size <= 0)
			return nullptr;
		if (buf_ && was_buf_allocation_)
			std::free(buf_);
		buf_ = static_cast<char *>(std::malloc(static_cast<size_t>(buf_size)));
		if (buf_)
		{
			was_buf_allocation_ = true;
			buf_size_ = buf_size;
		}
		return buf_;
	}

	void clear_header()
	{
		Header * _header = header();
		_header->mark_byte_order_ = mark_host_order;
		_header->data_size_ = 0;
		_header->key_ = 0;
		_header->data_type_ = types_map<Empty>::value;
	}

	void copy_to_stream_header_internal(std::ofstream & out, const std::int32_t data_size)
	{
		if (dson_kind_ == DsonKind::DsonContainer)
		{
			set_data_size_internal(data_size);
		}
		char * buf = header_as_char_buf();
		std::copy(buf, buf + header_size, std::ostream_iterator<char>(out));
	}

	void copy_to_stream_data_internal(std::ofstream & out, const std::int32_t data_size)
	{
		auto buf = static_cast<char *>(data());
		std::copy(buf, buf + data_size, std::ostream_iterator<char>(out));
	}

private:
	alignas(Header) mutable char header_[sizeof(Header)];

	/*
	 * Указатель на буффер загруженный с сети (без заголовка)
	 * или на внешний буффер (с заголовком)
	 */
	char * buf_{nullptr};

	// Размер buf_
	std::int32_t buf_size_{0};

	/*
	 * Аллоцировал ли data_buf_ сам,
	 * или это view на внешний буффер.
	 * В случае внешнего буффера Header находится в data_buf_
	 */
	bool was_buf_allocation_{false};

	// Что лежит внутри
	enum class DsonKind
	{
		// Объекты лежат в key_to_val_map_
		DsonContainer,
		// Dson натянут поверх буффера, что внутри неизвестно
		DataBufNeedParse,
		/*
		 * Dson натянут поверх буффера, внутри 1 объект.
		 * При добавлении новых объектов необходимо будет
		 * преобразовать в DsonContainer
		 */
		OneObjectInDataBuf
	};
	DsonKind dson_kind_{DsonKind::DsonContainer};

	/*
	 * Содержимое Dson.
	 * Позволяет искать по ключу целевой объект.
	 */
	std::map<std::int32_t, std::unique_ptr<DsonObj>> key_to_val_map_;

	// Итератор выгрузки key_to_val_map_
	decltype(key_to_val_map_.begin()) copy_iter_{};
};

inline std::ifstream & operator>>(std::ifstream & in, Dson & dson)
{
	dson.load_from_stream(in);
	return in;
}

inline void Dson::Converters::dson_lib_defined_converters(
	ConvertersMap & to_host_order,
	ConvertersMap & to_network_order)
{
	{ // std::int32_t, std::uint32_t,
		auto to_host = [](Dson::Header &, char * data)
		{
			std::uint32_t * var = std::launder(reinterpret_cast<std::uint32_t *>(data));
			*var = ntohl(*var);
		};
		auto to_network = [](Dson::Header &, char * data)
		{
			std::uint32_t * var = std::launder(reinterpret_cast<std::uint32_t *>(data));
			*var = htonl(*var);
		};
		{
			const auto key = types_map<std::int32_t>::value;
			to_host_order.insert_or_assign(key, to_host);
			to_network_order.insert_or_assign(key, to_network);
		}
		{
			const auto key = types_map<std::uint32_t>::value;
			to_host_order.insert_or_assign(key, to_host);
			to_network_order.insert_or_assign(key, to_network);
		}
	} // std::uint32_t, std::int32_t

	{ // std::int64_t, std::uint64_t,
		auto to_host = [](Dson::Header &, char * data)
		{
			std::uint64_t * var = std::launder(reinterpret_cast<std::uint64_t *>(data));
			*var = ntohll(*var);
		};
		auto to_network = [](Dson::Header &, char * data)
		{
			std::uint64_t * var = std::launder(reinterpret_cast<std::uint64_t *>(data));
			*var = htonll(*var);
		};
		{
			const auto key = types_map<std::int64_t>::value;
			to_host_order.insert_or_assign(key, to_host);
			to_network_order.insert_or_assign(key, to_network);
		}
		{
			const auto key = types_map<std::uint64_t>::value;
			to_host_order.insert_or_assign(key, to_host);
			to_network_order.insert_or_assign(key, to_network);
		}
	} // std::uint64_t, std::int64_t

	{ // std::vector<std::uint32_t>
		const auto key = types_map<std::vector<std::uint32_t>>::value;
		to_host_order.insert_or_assign(
			key,
			[](Dson::Header & header, char * data)
			{
				std::uint32_t size = header.data_size_ / sizeof(std::uint32_t);
				std::uint32_t * array = std::launder(reinterpret_cast<std::uint32_t *>(data));
				for (std::uint32_t i = 0; i < size; ++i)
				{
					array[i] = ntohl(array[i]);
				}
			});
		to_network_order.insert_or_assign(
			key,
			[](Dson::Header & header, char * data)
			{
				std::uint32_t size = header.data_size_ / sizeof(std::uint32_t);
				std::uint32_t * array = std::launder(reinterpret_cast<std::uint32_t *>(data));
				for (std::uint32_t i = 0; i < size; ++i)
				{
					array[i] = htonl(array[i]);
				}
			});
	} // std::vector<std::uint32_t>

	// для std::string преобразования не требуются
}

template <>
inline Dson::Dson(std::uint32_t data)
{
	std::uint32_t * buf = static_cast<std::uint32_t *>(init(types_map<std::uint32_t>::value, sizeof(std::uint32_t)));
	*buf = data;
}

template <>
inline Dson::Dson(std::int32_t data)
{
	std::int32_t * buf = static_cast<std::int32_t *>(init(types_map<std::int32_t>::value, sizeof(std::int32_t)));
	*buf = data;
}

template <>
inline Dson::Dson(std::string_view data)
{
	char * buf = static_cast<char *>(init(types_map<std::string>::value, data.size()));
	std::memcpy(buf, data.data(), data.size());
}

template <>
inline Dson::Dson(std::string data)
{
	char * buf = static_cast<char *>(init(types_map<std::string>::value, data.size()));
	std::memcpy(buf, data.data(), data.size());
}

} // namespace hi

#endif // DSON_H

#ifndef DSON_OBJ_H
#define DSON_OBJ_H

#include <dson/impl/dson_tools.h>
#include <dson/impl/result.h>
#include <dson/impl/types_map.h>

#include <cassert>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

namespace hi
{

inline constexpr std::uint32_t mark_host_order{1};
inline const std::uint32_t mark_network_order{htonl(mark_host_order)};

/**
  Интерфейс доступа к данным.
  Потоко небезопасно.

  Используется для:
  1) Хранения объекта C++ или буфера в который был сериализован объект C++
  2) Подготовки объекта С++ к выгрузке в Dson буфер в host или network byte order
  (для IPC host order, для хранения в файле или передачи по сети в network order)
  3) Извлечения объекта С++ в host order
*/
struct DsonObj
{
public:
	virtual ~DsonObj() = default;

	/**
	 * @brief is_host_order
	 * Находятся ли заголовок и данные в host byte order
	 * @return
	 * @note чтобы для компьютеров где network byte order совпадает с host byte order
	 * не выполнять излишние преобразования
	 * следует проверять сразу целевое состояние, например если нужен host byte order
	 * то сразу if (is_host_order) return this;
	 */
	virtual bool is_host_order() const noexcept = 0;
	/**
	 * @brief is_network_order
	 * Находятся ли заголовок и данные в network byte order
	 * @return
	 * @note чтобы для компьютеров где network byte order совпадает с host byte order
	 * не выполнять излишние преобразования
	 * следует проверять сразу целевое состояние, например если нужен network byte order
	 * то сразу if (is_network_order) return this;
	 */
	virtual bool is_network_order() const noexcept = 0;
	virtual std::int32_t data_size() const noexcept = 0;
	virtual std::int32_t key() const noexcept = 0;
	virtual void set_key(std::int32_t _key) noexcept = 0;
	//    template<typename K>
	//    void typed_set_key(K key)
	//    {
	//      set_key(static_cast<std::int32_t>(key));
	//    }
	virtual std::int32_t data_type() const noexcept = 0;

	virtual void copy_to_stream_host_order(std::ofstream & out) = 0;
	virtual void copy_to_stream_network_order(std::ofstream & out) = 0;
	virtual Result copy_to_fd_host_order(std::int32_t fd) = 0;
	virtual Result copy_to_fd_network_order(std::int32_t fd) = 0;
	virtual Result copy_to_buf_host_order(char *& buf, std::int32_t & buf_size) = 0;
	virtual Result copy_to_buf_network_order(char *& buf, std::int32_t & buf_size) = 0;

	// Возможные состояния
	enum class State
	{
		// готов к использованию
		Ready,
		// идёт загрузка заголовка
		LoadingHeader,
		// идёт загрузка данных
		LoadingData,
		// идёт выгрузка заголовка
		CopyingHeader,
		// идёт выгрузка данных
		CopyingData,
		// сломался
		Error
	};
	virtual State state() const noexcept = 0;

	/**
	 * @brief reset_state
	 * Если шла итерация выгрузки, то сбросит итерацию в начальное положение.
	 * Если шла загрузка, то сбросит в исходное состояние.
	 * Если было состояние State::Error, то всё очистит
	 */
	virtual void reset_state() noexcept = 0;

	/**
	  Заголовок буфера объекта, хранящегося в Dson.
	  Описывает содержимое буфера.
	  Аллоцированный буфер всегда начинается с заголовка
	  (размер буфера = sizeof(Header) + размер хранимых данных)
	*/
	struct Header
	{
		/*
		 * Метка: как расположены байты:
		 * mark_host_order => host byte order
		 * mark_network_order => network byte order
		 */
		std::uint32_t mark_byte_order_; // [0]
		// Размер данных
		std::int32_t data_size_; // [1]
		// Ключ для поиска/идентификации данных
		std::int32_t key_; // [2]
		// Тип данных
		std::int32_t data_type_; // [3]
	};
	static constexpr std::int32_t header_size{sizeof(Header)};
	static constexpr std::int32_t header_array_len{sizeof(Header) / sizeof(std::uint32_t)};

	/**
	 * @brief to_buf_host_order
	 * Для передачи в/из сторонние библиотеки
	 * лучше использовать простые типы.
	 *
	 * Этот вспомогательный метод поможет представить Dson в виде char * буфера.
	 * @return
	 */
	std::vector<char> to_buf_host_order()
	{
		std::int32_t buf_size = data_size() + header_size;
		std::vector<char> re(data_size() + header_size);
		char * buf = re.data();
		const auto result = copy_to_buf_host_order(buf, buf_size);
		assert(0 == buf_size && result == Result::Ready);
		return re;
	}

	State state_{State::Ready};
	// Итератор загрузки/выгрузки
	std::int32_t offset_{0};
};

inline std::ofstream & operator<<(std::ofstream & out, DsonObj & dson)
{
	dson.copy_to_stream_network_order(out);
	return out;
}

} // namespace hi

#endif // DSON_OBJ_H

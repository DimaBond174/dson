#ifndef PROGRESS_STATE_H
#define PROGRESS_STATE_H

namespace hi
{

/**
 * @brief The Result enum
 * Результат операции
 */
enum class Result
{
	// Возникли ошибки
	Error,

	// Готово, можно идти дальше
	Ready,

	// Работа в процессе, но всё равно можно идти дальше
	InProcess
};

} // namespace hi

#endif // PROGRESS_STATE_H

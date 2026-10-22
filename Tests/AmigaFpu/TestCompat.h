#pragma once

// The production headers currently rely on these being included transitively.
#include <limits>
#include <memory>

// std::uninitialized_move became public in C++17. GCC 6.5 has the internal
// machinery but not the public function, while Jazz2's SmallVector uses it.
#if __cplusplus < 201703L
namespace std
{
	template<class InputIterator, class ForwardIterator>
	ForwardIterator uninitialized_move(InputIterator first, InputIterator last, ForwardIterator result)
	{
		return std::uninitialized_copy(std::make_move_iterator(first),
			std::make_move_iterator(last), result);
	}
}
#endif

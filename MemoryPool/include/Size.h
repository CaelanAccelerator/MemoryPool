#pragma once
#include <cstddef>
using std::size_t;

namespace Size {
	constexpr size_t MAX_ALLOC_SIZE{ 512 };
	constexpr size_t ALIGNMENT{ 8 };
	constexpr size_t FREE_LIST_SIZE{ 64 };
	constexpr size_t PAGE_SIZE{ 4096 };
	constexpr size_t SPAN_PAGES{ 8 };
}
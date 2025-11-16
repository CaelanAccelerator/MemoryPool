#pragma once

namespace Size {
	constexpr size_t ALIGNMENT = 8; 
	constexpr size_t FREE_LIST_SIZE = 64;
	const size_t PAGE_SIZE{ 4096 };
	const size_t SPAN_PAGES{ 8 };
}
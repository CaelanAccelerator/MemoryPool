#pragma once

namespace Size {
	constexpr size_t ALIGNMENT = 8;
	constexpr size_t MAX_BYTES = 256 * 1024; // 256KB
	constexpr size_t FREE_LIST_SIZE = MAX_BYTES / ALIGNMENT;
	const size_t PAGE_SIZE{ 4096 };
	const size_t SPAN_PAGES{ 8 };
}
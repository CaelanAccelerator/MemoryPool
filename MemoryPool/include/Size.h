#pragma once
#include <cstddef>
using std::size_t;

namespace Size {
	constexpr size_t MAX_ALLOC_SIZE{ 2048 };
	constexpr size_t ALIGNMENT{ 8 };
	constexpr size_t FREE_LIST_SIZE{ 28 };
	constexpr size_t PAGE_SIZE{ 4096 };
	constexpr size_t SPAN_PAGES{ 8 };

	// Tiered size-class scheme (28 classes, 8 B – 2048 B):
	//   [0-7]   8 B aligned:  8, 16, 24, 32, 40, 48, 56, 64
	//   [8-11]  16B aligned:  80, 96, 112, 128
	//   [12-15] 32B aligned:  160, 192, 224, 256
	//   [16-19] 64B aligned:  320, 384, 448, 512
	//   [20-23] 128B aligned: 640, 768, 896, 1024
	//   [24-27] 256B aligned: 1280, 1536, 1792, 2048

	inline size_t sizeToIndex(size_t size)
	{
		if (size <= 64)   return (size - 1) / 8;
		if (size <= 128)  return 8  + (size - 65)  / 16;
		if (size <= 256)  return 12 + (size - 129) / 32;
		if (size <= 512)  return 16 + (size - 257) / 64;
		if (size <= 1024) return 20 + (size - 513) / 128;
		return                   24 + (size - 1025) / 256;
	}

	inline size_t indexToBlockSize(size_t index)
	{
		if (index < 8)  return (index + 1) * 8;
		if (index < 12) return 64  + (index - 7)  * 16;
		if (index < 16) return 128 + (index - 11) * 32;
		if (index < 20) return 256 + (index - 15) * 64;
		if (index < 24) return 512 + (index - 19) * 128;
		return                 1024 + (index - 23) * 256;
	}
}

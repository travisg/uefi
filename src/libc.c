
#include <string.h>

void *memset(void *_dst, int c, size_t n) {
	uint8_t *dst = _dst;
	while (n-- > 0) {
		*dst++ = c;
	}
	return _dst;
}

void *memcpy(void *_dst, const void *_src, size_t n) {
	uint8_t *dst = _dst;
	const uint8_t *src = _src;
	while (n-- > 0) {
		*dst++ = *src++;
	}
	return _dst;
}

int memcmp(const void *_a, const void *_b, size_t n) {
	const uint8_t *a = _a;
	const uint8_t *b = _b;
	while (n-- > 0) {
		int x = *a++ - *b++;
		if (x != 0) {
			return x;
		}
	}
	return 0;
}


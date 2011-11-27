#include <stdlib.h>
#include <string.h>

#include "buffer.h"

HBUF
buf_alloc(buf_size_callback size_callback) {
	HBUF res = NULL;
	uint8_t *buf = NULL;
	uint32_t size = 0;

/*	if (!size_callback) {
		return NULL;
	} */

	res = malloc(sizeof(SBUF));
	memset(res, 0, sizeof(SBUF));

	if (size_callback) {
		size = size_callback(res, BUFFER_SIZE_TYPE_HEADER);
	}

	if (size > 0) {
		buf = malloc(size);
	}

	if (!res || (!buf && size > 0)) {
		if (res) {
			free(res);
			res = NULL;
		}
	} else {
		res->buf = buf;
		res->alloced_size = size;
		res->full_size = size;
		res->ref = 1;
		res->size_callback = size_callback;
	}

	return res;
}

void
buf_free(HBUF buf)
{
	if (!buf) {
		return;
	}

	buf->ref -= 1;
	if (buf->ref > 0) {
		return;
	}

	if (buf->buf) {
		free(buf->buf);
	}

	free(buf);

	return;
}

void
buf_get(HBUF buf)
{
	if (buf) {
		/* :BUG: ref counter can be overflowed */
		buf->ref++;
	}

	return;
}

void
buf_resize(HBUF buf, uint32_t new_size)
{
	if (!buf || buf->alloced_size == new_size)
		return;

	if (buf->size > new_size)
		buf->size = new_size;

	buf->buf = realloc(buf->buf, new_size);
	buf->alloced_size = new_size;

	return;
}

#include <stdlib.h>

#include "buffer.h"

HBUF
buf_alloc(uint32_t size) {
	HBUF res = NULL;
	uint8_t *buf;

	res = malloc(sizeof(SBUF));
	buf = malloc(sizeof(size));

	if (!res || (!buf && size > 0)) {
		if (res) {
			free(res);
			res = NULL;
		}
	} else {
		res->buf = buf;
		res->size = size;
		res->ref = 1;
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
}

void
buf_get(HBUF buf)
{
	if (buf) {
		/* :BUG: ref counter can be overflowed */
		buf->ref++;
	}
}


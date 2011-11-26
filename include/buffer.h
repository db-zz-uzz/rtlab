#ifndef BUFFER_H_INCLUDED
#define BUFFER_H_INCLUDED

#include <stdint.h>

#define BUFFER_FLAG_HEADER_ASSIGNED	0x01
#define BUFFER_FLAG_DATA_READY		0x02
#define BUFFER_FLAG_DATA_PROCESSED	0x04
#define BUFFER_FLAG_HEADER_READED	0x08

#define BUFFER_SIZE_TYPE_HEADER		0x01
#define BUFFER_SIZE_TYPE_MESSAGE	0x02

#define BUF_READ_POINT(pbuffer) ((pbuffer)->buf + (pbuffer)->size)

#define HSAMPLE HBUF
typedef struct tagSBUF SBUF, *HBUF;

typedef uint32_t (*buf_size_callback)(HBUF buf, uint8_t type);

struct tagSBUF {
	uint8_t *buf;

	/* allocated buffer size */
	uint32_t alloced_size;

	/* current data size */
	uint32_t size;

	/* full data size */
	uint32_t full_size;

	/* reference counter */
	uint8_t ref;

	uint32_t flags;

	buf_size_callback size_callback;
};

HBUF
buf_alloc(buf_size_callback size_callback);

void
buf_free(HBUF buf);

void
buf_get(HBUF buf);

void
buf_resize(HBUF buf, uint32_t size);

#endif // BUFFER_H_INCLUDED

#ifndef BUFFER_H_INCLUDED
#define BUFFER_H_INCLUDED

#include <stdint.h>

typedef struct tagSBUF {
	uint8_t *buf;
	uint32_t size;
	uint8_t ref;
} SBUF, *HBUF;

HBUF
buf_alloc(uint32_t size);

void
buf_free(HBUF buf);

void
buf_get(HBUF buf);


#endif // BUFFER_H_INCLUDED

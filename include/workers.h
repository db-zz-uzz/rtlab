#ifndef WORKERS_H_INCLUDED
#define WORKERS_H_INCLUDED

#include <stdint.h>

#include "utils.h"

#define STATE_FLAG_HEADER_ASSIGNED	0x01
#define STATE_FLAG_DATA_READY		0x02
#define STATE_FLAG_DATA_PROCESSED	0x04

typedef struct tagSSAMPLESTATE {
	uint32_t readed;
	uint8_t *buf;
	uint32_t size;
	uint32_t data_size;
	uint8_t flags;
} SSAMPLESTATE, *PSSAMPLESTATE;

int
do_read_fd(int fd, PSSAMPLE sample);


#endif // WORKERS_H_INCLUDED

#ifndef AUDIO_SAMPLE_H_INCLUDED
#define AUDIO_SAMPLE_H_INCLUDED

#include <sys/time.h>

#include "buffer.h"

#define HEADER_SIZE sizeof(SSAMPLEHEADER)
#define BUF_SIZE(header) 	((header)->channels * (header)->sample_size * (header)->samples)

enum E_SOCK_USE {
	SOCK_USE_OK 	= 0,
	SOCK_USE_CLOSE 	= 1,
	SOCK_USE_READY 	= 2,
};

enum E_BUFFER_TYPE {
	BUF_TYPE_INTERLEAVED 	= 0,
	BUF_TYPE_SEQUENTIAL 	= 1,
};

typedef struct tagSSAMPLEHEADER {
	uint8_t channels;
	uint8_t sample_size;

	/* buffer type. interleaved or sequential */
	uint8_t buf_type;

	/* media sample timestamp */
	struct timeval timestamp;

	uint32_t number;
	/* sample count in buffer */
	uint32_t samples;
} SSAMPLEHEADER, *PSSAMPLEHEADER;

void
print_header(PSSAMPLEHEADER header, uint8_t *buf, uint32_t data_size);

#endif // AUDIO_SAMPLE_H_INCLUDED
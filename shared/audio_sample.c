#include <xcrypt.h>

#include "audio_sample.h"


void
print_header(PSSAMPLEHEADER header, uint8_t *buf, uint32_t data_size)
{
	uint8_t i;
	uint8_t res[16];

	md5_buffer((char *)buf, data_size, (void *)res);

	printf("[%6u] %i.%06i  %u/%u/%u (%s)  ",
			header->number,
			header->timestamp.tv_sec,
			header->timestamp.tv_usec,
			header->channels,
			header->sample_size,
			header->samples,
			header->buf_type == BUF_TYPE_INTERLEAVED ? "I" : "S");

	for (i = 0; i < 16; i++) {
		printf("%02x", res[i]);
	}
	printf("  %u/%u B\n", BUF_SIZE(header), data_size);

	return;
}

uint32_t
sample_size_callback(HBUF buf, uint8_t type)
{
	uint32_t res = 0;

	switch (type) {
	case BUFFER_SIZE_TYPE_MESSAGE:
		if (buf) {
			PSSAMPLEHEADER header = (PSSAMPLEHEADER)buf->buf;
			res += BUF_SIZE(header);
		}
		/* fall through due to need complete data size */
	case BUFFER_SIZE_TYPE_HEADER:
		res += HEADER_SIZE;

	default:
		/* unknown action */
		break;
	}

	return res;
}

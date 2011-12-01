#include <stdio.h>

#include "audio_sample.h"

//#define PRINT_MD5
//#define PRINT_LIGHT
#define ASAMPLE_SILENT

#ifdef PRINT_MD5
#include <xcrypt.h>
#endif

#ifndef ASAMPLE_SILENT
#ifndef PRINT_LIGHT
static char *
buf_type_letter(enum E_BUFFER_TYPE buf_type)
{
	switch (buf_type) {
		case BUF_TYPE_INTERLEAVED:
			return "I";

		case BUF_TYPE_SEQUENTIAL:
			return "S";

		case BUF_TYPE_FFTED:
			return "F";

		case BUF_TYPE_UNKNOWN:
		default:
			return "U";
	}
}
#endif
#endif // ASAMPLE_SILENT

#ifdef PRINT_LIGHT
void
print_header(PSSAMPLEHEADER header, uint8_t *buf, uint32_t data_size)
{
#ifndef ASAMPLE_SILENT
	printf("[%6u] %ld.%06ld\n",
			header->number,
			header->timestamp.tv_sec,
			header->timestamp.tv_usec);
	return;
#endif // ASAMPLE_SILENT
}

#else

void
print_header(PSSAMPLEHEADER header, uint8_t *buf, uint32_t data_size)
{
#ifndef ASAMPLE_SILENT

#ifdef PRINT_MD5
	uint8_t i;
	uint8_t res[16];

	md5_buffer((char *)buf, data_size, (void *)res);
#endif

	printf("[%6u] %ld.%06ld  %u(%u)/%u/%u/%u (%s)  ",
			header->number,
			header->timestamp.tv_sec,
			header->timestamp.tv_usec,
			header->channel_no,
			header->channels,
			header->sample_size,
			header->samples,
			header->samplerate,
			buf_type_letter(header->buf_type));

#ifdef PRINT_MD5
	for (i = 0; i < 16; i++) {
		printf("%02x", res[i]);
	}
#endif

	printf("  %u/%u B\n", BUF_SIZE(header), data_size);

	return;
#endif // ASAMPLE_SILENT
}
#endif

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

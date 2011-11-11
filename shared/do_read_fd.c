
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "workers.h"

static int
sample_append_data(PSSAMPLE sample, uint8_t *buf, int size)
{
	PSSAMPLESTATE state = (PSSAMPLESTATE)sample->private_data;
/*
	printf("append() sb %p, ss %u, sd %u : buf %p, bs %u",
			state->buf,
			state->size,
			state->data_size,
			buf,
			size);
*/
	int excess_size = state->data_size + size - state->size;
	if (excess_size > 0) {
		state->size += excess_size;
		state->buf = realloc(state->buf, state->size);
	}

	memcpy(state->buf + state->data_size, buf, size);
	state->data_size += size;

	return state->data_size;
}

#define RECV_BUF_LEN 30000

int
do_read_fd(int fd, PSSAMPLE sample)
{
	PSSAMPLESTATE state = (PSSAMPLESTATE)sample->private_data;

	uint32_t buflen = RECV_BUF_LEN;
	uint8_t buf[RECV_BUF_LEN];
	int ret = 0;

	if (state->flags & STATE_FLAG_DATA_READY &&
		state->flags & STATE_FLAG_DATA_PROCESSED) {
		/* move new sample partial at the start of buffer */
		uint32_t message_size = HEADER_SIZE + BUF_SIZE(sample->header);

		if (state->data_size > message_size) {
			memmove(state->buf, state->buf + message_size, state->data_size - message_size);
		}
		state->data_size -= message_size;

		state->flags &= ~STATE_FLAG_DATA_PROCESSED;

		if (state->data_size < HEADER_SIZE) {
			state->flags &= ~STATE_FLAG_HEADER_ASSIGNED;
		}

		if (state->data_size < message_size) {
			state->flags &= ~STATE_FLAG_DATA_READY;
		}
	}

	for (;/*readed < RECV_BUF_LEN*/;) {
		ret = recv(fd, buf, buflen, MSG_DONTWAIT);
		if (ret == 0) {
			/* connection closed */
			return SOCK_USE_CLOSE;
		} else if (ret == -1) {
			if (errno == EWOULDBLOCK) {
				/* no more data */
				/* printf("return no-data\n"); */
				break;
			} else {
				/* error occured */
				fprintf(stderr, "%s(): recv error\n", __func__);
				return SOCK_USE_CLOSE;
			}
		}

		/* printf("private buf size %u, data %u\n", state->size, state->data_size); */
		sample_append_data(sample, buf, ret);
		/* printf("            size %u, data %u\n", state->size, state->data_size); */

		if (state->data_size >= HEADER_SIZE &&
			(state->flags & STATE_FLAG_HEADER_ASSIGNED) == 0) {
			/* copy header data into header */
			/* just set pointers :] */
			sample->header = (PSSAMPLEHEADER)state->buf;
			sample->buf = state->buf + HEADER_SIZE;

			state->flags |= STATE_FLAG_HEADER_ASSIGNED;
		}

		if (state->flags & STATE_FLAG_HEADER_ASSIGNED) {
			uint32_t message_size = HEADER_SIZE + BUF_SIZE(sample->header);
/*
			printf("[%6u] %i.%06i  %u/%u/%u (%s)\n",
				sample->header->number,
				sample->header->timestamp.tv_sec,
				sample->header->timestamp.tv_usec,
				sample->header->channels,
				sample->header->sample_size,
				sample->header->samples,
				sample->header->buf_type == BUF_TYPE_INTERLEAVED ? "I" : "S");

			print_buf_head(state->buf, 48, "buffer");
			print_buf_head(sample->header, 48, "header");
*/

			if (state->data_size >= message_size) {
				state->flags |= STATE_FLAG_DATA_READY;
			}
		}

		if (state->flags & STATE_FLAG_DATA_READY) {
			//printf("buf ready\n");
			return SOCK_USE_READY;
		}
		//printf("readed %i\t flags %02x\n", res, state->flags);
		//readed += res;
	}

	return SOCK_USE_OK;
}

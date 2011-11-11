#ifndef __SHARED_UTILS_INCLUDED
#define __SHARED_UTILS_INCLUDED

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/queue.h>
#include <sys/time.h>

#define handle_error_en(en, msg) \
	do { fprintf(stderr, "%s:%i %s:%s", __FILE__, __LINE__, msg, strerror(en)); exit(EXIT_FAILURE); } while (0)

#define handle_error(msg) \
	do { fprintf(stderr, "%s:%i %s:%s", __FILE__, __LINE__, msg, strerror(errno)); exit(EXIT_FAILURE); } while (0)

#define MAGIC_NUM 0xDEADBEEF

enum E_SOCK_USE {
	SOCK_USE_OK 	= 0,
	SOCK_USE_CLOSE 	= 1,
	SOCK_USE_READY 	= 2,
};

enum E_BUFFER_TYPE {
	BUF_TYPE_INTERLEAVED 	= 0,
	BUF_TYPE_SEQUENTIAL 	= 1,
};

#define BUF_SIZE(header) 	((header)->channels * (header)->sample_size * (header)->samples)

#define HEADER_SIZE		sizeof(SSAMPLEHEADER)

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

typedef struct tagSSAMPLE  SSAMPLE, *PSSAMPLE;

typedef int(*SAMPLE_HANDLER)(int, PSSAMPLE);
typedef void(*PRIVATE_DESTRUCTOR)(void *);

struct tagSSAMPLE {
	PSSAMPLEHEADER header;
	uint8_t *buf;
	uint32_t buf_size;

	int epollfd;
	SAMPLE_HANDLER write_func;
	SAMPLE_HANDLER read_func;

	void *private_data;
	PRIVATE_DESTRUCTOR private_destructor;
};

/* fd list definitions */
LIST_HEAD(fd_list_head, fd_list_entry);
struct fd_list_entry {
	int fd;
	struct sockaddr_in addr;

	LIST_ENTRY(fd_list_entry) l;
};

int
bind_addr(int port, int backlog);

int
connect_to(char *addr, char *port);

/*
void
setnonblocking(int sock);
*/

int
close_connection(int epollfd, struct fd_list_head *fd_list, int fd, const char *debug_str);

int
make_connection(int epollfd, struct fd_list_head *fd_list, int fd);


int
sample_alloc_buffer(PSSAMPLE sample, uint32_t size);

int
sample_free_buffer(PSSAMPLE sample);

void
sample_zero_buffer(PSSAMPLE sample);

int
deliver_data(struct fd_list_head *fd_list, PSSAMPLE sample);

int
print_md5(PSSAMPLE sample);

void
print_buf_head(void *buf, int size, char *prefix);


int
do_write_fd(int fd, PSSAMPLE sample);

#endif

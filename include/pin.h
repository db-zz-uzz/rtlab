#ifndef PIN_H_INCLUDED
#define PIN_H_INCLUDED

#include <stdint.h>
#include <netdb.h>

#include "buffer.h"

#define PIN_TYPE_UNKNOWN	0x00
#define PIN_TYPE_INPUT		0x01
#define PIN_TYPE_OUTPUT		0x02
#define PIN_TYPE_LISTEN		0x04
#define PIN_TYPE_CUSTOM		0x08

#define PIN_EVENT_READ		0x01
#define PIN_EVENT_WRITE		0x02

#define PIN_ERROR			0x04
#define PIN_OK				0x08

#define PIN_STATUS_READY	1
#define PIN_STATUS_PARTIAL	3
#define PIN_STATUS_CLOSED	2
#define PIN_STATUS_NO_DATA	4

#define MAX_PIN_COUNT		10

typedef struct tagSBUFLISTENTRY SBUFLISTENTRY, *PSBUFLISTENTRY;
typedef struct tagSPINLIST SPINLIST, *HPINLIST;
typedef struct tagSPIN SPIN, *HPIN;

typedef void (*pin_accept_callback_t)(HPIN, HPIN);

struct tagSBUFLISTENTRY {
	uint8_t *buffer;
	uint32_t size;
	uint32_t written;
	uint8_t ref;

	SBUFLISTENTRY *next;
};

struct tagSPIN {
	int fd;
	struct sockaddr_in addr;

	uint8_t type;
	uint8_t user_flags;
	SBUFLISTENTRY *buf_list_head;
	SBUFLISTENTRY **buf_list_tail;
	HPIN next_pin;
	HPINLIST parent_list;

	pin_accept_callback_t accept_callback;
};

struct tagSPINLIST {
	int epollfd;
	HPIN head;

	void *events;
	int events_count;
	/* array of pointers to available for write */
	HPIN *write_list;
	int write_list_size;
	/* array of pointers to available for read */
	HPIN *read_list;
	int read_list_size;
};

void
setnonblocking(int sock);

uint8_t
pin_get_flags(HPIN pin);

void
pin_set_flags(HPIN pin, uint8_t flags);

HPINLIST
pin_list_create(int events_count);

void
pin_list_destroy(HPINLIST pin_list);

HPIN
pin_list_add_custom_fd(HPINLIST pin_list, int fd, uint32_t pin_type);

/* bind addr and add into list */
HPIN
pin_listen(HPINLIST pin_list, int port, int backlog, pin_accept_callback_t accept_callback);

/* connect to addr and add into list */
HPIN
pin_connect(HPINLIST pin_list, char *addr);

/* discart all pending data and close connection */
int
pin_disconnect(HPIN pin);

void
pin_modify(HPIN pin, uint16_t flags);

/* request notify about write buf availability
		is_notify = 0/1 - disables/enables notifying */
void
pin_notify_write(HPIN pin, uint8_t is_notify);

/* wait event on pin list */
int
pin_list_wait(HPINLIST pin_list, int timeout);

/* return next pin with available event event_type */
HPIN
pin_list_get_next_event(HPINLIST pin_list, uint16_t event_type);

uint8_t
pin_list_pending_events(HPINLIST pin_list, uint16_t event_type);

/* write all delayed buffers */
int
pin_list_deliver(HPINLIST pin_list);

int
pin_read_raw(HPIN pin, void *dst, int siz);

int
pin_read_sample(HPIN pin, HBUF sample);

int
pin_list_write_sample(HPINLIST pin_list, HBUF sample, uint8_t restrict_pin);

#endif /* PIN_H_INCLUDED */

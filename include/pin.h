#ifndef PIN_H_INCLUDED
#define PIN_H_INCLUDED

#include <sys/queue.h>
#include <stdint.h>
#include <netdb.h>

#define PIN_TYPE_INPUT		0x01
#define PIN_TYPE_OUTPUT		0x02

#define PIN_EVENT_READ		0x01
#define PIN_EVENT_WRITE		0x02

#define PIN_ERROR			0x04
#define PIN_OK				0x08

#define MAX_PIN_COUNT		10

typedef void *HSAMPLE;
typedef struct tagSBUF SBUF, *HBUF;
typedef struct tagSPINLIST SPINLIST, *HPINLIST;
typedef struct tagSPIN SPIN, *HPIN;

struct tagSBUF {
	uint8_t *buf;
	uint32_t buf_size;
	HBUF next;
};

struct tagSPIN {
	int fd;
	struct sockaddr_in addr;

	HBUF buf_list_head;
	HBUF buf_list_tail;
	HPIN next_pin;
	HPINLIST parent_list;
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

HPINLIST
pin_list_create();

void
pin_list_destroy(HPINLIST pin_list);


/* bind addr and add into list */
int
pin_listen(HPINLIST pin_list, int port, int events_count, int backlog);

/* connect to addr and add into list */
int
pin_connect(HPINLIST pin_list, char *addr, char *port);

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

/* write all delayed buffers */
int
pin_list_deliver(HPINLIST *pin_list);

int
pin_read_sample(HPIN pin, HSAMPLE sample);

int
pin_write_sample(HPIN pin, HSAMPLE sample);

#endif // PIN_H_INCLUDED

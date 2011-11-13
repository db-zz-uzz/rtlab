#include <stdio.h>
#include <stdlib.h>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <errno.h>

#include "buffer.h"
#include "pin.h"

#define MAX_EVENTS 5
#define BACKLOG 50


int
main(int argc, char *argv[])
{
	uint8_t active = 1;

	int listen_port = 15020;

	HPINLIST connection = NULL;
	HPIN pin;
	HPIN input_pin;
	HSAMPLE sample, input_sample, dummy_sample;

	if (argc < 3) {
		printf("use: split <host> <port>\n");
		return 0;
	}

	if (argc > 3) {
		sscanf(argv[3], "%i", &listen_port);
		printf("Will listen %i port\n", listen_port);
	}

	connection = pin_list_create();
	pin_listen(connection, listen_port, MAX_EVENTS, BACKLOG);
	input_pin = pin_connect(connection, argv[1], argv[2]);

	printf("Enter main loop\n");
	while (active && pin_list_wait(connection, -1) != PIN_ERROR) {

		/* loop for pins with active events */
		while ( (pin = pin_list_get_next_event(connection, PIN_EVENT_READ)) != NULL ) {

			sample = pin == input_pin ? input_sample : dummy_sample;

			switch (pin_read_sample(pin, sample)) {
				case PIN_STATUS_READY:
				{
					/* process data here */
					break;
				}
				case PIN_STATUS_CLOSED:
				{
					/* close data and skip iteration */
					continue;
				}
				case PIN_STATUS_PARTIAL:
				{
					/* do nothing since no data ready */
					break;
				}
				case PIN_STATUS_NO_DATA:
				default:
				{
					break;
				}
			}
		}

		pin_list_deliver(connection);
	}

	pin_list_destroy(connection);

	exit(EXIT_SUCCESS);
}

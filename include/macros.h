#ifndef MACROS_H_INCLUDED
#define MACROS_H_INCLUDED

#include <string.h>
#include <errno.h>

#define handle_error_en(en, msg) \
	do { fprintf(stderr, "%s:%i %s:%s\n", __FILE__, __LINE__, msg, strerror(en)); exit(EXIT_FAILURE); } while (0)

#define handle_error(msg) \
	do { fprintf(stderr, "%s:%i %s:%s\n", __FILE__, __LINE__, msg, strerror(errno)); exit(EXIT_FAILURE); } while (0)

#define PTR_SIZE	(sizeof(void *))
#define MESSAGE_END	(void *)-1


#endif /* MACROS_H_INCLUDED */

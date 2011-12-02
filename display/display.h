#ifndef DISPLAY_H_INCLUDED
#define DISPLAY_H_INCLUDED

#include <stdint.h>
#include "buffer.h"

#define PRINT_DEBUG

#ifdef PRINT_DEBUG
# include <pthread.h>
#endif

typedef struct tagSMETABUFER {
	/** CAUTION: order matters */
	HBUF left_fft;
	HBUF right_fft;
	HBUF left;
	HBUF right;
	/** CAUTION: doesn't rearrange first 4 fields */

	HBUF sd_log;
	HBUF sd_mod;
	uint64_t timestamp;
	uint64_t dts;
} SMETABUFER, *PSMETABUFER;

typedef struct tagSTHRPARAMS {
	int argc;
	char **argv;
	int infd;
	int outfd;
#ifdef PRINT_DEBUG
	pthread_mutex_t *print_mutex;
#endif
} STHRPARAMS, *PSTHRPARAMS;

#ifdef PRINT_DEBUG
# define PRINT_LOCK(mutex)		pthread_mutex_lock(mutex)
# define PRINT_UNLOCK(mutex)	pthread_mutex_unlock(mutex)
# define safe_print(mutex, args...) do { \
	PRINT_LOCK(mutex); \
	printf(args); \
	PRINT_UNLOCK(mutex); \
} while(0)
#else
# define PRINT_LOCK(mutex)
# define PRINT_UNLOCK(mutex)
# define safe_print(mutex, args...) printf(args)
#endif

void *
reorderer_thr(void *args);

void *
spawn_ui_thr(void *args);

PSMETABUFER
metabuf_alloc();

void
buffering_init();

void
metabuf_free(PSMETABUFER metabuf);

PSMETABUFER
buferize_sample(HBUF sample);

void
calc_data(PSMETABUFER metabuf);

#endif /* DISPLAY_H_INCLUDED */

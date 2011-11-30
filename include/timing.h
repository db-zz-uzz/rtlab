#ifndef TIMING_H_INCLUDED
#define TIMING_H_INCLUDED

#include <sys/time.h>

#define TIMING_MEASURE_AREA	\
	struct timeval timing_measure_start = {0}

#define TIMING_START() \
	gettimeofday(&timing_measure_start, NULL)

#define TIMING_END(prefix) \
do { \
	uint64_t usec; \
	struct timeval timing_measure_end = {0}; \
	gettimeofday(&timing_measure_end, NULL); \
	usec = ((uint64_t)timing_measure_end.tv_sec * 1000000 + timing_measure_end.tv_usec) - \
		   ((uint64_t)timing_measure_start.tv_sec * 1000000 + timing_measure_start.tv_usec); \
	printf("[%s] work time %llu usec\n", (prefix), usec); \
} while (0)

#define TIMING_DIFF(start, end) \
	(uint64_t)((end).tv_sec * 1000000 + (end).tv_usec) - (uint64_t)((start).tv_sec * 1000000 + (start).tv_usec)

#endif // TIMING_H_INCLUDED

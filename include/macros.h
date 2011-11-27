#ifndef MACROS_H_INCLUDED
#define MACROS_H_INCLUDED

#define handle_error_en(en, msg) \
	do { fprintf(stderr, "%s:%i %s:%s\n", __FILE__, __LINE__, msg, strerror(en)); exit(EXIT_FAILURE); } while (0)

#define handle_error(msg) \
	do { fprintf(stderr, "%s:%i %s:%s\n", __FILE__, __LINE__, msg, strerror(errno)); exit(EXIT_FAILURE); } while (0)


#endif // MACROS_H_INCLUDED

#ifndef PORTALS4_UTIL_H
#define PORTALS4_UTIL_H


/**
 * Wrapper for Portals API calls.
 * Aborts the program if there is an error.
 */
#define PTL_CHECK(x) \
	do { \
		int rc; \
		/* fprintf(stderr, "Calling %s\n", #x); */ \
		rc = x; \
		if ((rc == PTL_OK) || (rc == PTL_IGNORED)) { \
			/* Success */ \
			/* fprintf(stderr, "    %s\n", ptl_return_code_str(rc)); */ \
		} else { \
			/* Failure */ \
			fprintf(stderr, "    %s (%s(), line %u).\n", \
					ptl_return_code_str(rc), \
					__FUNCTION__, (unsigned int)__LINE__); \
			abort(); \
		} \
	} while (0)


/**
 * Wrapper that aborts the program if the expression x is false.
 * Prints function name and line number that caused the error to the screen.
 */
#define PTL_ASSERT(x) \
	do { \
		if (!(x)) { \
			/* Failure */ \
			fprintf(stderr, "PTL_ASSERT(%s) failed at %s(), line %u.\n", \
			        #x, __FUNCTION__, (unsigned int)__LINE__); \
			abort(); \
		} \
	} while (0)


/**
 * Wrapper that aborts the program unconditionally.
 */
#define PTL_ABORT \
	do { \
		fprintf(stderr, "PTL_ABORT at %s(), line %u.\n", \
		        __FUNCTION__, (unsigned int)__LINE__); \
		abort(); \
	} while (0)


/**
 * Malloc wrapper that aborts the program if there is an error.
 * Prints function name and line number that caused the error to the screen.
 */
#define MALLOC(x) Malloc(x, __FUNCTION__, __LINE__)
static inline void *
Malloc(size_t size, const char *function, unsigned int line)
{
	void *mem;
	if ((mem = malloc(size)) == NULL) {
		fprintf(stderr, "malloc(%llu) failed in %s(), line %u.\n",
		        (unsigned long long)size, function, line);
		abort();
	}
	return mem;
}


/**
 * Debug function prototypes.
 */
char *ptl_return_code_str(int return_code);
char *ptl_event_kind_str(ptl_event_kind_t type);
void ptl_print_event(ptl_event_t *ev);


/**
 * Queue function prototypes.
 */
typedef void *ptl_queue_t;
int ptl_queue_init(ptl_handle_ni_t ni_h,
                   ptl_uid_t uid, ptl_process_t match_id,
                   ptl_pt_index_t pt_index, ptl_handle_eq_t eq_h,
                   int num_blocks, size_t block_size, size_t min_free,
                   ptl_queue_t *q);
int ptl_queue_process_event(ptl_queue_t q, ptl_event_t *ev);
void ptl_queue_is_drained(ptl_queue_t q);
int ptl_enqueue(ptl_process_t target_id, ptl_pt_index_t pt_index,
                ptl_handle_md_t md_h,
                ptl_size_t local_offset, ptl_size_t length,
                ptl_handle_eq_t eq_h);

#endif

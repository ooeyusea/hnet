#include "util.h"
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

void __OAssert(const char * file, int32_t line, const char * func, const char * debug) {
	fflush(stdout);
	fprintf(stderr,
		"\nAsssertion failed : \n=======assert string=======\nfile:%s\nline:%d\nfunction:%s\ndebug:%s\n===========================\n",
		file, line, func, debug);
	fflush(stderr);
	assert(false);
}

#ifdef __cplusplus
}
#endif


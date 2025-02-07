#include <stdlib.h>

__attribute__((__noreturn__))
void abort(void) {
#if defined(__is_libk)
	// TODO: Add proper kernel panic.
#else
	// TODO: Abnormally terminate the process as if by SIGABRT.

#endif
	while (1) { }
	__builtin_unreachable();
}

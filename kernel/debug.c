#include <lwk/kernel.h>

char *
strerror(int errnum)
{
	if (errnum < 0)
		errnum *= -1;

	switch (errnum) {
		case ENOMEM:	return "Out of memory";
		case EINVAL:	return "Invalid argument";
	}

	return "unknown";
}


#include <sys/time.h>
#include <sys/select.h>

struct pollfd {
	int fd;
	short events;
	short revents;
};

enum {
	POLLIN = 1,
	POLLOUT = 2,
	POLLERR = 4,
	POLLHUP = 8,
	POLLNVAL = 16,
};

int poll(struct pollfd *, unsigned int, int);

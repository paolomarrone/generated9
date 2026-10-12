#include <stddef.h>
#include <fcntl.h>
#include "poll9.h"

int
poll(struct pollfd *fds, unsigned int n, int timeout)
{
	fd_set rd, wr;
	struct timeval tv, *tp;
	unsigned int i;
	int maxfd, ready;

	FD_ZERO(&rd);
	FD_ZERO(&wr);
	maxfd = -1;
	ready = 0;
	for(i = 0; i < n; i++){
		fds[i].revents = 0;
		if(fds[i].fd < 0)
			continue;
		if(fds[i].fd >= FD_SETSIZE || fcntl(fds[i].fd, F_GETFD) < 0){
			fds[i].revents = POLLNVAL;
			ready++;
			continue;
		}
		if(fds[i].events & POLLIN)
			FD_SET(fds[i].fd, &rd);
		if(fds[i].events & POLLOUT)
			FD_SET(fds[i].fd, &wr);
		if(fds[i].fd > maxfd)
			maxfd = fds[i].fd;
	}
	if(ready)
		timeout = 0;
	tp = NULL;
	if(timeout >= 0){
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = timeout % 1000 * 1000;
		tp = &tv;
	}
	if(select(maxfd + 1, &rd, &wr, NULL, tp) < 0)
		return -1;
	ready = 0;
	for(i = 0; i < n; i++){
		if(fds[i].fd < 0)
			continue;
		if(fds[i].revents == POLLNVAL){
			ready++;
			continue;
		}
		if(FD_ISSET(fds[i].fd, &rd))
			fds[i].revents |= POLLIN;
		if(FD_ISSET(fds[i].fd, &wr))
			fds[i].revents |= POLLOUT;
		if(fds[i].revents)
			ready++;
	}
	return ready;
}

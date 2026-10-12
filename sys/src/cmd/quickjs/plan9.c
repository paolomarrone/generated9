#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <lib9.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include "plan9.h"

#define PATH_MAX 4096

void
p9_init(void)
{
	/* JavaScript requires nearest rounding, subnormals and masked traps. */
	setfcr(0);
	setfsr(0);
}

char *
realpath(const char *path, char *buf)
{
	int fd, n;

	fd = open(path, O_RDONLY);
	if(fd < 0)
		return NULL;
	n = fd2path(fd, buf, PATH_MAX);
	close(fd);
	if(n < 0){
		errno = EIO;
		return NULL;
	}
	return buf;
}

/* APE's initial environment strings share one allocation. Own a copy
 * before changing individual entries; libc consumers may retain the original. */
static int
copyenv(void)
{
	static int copied;
	char **env;
	int i, n;

	if(copied)
		return 0;
	for(n = 0; environ[n]; n++)
		;
	env = malloc((n + 1) * sizeof *env);
	if(env == NULL)
		return -1;
	for(i = 0; i < n; i++){
		env[i] = strdup(environ[i]);
		if(env[i] == NULL){
			while(i > 0)
				free(env[--i]);
			free(env);
			return -1;
		}
	}
	env[n] = NULL;
	free(environ);
	environ = env;
	copied = 1;
	return 0;
}

int
setenv(const char *name, const char *value, int overwrite)
{
	char **env, *entry;
	size_t len;
	int i;

	if(*name == 0 || strchr(name, '=')){
		errno = EINVAL;
		return -1;
	}
	if(copyenv() < 0)
		return -1;
	len = strlen(name);
	for(i = 0; environ[i]; i++)
		if(strncmp(environ[i], name, len) == 0 && environ[i][len] == '=')
			break;
	if(environ[i] && !overwrite)
		return 0;
	entry = malloc(len + strlen(value) + 2);
	if(entry == NULL)
		return -1;
	strcpy(entry, name);
	entry[len] = '=';
	strcpy(entry + len + 1, value);
	if(environ[i])
		free(environ[i]);
	else{
		env = realloc(environ, (i + 2) * sizeof *env);
		if(env == NULL){
			free(entry);
			return -1;
		}
		environ = env;
		environ[i + 1] = NULL;
	}
	environ[i] = entry;
	return 0;
}

int
unsetenv(const char *name)
{
	size_t len;
	int i;

	if(*name == 0 || strchr(name, '=')){
		errno = EINVAL;
		return -1;
	}
	if(copyenv() < 0)
		return -1;
	len = strlen(name);
	for(i = 0; environ[i]; i++){
		if(strncmp(environ[i], name, len) == 0 && environ[i][len] == '='){
			free(environ[i]);
			do environ[i] = environ[i + 1]; while(environ[i++]);
			break;
		}
	}
	return 0;
}

/* Keep allocation accounting independent of APE's private malloc layout. */
typedef union Header Header;
union Header {
	size_t size;
	double align;
	void *pointer;
};

void *
p9_malloc(size_t size)
{
	Header *h;

	if(size > SIZE_MAX - sizeof *h)
		return NULL;
	h = malloc(sizeof *h + size);
	if(h == NULL)
		return NULL;
	h->size = size;
	return h + 1;
}

size_t
p9_msize(const void *p)
{
	return ((const Header *)p)[-1].size;
}

void
p9_free(void *p)
{
	if(p != NULL)
		free((Header *)p - 1);
}

void *
p9_realloc(void *p, size_t size)
{
	Header *h;

	if(p == NULL)
		return p9_malloc(size);
	if(size == 0){
		p9_free(p);
		return NULL;
	}
	if(size > SIZE_MAX - sizeof *h)
		return NULL;
	h = realloc((Header *)p - 1, sizeof *h + size);
	if(h == NULL)
		return NULL;
	h->size = size;
	return h + 1;
}

int
p9_signbit(double x)
{
	union { double d; uint64_t u; } v;

	v.d = x;
	return v.u >> 63;
}

int
__builtin_clz(unsigned int x)
{
	int n;

	if(x == 0)
		return 32;
	for(n = 0; (x & 0x80000000U) == 0; n++)
		x <<= 1;
	return n;
}

int
__builtin_ctz(unsigned int x)
{
	int n;

	if(x == 0)
		return 32;
	for(n = 0; (x & 1) == 0; n++)
		x >>= 1;
	return n;
}

int
__builtin_clzll(uint64_t x)
{
	return x >> 32 ? __builtin_clz(x >> 32) : 32 + __builtin_clz(x);
}

int
__builtin_ctzll(uint64_t x)
{
	return (uint32_t)x ? __builtin_ctz(x) : 32 + __builtin_ctz(x >> 32);
}

/* QuickJS uses lrint only for nonnegative values within a 32-bit long. */
long
lrint(double x)
{
	double i, f;

	i = floor(x);
	f = x - i;
	if(f > 0.5 || (f == 0.5 && fmod(i, 2.0) != 0))
		i++;
	return i;
}

double
fmax(double x, double y)
{
	if(isnan(x))
		return y;
	if(isnan(y))
		return x;
	if(x == y)
		return p9_signbit(x) ? y : x;
	return x > y ? x : y;
}

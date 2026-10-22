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

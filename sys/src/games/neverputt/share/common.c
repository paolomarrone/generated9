/*
 *  Copyright (C) 2007  Neverball authors
 *
 *  This  program is  free software;  you can  redistribute  it and/or
 *  modify it  under the  terms of the  GNU General Public  License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program  is distributed in the  hope that it  will be useful,
 *  but  WITHOUT ANY WARRANTY;  without even  the implied  warranty of
 *  MERCHANTABILITY or FITNESS FOR  A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a  copy of the GNU General Public License
 *  along  with this  program;  if  not, write  to  the Free  Software
 *  Foundation,  Inc.,   59  Temple  Place,  Suite   330,  Boston,  MA
 *  02111-1307 USA
 */


#include "port.h"
#include "common.h"
#include "fs.h"

#define MAXSTR 256

/*---------------------------------------------------------------------------*/

int read_line(char **dst, fs_file fin)
{
    char buff[MAXSTR];

    char *line, *new;
    size_t len0, len1;

    line = NULL;

    while (fs_gets(buff, sizeof (buff), fin))
    {
        /* Append to data read so far. */

        if (line)
        {
            new  = concat_string(line, buff, NULL);
            free(line);
            line = new;
        }
        else
        {
            line = strdup(buff);
        }

        /* Strip newline, if any. */

        len0 = strlen(line);
        strip_newline(line);
        len1 = strlen(line);

        if (len1 != len0)
        {
            /* We hit a newline, clean up and break. */
            line = realloc(line, len1 + 1);
            break;
        }
    }

    return (*dst = line) ? 1 : 0;
}

char *strip_newline(char *str)
{
    char *c = str + strlen(str) - 1;

    while (c >= str && (*c == '\n' || *c =='\r'))
        *c-- = '\0';

    return str;
}

char *dupe_string(const char *src)
{
    char *dst = NULL;

    if (src && (dst = malloc(strlen(src) + 1)))
        strcpy(dst, src);

    return dst;
}

char *concat_string(const char *first, ...)
{
    char *full;

    if ((full = strdup(first)))
    {
        const char *part;
        va_list ap;

        va_start(ap, first);

        while ((part = va_arg(ap, const char *)))
        {
            char *new;

            if ((new = realloc(full, strlen(full) + strlen(part) + 1)))
            {
                full = new;
                strcat(full, part);
            }
            else
            {
                free(full);
                full = NULL;
                break;
            }
        }

        va_end(ap);
    }

    return full;
}

time_t make_time_from_utc(Tm *tm)
{
    strcpy(tm->zone, "GMT");
    tm->tzoff = 0;
    return tm2sec(tm);
}

const char *date_to_str(time_t i)
{
    static char str[32];
    Tm *tm = localtime(i);
    snprint(str, sizeof str, "%04d-%02d-%02d %02d:%02d:%02d",
        tm->year+1900, tm->mon+1, tm->mday, tm->hour, tm->min, tm->sec);
    return str;
}

int file_exists(const char *name)
{
    FILE *fp;

    if ((fp = fopen(name, "r")))
    {
        fclose(fp);
        return 1;
    }
    return 0;
}

int file_rename(const char *src, const char *dst)
{
    Dir d;
    const char *a = path_last_sep(src), *b = path_last_sep(dst);

    if(a == NULL || b == NULL || a-src != b-dst || strncmp(src,dst,a-src) != 0) {
        werrstr("rename across directories");
        return -1;
    }
    nulldir(&d);
    d.name = (char*)b+1;
    return dirwstat(src, &d);
}

void file_copy(FILE *fin, FILE *fout)
{
    char   buff[MAXSTR];
    size_t size;

    while ((size = fread(buff, 1, sizeof (buff), fin)) > 0)
        fwrite(buff, 1, size, fout);
}

/*---------------------------------------------------------------------------*/

int path_is_sep(int c)
{
#ifdef _WIN32
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}

int path_is_abs(const char *path)
{
    if (path_is_sep(path[0]))
        return 1;

#ifdef _WIN32
    if (isalpha(path[0]) && path[1] == ':' && path_is_sep(path[2]))
        return 1;
#endif

    return 0;
}

char *path_join(const char *head, const char *tail)
{
    return *head ? concat_string(head, "/", tail, NULL) : strdup(tail);
}

const char *path_last_sep(const char *path)
{
    const char *sep;

    sep = strrchr(path, '/');

#ifdef _WIN32
    if (!sep)
    {
        sep = strrchr(path, '\\');
    }
    else
    {
        const char *tmp;

        if ((tmp = strrchr(path, '\\')) && sep < tmp)
            sep = tmp;
    }
#endif

    return sep;
}

const char *path_next_sep(const char *path)
{
    size_t skip;

#ifdef _WIN32
    skip = strcspn(path, "/\\");
#else
    skip = strcspn(path, "/");
#endif

    return *(path + skip) ? path + skip : NULL;
}

char *path_normalize(char *path)
{
    char *sep = path;

    while ((sep = (char *) path_next_sep(sep)))
        *sep++ = '/';

    return path;
}

const char *base_name_sans(const char *name, const char *suffix)
{
    static char base[MAXSTR];
    size_t blen, slen;

    if (!name)
        return NULL;
    if (!suffix)
        return base_name(name);

    /* Remove the directory part. */

    SAFECPY(base, base_name(name));

    /* Remove the suffix. */

    blen = strlen(base);
    slen = strlen(suffix);

    if (blen >= slen && strcmp(base + blen - slen, suffix) == 0)
        base[blen - slen] = '\0';

    return base;
}

const char *base_name(const char *name)
{
    const char *sep;
    return (name && (sep = path_last_sep(name))) ? sep + 1 : name;
}

const char *dir_name(const char *name)
{
    static char buff[MAXSTR];

    char *sep;

    SAFECPY(buff, name);

    if ((sep = (char *) path_last_sep(buff)))
    {
        if (sep == buff)
            return "/";

        *sep = '\0';

        return buff;
    }

    return ".";
}

/*---------------------------------------------------------------------------*/

int rand_between(int low, int high)
{
    return low + rand() / (RAND_MAX / (high - low + 1) + 1);
}

/*---------------------------------------------------------------------------*/

#ifdef _WIN32

/* MinGW hides this from ANSI C. MinGW-w64 doesn't. */
_CRTIMP int _putenv(const char *envstring);

int set_env_var(const char *name, const char *value)
{
    if (name)
    {
        char str[MAXSTR];

        if (value)
            sprintf(str, "%s=%s", name, value);
        else
            sprintf(str, "%s=", name);

        return (_putenv(str) == 0);
    }
    return 0;
}

#else

int set_env_var(const char *name, const char *value)
{
    if(name == NULL) return 0;
    return putenv((char*)name, (char*)(value ? value : "")) == 0;
}

#endif

/*---------------------------------------------------------------------------*/

/* Native stdio owns the growing string until sclose transfers it. */
char *format_string(const char *fmt, va_list args)
{
    FILE *f = sopenw();
    if (f == NULL) return NULL;
    vfprintf(f, fmt, args);
    return sclose(f);
}

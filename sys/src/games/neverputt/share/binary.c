/*
 * Copyright (C) 2003 Robert Kooima
 *
 * NEVERBALL is  free software; you can redistribute  it and/or modify
 * it under the  terms of the GNU General  Public License as published
 * by the Free  Software Foundation; either version 2  of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT  ANY  WARRANTY;  without   even  the  implied  warranty  of
 * MERCHANTABILITY or  FITNESS FOR A PARTICULAR PURPOSE.   See the GNU
 * General Public License for more details.
 */



#include "port.h"
#include "fs.h"

/*---------------------------------------------------------------------------*/

void put_float(fs_file fout, float f)
{
    union { float value; uint bits; } u;
    int k;
    u.value = f;
    for(k=0; k<4; k++) fs_putc((u.bits >> (8*k)) & 255, fout);
}

void put_index(fs_file fout, int i)
{
    union { int value; uint bits; } u;
    int k;
    u.value = i;
    for(k=0; k<4; k++) fs_putc((u.bits >> (8*k)) & 255, fout);
}

void put_short(fs_file fout, short s)
{
    union { short value; ushort bits; } u;
    int k;
    u.value = s;
    for(k=0; k<2; k++) fs_putc((u.bits >> (8*k)) & 255, fout);
}

void put_array(fs_file fout, const float *v, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++)
        put_float(fout, v[i]);
}

/*---------------------------------------------------------------------------*/

float get_float(fs_file fin)
{
    union { float value; uint bits; } u;
    int k;
    u.bits = 0;
    for(k=0; k<4; k++) u.bits |= (uint)(uchar)fs_getc(fin) << (8*k);
    return u.value;
}

int get_index(fs_file fin)
{
    union { int value; uint bits; } u;
    int k;
    u.bits = 0;
    for(k=0; k<4; k++) u.bits |= (uint)(uchar)fs_getc(fin) << (8*k);
    return u.value;
}

short get_short(fs_file fin)
{
    union { short value; ushort bits; } u;
    int k;
    u.bits = 0;
    for(k=0; k<2; k++) u.bits |= (uint)(uchar)fs_getc(fin) << (8*k);
    return u.value;
}

void get_array(fs_file fin, float *v, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++)
        v[i] = get_float(fin);
}

/*---------------------------------------------------------------------------*/

void put_string(fs_file fout, const char *s)
{
    fs_puts(s, fout);
    fs_putc('\0', fout);
}

void get_string(fs_file fin, char *s, size_t max)
{
    size_t pos = 0;
    int c;

    while ((c = fs_getc(fin)) >= 0)
    {
        if (pos < max)
        {
            s[pos++] = c;

            /* Terminate the string, but keep reading until NUL. */

            if (pos == max)
                s[pos - 1] = 0;
        }

        if (c == 0)
            break;
    }
}

/*---------------------------------------------------------------------------*/

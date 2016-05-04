/*
 * nciddconf.h - This file is part of ncidd.
 *
 * Copyright (c) 2005-2014
 * by John L. Chmielewski <jlc@users.sourceforge.net>
 *
 * ncidd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * ncidd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with ncidd.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>

#ifndef CIDCONF
#define CIDCONF     "/etc/ncid/ncidd.conf"
#endif

#define WORDZERO    0x00
#define WORDSTR     0x10
#define WORDNUM     0x20
#define WORDFONT    0x40
#define WORDFLAG    0x80

#define ON          1
#define OFF         0

#define ERRCMD      "unknown command:"
#define ERRWORD     "unknown word:"
#define ERRARG      "missing argument for word:"
#define ERREQA      "missing '=' after word:"
#define ERREQB      "missing '=' before word:"
#define ERRMISS     "missing:"
#define ERRNUM      "invalid number:"
#define ERRPORT     "invalid port number:"

struct setword
{
    char *word;
    int type;
    char **buf;
    int *value;
    int min;
    int max;
};

struct sendclient
{
    char *word;
    int *value;
};

extern char *name, *cidconf;
extern int errorStatus;
extern struct setword setword[];
extern struct sendclient sendclient[];
extern char *getWord();
extern int findWord(), findSend();
extern void configError();

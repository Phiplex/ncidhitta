/*
 * nciddalias.h - This file is part of ncidd.
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
#include <sys/types.h>
#include <regex.h>

#ifndef CIDALIAS
#define CIDALIAS     "/etc/ncid/ncidd.alias"
#endif

#define ALIASSIZE    3000
#define CIDSIZE      51
#define NMBRNAME     0x01
#define NMBRONLY     0x10
#define NMBRDEP      0x11
#define NAMEONLY     0x20
#define NAMEDEP      0x21
#define LINEONLY     0x40

#define NMBRNAME_TXT "NMBRNAME"
#define NMBRONLY_TXT "NMBRONLY"
#define NMBRDEP_TXT  "NMBRDEP"
#define NAMEONLY_TXT "NAMEONLY"
#define NAMEDEP_TXT  "NAMEDEP"
#define LINEONLY_TXT "LINEONLY"
#define NOALIAS_TXT  "NOALIAS"

#define ERRIF        "missing 'if' before word:"
#define ERRLONG      "word is too long:"

struct alias_info
{
    int type;
    char *type_txt;
    char from[CIDSIZE];
    char to[CIDSIZE];
    char depend[CIDSIZE];
    regex_t preg;
};

struct alias
{
    struct alias_info info;
    struct alias *next;
};

extern char *name, *cidalias;
extern char *findAlias();
extern void builtinAlias(), userAlias(), rmaliases();

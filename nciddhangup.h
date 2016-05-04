/*
 * nciddhangup.h - This file is part of ncidd.
 *
 * Copyright (c) 2005-2015
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

#include <sys/types.h>
#include <regex.h>

#ifndef BLACKLIST
#define BLACKLIST   "/etc/ncid/ncidd.blacklist"
#endif

#ifndef WHITELIST
#define WHITELIST   "/etc/ncid/ncidd.whitelist"
#endif

#ifndef RECORDING
#define RECORDING   "/usr/share/ncid/recordings/NumberDisconnected.rmd"
#endif

#define NOECHO      "ATE0"
#define PICKUP      "ATH1"
#define HANGUP      "ATH0"
#define DATAMODE    "AT+FCLASS=0"

#define FAXANS      "ATA"
#define FAXMODE     "AT+FCLASS=1"
#define FAXDELAY    10   /* seconds */

#define VOICEMODE   "AT+FCLASS=8"
#define SPKRVOL     "AT+VGT=128"

/*
 * Some chipsets take 1 argument, some 2 arguments and some 4 arguments
 * for the voice data formats shown with AT+VSM=?
 * each () indicates an additional argument
 * 4 arguments can always be given, modems seem to ignore extra arguments
 */
//jlc #define XPRESS      "AT+VSM=128,8000" // 128,"8-BIT LINEAR",(7200,8000,11025)
//jlc #define XPRESS      "AT+VSM=0,8000,0,0" // 0,"SIGNED PCM",8,0,(8000),(0),(0)
#define XPRESS      "AT+VSM=130" // 130,"UNSIGNED PCM",8,0,8000,0,0

#define ANSCALL     "AT+VLS=1"
#define VOICEXFR    "AT+VTX"
#define DLEETX      "\x10\x03"

#define GETOK       "AT"
#define HANGUPTRY   6
#define HANGUPDELAY 2   /* seconds */

#define ENTRYSIZE   180 /* maximum size of a list entry */

typedef struct list
{
    char entry[ENTRYSIZE];
    char newname[CIDSIZE];
    struct list *next;
    regex_t preg;
}list_t;

extern char *announce, *blacklist, *whitelist, *audiofmt, *listname;
extern int pickup, regex, wflag;
extern struct list *blkHead, *blkCurrent, *whtHead, *whtCurrent,
                   *listHead, *listCurrent;
extern int doList(), doHangup(), onBlackWhite();
extern void rmEntries();

/*
 * ncidd.h - This file is part of ncidd.
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

#include <stdio.h>
#include "nciddconf.h"
#include "nciddalias.h"
#include "nciddhangup.h"

#include <getopt.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/utsname.h>

#if (defined(__MACH__))
# include "poll.h"
#else
#include <sys/poll.h>
#endif

#include <sys/socket.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <netinet/in.h> /* needed for TiVo Series1 */
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include "version.h"

#if (!defined(O_SYNC))
# define O_SYNC 0
#endif

#define SHOWVER     "%s %s\n%s\n"
#define DESC        "%s - Network CallerID Server\n"
#define NOOPT       "%s: not a option: %s\n"
#define USAGE       "\
Usage:   %s [options]\n\
Options: [-A aliasfile  | --alias <file>]\n\
         [-a announce   | --announce <file>]\n\
         [-B blacklist  | --blacklist <file>]\n\
         [-C configfile | --config <file>]\n\
         [-c calllog    | --cidlog <file>]\n\
         [-D            | --debug]\n\
         [-d datalog    | --datalog <file>]\n\
         [-e lineid     | --lineid <identifier>]\n\
         [-f command    | --audiofmt <command>]\n\
         [-g 0|1        | --gencid <0|1>]\n\
         [-H 0|1|2|3    | --hangup <0|1|2|3>]\n\
         [-h            | --help]\n\
         [-I modemstr   | --initstr <initstring>]\n\
         [-i cidstr     | --initcid <cidstring>]\n\
         [-L logfile    | --logfile <file>]\n\
         [-l lockfile   | --lockfile <file>]\n\
         [-M MaxBytes   | --cidlogmax <MaxBytes>]\n\
         [-N 0|1        | --noserial <0|1>]\n\
         [-n 0|1        | --nomodem <0|1>]\n\
         [-P pidfile    | --pidfile <file>]\n\
         [-p portnumber | --port <portnumber>]\n\
         [-r 0|1        | --regex <0|1>]\n\
         [-S ttyspeed   | --ttyspeed <ttyspeed>]\n\
         [-s datatype   | --send cidlog|cidinfo]\n\
         [-T 0|1        | --sttyclocal <0|1>]\n\
         [-t ttyport    | --ttyport <ttyport>]\n\
         [-V            | --version]\n\
         [-v 1-9        | --verbose <1-9>]\n\
         [-W whitelist  | --whitelist <file>]\n\
         [--osx-launchd]\n\
"

#ifndef TTYPORT
#define TTYPORT     "/dev/modem"
#endif
#ifndef CIDLOG
#define CIDLOG      "/var/log/cidcall.log"
#endif
#ifndef DATALOG
#define DATALOG     "/var/log/ciddata.log"
#endif
#ifndef LOGFILE
#define LOGFILE     "/var/log/ncidd.log"
#endif
#ifndef PIDFILE
#define PIDFILE     "/var/run/ncidd.pid"
#endif
#ifndef LOCKFILE
#define LOCKFILE    "/var/lock/LCK.."
#endif
#ifndef NCIDUPDATE
#define NCIDUPDATE  "/usr/local/bin/cidupdate"
#endif
#ifndef NCIDUTIL
#define NCIDUTIL    "/usr/local/bin/ncidutil"
#endif

#define STDOUT      1
#define CHARWAIT    2       /* deciseconds */
#define READWAIT    100000  /* microseconds */
#define READTRY     10      /* number of times to INITWAIT for a character */
#define MODEMTRY    6
#define TTYSPEED    B115200

/* server messages */
#define ANNOUNCE    "200 Server:"
#define APIANNOUNCE "210 "
#define LOGEND      "250 End of call log"
#define NOLOGSENT   "251 Call log not sent"
#define EMPTYLOG    "252 Call log empty"
#define NOLOG       "253 No Call log"
#define ENDSTARTUP  "300 End of connection startup"
#define BEGIN_DATA  "400 Start of data requiring OK"
#define BEGIN_DATA1 "401 Start of data requiring ACCEPT or REJECT"
#define BEGIN_DATA2 "402 Start of data showing status of handled request"
#define BEGIN_DATA3 "403 Start of data defining permitted requests"
#define END_DATA    "410 End of data"
#define END_RESP    "411 End of response"

#define NOCHANGES   "no changes"
#define DENIED      "denied"
#define DOUPDATE    NCIDUPDATE " -a %s -c %s < /dev/null 2>&1"
#define DOUTIL      NCIDUTIL " %s %s \"%s\" %s %s 2>&1"

/* server warning messages */
#define LOGMSG      "MSG: Call Log too big: (%lu > %lu) bytes %s%s"
#define TOOMSG      "MSG: Too many clients connected (%d) %s%s"

#define INITSTR     "AT Z S0=0 E1 V1 Q0"
#define INITCID1    "AT+VCID=1"
#define INITCID2    "AT#CID=1"
#define HEXLEVEL    5
#define QUERYATI3   "ATI3" 
#define QUERYATGCI  "AT+GCI?" 
#define QUERYATFCLASS "AT+FCLASS=?"
#define QUERYATVSM  "AT+VSM=?"
#define QUERYATFMI  "AT+FMI"
#define QUERYV      "AT&V"

#define PORT        3333
#define MAXCLIENTS  50      /* maximun number of clients that can connect */
#define MAXCONNECT  MAXCLIENTS + 2 /* clients + mainsocket + ttyfd */
#define MAXIPBUF    500     /* max number of characters in address or name */
#define TIMEOUT     200     /* poll() timeout in milliseconds */
#define RINGWAIT    29      /* number of poll() timeouts to wait for RING */
#define SIZE        50      /* array size used for country[] */

#define CRLF        "\r\n"
#define NL          "\n"
#define CR          "\r"

#define WITHSEP     1       /* MM/DD/YYYY HH:MM:SS */
#define NOSEP       2       /* MMDDYYYY HHMM */
#define ONLYTIME    4       /* HH:MM:SS */
#define ONLYYEAR    8       /* YYYY */
#define LOGFILETIME 16      /* HH:MM:SS.ssss */

#define NONAME      "NO NAME"
#define NONMBR      "NO-NUMBER"
#define NOLINE      "NO-LINE"
#define NOTYPE      "-"
#define NOMESG      "NONE"
#define NOCID       "No Caller ID"

#define LOGMAX      110000
#define LOGMAXNUM   100000000

#define BLMSG       "Calls in the blacklist file will be terminated"
#define WLMSG       "Calls in the whitelist file will not be terminated"
#define IGNORE1     "Leading 1 ignored in call & alias/blacklist/whitelist"
#define NOIGNORE1   "Leading 1 in call & alias/blacklist/whitelist not ignored"
#define RELOADED    "Alias, blacklist and whitelist files have been read"

/* Call and message line labels sent to clients */
/* sorted alphabetically here for readability */

#define BLKLINE     "BLK: "
#define CIDLINE     "CID: "
#define ENDLINE     "END: "
#define HUPLINE     "HUP: "
#define MSGLINE     "MSG: "
#define NOTLINE     "NOT: "
#define OUTLINE     "OUT: "
#define PIDLINE     "PID: "
#define WIDLINE     "WID: "

/* Other line labels sent to clients */
/* sorted alphabetically here for readability */

#define ACKLINE     "ACK: "
#define INFOLINE    "INFO: "
#define LOGLINE     "LOG: "
#define OPTLINE     "OPT: "
#define REQLINE     "REQ: "
#define RESPLINE    "RESP: "
#define WRKLINE     "WRK: "

#define MESSAGE     "%s ***DATE*%s*TIME*%s*NAME*%s*NMBR*%s*LINE*%s*MTYPE*%s*"

#define RELOAD      "RELOAD"    /* reload alias, blacklist & whitelist files */
#define UPDATE      "UPDATE"    /* update current CID call log file          */
#define UPDATES     "UPDATES"   /* update all of the CID call log files      */
#define REREAD      "REREAD"    /* reread current CID call log file          */
#define ACPT_LOG    "ACCEPT LOG"
#define ACPT_LOGS   "ACCEPT LOGS"
#define RJCT_LOG    "REJECT LOG"
#define RJCT_LOGS   "REJECT LOGS"
#define BLK_LST     "black"
#define ALIAS_LST   "alias"
#define WHT_LST     "white"
#define INFO_REQ    "INFO"
#define ACK         "ACK"
#define YO          "YO"
#define REQ_ACK     "REQ: ACK"
#define REQ_YO      "REQ: YO"

#define IN          0
#define CID         0
#define OUT         1
#define HUP         2
#define BLK         4
#define PID         8
#define WID         16

#define LINETYPE    25
#define ONELINE     "-"

#define CALLOUT     "CALLOUT"
#define CALLIN      "CALLIN"
#define CALLHUP     "CALLHUP"
#define CALLBLK     "CALLBLK"
#define CALLPID     "CALLPID"
#define CALLWID     "CALLWID"

#define DATE        "*DATE*"
#define TIME        "*TIME*"
#define NMBR        "*NMBR*"
#define MESG        "*MESG*"
#define NAME        "*NAME*"
#define LINE        "*LINE*"
#define HTYPE       "*HTYPE*"
#define SCALL       "*SCALL*"
#define ECALL       "*ECALL*"
#define CTYPE       "*CTYPE*"
#define STAR        "*"

#define CIDINFO     "CIDINFO: "
#define RING        "*RING*"

#define CALL        "CALL: "
#define CALLINFO    "CALLINFO: "
#define CANCEL      "CANCEL"
#define BYE         "BYE"

#define O            "OUT-OF-AREA"
#define A            "ANONYMOUS"
#define P            "PRIVATE"

#define CIDDATE      0x01
#define CIDTIME      0x02
#define CIDNMBR      0x04
#define CIDNAME      0x08
#define CIDMESG      0x10
#define IGNHIGH      0x0F
#define CIDALL3      (CIDDATE | CIDTIME | CIDNMBR)
#define CIDALT3      (CIDDATE | CIDTIME | CIDNAME)
#define CIDALL4      (CIDDATE | CIDTIME | CIDNMBR | CIDNAME)
#define CIDNODT      (CIDNMBR | CIDNAME)

#define MAXLEVEL     9
enum
{
    LEVEL1 = 1,
    LEVEL2,
    LEVEL3,
    LEVEL4,
    LEVEL5,
    LEVEL6,
    LEVEL7,
    LEVEL8,
    LEVEL9
};

extern char *ttyport, *TTYspeed;
extern char *initstr, *initcid;
extern char *cidlog, *datalog, *lineid, *lockfile, *pidfile, *fnptr;
extern int setcid, port, clocal, ttyspeed, ttyfd, hangup;
extern int sendlog, sendinfo, ignore1, cidnoname;
extern int nomodem, noserial, gencid, verbose;
extern long unsigned int cidlogmax;
extern void logMsg();
extern int errorExit(), CheckForLockfile(), openTTY(), doTTY(), initModem();
extern struct termios rtty, ntty;

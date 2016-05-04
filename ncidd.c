/*
 * ncidd.c - This file is part of ncidd.
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

#include "ncidd.h"

/* globals */
char *cidlog   = CIDLOG;
char *datalog  = DATALOG;
char *ttyport  = TTYPORT;
char *initstr  = INITSTR;
char *initcid  = INITCID1;
char *logfile  = LOGFILE;
char *pidfile, *fnptr;
char *lineid   = ONELINE;
char *lockfile, *name;
char *TTYspeed;
int ttyspeed   = TTYSPEED;
int port = PORT;
int debug, conferr, setcid, locked, sendlog, sendinfo, calltype, cidnoname;
int ttyfd, mainsock, pollpos, pollevents, update_call_log = 0;
int ring, ringwait, lastring, clocal, nomodem, noserial, gencid = 1;
int cidsent, verbose = 1, hangup, ignore1, OSXlaunchd, fixModembuf;
long unsigned int cidlogmax = LOGMAX;
pid_t pid;

char tmpIPaddr[MAXIPBUF];
char tmpHostName[MAXIPBUF];
char infoline[CIDSIZE] = ONELINE;
char modembuf[BUFSIZ];

struct pollfd polld[MAXCONNECT];
struct termios otty, rtty, ntty;
FILE *logptr;

struct ipinfo {
    char addr[MAXIPBUF];
    char name[MAXIPBUF];
} IPinfo[MAXCONNECT];

/* ack[pos] is for same client/gateway as in polld[pos] */
int ack[MAXCONNECT]; /* only for clients */

struct cid
{
    int status;
    char ciddate[CIDSIZE];
    char cidtime[CIDSIZE];
    char cidnmbr[CIDSIZE];
    char cidname[CIDSIZE];
    char cidmesg[CIDSIZE];
    char cidline[CIDSIZE];
} cid = {0, "", "", "", "", NOMESG, ONELINE};

struct mesg
{
    char date[CIDSIZE];
    char time[CIDSIZE];
    char nmbr[CIDSIZE];
    char name[CIDSIZE];
    char line[CIDSIZE];
    char type[CIDSIZE];
} mesg;

struct end
{
    char htype[CIDSIZE];
    char ctype[CIDSIZE];
    char  date[CIDSIZE];
    char  time[CIDSIZE];
    char scall[CIDSIZE];
    char ecall[CIDSIZE];
    char  line[CIDSIZE];
    char  nmbr[CIDSIZE];
    char  name[CIDSIZE];
} endcall;

/* All line labels of interest to a client, new types must be added */
/* sorted alphabetically here for readability */
char *lineTags[] =
{
    BLKLINE,
    CIDLINE,
    ENDLINE,
    HUPLINE,
    MSGLINE,
    NOTLINE,
    OUTLINE,
    PIDLINE,
    WIDLINE,
    NULL
};

/* server lines sent to clients */
/* sorted alphabetically here for readability */
char *serverTags[] =
{
    "BLK:",
    "CID:",
    "CIDINFO:",
    "END:",
    "HUP:",
    "MSG:",
    "NOT:",
    "OUT:",
    "PID:",
    "WID:",
    NULL
};

/* List of popular country codes as returned by QUERYATGCI ("AT+GCI?") 
 * http://doc.slitaz.org/en:handbook:pstn:countries
 * http://www.cisco.com/c/en/us/td/docs/routers/access/modem/AT/wic/command/reference/atwic/atwic3.html
 *
 * Country codes have their origin based on fascimile standards. 
 * The definitive guide for all country codes is maintained by the 
 * International Telegraph Union (ITU), a specialized agency of the 
 * United Nations. Not all modem manufacturers use a consistent list
 * of country codes, and not all modem manufactures even use the
 * complete list. The ITU's complete list is available here:
 *     http://www.itu.int/dms_pub/itu-t/opb/sp/T-SP-T.35-2012-OAS-PDF-E.pdf
 */
char *modemGCI[] =
{
     "00 Japan",
     "09 Australia",
     "0A Austria",
     "0F Belgium",
     "16 Brazil",
     "1B Bulgaria",
     "20 Canada",
     "26 China",
     "2D Cyprus",
     "2E Czech Republic",
     "31 Denmark",
     "3C Finland",
     "3D France",
     "42 Germany",
     "46 Greece",
     "50 Hong Kong",
     "51 Hungary",
     "52 Iceland",
     "53 India",
     "57 Ireland",
     "58 Israel",
     "59 Italy",
     "61 Korea",
     "68 Liechtenstein",
     "69 Luxembourg",
     "6C Malaysia",
     "73 Mexico",
     "7B Netherlands",
     "7E New Zealand",
     "82 Norway",
     "89 Philippines",
     "8A Poland",
     "8B Portugal",
     "8C Singapore",
     "9F South Africa",
     "A0 Spain",
     "A5 Sweden",
     "A6 Switzerland",
     "A9 Thailand",
     "AE Turkey",
     "B4 United Kingdom",
     "B5 United States",
     "B8 Russia",
     "F6 TBR-21(Default)",
     "F7 Lithuania",
     "F8 Latvia",
     "F9 Estonia",
     "FB Slovakia",
     "FC Slovenia",
     "FE Taiwan",
     NULL
};

char *strdate();
#ifndef __CYGWIN__
    extern char *strsignal();
#endif

void exit(), finish(), free(), reload(), ignore(), doPoll(), formatCID(),
     writeClients(), writeLog(), sendLog(), sendInfo(), logMsg(), cleanup(),
     update_cidcall_log(), getINFO(), getField(), hexdump(), checkModem(),
     normalExit(), showConnected();

int getOptions(), doConf(), errorExit(), doAlias(), doTTY(), CheckForLockfile(),
    addPoll(), tcpOpen(), doModem(), initModem(), gettimeofday(), doPID(),
    tcpAccept(), openTTY();

char *trimWhitespace();

int main(int argc, char *argv[])
{
    int events, argind, i, fd, errnum, ret;
    char *ptr;
    struct stat statbuf;
    struct utsname utsbuf;
    char msgbuf[BUFSIZ];

    signal(SIGHUP,  finish);
    signal(SIGINT,  finish);
    signal(SIGQUIT, finish);
    signal(SIGABRT, finish);
    signal(SIGSEGV, finish);
    signal(SIGALRM, finish);
    signal(SIGTERM, finish);
    signal(SIGUSR2,  showConnected);

    signal(SIGPIPE, ignore);
    
    /* global containing name of program */
    name = strrchr(argv[0], (int) '/');
    name = name ? name + 1 : argv[0];

    /* process options from the command line */
    argind = getOptions(argc, argv);

    /* should not be any arguments */
    if (argc - argind != 0)
    {
        fprintf(stderr, NOOPT, name, argv[argind]);
        fprintf(stderr, USAGE, name);
        exit(0);
    }

    /* open or create logfile */
    logptr = fopen(logfile, "a+");
    errnum = errno;

    sprintf(msgbuf, "Started: %s\nServer: %s %s\n%s\n",strdate(WITHSEP),
            name, VERSION, API);
    logMsg(LEVEL1, msgbuf);

    /* uname system call information */
    if ((ret = uname(&utsbuf) == -1))
    {
        sprintf(msgbuf, "uname system call failed with errno = %d\n", errno);
    }
    else
    {
        sprintf(msgbuf, "Sysname: %s\nMachine: %s\nRelease: %s\nVersion: %s\n",
             utsbuf.sysname, utsbuf.machine, utsbuf.release, utsbuf.version);
    }
    logMsg(LEVEL1, msgbuf);

    /* log command line and any options on separate lines */
    sprintf(msgbuf, "Command line: %s", argv[0]);
    for (i = 1; i < argc; i++)
    {
        if (*argv[i] == '-')
            strcat(strcat(msgbuf, "\n              "), argv[i]);
        else strcat(strcat(msgbuf, " "), argv[i]);
    }
    strcat(msgbuf, NL);
    logMsg(LEVEL1, msgbuf);

    /* check status of logfile */
    if (logptr)
    {
        /* logfile opened */
        sprintf(msgbuf, "Logfile: %s\n", logfile);
        logMsg(LEVEL1, msgbuf);
    }
    else
    {
        /* logfile open failed */
        sprintf(msgbuf, "%s: %s\n", logfile, strerror(errnum));
        logMsg(LEVEL1, msgbuf);
    }

    /*
     * read config file, if present, exit on any errors
     * do not override any options set on the command line
     */
    if (doConf()) errorExit(-104, 0, 0);

    sprintf(msgbuf, "Verbose level: %d\n", verbose);
    logMsg(LEVEL1, msgbuf);

    if (nomodem && hangup)
    {
    sprintf(msgbuf,
        "The nomodem option cannot be used with the hangup option.");
        errorExit(-110, "Fatal", msgbuf);
    }

    if (cidnoname)
    {
        sprintf(msgbuf,
            "Configured to receive a CID without a NAME\n");
        logMsg(LEVEL1, msgbuf);
    }

    /*
     * indicate what is configured to send to the clients
     */
    for (i = 0; sendclient[i].word; i++)
        if (*sendclient[i].value)
        {
            sprintf(msgbuf, "Configured to send '%s' to clients.\n",
                sendclient[i].word);
            logMsg(LEVEL1, msgbuf);
        }

    /*
     * indicate location of helper scripts
     */
    sprintf(msgbuf, "Helper tools:\n    %s\n    %s\n", NCIDUPDATE, NCIDUTIL);
    logMsg(LEVEL1, msgbuf);

    if (regex)
    {
        sprintf(msgbuf, "Using regular expressions for aliases\n");
        logMsg(LEVEL1, msgbuf);
    }
    else
    {
        sprintf(msgbuf, "Using simple expressions for aliases\n");
        logMsg(LEVEL1, msgbuf);
    }
    if (hangup)
    {
        if (regex)
        {
            sprintf(msgbuf, "Using regular expressions for blacklist/whitelist entries\n");
        }
        else sprintf(msgbuf, "Using simple expressions for blacklist/whitelist entries\n");
        logMsg(LEVEL1, msgbuf);
    }

    sprintf(msgbuf,
      "\nBegin: Loading alias, blacklist, and whitelist files [%s]\n", strdate(ONLYTIME));
    logMsg(LEVEL1, msgbuf);

    /*
     * read alias file, if present, exit on any errors
     */
    if (doAlias()) errorExit(-109, 0, 0);

    /* read blacklist and whitelist files, exit on any errors */

    if (doList(blacklist, &blkHead, &blkCurrent)) errorExit(-114, 0, 0);
    if (hangup)
    {
        sprintf(msgbuf, "%s\n", BLMSG);
        logMsg(LEVEL1, msgbuf);
    }

    if (doList(whitelist, &whtHead, &whtCurrent)) errorExit(-114, 0, 0);
    if (hangup)
    {
        sprintf(msgbuf, "%s\n", WLMSG);
        logMsg(LEVEL1, msgbuf);
    }
    sprintf(msgbuf, "%s\n", ignore1 ? IGNORE1 : NOIGNORE1);
    logMsg(LEVEL1, msgbuf);

    sprintf(msgbuf,
      "End: Loaded alias, blacklist, and whitelist files [%s]\n\n", strdate(ONLYTIME));
    logMsg(LEVEL1, msgbuf);
    if (verbose == LEVEL8) normalExit();
    
    if (stat(cidlog, &statbuf) == 0)
    {
      sprintf(msgbuf, "CID logfile: %s\nCID logfile maximum size: %lu bytes\n",
        cidlog, cidlogmax);
      logMsg(LEVEL1, msgbuf);
    }
    else
    {
        /* Create the call log file if not present */
        if ((fd = open(cidlog, O_WRONLY | O_APPEND | O_CREAT,
             S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0)
        {
            sprintf(msgbuf, "%s: %s\n", cidlog, strerror(errno));
            logMsg(LEVEL1, msgbuf);
        }
        else
        {
          close(fd);
          sprintf(msgbuf,
            "Created CID logfile: %s\nCID logfile maximum size: %lu bytes\n",
            cidlog, cidlogmax);
          logMsg(LEVEL1, msgbuf);
        }
    }

    if (stat(datalog, &statbuf) == 0)
    {
        sprintf(msgbuf, "Data logfile: %s\n", datalog);
        logMsg(LEVEL1, msgbuf);
    }
    else
    {
        sprintf(msgbuf, "Data logfile not present: %s\n", datalog);
        logMsg(LEVEL1, msgbuf);
    }

    /*
     * lineid could have been changed in either ncidd.conf or ncidd.alias
     * this sets cid.cidline  and infoline to lineid after any changes to it
     */
    strncpy(cid.cidline, lineid, CIDSIZE - 1);
    strncpy(infoline, lineid, CIDSIZE - 1);

    sprintf(msgbuf, "Maximum number of clients/gateways: %d\n",
            noserial ? MAXCLIENTS + 1 : MAXCLIENTS);
    logMsg(LEVEL1, msgbuf);

    sprintf(msgbuf, "Telephone Line Identifier: %s\n", lineid);
    logMsg(LEVEL1, msgbuf);

    /*
     * noserial = 1: serial port not used
     * noserial = 0: serial port used for Caller ID
     */

    if (!noserial || hangup)
    {
        /*
        * If the tty port speed was set, map it to the correct integer.
        */
        if (TTYspeed)
        {
            if (!strcmp(TTYspeed, "115200")) ttyspeed = B115200;
            else if (!strcmp(TTYspeed, "38400")) ttyspeed = B38400;
            else if (!strcmp(TTYspeed, "19200")) ttyspeed = B19200;
            else if (!strcmp(TTYspeed, "9600")) ttyspeed = B9600;
            else if (!strcmp(TTYspeed, "4800")) ttyspeed = B4800;
            else errorExit(-108, "Invalid TTY port speed set in config file",
                TTYspeed);
        }

        /* Create lock file name from TTY port device name */
        if (!lockfile)
        {
            if ((ptr = strrchr(ttyport, '/'))) ptr++;
            else ptr = ttyport;

            if ((lockfile = (char *) malloc(strlen(LOCKFILE)
                + strlen(ptr) + 1)))
                strcat(strcpy(lockfile, LOCKFILE), ptr);
            else errorExit(-1, name, 0);
        }

        /* check TTY port lock file */
        if (CheckForLockfile())
            errorExit(-102, "Exiting - TTY lockfile exists", lockfile);

        /* Open tty port; exit program if it fails */
        if (openTTY() < 0) errorExit(-1, ttyport, 0);

        switch(ttyspeed)
        {
            case B4800:
                TTYspeed = "4800";
                break;
            case B9600:
                TTYspeed = "9600";
                break;
            case B19200:
                TTYspeed = "19200";
                break;
            case B38400:
                TTYspeed = "38400";
                break;
            case B115200:
                TTYspeed = "115200";
                break;
        }

        sprintf(msgbuf, "TTY port opened: %s\n", ttyport);
        logMsg(LEVEL1, msgbuf);
        sprintf(msgbuf, "TTY port speed: %s\n", TTYspeed);
        logMsg(LEVEL1, msgbuf);
        sprintf(msgbuf, "TTY lock file: %s\n", lockfile);
        logMsg(LEVEL1, msgbuf);
        sprintf(msgbuf, "TTY port control signals %s\n",
            clocal ? "disabled" : "enabled");
        logMsg(LEVEL1, msgbuf);

        if (noserial)
        {
            sprintf(msgbuf,
                "CallerID from gateways\n");
            logMsg(LEVEL1, msgbuf);
        }
        else if (nomodem)
        {
            sprintf(msgbuf,
                "CallerID from serial device and optional gateways\n");
            logMsg(LEVEL1, msgbuf);
        }
        else
        {
            sprintf(msgbuf, "CallerID from AT Modem and optional gateways\n");
            logMsg(LEVEL1, msgbuf);

            if (gencid)
            {
            sprintf(msgbuf, "Handles modem calls without Caller ID\n");
            logMsg(LEVEL1, msgbuf);
            }
            else
            {
            sprintf(msgbuf, "Does not handle modem calls without Caller ID\n");
            logMsg(LEVEL1, msgbuf);
            }
        }

        if (!noserial)
        {
            /* Save tty port settings */
            if (tcgetattr(ttyfd, &otty) < 0) return -1;

            /* initialize tty port */
            if (doTTY() < 0) errorExit(-1, ttyport, 0);
        }
    }
    else if (noserial)
    {
        sprintf(msgbuf, "CallerID from Gateway\n");
        logMsg(LEVEL1, msgbuf);
    }

    if (hangup)
    {
        if (noserial)
        {
            ret = initModem(initstr, READTRY);
            logMsg(LEVEL1, "Modem initialized\n");
            checkModem();
            (void) close(ttyfd);
            ttyfd = 0;
            sprintf(msgbuf, "Modem only used to terminate calls\n");
        }
        else sprintf(msgbuf, "Modem used for CallerID and to terminate calls\n");
        logMsg(LEVEL1, msgbuf);
    }

    sprintf(msgbuf, "Network Port: %d\n", port);
    logMsg(LEVEL1, msgbuf);

    if (debug || OSXlaunchd)
    {
        if (debug) sprintf(msgbuf, "Debug Mode\n");
        else sprintf(msgbuf, "OSX Launchd Mode\n"); 
        logMsg(LEVEL1, msgbuf);
    }
    else
    {
        /* fork and exit parent */
        if(fork() != 0) return 0;

        /* close stdin, and  and make fd 0 unavailable */
        close(0);
        if (open("/dev/null",  O_WRONLY | O_SYNC) < 0)
        {
            errorExit(-1, "/dev/null", 0);
        }

        /* become session leader */
        setsid();
    }

    /* reload files on SIGHUP */
    signal(SIGHUP, reload);

    /* replace CID call log file on SIGUSR1 */
    signal (SIGUSR1, update_cidcall_log);

    /*
     * Create a pid file
     */
    if (doPID())
    {
        sprintf(msgbuf,"%s already exists", pidfile);
        errorExit(-110, "Fatal", msgbuf);
    }

    if (!noserial) {
            pollpos = addPoll(ttyfd);
            sprintf(msgbuf,"%s is fd %d\n",
                    nomodem ? "Caller ID Device" : "Modem", ttyfd);
            logMsg(LEVEL3, msgbuf);
        }

    /* initialize server socket */
    if ((mainsock = tcpOpen()) < 0) errorExit(-1, "socket", 0);

    ret = addPoll(mainsock);
    sprintf(msgbuf,"NCID connection socket is sd %d pos %d\n", mainsock, ret);
    logMsg(LEVEL3, msgbuf);

    /* Read and display data */
    while (1)
    {
        switch (events = poll(polld, MAXCONNECT, TIMEOUT))
        {
            case -1:    /* error */
                if (errno != EINTR) /* No error for SIGHUP */
                    errorExit(-1, "poll", 0);
                break;
            case 0:        /* time out, without an event */
                if (ring > 0)
                {
                    /* ringing detected  */
                    if (ringwait < RINGWAIT) ++ringwait;
                    else
                    {
                            sprintf(msgbuf, "lastring: %d ring: %d time: %s\n",
                                lastring, ring, strdate(ONLYTIME));
                            logMsg(LEVEL5, msgbuf);
                        if (lastring == ring)
                        {
                            /* ringing stopped */
                            ring = lastring = ringwait = cidsent = 0;
                            sendInfo();
                        }
                        else
                        {
                            /* ringing */
                            ringwait = 0;
                            lastring = ring;
                        }
                    }
                }
                if (update_call_log)
                {
                    update_call_log = 0;
                    sprintf (msgbuf, "%s.new", cidlog);
                    if (access (msgbuf, F_OK) == 0)
                    {
                        rename (msgbuf, cidlog);
                        sprintf (msgbuf,
                        "Replaced %s with %s.new: %s\n", cidlog, cidlog, strdate(ONLYTIME));
                        logMsg(LEVEL1, msgbuf);
                    }
                }
                /* if no serial port, skip TTY code */
                if (!noserial)
                {
                    /* TTY port lockfile */
                    if (CheckForLockfile())
                    {
                        if (!locked)
                        {
                            /* lockfile just found */

                            /* save TTY events */
                            pollevents = polld[pollpos].events;
                            /* remove TTY poll events */
                            polld[pollpos].events = polld[pollpos].revents = 0;
                            polld[pollpos].fd = 0;
                            close(ttyfd);
                            ttyfd = 0;
                            sprintf(msgbuf, "TTY in use: releasing modem %s\n",
                                strdate(WITHSEP));
                            logMsg(LEVEL1, msgbuf);
                            locked = 1;
                            ringwait = 0;
                        }
                    }
                    else if (locked)
                    {
                        /* lockfile just went away */
                        sprintf(msgbuf, "TTY free: using modem again %s\n",
                            strdate(WITHSEP));
                        logMsg(LEVEL1, msgbuf);
                        if (openTTY() < 0) errorExit(-1, ttyport, 0);
                        if (doTTY() < 0)
                        {
                            sprintf(msgbuf,
                                "%sCannot init TTY, Terminated %s",
                                MSGLINE, strdate(WITHSEP));
                            writeClients(msgbuf);
                            tcsetattr(ttyfd, TCSANOW, &otty);
                            errorExit(-111, "Fatal", "Cannot init TTY");
                        }
                        locked = 0;
                        /* restore tty poll events */
                        polld[pollpos].fd = ttyfd;
                        polld[pollpos].events = pollevents;
                    }
                }
                break;
            default:    /* 1 or more events */
                doPoll(events);
                break;
        }
    }
}

int getOptions(int argc, char *argv[])
{
    int c, num;
    int option_index = 0;
    static struct option long_options[] = {
        {"alias", 1, 0, 'A'},
        {"announce", 1, 0, 'a'},
        {"audiofmt", 1, 0, 'f'},
        {"blacklist", 1, 0, 'B'},
        {"config", 1, 0, 'C'},
        {"cidlog", 1, 0, 'c'},
        {"cidlogmax", 1, 0, 'M'},
        {"datalog", 1, 0, 'd'},
        {"debug", 0, 0, 'D'},
        {"gencid", 1, 0, 'g'},
        {"help", 0, 0, 'h'},
        {"hangup", 1, 0, 'H'},
        {"initcid", 1, 0, 'i'},
        {"initstr", 1, 0, 'I'},
        {"lineid", 1, 0, 'e'},
        {"lockfile", 1, 0, 'l'},
        {"logfile", 1, 0, 'L'},
        {"nomodem", 1, 0, 'n'},
        {"noserial", 1, 0, 'N'},
        {"pidfile", 1, 0, 'P'},
        {"port", 1, 0, 'p'},
        {"regex", 1, 0, 'r'},
        {"send", 1, 0, 's'},
        {"ttyspeed", 1, 0, 'S'},
        {"ttyclocal", 1, 0, 'T'},
        {"ttyport", 1, 0, 't'},
        {"verbose", 1, 0, 'v'},
        {"version", 0, 0, 'V'},
        {"audiofmt", 0, 0, 'f'},
        {"whitelist", 1, 0, 'W'},
        {"osx-launchd", 0, 0, '0'},
        {0, 0, 0, 0}
    };

    while ((c = getopt_long (argc, argv, "a:c:d:e:f:g:hi:l:n:p:r:s:t:v:A:B:C:DH:I:L:M:N:P:S:T:VW:",
        long_options, &option_index)) != -1)
    {
        switch (c)
        {
            case '0':
                ++OSXlaunchd;
                break;
            case 'A':
                if (!(cidalias = strdup(optarg))) errorExit(-1, name, 0);
                if ((num = findWord("cidalias")) >= 0) setword[num].type = 0;
                break;
            case 'B':
                if (!(blacklist = strdup(optarg))) errorExit(-1, name, 0);
                if ((num = findWord("blacklist")) >= 0) setword[num].type = 0;
                break;
            case 'C':
                if (!(cidconf = strdup(optarg))) errorExit(-1, name, 0);
                break;
            case 'D':
                ++debug;
                break;
            case 'H':
                hangup = atoi(optarg);
                if (strlen(optarg) != 1 ||
                    (!(hangup == 0 && *optarg == '0') && hangup > 3))
                    errorExit(-107, "Invalid number", optarg);
                if ((num = findWord("hangup")) >= 0) setword[num].type = 0;
                break;
            case 'I':
                if (!(initstr = strdup(optarg))) errorExit(-1, name, 0);
                if ((num = findWord("initstr")) >= 0) setword[num].type = 0;
                break;
            case 'L':
                if (!(logfile = strdup(optarg))) errorExit(-1, name, 0);
                break;
            case 'M':
                cidlogmax = atoi(optarg);
                if ((num = findWord("cidlogmax")) >= 0)
                {
                    if (cidlogmax < (unsigned) setword[num].min ||
                        cidlogmax > (unsigned) setword[num].max)
                        errorExit(-107, "Invalid number", optarg);
                    setword[num].type = 0;
                }
                break;
            case 'N':
                noserial = atoi(optarg);
                if (strlen(optarg) != 1 ||
                    (!(noserial == 0 && *optarg == '0') && noserial != 1))
                    errorExit(-107, "Invalid number", optarg);
                if ((num = findWord("noserial")) >= 0) setword[num].type = 0;
                break;
            case 'P':
                if (!(pidfile = strdup(optarg))) errorExit(-1, name, 0);
                if ((num = findWord("pidfile")) >= 0) setword[num].type = 0;
                break;
            case 'S':
                if (!(TTYspeed = strdup(optarg))) errorExit(-1, name, 0);
                if (!strcmp(TTYspeed, "115200")) ttyspeed = B115200;
                else if (!strcmp(TTYspeed, "38400")) ttyspeed = B38400;
                else if (!strcmp(TTYspeed, "19200")) ttyspeed = B19200;
                else if (!strcmp(TTYspeed, "9600")) ttyspeed = B9600;
                else if (!strcmp(TTYspeed, "4800")) ttyspeed = B4800;
                else errorExit(-108, "Invalid TTY port speed", TTYspeed);
                if ((num = findWord("ttyspeed")) >= 0) setword[num].type = 0;
                break;
            case 'T':
                clocal = atoi(optarg);
                if (strlen(optarg) != 1 ||
                    (!(clocal == 0 && *optarg == '0') && clocal != 1))
                    errorExit(-107, "Invalid number", optarg);
                if ((num = findWord("ttyclocal")) >= 0) setword[num].type = 0;
                break;
            case 'V': /* version */
                fprintf(stderr, SHOWVER, name, VERSION, API);
                exit(0);
            case 'W':
                if (!(whitelist = strdup(optarg))) errorExit(-1, name, 0);
                if ((num = findWord("whitelist")) >= 0) setword[num].type = 0;
                break;
            case 'a':
                if (!(announce = strdup(optarg))) errorExit(-1, name, 0);
                if ((num = findWord("announce")) >= 0) setword[num].type = 0;
                break;
            case 'c':
                if (!(cidlog = strdup(optarg))) errorExit(-1, name, 0);
                if ((num = findWord("cidlog")) >= 0) setword[num].type = 0;
                break;
            case 'd':
                if (!(datalog = strdup(optarg))) errorExit(-1, name, 0);
                if ((num = findWord("datalog")) >= 0) setword[num].type = 0;
                break;
            case 'e':
                if (!(lineid = strdup(optarg))) errorExit(-1, name, 0);
                if (strlen(lineid) > CIDSIZE -1)
                    errorExit(-113, "string too long", optarg);
                if ((num = findWord("lineid")) >= 0) setword[num].type = 0;
                break;
            case 'f':
                if (!(audiofmt = strdup(optarg))) errorExit(-1, name, 0);
                if ((num = findWord("audiofmt")) >= 0) setword[num].type = 0;
                break;
            case 'g':
                gencid = atoi(optarg);
                if (strlen(optarg) != 1 ||
                    (!(gencid == 0 && *optarg == '0') && gencid != 1))
                    errorExit(-107, "Invalid number", optarg);
                if ((num = findWord("gencid")) >= 0) setword[num].type = 0;
                break;
            case 'h': /* help message */
                fprintf(stderr, DESC, name);
                fprintf(stderr, USAGE, name);
                exit(0);
            case 'i':
                if (!(initcid = strdup(optarg))) errorExit(-1, name, 0);
                ++setcid;
                if ((num = findWord("initcid")) >= 0) setword[num].type = 0;
                break;
            case 'l':
                if (!(lockfile = strdup(optarg))) errorExit(-1, name, 0);
                if ((num = findWord("lockfile")) >= 0) setword[num].type = 0;
                break;
            case 'n':
                nomodem = atoi(optarg);
                if (strlen(optarg) != 1 ||
                    (!(nomodem == 0 && *optarg == '0') && nomodem != 1))
                    errorExit(-107, "Invalid number", optarg);
                if ((num = findWord("nomodem")) >= 0) setword[num].type = 0;
                break;
            case 'p':
                if((port = atoi(optarg)) == 0)
                    errorExit(-101, "Invalid port number", optarg);
                if ((num = findWord("port")) >= 0) setword[num].type = 0;
                break;
            case 'r':
                regex = atoi(optarg);
                if (strlen(optarg) != 1 ||
                    (!(regex == 0 && *optarg == '0') && regex != 1))
                    errorExit(-107, "Invalid number", optarg);
                if ((num = findWord("regex")) >= 0) setword[num].type = 0;
                break;
            case 's':
                if ((num = findSend(optarg)) < 0)
                    errorExit(-106, "Invalid send data type", optarg);
                ++(*sendclient[num].value);
                break;
            case 't':
                if (!(ttyport = strdup(optarg))) errorExit(-1, name, 0);
                if ((num = findWord("ttyport")) >= 0) setword[num].type = 0;
                break;
            case 'v':
                verbose = atoi(optarg);
                /* valid range: 1-9 */
                if (strlen(optarg) != 1 || (verbose == 0))
                {
                    verbose = 1;
                    errorExit(-107, "Invalid number", optarg);
                }
                if ((num = findWord("verbose")) >= 0) setword[num].type = 0;
                break;
            case '?': /* bad option */
                fprintf(stderr, USAGE, name);
                errorExit(-100, 0, 0);
        }
    }
    return optind;
}

/*
 * Open tty port; tries to make sure the open does
 * not hang if port in use, or not restored after use
 */

int openTTY()
{
    if ((ttyfd = open(ttyport, O_RDWR | O_NOCTTY | O_NDELAY)) < 0)
    {
        ttyfd = 0; 
        return -1;
    }
    if (fcntl(ttyfd, F_SETFL, fcntl(ttyfd, F_GETFL, 0) & ~O_NDELAY) < 0)
        return -1;

    return 0;
}

int doTTY()
{
    char msgbuf[BUFSIZ];

    /* Setup tty port in raw mode */
    if (tcgetattr(ttyfd, &rtty) < 0) return -1;
    rtty.c_lflag     &= ~(ICANON | ECHO | ECHOE | ISIG);
    rtty.c_oflag     &= ~OPOST;
    rtty.c_iflag = (IGNBRK | IGNPAR);
    rtty.c_cflag = (ttyspeed | CS8 | CREAD | HUPCL | CRTSCTS);
    if (clocal) rtty.c_cflag |= CLOCAL;
    rtty.c_cc[VEOL] = '\r';
    rtty.c_cc[VMIN]  = 0;
    rtty.c_cc[VTIME] = CHARWAIT;
    if (tcflush(ttyfd, TCIOFLUSH) < 0) return -1;
    if (tcsetattr(ttyfd, TCSANOW, &rtty) < 0) return -1;

    if (!nomodem)
    {
        /* initialize modem for CID */
        if (doModem() < 0) errorExit(-1, ttyport, 0);
    }

    /* take tty port out of raw mode */
    if (tcgetattr(ttyfd, &ntty) < 0) return -1;
    ntty.c_lflag = (ICANON);
    if (tcsetattr(ttyfd, TCSANOW, &ntty) < 0) return -1;

    if (nomodem)
    {
        sprintf(msgbuf, "CallerID TTY port initialized.\n");
        logMsg(LEVEL1, msgbuf);
    }

    return 0;
}

/*
 * Check some modem capibilities and hangup options
 */
void checkModem()
{
    int ret;
    struct stat statbuf;
    char msgbuf[BUFSIZ], countryGCI[SIZE], *ptr, *eptr, **gci;

    /* Query modem software) */
    ret = initModem(QUERYATI3, READTRY);
    if (!ret)
    {
        ptr = modembuf + 7;
        while (*ptr == '\r' || *ptr == '\n') ++ptr;
        eptr = strchr(ptr, (int) '\r');
        *eptr = '\0';
        sprintf(msgbuf, "Modem Identifier: %s\n", ptr);
        logMsg(LEVEL1, msgbuf);
        *eptr = '\r';
    }
    else logMsg(LEVEL1, "Cannot determine Modem Identifier\n");

    /* Query modem country setting (Unites states = B5) */
    ret = initModem(QUERYATGCI, READTRY);
    if ((ptr = strstr(modembuf, "GCI:")))
    {
        if (strchr(ptr, (int) ' ')) ptr += 5;
        else ptr +=4;
        strcat(strncpy(countryGCI, ptr, 2), "");
        for(gci = modemGCI; gci; ++gci)
        {
           if (!strncmp(countryGCI, *gci, 2))
           {
              strncpy(countryGCI, *gci, SIZE - 2);
              break;
           }
        }
        sprintf(msgbuf, "Modem country code: %s\n", countryGCI);
        logMsg(LEVEL1, msgbuf);
    }
    else logMsg(LEVEL1, "Modem country code cannot be determined\n");

    /* Query active profile */
    fixModembuf = 1;
    ret = initModem(QUERYV, READTRY);
    if (!ret)
    {
        ptr = strchr(modembuf, (int) '\n') + 1;
        if ((eptr = strstr(modembuf, "OK")))
        {
            *(eptr - 2) = '\0';
            if (strstr(modembuf, "ACTIVE PROFILE"))
               sprintf(msgbuf, "Modem %s", ptr);
            else 
               sprintf(msgbuf, "Modem Active Profile settings: %s", ptr);
            logMsg(LEVEL1, msgbuf);
        }
        else logMsg(LEVEL1, "Cannot retrieve modem Active Profile\n");
    }
    else logMsg(LEVEL1, "Modem profile query failed\n");

    /* Query modem modes supported */
    ret = initModem(QUERYATFCLASS, READTRY);
    if (!ret)
    {
    /* Modes start with 0 then optional mode numbers for example: 0,1,1.0,2,8 */
        if (strncmp(modembuf, "0", 1))
        {
            logMsg(LEVEL1, "Modem supports Data Mode\n");
        }
        if (strstr(modembuf, ",1"))
        {
            logMsg(LEVEL1, "Modem supports FAX Mode 1\n");
        }
        if (strstr(modembuf, ",2"))
        {
            logMsg(LEVEL1, "Modem supports FAX Mode 2\n");
        }
        if (strstr(modembuf, ",8"))
        {
            logMsg(LEVEL1, "Modem supports VOICE Mode\n");
        }
    }

    /* if hangup option set, do checks on hangup modes  */
    if (hangup) {

        /* determine if FAX or VOICE mode can be used, if not change it */
        switch (hangup)
        {
            case 1: /* normal hangup */
                break;          
            case 2: /* FAX mode */
                if (!ret && !strstr(modembuf, ",1"))
                {
                hangup = 1;
                logMsg(LEVEL1, "WARNING: Using Normal Hangup, modem does not support FAX Hangup\n");
                }
                break;
            case 3: /* VOICE mode */
                if (!ret && !strstr(modembuf, ",8"))
                {
                    hangup = 1;
                    logMsg(LEVEL1, "WARNING: Using Normal Hangup, modem does not support Announce Hangup\n");
                }
                if (stat(announce, &statbuf))
                {
                    hangup = 1;
                    sprintf(msgbuf, "WARNING: Using Normal Hangup, no announcement file: %s\n", announce);
                    logMsg(LEVEL1, msgbuf);
                }
                break;
            default: /* should never happen */
                sprintf(msgbuf, "WARNING: Unknown hangup option (%d), using Normal Hangup\n", hangup);
                logMsg(LEVEL1, msgbuf);
                hangup = 1;
                break;
        }

        /* log hangup option used */
        sprintf(msgbuf, "Hangup option = ");
        switch (hangup)
        {
            case 1:
                strcat(msgbuf, "1: hangup");
                break;
            case 2:
                strcat(msgbuf, "2: generate FAX tones then hangup");
                break;
            case 3:
                strcat(msgbuf, "3: play an announcement then hangup");
                break;
        }
        strcat(msgbuf, " on a blacklisted call\n");
        logMsg(LEVEL1, msgbuf);

        /* if FAX, log pickup option status, if VOICE log announcement file */
        switch (hangup)
        {
            case 2: /* FAX */
                sprintf(msgbuf, "Pickup %s for FAX hangup\n",
                pickup == 1 ? "enabled" : "not enabled");
                logMsg(LEVEL1, msgbuf);
                break;
            case 3: /* VOICE */
                sprintf(msgbuf, "Announcement File: %s\n", announce);
                logMsg(LEVEL1, msgbuf);

                /* Query Voice Sampling Methods (audio data formats) */
                ret = initModem(VOICEMODE, READTRY);
                ret = initModem(QUERYATFMI, READTRY);
                if (!ret)
                {
                    ptr = strchr(modembuf, (int) '\n') + 1;
                    eptr = strchr(ptr, (int) '\r');
                    *(eptr + 2) = '\0'; /* skip over "\r\n" */
                    sprintf(msgbuf, "Manufacturer: %s", ptr);
                    logMsg(LEVEL1, msgbuf);
                }
                else logMsg(LEVEL1, "Modem AT+FMI query failed\n");
                ret = initModem(QUERYATVSM, READTRY);
                if (!ret)
                {
                    ptr = strchr(modembuf, (int) '\n') + 1;
                    eptr = strrchr(modembuf, (int) ',');
                    eptr = strchr(eptr, (int) '\r');
                    *(eptr + 2) = '\0'; /* skip over "\r\n" */
                    sprintf(msgbuf, "Modem Voice Sampling Methods:\n%s", ptr);
                    logMsg(LEVEL1, msgbuf);
                }
                else logMsg(LEVEL1, "Modem AT+VSM=? query failed\n");
                sprintf(msgbuf, "Modem Voice Sampling Method selected: %s\n", audiofmt);
                logMsg(LEVEL1, msgbuf);
                ret = initModem(DATAMODE, READTRY);
                break;
        }
    }
    else
    {
        sprintf(msgbuf, "Hangup option = %d: disabled\n", hangup);
        logMsg(LEVEL1, msgbuf);
    }
}

/*
 * Configure the modem
 * returns:  0 if successful
 *          -1 if cannot read from or write to modem
 * exits program if major problem
 */
int doModem()
{
    int cnt, ret = 2;
    char msgbuf[BUFSIZ];

    if (*initstr)
    {
        /*
        * Try to initialize modem, sometimes the modem
        * fails to respond the 1st time, so try multiple
        * times on a no response return code, before
        * indicating no modem.
        */
        for (cnt = 0; ret == 2 && cnt < MODEMTRY; ++cnt)
        {
            if ((ret = initModem(initstr, READTRY)) < 0) return -1;
            sprintf(msgbuf, "Try %d to init modem: return = %d.\n", cnt + 1, ret);
            logMsg(LEVEL3, msgbuf);
        }

        if (ret)
        {
            tcsetattr(ttyfd, TCSANOW, &otty);
            switch (ret)
            {
                case 1: /* CONNECT */
                    sprintf(msgbuf, "Modem returned \"CONNECT\".\n");
                    logMsg(LEVEL1, msgbuf);
                    break;
                case 2: /* ERROR */
                case 3: /* incomplete or unexpected response from modem */
                    errorExit(-103, "Unable to initialize modem", ttyport);
                    break;
                case 4: /* no response */
                    errorExit(-105, "No modem found", ttyport);
                    break;
                case -1: /* cannot read from or write to modem */
                    errorExit(-103, "Unable to initialize modem", ttyport);
                    break;

            }
        }
        else
        {
            /* OK */
            sprintf(msgbuf, "Modem initialized.\n");
            logMsg(LEVEL1, msgbuf);
        }
    }
    else
    {
        /* initstr is null */
        sprintf(msgbuf, "Initialization string for modem is null.\n");
        logMsg(LEVEL1, msgbuf);
    }

    /* check some modem information and hangup options */
    checkModem();

    if (!noserial && *initcid)
    {
        /* try to initialize modem for CID */
        if ((ret = initModem(initcid, READTRY)) < 0) return -1;

        if (ret && !setcid)
        {
            /*default init string 1 failed, try default init string 2 */
            initcid = INITCID2;
            if ((ret = initModem(initcid, READTRY)) < 0) return -1;
        }

        if (ret)
        {
            /* CID initialization failed */
            tcsetattr(ttyfd, TCSANOW, &otty);
            errorExit(-103, "Unable to set modem CallerID", ttyport);
        }
        else
        {
            /* CID initialization succeeded */
            sprintf(msgbuf, "Modem set for CallerID.\n");
            logMsg(LEVEL1, msgbuf);
        }
    }
    else if (*initcid == '\0')
    {
        /* initcid is null */
        sprintf(msgbuf, "CallerID initialization string for modem is null.\n");
        logMsg(LEVEL1, msgbuf);
    }

    return 0;
}

/*
 * Initialize modem
 * expects:  initialization string
 * returns:  0 modem returns OK
 *           1 modem returns "CONNECT"
 *           2 modem returns "ERROR"
 *           3 incomplete or unexpected response from modem
 *           4 no response from modem
 *          -1 cannot read from or write to modem
 */
int initModem(char *ptr, int maxtry)
{
    int num, size, try, ret = 4;
    char *bufptr, msgbuf[BUFSIZ];

    /* send string to modem */
    strcat(strncpy(modembuf, ptr, BUFSIZ - 2), CRLF);
    size = strlen(modembuf);
    if ((num = write(ttyfd, modembuf, size)) < 0) return -1;
    sprintf(msgbuf, "Sent Modem %d of %d characters: \n%s", num, size, modembuf);
    logMsg(LEVEL3, msgbuf);
    if (verbose >= 7) hexdump(modembuf,size);

    /*
     * read until OK, CONNECT or ERROR response detected
     * or number of tries exceeded
     */
    for (size = try = 0; try < maxtry; try++)
    {
        usleep(READWAIT);
        if ((num = read(ttyfd, modembuf + size, BUFSIZ - size - 1)) < 0) return -1;
        size += num;
        if (size)
        {
            /* check response */
            modembuf[size] = 0;
            if (strstr(modembuf, "OK"))
            {
                ret = 0;
                break;
            }
            else if (strstr(modembuf, "CONNECT"))
            {
                ret = 1;
                break;
            }
            else if (strstr(modembuf, "ERROR"))
            {
                ret = 2;
                break;
            }
            else
            {
                ret = 3;
            }
        }
    }
    modembuf[size] = 0;

    if (size)
    {
        if (fixModembuf)
        {
            bufptr = modembuf;
            while (bufptr < modembuf + size)
            {
                bufptr = strchr(bufptr, (int) '\0');
                if (!(bufptr && (bufptr < modembuf + size))) break;
                if (*(bufptr + 1) =='\n') *bufptr = '\r';
            }
            fixModembuf = 0;
        }
        sprintf(msgbuf, "Modem response: %d characters in %d %s:\n%s",
            size, try > maxtry ? try -1 : try + 1,
            try == 0 ? "read" : "reads", modembuf);
        logMsg(LEVEL3, msgbuf);
        if (verbose >= HEXLEVEL) hexdump(modembuf,size);
    }
    else
    {
        /* maxtry can be zero to not read a modem response */
        if (maxtry) {
            sprintf(msgbuf, "No Modem Response\n");
            logMsg(LEVEL3, msgbuf);
        }
        else
        {
            sprintf(msgbuf, "Skipped read for a modem response\n");
            logMsg(LEVEL3, msgbuf);
        }
    }

    return ret;
}

int tcpOpen()
{
    int     sd, ret, optval;
    static struct  sockaddr_in bind_addr;
    int socksize = sizeof(bind_addr);

    optval = 1;
    bind_addr.sin_family = PF_INET;
    bind_addr.sin_addr.s_addr = 0;    /*  0.0.0.0  ==  this host  */
    memset(bind_addr.sin_zero, 0, 8);
    bind_addr.sin_port = htons(port);
    if ((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
        return sd;
    if((ret = setsockopt(sd, SOL_SOCKET, SO_REUSEADDR,
        &optval, sizeof(optval))) < 0)
        return ret;
    if((ret = setsockopt(sd, SOL_SOCKET, SO_KEEPALIVE,
        &optval, sizeof(optval))) < 0)
        return ret;
    if ((ret = bind(sd, (struct sockaddr *)&bind_addr, socksize)) < 0)
    {
        close(sd);
        return ret;
    }
    if ((ret = listen(sd, MAXCONNECT)) < 0)
    {
        close(sd);
        return ret;
    }
    return sd;
}

int  tcpAccept()
{
    int sd;
    int ret;
    char tmpbuf[BUFSIZ];

    struct  sockaddr_in sa;
    unsigned int sa_len = sizeof(sa);

    if ((sd = accept(mainsock, (struct sockaddr *) &sa, &sa_len)) != -1)
    {
        strcpy(tmpIPaddr, inet_ntoa(sa.sin_addr));
        ret = getnameinfo((struct sockaddr *) &sa, sa_len, tmpbuf, sizeof(tmpbuf), NULL, 0, 0);
        if (ret == 0)
        {
           sprintf(tmpHostName, " [%s]",tmpbuf);
        } else {
           sprintf(tmpHostName, " [hostname lookup error %d, %s]", ret, gai_strerror(ret));
        }
    }    

    return sd;
}

int addPoll(int pollfd)
{
    int added = 0, pos;

    for (added = pos = 0; pos < MAXCONNECT; ++pos)
    {
        if (polld[pos].fd) continue;
        ack[pos] = 0;
        polld[pos].revents = 0;
        polld[pos].fd = pollfd;
        polld[pos].events = (POLLIN | POLLPRI);
        ++added;
        break;
    }
    return added ? pos : -1;
}

void doPoll(int events)
{
  static int cnt;
  int num, pos, sd = 0, ret, tmpint;
  char buf[BUFSIZ], tmpbuf[BUFSIZ], msgbuf[BUFSIZ], msgbuf2[BUFSIZ];
  char *ptr, *sptr, *eptr, *label;
  char **svrtag;

  (void) ret;

  /*
   * Poll is configured for POLLIN and POLLPRI events
   * POLLERR, POLLHUP, POLLNVAL events can also happen
   * Poll is not configured for the POLLOUT event
   */

  for (pos = 0; events && pos < MAXCONNECT; ++pos)
  {
    if (!polld[pos].revents) continue; /* no events */

    /* log event flags */
    sprintf(msgbuf, "polld[%d].revents: 0x%X, fd: %d\n",
            pos, polld[pos].revents, polld[pos].fd);
    logMsg(LEVEL9, msgbuf);

    if (polld[pos].revents & POLLHUP) /* Hung up */
    {
      if (!noserial && polld[pos].fd == ttyfd)
      {
        sprintf(buf, "%sSerial device %d pos %d Hung Up, Terminated  %s",
                MSGLINE,  polld[pos].fd, pos, strdate(WITHSEP));
        writeClients(buf);
        errorExit(-112, "Fatal", "Serial device hung up");
      }
      sprintf(msgbuf, "Client %d pos %d Hung Up\n", polld[pos].fd, pos);
      logMsg(LEVEL2, msgbuf);
      close(polld[pos].fd);
      polld[pos].fd = polld[pos].events = polld[pos].revents = 0;
    }

    if (polld[pos].revents & POLLERR) /* Poll Error */
    {
      if (!noserial && polld[pos].fd == ttyfd)
      {
        sprintf(buf, "%sSerial device %d pos %d error, Terminated  %s",
                MSGLINE, polld[pos].fd, pos, strdate(WITHSEP));
        writeClients(buf);
        errorExit(-112, "Fatal", "Serial device error");
      }
        sprintf(msgbuf, "Poll Error, closed client %d pos %d.\n",
                polld[pos].fd, pos);
        logMsg(LEVEL1, msgbuf);
        close(polld[pos].fd);
        polld[pos].fd = polld[pos].events = polld[pos].revents = 0;
    }

    if (polld[pos].revents & POLLNVAL) /* Invalid Request */
    {
    if (!noserial && polld[pos].fd == ttyfd)
      {
        sprintf(buf, "%sInvalid Request from Serial device %d pos %d, Terminated  %s",
                MSGLINE, polld[pos].fd, pos, strdate(WITHSEP));
        writeClients(buf);
        errorExit(-112, "Fatal", "Invalid Request from Serial device");
      }
      sprintf(msgbuf, "Removed client %d pos %d, invalid request.\n",
              polld[pos].fd, pos);
      logMsg(LEVEL1, msgbuf);
      polld[pos].fd = polld[pos].events = polld[pos].revents = 0;
    }

    if (polld[pos].revents & POLLOUT) /* Write Event */
    {
      sprintf(msgbuf, "Removed client %d pos %d, write event not configured.\n",
              polld[pos].fd, pos);
      logMsg(LEVEL1, msgbuf);
      polld[pos].fd = polld[pos].events = polld[pos].revents = 0;
    }

    if (polld[pos].revents & (POLLIN | POLLPRI))
    {
      if (!noserial && polld[pos].fd == ttyfd)
      {
        if (!locked)
        {
          /* Modem or device has data to read */
          if ((num = read(ttyfd, buf, BUFSIZ-1)) < 0)
          {
            sprintf(msgbuf, "Serial device %d pos %d read error: %s\n",
                    ttyfd, pos, strerror(errno));
            errorExit(-112, "Fatal", msgbuf);
          }

          /* Modem or device returned no data */
          else if (!num)
          {
            cnt++;
            /* if no data 10 times in a row, something wrong */
            if (cnt >= 10)
            {
              sprintf(buf,
                "%sSerial device %d pos %d returned no data in %d tries. Terminated  %s",                
                MSGLINE, ttyfd, pos, cnt, strdate(WITHSEP));
              writeClients(buf);
              errorExit(-112, "Fatal", "Serial device returned no data");
            }
            else
            {
                sprintf(msgbuf, "Serial device %d pos %d returned no data in try #%d.\n", ttyfd, pos, cnt);
                logMsg(LEVEL2, msgbuf);
            }
          }
          else
          {
            /* Modem or device returned data */

            cnt = 0;

            /* Terminate String */
            buf[num] = '\0';

            /* strip <CR> and <LF> */
            if ((ptr = strchr(buf, '\r'))) *ptr = '\0';
            if ((ptr = strchr(buf, '\n'))) *ptr = '\0';

            writeLog(datalog, buf);
            formatCID(buf);
          }
        }
      }
      else if (polld[pos].fd == mainsock)
      {
        /* TCP/IP Client Connection */
        if ((sd = tcpAccept()) < 0)
        {
          sprintf(msgbuf, "Connect Error: %s\n", strerror(errno));
          logMsg(LEVEL1, msgbuf);
        }
        else
        {
          /* Client connected */

          if (fcntl(sd, F_SETFL, O_NONBLOCK) < 0)
          {
            sprintf(msgbuf, "NONBLOCK Error: %s, sd: %d\n",
              strerror(errno), sd);
            logMsg(LEVEL1, msgbuf);
            close(sd);
          }
          else
          {
            if ((pos = addPoll(sd)) < 0)
            {
              sprintf(msgbuf, "Client trying to connect.\n");
              logMsg(LEVEL1, msgbuf);
              sprintf(msgbuf, NOLOGSENT NL);
              logMsg(LEVEL1, msgbuf);
              sprintf(msgbuf, TOOMSG, noserial ? MAXCLIENTS + 1 : MAXCLIENTS,
                      strdate(WITHSEP), NL);
              logMsg(LEVEL1, msgbuf);
              sprintf(buf, NOLOGSENT CRLF);
              ret = write(sd, buf, strlen(buf));
              sprintf(buf, TOOMSG, noserial ? MAXCLIENTS + 1 :MAXCLIENTS,
                      strdate(WITHSEP), CRLF);
              ret = write(sd, buf, strlen(buf));
              close(sd);
            }
            else
            {
              strcpy(IPinfo[pos].addr, tmpIPaddr);
              strcpy(IPinfo[pos].name, tmpHostName);
              sprintf(msgbuf, "Client %d pos %d from %s%s connected %s\n", 
                      sd, pos, IPinfo[pos].addr, IPinfo[pos].name, strdate(WITHSEP));
              logMsg(LEVEL2, msgbuf);

              sprintf(buf, "%s %s %s%s", ANNOUNCE, name, VERSION, CRLF);
              ret = write(sd, buf, strlen(buf));
              sprintf(buf, "%s %s %s\n", ANNOUNCE, name, VERSION);
              logMsg(LEVEL3, buf);

              sprintf(buf, "%s%s%s", APIANNOUNCE, API, CRLF);
              ret = write(sd, buf, strlen(buf));
              sprintf(buf, "%s%s\n", APIANNOUNCE, API);
              logMsg(LEVEL3, buf);

              if (sendlog)
              {
                sendLog(sd, buf);
              }
              else
              {
                /* CID log not sent */
                sprintf(msgbuf, "%s%s", NOLOGSENT, CRLF);
                ret = write(sd, msgbuf, strlen(msgbuf));
                sprintf(msgbuf, "Call log not sent: %s\n", cidlog);
                logMsg(LEVEL3, msgbuf);
              }
              if (hangup)
              { 
                sprintf (msgbuf, OPTLINE "hangup" CRLF);
                ret = write (sd, msgbuf, strlen(msgbuf));
                sprintf(msgbuf, "Sent 'hangup' option to client\n");
                logMsg(LEVEL3, msgbuf);
              }
              /* End of startup messages */
              sprintf(msgbuf, "%s%s", ENDSTARTUP, CRLF);
              ret = write(sd, msgbuf, strlen(msgbuf));
              sprintf(msgbuf, "%s\n", ENDSTARTUP);
              logMsg(LEVEL3, msgbuf);
            }
          }
        }
      }
      else
      {
        if (polld[pos].fd)
        {
          if ((num = read(polld[pos].fd, buf, BUFSIZ-1)) < 0)
          {
            sprintf(msgbuf, "Client %d pos %d read error %d: %s\n",
                    polld[pos].fd, pos, errno, strerror(errno));
            logMsg(LEVEL1, msgbuf);
            if (errno != EAGAIN)
            {
                sprintf(msgbuf, "Client %d pos %d removed.\n", polld[pos].fd, pos);
                logMsg(LEVEL1, msgbuf);
                close(polld[pos].fd);
                polld[pos].fd = polld[pos].events = polld[pos].revents = 0;
            }
          }
          /* read will return 0 for a disconnect */
          else if (num == 0)
          {
            /* TCP/IP Client End Connection */
            sprintf(msgbuf, "Client %d pos %d from %s%s disconnected %s\n",
                    polld[pos].fd, pos, IPinfo[pos].addr, IPinfo[pos].name, strdate(WITHSEP));
            logMsg(LEVEL2, msgbuf);
            close(polld[pos].fd);
            polld[pos].fd = polld[pos].events = polld[pos].revents = 0;
          }
          else
          {
            /*
             * Client sent message to server
             */

            /* Terminate String */
            buf[num] = '\0';

            /* strip <CR> and <LF> */
            if ((ptr = strchr(buf, '\r'))) *ptr = '\0';
            if ((ptr = strchr(buf, '\n'))) *ptr = '\0';

            /*
             * Check first character is a 7-bit unsigned char value
             * if not, assume entire line is not wanted.  This may
             * need to be improved, but this gets rid of telnet binary.
             */
             if (isascii((int) buf[0]) == 0)
             {
                buf[0] = '\0';
                sprintf(msgbuf, "Message deleted, not 7-bit ASCII, sd: %d\n",
                  polld[pos].fd);
                logMsg(LEVEL3, msgbuf);
             }

            /* Make sure there is data in the message line */
            if (strlen(buf) != 0)
            {

              /* Look for CALL, CALLINFO, or MSG lines */
              if (strncmp(buf, CALL, strlen(CALL)) == 0)
              {
                /*
                 * Found a CALL Line
                 * See comments for formatCID for line format
                 */

                sprintf(msgbuf, "Gateway (sd %d) sent CALL data.\n",
                  polld[pos].fd);
                logMsg(LEVEL3, msgbuf);

                writeLog(datalog, buf);
                if (ack[pos])
                {
                    sprintf(msgbuf, "%s%s%s", ACKLINE, buf, CRLF);
                    ret = write(polld[pos].fd, msgbuf, strlen (msgbuf));
                    sprintf(msgbuf, "(sd %d) %s%s%s", polld[pos].fd, ACKLINE, buf, NL);
                    logMsg(LEVEL3, msgbuf);
                }
                formatCID(buf + strlen(CALL));
              }
              else if (strncmp(buf, CALLINFO, strlen(CALLINFO)) == 0)
              {
                /*
                 * Found a CALLINFO Line
                 *
                 * CALLINFO Line Format:
                 *  CALLINFO: ###CANCEL...DATE%s...SCALL%S...ECALL%s...CALLIN...LINE%s...NMBR%s...NAME%s+++
                 *  CALLINFO: ###CANCEL...DATE%s...SCALL%S...ECALL%s...CALLOUT...LINE%s...NMBR%s...NAME%s+++
                 *  CALLINFO: ###BYE...DATE%s...SCALL%S...ECALL%s...CALLIN...LINE%s...NMBR%s...NAME%s+++
                 *  CALLINFO: ###BYE...DATE%s...SCALL%S...ECALL%s...CALLOUT...LINE%s...NMBR%s...NAME%s+++
                 */

                sprintf(msgbuf, "Gateway (sd %d) sent CALLINFO:\n",
                        polld[pos].fd);
                logMsg(LEVEL3, msgbuf);

                writeLog(datalog, buf);
                if (ack[pos])
                {
                    sprintf(msgbuf, "%s%s%s", ACKLINE, buf, CRLF);
                    ret = write(polld[pos].fd, msgbuf, strlen (msgbuf));
                    sprintf(msgbuf, "(sd %d) %s%s%s", polld[pos].fd, ACKLINE, buf, NL);
                    logMsg(LEVEL3, msgbuf);
                }

                /* get and process end of call termination */
                if (strstr(buf, CANCEL))
                {
                    strcpy(endcall.htype, CANCEL);
                    strcpy(infoline, endcall.line);
                    tmpint = ring;
                    ring = -1;
                    sendInfo();
                    ring = tmpint;
                }
                else if (strstr(buf, BYE))
                {
                    strcpy(endcall.htype, BYE);
                    strcpy(infoline, endcall.line);
                    tmpint = ring;
                    ring = -2;
                    sendInfo();
                    ring = tmpint;
                }
                else strcpy(endcall.htype, "-");

                /* get end of call date and time */
                label = "DATE";
                if ((sptr = strstr(buf, label)) != NULL)
                {
                    /* points to DATEmmddhhmm... */
                    sptr += (strlen(label));        /* MMDDHHMM */
                    ptr = strdate(ONLYYEAR);        /* returns: YYYY */
                    strncpy(endcall.date, sptr, 4); /* MMDD */
                    endcall.date[4] = '\0';
                    strcat(endcall.date, ptr);      /* MMDDYYYY */
                    
                    sptr += (4);                    /* HHMM */
                    strncpy(endcall.time, sptr, 4);
                    endcall.time[4] = '\0';
                }
                else
                {
                    strcpy(endcall.date, "-");
                    strcpy(endcall.time, "-");
                }

                /* get end of call start date and extended time */
                label = "SCALL";
                if ((sptr = strstr(buf, label)) != NULL)
                {
                    sptr += strlen(label);
                    if (!(eptr = strstr(sptr, "...")))
                        eptr = strstr(sptr, "+++");
                    strncpy(endcall.scall, sptr, eptr - sptr);
                    endcall.scall[eptr - sptr] = '\0';
                }
                else  strcpy(endcall.scall, "-");

                /* get end of call end date and extended time */
                label = "ECALL";
                if ((sptr = strstr(buf, label)) != NULL)
                {
                    sptr += strlen(label);
                    if (!(eptr = strstr(sptr, "...")))
                        eptr = strstr(sptr, "+++");
                    strncpy(endcall.ecall, sptr, eptr - sptr);
                    endcall.ecall[eptr - sptr] = '\0';
                }
                else  strcpy(endcall.ecall, "-");

                /* get end of call type */
                label = ".CALL";
                if ((sptr = strstr(buf, label)) != NULL)
                {
                    sptr += strlen(label);
                    if (!(eptr = strstr(sptr, "...")))
                        eptr = strstr(sptr, "+++");
                    strncpy(endcall.ctype, sptr, eptr - sptr);
                    endcall.ctype[eptr - sptr] = '\0';
                }
                else  strcpy(endcall.ctype, "-");
                

                /* get end of call line label */
                label = "LINE";
                if ((sptr = strstr(buf, label)) != NULL)
                {
                    sptr += strlen(label);
                    if (!(eptr = strstr(sptr, "...")))
                        eptr = strstr(sptr, "+++");
                    strncpy(endcall.line, sptr, eptr - sptr);
                    endcall.line[eptr - sptr] = '\0';
                    strcpy(infoline, endcall.line);
                }
                else
                {
                    strcpy(infoline, lineid);
                    strcpy(endcall.line, "-");
                }

                /* get end of call telephone number */
                label = "NMBR";
                if ((sptr = strstr(buf, label)) != NULL)
                {
                    sptr += strlen(label);
                    if (!(eptr = strstr(sptr, "...")))
                        eptr = strstr(sptr, "+++");
                    strncpy(endcall.nmbr, sptr, eptr - sptr);
                    endcall.nmbr[eptr - sptr] = '\0';
                }
                else  strcpy(endcall.nmbr, "-");

                /* get end of call name */
                label = "NAME";
                if ((sptr = strstr(buf, label)) != NULL)
                {
                    sptr += strlen(label);
                    if (!(eptr = strstr(sptr, "...")))
                        eptr = strstr(sptr, "+++");
                    strncpy(endcall.name, sptr, eptr - sptr);
                    endcall.name[eptr - sptr] = '\0';
                }
                else  strcpy(endcall.name, "-");

                userAlias(endcall.nmbr, endcall.name, endcall.line);

                /*
                 * This sprintf() probably needs the optional blacklist
                 * match name for NAME if ncidd does hangup but it is
                 * not simple to add because of other possible calls
                 * before this one ends.  --jlc
                 */
                sprintf(msgbuf, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
                    ENDLINE,
                    HTYPE, endcall.htype,
                    DATE,  endcall.date,
                    TIME,  endcall.time,
                    SCALL, endcall.scall,
                    ECALL, endcall.ecall,
                    CTYPE, endcall.ctype,
                    LINE,  endcall.line,
                    NMBR,  endcall.nmbr,
                    NAME,  endcall.name,
                    STAR);

                /* Log the end of call "END:" line */
                writeLog(cidlog, msgbuf);
              }
              else if (!strncmp(buf + 3, ": *", strlen(": *")) ||
                      !strncmp(buf + 8, ": *", strlen(": *")) ||
                      (!strncmp(buf + 3, ": ", strlen(": ")) && strstr (buf, "***")))
//jlc:
              {
                /*
                 * Found a line from another server
                 * for example: "CID: *", "HUP: *", "CIDINFO: *
                 * messages require two checks:
                 *      "MSG: " and "***",  "NOT: " and "***"
                 */

                for (svrtag = serverTags; *svrtag; svrtag++)
                {
                    if (!strncmp(buf, *svrtag, strlen(*svrtag)))
                    {
                        sprintf(msgbuf, "Server (sd %d) sent %s\n",
                            polld[pos].fd, buf);
                        logMsg(LEVEL3, msgbuf);
                        writeLog(cidlog, buf);
                        writeClients(buf);
                    }
                }
                if (*svrtag == '\0')
                {
                    sprintf(msgbuf, "Ignoring Server (sd %d) line %s\n",
                            polld[pos].fd, buf);
                    logMsg(LEVEL3, msgbuf);
                }
              }
              else if (!strncmp(buf, MSGLINE, strlen(MSGLINE)))
              {
                /*
                 * Found a MSG: line
                 * MSG: <message> ###DATE*mmddyyyy*TIME*hhmm*NAME*<name>*NMBR*<number>*LINE*<id>*MTYPE*<IN|OUT>*
                 * Write message to cidlog and all clients
                 */

                sprintf(msgbuf, "Client %d sent text message.\n", polld[pos].fd);
                logMsg(LEVEL3, msgbuf);
                writeLog(datalog, buf);
                getINFO(buf);
                sprintf(tmpbuf, MESSAGE, buf, mesg.date, mesg.time, mesg.name, mesg.nmbr, mesg.line, mesg.type);
                writeLog(cidlog, tmpbuf);
                writeClients(tmpbuf);
              }
              else if (!strncmp(buf, NOTLINE, strlen(NOTLINE)))
              {
                /*
                 * Found a NOT: (remote notification) line from a cell phone
                 * NOT: <message> ###DATE*mmddyyyy*TIME*hhmm*NAME*<name>*NMBR*<number>*LINE*<id>*MTYPE*<IN|OUT>*
                 * Write notice to cidlog and all clients
                 */

                sprintf(msgbuf, "Gateway (sd %d) sent a notice.\n", polld[pos].fd);
                logMsg(LEVEL3, msgbuf);
                writeLog(datalog, buf);
                if (ack[pos])
                {
                    sprintf(msgbuf, "%s%s%s", ACKLINE, buf, CRLF);
                    ret = write(polld[pos].fd, msgbuf, strlen (msgbuf));
                    sprintf(msgbuf, "(sd %d) %s%s%s", polld[pos].fd, ACKLINE, buf, NL);
                    logMsg(LEVEL3, msgbuf);
                }
                getINFO(buf);
                sprintf(tmpbuf, MESSAGE, buf, mesg.date, mesg.time, mesg.name, mesg.nmbr, mesg.line, mesg.type);
                writeLog(cidlog, tmpbuf);
                writeClients(tmpbuf);
              }
              else if (strncmp (buf, REQLINE, strlen(REQLINE)) == 0)
              {
                /* 
                 * Found a REQ: line
                 * Perform the requested action and send a response
                 * back to the client
                 */
                 strcat(strcpy(msgbuf, buf), NL);
                 logMsg(LEVEL2, msgbuf);
                 if (strstr(buf, RELOAD))
                 {
                    long position = 0;

                    if (logptr) {
                        position = ftell (logptr);
                    }
                    reload (1);
                    if (logptr)
                    {
                       *buf = 0;
                       cnt = 0;
                       fseek (logptr, position, SEEK_SET);
                       while (fgets (tmpbuf, sizeof (tmpbuf), logptr) != 0)
                       {
                           cnt += sizeof (INFOLINE) + strlen (tmpbuf);
                           if ((unsigned)cnt >= sizeof (buf) - 2) break;
                           strcat (buf, INFOLINE);
                           strcat (buf, tmpbuf);
                       }
                    }
                    else
                    {
                       strcpy (buf, INFOLINE RELOADED NL);
                    }
                    ret = write (polld[pos].fd, BEGIN_DATA CRLF,
                                 strlen (BEGIN_DATA CRLF));
                    logMsg(LEVEL2, BEGIN_DATA NL);
                    ret = write (polld[pos].fd, buf, strlen(buf));
                    logMsg(LEVEL2, buf);
                    ret = write (polld[pos].fd, END_DATA CRLF,
                                 strlen (END_DATA CRLF));
                    logMsg(LEVEL2, END_DATA NL);
                 }
                 else if (strstr (buf, UPDATE))
                 {
                   /* can be UPDATE or UPDATES */

                    FILE *respHandle;
                    char *ignore;

                    (void) ignore;

                    sprintf (tmpbuf, DOUPDATE, cidalias, cidlog);
                    if (strstr (buf, UPDATES)) strcat(tmpbuf, " --multi");
                    if (ignore1) strcat(tmpbuf, " --ignore1");
                    if (regex) strcat(tmpbuf, " --regex");
                    strcat(tmpbuf, " < /dev/null 2>&1");

                    sprintf(msgbuf,
                             "Begin: Executing %s [%s]\n", NCIDUPDATE, strdate(ONLYTIME));
                    logMsg(LEVEL4, msgbuf);
                    respHandle = popen (tmpbuf, "r");
                    sprintf(msgbuf,
                             "End: Executing %s [%s]\n", NCIDUPDATE, strdate(ONLYTIME));
                    logMsg(LEVEL4, msgbuf);

                    strcat(tmpbuf, "\n");
                    logMsg(LEVEL2, tmpbuf);
                    strcpy (msgbuf, INFOLINE);
                    ptr = msgbuf + sizeof (INFOLINE) - 1;
                    cnt = sizeof (msgbuf) - sizeof (INFOLINE);
                    ignore = fgets (ptr, cnt, respHandle);
                    if (strstr(msgbuf, NOCHANGES) || strstr(msgbuf, DENIED))
                    {
                        /* There were no changes to the call log */
                        ret = write (polld[pos].fd, BEGIN_DATA CRLF,
                                     strlen (BEGIN_DATA CRLF));
                        logMsg(LEVEL2, BEGIN_DATA NL);
                        ret = write (polld[pos].fd, msgbuf, strlen (msgbuf));
                        logMsg(LEVEL2, msgbuf);
                    }
                    else
                    {
                        /* There were changes to the call log */
                        ret = write (polld[pos].fd, BEGIN_DATA1 CRLF,
                                     strlen (BEGIN_DATA1 CRLF));
                        logMsg(LEVEL2, BEGIN_DATA1 NL);
                        ret = write(polld[pos].fd, msgbuf, strlen(msgbuf));
                        logMsg(LEVEL2, msgbuf);
                        while (fgets(ptr, cnt, respHandle))
                        {
                            ret = write(polld[pos].fd, msgbuf, strlen(msgbuf));
                            logMsg(LEVEL2, msgbuf);
                        }
                    }
                    ret = write(polld[pos].fd, END_DATA CRLF,
                                strlen (END_DATA CRLF));
                    pclose (respHandle);
                    logMsg(LEVEL2, END_DATA NL);

                 }
                 else if (strstr(buf, REREAD))
                 {
                    sendLog(polld[pos].fd, buf);
                 }
                 else if (!strcmp(buf, REQ_ACK) || !strcmp(buf, REQ_YO))
                 {
                    if (strstr(buf, ACK)) ack[pos] = 1;
                    sprintf(msgbuf, "(sd %d) sent %s\n", polld[pos].fd, buf);
                    logMsg(LEVEL3, msgbuf);
                    sprintf(msgbuf, "%s%s%s", ACKLINE, buf, CRLF);
                    ret = write(polld[pos].fd, msgbuf, strlen (msgbuf));
                    sprintf(msgbuf, "(sd %d) %s%s%s", polld[pos].fd, ACKLINE, buf, NL);
                    logMsg(LEVEL3, msgbuf);
                 }
                 else 
                 {
                    char *filename = "", *ptr, *type = "", multi[BUFSIZ],
                         opt[BUFSIZ];

                    multi[0] = opt[0] = '\0';
                    if (ignore1) strcat(opt, " --ignore1");
                    if (regex) strcat(opt, "--regex");
                    ptr = buf + strlen(REQLINE);
                    if (strncmp(ptr, BLK_LST , strlen(BLK_LST)) == 0)
                    {
                       filename = blacklist;
                       ptr += strlen(BLK_LST);
                       type = "Blacklist";
                    }
                    else if (strncmp(ptr, ALIAS_LST , strlen(ALIAS_LST)) == 0)
                    {
                       filename = cidalias;
                       ptr += strlen(ALIAS_LST);
                       type = "Alias";
                       sprintf (multi, "--multi \"%s %s\"", blacklist, whitelist);
                    }
                    else if (strncmp(ptr, WHT_LST , strlen(WHT_LST)) == 0)
                    {
                       filename = whitelist;
                       ptr += strlen(WHT_LST);
                       type = "Whitelist";
                    }
                    else if (strncmp(ptr, INFO_REQ, strlen(INFO_REQ)) == 0)
                    {
                       /* found a REQ: INFO <nmbr>&&<name>&&<line> line */
                       char  name[CIDSIZE], number[CIDSIZE], line[CIDSIZE], *temp;
                       int   which;

                        /* all this in case the REQ: line is incomplete */
                        number[0] = name[0] = line[0] = '\0';
                        if (strlen(ptr) > (strlen(INFO_REQ) + 1))
                        {
                          ptr += strlen(INFO_REQ) + 1;
                          if ((temp = strstr(ptr, "&&"))) *temp = 0;
                          strncpy (number, ptr, CIDSIZE-1);
                          number[CIDSIZE-1] = 0;
                          if (temp)
                          {
                            ptr += strlen(number) + 2;
                            if ((temp = strstr(ptr, "&&"))) *temp = 0;
                            strncpy (name, ptr, CIDSIZE-1);
                            name[CIDSIZE-1] = 0;
                          }
                          if (temp)
                          {
                            ptr += strlen(name) + 2;
                            strncpy (line, ptr, CIDSIZE-1);
                            line[CIDSIZE-1] = 0;
                          }
                        }

                        sprintf(msgbuf,
                                 "Begin: findALias() [%s]\n", strdate(ONLYTIME));
                        logMsg(LEVEL4, msgbuf);
                        temp = findAlias(name, number, line);
                        sprintf(msgbuf,
                                 "End: findALias() [%s]\n", strdate(ONLYTIME));
                        logMsg(LEVEL4, msgbuf);

                        ret = write(polld[pos].fd, BEGIN_DATA3 CRLF,
                                     strlen(BEGIN_DATA3 CRLF));
                        logMsg(LEVEL2, BEGIN_DATA3 NL);
                        sprintf(msgbuf, INFOLINE "alias %s\n", temp);
                        logMsg(LEVEL2, msgbuf);
                        sprintf(msgbuf, INFOLINE "alias %s\r\n", temp);
                        ret = write(polld[pos].fd, msgbuf, strlen(msgbuf));

                        which = onBlackWhite(name, number);
                        switch (which)
                        {
                            case 0:
                                temp = "neither";
                                break;
                            case 1:
                                temp = "black name";
                                break;
                            case 2:
                                temp = "white name";
                                break;
                            case 5:
                                temp = "black number";
                                break;
                            case 6:
                                temp = "white number";
                                break;
                            default:
                                temp = "";
                                break;
                        }
                        sprintf (msgbuf, INFOLINE "%s\r\n" END_RESP CRLF, temp);
                        ret = write (polld[pos].fd, msgbuf, strlen (msgbuf));
                        sprintf (msgbuf, INFOLINE "%s\n" END_RESP NL, temp);
                        logMsg(LEVEL2, msgbuf);

                        if (number[0] == 0 || name[0] == 0) filename = "X";
                        else filename = "Dummy";
                        *ptr = 0;
                    }
                    if (strlen (filename) < 3)
                    {
                        char *temp;

                        if ((temp = strchr(ptr, ' '))) *temp = 0;
                        sprintf (msgbuf,
                                 "Unable to handle %s request - Ignored.\n",
                                 ptr);
                        logMsg(LEVEL1, msgbuf);
                    }
                    else if (strlen (ptr) > 4)
                    {
                                                
                        FILE        *respHandle;

                        ptr++;
                        sprintf (tmpbuf, DOUTIL, opt, multi, filename, type, ptr);

                        sprintf(msgbuf,
                                 "Begin: Executing %s [%s]\n", NCIDUTIL, strdate(ONLYTIME));
                        logMsg(LEVEL4, msgbuf);
                        respHandle = popen (tmpbuf, "r");
                        sprintf(msgbuf,
                                 "End: Executing %s [%s]\n", NCIDUTIL, strdate(ONLYTIME));
                        logMsg(LEVEL4, msgbuf);

                        strcat(tmpbuf, "\n");
                        logMsg(LEVEL2, tmpbuf);
                        ret = write (polld[pos].fd, BEGIN_DATA2 CRLF,
                                     strlen (BEGIN_DATA2 CRLF));
                        logMsg(LEVEL2, BEGIN_DATA2 NL);
                        strcpy(msgbuf, RESPLINE);
                        ptr = msgbuf + sizeof (RESPLINE) - 1;
                        cnt = sizeof (msgbuf) - sizeof (RESPLINE);
                        while (fgets (ptr, cnt, respHandle))
                        {
                            ret = write (polld[pos].fd, msgbuf, strlen (msgbuf));
                            logMsg(LEVEL2, msgbuf);
                        }
                        ret = write (polld[pos].fd, END_RESP CRLF,
                                     strlen (END_RESP CRLF));
                        pclose (respHandle);
                        logMsg(LEVEL2, END_RESP NL);
                        
                    }
                 }
              }
              else if (strncmp (buf, WRKLINE, strlen(WRKLINE)) == 0)
              {
                /* 
                 * Found a WRK: line
                 * Perform the requested work on behalf of the client
                 */
                 strcat(strcpy(msgbuf, buf), NL);
                 logMsg(LEVEL2, msgbuf);
                 if (strncmp (buf + strlen(WRKLINE), ACPT_LOG,
                     strlen (ACPT_LOG)) == 0)
                 {
                    if (strstr (buf + strlen(WRKLINE), ACPT_LOGS)) {
                        sprintf (msgbuf,
                                 "for f in %s.*[0-9]; do mv $f.new $f; done",
                                 cidlog);
                        ret = system (msgbuf);
                        sprintf (msgbuf2, " [%s]\n", strdate(ONLYTIME));
                        strcat(msgbuf, msgbuf2);
                        logMsg(LEVEL2, msgbuf);
                    }
                    sprintf (msgbuf, "mv %s.new %s", cidlog, cidlog);
                    ret = system (msgbuf);
                    sprintf (msgbuf2, " [%s]\n", strdate(ONLYTIME));
                    strcat(msgbuf, msgbuf2);
                    logMsg(LEVEL2, msgbuf);
                 }
                 else if (strncmp (buf + strlen(WRKLINE), RJCT_LOG,
                          strlen (RJCT_LOG)) == 0)
                 {
                    if (strstr (buf + strlen(WRKLINE), RJCT_LOGS)) {
                        sprintf (msgbuf, "rm %s.*.new",cidlog);
                        ret = system (msgbuf);
                        sprintf (msgbuf2, " [%s]\n", strdate(ONLYTIME));
                        strcat(msgbuf, msgbuf2);
                        logMsg(LEVEL2, msgbuf);
                    }
                    sprintf (msgbuf, "rm %s.new", cidlog);
                    ret = system (msgbuf);
                    sprintf (msgbuf2, " [%s]\n", strdate(ONLYTIME));
                    strcat(msgbuf, msgbuf2);
                    logMsg(LEVEL2, msgbuf);
                 }
              }
              else
              {
                /*
                 * Found unknown data
                 */

                sprintf(msgbuf, "Client %d sent unknown data.\n",
                        polld[pos].fd);
                logMsg(LEVEL3, msgbuf);
                writeLog(datalog, buf);
              }
            }
            else
            {
              /*
               * Found empty line
               */

                sprintf(msgbuf, "Client %d sent empty line.\n",
                        polld[pos].fd);
                logMsg(LEVEL6, msgbuf);
            }
          }
        }
        /* file descripter 0 treated as empty slot */
        else polld[pos].fd = polld[pos].events = 0;
      }
    }

    polld[pos].revents = 0;
    --events;
  }
}

/*
 * Get or create the INFO from a MSG: or NOT:
 */
void getINFO(char *bufptr)
{
    char *ptr;

    *mesg.date = *mesg.time = '\0';
    if ((ptr = strstr(bufptr, " ###")))
    {
        getField(ptr, "DATE", mesg.date, "");
        getField(ptr, "TIME", mesg.time, "");
        getField(ptr, "NAME", mesg.name, NONAME);
        getField(ptr, "NMBR", mesg.nmbr, NONMBR);
        getField(ptr, "LINE", mesg.line, NOLINE);
        getField(ptr, "MTYPE", mesg.type, NOTYPE);
        *ptr = '\0';

    }
    else
    {
        /* fill in missing fields */
        strcpy(mesg.name, NONAME);
        strcpy(mesg.nmbr, NONMBR);
        strcpy(mesg.line, NOLINE);
        strcpy(mesg.type, NOTYPE);
    }
    userAlias(mesg.nmbr, mesg.name, mesg.line);

    if (!*mesg.date || !*mesg.time)
    {
        /* no date and time, create both */
        ptr = strdate(NOSEP);
        strncpy(mesg.date, ptr, 8);
        mesg.date[8] = 0;
        strncpy(mesg.time, ptr + 9, 4);
        mesg.time[4] = 0;
    }
}

/*
 * Get a INFO field from a MSG: or NOT:
 */
void getField(char *bufptr, char *field_name, char *mesgptr, char *noval)
{
    char tmpbuf[BUFSIZ], *ptr;

    tmpbuf[BUFSIZ -1] = '\0';
    if ((ptr = strstr(bufptr, field_name)))
    {
        if (*(ptr + strlen(field_name) + 1) == '*') strncpy(mesgptr, noval, CIDSIZE - 1);
        else
        {
            strncpy(tmpbuf, ptr + strlen(field_name) + 1, BUFSIZ -1);
            ptr = strchr(tmpbuf, '*');
            if (ptr) *ptr = '\0';
            strncpy(mesgptr, tmpbuf, CIDSIZE - 1);
        }
    }
    else strncpy(mesgptr, noval, CIDSIZE - 1);
}

/*
 * Format of data from modem:
 *
 *      DATE = 0330            -or-  DATE=0330
 *      TIME = 1423            -or-  TIME=1423
 *      NMBR = 4075551212      -or-  NMBR=4075551212
 *      NAME = WIRELESS CALL   -or-  NAME=WIRELESS CALL
 *
 * kortex Modems  France Telecom:
 *      CID RING
 *
 *      CALLING MSG
 *
 *      DATE TIME=11/24 22:10
 *
 *      NBR=0632727751
 *
 *      END MSG
 *
 * Format of EXTRA line passed to TCP/IP clients by python cidd (not used)
 *
 *      EXTRA: *DATE*0330*TIME*1423*NUMBER*4075551212*MESG*NONE*NAME*WIRELESS CALL*
 *
 * Format of a CALL line passed to TCP/IP clients by ncidd:
 *      where CALL is one of: CID OUT HUP BLK WID PID
 *
 *      CALL: *DATE*03302002*TIME*1423*LINE*-*NMBR*4075551212*MESG*NONE*NAME*CALL*
 *
 * Format of a MESG line passed to TCP/IP clients by ncidd:
 *      where MESG is one of: MSG NOT
 *
 *      MESG: <mesg> ***DATE*<date>*TIME*<time>*NAME*<name>*NMBR*<nmbr>*LINE*<line>*MTYPE*<type>*
 *
 * Format of data from NetCallerID for three types of calls and a message
 *
 *      ###DATE03301423...NMBR4075551212...NAMEWIRELESS CALL+++\r
 *      ###DATE03301423...NMBR...NAME-UNKNOWN CALLER-+++\r
 *      ###DATE03301423...NMBR...NAME+++\r
 *      ###DATE...NMBR...NAME   -MSG OFF-+++\r
 *
 * Format of data from serial TCI multi-pots-line devices. Should also
 * work for serial Whozz Calling devices that have the output format
 * (usually switch 7) set to ON for TCI.
 *
 * Fixed field format, should always 70 characters per line.
 *
 *                1         2         3         4         5         6         
 *      0123456789012345678901234567890123456789012345678901234567890123456789
 *      01      9/03      2:00 PM            PRIVATE                          
 *      02      9/03      2:15 PM            PRIVATE           PRIVATE        
 *      03      9/03      2:23 PM       407-555-1212           WIRELESS CALLER
 *
 * Gateway CALL Line Format:
 *      where type is one of: IN OUT PID BLK HUP WID
 *
 *      CALL: ###DATEmmddhhss...CALLtype...LINEidentifier...NMBRnumber...NAMEwords+++\r
 */

void formatCID(char *buf)
{
    char cidbuf[BUFSIZ], msgbuf[BUFSIZ], tmpbuf[BUFSIZ];
    char *ptr, *sptr, *tptr, *linelabel, *nameptr;
    int i;
    time_t t;

    /*
     * At a RING
     * Could be "RING", "RING X" or "CID RING"
     *
     * US systems send Caller ID between the 1st and 2nd ring
     * Some non-US systems send Caller ID before 1st ring.
     *
     * If NAME, NUMBER, or DATE and TIME is not received, provide
     * the missing information.
     *
     * If generate Caller ID set and Caller ID not received, generate
     * a generic Caller ID at RING 2.
     *
     * Clear Caller ID info between rings.
     */
    if (strstr(buf, "RING"))
    {
        /*
         * If distinctive ring, save line indicator, it will be
         * "RING A", "RING B", etc.
         */
         if (strlen(buf) == 6) strncpy(cid.cidline, buf + 5, 1);

        /*
         * If ring information is wanted, send it to the clients.
         */
        if (sendinfo)
        {
            ++ring;
            sendInfo();
        }

        if (CIDALL3 == cid.status)
        {
            /*
            * date, time, and nmbr were received
            * indicate No NAME, and process
            */
            strncpy(cid.cidname, NONAME, CIDSIZE - 1);
            cid.status |= CIDNAME;
            logMsg(LEVEL4, "received date, time, nmbr\n");
        }
        else if (CIDALT3 == (cid.status & IGNHIGH))
        {
            /*
            * date, time, and name were received
            * mesg may have been received
            * indicate No Number, and process
            */
            strncpy(cid.cidnmbr, NONMBR, CIDSIZE - 1);
            cid.status |= CIDNMBR;
            sprintf(msgbuf, "received %s\n", cid.status == CIDALT3 ?
                "date, time, name" : "date, time, name mesg");
            logMsg(LEVEL4, msgbuf);
        }
        else if (cid.status == (CIDALL3 | CIDMESG))
        {
            /*
            * date, time, nmbr, and mesg were received
            * name is in the mesg field
            * copy mesg to name, and process
            */
            strncpy(cid.cidname, cid.cidmesg, CIDSIZE - 1);
            cid.status |= CIDNAME;
            logMsg(LEVEL4, "received date, time, nmbr, mesg\n");
        }
        else if (cid.status & (CIDNMBR | CIDNAME))
        {
            /*
             * number, name or both received but no date and time
             * add missing data and process
             *
             * name and number but no date and time should be caught
             * below at comparison to CIDNODT
             */
            if (!(cid.status & CIDNMBR))
            {
                strncpy(cid.cidnmbr, NONMBR, CIDSIZE - 1);
                cid.status |= CIDNMBR;
            }
            else if (!(cid.status & CIDNAME))
            {
                strncpy(cid.cidname, NONAME, CIDSIZE - 1);
                cid.status |= CIDNAME;
            }
            ptr = strdate(NOSEP);           /* returns: MMDDYYYY HHMM */
            strncpy(cid.ciddate, ptr, 8);   /*          0123456789012 */
            cid.ciddate[8] = '\0';
            strncpy(cid.cidtime, ptr + 9, 4);
            cid.cidtime[4] = '\0';
            cid.status |= (CIDDATE | CIDTIME);
        }
        else if (gencid && cidsent == 0 && ring == 2 )
        {
            /*
             * gencid = 1: generate a Caller ID if it is not received
             * gencid = 0: do not generate a Caller ID
             *
             * CID information always received between before RING 2
             * no CID information received, so create one.
             */
            ptr = strdate(NOSEP);     /* returns: MMDDYYYY HHMM */
            for(sptr = cid.ciddate; *ptr && *ptr != ' ';) *sptr++ = *ptr++;
            *sptr = '\0';
            for(sptr = cid.cidtime, ptr++; *ptr;) *sptr++ = *ptr++;
            *sptr = '\0';
            strncpy(cid.cidnmbr, "RING", CIDSIZE - 1);
            strncpy(cid.cidname, NOCID, CIDSIZE - 1);
            cid.status = (CIDDATE | CIDTIME | CIDNMBR | CIDNAME);
        }
        else
        {
            /*
             * CID not here yet or already processed
             * Make sure status and cidsent are zero
             */
            cid.status = cidsent = 0;
            return;
        }
    }

    /*
     * A Mac Mini Motorola Jump  modem sends "^PR^PX\n\n" before the CID
     * information.  It also sends "^P.^XNAME"
     */
     if ((ptr = strstr(buf, "\020R\020X")))
     {
        cid.status = cidsent = 0;
     }

    /* Process Caller ID information */
    if (strncmp(buf, "###", 3) == 0)
    {
        /*
         * Found a NetCallerID box, or a Gateway
         * All information received on one line
         * The Gateway creates a CID, HUP, OUT, PID Message Line
         * The Gateway contains a LINE and a CALL field
         * The NetCallerID box does not have a LINE or CALL field
         */
        logMsg(LEVEL4, "Detected NetCallerID or gateway format\n");

        /* Make sure the status field and cidsent are zero */
        cid.status = cidsent = 0;

        if ((ptr = strstr(buf, "DATE")))
        {
            if (*(ptr + 4) == '.')
            {
                /* no date and time, create both */
                ptr = strdate(NOSEP);
                strncpy(cid.ciddate, ptr, 8);
                cid.ciddate[8] = 0;
                cid.status |= CIDDATE;
                strncpy(cid.cidtime, ptr + 9, 4);
                cid.cidtime[4] = 0;
                cid.status |= CIDTIME;
            }
            else
            {
                strncpy(cid.cidtime, ptr + 8, 4);
                cid.cidtime[4] = 0;
                cid.status |= CIDTIME;

                strncpy(cid.ciddate, ptr + 4, 4);
                cid.ciddate[4] = 0;

                /* need to generate year */
                t = time(NULL);
                ptr = ctime(&t);
                *(ptr + 24) = 0;
                strncat(cid.ciddate, ptr + 20,
                        CIDSIZE - strlen(cid.ciddate) - 1);
                cid.status |= CIDDATE;
            }
        }

        /*
         * this field is only from a Gateway
         * will be either CALLIN, CALLOUT, CALLHUP, CALLBLK, CALLPID, CALLWID
         * CALLIN is the default
         */
        if ((ptr = strstr(buf, CALLOUT)))
        {
             calltype = OUT; /* this is a outgoing call*/
        }
        else if ((ptr = strstr(buf, CALLHUP)))
        {
            calltype = HUP; /* this is a blacklisted call hangup*/
        }
        else if ((ptr = strstr(buf, CALLBLK)))
        {
            calltype = BLK; /* this is a blocked call */
        }
        else if ((ptr = strstr(buf, CALLPID)))
        {
            calltype = PID; /* this is a call from a smart phone */
        }
        else if ((ptr = strstr(buf, CALLWID)))
        {
            calltype = WID; /* this is call waiting */
        }

        if ((ptr = strstr(buf, "LINE")))
        {
            /* this field is only from a Gateway */
            if (*(ptr + 5) == '.') strncpy(cid.cidline, lineid, CIDSIZE - 1);
            else
            {
                strncpy(cid.cidline, ptr + 4, CIDSIZE -1);
                ptr = strchr(cid.cidline, '.');
                if (ptr) *ptr = 0;
            }
        }

        if ((ptr = strstr(buf, "NMBR")))
        {
            if (*(ptr + 5) == '.') strncpy(cid.cidnmbr, NONMBR, CIDSIZE - 1);
            else
            {
                strncpy(cidbuf, ptr + 4, BUFSIZ -1);
                ptr = strchr(cidbuf, '.');
                if (ptr) *ptr = 0;
                builtinAlias(cid.cidnmbr, cidbuf);
            }
            cid.status |= CIDNMBR;
        }

        if ((ptr = strstr(buf, "NAME")))
        {
            if (*(ptr + 5) == '+') strncpy(cid.cidname, NONAME, CIDSIZE - 1);
            else
            {
                strncpy(cidbuf, ptr + 4, BUFSIZ -1);
                ptr = strchr(cidbuf, '+');
                if (ptr) *ptr = 0;
                builtinAlias(cid.cidname, cidbuf);
            }
            cid.status |= CIDNAME;
        }
    }
    else if (strlen(buf) == 70 && isdigit((int) *buf) && *(buf + 9) == '/' && *(buf + 24) == 'M')
    {
       /* 
        *
        * Found a TCI serial device
        * All information received on one line
        * Only inbound calls
        * No MESG field
        */
        logMsg(LEVEL4, "Detected TCI serial device format\n");

        /* Make sure the status field and cidsent are zero */
        cid.status = cidsent = 0;

        /* copy data to working buffer and init pointer */
        strncpy(cidbuf, buf, BUFSIZ - 1);
        ptr = cidbuf;
        
       /*
        * TELEPHONE LINE
        * positions: 0-1
        */
        *(ptr + 2) = '\0';
        if (strncmp(lineid,"-",1) == 0) strncpy(cid.cidline, ptr, CIDSIZE - 1);
        
        /*
         * MONTH
         * positions 7-8
         * force leading zero
         */
        *(ptr + 9) = '\0';
        sprintf(cid.ciddate, "%02d", atoi(ptr + 7));

        /*
         * DAY
         * positions 10-11
         */
        *(ptr + 12) = '\0';
        strcat(cid.ciddate, ptr + 10);

        /* need to generate year */
        t = time(NULL);
        tptr = ctime(&t);
        *(tptr + 24) = 0;
        strcat(cid.ciddate, tptr + 20);
        cid.status |= CIDDATE;
        
        /*
         * HOUR
         * positions 17-18
         * AM/PM positions 23-24
         */
        *(ptr + 19) = '\0';
        i = atoi(ptr + 17);
        if (strncmp(ptr + 23, "PM", 2) == 0) { if (i < 12) i = i + 12; }
        else if (i == 12) i = 0;
        sprintf(cid.cidtime, "%02d", i);

        /*
         * MINUTE
         * positions 20-21
         */
        *(ptr + 22) = '\0';
        strcat(cid.cidtime, ptr + 20);
        cid.status |= CIDTIME;
        
        /*
         * NMBR
         * positions 29-43
         * right justified (15 characters)
         */
        *(ptr + 44) = '\0';
        sptr = trimWhitespace(ptr + 29, ptr + 43);
        if (isdigit((int) *sptr))
        {
            /* remove hyphens from a number, builtinAlias() not needed */
            tptr = tmpbuf;
            while(*sptr)
            {
                if (*sptr != '-') *tptr++ = *sptr++;
                else ++sptr;
            }
            *tptr = '\0';
            strncpy(cid.cidnmbr, tmpbuf, CIDSIZE - 1);
        }
        else builtinAlias(cid.cidnmbr, sptr);
        cid.status |= CIDNMBR;
        
        /*
         * NAME
         * positions 55-69
         * left justified (15 characters)
         */
        *(ptr + 54) = '\0';
        sptr = trimWhitespace(ptr + 55, ptr + 69);
        if (*sptr) builtinAlias(cid.cidname, ptr + 55);
        else strncpy(cid.cidname, NONAME, CIDSIZE - 1);
        cid.status |= CIDNAME;
    }
    else if (strncmp(buf, "DATE", 4) == 0)
    {
        if ((ptr = strchr(buf, '='))) ++ptr;
        else ptr = buf + 7; /* this should never happen */
        if (*ptr == ' ') ++ptr;

        /* NetTalk sends no date but modem has line: "DATE=" */
        if (*ptr)
        {
            if (strstr(buf, "TIME"))
            {
                /* DATE TIME=11/24 22:10 */
                strncpy(cid.ciddate, ptr, 2);
                strncpy(cid.ciddate + 2, ptr + 3, 2);
                cid.ciddate[4] = '\0';
                strncpy(cid.cidtime, ptr + 6, 2);
                strncpy(cid.cidtime + 2, ptr + 9, 3);
                cid.status |= CIDTIME;
            }
            else
            {
                strncpy(cid.ciddate, ptr, CIDSIZE - 1);
            }

            /* need to generate year */
            t = time(NULL);
            ptr = ctime(&t);
            *(ptr + 24) = 0;
            strncat(cid.ciddate, ptr + 20, CIDSIZE - strlen(cid.ciddate) - 1);
            cid.status |= CIDDATE;
        }
        cidsent = 0;
    }
    else if (strncmp(buf, "TIME", 4) == 0)
    {
        if ((ptr = strchr(buf, '='))) ++ptr;
        else ptr = buf + 7; /* this should never happen */
        if (*ptr == ' ') ++ptr;
        if (*ptr)
        {
            /* NetTalk sends no time but modem has line: "TIME=" */
            strncpy(cid.cidtime, ptr, CIDSIZE - 1);
            cid.status |= CIDTIME;
        }
        cidsent = 0;
    }
    /*
     * Some modems send DDN_NMBR, DDN, or NBR instead of NMBR.
     * This will catch the 4 cases.
     */
    else if (!strncmp(buf, "NMBR", 4) || !strncmp(buf, "DDN", 3) ||
             !strncmp(buf, "NBR", 3))
    {
        /* some telcos send NMBR = ##########, then NMBR = O to mask it */
        if (!(cid.status & CIDNMBR))
        {
            if ((ptr = strchr(buf, '='))) ++ptr;
            else while (*ptr && !isblank((int) *ptr)) ++ptr; /* this should never happen */
            if (*ptr == ' ') ++ptr;
            builtinAlias(cid.cidnmbr, ptr);
            cid.status |= CIDNMBR;
            cidsent = 0;
        }
        if (cidnoname)
        {
            /*
             * CIDNAME optional on some systems
             * if ncidd.conf set cidnoname then set
             * cid.cidname to NONAME to get response
             * before ring 2
             *
             * WARNING, if CIDNAME sent this make it NONAME
             * be sure cidnoname is needed in ncidd.conf
             */
            cid.status |= CIDNAME;
            strncpy(cid.cidname, NONAME, CIDSIZE - 1);
        }
    }
    /*
     * Using strstr() instead of strncmp() because a Mac
     * Mini Jump modem sent '^P.^PXNAME' instead of 'NAME'.
     * At this point the string was converted to '?.?XNAME'
     */
    else if ((ptr = strstr(buf, "NAME")))
    {
        /*
         * if cidnoname set, ignore NAME or
         * if NAME already received, discard the second one
         */
        if (!cidnoname && !(cid.status & CIDNAME))
        {
            /* remove any trailing spaces */
            for (sptr = buf; *sptr; ++sptr);
            for (--sptr; *sptr && *sptr == ' '; --sptr) *sptr = 0;

            if (*(ptr + 4) == '=') ptr += 5;
            else ptr += 7;
            if (*ptr == ' ') ++ptr;
            builtinAlias(cid.cidname, ptr);
            cid.status |= CIDNAME;
            cidsent = 0;
        }
    }
    else if (strncmp(buf, "MESG", 4) == 0)
    {
        ptr = buf;
        if (*(ptr + 4) == '=') ptr += 5;
        else ptr += 7;
        if (*ptr == ' ') ++ptr;
        strncpy(cid.cidmesg, ptr, CIDSIZE - 1);
        cid.status |= CIDMESG;
        cidsent = 0;
    }

    if (cid.status == CIDNODT || cid.status == (CIDNODT | CIDMESG))
    {
        /*
         * number and name received but no date and time
         * assumes date and time sent before nmbr and name
         * mesg may have been received
         */

        logMsg(LEVEL4,"No date and time found -- using current\n");
        ptr = strdate(NOSEP);           /* returns: MMDDYYYY HHMM */
        strncpy(cid.ciddate, ptr, 8);   /*          0123456789012 */
        cid.ciddate[8] = '\0';
        strncpy(cid.cidtime, ptr + 9, 4);
        cid.cidtime[4] = '\0';
        cid.status |= (CIDDATE | CIDTIME);
    }

    if (CIDALL4 == (cid.status & IGNHIGH))
    {
        /*
         * All Caller ID or outgoing call information received.
         *
         * Create the CID (Caller ID), OUT (outgoing call),
         * HUP (hungup call), or BLK (call bloackd) text line.
         *
         * For OUT text lines (outgoing calls):
         *     the MESG field is not used
         *     the NAME field will be generic if no alias
         *
         * For HUP server generated text lines (hungup call):
         *     the CID label is replaced by a HUP label
         */

       sprintf(msgbuf, "received %s\n", cid.status == CIDALL4 ?
           "date, time, nmbr, name" : "date, time, nmbr, name, mesg");
       logMsg(LEVEL4, msgbuf);

        userAlias(cid.cidnmbr, cid.cidname, cid.cidline);

        switch(calltype)
        {
            case CID:
                linelabel = CIDLINE;
                break;
            case OUT:
                linelabel = OUTLINE;
                break;
            case HUP:
                linelabel = HUPLINE;
                break;
            case BLK:
                linelabel = BLKLINE;
                break;
            case PID:
                linelabel = PIDLINE;
                break;
            case WID:
                linelabel = WIDLINE;
                break;
            default: /* should not happen */
                linelabel = CIDLINE;
                break;
        }
        nameptr = cid.cidname;
        if (hangup && linelabel == &CIDLINE[0])
        {
            /*
             * hangup phone
             * if a CID call and if on blacklist but not whitelist
             */
            if (doHangup(cid.cidname, cid.cidnmbr))
            {
                linelabel = HUPLINE;
                if (strlen(listname)) nameptr = listname;
            }
            else if (wflag && strlen(listname)) nameptr = listname;
        }

        sprintf(cidbuf, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
            linelabel,
            DATE, cid.ciddate,
            TIME, cid.cidtime,
            LINE, cid.cidline,
            NMBR, cid.cidnmbr,
            MESG, cid.cidmesg,
            NAME, nameptr,
            STAR);

        /* Log the CID, OUT, or HUP text line */
        writeLog(cidlog, cidbuf);

        /*
         * Send the CID, OUT, or HUP text line to clients
         */
        writeClients(cidbuf);

        /*
         * Reset mesg, line, and status
         * Set sent indicator
         * Reset call out indicator if it was set
         */
        strncpy(cid.cidmesg, NOMESG, CIDSIZE - 1); /* default message */
        strcpy(cid.cidline, lineid); /* default line indicator */
        strcpy(infoline, lineid); /* default line indicator */
        cid.status = 0;
        cidsent = 1;
        if (calltype) calltype = 0;
    }
}

/*
 * remove whitespace from the start and end of a string
 */
char *trimWhitespace(char *sptr, char *eptr)
{
    while (*sptr && isspace((int) *sptr)) ++sptr;
    while (*eptr && isspace((int) *eptr)) *eptr-- = '\0';

return sptr;
}

/*
 * Send string to all TCP/IP CID clients.
 */

void writeClients(char *inbuf)
{
    int pos, ret;
    char buf[BUFSIZ];

    (void) ret;

    strcat(strcpy(buf, inbuf), CRLF);
    for (pos = 0; pos < MAXCONNECT; ++pos)
    {
        if (polld[pos].fd == 0 || polld[pos].fd == ttyfd ||
            polld[pos].fd == mainsock)
            continue;
        ret = write(polld[pos].fd, buf, strlen(buf));
    }
}

/*
 * Send log, if log file exists.
 */

void sendLog(int sd, char *logbuf)
{
    struct stat statbuf;
    char **ptr, *iptr, *optr, input[BUFSIZ], msgbuf[BUFSIZ];
    FILE *fp;
    int ret, len;

    if (stat(cidlog, &statbuf) == 0)
    {
        if ((long unsigned int) statbuf.st_size > cidlogmax)
        {
            sprintf(logbuf, LOGMSG, (long unsigned int) statbuf.st_size,
                    cidlogmax, strdate(WITHSEP), CRLF);
            ret = write(sd, logbuf, strlen(logbuf));
            sprintf(msgbuf, LOGMSG, (long unsigned int) statbuf.st_size,
                    cidlogmax, strdate(WITHSEP), NL);
            logMsg(LEVEL1, msgbuf);
            sprintf(msgbuf, "%s%s", NOLOGSENT, CRLF);
            ret = write(sd, msgbuf, strlen(msgbuf));
            return;
        }
    }

    if ((fp = fopen(cidlog, "r")) == NULL)
    {
        sprintf(msgbuf, "%s%s", NOLOG, CRLF);
        ret = write(sd, msgbuf, strlen(msgbuf));
        sprintf(msgbuf, "cidlog: %d %s [%s]\n", errno, strerror(errno), strdate(ONLYTIME));
        logMsg(LEVEL6, msgbuf);
        return;
    }

    /*
     * read each line of file, one line at a time
     * add "LOG" to line tag (CID: becomes CIDLOG:)
     * send line to clients
     */
    iptr = 0;
    sprintf(msgbuf, "Begin: Send call log: %s [%s]\n", cidlog, strdate(ONLYTIME));
    logMsg(LEVEL4, msgbuf);

    while (fgets(input, BUFSIZ - sizeof(LINETYPE), fp) != NULL)
    {
        /* strip <CR> and <LF> */
        if ((iptr = strchr( input, '\r')) != NULL) *iptr = 0;
        if ((iptr = strchr( input, '\n')) != NULL) *iptr = 0;

        optr = logbuf;
        iptr = input;
        if (strstr(input, ": ") != NULL)
        {
            /* possible line tag found */
            for(ptr = lineTags; *ptr; ++ptr)
            {
                if (!strncmp(input, *ptr, strlen(*ptr)))
                {
                    /* copy line tag, skip ": " */
                    for(iptr = input; *iptr != ':';) *optr++ = *iptr++;
                    iptr += 2;
                    break;
                }
            }
        }
        /*
         * if line "<label>: " found, line begins with "<label>LOG: "
         * if line label not found, line begins with "LOG: "
         */
        strcat(strncat(strcpy(optr, LOGLINE), iptr, BUFSIZ - (iptr - input - 1)), CRLF);
        len = strlen(logbuf);
        ret = write(sd, logbuf, len);

        optr = logbuf;
        while ((ret != -1 && ret != len) ||
               (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)))
        {
            /* short write or resource busy, need to attempt subsequent write */

            if (ret != -1 && ret != len)
            {
               /* short write, */
               len -= ret;
               optr += ret;
            }

            /* short delay before trying to rewrite line or rest of line */
            usleep(READWAIT);
            ret = write(sd, optr, len);
        }

        if (ret == -1)
        {
            /* write error */
            sprintf(msgbuf, "sending log: %d %s\n", errno, strerror(errno));
            logMsg(LEVEL1, msgbuf);
        }
    }

    (void) fclose(fp);

    /* Determine if a Call Log was sent */
    if (iptr)
    {
        /* Indicate end of the Call Log */
        sprintf(msgbuf, "%s%s", LOGEND, CRLF);
        ret = write(sd, msgbuf, strlen(msgbuf));
        sprintf(msgbuf, "Sent call log: %s\n", cidlog);
        logMsg(LEVEL3, msgbuf);
    }
    else
    {
        sprintf(msgbuf, "%s%s", EMPTYLOG, CRLF);
        ret = write(sd, msgbuf, strlen(msgbuf));
        sprintf(msgbuf, "Call log empty: %s\n", cidlog);
        logMsg(LEVEL3, msgbuf);
    }
    sprintf(msgbuf, "End: Send call log: %s [%s]\n", cidlog, strdate(ONLYTIME));
    logMsg(LEVEL4, msgbuf);
}

/*
 * Write log, if logfile exists.
 */

void writeLog(char *logf, char *logbuf)
{
    int logfd, ret;
    char msgbuf[BUFSIZ];

    (void) ret;

    /* write to server log */
    sprintf(msgbuf, "%s\n", logbuf);
    logMsg(LEVEL3, msgbuf);

    if ((logfd = open(logf, O_WRONLY | O_APPEND)) < 0)
    {
        sprintf(msgbuf, "%s: %s\n", logf, strerror(errno));
        logMsg(LEVEL6, msgbuf);
    }
    else
    {
        /* write log entry */
        sprintf(msgbuf, "%s\n", logbuf);
        ret = write(logfd, msgbuf, strlen(msgbuf));
        close(logfd);
    }
}

/*
 * Send call information
 *
 * Format of CIDINFO line passed to TCP/IP clients by ncidd:
 *
 * CIDINFO: *LINE*<label>*RING*<number>*TIME<hh:rr:mm>
 */

void sendInfo()
{
    char buf[BUFSIZ];

    userAlias("", "", infoline);
    sprintf(buf, "%s%s%s%s%d%s%s%s",CIDINFO, LINE, infoline, \
            RING, ring, TIME, strdate(ONLYTIME), STAR);
    writeClients(buf);

    strcat(buf, NL);
    logMsg(LEVEL3, buf);
}

/*
 * Returns the current date and time as a string in the format:
 *      WITHSEP:     MM/DD/YYYY HH:MM:SS
 *      NOSEP:       MMDDYYYY HHMM
 *      ONLYTIME:    HH:MM:SS
 *      ONLYYEAR:    YYYY
 *      LOGFILETIME: HH:MM:SS.ssss
 */
char *strdate(int separator)
{
    static char buf[BUFSIZ];
    struct tm *tm;
    struct timeval tv;

    (void) gettimeofday(&tv, 0);
    tm = localtime((time_t *) &(tv.tv_sec));
    if (separator & WITHSEP)
        sprintf(buf, "%.2d/%.2d/%.4d %.2d:%.2d:%.2d", tm->tm_mon + 1,
            tm->tm_mday, tm->tm_year + 1900, tm->tm_hour, tm->tm_min,
            tm->tm_sec);
    else if (separator & NOSEP)
        sprintf(buf, "%.2d%.2d%.4d %.2d%.2d", tm->tm_mon + 1, tm->tm_mday,
            tm->tm_year + 1900, tm->tm_hour, tm->tm_min);
    else if (separator & ONLYTIME)
        sprintf(buf, "%.2d:%.2d:%.2d",  tm->tm_hour, tm->tm_min, tm->tm_sec);
    else if (separator & ONLYYEAR) sprintf(buf, "%.4d", tm->tm_year + 1900);
    else  /* LOGFILETIME */
        sprintf(buf, "%.2d:%.2d:%.2d.%.4ld",  tm->tm_hour, tm->tm_min,
                tm->tm_sec, (long int) tv.tv_usec / 100);
    return buf;
}

/*
 * if PID file exists, and PID in process table, ERROR
 * if PID file exists, and PID not in process table, replace PID file
 * if no PID file, write one
 * if write a pidfile failed, OK
 * If pidfile == 0, do not write PID file
 */
int doPID()
{
    struct stat statbuf;
    char msgbuf[BUFSIZ];
    FILE *pidptr;
    pid_t curpid, foundpid = 0;
    int ret;

    /* if pidfile == 0, no pid file is wanted */
    if (pidfile == 0)
    {
        logMsg(LEVEL1, "Not using PID file, there was no '-P' option.\n");
        return 0;
    }

    /* check PID file */
    curpid = getpid();
    if (stat(pidfile, &statbuf) == 0)
    {
        if ((pidptr = fopen(pidfile, "r")) == NULL) return(1);
        ret = fscanf(pidptr, "%u", &foundpid);
        fclose(pidptr);
        if (foundpid) ret = kill(foundpid, 0);
        if (ret == 0 || (ret == -1 && errno != ESRCH)) return(1);
        sprintf(msgbuf, "Found stale pidfile: %s\n", pidfile);
        logMsg(LEVEL1, msgbuf);
    }

    /* create logfile */
    if ((pidptr = fopen(pidfile, "w")) == NULL)
    {
        sprintf(msgbuf, "Cannot write %s: %s\n", pidfile, strerror(errno));
        logMsg(LEVEL2, msgbuf);
    }
    else
    {
        pid = curpid;
        fprintf(pidptr, "%d\n", pid);
        fclose(pidptr);
        sprintf(msgbuf, "Wrote pid %d in pidfile: %s\n", pid, pidfile);
        logMsg(LEVEL1, msgbuf);
    }

    return(0);
}

/*
 * Check if lockfile present and has an active process number in it
 * ret == 0 if lockfile is not present or lockfile has stale process number
 * ret == 1 if lockfile present and process number active, or error
 */

int CheckForLockfile()
{
    int kret, ret = 0, lockp;
    static unsigned int sentmsg = 0;
    FILE *fp;
    char lockbuf[BUFSIZ];
    char msgbuf[BUFSIZ];
    struct stat statbuf;

    if (lockfile != 0)
    {
        if (stat(lockfile, &statbuf) == 0)
        {
            ret = 1;
            if ((fp = fopen(lockfile, "r")) == NULL)
            {
                if (!(sentmsg & 1))
                {
                    sprintf(msgbuf, "%s: %s\n", lockfile, strerror(errno));
                    logMsg(LEVEL1, msgbuf);
                    sentmsg |= 1;
                }
            }
            else
            {
                if (fgets(lockbuf, BUFSIZ - 1, fp) != NULL)
                {
                    lockp = atoi(lockbuf);
                    if (lockp)
                    {
                        /* lockfile contains a process number */
                        kret = kill(lockp, 0);
                        if (kret && errno != EPERM)
                        {
                            /* the error is not permission denied */
                            if (unlink(lockfile))
                            {
                                if (!(sentmsg & 2))
                                {
                                    sprintf(msgbuf,
                                        "Failed to remove stale lockfile: %s\n",
                                        lockfile);
                                    logMsg(LEVEL1, msgbuf);
                                    sentmsg |= 2;
                                }
                            }
                            else
                            {
                                sprintf(msgbuf, "Removed stale lockfile: %s\n",
                                        lockfile);
                                logMsg(LEVEL1, msgbuf);
                                ret = 0;
                            }
                        }
                    }
                    fclose(fp);
                }
            }
        }
    }

/* clear all locally latched failures if lockfile no longer present */
if (ret == 0) sentmsg = 0;

return ret;
}

/*
 * Close all file descriptors and restore tty parameters.
 */

void cleanup()
{
    int pos;
    char msgbuf[BUFSIZ];

    /* restore tty parameters */
    if (ttyfd > 2)
    {
        tcflush(ttyfd, TCIOFLUSH);
        tcsetattr(ttyfd, TCSANOW, &otty);
    }

    /* close open files */
    for (pos = 0; pos < MAXCONNECT; ++pos)
        if (polld[pos].fd != 0) close(polld[pos].fd);

    /* remove pid file, if it was created */
    if (pid)
    {
        unlink(pidfile);
        sprintf(msgbuf, "Removed pidfile: %s\n", pidfile);
        logMsg(LEVEL1, msgbuf);
    }

    /* 
     * send hangup to the modem if hangup option given
     * this makes sure modem hangups up if off-hook when ncidd dies
     */
    if (hangup) (void) initModem(HANGUP, 0);

    /* close log file, if open */
    if (logptr) fclose(logptr);
}

/* signal exit */
void finish(int sig)
{
    char msgbuf[BUFSIZ];

    sprintf(msgbuf, "Received Signal %d: %s\nTerminated: %s\n",
            sig, strsignal(sig), strdate(WITHSEP));
    logMsg(LEVEL1, msgbuf);

    cleanup();

    /* allow signal to terminate the process */
    signal (sig, SIG_DFL);
    raise (sig);
}

/*
 * reload signal SIGHUP
 *
 * reload alias, blacklist, and whitelist files
 */
void reload(int sig)
{
    char msgbuf[BUFSIZ];
    
    sprintf(msgbuf,
      "Received Signal %d: %s\nBegin: Reloading alias, blacklist, and whitelist files [%s]\n",
      sig, strsignal(sig), strdate(WITHSEP));
    logMsg(LEVEL1, msgbuf);

    /*
     * Decided not to do a reconfig because it seems like too much work
     * for a small gain:
     *   - open configurable files need to be closed and opened again
     *   - configuration verbose indicators, need to be output again
     * if (doConf()) errorExit(-104, 0, 0);
     */

    /* remove existing aliases to free memory used */
    rmaliases();

    /* reload alias file, but quit on error */
    if (doAlias()) errorExit(-109, 0, 0);

    /* remove existing blacklist entries to free memory used */
    rmEntries(&blkHead, &blkCurrent);

    /* reload blacklist file but quit on error */
    if (doList(blacklist, &blkHead, &blkCurrent)) errorExit(-114, 0, 0);

    /* remove existing whitelist entries to free memory used */
    rmEntries(&whtHead, &whtCurrent);

    /* reload whitelist file but quit on error */
    if (doList(whitelist, &whtHead, &whtCurrent)) errorExit(-114, 0, 0);
    
    sprintf(msgbuf,
      "End: Reloaded alias, blacklist, and whitelist files [%s]\n", strdate(ONLYTIME));
    logMsg(LEVEL1, msgbuf);

}

/*
 * new CID call log signal
 *
 * replace cidcall.log file with cidcall.log.new
 */
void update_cidcall_log (int sig)
{
    char msgbuf[BUFSIZ];

    sprintf (msgbuf,
      "Received Signal %d: %s\nReplacing %s with %s.new: %s\n", sig,
      strsignal(sig), cidlog, cidlog, strdate(WITHSEP));
    logMsg(LEVEL1, msgbuf);
    /*
     * can't replace log file now because it may be in the process of
     * being written to.  Set the flag value so that it can be updated
     * when it is safe to do so.
     */
    update_call_log = 1;
}

/* signal show connected clients */
void showConnected(int sig)
{
    int pos;
    char msgbuf[BUFSIZ];

    sprintf(msgbuf, "Received Signal %d: %s at %s\n",
            sig, strsignal(sig), strdate(WITHSEP));
    logMsg(LEVEL1, msgbuf);

    for (pos = 0; pos < MAXCONNECT; ++pos)
    {
        if (polld[pos].fd == 0 || polld[pos].fd == ttyfd ||
            polld[pos].fd == mainsock)
            continue;
            
        sprintf(msgbuf, "Client %5d pos %5d from %s%s is connected\n", 
            polld[pos].fd, pos, IPinfo[pos].addr, IPinfo[pos].name);
        logMsg(LEVEL1, msgbuf);
    }
}
    

/* ignored signals */
void ignore(int sig)
{
    char msgbuf[BUFSIZ];

    sprintf(msgbuf, "Received Signal %d: %s\nIgnored: %s\n",
            sig, strsignal(sig), strdate(WITHSEP));
    logMsg(LEVEL1, msgbuf);
}

int errorExit(int error, char *msg, char *arg)
{
    char msgbuf[BUFSIZ];

    if (error == -1)
    {
        /* should not happen */
        if (msg == 0) msg = "oops";

        /*
         * system error
         * print msg, arg should be zero
         */
        error = errno;
        sprintf(msgbuf, "%s: %s\n", msg, strerror(errno));
        logMsg(LEVEL1, msgbuf);
    }
    else
    {
        /* should not happen */
        if (msg != 0 && arg == 0) arg = "oops";

        /*
         * internal program error
         * print msg and arg if both are not 0
         */
        if (msg != 0 && arg != 0)
        {
            sprintf(msgbuf, "%s: %s\n", msg, arg);
            logMsg(LEVEL1, msgbuf);
        }
    }

    /* do not print terminated message, or cleanup, if option error */
    if (error != -100 && error != -101 && error != -106 && error != -107 &&
        error != -108 && error != -113)
    {
        sprintf(msgbuf, "Terminated:  %s\n", strdate(WITHSEP));
        logMsg(LEVEL1, msgbuf);
        cleanup();
    }

    exit(error);
}

/*
 * Normal exit
 */

void normalExit()
{
    char msgbuf[BUFSIZ];

    sprintf(msgbuf, "Terminated by Verbose Level 8:  %s\n", strdate(WITHSEP));
    logMsg(LEVEL1, msgbuf);
    cleanup();
    exit(0);
}

/*
 * hexdump from http://stackoverflow.com/a/29865 by user 'epatel' dated Aug 27 2008
 */
void hexdump(void *ptr, int buflen) {
  unsigned char *buf = (unsigned char*)ptr;
  char msgbuf[BUFSIZ];
  char tmpbuf[BUFSIZ];
  int i, j;
  for (i=0; i<buflen; i+=16) {
    sprintf(msgbuf,"%06x: ", i);
    for (j=0; j<16; j++) 
      if (i+j < buflen) {
        sprintf(tmpbuf,"%02x ", buf[i+j]);
        strcat(msgbuf,tmpbuf);
      } else
        strcat(msgbuf,"   ");
    strcat(msgbuf," ");
    for (j=0; j<16; j++) 
      if (i+j < buflen) {
        sprintf(tmpbuf,"%c", isprint(buf[i+j]) ? buf[i+j] : '.');
        strcat(msgbuf,tmpbuf);
      }
    strcat(msgbuf,NL);
    logMsg(HEXLEVEL,msgbuf);
  }
}

/*
 * log messages, and print messages in debug mode
 */
void logMsg(int level, char *message)
{
    /* write to stdout in debug mode */
    if (debug && verbose >= level) fputs(message, stdout);

    /* write to logfile */
    if (logptr && verbose >= level)
    {
        fputs(message, logptr);
        fflush(logptr);
    }
}

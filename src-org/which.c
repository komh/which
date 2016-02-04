/*---------------------------------------------------------------------------

    which.c

    This program by default finds the path of an executable command under
    either OS/2 or MS-DOS.  It can also be used to find data files in the
    DPATH or DLLs in the LIBPATH (at least under OS/2).  It does not have
    a whole lot of error-checking, but the array sizes are reasonably gen-
    erous.

    To do:  add support for ksh, bash, etc.; allow wildcard commands?

    Copyright (c) 1993 by Greg Roelofs.  4OS2/4DOS spawnl() code by Michael
    D. Lawler.  DosQuerySysInfo() code by Kenneth Porter.  You may use this
    code for any purpose whatsoever, as long as you don't sell it and don't
    claim to have written it.  Not that you'd want to.

  ---------------------------------------------------------------------------*/

#define VERSION   "v2.1 of 28 July 1993"

/* #define DEBUG */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <process.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef TRUE
#  define TRUE   1
#  define FALSE  0
#endif

/* not used:
    #if defined(__OS2__) && !defined(OS2)
    #  define OS2
    #endif
 */

#if defined(__IBMC__)
#  define S_IFMT   0xF000   /* or (S_IFREG | S_IFDIR | S_IFCHR) */
#endif
#ifndef S_ISDIR
#  define S_ISDIR(m)   (((m) & S_IFMT) == S_IFDIR)
#endif

#if defined(__IBMC__) || defined(__EMX__) || defined(__WATCOMC__)
#  ifndef __32BIT__
#    define __32BIT__
#  endif
#  define INCL_DOSMISC       /* for IBM Toolkit headers */
#  define INCL_DOSERRORS     /* for IBM Toolkit headers */
#  define INCL_NOPMAPI       /* for IBM Toolkit headers */
#  include <os2.h>
#endif

#ifdef DEBUG
#  define Trace(x)   fprintf x
#else
#  define Trace(x)
#endif


int get_libpath(char **path);      /* our sole prototype... */


struct stat statbuf;


/* extensions must be in order of operating-system precedence! */

static char *ext_command[] = {     /* COMMAND extensions */
    ".com",
    ".exe",
    ".bat"
};

static char *ext_cmd[] = {         /* CMD extensions */
    ".com",
    ".exe",
    ".cmd",
    ".bat"
};

static char *ext_4dos[] = {        /* 4DOS extensions */
    ".com",
    ".exe",
    ".btm",
    ".bat"
};

static char *ext_4os2[] = {        /* 4OS2 extensions */
    ".com",
    ".exe",
    ".btm",
    ".cmd",
    ".bat"
};

static char *ext_libpath[] = {     /* LIBPATH extension(s) */
    ".dll"
};

static char *ext_dpath[] = {       /* DPATH extension(s) */
    ".boo",
    ".dat",
    ".inf",
    ".ini",   /* does this belong here? */
    ".hlp",
    ".msg",
    ".ndx"
};


/* OS/2 internal commands:  some of these are rarely used outside of
 * command files, but all of them can be called from the command line.
 * (I don't know if some of the OS/2-only commands might be in later
 * versions of MS-DOS.  One could use spawnl() and check the errors,
 * but that would be slow.)
 */

static char *int_command[] = {
    "call", "cd", "chdir", "cls", "copy", "date", "del", "dir", "echo",
    "erase", "exit", "for", "goto", "if", "md", "mkdir", "path", "pause",
    "prompt", "rd", "rem", "ren", "rename", "rmdir", "set", "shift",
    "time", "type", "ver", "verify", "vol"
};

static char *int_cmd[] = {

    /* note that extproc cannot be called from command line in 4OS2 */
    "chcp", "detach", "dpath", "endlocal", "extproc", "keys", "move",
    "setlocal", "start",

    /* all the rest are identical to int_command[] */
    "call", "cd", "chdir", "cls", "copy", "date", "del", "dir", "echo",
    "erase", "exit", "for", "goto", "if", "md", "mkdir", "path", "pause",
    "prompt", "rd", "rem", "ren", "rename", "rmdir", "set", "shift",
    "time", "type", "ver", "verify", "vol"
};


typedef struct shellinfo {
    char *name;
    char **extension;
    int num_ext;
    char **internal;
    int num_int;
} SHELLINFO;

/* these guys must match the order of shells[] */
#define _COMMAND   0
#define _CMD       1
#define _4DOS      2
#define _4OS2      3
#define _4OS2_16   4
#define _4OS2_32   5

SHELLINFO shells[] = {
    {"COMMAND.COM", ext_command,   sizeof(ext_command)/sizeof(char *),
                    int_command,   sizeof(int_command)/sizeof(char *) },
    {"CMD.EXE",     ext_cmd,       sizeof(ext_cmd)/sizeof(char *),
                    int_cmd,       sizeof(int_cmd)/sizeof(char *) },
    {"4DOS.COM",    ext_4dos,      sizeof(ext_4dos)/sizeof(char *),
                    (char **)NULL, 0 },
    {"4OS2.EXE",    ext_4os2,      sizeof(ext_4os2)/sizeof(char *),
                    (char **)NULL, 0 },
    {"4OS2-16.EXE", ext_4os2,      sizeof(ext_4os2)/sizeof(char *),
                    (char **)NULL, 0 },
    {"4OS2-32.EXE", ext_4os2,      sizeof(ext_4os2)/sizeof(char *),
                    (char **)NULL, 0 }
};

#define REGPATH    0   /* for pathtype */
#define LIBPATH    1
#define DPATH      2

char *prognam;


/****************/
/* Main program */
/****************/

int main(int argc, char *argv[])
{
    static char tempname[1024];
    char *p, *q, *comspec, *path, *pathvar, *dir[1000];
    int i, j, k, c, error=0, startdir=0;
    int sh, all=FALSE, pathtype=REGPATH, regpath, fourshell=FALSE;
    int numdirs=0, numshells=sizeof(shells)/sizeof(SHELLINFO);


/*---------------------------------------------------------------------------
    Parse the command line...
  ---------------------------------------------------------------------------*/

    prognam = argv[0];
    p = prognam - 1;
    while (*++p)
        *p = tolower(*p);   /* assumes "smart" tolower() */

    if (argc <= 1)
        --argc;
    else
        while (--argc > 0  &&  (*++argv)[0] == '-')
            while ((c = *++(argv[0])) != '\0')
                switch (c) {
                    case 'a':             /* list all matches */
                        all = TRUE;
                        break;
                    case 'l':             /* search for DLLs */
                        pathtype = LIBPATH;
                        break;
                    case 'd':             /* search for data files */
                        pathtype = DPATH;
                        break;
                    default:
                        ++error;
                        break;
                } /* end switch, while, while, if */

/*---------------------------------------------------------------------------
    Print usage if any errors or if no arguments.
  ---------------------------------------------------------------------------*/

    if (error || (argc <= 0)) {
#ifdef TRADITIONAL
        fprintf(stderr, "%s: too few arguments\n", prognam);
#else
        fprintf(stderr, "\n\
which (%s) for OS/2 %s, from Newtware\n\n\
usage:  %s [ -a ] [ -l | -d ] cmd [ cmd ... ]\n\
(default behavior is to find location of program executed as \"cmd\")\n\
  -a  list all matches, not just first\n\
  -l  search directories in LIBPATH for DLLs%s\n\
  -d  search directories in DPATH for data files%s\n", VERSION,
#ifdef __32BIT__
          "2.x", prognam, "", "");
#else
          "and MS-DOS", prognam, " (OS/2)", " (OS/2)");
#endif /* ?__32BIT__ */
#endif /* ?TRADITIONAL */
        exit(1);
    }

/*---------------------------------------------------------------------------
    Try to figure out what shell we're in, based on COMSPEC.
  ---------------------------------------------------------------------------*/

    if ((comspec = getenv("COMSPEC")) == (char *)NULL || *comspec == '\0') {
        Trace((stderr, "COMSPEC is empty...assuming COMSPEC = cmd.exe\n"));
        sh = _CMD;
    } else {
        Trace((stderr, "COMSPEC = %s\n", comspec));
        p = strrchr(comspec, '\\');
        if (p == NULL)
            p = comspec;
        else
            ++p;
        for (i = 0;  i < numshells;  ++i) {
            if (stricmp(p, shells[i].name) == 0) {
                sh = i;
                break;
            }
        }
        if (i == numshells) {
            sh = _CMD;
            fprintf(stderr,
              "%s: unknown command shell \"%s\"\n%s: assuming %s\n",
              prognam, comspec, prognam, shells[sh].name);
        }
        if (sh >= 2 && sh <= 5)   /* 4DOS, 4OS2, 4OS2-16, 4OS2-32 */
            fourshell = TRUE;
    }
    Trace((stderr, "shell is %s\n\n", shells[sh].name));

/*---------------------------------------------------------------------------
    Get the PATH, DPATH or LIBPATH, depending on the user's wishes.
  ---------------------------------------------------------------------------*/

    /* for regular path, current directory is always implied; not for others */
    switch (pathtype) {
        case REGPATH:
            pathvar = "PATH";
            regpath = TRUE;
            dir[numdirs++] = ".";
            path = getenv(pathvar);
            break;
        case DPATH:
            pathvar = "DPATH";
            regpath = FALSE;
            shells[sh].extension = ext_dpath;
            shells[sh].num_ext = sizeof(ext_dpath)/sizeof(char *);
            path = getenv(pathvar);
            break;
        case LIBPATH:
            pathvar = "LIBPATH";
            regpath = FALSE;
            shells[sh].extension = ext_libpath;
            shells[sh].num_ext = sizeof(ext_libpath)/sizeof(char *);
            get_libpath(&path);
            break;
    }
    Trace((stderr, "COMSPEC now = %s\n", comspec));

/*---------------------------------------------------------------------------
    Terminate the path elements and store pointers to each one.
  ---------------------------------------------------------------------------*/

    if (path == (char *)NULL || *path == '\0') {
        Trace((stderr, "\n%s is empty\n\n", pathvar));
        if (!regpath) {
            fprintf(stderr, "%s: %s is empty\n", prognam, pathvar);
            exit(2);
        }
    } else {
        Trace((stderr, "\n%s = %s\n\n", pathvar, path));
        if (*path != ';')
            dir[numdirs++] = path;
        p = path - 1;
        while (*++p)
            if (*p == ';') {
                *p = '\0';
                if ((p[1] != '\0') && (p[1] != ';'))
                    dir[numdirs++] = p + 1;
            } else
                *p = tolower(*p);  /* should probably make this an option... */
    }

/*---------------------------------------------------------------------------
    If we're doing a normal PATH search under 4OS2 or 4DOS, check the path
    for a "." entry; if find one, ignore the extra "." entry which was previ-
    ously inserted at the beginning of the dir[] array.  (Entries of the form
    "d:." don't count, and CMD and COMMAND always insert the "." first.)
  ---------------------------------------------------------------------------*/

    if (fourshell && pathtype == REGPATH) {
        for (i = 1;  i < numdirs;  ++i)
            if (dir[i][0] == '.' && dir[i][1] == '\0') {    /* "." */
                startdir = 1;
                break;
            }
    }

/*---------------------------------------------------------------------------
    For each command or file given as an argument, check all of the direc-
    tories in the appropriate path.  For commands, first see if it's an in-
    ternal command or (in the case of 4DOS/4OS2) an alias; if not, search
    the path for it.  For each directory in the path, see if the OS will con-
    sider it a command as is (it has a dot in its name), and if so whether
    it exists; then try appending each extension (in order of precedence)
    and again check for existence.  For data files or DLLs, just check direc-
    tories in the path for the filename or the filename with an appropriate
    extension (".dll", ".inf", etc.) appended.
  ---------------------------------------------------------------------------*/

    for (j = 0;  j < argc;  ++j) {
        int hasdot, found=0;

        /* don't bother with internal commands if argument has a dot */
        hasdot = (strchr(argv[j], (int)'.') != (char *)NULL);

        if (regpath && !hasdot) {
            Trace((stderr, "checking %s internals\n", shells[sh].name));

            /* 4DOS/4OS2 have tests for internals/aliases, so check smartly */
            if (fourshell) {
                char *tempenv[2] = {tempname, NULL};
                int rc;

#ifdef COMMANDSEP_WORKS   /* This form always fails (get correct response only
                           * if first condition is true, i.e., both alias and
                           * internal command).  It appears to be another bug
                           * in 4OS2 (rc always = 0).  The line is too long for
                           * 4DOS in any case (257 bytes for 5-char alias name).
                           */
                sprintf(tempname, "4WHICH=iff isalias %s .and. isinternal %s "
                  "then %%+ echos %s:  aliased to \"%%@alias[%s]\" %%+ exit 43 %%+"
                  " elseiff isalias %s then %%+ echos %s:  aliased to \"%%@alias"
                  "[%s]\" %%+ exit 42 %%+ elseiff isinternal %s then %%+ exit 41 "
                  "%%+ else %%+ exit 40 %%+ endiff", argv[j], argv[j],
                  argv[j], argv[j], argv[j], argv[j], argv[j], argv[j]);
                Trace((stderr, "4WHICH length = %d bytes\n\n%s\n\n",
                  strlen(tempname), tempname));
                rc = spawnle(P_WAIT, comspec, comspec, "/c", "%4which%", NULL,
                  tempenv);
                Trace((stderr, "4WHICH return code = %d\n", rc));
#else /* !COMMANDSEP_WORKS */
                /* quotes around alias necessary due to 4DOS/4OS2 bug */
                if (sh != _4DOS) {   /* 4OS2:  can do in one long command */
                    sprintf(tempname, "4WHICH=iff isalias %s .and. isinternal "
                      "%s then & echos %s:  aliased to \"%%@alias[%s]\" & exit "
                      "43 & elseiff isalias %s then & echos %s:  aliased to "
                      "\"%%@alias[%s]\" & exit 42 & elseiff isinternal %s then "
                      "& exit 41 & else & exit 40 & endiff", argv[j], argv[j],
                      argv[j], argv[j], argv[j], argv[j], argv[j], argv[j]);
                    Trace((stderr, "4WHICH length = %d bytes\n\n%s\n\n",
                      strlen(tempname), tempname));
                    /* "/c", "4which" arguments must be separate for emx+gcc */
                    rc = spawnle(P_WAIT, comspec, comspec, "/c", "%4which%",
                      NULL, tempenv);
                    Trace((stderr, "4WHICH return code = %d\n", rc));
                    if (rc == 42 || rc == 43)  /* alias (and internal if 43) */
                        ++found;
                } else {
                    sprintf(tempname, "4WHICH=iff isalias %s .and. isinternal "
                      "%s then ^ exit 43 ^ elseiff isalias %s then ^ exit 42 ^ "
                      "elseiff isinternal %s then ^ exit 41 ^ else ^ exit 40 ^ "
                      "endiff", argv[j], argv[j], argv[j], argv[j]);
                    Trace((stderr, "4WHICH length = %d bytes\n\n%s\n\n",
                      strlen(tempname), tempname));
                    rc = spawnle(P_WAIT, comspec, comspec, "/c", "%4which%", NULL,
                      tempenv);
                    Trace((stderr, "4WHICH return code = %d\n", rc));
                    if (rc == 42 || rc == 43) {   /* alias */
                        ++found;
                        sprintf(tempname, "echos %s:  aliased to \"%%@alias"
                          "[%s]\"", argv[j], argv[j]);
                        Trace((stderr, "2nd cmd length = %d bytes\n\n%s\n\n",
                          strlen(tempname), tempname));
                        spawnl(P_WAIT, comspec, comspec, "/c", tempname, NULL);
                    }
                }
#endif /* ?COMMANDSEP_WORKS */
                if (rc == 41 || (rc == 43 && all)) {   /* internal */
                    ++found;
                    sprintf(tempname, "%s internal command", shells[sh].name);
                    if (rc == 41)
                        printf("%s:  %s", argv[j], tempname);
                    else
                        printf(" (also %s", tempname);
                }
            } else {
                /* quit as soon as found:  only one internal match allowed */
                for (i = 0;  (i < shells[sh].num_int) && !found;  ++i) {
                    Trace((stderr, "checking %s\n", shells[sh].internal[i]));
                    if (stricmp(argv[j], shells[sh].internal[i]) == 0) {
                        ++found;
                        printf("%s:  %s internal command", argv[j],
                          shells[sh].name);
                        break;
                    }
                }
            }
        }
        for (i = startdir;  (i < numdirs) && (!found || all);  ++i) {
            p = tempname;
            q = dir[i];
            while ((*p++ = *q++) != '\0');  /* p now points to char *after* 0 */
            if (p[-2] == '\\')              /* could be root dir (e.g., c:\) */
                --p;
            else                            /* replace null with dir sep. */
                p[-1] = '\\';
            q = argv[j];
            while ((*p++ = *q++) != '\0');  /* copy program name */
            --p;                            /* point at null */
            if (!regpath || hasdot) {
                Trace((stderr, "checking %s\n", tempname));
                if (!stat(tempname, &statbuf) && !S_ISDIR(statbuf.st_mode)) {
                    if (!found || all) {
                        if (found == 1)
                            printf(" (also %s", tempname);
                        else
                            printf("%s%s", found? ", " : "", tempname);
                    }
                    ++found;
                    if (!all)
                        break;   /* quit right now unless finding all */
                }
            }
            for (k = 0;  (k < shells[sh].num_ext) && (!found || all); ++k) {
                strcpy(p, shells[sh].extension[k]);
                Trace((stderr, "checking %s\n", tempname));
                if (!stat(tempname, &statbuf) && !S_ISDIR(statbuf.st_mode)) {
                    if (!found || all) {
                        if (found == 1)
                            printf(" (also %s", tempname);
                        else
                            printf("%s%s", found? ", " : "", tempname);
                    }
                    ++found;
                    if (!all)
                        break;   /* quit right now unless finding all */
                }
            }
        } /* end i-loop */

        if (!found) {
            printf("no %s in", argv[j]);
            for (i = 0;  i < numdirs;  ++i)
                printf(" %s", dir[i]);
        }
        printf("%s\n", (found > 1)? ")" : "");
    }

    return 0;
}



/**************************/
/* Function get_libpath() */
/**************************/

int get_libpath(char **path)
{
    char *line, *p, config_sys[]="c:\\config.sys";
    int foundcfg=FALSE;
#ifdef __32BIT__
    int drive;
    APIRET rc;
#endif
    FILE *cfg;


    if ((line = (char *)malloc(4096)) == NULL) {
        fprintf(stderr, "%s: not enough memory\n", prognam);
        exit(8);
    }

#ifdef __32BIT__
    rc = DosQuerySysInfo(QSV_BOOT_DRIVE, QSV_BOOT_DRIVE, &drive, sizeof(drive));
    if (rc == NO_ERROR) {
        config_sys[0] = drive - 1 + 'a';
        if (!stat(config_sys, &statbuf) && !S_ISDIR(statbuf.st_mode) &&
            (cfg = fopen(config_sys, "r")) != NULL)
                foundcfg = TRUE;
    } else {
        fprintf(stderr, "%s: internal error (%d)\n", prognam, rc);
#endif
        if (!stat(config_sys+2, &statbuf) && !S_ISDIR(statbuf.st_mode) &&
            (cfg = fopen(config_sys+2, "r")) != NULL)
                foundcfg = TRUE;
        else if (!stat(config_sys, &statbuf) && !S_ISDIR(statbuf.st_mode) &&
            (cfg = fopen(config_sys, "r")) != NULL)
                foundcfg = TRUE;
#ifdef __32BIT__
    }
#endif

    if (!foundcfg) {
        fprintf(stderr, "%s: can't find config.sys\n", prognam);
        *path = (char *)NULL;
        return -1;
    }

    /* assume cfg is open file pointer to config.sys */
    while (fgets(line, 4096, cfg)) {
        if (strnicmp(line, "LIBPATH", 7))  /* assumes no leading spaces: ? */
            continue;

        /* get rid of trailing newline, if any */
        if ((p=strrchr(line, '\n')) != NULL)
            *p = '\0';
        Trace((stderr, "found LIBPATH line:\n%s\n", line));
        p = strchr(line+7, '=');
        if (p == NULL) {
            *path = (char *)NULL;
            return -1;
        }
        while (*++p == ' ');
        *path = p;
        return 0;
    }

    /* LIBPATH not found in config.sys */
    *path = (char *)NULL;
    return -1;
}

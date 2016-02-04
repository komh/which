/*---------------------------------------------------------------------------

    which.c

    This program by default finds the path of an executable command under
    either OS/2 or MS-DOS.  It can also be used to find data files in the
    DPATH or DLLs in the LIBPATH (at least under OS/2).  It does not have
    a whole lot of error-checking, but the array sizes are reasonably gen-
    erous.

    To do:  add support for ksh, bash, etc.; allow wildcard commands?

    Copyright (c) 1993 by Greg Roelofs.  4OS2/4DOS spawnl() code by Michael
    D. Lawler.	DosQuerySysInfo() code by Kenneth Porter.  You may use this
    code for any purpose whatsoever, as long as you don't sell it and don't
    claim to have written it.  Not that you'd want to.

    Modified by Wonkoo Kim (wkim+@pitt.edu):
    (The original starting version was v2.1)

    v2.1.1  September 15, 1995
	- Wildcard support (for emx/gcc) is added.
	- Added option: -s Show file size and date.

    v2.1.2  January 14, 1996
	- Added options:
	    -h	 Search *.hlp in HELP dir.
	    -b	 Search *.inf in BOOKSHELF dir.
	    -e * Search dirs set by env vars.

    v2.1.3  June 3, 1997
	- Option -l now searches BEGINLIBPATH + LIBPATH + ENDLIBPATH.
	    (Thanks to "Bruce A. Mallett" <bam@NightStorm.com>)
	- New options:
	    -1	 Show only the first match from all command line args, i.e.
		 displays a single path name if found (useful in makefile)
		 (-1 + -a for all first matches of command line args)
	    -u	 Unix style path names (use forward slashes as dir separators)
	- Minor refinements


  ---------------------------------------------------------------------------*/

#define VERSION   "v2.1.3 June-03-1997 updated by Wonkoo Kim"

/* #define DEBUG */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <process.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#ifndef TRUE
#  define TRUE	 1
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
#  define INCL_DOSMISC	     /* for IBM Toolkit headers */
#  define INCL_DOSERRORS     /* for IBM Toolkit headers */
#  define INCL_NOPMAPI	     /* for IBM Toolkit headers */
#  include <os2.h>
#if defined(__EMX__)
#  include <fnmatch.h>	    /* for wildcard support -- Wonkoo Kim */
#endif
#endif

#ifdef DEBUG
#  define Trace(x)   fprintf x
#else
#  define Trace(x)
#endif


int get_libpath(char **path);
int get_helppath(char **path);
int get_bookpath(char **path);
int printmsg (char *msg, int pos);
void pretty_path (char *path, char dir_sep);


struct stat statbuf;


/* extensions must be in order of operating-system precedence! */

static char *ext_command[] = {	   /* COMMAND extensions */
    ".com",
    ".exe",
    ".bat"
};

static char *ext_cmd[] = {	   /* CMD extensions */
    ".com",
    ".exe",
    ".cmd",
    ".bat"
};

static char *ext_4dos[] = {	   /* 4DOS extensions */
    ".com",
    ".exe",
    ".btm",
    ".bat"
};

static char *ext_4os2[] = {	   /* 4OS2 extensions */
    ".com",
    ".exe",
    ".btm",
    ".cmd",
    ".bat"
};

static char *ext_libpath[] = {	   /* LIBPATH extension(s) */
    ".dll"
};

static char *ext_dpath[] = {	   /* DPATH extension(s) */
    ".boo",
    ".dat",
    ".inf",
    ".ini",   /* does this belong here? */
    ".hlp",
    ".msg",
    ".ndx"
};

static char *ext_helppath[] = {     /* HELP extension(s) */
    ".hlp"
};

static char *ext_bookpath[] = {     /* BOOKSHELF extension(s) */
    ".inf"
};

static char *ext_envpath[] = {	   /* ENV extension(s) */
    ".*"
};


/* OS/2 internal commands:  some of these are rarely used outside of
 * command files, but all of them can be called from the command line.
 * (I don't know if some of the OS/2-only commands might be in later
 * versions of MS-DOS.	One could use spawnl() and check the errors,
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
#define _CMD	   1
#define _4DOS	   2
#define _4OS2	   3
#define _4OS2_16   4
#define _4OS2_32   5

SHELLINFO shells[] = {
    {"COMMAND.COM", ext_command,   sizeof(ext_command)/sizeof(char *),
		    int_command,   sizeof(int_command)/sizeof(char *) },
    {"CMD.EXE",     ext_cmd,       sizeof(ext_cmd)/sizeof(char *),
		    int_cmd,	   sizeof(int_cmd)/sizeof(char *) },
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
#define DPATH	   2
#define HELPPATH   3
#define BOOKPATH   4
#define ENVPATH    5

char *prognam;


/****************/
/* Main program */
/****************/

int main(int argc, char *argv[])
{
    static char tempname[1024];
    char *p, *q, *comspec, *path, *pathvar="", *dir[1000];
    int i, j, k, c, error=0, startdir=0;
    int sh=_CMD, all=FALSE, pathtype=REGPATH, regpath=TRUE, fourshell=FALSE;
    int firstmatch=FALSE;
    int numdirs=0, numshells=sizeof(shells)/sizeof(SHELLINFO);
    int showinfo=FALSE, quiet=FALSE;
    int found = 0;
    char info[20];
    char *envpath = "", envvar[100];
    char dir_separator = '\\';          /* OS/2 or DOS style dir separator */

/*---------------------------------------------------------------------------
    Parse the command line...
  ---------------------------------------------------------------------------*/

#ifdef __EMX__
    prognam = _getname (argv[0]);   /* remove pathname */
#else
    prognam = argv[0];
#endif
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
		    case 'h':             /* search for help files */
			pathtype = HELPPATH;
			break;
		    case 'b':             /* search for book files */
			pathtype = BOOKPATH;
			break;
		    case 'e':             /* search files in paths set by env */
			pathtype = ENVPATH;
			if (*++(argv[0])) {
			    strcpy (envvar, argv[0]);
			    envpath = getenv(envvar);
			    *(argv[0]+1) = '\0';
			} else if (--argc > 0 && (*++argv)[0]) {
			    strcpy (envvar, argv[0]);
			    envpath = getenv(envvar);
			    *(argv[0]+1) = '\0';
			} else
			    error++;
			break;
		    case '1':           /* find the first match of all cmd args */
			firstmatch = TRUE;
			break;
		    case 's':             /* show file information */
			showinfo = TRUE;
			break;
		    case 'q':             /* quite mode (return errlevel) */
			quiet = TRUE;
			break;
		    case 'u':           /* unix style path (forward slashes) */
			dir_separator = '/';
			break;
		    default:
			++error;
			break;
		} /* end switch, while, while, if */

/*---------------------------------------------------------------------------
    Print usage if any errors or if no arguments.
  ---------------------------------------------------------------------------*/

    if (error || ((pathtype == REGPATH) && (argc <= 0))) {
#ifdef TRADITIONAL
	fprintf(stderr, "%s: too few arguments\n", prognam);
#else
	fprintf(stderr, "\n"
"which (%s) for OS/2%s, from Newtware\n\n"
"Usage:  %s [options] [cmd ... ]\n\n"
"Options:\n"
"    (default behavior is to find location of program executed as \"cmd\")\n"
"    -1  show the first match from all cmd args; display nothing if not found\n"
"    -a  list all matches (With -1, list all first matches of cmd args)\n"
"    -b  search directories in BOOKSHELF for book (.inf) files%s\n"
"    -d  search directories in DPATH for data files%s\n"
"    -e  search directories in a given env var; Exapmple: -e EPMPATH\n"
"    -h  search directories in HELP for help (.hlp) files%s\n"
"    -l  search directories in LIBPATH (BEGIN/ENDLIBPATH) for DLLs%s\n"
"    -q  quiet mode (only returns error code without onscreen messages)\n"
"    -s  show file date/time and size\n"
"    -u  unix style path names (use forward-slashes as dir separators)\n\n"
"   cmd  name to be searched (wildcards can be used)\n\n"
"Examples:  %s -las emx*\n"
"           %s -las foo*.bar  (override the default extension)\n"
, VERSION,
#if defined(__32BIT__) && !defined(EMX)
	    "", prognam, "", "", "", "",
#else
	    " & DOS", prognam, " (OS/2)", " (OS/2)",  " (OS/2)", " (OS/2)",
#endif /* ?__32BIT__ */
	    prognam, prognam);
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
	if ((p = strrchr (comspec, '\\')))
	    p++;
	else if ((p = strrchr (comspec, '/')))
	    p++;
	else
	    p = comspec;
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
	case HELPPATH:
	    pathvar = "HELP";
	    regpath = FALSE;
	    shells[sh].extension = ext_helppath;
	    shells[sh].num_ext = sizeof(ext_helppath)/sizeof(char *);
	    path = getenv(pathvar);
	    break;
	case BOOKPATH:
	    pathvar = "BOOKSHELF";
	    regpath = FALSE;
	    shells[sh].extension = ext_bookpath;
	    shells[sh].num_ext = sizeof(ext_bookpath)/sizeof(char *);
	    path = getenv(pathvar);
	    break;
	case ENVPATH:
	    pathvar = envvar;
	    regpath = FALSE;
	    shells[sh].extension = ext_envpath;
	    shells[sh].num_ext = sizeof(ext_envpath)/sizeof(char *);
	    path = envpath;
	    break;
    }
    Trace((stderr, "COMSPEC now = %s\n", comspec));
    if (argc <= 0) {
	printf ("%s=%s\n", pathvar, path);
	exit (0);
    }

/*---------------------------------------------------------------------------
    Terminate the path elements and store pointers to each one.
  ---------------------------------------------------------------------------*/

    if (path == (char *)NULL || *path == '\0') {
	Trace((stderr, "\n%s is empty\n\n", pathvar));
	if (!regpath) {
	    fprintf(stderr, "%s: %s is empty\n", prognam, pathvar);
	    exit (2);
	}
    } else {
	pretty_path (path, dir_separator);
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
	int hasdot;
	int pos=0;
#ifdef __EMX__
	int wildcard;
	char **list;
	int l;
	wildcard = (strchr(argv[j], (int)'?') != (char *)NULL) ||
		(strchr(argv[j], (int)'*') != (char *)NULL);
#endif
	/* don't bother with internal commands if argument has a dot */
	hasdot = (strchr(argv[j], (int)'.') != (char *)NULL);

	if (!quiet && j) {
	    if (! firstmatch)
		printf ("\n");
	    else if (found && ! showinfo)
		printf (" ");
	}
	found = 0;

	if (regpath && !hasdot) {
	    Trace((stderr, " checking %s internals\n", shells[sh].name));

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
		if (rc == 41 || (rc == 43 && all && !firstmatch)) {   /* internal */
		    ++found;
		    sprintf(tempname, "%s internal command", shells[sh].name);
		    if (rc == 41)
			printf("%s:  %s", argv[j], tempname);
		    else
			printf(" (also %s", tempname);
		}
	    } else {
		/* quit as soon as found:  only one internal match allowed */
		for (i = 0;  i < shells[sh].num_int;  ++i) {
		    Trace((stderr, " checking %s\n", shells[sh].internal[i]));
#ifdef __EMX__
		    if (wildcard) {
			if (fnmatch(argv[j], shells[sh].internal[i],
			  _FNM_IGNORECASE|_FNM_OS2) == 0) {
			    found++;
			    if (quiet) break;
			    if (showinfo) {
				printf("%4d: %s:%*s %s internal command\n",
				  found, shells[sh].internal[i],
				  8-(int)strlen(shells[sh].internal[i]), "",
				  shells[sh].name);
			    } else {
				sprintf(tempname, "%s: %s internal command",
				  shells[sh].internal[i], shells[sh].name);
				if (found == 2) {
				    pos = printmsg (" (also ", -pos);
				    pos = printmsg (tempname, -pos);
				} else {
				    pos = printmsg (tempname, pos);
				}
			    }
			    if (!all || firstmatch)
				break;	 /* quit right now unless finding all */
			}
		    } else {
#endif
			if (stricmp(argv[j], shells[sh].internal[i]) == 0) {
			    ++found;
			    if (quiet) break;
			    if (showinfo) {
				printf("%4d: %s:%*s %s internal command\n",
				  found, argv[j], 8-(int)strlen(argv[j]), "",
				  shells[sh].name);
			    } else {
				printf("%s:%*s %s internal command\n",
				  argv[j], 8-(int)strlen(argv[j]), "",
				  shells[sh].name);
			    }
			    break;
			}
#ifdef __EMX__
		    }
#endif
		}
	    }
	}
	for (i = startdir;  (i < numdirs) && (!found || (all && !firstmatch)); ++i) {
	    p = tempname;
	    q = dir[i];
	    while ((*p++ = *q++) != '\0');      /* p now points to char *after* 0 */
	    if (p[-2] == '\\' || p[-2] == '/')  /* could be root dir (e.g., c:\) */
		--p;
	    p[-1] = dir_separator;	    /* replace null with dir sep. */
	    q = argv[j];
	    while ((*p++ = *q++) != '\0');  /* copy program name */
	    --p;			    /* point at null */
	    if (hasdot) {
		Trace((stderr, " checking %s\n", tempname));
#ifdef __EMX__
		list = _fnexplode (tempname);
		if (list) {
		    for (l = 0; list[l] != NULL; ++l) {
			if (!stat(list[l], &statbuf) && !S_ISDIR(statbuf.st_mode)) {
			    found++;
			    if (quiet) break;
			    if (showinfo) {
				strftime (info, 20, "%T %D",
				    localtime(&statbuf.st_mtime));
				printf ("%4d: %s %9ld  %s\n", found, info,
				    statbuf.st_size, list[l]);
			    } else {
				if (found == 2) {
				    pos = printmsg (" (also ", -pos);
				    pos = printmsg (list[l], -pos);
				} else
				    pos = printmsg (list[l], pos);
			    }
			    if (!all || firstmatch)
				break;	 /* quit right now unless finding all */
			}
		    }
		    _fnexplodefree (list);
		}
#else
		if (!stat(tempname, &statbuf) && !S_ISDIR(statbuf.st_mode)) {
		    ++found;
		    if (quiet) break;
		    if (showinfo) {
			strftime (info, 20, "%T %D",
			    localtime(&statbuf.st_mtime));
			printf ("%4d: %s %9ld  %s\n", found, info,
			    statbuf.st_size, tempname);
		    } else {
			if (found == 2) {
			    pos = printmsg (" (also ", -pos);
			    pos = printmsg (tempname, -pos);
			} else
			    pos = printmsg (tempname, pos);
		    }
		    if (!all || firstmatch)
			break;	 /* quit right now unless finding all */
		}
#endif
	    }
	    for (k = 0;  (k < shells[sh].num_ext) && (!found || (all && !firstmatch)); ++k) {
		strcpy(p, shells[sh].extension[k]);
		Trace((stderr, " checking %s\n", tempname));
#ifdef __EMX__
		list = _fnexplode (tempname);
		if (list) {
		    for (l = 0; list[l] != NULL; ++l) {
			if (!stat(list[l], &statbuf) && !S_ISDIR(statbuf.st_mode)) {
			    found++;
			    if (quiet) break;
			    if (showinfo) {
				strftime (info, 20, "%T %D",
				    localtime(&statbuf.st_mtime));
				printf ("%4d: %s %9ld  %s\n", found, info,
				    statbuf.st_size, list[l]);
			    } else {
				if (found == 2) {
				    pos = printmsg (" (also ", -pos);
				    pos = printmsg (list[l], -pos);
				} else
				    pos = printmsg (list[l], pos);
			    }
			    if (!all || firstmatch)
				break;	 /* quit right now unless finding all */
			}
		    }
		    _fnexplodefree (list);
		}
#else
		if (!stat(tempname, &statbuf) && !S_ISDIR(statbuf.st_mode)) {
		    ++found;
		    if (quiet) break;
		    if (showinfo) {
			strftime (info, 20, "%T %D",
			    localtime(&statbuf.st_mtime));
			printf ("%4d: %s %9ld  %s\n", found, info,
			    statbuf.st_size, tempname);
		    } else {
			if (found == 2) {
			    pos = printmsg (" (also ", -pos);
			    pos = printmsg (tempname, -pos);
			} else
			    pos = printmsg (tempname, pos);
		    }
		    if (!all || firstmatch)
			break;	 /* quit right now unless finding all */
		}
#endif
	    }
	} /* end i-loop */
	if (quiet) {
	    if (!found) exit (1);
	} else {
	    if (!found && !firstmatch) {
		printf("%s is NOT FOUND in:\n ", argv[j]);
		pos = -1;
		for (i = 0;  i < numdirs;  ++i) {
		    pos = printmsg (dir[i], pos);
		}
	    }
	    if (!showinfo && found > 1) {
		if (pos < 75)
		    printf (") %d\n", found);
		else {
		    printf (")\n %d\n", found);
		}
	    }
	    if (firstmatch && found && !all) break;
	}
    }
    exit (0);
}



/**************************/
/* Function get_libpath() */
/**************************/

int get_libpath(char **path)
{
    char *line, *s, *p, config_sys[]="c:\\config.sys";
    char *libpath[3];
    int i, len;
    int foundcfg=FALSE;
#ifdef __32BIT__
    int drive;
    APIRET rc;
#endif
    FILE *cfg=NULL;

    *path = (char *)NULL;

#ifdef __EMX__
    if (_osmode == DOS_MODE) {
	return -1;
    }
#endif

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
	fprintf(stderr, "%s: internal error (%ld)\n", prognam, rc);
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

    libpath[1] = (char *)NULL;	    /* Assume no libpath present */

    if (foundcfg) {
	/* assume cfg is open file pointer to config.sys */
	while (fgets((s = line), 4096, cfg)) {
	    while (*s == ' ') s++;              /* remove leading blanks */
	    if (strnicmp(s, "LIBPATH", 7) == 0) {
		/* get rid of trailing newline, if any */
		if ((p=strrchr(s, '\n')) != NULL)
		    *p = '\0';
		Trace((stderr, "found LIBPATH line:\n%s\n", line));
		p = strchr(s+7, '=');
		if (p != NULL) {
		    while (*++p == ' ') ;
		    libpath[1] = p;
		}
		break;
	    }
	}
	fclose (cfg);
    }

    /*
     * Here with libpath[1] either null (because no LIBPATH was found in a config.sys
     * file) or pointing into line[] to the start of the path data.  Now to grab the
     * begin and end libpaths, allocate a buffer large enough for all of them, and
     * glue the whole thing together in the appropriate order.
     */
#ifdef __32BIT__

    libpath[0] = malloc (512);
    libpath[2] = malloc (512);

    if (!libpath[0] || !libpath[2]) {
	fprintf (stderr, "No memory!\n");
	exit (8);
    }

    *libpath[0] = '\0';
    *libpath[2] = '\0';

    rc = DosQueryExtLIBPATH (libpath[0], BEGIN_LIBPATH);

    if (rc != NO_ERROR)
	fprintf (stderr, "Error %ld (0x%0lx) obtaining BEGINLIBPATH!\n", rc, rc);

    rc = DosQueryExtLIBPATH (libpath[2], END_LIBPATH);

    if (rc != NO_ERROR)
	fprintf (stderr, "Error %ld (0x%0lx) obtaining ENDLIBPATH!\n", rc, rc);
#else
    /* Getting libpaths by getenv() is not best, but try ... */
    libpath[0] = getenv ("BEGINLIBPATH");
    libpath[2] = getenv ("ENDLIBPATH");
#endif

    len = 0;
    for (i = 0; i < 3; ++i ) {
	if (libpath[i] && *libpath[i])
	    len += strlen (libpath[i]) + 1;	/* extra space for a semicolon */
    }
    if (len == 0) {		/* no library path */
#ifdef __32BIT__
	free (libpath[0]);
	free (libpath[2]);
#endif
	return (-1);
    }
    p = malloc (len + 1);
    if (! p) {
	fprintf (stderr, "No memory!\n");
	exit (8);
    }

    *path = p;

    for (i = 0; i < 3; i++) {			/* merge library paths */
	if (libpath[i] && *libpath[i]) {
	    strcpy (p, libpath[i]);
	    p += strlen (p);
	    if (p[-1] != ';') *p++ = ';';
	}
    }
    *p = '\0';          /* Terminate the path */
#ifdef __32BIT__
    free (libpath[0]);
    free (libpath[2]);
#endif
    return (0);
}

/*--------------------------------------------------------------------------*/
int printmsg (char *msg, int pos)
/*--------------------------------------------------------------------------*/
/* Smartly print msg at the column pos with comma-and-space delimited
 * if the msg can fit in the 80-column screen width, and return the next
 * column postion to be printed.  If msg would bump to screen border, msg is
 * printed at the next line after a space.
 */
{
    int mode;

    mode = pos;
    if (mode > 0) {
	printf (",");
	pos++;
    } else if (mode < 0) {
	pos = -pos;
    }
    pos += strlen(msg);
    if (pos >= 78) {		/* if not fit in screen */
	if (mode)		/* if previous pos was not zero */
	    printf ("\n");
	pos = strlen(msg);
    }
    if (mode > 0) {
	printf (" ");
	pos++;
    }
    printf ("%s", msg);
    return (pos);
}

/*----------------------------------------------------------------------*/
void	pretty_path (char *path, char dir_sep)
/*----------------------------------------------------------------------*/
/* convert path to either unix style or OS/2 (DOS) style */
{
    char bad_sep;

    switch (dir_sep) {
    case '\\':
	bad_sep = '/';
	break;
    case '/':
	bad_sep = '\\';
	break;
    default:
	return;
	break;
    }
    for ( ; *path; path++) {
	if (*path == bad_sep) *path = dir_sep;
    }
}


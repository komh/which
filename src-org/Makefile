# Makefile for "which"                                   28 July 1993
#
# - for Microsoft C under MS-DOS, version 6.0 or higher
# - for Microsoft C under OS/2 1.x (16-bit), version 6.0 or higher
# - for IBM C/C++ Set/2 under OS/2 2.0 (32-bit)
# - for GNU gcc (emx kit) under OS/2 2.0 (32-bit), version 0.8e or higher
# - for Watcom C/386 under OS/2 2.0 (32-bit), version 9.0 or higher
# - for Borland C++ 3.1 under MS-DOS
# - for Borland C++ beta and possibly 1.0 release, under OS/2 2.0
#
# To use, enter "{d,n}make" (only tested with nmake), or "nmake list"
# for list of targets.  Name of makefile must be "makefile".  GNU make
# supposedly works but doesn't always give the correct error messages.
#
# Notes on IBM C Set/2, Watcom C/386, Borland C++ or gcc compilation:
#   The resulting programs can be used under OS/2 protected
#   mode of OS/2 2.0 only, not under 1.x and not under DOS.

OBJ=.obj

default:	msc

list:
	@echo 	Enter "$(MAKE) target" where target is one of:
	@echo 	   msc mscdos mscos2 ibm ibmdyn watcom
	@echo 	   bccdos bccos2 gcc gccdyn gccdebug
	@echo 	default is msc (MSDOS/OS2 bound executable)

# MSC 6.00 for OS/2 and MS-DOS, 16-bit bound
msc:	which.c which.def
	cl -nologo -AS -Ocegit -Gs -DMSC $(FP) -F 2000 -Lp -Fbwhich.exe \
	 which.c which.def
#	LDFLAGS2="-link /noe /pm:vio"

# MSC 6.00 cross-compilation for MS-DOS
mscdos:	which.c which.def
	cl -nologo -AS -Oacit -Gs -DMSC $(FP) -F 2000 -Lr -Fewhich.exe which.c
#	LDFLAGS2="-link /noe /exe"

# MSC 6.00 for OS/2, 16-bit
mscos2:	which.c which.def
	cl -nologo -AS -Ocegit -G2s -DOS2 -DMSC $(FP) -F 2000 -Lp -Fewhich.exe \
	 which.c which.def
#	LDFLAGS2="-link /noe /pm:vio"

# IBM C Set/2, statically linked runtime
ibm:
	$(MAKE) all \
	CC="icc -Q -O -Gs" \
	CFLAGS="-Sm -Sp1 -DOS2" \
	LDFLAGS="-Fe" \
	LDFLAGS2="" \
	OUT="-Fo" \
	DEF="which.def"

# IBM C Set/2, dynamically linked runtime
ibmdyn:
	$(MAKE) all \
	CC="icc -Q -O -Gs -Gd" \
	CFLAGS="-Sm -Sp1 -DOS2" \
	LDFLAGS="-Fe" \
	LDFLAGS2="" \
	OUT="-Fo" \
	DEF="which.def"

# Watcom C/386 9.0
watcom:
	$(MAKE) all \
	CC="wcl386 -zq -Ox -s" \
	CFLAGS="-Zp1 -DOS2" \
	LDFLAGS="-k0x40000 -x -Fe=" \
	LDFLAGS2="" \
	OUT="-Fo" \
	DEF=""

# Borland C++ 3.1
# This form *should* work without the OBJ= line above, but it doesn't...
#	bcc -mt -lt -O2 -G -Z -a -d -P-.c -ewhich.com which.c
bccdos:
	bcc -mt -lt -O2 -G -Z -a -d -P-.c which

# Borland C++ beta (TOPAZ)
bccos2:
	$(MAKE) all \
	CC="bcc -O" \
	CFLAGS="-w- -DOS2" \
	LDFLAGS="-e" \
	LDFLAGS2="" \
	OUT="-o" \
	DEF=""

# emx 0.8f, gcc, OMF format, statically linked C runtime and emx
gcc:
	$(MAKE) all \
	CC="gcc -Zomf -Zsys -O" \
	CFLAGS="-DOS2" \
	LDFLAGS="-o ./" \
	LDFLAGS2="-s -los2" \
	OUT="-o" \
	DEF="which.def"

# emx 0.8f, gcc, OMF format, dynamically linked C runtime and emx
gccdyn:
	$(MAKE) all \
	CC="gcc -Zomf -Zmt -O" \
	CFLAGS="-DOS2" \
	LDFLAGS="-o ./" \
	LDFLAGS2="-s -los2" \
	OUT="-o" \
	DEF="which.def"

# emx, gcc, a.out format, with debug info for gdb
gccdebug:
	$(MAKE) all \
	CC="gcc -g" \
	CFLAGS="-DOS2" \
	LDFLAGS="-o ./" \
	LDFLAGS2="-los2" \
	OUT="-o" \
	OBJ=".o"

# variables
OBJS  = which$(OBJ)

.c$(OBJ):
	$(CC) $(CFLAGS) -c $<

all:	which.exe

which.exe: $(OBJS) $(DEF)
	$(CC) $(LDFLAGS)$@ $(DEF) $(OBJS) $(LDFLAGS2)

which$(OBJ):	which.c

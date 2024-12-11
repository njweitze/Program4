# Makefile for JSON Server
#

CC = gcc
CFLAGS = -g -Wall -Werror -Wno-deprecated-declarations
OS = $(shell uname -s)
PROC = $(shell uname -p)
EXEC_SUFFIX=$(OS)-$(PROC)

ifeq ("$(OS)", "SunOS")
	OSLIB=-L/opt/csw/lib -R/opt/csw/lib -lsocket -lnsl
	OSINC=-I/opt/csw/include
	OSDEF=-DSOLARIS
else
ifeq ("$(OS)", "Darwin")
	OSLIB=
	OSINC=
	OSDEF=-DDARWIN
else
	OSLIB=
	OSINC=
	OSDEF=-DLINUX
endif
endif

all: json-server-$(EXEC_SUFFIX)

json-server-$(EXEC_SUFFIX): json-server.c
	$(CC) $(CFLAGS) $(OSINC) $(OSLIB) $(OSDEF) -o $@ json-server.c -pthread

handin: README
	handin bellardo 464_fp README json-server.c json_server.h Makefile

clean:
	rm -rf json-server-* json-server-*.dSYM

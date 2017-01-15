## Process this file with automake to produce Makefile.in

## Created by Anjuta

AM_CPPFLAGS = \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" 

AM_CFLAGS =\
	 -Wall\
	 -g

bin_PROGRAMS = tmmdsreader

tmmdsreader_SOURCES = \
	main.c       \
	errormanager.c       \
	gidsreader.c       \
	queue.c       \
	socketengine.c       \
	swapendian.c       \
	volume.c       \
	queue.h       \
	errormanager.h       \
	itchreader.h       \
	batsreader.h       \
	arcareader.h       \
	main.h       \
	gidsreader.h       \
	volume.h       \
	swapendian.h       \
	readquotes.h       \
	global.h       \
	global.c       \
	itchreader.c       \
	arcareader.c       \
	batsreader.c       \
	readquotes.c       \
	calcengine.c       \
	calcengine.h

tmmdsreader_LDFLAGS = \
	-lpthread \
	-lm

tmmdsreader_LDADD =  \
	-lpthread \
	-lm


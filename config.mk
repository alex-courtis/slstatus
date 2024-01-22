# slstatus version
VERSION = 0

# customize below to fit your system

# paths
PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

# flags
CPPFLAGS = -I$(X11INC) -D_DEFAULT_SOURCE -DHOST_$(shell hostname)
CFLAGS   = -std=c99 -pedantic -Wall -Wextra -Werror -Os -g
LDFLAGS  = -L$(X11LIB)
LDLIBS   = -lX11 -lsensors -lpulse -lm -lpthread

# compiler and linker
CC = cc

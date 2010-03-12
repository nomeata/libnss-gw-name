# Makefile for libnss-gw-name

CC = gcc
prefix = /usr
exec_prefix = ${prefix}
BITSOFS=
libprefix = ${exec_prefix}/lib$(BITSOFS)
DESTDIR=
OBJSUFFIX=$(BITSOFS).o
OBJECTS=libnss_gw_name$(OBJSUFFIX) 
SHARED_OBJECT = libnss_gw_name$(BITSOFS).so.2
INSTALL_NAME = libnss_gw_name.so.2
# This only works sometimes, give manually when needed:
BIT_CFLAGS = $(if $(BITSOFS),-m$(BITSOFS))
CFLAGS = $(BIT_CFLAGS) -g -O2 -Wall -Wpointer-arith
CPPFLAGS =
LIBS = `pkg-config --libs libnl-1`
LDFLAGS = -shared -Wl,-soname,$(INSTALL_NAME) -Wl,-z,defs --no-allow-shlib-undefined

all: $(SHARED_OBJECT)

$(SHARED_OBJECT): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(SHARED_OBJECT) $(OBJECTS) $(LIBS)

%$(OBJSUFFIX): %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -c -o $@ $<

install:
	install -m755 -d $(DESTDIR)$(libprefix)/
	install -m644 $(SHARED_OBJECT) $(DESTDIR)$(libprefix)/$(INSTALL_NAME)

clean:
	rm -f $(OBJECTS)
	rm -f $(SHARED_OBJECT)

distclean: clean

.PHONY: all

.PHONY: all clean distclean

SSL_WRAP  = ssl_wrapper
EXEC_DIR  = /sbin/
UNAME    := $(shell uname)

CPPFLAGS ?= -D_LARGEFILE_SOURCE \
            -D_LARGEFILE64_SOURCE \
            -D_FILE_OFFSET_BITS=64

CFLAGS   ?= -Wall \
            -Winit-self \
            -Wmissing-field-initializers \
            -Wpointer-arith \
            -Wredundant-decls \
            -Wshadow \
            -Wstack-protector \
            -Wswitch-enum \
            -Wundef \
            -fdata-sections \
            -ffunction-sections \
            -fstack-protector-all \
            -ftabstop=4 \
            -g3 \
            -pipe \
            -std=c99

LDFLAGS  ?= -lndm

LDFLAGS  +=  -lcrypto -lssl

CFLAGS   += -D_DEFAULT_SOURCE \
            -DNS_ENABLE_SSL

ifeq ($(UNAME),Linux)
CFLAGS   += -D_POSIX_C_SOURCE=200809L \
            -D_XOPEN_SOURCE=600 \
            -D_SVID_SOURCE=1
endif

all: $(SSL_WRAP)

$(SSL_WRAP): ssl_wrapper.c Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) $< net_skeleton.c $(LDFLAGS) -o $@

clean:
	rm -fv *.o *~ $(SSL_WRAP)

distclean: clean

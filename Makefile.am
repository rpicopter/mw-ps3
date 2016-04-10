bin_PROGRAMS = mw-ps3 
mw_ps3_SOURCES = ps3.c ps3dev.c 
mw_ps3_CFLAGS = -Wall
mw_ps3_LDFLAGS = 
mw_ps3_LDADD = -lmw_core -lrt -lpthread -lssl -lcrypto -lresolv

mwconfdir=$(sysconfdir)/mw

install-exec-hook:
	mkdir -p $(mwconfdir)
	cp utils/mw-ps3.start $(mwconfdir)/
	chmod 666 $(mwconfdir)/mw-ps3.start


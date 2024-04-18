include $(TOPDIR)/Makefile.inc

APP_DLA_OBJS = api_ftd2xx.o command_parser.o dlatool.o
APP_DLA_OBJS += 

OBJS = $(APP_DLA_OBJS)

LIBS = $(TOPDIR)/lib/libcommon.a $(TOPDIR)/lib/libdla.a $(TOPDIR)/lib/libftd2xx/libftd2xx.a
LIBS_INCLUDE = -L.. -L../lib -L../lib/libftd2xx -L/usr/local/lib -lcommon -ldla -lftd2xx -lpthread -lreadline -lm

CPPFLAGS += -I.. -I../include/ftd2xx
CFLAGS += -I.. -I../include/ftd2xx

all: tsn-hat

tsn-hat: .depend $(LIBS) $(APP_DLA_OBJS)
	$(CC) $(CFLAGS) $(APP_DLA_OBJS) -o $@ $(LIBS_INCLUDE)

clean:
	rm -rf *.o *~ core .depend
	rm -rf tsn-hat 

#########################################################################

.depend:	Makefile $(OBJS:.o=.c)
		$(CC) -M $(CFLAGS) $(OBJS:.o=.c) > $@

sinclude .depend

#########################################################################

LIB	= libdla.a

OBJS = helper.o log.o dla_main.o dla_LargeRead.o dla_functions.o

INCLUDES = 

#CFLAGS = -Wall -Werror -I./ -I../ -I../include/ -I../include/libcom -I../include/dlatool -I../include/engine
CFLAGS =  -I./ -I../ -I../include/ -I../include/libcom -I../include/dlatool

all:	$(LIB) $(OBJS)

$(LIB):	.depend $(OBJS)
	$(AR) crv $@ $(OBJS)
	cp -f $@ ../lib

#########################################################################

.depend:	Makefile $(OBJS:.o=.c)
		$(CC) -M $(CFLAGS) $(INCLUDES) $(OBJS:.o=.c) > $@

sinclude .depend

#########################################################################

clean :
	rm -rf *.o *.a

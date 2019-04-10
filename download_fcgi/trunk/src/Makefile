CC = g++
CFLAGS  = -Wall -pipe -DDEBUG -g -D_GNU_SOURCE -D_FASTCGI 
LIB 	= -L/usr/local/lib -ldl -lrt -ltaomee++ -ltaomee -L./ -lfcgi -lfastcommon -lfdfsclient -lgd
INCLUDE = -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include/ -I/usr/include/libxml2 \
          -I/usr/local/include/fastcommon -I/usr/local/include/fastdfs -I../../../include

OO	= log.o download_fcgi.o
TARGETS = download.fcgi


#all: $(LOGLIB) $(OO) $(TARGETS) 

all: $(OO) $(TARGETS)



$(TARGETS):
	$(CC) $(CFLAGS) $(OO) -o $@ $(LIBDIR) $(LIB) $(INCLUDE)
	rm -fr ../bin/$(TARGETS)
	cp $(TARGETS) ../bin/
.c.o:
	$(CC)  $(CFLAGS) -c $(INCLUDE) $<

%:%.c
	$(CC) $(CFLAGS) -o $@ $< $(OO) $(LDFLAGS)
	
clean:
	rm -f *.o 
	rm -f $(TARGETS)


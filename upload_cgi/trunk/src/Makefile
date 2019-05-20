CC = g++
CFLAGS  = -Wall -pipe -g -D_GNU_SOURCE
#CFLAGS  += -DCGICDEBUG
#CFLAGS += -DDEV_TEST
#CFLAGS += -DDEBUG

CFLAGS += -DFDFS_SUPPORT_THUMBNAIL
CFLAGS += -DFDFS_SUPPORT_SQUARENAIL

LIB    = -L../../../fastDFS/lib -ldl -lrt -ltaomee++ -ltaomee -lgd /usr/local/lib/libjson-c.a -lfdfsclient -lfastcommon
#LIB    += -lfcgi
INCLUDE = -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include/ -I/usr/include/libxml2 \
		-I../../../fastDFS/include/fastcommon -I../../../fastDFS/include/fastdfs -I../../../include
INCLUDE += -I/usr/local/include/json-c
INCLUDE += -I/home/ian/workspace/mifan/mifan/server/friend_backend/hiredis
LIB     += -lhiredis

OO=log.o cgic.o upload_cgi.o tcpip.o
UPLOAD_TARGETS=upload.cgi


#all: $(OO) $(UPLOAD_TARGETS) $(UPLOAD_TARGETS_BUF)
all: $(OO) $(UPLOAD_TARGETS)

$(UPLOAD_TARGETS):
	$(CC) $(CFLAGS) $(OO) -o $@ $(LIBDIR) $(LIB)
	mv $(UPLOAD_TARGETS) ../bin/upload.cgi

.c.o:
	$(CC)  $(CFLAGS) -c $(INCLUDE) $<

%:%.c
	$(CC) $(CFLAGS) -o $@ $< $(OO) $(LDFLAGS)

clean:
	rm -f *.o *.cgi ../bin/upload.cgi

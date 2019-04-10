#!/bin/bash 

### BEGIN INIT INFO 
# Provides: $fdfs_storaged 
### END INIT INFO 

export LD_LIBRARY_PATH=./
PORCNAME=fdfs_storage 
PRG=./fdfs_storage
CONF=../etc/storage.conf 

if [ ! -f $PRG ]; then 
echo "file $PRG does not exist!" 
exit 2 
fi 

if [ ! -f $CONF ]; then 
echo "file $CONF does not exist!" 
exit 2 
fi 

CMD="$PRG $CONF" 
RETVAL=0 

start() { 
    echo -n $"Starting FastDFS storaged server: $PORCNAME" 
        export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./../../libs/
        $CMD & 
        RETVAL=$? 
        echo
        return $RETVAL
}

stop() {
    killall $PORCNAME
        RETVAL=$?
        return $RETVAL
}

restart() {
    stop
        sleep 2
        start
}

case "$1" in
start)
start
;;
stop)
stop
;;
restart)
restart
;;
*)
echo $"Usage: $0 {start|stop|restart}"
exit 1
esac

exit $?


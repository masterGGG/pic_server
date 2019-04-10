if [ -f ../var/run/lighttpd.pid ] 
then
    pid=`cat ../var/run/lighttpd.pid`
    echo "kill $pid"
    kill $pid
    sleep 1
fi
export LD_LIBRARY_PATH=/usr/lib
./lighttpd -f ../etc/lighttpd.conf -m ../modules
echo "started lighttpd-cgi"


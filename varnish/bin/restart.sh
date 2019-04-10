if [ -f ../var/run/varnish.pid ] 
then
pid=`cat ../var/run/varnish.pid`
echo "kill $pid"
kill $pid
fi

./varnishd -f /home/ian/local_service/pic_storage/varnish/etc/default.vcl \
        -n /home/ian/local_service/pic_storage/varnish/var/ \
        -a 0.0.0.0:8081 \
        -P ../var/run/varnish.pid \
        -s file,../var/cache/varnish_storage.bin,6G \
        -T localhost:6082 \
        -p thread_pools=4 \
        -p thread_pool_add_delay=2 \
        -p lru_interval=20 \
        -h simple_list \
#        -N /var/lib/varnish/debian9/_.vsm \

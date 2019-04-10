#########################################################################
# File Name: varnish_monitor.sh
# Author: ian
# mail: ian@taomee.com
# Created Time: Fri 15 Mar 2019 11:32:11 AM CST
#########################################################################
#!/bin/bash

/usr/local/bin/varnishstat -1 -N /home/ian/local_service/pic_storage/varnish/var/_.vsm | grep -w 'MAIN.cache_hit\|MAIN.cache_miss' |awk '{print $2}' | awk -v hit=0 -v mis=0 '{if(NR == 1){hit=$1;}else{mis=$1;}} END{print(hit/(hit+mis)*100)}'

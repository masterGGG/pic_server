#!/bin/bash
red_clr="\033[31m"
grn_clr="\033[32m"
end_clr="\033[0m"
cd `dirname $0`
cwd=`pwd`

appname='lighttpd'
conf_path='../etc/lighttpd.conf'
pidfile='./var/run/lighttpd.pid'
svr_path=`pwd`

add_crontab()
{
    tmpfile=crontab-ori.tempXX
    item1='*/1 * * * * cd '${cwd}' && ./keep-alive.sh server >> keep-alive.log 2>&1'

    crontab -l >$tmpfile 2>/dev/null

    fgrep "${item1}" $tmpfile &>/dev/null
    if [ $? -ne 0 ]
    then
        echo "${item1}" >> $tmpfile
    fi

    crontab $tmpfile
	rm -f $tmpfile
}

delete_crontab()
{
	tmpfile1=crontab-ori.temp1XX
	tmpfile2=crontab-ori.temp2XX
	item1='*/1 * * * * cd '${cwd}' && ./keep-alive.sh server >> keep-alive.log 2>&1'

	crontab -l >$tmpfile1 2>/dev/null

    if [ -s $tmpfile ] ; then
    	fgrep "${item1}" $tmpfile1 &>/dev/null
	    if [ $? -eq 0 ]
    	then
	    	fgrep -v "${item1}" $tmpfile1 &> $tmpfile2
		    crontab $tmpfile2
        fi
    else
        echo "${item1}" >> $tmpfile
	fi

	rm -f $tmpfile1
	rm -f $tmpfile2
}


start()
{
    if [ -f $pidfile ] 
    then
        ps -f `cat $pidfile` | grep ${appname} > /dev/null 2>&1
    	if [ $? -eq 0 ]
    	then
	    	printf "$red_clr%50s$end_clr\n" "${appname} is already running"
		    exit 1
    	fi
    fi
    cd ./bin
    export LD_LIBRARY_PATH=/usr/lib
    ./lighttpd -f ../etc/lighttpd.conf -m ../modules
    echo "started lighttpd-cgi"
    cd ../

	sleep 1
    ps -f `cat $pidfile` | grep ${appname} > /dev/null 2>&1
	if [ $? -ne 0 ]
	then
		printf "$red_clr%50s$end_clr\n" "start ${appname} failed."
		exit 1
	fi
}

stop()
{
    if [ -f $pidfile ] 
    then
        pid=`cat $pidfile`
        echo "kill $pid"
        kill $pid
        sleep 1
    fi
}

restart()
{
	stop
    delete_crontab
	start
	add_crontab
}

state()
{
    if [ -f $pidfile ] 
    then
        ps -f `cat ${pidfile}` | grep ${appname} 
        if [ $? -ne 0 ]
        then
            printf "$red_clr%50s$end_clr\n" "$appname is not running"
            exit 1
        fi
    else
        printf "$red_clr%50s$end_clr\n" "$appname is not running"
        exit 1
    fi
}

usage()
{
	echo "$0 <start|stop|restart|state|> <center|file>"
}

if [[ $# -ne 1 && $# -ne 2 ]]; then
    usage
    exit 1
fi

case $1 in
    start)
        start $2
		add_crontab
        ;;
    stop)
        stop
		delete_crontab
        ;;
    restart)
        restart 
        ;;
    state)
        state 
        ;;
    *)
        usage 
        ;;
    esac

exit 0

<?php
$filename = "ret.txt";
$fp = fopen($filename,'r');
for($i = 0; $i < 1; $i++) {
    $pid = pcntl_fork();
    if ($pid == -1) {
        die('could not fork');
    } elseif ($pid == 0) {
        while (!feof($fp))
        {
            $line = fgets($fp);
            echo "line $line\n";
            list($ret,$key,$type,$photoid,$fdfs_url,$filesize) = split ("\t", $line);   
            echo "url : $fdfs_url \n";
            $pic_url3 = "http://10.1.1.197:12580/download.fcgi?$fdfs_url";
            echo "$pic_url3\n";	
            $ch3 = curl_init();
            curl_setopt ($ch3, CURLOPT_URL, $pic_url3);
            curl_setopt ($ch3, CURLOPT_RETURNTRANSFER, 1);
            curl_setopt ($ch3, CURLOPT_CONNECTTIMEOUT,10);
            $pic_content3 = curl_exec($ch3);
            $num3 =  strlen($pic_content3);
            curl_close($ch3);	
            if($num3 < 1024){
                echo  "404\n";
            }else{
                echo "get ok filelen $num3\n";
            }
        }
        exit (0);
    }
}
for($i = 0; $i < 1; $i++)
{
    $pid = pcntl_wait($status);
    echo "exit child:".$pid."\n";
}

fclose($fp);
exit;
?>

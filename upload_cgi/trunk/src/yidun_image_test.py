#!/usr/bin/env python
#encoding=utf8


import hashlib
import time
import random
import urllib
import urllib2
import json
import sys
import base64
import redis        
import requests

import config_pic
configs = config_pic.configs

class Networkerror(RuntimeError):
    def __init__(self, arg):
        self.args = arg

class ImageCheckAPIDemo(object):
    """图片在线检测接口示例代码"""
    API_URL = configs['server']['url']

    def __init__(self, secret_id, secret_key):
        """
        Args:
            secret_id (str) 产品密钥ID，产品标识
            secret_key (str) 产品私有密钥，服务端生成签名信息使用
        """
        self.secret_id = secret_id
        self.secret_key = secret_key

    def gen_signature(self, params=None):
        """生成签名信息
        Args:
            params (object) 请求参数
        Returns:
            参数签名md5值
        """
        buff = ""
        for k in sorted(params.keys()):
            buff += str(k)+ str(params[k])
        buff += self.secret_key
        return hashlib.md5(buff).hexdigest()

    def check(self, params):
        """请求易盾接口
        Args:
            params (object) 请求参数
        Returns:
            请求结果，json格式
        """
        params["secret_id"] = self.secret_id
        params["timestamp"] = int(time.time() * 1000)
        params["nonce"] = int(random.random() * 100000000)
        params["signature"] = self.gen_signature(params)

        response = requests.post(self.API_URL, data=json.dumps(params))
        prediction = response.json()

        return prediction
    
def check_by_url(message):
    start = time.time()
    """示例代码入口"""
    SECRET_ID = configs['params']['id'] # 产品密钥ID，产品标识
    SECRET_KEY = configs['params']['id'] # 产品私有密钥，服务端生成签名信息使用，请严格保管，避免泄露
    
    image_check_api = ImageCheckAPIDemo(SECRET_ID, SECRET_KEY)
    images = []
   
    picinfo = message.split('-', 1)
    clientip = picinfo[0]
    imageurl = picinfo[1]
    IMAGE_URL = configs['picture_basic_path'] + picinfo[1].split('/', 2)[2]
    imagebase64 = {
        "name":"{\"url\": IMAGE_URL, \"clientIP\": clientip}",
        "type":2,
        "data":base64.urlsafe_b64encode(open(IMAGE_URL, "rb").read())
    }
    images.append(imagebase64)
    
    params = {
        "images": json.dumps(images),
        "account": 463707112,
        "ip": clientip,
        "game_id": configs['params']['game_id'],
    }
    
#    print json.dumps(params)
    ret = image_check_api.check(params)
    if ret["status_code"] == configs['server']['code']['succ']:
        results = ret["result"]
        for result in results:
            maxLevel = -1
            for labelObj in result["labels"]:
                label = labelObj["label"]
                level = labelObj["level"]
                rate  = labelObj["rate"]
                print "label:%s, level=%s, rate=%s" %(label, level, rate)
                maxLevel =level if level > maxLevel else maxLevel
            if maxLevel==0:
                print "#图片机器检测结果：最高等级为\"正常\"\n"
            elif maxLevel==1:
                print "#图片机器检测结果：最高等级为\"嫌疑\"\n"
            elif maxLevel==2:
                print "#图片机器检测结果：最高等级为\"确定\"\n"
            else:
                print "ERROR: ret.code=%s, ret.err=%s" % (ret["status_code"], ret["err"])
        end = time.time()
        print "cost time: " + str(end - start)
    elif ret["status_code"] == configs['server']['code']['retry']:
        raise Exception,"Yidun internal error!"

if __name__ == "__main__":
#    print json.dumps(configs)
    try:
        r = redis.Redis(host = configs['cached']['host'], port = configs['cached']['port'], password= configs['cached']['password'])
        r.ping()
    except redis.exceptions.ConnectionError, err:
        print err
        exit(-1)
    
    while (True) :
        try:
            resp = r.brpop(configs['cached']['check_queue'], configs['cached']['timeout'])
            if resp:
                check_by_url(resp[1])
            else:
                print "No data in check queue, get from retry queue"
                resp = r.brpop(configs['cached']['retry_queue'], configs['cached']['timeout'])
                if resp:
                    check_by_url(resp[1])
                else:
                    print "No data in retry queue, continue get from check queue"
        except requests.exceptions.ConnectionError, log:
            print log
        except Exception, log:
            if resp:
                r.lpush(configs['cached']['retry_queue'], resp[1])
            print "Internal error %s, lpush to retry queue", log

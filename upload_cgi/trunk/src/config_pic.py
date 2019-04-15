# config_pic.py

configs = {
    'cached': {
        'host': '0.0.0.0',
        'port': 20190,
        'password': 'ta0mee@123',
        'check_queue': 'image:check:queue',
        'retry_queue': 'image:retry:queue',
        'timeout': 5
    },
    'server': {
        'url': 'http://10.30.1.213:5000/yidun/image/check',
        'code': {
            'succ': 0,
            'retry': 530
        }
    },
    'picture_basic_path': '/home/ian/local_service/pic_storage/fdfs/g02_storage/g01/data/',
    'params': {
        'id': '201811230700',
        'key': '201811230700',
        'game_id': 700
    }
}

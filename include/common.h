#ifndef  COMMON_H
#define  COMMON_H

#include <arpa/inet.h>
#include <stdint.h>

/* define */
#define	PIC_MAX_LEN         (5*1024*1024)
#define PIC_TAG_LEN         8

#define	SUCC                0
#define	FAIL                -1
#define	TOO_SMALL           -2
#define	DECRYPT_KEY         "!tA:mEv,"
#define	LOG_SIZE            104857600
#define	MAX_FILENAME_LEN    256
#define FDFS_URL_LEN        64
#define SUFFIX_LEN          16
#define THUMB_ARG_LEN       16
#define TIME_LEN            128
#define SYNC_URL_LEN        1024
#define MD5_LEN             32

//#define STAT_MSG_LOG_PATH   "/opt/taomee/stat/spool/inbox/pic_msg.log"
//#define STAT_MSG_LOG_PATH   "../log/pic_msg.log"

/* cgi error code */
#define ERR_UNKNOWN                 4000
#define ERR_SESSION_KEY             4001
#define ERR_THUMB_CNT               4002
#define ERR_RETRIEVE_FILENAME       4003
#define ERR_IMAGE_SIZE              4004
#define ERR_OPEN_FILE               4005
#define ERR_IMAGE_TYPE              4006
#define ERR_NET                     4007
#define ERR_READ_FILE               4008
#define ERR_HOSTID                  4009
#define ERR_TIMEOUT                 4010
#define ERR_INVALID_FILE            4011
#define ERR_SYNC_WEB_SERVER         4012
#define ERR_UPLOAD_TYPE             4013
#define ERR_FILE_TOO_SMALL          4014
#define ERR_BEYOND_ALBUM_SIZE       4015


#define ERR_CODE_SYNC               91011

/* image type */
#define TYPE_UNKNOWN    0
#define TYPE_JPG        1
#define TYPE_PNG        2
#define TYPE_GIF        3

/*stat msg id*/
#define STAT_00K_to_20K_MSG_ID          0x0D000501
#define STAT_20K_to_40K_MSG_ID          0x0D000502
#define STAT_40K_to_80K_MSG_ID          0x0D000503
#define STAT_80K_to_160K_MSG_ID         0x0D000504
#define STAT_160K_to_320K_MSG_ID        0x0D000505
#define STAT_320K_to_640K_MSG_ID        0x0D000506
#define STAT_640K_to_1M_MSG_ID          0x0D000507
#define STAT_1M_to_2M_MSG_ID            0x0D000508
#define STAT_2M_to_4M_MSG_ID            0x0D000509
#define STAT_4M_to_5M_MSG_ID            0x0D00050A

/* download file cmd */
#define	CMD_DOWNLOAD_ERR            5000
#define	CMD_DOWNLOAD_IMAGE          5001
#define	CMD_DOWNLOAD_THUMB          5002

/* http status */
#define HTTP_OK                     200
#define HTTP_NOT_MODIFIED           304
#define HTTP_BAD_REQUEST            400
#define HTTP_UNAUTHORIZED           401
#define HTTP_FORBIDDEN              403
#define HTTP_NOT_FOUND              404
#define HTTP_NOT_ALLOWED            405
#define HTTP_REQUEST_TIME_OUT       408
#define HTTP_INTERNAL_SERVER_ERROR  500

/* http status string */
#define HTTP_OK_STR                     "OK"
#define HTTP_NOT_MODIFIED_STR           "Not Modified"
#define HTTP_BAD_REQUEST_STR            "Bad Request"
#define HTTP_UNAUTHORIZED_STR           "Unauthorized"
#define HTTP_FORBIDDEN_STR              "Forbidden"
#define HTTP_NOT_FOUND_STR              "Not Found"
#define HTTP_NOT_ALLOWED_STR            "Method not allowed"
#define HTTP_REQUEST_TIME_OUT_STR       "Request timeout"
#define HTTP_INTERNAL_SERVER_ERROR_STR  "Internal Server Error"

#define HEADER_CONTENT_TYPE(type) \
    printf("Content-type: %s\r\n", type);

#define HEADER_LAST_MODIFIED(time) \
    printf("Last-Modified: %s\r\nCache-Control: max-age=94608000\r\n", time);

#define HEADER_STATUS(status,statusMessage) \
    printf("Status: %d %s\r\n", status, statusMessage);

#define HEADER_CONTENT_DISPOSITION(filename) \
    printf("Content-Disposition: inline; filename=%s\r\n", filename);

#define HEADER_END \
    printf("\r\n");

/******************************************************************************/
/* proto                                                                      */
/******************************************************************************/
/*
 * webclient -> upload_cgi
 */
struct upload_request_t {
    uint32_t    userid;
    uint32_t	channel;
    uint32_t	albumid;
    uint32_t    ip;
    uint32_t    time;
    uint32_t    width_limit;
    uint32_t    height_limit;
    uint32_t    is_head;
}__attribute__((packed));

/*
 * upload_cgi -> webclient
 */
struct upload_reponse_t {
    uint32_t    hostid;
    uint32_t    albumid;
    uint32_t    photoid;
    char        fdfs_url[FDFS_URL_LEN]; // url
    uint32_t    len;                    // file size
}__attribute__((packed));

/*
 * upload_cgi -> webserver
 */
struct sync_request_t {
    uint32_t    userid;
    uint32_t    hostid;
    uint32_t    albumid;
    uint32_t    len;
    char        fdfs_url[FDFS_URL_LEN]; // url
    char        filename[3*MAX_FILENAME_LEN]; // filename
}__attribute__((packed));

/*
 * webserver -> upload_cgi
 */
struct sync_reponse_t {
    uint32_t    ret;
    uint32_t    errorid;
    uint32_t    userid;
    uint32_t    photoid;
}__attribute__((packed));


/******************************************************************************/
/* admin proxy proto                                                          */
/******************************************************************************/
#define	CMD_DEL_FILE_SINGLE  3001
#define CMD_DEL_FILE_MULTI   3002
#define CMD_CUT_LOGO         3005
#define CMD_CS_DEL_FILE      3030


/* web svr error code */
#define ERR_ADMIN_THUMBSERV_NET_ERR       10001		
#define ERR_ADMIN_ADMINSERV_BUSY          10002
#define ERR_ADMIN_SYSTEM_FATAL_ERR        10010		
#define ERR_ADMIN_SRC_FILE_NOT_EXIST      10101		
#define ERR_ADMIN_THUMB_FILE_NOT_EXIST    10102		
#define ERR_ADMIN_FILE_NOT_PIC            10103		
#define ERR_ADMIN_FILE_TOO_LARGE          10106
#define ERR_ADMIN_CGI_PARA_ERR            10110
#define ERR_ADMIN_INVALID_FILE            10111
#define ERR_ADMIN_UPLOAD_BUSY_NOW         10112
#define ERR_ADMIN_INVALID_PARA            10113
#define ERR_ADMIN_DEL_TOO_MANY_ONCE       10114
#define ERR_ADMIN_CHG_LOGO_TO_QUICKLY     10115
#define ERR_ADMIN_LLOCC_FAULT             10116
#define ERR_ADMIN_CANNOT_CREATE_IMAGE     10117
#define ERR_ADMIN_CANNOT_SAVE_FILE        10118
#define ERR_ADMIN_WRITE_LLOCC_DB_ERR      10119

/* other define */
#define MAX_DEL_FILE_NUM_ONCE		16

/*
 * webserver -> cut_svr
 */
struct cut_request_t {
    uint32_t    hostid;
    char        fdfs_url[FDFS_URL_LEN]; // url
    uint32_t    clip_w;
    uint32_t    clip_h;
    int32_t     start_x;
    int32_t     start_y;
    uint32_t    thumb_w;
    uint32_t    thumb_h;
}__attribute__((packed));

/*
 * cut_svr -> webserver
 */
struct cut_reponse_t {
    uint32_t    hostid;
    char fdfs_url[FDFS_URL_LEN];
}__attribute__((packed));

/*
 * metadata_dbsvr cmdid
 */
#define META_CHK_META   2030
#define META_ADD_META   2031
#define META_DEL_META   2032
#define META_GET_MD5    2040
#define META_ADD_MD5    2041
#define META_DEL_MD5    2042

/*
 * metadata_dbsvr errorid
 */
#define META_CMD_LEN_ERR 10540
#define META_CMD_ERR     10541
#define META_DB_ERR      10550
#define META_NOT_EXIST   10551
#define META_IS_EXIST    10552
#define META_NOT_HIT     10553
#define MD5_NOT_EXIST    10564
#define MD5_IS_EXIST     10565
/*
 * sync bus serv cmdid
 */
#define SYNC_BUS_SERV   0x10A4

/*
 * check session errorid
 */
#define CHECK_SESS_BAD_SESS 10601

/*
 * check session cmdid
 */
#define CHECK_SESS   0x10B5

#endif

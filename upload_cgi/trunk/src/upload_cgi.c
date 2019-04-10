#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <string>
#include <gd.h>
#include <set>

#include "libtaomee++/utils/md5.h"
extern "C" {
#include "libtaomee/crypt/qdes.h"
#include "libtaomee/dataformatter/bin_str.h"
#include "libtaomee/conf_parser/config.h"
#include "libtaomee/project/stat_agent/msglog.h"
#include "json/json.h"
#include "fdfs_client.h"
#include "logger.h"
}

#include "proto.h"
#include "common.h"
#include "log.h"
#include "cgic.h"
#include "tcpip.h"

#define FDFS_SUPPORT_THUMBNAIL 1
#ifdef FDFS_SUPPORT_THUMBNAIL
//Support thumbnail for 256 max
#define FDFS_THUMBNAIL_MAXSIZE 256
char *g_nail_file_buffer = NULL;
int  g_nail_file_size = 0;
char g_nail_suffix[SUFFIX_LEN];
char g_fdfs_url_nail[FDFS_URL_LEN + 20];
#define FDFS_SUPPORT_SQUARENAIL 1
//Support squarenail for 128 max
#ifdef FDFS_SUPPORT_SQUARENAIL
#define FDFS_SQUARENAIL_MAXSIZE 128
char *g_square_file_buffer = NULL;
int  g_square_file_size = 0;
char g_square_suffix[SUFFIX_LEN];
char g_fdfs_url_squarenail[FDFS_URL_LEN + 20];
#endif
#endif

CTcp *g_sync_bus_serv = NULL;
CTcp *g_meta_serv = NULL;
//CTcp *g_check_sess_serv = NULL;

int g_errorid = SUCC;
int g_upload_type = 0;
char session_key[512];
upload_request_t g_request;

cgiFilePtr g_cgi_file = NULL;

char *g_file_buffer = NULL;
int  g_file_size = 0;
char g_fdfs_url[FDFS_URL_LEN];
char g_suffix[SUFFIX_LEN];
char g_filename_on_server[MAX_FILENAME_LEN + 1] = {0};
char g_md5_str[MD5_LEN];

uint32_t g_photoid = 0;  
uint32_t g_hostid = 0;

#ifdef FDFS_SUPPORT_THUMBNAIL
static void __get_thumbnail(gdImagePtr p_image, int pic_type) { 
    int  nail_width = 0;
    int  nail_height = 0;
    if (gdImageSX(p_image) > FDFS_THUMBNAIL_MAXSIZE || gdImageSY(p_image) > FDFS_THUMBNAIL_MAXSIZE) {
        int max = gdImageSX(p_image) > gdImageSY(p_image) ? gdImageSX(p_image) : gdImageSY(p_image);
        nail_width = gdImageSX(p_image) * FDFS_THUMBNAIL_MAXSIZE / max;
        nail_height = gdImageSY(p_image) * FDFS_THUMBNAIL_MAXSIZE / max;
    } else {
        /*
         * TODO : Support limit to pic size (128KB)
         */
        nail_width = gdImageSX(p_image) / 2;
        nail_height = gdImageSY(p_image) / 2;
    }
    snprintf(g_nail_suffix, sizeof(g_nail_suffix), "_%dx%d",nail_width, nail_height);

    gdImagePtr p_nail_image;
    p_nail_image = gdImageCreateTrueColor(nail_width, nail_height);
    gdImageCopyResampled(p_nail_image, p_image, 0, 0, 0, 0, nail_width, nail_height, gdImageSX(p_image), gdImageSY(p_image));
    
    switch (pic_type) {
    case TYPE_JPG:
        g_nail_file_buffer = (char *)gdImageJpegPtr(p_nail_image, &g_nail_file_size, -1);
        break;
    case TYPE_PNG:
        g_nail_file_buffer = (char *)gdImagePngPtr(p_nail_image, &g_nail_file_size);
        break;
    case TYPE_GIF:
        g_nail_file_buffer = (char *)gdImageGifPtr(p_nail_image, &g_nail_file_size);
        break;
    }
    gdImageDestroy(p_nail_image);
}

#ifdef FDFS_SUPPORT_SQUARENAIL
static void __get_squarenail(gdImagePtr p_image, int pic_type) {
    int srcw = gdImageSX(p_image), srch = gdImageSY(p_image), srcx = 0, srcy = 0;
    int dstw = FDFS_SQUARENAIL_MAXSIZE, dsth = FDFS_SQUARENAIL_MAXSIZE;
    int dstx = 0, dsty = 0;

    if (srcw > FDFS_SQUARENAIL_MAXSIZE && srch > FDFS_SQUARENAIL_MAXSIZE) {
        //pic is bigger than 128 * 128 ,need to nail first
        int min = srcw > srch ? srch : srcw;
        srcx = (srcw - min) / 2;
        srcy = (srch - min) / 2;
        srcw = srch = min;
    } else {
        if (srcw > dstw) {
            //原图宽度大于128，则从中心位置裁剪出128的宽度
            srcx = (srcw - dstw) / 2;
            srcw = dstw;
        } else {
            dstx = (dstw - srcw) / 2;        //原图宽度小于128，设定画板填充宽度的起始位置
            dstw = srcw;
        }
 
        if (srch > dsth) {
            //原图高度大于128，则从中心位置裁剪出128的高度
            srcy = (srch - dsth) / 2;
            srch = dsth;
        } else {
            dsty = (dsth - srch) / 2;        //原图高度小于128，设定画板填充高度的起始位置
            dsth = srch;
        }
    }

    //初始化九宫格画板
    gdImagePtr p_nail_image;
    p_nail_image = gdImageCreateTrueColor(FDFS_SQUARENAIL_MAXSIZE, FDFS_SQUARENAIL_MAXSIZE);
    gdImageCopyResampled(p_nail_image, p_image, dstx, dsty, srcx, srcy, dstw, dsth, srcw, srch);
    
    snprintf(g_square_suffix, sizeof(g_square_suffix), "_%dx%d", FDFS_SQUARENAIL_MAXSIZE, FDFS_SQUARENAIL_MAXSIZE);
    switch (pic_type) {
    case TYPE_JPG:
        g_square_file_buffer = (char *)gdImageJpegPtr(p_nail_image, &g_square_file_size, -1);
        break;
    case TYPE_PNG:
        g_square_file_buffer = (char *)gdImagePngPtr(p_nail_image, &g_square_file_size);
        break;
    case TYPE_GIF:
        g_square_file_buffer = (char *)gdImageGifPtr(p_nail_image, &g_square_file_size);
        break;
    }
    gdImageDestroy(p_nail_image);
}
#endif
#endif

int fdfs_init()
{
    /* fdfs log init*/
    log_init_fdfsx();
    g_log_context.log_level = LOG_ERR;

    //if (fdfs_client_init("../../client.conf") != 0) {
    if (fdfs_client_init("../../client.conf") != 0) {
        CGI_ERROR_LOG("fdfs_client_init ../../client.conf");
        return FAIL;
    }

    return SUCC;
}

void fdfs_fini()
{
    tracker_close_all_connections();
    fdfs_client_destroy();
    log_destroy();
}

int fdfs_upload_file(const char *image_buf, uint32_t size, 
                     const char suffix[SUFFIX_LEN], char fdfs_url[FDFS_URL_LEN])
{
    int result;

    ConnectionInfo *p_tracker_server;
    p_tracker_server = tracker_get_connection();
    if (p_tracker_server == NULL) {
        CGI_ERROR_LOG("tracker_get_connection fail, errorno[%d] %s", \
                      errno, STRERROR(errno));
        return FAIL;
    }

    ConnectionInfo storage_server;
    int store_path_index;
    char group_name[64];
    *group_name = '\0';
    if ((result = tracker_query_storage_store(p_tracker_server, 
                                              &storage_server,
                                              group_name,
                                              &store_path_index)) != 0) {
        CGI_ERROR_LOG("tracker_query_storage fail, errorno[%d] %s", \
                      result, STRERROR(result));
        return FAIL;
    }

    result = storage_upload_by_filebuff1(p_tracker_server,
                                         &storage_server, store_path_index,
                                         image_buf, size, suffix,
                                         NULL, 0, "", fdfs_url);

    if (result != 0) {
        CGI_ERROR_LOG("storage_upload_by_filebuff1 fail, errorno[%d] %s", \
                      result, STRERROR(result));
        if (storage_server.sock >= 0) {
            fdfs_quit(&storage_server);
        }

        tracker_disconnect_server(&storage_server);
        return FAIL;
    }
    
#ifdef FDFS_SUPPORT_THUMBNAIL
    result = storage_upload_slave_by_filebuff1(p_tracker_server,
                                         &storage_server,
                                         g_nail_file_buffer, g_nail_file_size, fdfs_url, g_nail_suffix, suffix,
                                         NULL, 0, g_fdfs_url_nail);
    if (result != 0)
    {
        CGI_ERROR_LOG("storage_upload_slave_by_filebuff1 fail, errorno");
        if (storage_server.sock >= 0) {
            fdfs_quit(&storage_server);
        }

        tracker_disconnect_server(&storage_server);
        return FAIL;
    }
    
#ifdef FDFS_SUPPORT_SQUARENAIL
    result = storage_upload_slave_by_filebuff1(p_tracker_server,
                                         &storage_server,
                                         g_square_file_buffer, g_square_file_size, fdfs_url, g_square_suffix, suffix,
                                         NULL, 0, g_fdfs_url_squarenail);
    if (result != 0)
    {
        CGI_ERROR_LOG("storage_upload_slave_by_filebuff1 fail, errorno");
        if (storage_server.sock >= 0) {
            fdfs_quit(&storage_server);
        }

        tracker_disconnect_server(&storage_server);
        return FAIL;
    }
#endif
#endif
    return SUCC;
}

int fdfs_delete_file(const char fdfs_url[FDFS_URL_LEN])
{
    std::ofstream delete_log;
    delete_log.open(config_get_strval("delete_log"), std::ios_base::app);
    delete_log << fdfs_url << "\n";
    delete_log.flush();
    return SUCC;
}

void send_stat_data(const uint32_t file_size)
{
    uint32_t stat_msg_id;
    if(file_size >= (4*1024*1024)) {
        stat_msg_id = STAT_4M_to_5M_MSG_ID;
    } else if(file_size >= (2*1024*1024)) {
        stat_msg_id = STAT_2M_to_4M_MSG_ID;
    } else if(file_size >= (1*1024*1024)) {
        stat_msg_id = STAT_1M_to_2M_MSG_ID; 
    } else if(file_size >= (640*1024)) {
        stat_msg_id = STAT_640K_to_1M_MSG_ID;
    } else if(file_size >= (320*1024)) {
        stat_msg_id = STAT_320K_to_640K_MSG_ID;
    } else if(file_size >= (160*1024)) {
        stat_msg_id = STAT_160K_to_320K_MSG_ID;
    } else if(file_size >= (80*1024)) {
        stat_msg_id = STAT_80K_to_160K_MSG_ID;
    } else if(file_size >= (40*1024)) {
        stat_msg_id = STAT_40K_to_80K_MSG_ID;
    } else if(file_size >= (20*1024)) {
        stat_msg_id = STAT_20K_to_40K_MSG_ID;
    } else {
        stat_msg_id = STAT_00K_to_20K_MSG_ID;
    }
    CGI_DEBUG_LOG("stat_msg_id %x",stat_msg_id);
    uint32_t stat_cnt = 1;
    if(msglog(config_get_strval("stat_msg_log"), stat_msg_id, time(NULL), 
             (const char *)&stat_cnt, 4) == FAIL) {
        CGI_ERROR_LOG("send stat error");
    }
}

int check_image(const char *image_buf, int size, char suffix[SUFFIX_LEN])
{
    static uint8_t jpg_type[2] = {0xff, 0xd8};
    static uint8_t png_type[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    static uint8_t gif_type_1[6] = {0x47, 0x49, 0x46, 0x38, 0x39, 0x61};
    static uint8_t gif_type_2[6] = {0x47, 0x49, 0x46, 0x38, 0x37, 0x61};

    if (size <= 8)
        return FAIL;

    int pic_type;

    if (0 == memcmp(image_buf, jpg_type, sizeof(jpg_type))) {
        strcpy(suffix, "jpg");
        pic_type = TYPE_JPG;    //jpg
    } else if (0 == memcmp(image_buf, png_type, sizeof(png_type))) {
        strcpy(suffix, "png");
        pic_type = TYPE_PNG;    //png
    } else if (0 == memcmp(image_buf, gif_type_1, sizeof(gif_type_1)) || 
               0 == memcmp(image_buf, gif_type_2, sizeof(gif_type_2))) {
        strcpy(suffix, "gif");
        pic_type = TYPE_GIF;    //gif
    } else {
        pic_type = TYPE_UNKNOWN;    
    }

    gdImagePtr p_image;

    switch (pic_type) {
    case TYPE_JPG:
        p_image = gdImageCreateFromJpegPtr(size, (void*)image_buf);
        break;
    case TYPE_PNG:
        p_image = gdImageCreateFromPngPtr(size, (void*)image_buf);
        break;
    case TYPE_GIF:
        p_image = gdImageCreateFromGifPtr(size, (void*)image_buf);
        break;
    default:
        p_image = NULL;
        CGI_ERROR_LOG("bad pic type [%u]", pic_type);
        break;
    }

    if (!p_image)
        return FAIL;

    if (gdImageSX(p_image) < 5 || gdImageSY(p_image) < 5) {
        gdImageDestroy(p_image);
        return TOO_SMALL; 
    }

#ifdef FDFS_SUPPORT_THUMBNAIL
    //制作缩略图
    __get_thumbnail(p_image, pic_type);
#ifdef FDFS_SUPPORT_SQUARENAIL
    __get_squarenail(p_image, pic_type);
#endif
#endif

    gdImageDestroy(p_image);
    return SUCC;
}

void print_result()
{
    cgiHeaderContentType((char *)"text/html");
    json_object *jo = json_object_new_object();

    if(g_errorid != SUCC) {
        json_object_object_add(jo, "result", json_object_new_string("fail"));
        json_object_object_add(jo, "filename", json_object_new_string(g_filename_on_server));
        json_object_object_add(jo, "reason", json_object_new_int(g_errorid));
        goto PRINT_END;
    }

    json_object_object_add(jo, "result", json_object_new_string("success"));
    json_object_object_add(jo, "albumid", json_object_new_int(g_request.albumid));
    json_object_object_add(jo, "photoid", json_object_new_int(g_photoid));
    json_object_object_add(jo, "file_name", json_object_new_string(g_filename_on_server));
    json_object_object_add(jo, "hostid", json_object_new_int(g_hostid));
    json_object_object_add(jo, "lloc", json_object_new_string(g_fdfs_url));
    json_object_object_add(jo, "nail_lloc", json_object_new_string(g_fdfs_url_nail));
    json_object_object_add(jo, "square_lloc", json_object_new_string(g_fdfs_url_squarenail));
    json_object_object_add(jo, "len", json_object_new_int(g_file_size));

PRINT_END:
    /*
    json_object_object_add(jo, "mid", json_object_new_int(g_request.userid));
    json_object_object_add(jo, "ip", json_object_new_int(g_request.ip));
    json_object_object_add(jo, "albumid", json_object_new_int(g_request.albumid));
    json_object_object_add(jo, "time", json_object_new_int(g_request.time));
    json_object_object_add(jo, "weight_limit", json_object_new_int(g_request.width_limit));
    json_object_object_add(jo, "height_limit", json_object_new_int(g_request.height_limit));
    json_object_object_add(jo, "is_head", json_object_new_int(g_request.is_head));
     */
    if (g_upload_type) {
        fprintf(cgiOut, "<script type=\"text/javascript\">\ndocument.domain=\"61.com\";"
                "\nwindow.parent.uploadCallback(%s);\n</script>",
                json_object_to_json_string(jo));
    } else {
        fprintf(cgiOut, "%s\n", json_object_to_json_string(jo));
    }
    json_object_put(jo);
    return;
}

/*
int check_sess(const char *sess_key)
{
    g_check_sess_serv = new CTcp(config_get_strval("check_sess_serv"), 5, 10);
    if (!g_check_sess_serv->is_connect()) {
        CGI_ERROR_LOG("connect check sess server fail");
        return SUCC;
    }

    char send_buf[1024];
    memset(send_buf, 0, sizeof send_buf);
    int j = PROTO_H_SIZE + 128;
    strcpy(send_buf + PROTO_H_SIZE, sess_key);

    init_proto_head(send_buf, g_request.userid, CHECK_SESS, j);
    
    char *recv_buf = NULL;
    int recv_len = 0;

    int ret = g_check_sess_serv->do_net_io((const char *)send_buf, j, &recv_buf, &recv_len);

    if (ret != SUCC || (uint32_t)recv_len < PROTO_H_SIZE) {
        CGI_ERROR_LOG("send to check_sess_srv fail. ret %u", ret);
        if (recv_buf)
            free(recv_buf);
        return SUCC;
    }
    
    protocol_t* pkg_recv = (protocol_t*)recv_buf; 

    uint32_t result = pkg_recv->ret;

    if(recv_buf) 
        free(recv_buf);
    return result;
}
*/
int check_file()
{
    /* get upload type */
    if (cgiFormIntegerBounded((char*)"type", &g_upload_type, 0, 1, 0) != cgiFormSuccess) {
        CGI_ERROR_LOG("upload type upload_type:%d", g_upload_type);
        g_errorid = ERR_UPLOAD_TYPE;
        return FAIL;
    }

    if (cgiFormString((char*)"session_key", session_key, sizeof(session_key)) 
                      != cgiFormSuccess) {
        CGI_ERROR_LOG("session_key empty");
        g_errorid = ERR_SESSION_KEY;
        return FAIL;
    }
    CGI_DEBUG_LOG("session_key is : %s", session_key);

    /* check upload request */
    char upload_s_tmp[256];
    char d_tmp[256];

    int s_len = strlen(session_key);
    if (s_len != 64) {
        CGI_ERROR_LOG("session_key len != 64 len = %d", s_len);
        g_errorid = ERR_SESSION_KEY;
        return FAIL;
    }

    str2hex(session_key, s_len, upload_s_tmp);

    upload_s_tmp[s_len / 2] = '\0';
    des_decrypt_n(DECRYPT_KEY, upload_s_tmp, d_tmp, s_len / 16);

    uint32_t j = 0;
    UNPKG_H_UINT32(d_tmp, g_request.userid, j);
    UNPKG_H_UINT32(d_tmp, g_request.channel, j);
    UNPKG_H_UINT32(d_tmp, g_request.albumid, j);
    //UNPKG_H_UINT32(d_tmp, g_request.ip, j);
    UNPKG_UINT32(d_tmp, g_request.ip, j);
    UNPKG_H_UINT32(d_tmp, g_request.time, j);
    UNPKG_H_UINT32(d_tmp, g_request.width_limit, j);
    UNPKG_H_UINT32(d_tmp, g_request.height_limit, j);
    UNPKG_H_UINT32(d_tmp, g_request.is_head, j);
    CGI_DEBUG_LOG("UPLOAD_REQUEST[uid:%u ch:%u alb:%u ip:%u time:%u w_lim:%u h_lim:%u is_head:%u]", 
                  g_request.userid, g_request.channel, g_request.albumid, \
                  g_request.ip, g_request.time, g_request.width_limit, \
                  g_request.height_limit, g_request.is_head);

    //if(cgiFormFileName((char *)"file", g_filename_on_server, 
    //                   sizeof(g_filename_on_server)) != cgiFormSuccess) {   
    int res_ret = cgiFormFileName((char *)"file", g_filename_on_server, 
                       sizeof(g_filename_on_server));
    if ( res_ret != cgiFormSuccess) {
        CGI_ERROR_LOG("could not retrieve filename, result: %d, file: %s", res_ret, \
                      g_filename_on_server);
        g_errorid = ERR_RETRIEVE_FILENAME;
        return FAIL;
    }

    /*filter ie6/7 path */   
    std::string filename_str = g_filename_on_server;
    std::string::size_type str_pos = filename_str.rfind("\\");
    if (str_pos != std::string::npos) {
        std::string new_filename_str;
        new_filename_str = filename_str.substr(str_pos + 1); 
        strcpy(g_filename_on_server, new_filename_str.c_str());
    }

    /*filter json "\'" */   
    {
        uint32_t len = strlen(g_filename_on_server);
        for (uint32_t i = 0; i < len; ++i) {
            if (g_filename_on_server[i] == '\'')
                g_filename_on_server[i] = '_';
        }
    }

    /* check ip */
    char *bind_ip = config_get_strval("bind_ip");
        struct in_addr addr1;
        memcpy(&addr1, &(g_request.ip), 4);
    if(g_request.ip != inet_addr(bind_ip)){
        CGI_ERROR_LOG("request is forbidden bind_ip[%d %s],req_ip[%u %s]",\
                       inet_addr(bind_ip),bind_ip,g_request.ip,inet_ntoa(addr1));
        g_errorid = ERR_HOSTID;
        return FAIL;
    }

    cgiFormFileSize((char *)"file", &g_file_size);
    if(g_file_size > PIC_MAX_LEN) {
        CGI_ERROR_LOG("upload file too big");
        g_errorid = ERR_IMAGE_SIZE;
        return FAIL;
    }

    if (cgiFormFileOpen((char *)"file", &g_cgi_file) != cgiFormSuccess) {
        CGI_ERROR_LOG("could not open the file");
        g_errorid = ERR_OPEN_FILE;
        return FAIL;
    }

    /* connect sync bus serv */
#if 0
    g_sync_bus_serv = new CTcp(config_get_strval("sync_bus_serv"), 5, 10);
    if (!g_sync_bus_serv->is_connect()) {
        CGI_ERROR_LOG("can not connect sync bus serv %s", config_get_strval("sync_bus_serv"));
        g_errorid = ERR_SYNC_WEB_SERVER;
        return FAIL;
    }
    CGI_DEBUG_LOG("[%d]Connect sync bus serv %s",__LINE__, config_get_strval("sync_bus_serv"));
#endif
    /* check timeout 
    */
    time_t now_time = time(NULL);  
    if (now_time < g_request.time)
    {
        CGI_ERROR_LOG("upload timeout nowtime[%u] earlier than request.time[%u]", \
                      now_time, g_request.time);
        g_errorid = ERR_TIMEOUT;
        return FAIL;
    }
    if((now_time - g_request.time) > 1 * 60 * 60) {
        CGI_ERROR_LOG("upload timeout nowtime[%u] request.time[%u]", \
                      now_time, g_request.time);
        g_errorid = ERR_TIMEOUT;
        return FAIL;
    }
    /* check sess */
    /*
    if (check_sess(session_key) != SUCC) {
        CGI_ERROR_LOG("is bad session.[%u %s]", g_request.userid, session_key);
        g_errorid = ERR_TIMEOUT;
        return FAIL;
    }
    */

    /* read file */
    g_file_buffer = (char *)malloc(PIC_MAX_LEN);

    int got = 0;
    if (cgiFormFileRead(g_cgi_file, g_file_buffer, PIC_MAX_LEN, &got) != cgiFormSuccess
                       || got != g_file_size) {
        CGI_ERROR_LOG("cgiFormFileRead fail");
        g_errorid = ERR_READ_FILE;
        return FAIL;
    }

    /* check pic is valid & get suffix*/
    int check_ret = check_image(g_file_buffer, g_file_size, g_suffix);
    if (check_ret == FAIL) {
        CGI_ERROR_LOG("invalid file");
        g_errorid = ERR_INVALID_FILE;
        return FAIL;
    }

    if (check_ret == TOO_SMALL) {
        CGI_ERROR_LOG("too small file 5 x 5");
        g_errorid = ERR_FILE_TOO_SMALL;
        return FAIL;
    }

    return SUCC;
}

struct sync_bus_serv_t {
    uint32_t    uid;
    uint32_t    hostid;
    char        fdfs_url[FDFS_URL_LEN]; // url
    char        filename[100]; // filename
    uint32_t    filesize;
    uint32_t    albumid;
    uint32_t    is_head;
}__attribute__((packed));

struct sync_bus_proto_t {
    uint32_t len;
    uint32_t seq;
    uint16_t cmd;
    uint32_t rlt;
    uint32_t id;
    char body[];
}__attribute__((packed));

int sync_fdfs_url()
{
    /* hostid = config hostid + groupid */
    uint32_t groupid = 0;
    sscanf(g_fdfs_url, "g%u", &groupid);
    g_hostid = config_get_intval("hostid", 10) + groupid;

    char send_buf[1024];
    sync_bus_proto_t *ph = (sync_bus_proto_t *)send_buf;
    ph->len = sizeof(sync_bus_proto_t) + sizeof(sync_bus_serv_t);
    ph->seq = 0;
    ph->cmd = SYNC_BUS_SERV;
    ph->rlt = 0;
    ph->id = g_request.userid;
    
    sync_bus_serv_t *sbs = (sync_bus_serv_t *)(ph->body);
    sbs->uid = g_request.userid;
    sbs->hostid = g_hostid;
    memcpy(sbs->fdfs_url, g_fdfs_url, FDFS_URL_LEN);
    memcpy(sbs->filename, g_filename_on_server, 100);
    sbs->filesize = g_file_size;
    sbs->albumid = g_request.albumid;
    sbs->is_head = g_request.is_head;
    char *recv_buf = NULL;
    int recv_len = 0;
    int ret = g_sync_bus_serv->do_net_io((const char *)send_buf, ph->len, 
                                         &recv_buf, &recv_len);

    if (ret != SUCC) {
        CGI_ERROR_LOG("sync serv net error");
        g_errorid = ERR_SYNC_WEB_SERVER;
        return FAIL;
    }

    if ((uint32_t)recv_len < sizeof(sync_bus_proto_t)) {
        CGI_ERROR_LOG("sync serv return pkglen error.[%u]", recv_len);
        free(recv_buf);
        g_errorid = ERR_SYNC_WEB_SERVER;
        return FAIL;
    } 
    
    ph = (sync_bus_proto_t *)recv_buf;
    if (ph->rlt == 0) {
        if (ph->len == sizeof(sync_bus_proto_t) + 4) {
            g_photoid = *(uint32_t*)ph->body;
            free(recv_buf);
            CGI_DEBUG_LOG("sync serv return photoid[%u]", g_photoid); 
            return SUCC;
        }
        CGI_DEBUG_LOG("sync fail pkglen error.[%u]", recv_len);
        g_errorid = ERR_UNKNOWN;
        free(recv_buf);
        return FAIL;
    } 

    CGI_DEBUG_LOG("sync fail errorid[%u]", ph->rlt); 
    g_errorid = ph->rlt;
    free(recv_buf);
    return FAIL;
}

int check_meta()
{
    g_meta_serv = new CTcp(config_get_strval("meta_db_serv"), 5, 10);
    if (!g_meta_serv->is_connect())
        return FAIL;

    /* get MD5 */
    utils::MD5 md5;
    md5.update(g_file_buffer, g_file_size);
    strcpy(g_md5_str, md5.toString().c_str());
    CGI_DEBUG_LOG("MD5[%s]", g_md5_str);
    char send_buf[1024];
    memset(send_buf, 0, sizeof send_buf);
    int j = PROTO_H_SIZE;
    PKG_STR(send_buf, g_md5_str, j, MD5_LEN);
    init_proto_head(send_buf, g_request.userid, META_CHK_META, j);
    
    char *recv_buf = NULL;
    int recv_len = 0;

    int ret = g_meta_serv->do_net_io((const char *)send_buf, j, &recv_buf, &recv_len);

    if (ret != SUCC || (uint32_t)recv_len < PROTO_H_SIZE) {
        CGI_ERROR_LOG("chk_md5_from_db error. ret %u", ret);
        free(recv_buf);
        return FAIL;
    }
    
    protocol_t* pkg_recv = (protocol_t*)recv_buf; 
    if (pkg_recv->ret == 0 && recv_len == PROTO_H_SIZE + FDFS_URL_LEN) {
        memcpy(g_fdfs_url, recv_buf + PROTO_H_SIZE, FDFS_URL_LEN);
        CGI_INFO_LOG("UPLOAD HIT[%s %u]", g_fdfs_url, g_request.userid);
        free(recv_buf);
        return SUCC;
    }
    uint32_t result = pkg_recv->ret;
    if(recv_buf) 
        free(recv_buf);
    return result;
}

void sync_meta()
{
    char send_buf[1024];
    memset(send_buf, 0, sizeof send_buf);
    int j = PROTO_H_SIZE;
    PKG_STR(send_buf, g_md5_str, j, MD5_LEN);
    PKG_STR(send_buf, g_fdfs_url, j, FDFS_URL_LEN);
    
    /*insert md5 fdfs_url into meta table*/
    init_proto_head(send_buf, g_request.userid, META_ADD_META, j);
    
    char *recv_buf = NULL;
    int recv_len = 0;
    int ret = g_meta_serv->do_net_io((const char *)send_buf, j, &recv_buf, &recv_len);
    if (ret != SUCC || recv_len != PROTO_H_SIZE) {
        CGI_ERROR_LOG("insert md5 fdfs_url into meta table fail");
        free(recv_buf);
        return;
    }

    protocol_t* pkg_recv = (protocol_t*)recv_buf; 
    if (pkg_recv->ret != SUCC) {
        CGI_ERROR_LOG("META_ADD_META error. errorid[%u]", pkg_recv->ret);
        free(recv_buf);
        return;
    }
    free(recv_buf);
    return;
}

void upload_file()
{
    /* init fdfs */
    if (fdfs_init() == FAIL) {
        CGI_ERROR_LOG("fdfs_init fail");
        g_errorid = ERR_UNKNOWN;
        goto RETURN;
    }

    /* upload */
    if (fdfs_upload_file(g_file_buffer, g_file_size, g_suffix, g_fdfs_url) == FAIL) {
        CGI_ERROR_LOG("fdfs_upload_file fail");
        g_errorid = ERR_UNKNOWN;
        goto RETURN;
    }

    CGI_INFO_LOG("UPLOAD [%s][%u]", g_fdfs_url, g_request.userid);
#if 0
    sync_meta();

    if (sync_fdfs_url() == FAIL) {
        CGI_INFO_LOG("SYNC N[%s]", g_fdfs_url);
        fdfs_delete_file(g_fdfs_url);
        goto RETURN;
    }

    send_stat_data(g_file_size);
#endif
RETURN:
    fdfs_fini();
}

int cgiMain(void)
{
    /* my log init*/
    cgi_log_init("../log/", (log_lvl_t)8, LOG_SIZE, 0, "");
 
    /* load config file */
    //if (FAIL == config_init("../etc/bench.conf")) {
    if (FAIL == config_init("../etc/bench.conf")) {
        CGI_ERROR_LOG("read conf_file error");
        g_errorid = ERR_UNKNOWN;
        goto RETURN;
    }
    
        CGI_ERROR_LOG("fdfs_init success");
    /* check file */
    if (FAIL == check_file())
    {
        CGI_ERROR_LOG("[%d] check file failed %d", __LINE__, g_errorid);
        goto RETURN;
    }

    /* check meta */
#if 0
    if (SUCC == check_meta()) {
        if (sync_fdfs_url() == FAIL) {
            CGI_INFO_LOG("SYNC N[%s]", g_fdfs_url);
            fdfs_delete_file(g_fdfs_url);
            goto RETURN;
        }
        send_stat_data(g_file_size);
        CGI_ERROR_LOG("[%d] sync file %d", __LINE__, g_errorid);
        goto RETURN;
    }
#endif    
    /* upload */
    upload_file();

RETURN:
    print_result();

    config_exit();

    /*
    if (g_check_sess_serv)
        delete g_check_sess_serv;
    */

    if (g_sync_bus_serv)
        delete g_sync_bus_serv;

    if (g_meta_serv)
        delete g_meta_serv;

    if (g_cgi_file)
        cgiFormFileClose(g_cgi_file);

    if (g_file_buffer)
        free(g_file_buffer);

#ifdef FDFS_SUPPORT_THUMBNAIL
    if (g_nail_file_buffer)
        gdFree(g_nail_file_buffer);
#ifdef FDFS_SUPPORT_SQUARENAIL
    if (g_square_file_buffer)
        gdFree(g_square_file_buffer);
#endif
#endif
    return SUCC;
}

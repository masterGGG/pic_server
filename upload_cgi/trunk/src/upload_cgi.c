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

#ifdef FDFS_SUPPORT_THUMBNAIL
#include "hiredis.h"
//Support thumbnail for 256 max
char *g_nail_file_buffer = NULL;
int  g_nail_file_size = 0;
char g_nail_suffix[SUFFIX_LEN];
char g_fdfs_url_nail[FDFS_URL_LEN + 20];
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
#define CREATE_PIC_CHECK_NOTIFICATION "LPUSH image:check:queue %s-%d-%s"
static int __checkPictures() {
    redisContext *ctx;
    int ret = SUCC,code = 0;
    struct timeval tm = { 1, config_get_intval("cache_timeout", 5000)};
    ctx = redisConnectWithTimeout(config_get_strval("cache_ip"), config_get_intval("cache_port", 6379), tm);
    if (ctx == NULL) {
        CGI_ERROR_LOG("Can not allocate redis context");
        ret = FAIL;
        goto ERR;
    } else if (ctx->err) {
        CGI_ERROR_LOG("Can not connect to redis server %s:%d %s", config_get_strval("cache_ip"), config_get_intval("cache_port", 6379), ctx->errstr);
        redisFree(ctx);
        ctx = NULL;
        ret = FAIL;
        goto ERR;
    }

    redisReply *reply;
    redisAppendCommand(ctx, "auth %s", config_get_strval("cache_pass"));
    code = redisGetReply(ctx, (void **)&reply);
    if (code == REDIS_ERR) {
        CGI_ERROR_LOG("[REDIS]:Redis Server pass:% err:%s", config_get_strval("cache_pass"), ctx->errstr);
        freeReplyObject(reply);
        ret = FAIL;
        goto ERR;
    }
    char query[256];
    struct in_addr addr1;
    memcpy(&addr1, &(g_request.ip), 4);

    snprintf(query, sizeof(query), CREATE_PIC_CHECK_NOTIFICATION, inet_ntoa(addr1), g_request.userid, g_fdfs_url_nail);
    redisAppendCommand(ctx, query);
    code = redisGetReply(ctx, (void **)&reply);
    if (code == REDIS_ERR) {
        CGI_ERROR_LOG("[REDIS]:Redis Server failed push pic to message queue err:%s", ctx->errstr);
        freeReplyObject(reply);
        ret = FAIL;
        goto ERR;
    }
ERR:
    if (reply != NULL)
        freeReplyObject(reply);
    if (ctx != NULL)
        redisFree(ctx);
    return ret;
}
static void __get_thumbnail(gdImagePtr p_image, int pic_type) { 
    /*
     * 缩略图默认尺寸为原图尺寸，只有图片长宽过大，且图片大小过大才会压缩。
     */
    int  nwidth = gdImageSX(p_image);
    int  nheight = gdImageSY(p_image);

    int max_size = 1024 * config_get_intval("thumbnail_max_size", 128);
    int min_edge = config_get_intval("thumbnail_min_edge", 256);
//    int rate = config_get_intval("thumbnail_rate", 50);

        
    if (g_file_size > max_size) {
        if (nwidth > min_edge) {
            nheight = nheight * min_edge / nwidth;
            nwidth = min_edge;
        }/* else {
            nwidth = nwidth * rate / 100;
            nheight = nheight * rate / 100;
        }*/
    }
    
    snprintf(g_nail_suffix, sizeof(g_nail_suffix), "_%dx%d",nwidth, nheight);

    gdImagePtr p_nail_image;
    p_nail_image = gdImageCreateTrueColor(nwidth, nheight);
    //设置保存PNG时保留透明通道信息
    gdImageSaveAlpha(p_nail_image, true);
    //拾取一个完全透明的颜色,最后一个参数127为全透明
    int color = gdImageColorAllocateAlpha(p_nail_image, 255, 255, 255, 127);
    //使用颜色通道填充，背景色透明
    gdImageFill(p_nail_image, 0, 0, color);
    gdImageCopyResampled(p_nail_image, p_image, 0, 0, 0, 0, nwidth, nheight, gdImageSX(p_image), gdImageSY(p_image));
    
#if 0 
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
#else
    //缩略图不分类型，全部存储为jpeg格式
    g_nail_file_buffer = (char *)gdImageJpegPtr(p_nail_image, &g_nail_file_size, 90);
#endif
    gdImageDestroy(p_nail_image);
}

#ifdef FDFS_SUPPORT_SQUARENAIL
static void __get_squarenail(gdImagePtr p_image, int pic_type) {
    int srcw = gdImageSX(p_image), srch = gdImageSY(p_image), srcx = 0, srcy = 0;

    int max = config_get_intval("thumbsquare_edge", FDFS_SQUARENAIL_MAXSIZE);
    //配置一个九宫格图片的后缀，用以保持图片名称不变的情况下，修改图片内部的尺寸
    char *suffix = config_get_strval("thumbsquare_suffix");
    int dstw = max, dsth = max;
    int dstx = 0, dsty = 0;
    snprintf(g_square_suffix, sizeof(g_square_suffix), "_%s", suffix);
#if 0
    if ((srcw == 2 * dstw) && (srch == dsth * 2)) {
        return ;
    }
#endif

    if ((srcw > max) && (srch > max)) {
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
    p_nail_image = gdImageCreateTrueColor(max, max);

    //设置保存PNG时保留透明通道信息
    gdImageSaveAlpha(p_nail_image, true);
    //拾取一个完全透明的颜色,最后一个参数127为全透明
    int color = gdImageColorAllocateAlpha(p_nail_image, 255, 255, 255, 127);
    //使用颜色通道填充，背景色透明
    gdImageFill(p_nail_image, 0, 0, color);
    gdImageCopyResampled(p_nail_image, p_image, dstx, dsty, srcx, srcy, dstw, dsth, srcw, srch);
#ifdef DEBUG 
    CGI_DEBUG_LOG("[%d] src:{x:%d, y:%d, w:%d, h:%d}", __LINE__, srcx, srcy, srcw, srch);
    CGI_DEBUG_LOG("[%d] dst:{x:%d, y:%d, w:%d, h:%d}", __LINE__, dstx, dsty, dstw, dsth);
    CGI_DEBUG_LOG("[%d] pic width:%d, height:%d", __LINE__, gdImageSX(p_nail_image), gdImageSY(p_nail_image));
#endif

#if 0
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
#else
    g_square_file_buffer = (char *)gdImageJpegPtr(p_nail_image, &g_square_file_size, 90);
#endif
    gdImageDestroy(p_nail_image);
}
#endif
#endif

int fdfs_init()
{
#ifdef DEBUG
    CGI_DEBUG_LOG("[%d] fdfs init ing...", __LINE__);
#endif
    /* fdfs log init*/
    log_init_fdfsx();
    g_log_context.log_level = LOG_ERR;

    if (fdfs_client_init(config_get_strval("conf_filename")) != 0) {
        CGI_ERROR_LOG("fdfs_client_init %s", config_get_strval("conf_filename"));
        return FAIL;
    }
#ifdef DEBUG
    CGI_DEBUG_LOG("[%d] fdfs init success.", __LINE__);
#endif

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
#ifdef DEBUG
    CGI_DEBUG_LOG("[%d] src<%s> size %d.", __LINE__, fdfs_url, size);
#endif
    
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
#ifdef DEBUG
    CGI_DEBUG_LOG("[%d] src nail<%s> size %d.", __LINE__, g_fdfs_url_nail, g_nail_file_size);
#endif
    
#ifdef FDFS_SUPPORT_SQUARENAIL
    if (0 != strcmp(g_square_suffix, g_nail_suffix)) {
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
    } else 
        strcpy(g_fdfs_url_squarenail, g_fdfs_url_nail);
#ifdef DEBUG
    CGI_DEBUG_LOG("[%d] src<%s> size %d.", __LINE__, g_fdfs_url_squarenail, g_square_file_size);
#endif
#endif
    return __checkPictures();
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

    gdImagePtr p_image_src, p_image = NULL;

    switch (pic_type) {
    case TYPE_JPG:
        p_image_src = gdImageCreateFromJpegPtr(size, (void*)image_buf);
        break;
    case TYPE_PNG:
        p_image_src = gdImageCreateFromPngPtr(size, (void*)image_buf);
        break;
    case TYPE_GIF:
        p_image_src = gdImageCreateFromGifPtr(size, (void*)image_buf);
        break;
    default:
        p_image_src = NULL;
        CGI_ERROR_LOG("bad pic type [%u]", pic_type);
        break;
    }

    if (!p_image_src)
        return FAIL;

    //GIF格式图片不需要旋转操作，直接跳过
    if (pic_type != TYPE_GIF) {
        //拾取一个完全透明的颜色,最后一个参数127为全透明
        int gdcolor = gdImageColorAllocateAlpha(p_image_src, 255, 255, 255, 127);
        switch (g_request.is_head) {
        case 3:
                p_image = gdImageRotateInterpolated(p_image_src, 180, gdcolor);
                gdImageDestroy(p_image_src);
            break;
        case 6:
            p_image = gdImageRotateInterpolated(p_image_src, 270, gdcolor);
            gdImageDestroy(p_image_src);
            break;
        case 8:
            p_image = gdImageRotateInterpolated(p_image_src, 90, gdcolor);
            gdImageDestroy(p_image_src);
            break;
        default:
            break;
        }
    }

    //对于不需要旋转的图片，直接使用原始gd指针即可
    if (p_image == NULL) 
        p_image = p_image_src;
    else {
        if (g_file_buffer)
            free(g_file_buffer);
        switch (pic_type) {
        case TYPE_JPG:
            g_file_buffer = (char*)gdImageJpegPtr(p_image, &g_file_size, 100);
            break;
        case TYPE_PNG:
            g_file_buffer = (char*)gdImagePngPtr(p_image, &g_file_size);
            break;
        }
    }

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
#ifdef FDFS_SUPPORT_THUMBNAIL
    json_object_object_add(jo, "nail_lloc", json_object_new_string(g_fdfs_url_nail));
    json_object_object_add(jo, "square_lloc", json_object_new_string(g_fdfs_url_squarenail));
#endif
    json_object_object_add(jo, "len", json_object_new_int(g_file_size));

PRINT_END:
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
#ifdef DEBUG
    CGI_DEBUG_LOG("session_key is : %s", session_key);
#endif
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
#ifdef DEBUG
    CGI_DEBUG_LOG("UPLOAD_REQUEST[uid:%u ch:%u alb:%u ip:%u time:%u w_lim:%u h_lim:%u is_head:%u]", 
                  g_request.userid, g_request.channel, g_request.albumid, \
                  g_request.ip, g_request.time, g_request.width_limit, \
                  g_request.height_limit, g_request.is_head);
#endif
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

    /* check timeout 
    */
    time_t now_time = time(NULL);  
    if (now_time < g_request.time - 5 * 60)
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
#ifdef DEBUG
    CGI_INFO_LOG("UPLOAD [%s][%u]", g_fdfs_url, g_request.userid);
#endif
RETURN:
    fdfs_fini();
}

int cgiMain(void)
{
    /* my log init*/
    cgi_log_init("../log/", (log_lvl_t)8, LOG_SIZE, 0, "");
 
    /* load config file */
    if (FAIL == config_init("../etc/bench.conf")) {
        CGI_ERROR_LOG("read conf_file error");
        g_errorid = ERR_UNKNOWN;
        goto RETURN;
    }
    
    /* check file */
    if (FAIL == check_file())
    {
        CGI_ERROR_LOG("[%d] check file failed %d", __LINE__, g_errorid);
        goto RETURN;
    }

    /* upload */
    upload_file();

RETURN:
    print_result();

    config_exit();

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

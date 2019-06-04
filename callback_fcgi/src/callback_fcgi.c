#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include "include/fcgi_stdio.h"
#include <gd.h>

extern "C"{
#include <libtaomee/conf_parser/config.h>
#include <logger.h>
}

#include "proto.h"
#include "log.h"
#include "common.h"

#include <curl/curl.h>
#include <curl/easy.h>
#define MAX_URL_LEN 128

#define MAX_BUF_LEN 1024
enum {
    _RESULT_OK = 0,
    _RESULT_INTERNAL_ERR,
    _RESULT_INVALID_URL,
    _RESULT_INVALID_CODE,
    
    _RESULT_SOURCE_NOT_FOUND,
    _RESULT_FILE_OP_ERR,
    _RESULT_NAIL_ERR,
    _RESULT_SQUARE_ERR,

    _RESULT_SYNC_CDN_ERR,
    _RESULT_SYNC_CDN_NAIL_ERR,
    _RESULT_SYNC_CDN_SQUARE_ERR,
    
    _RESULT_UNALLOWED_METHOD,
    _RESULT_304_STATS
};
const static char *descript[] = {
    "success",
    "Cgi internal error"
    "Invalid param url",
    "Invalid param review code",

    "File move failed"
    "Source picture not found",
    "Nail picture handle error",
    "Square picture handle error",
    
    "Source synchronous CDN failed",
    "Nail synchronous CDN failed",
    "Square synchronous CDN failed",
    "invalid http method",
    "304"
};
int g_errorid;
char *img_prefix = nullptr;
char *review_prefix = nullptr;
char *root = nullptr;
char *err_root = nullptr;
int square_size = 128;
char *base = nullptr;

static void print_result() {
    printf("Content-type: text/plain\r\n\r\n"); 
    printf("\{\"result\":\"%s\"}\r\n", descript[g_errorid]);
}

/**
 * @brief _move 文件移动函数
 *
 * @param src   文件源地址
 * @param dst   文件目的地址
 *
 * @return bool 文件拷贝成功与否
 */
static bool _move(const char *src, const char *dst) {
    if (access(src, F_OK)) {
        CGI_ERROR_LOG("[%d]Src img <%s> not exist...", __LINE__, src);
        g_errorid = _RESULT_SOURCE_NOT_FOUND;
        return false;
    }

    if (rename(src, dst)) {
        CGI_ERROR_LOG("[%d]: mv <%s> to <%s> failed...", __LINE__, src, dst);
        g_errorid = _RESULT_FILE_OP_ERR;
        return false;
    }
    
    return true;
}

/**
 * @brief _crashCDN 删除源站的图片资源后，强制CDN刷新缓存
 *
 * @param src       图片CDN中的访问地址
 * @param nail      缩略图地址
 * @param square    九宫格图访问地址
 *
 * @return  CDN刷新成功与否
 */
static bool _crashCDN(const char *src, const char *nail, const char *square) {
    CURL *curl = nullptr;
    CURLcode res;
    struct curl_slist *headers = nullptr;
    //headers = curl_slist_append(headers, "Accept: Agent-007");
    curl = curl_easy_init();
    if (curl) {
        char url[1024];
        memset(url, 0, sizeof(url));
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        snprintf(url, sizeof(url), "%s=%s", base, src+strlen(root));
        curl_easy_setopt(curl, CURLOPT_URL, url);
        
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) { 
            CGI_ERROR_LOG("[%d]CURL SYNC <%s> <%s> ...", __LINE__, url, curl_easy_strerror(res));
            g_errorid = _RESULT_SYNC_CDN_ERR;
        }

        snprintf(url, sizeof(url), "%s=%s", base, nail+strlen(root));
        curl_easy_setopt(curl, CURLOPT_URL, url);
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            CGI_ERROR_LOG("[%d]CURL SYNC <%s> <%s> ...", __LINE__, url, curl_easy_strerror(res));
            g_errorid = _RESULT_SYNC_CDN_NAIL_ERR;
        }

        snprintf(url, sizeof(url), "%s=%s", base, square+strlen(root));
        curl_easy_setopt(curl, CURLOPT_URL, url);
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            CGI_ERROR_LOG("[%d]CURL SYNC<%s> <%s> ...", __LINE__, url, curl_easy_strerror(res));
            g_errorid = _RESULT_SYNC_CDN_SQUARE_ERR;
            goto ERR;
        }

    } else {
        g_errorid = _RESULT_INTERNAL_ERR;
        goto ERR;
    }
    
    return true;
ERR:
    if (curl)
        curl_easy_cleanup(curl);
    return false;
}

static int _getFileSize(const char *filename) {
    struct stat file_info;
    if (stat(filename, &file_info) < 0 ) {
        CGI_ERROR_LOG("[%d]can not get src img <%s> length...", __LINE__, filename);
        g_errorid = _RESULT_SOURCE_NOT_FOUND;
        return 0;
    }
    return file_info.st_size;
}

/**
 * @brief _getNailSize 获取缩略图的尺寸，用于图片恢复时尺寸与地址的对应关系
 *
 * @param p
 * @param w
 * @param h
 * @param src
 *
 * @return 
 */
static bool _getNailSize(gdImagePtr p, int &w, int &h, const char *src) {
    int nail_w, nail_h;
    int max_edge = config_get_intval("thumbnail_max_edge", 256);
    int max_size = config_get_intval("thumbnail_max_size", 128);
    int file_size = _getFileSize(src);
    if (file_size == 0)
        return false;

    if (nullptr == p) {
        gdImagePtr p_gd = gdImageCreateFromFile(src);
        if (nullptr != p_gd)
            return false;
        nail_w = gdImageSX(p_gd);
        nail_h = gdImageSY(p_gd);
        gdImageDestroy(p_gd);
    } else {
        nail_w = gdImageSX(p);
        nail_h = gdImageSY(p);
    }

    if (file_size > max_size) {
        if (nail_w > max_edge) {
            h = nail_h * max_edge / nail_w;
            w = max_edge;
        }
    } else {
        h = nail_h;
        w = nail_w;
    }
    
    return true;
}

static bool _recoverSrc(const char *src, const char *dst) {
    return _move(src, dst);
}

static bool _recoverNail(gdImagePtr _im, int w, int h, const char*filename) {
    gdImagePtr _nail_image;
    _nail_image = gdImageCreateTrueColor(w, h);
    //设置保存PNG时保留透明通道信息
    gdImageSaveAlpha(_nail_image, true);
    //拾取一个完全透明的颜色,最后一个参数127为全透明
    int color = gdImageColorAllocateAlpha(_nail_image, 255, 255, 255, 127);
    //使用颜色通道填充，背景色透明
    gdImageFill(_nail_image, 0, 0, color);
    gdImageCopyResampled(_nail_image, _im, 0, 0, 0, 0, w, h, gdImageSX(_im), gdImageSY(_im));
   
#if 0
    //pass：需要将缩略图质量提高，并保存成jpeg格式,直接用文件名保存函数达不到效果
    if (gdImageFile(p_nail_image, filename)) {
        gdImageDestroy(p_nail_image);
        return true;
    }
#endif

    FILE *_fin = fopen(filename, "wb");
    if (_fin != nullptr) {
#if 1
        int _nail_len = 0;
        char *_nail_buf = (char *)gdImageJpegPtr(_nail_image, &_nail_len, 90);
        
        fwrite(_nail_buf, _nail_len, 1, _fin);
        gdFree(_nail_buf);
        _nail_buf = nullptr;
#else
        //gdImageJpeg函数会产生阻塞，未找到解决办法。
        gdImageJpeg(_nail_image, _fin, 90);
#endif
        fclose(_fin);
        gdImageDestroy(_nail_image);
        return true;
    } else {
        g_errorid = _RESULT_NAIL_ERR;

        gdImageDestroy(_nail_image);
        return false;
    }
}

static bool _recoverSquare(gdImagePtr p_image, const char *filename) {
    int srcw = gdImageSX(p_image), srch = gdImageSY(p_image), srcx = 0, srcy = 0;
    int max = square_size;
    int dstw = max, dsth = max;
    int dstx = 0, dsty = 0;
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
#if 0 
    if (gdImageFile(p_nail_image, filename)) {
        gdImageDestroy(p_nail_image);
        return true;
    }
#else
    FILE *_fin = fopen(filename, "wb");
    if (_fin != nullptr) {
        int _nail_len = 0;
        char *_nail_buf = (char *)gdImageJpegPtr(p_nail_image, &_nail_len, 90);
        fwrite(_nail_buf, _nail_len, 1, _fin);
        gdFree(_nail_buf);
        _nail_buf = nullptr;
        fclose(_fin);
        gdImageDestroy(p_nail_image);
        return true;
    }
#endif
    gdImageDestroy(p_nail_image);
    g_errorid = _RESULT_SQUARE_ERR;
    return false;
}

static void _recover(char *path) {
    char src[MAX_URL_LEN], dst[MAX_URL_LEN];
    char dst_nail[MAX_URL_LEN], dst_square[MAX_URL_LEN];
    int nail_w = 0, nail_h = 0; 

    snprintf(src, sizeof(src), "%s%s", err_root, path);
    snprintf(dst, sizeof(dst), "%s%s", root, path);

    if (!_recoverSrc(src, dst))
        return ;

    char *type = strrchr(path, '.');
    if (type == nullptr) {
        CGI_ERROR_LOG("[%d]delete src:<%s> error file type...", __LINE__, path);
        g_errorid = _RESULT_INVALID_URL;
        return;
    }
    *type++ = '\0';
    snprintf(dst_square, sizeof(dst_square), "%s%s_%dx%d.%s", root, path, square_size, square_size, type);

    gdImagePtr p_gd = gdImageCreateFromFile(dst);
    if (nullptr == p_gd) {
        CGI_ERROR_LOG("[%d]recover src:<%s> error open...", __LINE__, dst);
        g_errorid = _RESULT_FILE_OP_ERR;
        goto ERR;
    }

    if (!_getNailSize(p_gd, nail_w, nail_h, dst))
        goto ERR;

    snprintf(dst_nail, sizeof(dst_nail), "%s%s_%dx%d.%s", root, path, nail_w, nail_h, type);

    if (!_recoverNail(p_gd, nail_w, nail_h, dst_nail))
        goto ERR;
    
    if (!_recoverSquare(p_gd, dst_square))
        goto ERR;

ERR:
    if (nullptr != p_gd)
        gdImageDestroy(p_gd);
    return;
}

static void _delete(char *path) {
    char src[MAX_URL_LEN], dst[MAX_URL_LEN];
    char src_nail[MAX_URL_LEN], src_square[MAX_URL_LEN];

    snprintf(src, sizeof(src), "%s%s", root, path);
    snprintf(dst, sizeof(dst), "%s%s", err_root, path);
    
    if (access(src, F_OK)) {
        CGI_ERROR_LOG("[%d]Src img <%s> not exist...", __LINE__, src);
        g_errorid = _RESULT_SOURCE_NOT_FOUND;
        return ;
    }

    char *type = strrchr(path, '.');
    if (type == nullptr) {
        CGI_ERROR_LOG("[%d]delete src img <%s> error file type...", __LINE__, path);
        g_errorid = _RESULT_INVALID_URL;
        return;
    }
    *type++ = '\0';
    snprintf(src_square, sizeof(src_square), "%s%s_%dx%d.%s", root, path, square_size, square_size, type);
    
    gdImagePtr p_gd = gdImageCreateFromFile(src);
    int nail_w = 0, nail_h = 0; 

    if (!_getNailSize(p_gd, nail_w, nail_h, src)) {
        gdImageDestroy(p_gd);
        return ;
    }
    gdImageDestroy(p_gd);

    snprintf(src_nail, sizeof(src_nail), "%s%s_%dx%d.%s", root, path, nail_w, nail_h, type);
    
    if (!_move(src, dst))
        return ;

    if (0 != remove(src_nail)) {
        CGI_ERROR_LOG("[%d]remove <%s> error ", __LINE__, src_nail);
        g_errorid = _RESULT_NAIL_ERR;
        return;
    }
    if (0 != remove(src_square)) {
        CGI_ERROR_LOG("[%d]remove <%s> error ", __LINE__, src_square);
        g_errorid = _RESULT_SQUARE_ERR;
        return;
    }
    if (!_crashCDN(src, src_nail, src_square)) {
        CGI_ERROR_LOG("[%d]crash CDN <%s> error ", __LINE__, src_square);
        return;
    }
}

static void handle(char *para_list, const char *img_prefix, const char*review_prefix){
    char *path = nullptr;
    int review = -1;
    char *pos = para_list;

    if (strncmp(para_list, img_prefix, strlen(img_prefix))) {
        CGI_ERROR_LOG("[%d]parse first parameter img_prefix failed <%s> ...", __LINE__, para_list);
        g_errorid = _RESULT_INVALID_URL;
        return ;
    }
    pos += strlen(img_prefix);
    if ((pos != nullptr) && (*pos == '=') && ((pos++) + 1 != nullptr)) 
        path=pos;
    else {
        CGI_ERROR_LOG("[%d]parse img_prefix failed <%s> ...", __LINE__, para_list);
        g_errorid = _RESULT_INVALID_URL;
        return ;
    }
    
    if ((pos = strchr(para_list, '&')) == nullptr) {
        CGI_ERROR_LOG("[%d]parse delimer & failed <%s> ...", __LINE__, path);
        g_errorid = _RESULT_INVALID_URL;
        return ;
    }
    *pos++ = '\0';
    if (pos == nullptr || strncmp(pos, review_prefix, strlen(review_prefix))) {
        CGI_ERROR_LOG("[%d]parse review_prefix failed <%s> ...", __LINE__, pos);
        g_errorid = _RESULT_INVALID_URL;
        return ;
    }
    pos += strlen(review_prefix);
    if ((pos != nullptr) && (*pos == '=') && (nullptr != pos + 1))
        review = atoi(pos + 1);
    else {
        CGI_ERROR_LOG("[%d]parse review failed <%s> ...", __LINE__, pos);
        g_errorid = _RESULT_INVALID_CODE;
        return ;
    }
    
    if (0 == review)
        _recover(path);
    else if (1 == review)
        _delete(path);
    else
        g_errorid = _RESULT_INVALID_URL;
}

int main(void) {
    char para_list[MAX_BUF_LEN];
    /* load config file */
    if (FAIL == config_init("../etc/bench.conf")) {
        CGI_ERROR_LOG("read conf_file error");
        return FAIL;
    }
    
    img_prefix = config_get_strval("img_prefix");
    review_prefix = config_get_strval("review_prefix");
    root = config_get_strval("thumb_root");
    err_root = config_get_strval("thumb_err_root");
    square_size = config_get_intval("thumb_square_edge", 128);
    base = config_get_strval("CDN_url");

    if (img_prefix == nullptr 
            || review_prefix == nullptr 
            || root == nullptr
            || err_root == nullptr
            || base == nullptr) {
        CGI_ERROR_LOG("read conf_file error, img/review prefix needed");
        return FAIL;
    }

    /* my log init*/
    //cgi_log_init("../log/", (log_lvl_t)8, LOG_SIZE, 0, "");
    cgi_log_init(config_get_strval("log_path"), (log_lvl_t)config_get_intval("log_level", 8), LOG_SIZE, 0, "");
    
#ifdef DEBUG
    int fcgi_request_cnt = 0;

    CGI_DEBUG_LOG("fastcgi begin");
#endif
    while (FCGI_Accept() >= 0) {	
#ifdef DEBUG
        fcgi_request_cnt++;
        CGI_DEBUG_LOG("----Request number %d running on host %s Process ID: %d", \
                      fcgi_request_cnt, getenv("SERVER_NAME"), getpid());
#endif
        g_errorid = _RESULT_OK;
        /* check request method */
        char *request_method = getenv("REQUEST_METHOD");
        if (NULL == request_method || (strncmp(request_method, "GET", 3) != 0)) {
            CGI_ERROR_LOG("Request method error.[%s]",request_method);
            g_errorid = _RESULT_UNALLOWED_METHOD;
            print_result();
            FCGI_Finish();
            continue;
        }

        /* check url */
        char *request_url = getenv("QUERY_STRING");
        if (NULL == request_url) {
            CGI_ERROR_LOG("request url NULL");
            g_errorid = _RESULT_INVALID_URL;
            print_result();
            FCGI_Finish();
            continue;
        }

        /* return 304 not modified 
        */
        char *if_modified_since = getenv("HTTP_IF_MODIFIED_SINCE");
        if (NULL != if_modified_since) {
            CGI_DEBUG_LOG("return 304 status");
            g_errorid = _RESULT_304_STATS;
            print_result();
            FCGI_Finish();
            continue;
        }
        /* delete file from fdfs */
        memset(para_list, 0, sizeof(para_list));
        memcpy(para_list, request_url, strlen(request_url));

        handle(para_list, img_prefix, review_prefix);
        
        print_result();
        FCGI_Finish();
    }

    return SUCC;
}


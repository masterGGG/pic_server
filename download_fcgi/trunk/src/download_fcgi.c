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
#include <fdfs_client.h>
#include <logger.h>
}

#include "proto.h"
#include "log.h"
#include "common.h"

struct thumb_param_t {
    uint32_t w;
    uint32_t h;
};

void print_http_head(uint32_t contentType, uint32_t status, 
                     const char *str, const char *last_modify_time, const char *filename)
{
    /*print http content type*/
    switch (contentType) {
    case TYPE_JPG:
        HEADER_CONTENT_TYPE("image/jpeg");
        break;
    case TYPE_PNG:
        HEADER_CONTENT_TYPE("image/png");
        break;
    case TYPE_GIF:
        HEADER_CONTENT_TYPE("image/gif");
        break;
    default:
        HEADER_CONTENT_TYPE("image/jpeg");
        break;
    }
    
    /* print filename */
    if(filename) {
        HEADER_CONTENT_DISPOSITION(filename);
    }
    /*print http status*/
    HEADER_STATUS(status, str);

    /*print http last modify time*/
    if (last_modify_time) {
        HEADER_LAST_MODIFIED(last_modify_time);
    }

    /*print http head \r\n*/
    HEADER_END;
}

int get_para_from_url(const char *req_url, uint32_t *download_cmd, 
                      uint32_t *image_type, char fdfs_url[FDFS_URL_LEN], 
                      thumb_param_t *tp)
{
    int url_len = strlen(req_url);

    CGI_DEBUG_LOG("req_url[%s] len[%u]", req_url, url_len);

    char *pos;
    if ((pos = strstr((char *)req_url, ".")) == NULL) {
        return FAIL;
    }
    ++pos; // move to next ch

    int tmp = url_len - (pos - req_url);
    if (tmp < 3) {
        return FAIL;       
    }

    /* get image type */
    int n = (pos - req_url) + 3; // fdfs_url len

    if (memcmp(pos, "jpg", 3) == 0) {
        *image_type = TYPE_JPG;
    } else if (memcmp(pos, "png", 3) == 0) {
        *image_type = TYPE_PNG;
    } else if (memcmp(pos, "gif", 3) == 0) {
        *image_type = TYPE_GIF;
    } else {
        return FAIL;
    }

    if (n >= FDFS_URL_LEN)
        return FAIL;

    memcpy(fdfs_url, req_url, n);
    fdfs_url[n] = '\0';    

    if (tmp == 3) { // image
        *download_cmd = CMD_DOWNLOAD_IMAGE;
    } else if (tmp > 3) { // may be thumb
        pos += 3;
        if (2 != sscanf(pos, "_%ux%u.jpg", &(tp->w), &(tp->h))) {
            return FAIL;
        }
        *download_cmd = CMD_DOWNLOAD_THUMB;
    }

    return SUCC;
}

int fdfs_init()
{
    /* fdfs log init*/
    log_init_fdfsx();
    g_log_context.log_level = LOG_ERR;

    if (fdfs_client_init("../../../client.conf") != 0) {
        CGI_ERROR_LOG("fdfs_client_init client.conf");
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

int fdfs_download_file(const char fdfs_url[FDFS_URL_LEN], char **file_buff, 
                       int64_t *file_size)
{
    int result;

    ConnectionInfo *p_tracker_server = tracker_get_connection();
    //TrackerServerInfo *p_tracker_server = tracker_get_connection();
    if (p_tracker_server == NULL) {
        CGI_ERROR_LOG("tracker_get_connection fail");
        return FAIL;
    }

    result = storage_download_file_to_buff1(p_tracker_server, NULL, fdfs_url, 
                                            file_buff, file_size);
    if (result != 0) {
        CGI_ERROR_LOG("storage_download_file_to_buff1 fail: errorno[%d] %s", \
                      result, STRERROR(result));
        return FAIL;
    }

    return SUCC;
}

int main(void)
{
    int fcgi_request_cnt = 0;

    /* my log init*/
    cgi_log_init("../log/", (log_lvl_t)8, LOG_SIZE, 0, "");
    
    if (FAIL == fdfs_init()) {
        CGI_ERROR_LOG("fdfs_init fail");
        return FAIL;
    }

    CGI_DEBUG_LOG("fastcgi begin");
    while (FCGI_Accept() >= 0) {	
        fcgi_request_cnt++;
        CGI_DEBUG_LOG("----Request number %d running on host %s Process ID: %d", \
                      fcgi_request_cnt, getenv("SERVER_NAME"), getpid());

        /* check request method */
        char *request_method = getenv("REQUEST_METHOD");
        if (NULL == request_method || (strncmp(request_method, "GET", 3) != 0)) {
            CGI_ERROR_LOG("Request method error.[%s]",request_method);
            print_http_head(TYPE_JPG, HTTP_NOT_ALLOWED, HTTP_NOT_ALLOWED_STR, NULL, NULL);
            FCGI_Finish();
            continue;
        }

        /* check url */
        char *request_url = getenv("QUERY_STRING");
        if (NULL == request_url) {
            CGI_ERROR_LOG("request url NULL");
            print_http_head(TYPE_JPG, HTTP_BAD_REQUEST, HTTP_BAD_REQUEST_STR, NULL, NULL);
            FCGI_Finish();
            continue;
        }

        /* return 304 not modified */
        char *if_modified_since = getenv("HTTP_IF_MODIFIED_SINCE");
        if (NULL != if_modified_since) {
            CGI_DEBUG_LOG("return 304 status");
            print_http_head(TYPE_JPG, HTTP_NOT_MODIFIED, HTTP_NOT_MODIFIED_STR, 
                            if_modified_since, NULL);
            FCGI_Finish();
            continue;
        }

        /* parse url */
        uint32_t download_cmd;
        uint32_t image_type;
        thumb_param_t tp;
        char fdfs_url[FDFS_URL_LEN];
        if (FAIL == get_para_from_url(request_url, &download_cmd, &image_type, 
                                      fdfs_url, &tp)) {
            CGI_ERROR_LOG("get_para_from_url fail");
            print_http_head(TYPE_JPG, HTTP_BAD_REQUEST, HTTP_BAD_REQUEST_STR, NULL, NULL);
            FCGI_Finish();
            continue;
        }
        
        /* check thumb param */
        if (download_cmd == CMD_DOWNLOAD_THUMB) {
            if (tp.w != tp.h || (tp.w != 30
                                && tp.w != 32
                                && tp.w != 48
                                && tp.w != 50
                                && tp.w != 60
                                && tp.w != 65
                                && tp.w != 70
                                && tp.w != 74
                                && tp.w != 100 
                                && tp.w != 120 
                                && tp.w != 140
                                && tp.w != 146 
                                && tp.w != 150
                                && tp.w != 160
                                && tp.w != 186
                                && tp.w != 200
                                && tp.w != 230
                                && tp.w != 240
                                && tp.w != 300
                                && tp.w != 260
                                && tp.w != 400
                                && tp.w != 530
                                && tp.w != 700 )) { 
                CGI_ERROR_LOG("thumb param error %u %u ",tp.w, tp.h);
                print_http_head(TYPE_JPG, HTTP_BAD_REQUEST, HTTP_BAD_REQUEST_STR, NULL, NULL);
                FCGI_Finish();
                continue;
            }
        }

        /* download file from fdfs */
        char *image_buff;
        int64_t image_size;
        if (FAIL == fdfs_download_file(fdfs_url, &image_buff, &image_size)) {
            CGI_ERROR_LOG("fdfs_download_file fail");
            print_http_head(TYPE_JPG, HTTP_NOT_FOUND, HTTP_NOT_FOUND_STR,NULL, NULL);
            FCGI_Finish();
            continue;
        }

        /* scal image */
        if (download_cmd == CMD_DOWNLOAD_THUMB) {
            gdImagePtr p_image = NULL;
            switch (image_type) {
            case TYPE_JPG:
                p_image = gdImageCreateFromJpegPtr(image_size, (void*)image_buff);
                break;
            case TYPE_PNG:
                p_image = gdImageCreateFromPngPtr(image_size, (void*)image_buff);
                break;
            case TYPE_GIF:
                p_image = gdImageCreateFromGifPtr(image_size, (void*)image_buff);
                break;
            }

            if (!p_image) {
                CGI_ERROR_LOG("gdImageCreateFromXXX fail");
                print_http_head(TYPE_JPG, HTTP_NOT_FOUND, HTTP_NOT_FOUND_STR, NULL, NULL);
                free(image_buff);
                FCGI_Finish();
                continue;
            }
			
            if (gdImageSX(p_image) == 0 || gdImageSY(p_image) == 0) {
                CGI_ERROR_LOG("image fail");
                print_http_head(TYPE_JPG, HTTP_NOT_FOUND, HTTP_NOT_FOUND_STR, NULL, NULL);
                free(image_buff);
                FCGI_Finish();
                continue;
            }

            int w;
            int h;
            if (gdImageSX(p_image) > gdImageSY(p_image)) {
                w = tp.w;
                h = (int)((float)gdImageSY(p_image) * ((float)w / (float)gdImageSX(p_image)));
            } else if (gdImageSX(p_image) < gdImageSY(p_image)) {
                h = tp.h;
                w = (int)((float)gdImageSX(p_image) * ((float)h / (float)gdImageSY(p_image)));
            } else {
                w = tp.w;
                h = tp.h;
            }

            if (gdImageSX(p_image) <= w && gdImageSY(p_image) <= h) {
                w = gdImageSX(p_image);
                h = gdImageSY(p_image);
            }

            gdImagePtr p_out_image = gdImageCreateTrueColor(w, h);
            if (!p_out_image) {
                CGI_ERROR_LOG("gdImageCreateTrueColor fail");
                print_http_head(TYPE_JPG, HTTP_NOT_FOUND, HTTP_NOT_FOUND_STR, NULL, NULL);
                free(image_buff);
                gdImageDestroy(p_image);
                FCGI_Finish();
                continue;
            }
			
            int bg_color = gdImageColorAllocate(p_out_image, 255, 255, 255);
            //int bg_color = gdImageColorAllocateAlpha(p_out_image, 255, 255, 255, 0); //transparence
            gdImageFilledRectangle(p_out_image, 0, 0, w, h, bg_color);

			gdImageCopyResampled(p_out_image, p_image, 0, 0, 0, 0,
                                 w, h, gdImageSX(p_image), gdImageSY(p_image));

            // write pic
            char *pic_buf = NULL;
            int pic_size;
            //<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
            pic_buf = (char*)gdImageJpegPtr(p_out_image, &pic_size, 80);

            /* reponse image */
            time_t now_time;
            struct tm *time_ptr;
            now_time = time(NULL);
            time_ptr = gmtime(&now_time);
            char last_modify_time[TIME_LEN];
            strftime(last_modify_time, TIME_LEN, "%a, %d %b %Y %T GMT", time_ptr);

            print_http_head(TYPE_JPG, HTTP_OK, HTTP_OK_STR, last_modify_time,fdfs_url);
            FCGI_fwrite(pic_buf, 1, pic_size, FCGI_stdout);

            free(image_buff);
            gdImageDestroy(p_image);
            gdImageDestroy(p_out_image);
            gdFree(pic_buf); 
            FCGI_Finish();
        } else {
            /* reponse image */
            time_t now_time;
            struct tm *time_ptr;
            now_time = time(NULL);
            time_ptr = gmtime(&now_time);
            char last_modify_time[TIME_LEN];
            strftime(last_modify_time, TIME_LEN, "%a, %d %b %Y %T GMT", time_ptr);

            print_http_head(image_type, HTTP_OK, HTTP_OK_STR, last_modify_time, fdfs_url);
            FCGI_fwrite(image_buff, 1, image_size, FCGI_stdout);
            free(image_buff);
         
            FCGI_Finish();        
        }
    }

    fdfs_fini();
    return SUCC;
}


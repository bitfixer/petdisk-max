#ifndef __HTTP_OTA_H
#define __HTTP_OTA_H

#include <stdint.h>

typedef enum {
    DownloadingAndSaving_e = 0x01,
    Flashing_e
}DlState;

typedef void (*startDownloadCb)(void);
typedef void (*endDownloadCb)(void);
typedef void (*startFlashingCb)(void);
typedef void (*endFlashingCb)(void);
typedef void (*saveDataCb)(uint8_t *buffer, int bytes);
typedef void (*progressCb)(DlState state, int percent);
typedef int  (*readDataCb)(uint8_t *buffer, int len);
typedef void (*errorCb)(char *message);

typedef struct {
	char *url;
    char *md5;
    startDownloadCb     startDownloadCallback;
    endDownloadCb       endDownloadCallback;
    startFlashingCb     startFlashingCallback;
    endFlashingCb       endFlashingCallback;
	saveDataCb          saveDataCallback;
    readDataCb          readDataCallback;
    progressCb          progressCallback;
    errorCb             errorCallback;
}DlInfo;


class HttpOTA
{
  public:

    HttpOTA();
    ~HttpOTA();
    
    int start(DlInfo &info);

};

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_HTTPOTA)
extern HttpOTA httpOTA;
#endif

#endif /* __HTTP_OTA_H */
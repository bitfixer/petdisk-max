#include <WiFi.h>
#include <functional>
//#include <HTTPClient.h>
#include "SDOta.h"
#include "MD5Builder.h"
#include "Update.h"


HttpOTA::HttpOTA()
{ 
}

HttpOTA::~HttpOTA(){

}

int HttpOTA::start(DlInfo &info) {
    int downloaded = 0;
    int written = 0;
    int total = 1;
    uint8_t buff[1024] = { 0 };
    size_t size = sizeof(buff);
    int ret = 0;

    /*
    HTTPClient http;

    if(info.saveDataCallback == NULL || info.readDataCallback == NULL || 
        info.progressCallback == NULL || info.errorCallback == NULL ||
        info.startDownloadCallback == NULL || info.endDownloadCallback == NULL ||
        info.startFlashingCallback == NULL || info.endFlashingCallback == NULL ){
        return -1;
    }
    http.begin(info.url);

    int httpCode = http.GET();

    if(httpCode > 0 && httpCode == HTTP_CODE_OK) {

        // get lenght of document (is -1 when Server sends no Content-Length header)
        int len = http.getSize();
        total = len;
        downloaded = 0;
        // get tcp stream
        WiFiClient * stream = http.getStreamPtr();
        info.startDownloadCallback();
        // read all data from server
        while(http.connected() && (len > 0)) {
          // get available data size
          size = stream->available();

            if(size > 0) {
                // read up to 128 byte
                int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
                // write to storage
                info.saveDataCallback(buff, c);
                downloaded += c;
                info.progressCallback(DownloadingAndSaving_e, (100*downloaded)/total);

                if(len > 0) {
                    len -= c;
                }
            }
          delay(1);
        }
        info.endDownloadCallback();
    }else {
        info.errorCallback("[HTTP] GET... failed!");
        ret -1;
    }

    http.end();
    */
    info.errorCallback("here i am");
    total = 754880;
    downloaded = total;
    if(downloaded == total){
    //if (true) {
        info.errorCallback("Downloading ... done -> Flashing ..."); 
        if (Update.begin(total, U_FLASH)){
            Update.setMD5(info.md5);
            downloaded = 0;
            int r;
            info.startFlashingCallback();
            while (!Update.isFinished()) {
                //read sdcard
                r = info.readDataCallback(buff, size);
                char tmpstr[32];
                sprintf(tmpstr, "got %d\n", r);
                info.errorCallback((char*)tmpstr);
                written = Update.write(buff, r);
                if (written > 0) {
                    if(written != r){
                        info.errorCallback("Flashing chunk not full ... warning!");
                    }
                    downloaded += written;
                    info.progressCallback(Flashing_e, (100*downloaded)/total);
                } else {
                    info.errorCallback("Flashing ... failed!");

                    if (Update.isRunning()) {
                        info.errorCallback("not running\n");
                    }

                    if (Update.hasError()) {
                        info.errorCallback("has error\n");
                        uint8_t err = Update.getError();
                        char tmp[32];
                        sprintf(tmp, "err %d\n", err);
                        info.errorCallback(tmp);
                    }

                    ret = -1;
                    break;
                }
            }
            info.endFlashingCallback();
            if(downloaded == total && Update.end()){
                info.errorCallback("Flashing ... done!");
                delay(100);
                ESP.restart();                
            } else {
                info.errorCallback("Flashing or md5 ... failed!"); 
                ret = -1;
            }
        } else {
            info.errorCallback("Flashing init ... failed!");
            ret = -1;
        } 
    } else {
        info.errorCallback("Download firmware ... failed!");
        ret = -1;
    }
    return ret;
}

HttpOTA httpOTA;
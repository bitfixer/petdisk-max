#ifndef _SDFAT_H_
#define _SDFAT_H_

//
// SDFAT.h — IDF FATFS-backed DataSource
//
// Replaces the custom FAT32 + SD_routines + SPI_routines stack.
// Uses esp-idf's sdspi + vfs_fat layer for correctness and broad
// card compatibility (including SDXC, UHS-I, and modern high-capacity
// cards that the old hand-rolled SPI init failed to detect).
//
// Card-change detection uses the SD socket's CD pin via GPIO interrupt.
// The interrupt sets a volatile flag; the flag is checked and acted on
// at the start of every public method that touches the filesystem.
// This matches the contract the PET command loop expects: init() can
// be called at any time and will re-mount if needed.
//
// Pin assignments come from hardware_esp32.h.  Add CD_PIN there:
//
//   #define CD_PIN  <gpio_number>   // active-low, internal pull-up
//
// If CD_PIN is not defined this file falls back to software-only
// detection (init() forces a remount on every call), which is safe
// but slightly slower.
//
// Mount point: /disk
//
// SPI host: SPI2_HOST (same host the old stack used). The IDF sdspi
// driver takes ownership of the bus; do NOT call spi_init() from
// hardware_esp32.cpp after this class is in use.
//

#include "DataSource.h"
#include "hardware.h"

#include <stdint.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/spi_common.h>
#include <driver/sdspi_host.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include <driver/gpio.h>
#include <esp_log.h>

// ------------------------------------------------------------------
// Tunables
// ------------------------------------------------------------------

#define SDFAT_MOUNT_POINT   "/disk"
#define SDFAT_MAX_FILES     4       // max simultaneously open FDs in fatfs
#define SDFAT_SPI_HOST      SPI2_HOST

// Read/write buffer size.  Must match SD sector size.
#define SDFAT_BUF_SIZE      512

// ------------------------------------------------------------------

namespace bitfixer {

class SDFAT : public DataSource
{
public:
    SDFAT();
    ~SDFAT();

    // ---- DataSource interface ------------------------------------

    // Initialise (or re-initialise) the SPI bus and mount the card.
    // Safe to call repeatedly; unmounts cleanly before remounting.
    // Returns true on success.
    bool init() override;

    bool isInitialized() override;

    // Directory iteration
    void openCurrentDirectory() override;
    bool openDirectory(const char* dirName) override;
    bool getNextDirectoryEntry() override;

    // File attribute queries (valid after getNextDirectoryEntry)
    bool isHidden()    override;
    bool isVolumeId()  override;
    bool isDirectory() override;
    bool isLongFilename();
    uint8_t* getFilename() override;

    // Searches the current directory for fileName (case-insensitive, supports
    // '*' wildcard suffix).  Returns true if the file exists; no other state
    // is modified.
    bool findFile(char* fileName);

    // Deletes fileName from the current directory.  Returns true on success.
    bool deleteFile(char* fileName);

    // Reading
    bool     openFileForReading(uint8_t* fileName) override;
    uint16_t getNextFileBlock() override;
    bool     isLastBlock() override;
    uint32_t getFileSize();
    uint32_t seek(uint32_t pos) override;

    // Writing
    void openFileForWriting(uint8_t* fileName) override;
    void writeBufferToFile(uint16_t bytesToWrite) override;
    void updateBlock() override;
    void closeFile() override;

    // Buffer access
    uint8_t* getBuffer() override;
    uint16_t writeBufferSize() override { return SDFAT_BUF_SIZE; }
    uint16_t readBufferSize()  override { return SDFAT_BUF_SIZE; }

    // RTC support (called by upper layer before write operations)
    bool needRealTime() override { return true; }
    void setDateTime(int year, int month, int day,
                     int hour, int minute, int second) override;

    // ---- Card-detect interrupt -----------------------------------

    // Call once after gpio_install_isr_service() has been called
    // (setup_atn_interrupt() already does this, so call after that).
    void setupCardDetectInterrupt(gpio_num_t cdPin);

    // Called by the ISR — do not call from application code.
    void onCardDetectISR();

private:

    // ---- Mount / unmount ----------------------------------------
    bool     mount();
    void     unmount();
    bool     remountIfNeeded();

    // ---- Internal helpers ---------------------------------------
    void     buildPath(const char* name, char* out, size_t outLen);
    bool     fileExists(const char* path);

    // ---- State --------------------------------------------------
    bool              _initialized;
    volatile bool     _cardRemoved;   // set by ISR, cleared by remountIfNeeded
    bool              _cdInterruptInstalled;
    gpio_num_t        _cdPin;

    sdmmc_card_t*     _card;          // owned; NULL when unmounted
    bool              _spiHostInited;

    // Current directory (absolute path, e.g. "/disk" or "/disk/subdir")
    char              _currentDir[256];

    // Directory iteration
    DIR*              _dir;
    struct dirent*    _dirent;        // last entry returned
    struct stat       _direntStat;    // stat of last entry
    bool              _direntValid;

    // File I/O
    FILE*             _readFile;
    FILE*             _writeFile;
    uint32_t          _fileSize;
    uint32_t          _fileBytesRead;

    // Shared 512-byte sector buffer (read and write share this)
    uint8_t           _buf[SDFAT_BUF_SIZE];

    // Filename buffer for getFilename()
    char              _filenameBuf[256];

    // FAT timestamp components set by setDateTime()
    int _dtYear, _dtMonth, _dtDay;
    int _dtHour, _dtMinute, _dtSecond;
};

} // namespace bitfixer

#endif // _SDFAT_H_
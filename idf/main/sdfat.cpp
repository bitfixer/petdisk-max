#include "sdfat.h"

#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#include <esp_vfs_fat.h>
#include <driver/sdspi_host.h>
#include <driver/spi_common.h>
#include <sdmmc_cmd.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "sdfat";

// ---------------------------------------------------------------------------
// FAT timestamp callback — IDF's fatfs layer calls this to timestamp writes.
// We store what setDateTime() gave us in a file-scope variable because the
// callback has no user-data pointer.
// ---------------------------------------------------------------------------
static int  s_fat_year = 2024, s_fat_month = 1, s_fat_day = 1;
static int  s_fat_hour = 0,    s_fat_minute = 0, s_fat_second = 0;

// This function is called by the fatfs layer (ff_driver) when it needs the
// current time.  Signature is fixed by esp-idf; return value is a FAT DWORD.
// Only compiled when CONFIG_FATFS_CUSTOM_TIMEFUNCTION=y in sdkconfig.
// If you are not using a custom time function, remove this and let fatfs use
// esp_vfs_fat's default (which reads system time via time()/localtime()).
#if defined(CONFIG_FATFS_CUSTOM_TIMEFUNCTION)
extern "C" uint32_t get_fattime(void)
{
    uint32_t t = 0;
    t |= ((uint32_t)(s_fat_year  - 1980) & 0x7F) << 25;
    t |= ((uint32_t) s_fat_month          & 0x0F) << 21;
    t |= ((uint32_t) s_fat_day            & 0x1F) << 16;
    t |= ((uint32_t) s_fat_hour           & 0x1F) << 11;
    t |= ((uint32_t) s_fat_minute         & 0x3F) << 5;
    t |= ((uint32_t)(s_fat_second / 2)    & 0x1F);
    return t;
}
#endif

namespace bitfixer {

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

SDFAT::SDFAT()
    : _initialized(false)
    , _cardRemoved(false)
    , _cdInterruptInstalled(false)
    , _cdPin(GPIO_NUM_NC)
    , _card(nullptr)
    , _spiHostInited(false)
    , _dir(nullptr)
    , _dirent(nullptr)
    , _direntValid(false)
    , _readFile(nullptr)
    , _writeFile(nullptr)
    , _fileSize(0)
    , _fileBytesRead(0)
    , _dtYear(2024), _dtMonth(1), _dtDay(1)
    , _dtHour(0), _dtMinute(0), _dtSecond(0)
{
    strncpy(_currentDir, SDFAT_MOUNT_POINT, sizeof(_currentDir) - 1);
    memset(_buf, 0, sizeof(_buf));
    memset(_filenameBuf, 0, sizeof(_filenameBuf));
}

SDFAT::~SDFAT()
{
    unmount();
}

// ---------------------------------------------------------------------------
// Card-detect interrupt
// ---------------------------------------------------------------------------

static void IRAM_ATTR cd_isr_handler(void* arg)
{
    SDFAT* self = static_cast<SDFAT*>(arg);
    self->onCardDetectISR();
}

void IRAM_ATTR SDFAT::onCardDetectISR()
{
    // CD pin went high (card removed — active-low socket).
    // Just set the flag; the main task will handle unmount/remount
    // at a safe point between commands.
    _cardRemoved = true;
}

void SDFAT::setupCardDetectInterrupt(gpio_num_t cdPin)
{
    if (_cdInterruptInstalled)
    {
        return;
    }

    _cdPin = cdPin;

    // Configure pin: input with internal pull-up (CD is active-low)
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask  = 1ULL << cdPin;
    io_conf.mode          = GPIO_MODE_INPUT;
    io_conf.pull_up_en    = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en  = GPIO_PULLDOWN_DISABLE;
    // Trigger on both edges so we detect insertion as well as removal.
    // The ISR only sets _cardRemoved; init() distinguishes the two states
    // by reading the pin level.
    io_conf.intr_type     = GPIO_INTR_ANYEDGE;
    gpio_config(&io_conf);

    // gpio_install_isr_service() is already called by setup_atn_interrupt()
    // in hardware_esp32.cpp; adding our handler is all that's needed.
    esp_err_t err = gpio_isr_handler_add(cdPin, cd_isr_handler, this);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "failed to add CD ISR handler: %s", esp_err_to_name(err));
        return;
    }

    _cdInterruptInstalled = true;
    ESP_LOGI(TAG, "CD interrupt installed on GPIO %d", cdPin);
}

// ---------------------------------------------------------------------------
// Mount / unmount
// ---------------------------------------------------------------------------

bool SDFAT::mount()
{
    ESP_LOGI(TAG, "mounting SD card on %s", SDFAT_MOUNT_POINT);

    // ---- SPI host ----------------------------------------------------------
    // Only initialise the bus once across mount/unmount cycles.
    // The host number must match what hardware_esp32.cpp was using (SPI2_HOST).
    // If spi_init() was already called for the old stack this will return
    // ESP_ERR_INVALID_STATE; that's fine — the bus is already up.
    if (!_spiHostInited)
    {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num   = MOSI_PIN;
        buscfg.miso_io_num   = MISO_PIN;
        buscfg.sclk_io_num   = SCK_PIN;
        buscfg.quadwp_io_num = -1;
        buscfg.quadhd_io_num = -1;
        buscfg.max_transfer_sz = 4000;

        esp_err_t ret = spi_bus_initialize(SDFAT_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
        if (ret == ESP_OK)
        {
            _spiHostInited = true;
            ESP_LOGI(TAG, "SPI bus initialized");
        }
        else if (ret == ESP_ERR_INVALID_STATE)
        {
            // Already initialized externally — that's acceptable.
            _spiHostInited = true;
            ESP_LOGW(TAG, "SPI bus was already initialized, continuing");
        }
        else
        {
            ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(ret));
            return false;
        }
    }

    // ---- sdspi device ------------------------------------------------------
    // Use SDSPI_HOST_DEFAULT() without overriding slot or max_freq_khz.
    // Manually setting host.slot or host.max_freq_khz in some IDF 5.x builds
    // interferes with the SDIO probe sequence and causes CMD52 CRC errors
    // even on plain SD cards.  The default (SPI2_HOST, 20MHz) is correct;
    // the driver handles the 400kHz init ramp internally.
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.host_id  = (spi_host_device_t)SDFAT_SPI_HOST;
    slot.gpio_cs  = (gpio_num_t)CS_PIN;
    slot.gpio_cd  = GPIO_NUM_NC;   // we handle CD ourselves via interrupt
    slot.gpio_wp  = GPIO_NUM_NC;
    slot.gpio_int = GPIO_NUM_NC;

    // ---- VFS / FATFS -------------------------------------------------------
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
    mount_config.format_if_mount_failed = false;
    mount_config.max_files              = SDFAT_MAX_FILES;
    mount_config.allocation_unit_size   = 16 * 1024;

    esp_err_t ret = esp_vfs_fat_sdspi_mount(
        SDFAT_MOUNT_POINT,
        &host,
        &slot,
        &mount_config,
        &_card
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_vfs_fat_sdspi_mount failed: %s", esp_err_to_name(ret));
        _card = nullptr;
        return false;
    }

    sdmmc_card_print_info(stdout, _card);
    ESP_LOGI(TAG, "SD card mounted at %s", SDFAT_MOUNT_POINT);
    return true;
}

void SDFAT::unmount()
{
    // Close any open files / directories first to avoid VFS state issues.
    if (_readFile)
    {
        fclose(_readFile);
        _readFile = nullptr;
    }
    if (_writeFile)
    {
        fclose(_writeFile);
        _writeFile = nullptr;
    }
    if (_dir)
    {
        closedir(_dir);
        _dir = nullptr;
    }
    _dirent      = nullptr;
    _direntValid = false;

    if (_card)
    {
        esp_err_t ret = esp_vfs_fat_sdcard_unmount(SDFAT_MOUNT_POINT, _card);
        if (ret != ESP_OK)
        {
            ESP_LOGW(TAG, "unmount returned: %s (continuing anyway)", esp_err_to_name(ret));
        }
        _card = nullptr;
        ESP_LOGI(TAG, "SD card unmounted");
    }
}

// Called at the top of every public method that touches the filesystem.
// Returns true if the card is ready (mounted and usable).
bool SDFAT::remountIfNeeded()
{
    if (_cardRemoved)
    {
        ESP_LOGI(TAG, "card change detected, remounting");
        _cardRemoved = false;
        unmount();
        _initialized = false;

        // If CD pin is wired: only remount when card is actually present
        // (pin low = card seated, active-low convention).
        if (_cdInterruptInstalled)
        {
            int level = gpio_get_level(_cdPin);
            if (level != 0)
            {
                // Card is not seated yet; stay unmounted.
                ESP_LOGI(TAG, "card not present after change event, staying unmounted");
                return false;
            }
        }

        // Brief settle delay — mechanical contacts need a moment.
        vTaskDelay(pdMS_TO_TICKS(50));

        _initialized = mount();
        if (_initialized)
        {
            // Reset to root after remount.
            strncpy(_currentDir, SDFAT_MOUNT_POINT, sizeof(_currentDir) - 1);
        }
        return _initialized;
    }

    return _initialized;
}

// ---------------------------------------------------------------------------
// Public: init
// ---------------------------------------------------------------------------

bool SDFAT::init()
{
    // Always attempt a clean remount when the upper layer calls init().
    // This matches the old FAT32::init() contract: called on every PET
    // command, idempotent when already working, recovers from card changes.
    if (_initialized)
    {
        // Already up and no change event pending — nothing to do.
        if (!_cardRemoved)
        {
            return true;
        }
    }

    unmount();
    _initialized = false;
    _cardRemoved = false;

    // If we have a CD pin and the card isn't seated, bail early.
    if (_cdInterruptInstalled)
    {
        int level = gpio_get_level(_cdPin);
        if (level != 0)
        {
            ESP_LOGW(TAG, "init(): no card detected (CD pin high)");
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(50));  // contact settle
    }

    _initialized = mount();
    if (_initialized)
    {
        strncpy(_currentDir, SDFAT_MOUNT_POINT, sizeof(_currentDir) - 1);
    }
    return _initialized;
}

bool SDFAT::isInitialized()
{
    return _initialized;
}

// ---------------------------------------------------------------------------
// DateTime
// ---------------------------------------------------------------------------

void SDFAT::setDateTime(int year, int month, int day,
                        int hour, int minute, int second)
{
    _dtYear = year; _dtMonth = month; _dtDay = day;
    _dtHour = hour; _dtMinute = minute; _dtSecond = second;

    // Mirror into the file-scope variables used by get_fattime().
    s_fat_year = year; s_fat_month = month; s_fat_day = day;
    s_fat_hour = hour; s_fat_minute = minute; s_fat_second = second;
}

// ---------------------------------------------------------------------------
// Internal: path helpers
// ---------------------------------------------------------------------------

void SDFAT::buildPath(const char* name, char* out, size_t outLen)
{
    if (name == nullptr || name[0] == '\0')
    {
        strncpy(out, _currentDir, outLen - 1);
        out[outLen - 1] = '\0';
        return;
    }
    snprintf(out, outLen, "%s/%s", _currentDir, name);
}

// ---------------------------------------------------------------------------
// Directory operations
// ---------------------------------------------------------------------------

void SDFAT::openCurrentDirectory()
{
    if (!remountIfNeeded()) return;

    if (_dir)
    {
        closedir(_dir);
        _dir = nullptr;
    }
    _dirent      = nullptr;
    _direntValid = false;

    _dir = opendir(_currentDir);
    if (!_dir)
    {
        ESP_LOGE(TAG, "opendir(%s) failed: %d", _currentDir, errno);
    }
}

bool SDFAT::openDirectory(const char* dirName)
{
    if (!remountIfNeeded()) return false;

    if (_dir)
    {
        closedir(_dir);
        _dir = nullptr;
    }
    _dirent      = nullptr;
    _direntValid = false;

    char path[256];
    if (dirName == nullptr || strcmp(dirName, "/") == 0 || dirName[0] == '\0')
    {
        // Root of the mounted volume
        strncpy(_currentDir, SDFAT_MOUNT_POINT, sizeof(_currentDir) - 1);
    }
    else
    {
        // Relative to current directory
        buildPath(dirName, path, sizeof(path));

        struct stat st;
        if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode))
        {
            ESP_LOGE(TAG, "openDirectory: not a directory: %s", path);
            return false;
        }
        strncpy(_currentDir, path, sizeof(_currentDir) - 1);
    }
    _currentDir[sizeof(_currentDir) - 1] = '\0';

    _dir = opendir(_currentDir);
    if (!_dir)
    {
        ESP_LOGE(TAG, "opendir(%s) failed: %d", _currentDir, errno);
        return false;
    }
    return true;
}

bool SDFAT::getNextDirectoryEntry()
{
    if (!remountIfNeeded()) return false;
    if (!_dir) return false;

    // Skip "." and ".." — the PET doesn't know about those.
    while (true)
    {
        _dirent = readdir(_dir);
        if (!_dirent)
        {
            _direntValid = false;
            return false;
        }

        if (strcmp(_dirent->d_name, ".")  == 0 ||
            strcmp(_dirent->d_name, "..") == 0)
        {
            continue;
        }

        // Stat the entry so isDirectory() / isHidden() have real data.
        char full[512];
        snprintf(full, sizeof(full), "%s/%s", _currentDir, _dirent->d_name);
        if (stat(full, &_direntStat) != 0)
        {
            // Stat failed; skip rather than crash.
            ESP_LOGW(TAG, "stat(%s) failed: %d", full, errno);
            continue;
        }

        _direntValid = true;
        return true;
    }
}

// ---------------------------------------------------------------------------
// File attribute queries
// ---------------------------------------------------------------------------

bool SDFAT::isHidden()
{
    if (!_direntValid) return false;
    // FATFS surfaces hidden attribute through d_type on some ports, but
    // the most portable approach on IDF is to check for a leading dot.
    // The old FAT32 class checked the FAT attribute byte; we replicate
    // the meaningful subset here.
    return (_dirent && _dirent->d_name[0] == '.');
}

bool SDFAT::isVolumeId()
{
    // Volume label entries are not surfaced through POSIX readdir.
    return false;
}

bool SDFAT::isDirectory()
{
    if (!_direntValid) return false;
    return S_ISDIR(_direntStat.st_mode);
}

bool SDFAT::isLongFilename()
{
    // From the caller's perspective every name we return is "long" if it
    // doesn't fit in 8.3.  The upper layer uses this to decide display
    // formatting.  We return true for anything that isn't strict 8.3.
    if (!_direntValid || !_dirent) return false;

    const char* n = _dirent->d_name;
    size_t len = strlen(n);
    if (len > 12) return true;

    // Check for dot placement consistent with 8.3
    const char* dot = strchr(n, '.');
    if (dot == nullptr)
    {
        return (len > 8);
    }
    size_t baseLen = (size_t)(dot - n);
    size_t extLen  = len - baseLen - 1;
    return (baseLen > 8 || extLen > 3 || strchr(dot + 1, '.') != nullptr);
}

uint8_t* SDFAT::getFilename()
{
    if (!_direntValid || !_dirent)
    {
        _filenameBuf[0] = '\0';
        return (uint8_t*)_filenameBuf;
    }
    strncpy(_filenameBuf, _dirent->d_name, sizeof(_filenameBuf) - 1);
    _filenameBuf[sizeof(_filenameBuf) - 1] = '\0';
    return (uint8_t*)_filenameBuf;
}

// ---------------------------------------------------------------------------
// findFile / deleteFile
// ---------------------------------------------------------------------------

// Case-insensitive filename match that also handles a trailing '*' wildcard,
// matching the behaviour of the old FAT32::numCharsToCompare / strncmp path.
static bool filenameMatches(const char* candidate, const char* pattern)
{
    // Find wildcard position (if any)
    const char* star = strchr(pattern, '*');
    size_t cmpLen = (star != nullptr)
                  ? (size_t)(star - pattern)
                  : strlen(pattern);

    return strncasecmp(candidate, pattern, cmpLen) == 0 &&
           (star != nullptr || strlen(candidate) == cmpLen);
}

bool SDFAT::findFile(char* fileName)
{
    if (!remountIfNeeded()) return false;

    // Pure existence check in the current directory.
    // Supports the same case-insensitive / wildcard matching as the old
    // FAT32::findFile, but does not update any directory-entry state —
    // callers use this only to test whether a file is present.
    DIR* d = opendir(_currentDir);
    if (!d)
    {
        ESP_LOGE(TAG, "findFile: opendir(%s) failed: %d", _currentDir, errno);
        return false;
    }

    bool found = false;
    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr)
    {
        if (filenameMatches(entry->d_name, fileName))
        {
            found = true;
            break;
        }
    }

    closedir(d);
    ESP_LOGD(TAG, "findFile: '%s' %s", fileName, found ? "found" : "not found");
    return found;
}

bool SDFAT::deleteFile(char* fileName)
{
    if (!remountIfNeeded()) return false;

    char path[256];
    buildPath((const char*)fileName, path, sizeof(path));

    if (remove(path) != 0)
    {
        ESP_LOGE(TAG, "deleteFile: remove(%s) failed: %d", path, errno);
        return false;
    }

    ESP_LOGI(TAG, "deleted %s", path);
    return true;
}

bool SDFAT::openFileForReading(uint8_t* fileName)
{
    if (!remountIfNeeded()) return false;

    if (_readFile)
    {
        fclose(_readFile);
        _readFile = nullptr;
    }

    char path[256];
    buildPath((const char*)fileName, path, sizeof(path));

    _readFile = fopen(path, "rb");
    if (!_readFile)
    {
        ESP_LOGE(TAG, "fopen(%s, rb) failed: %d", path, errno);
        return false;
    }

    // Get file size
    fseek(_readFile, 0, SEEK_END);
    _fileSize = (uint32_t)ftell(_readFile);
    fseek(_readFile, 0, SEEK_SET);
    _fileBytesRead = 0;

    ESP_LOGI(TAG, "opened %s for reading, size=%" PRIu32, path, _fileSize);
    return true;
}

uint32_t SDFAT::getFileSize()
{
    return _fileSize;
}

uint16_t SDFAT::getNextFileBlock()
{
    if (!_readFile) return 0;

    size_t remaining = _fileSize - _fileBytesRead;
    size_t toRead    = (remaining < SDFAT_BUF_SIZE) ? remaining : SDFAT_BUF_SIZE;
    if (toRead == 0) return 0;

    size_t got = fread(_buf, 1, toRead, _readFile);
    _fileBytesRead += (uint32_t)got;

    return (uint16_t)got;
}

bool SDFAT::isLastBlock()
{
    return (_fileBytesRead >= _fileSize);
}

uint32_t SDFAT::seek(uint32_t pos)
{
    if (!_readFile) return 0;

    // Quantise to sector boundary (matches old FAT32::seek behaviour)
    uint32_t qpos = (pos / SDFAT_BUF_SIZE) * SDFAT_BUF_SIZE;
    if (fseek(_readFile, (long)qpos, SEEK_SET) != 0)
    {
        ESP_LOGE(TAG, "fseek failed: %d", errno);
        return _fileBytesRead;
    }
    _fileBytesRead = qpos;
    return qpos;
}

// ---------------------------------------------------------------------------
// Writing
// ---------------------------------------------------------------------------

void SDFAT::openFileForWriting(uint8_t* fileName)
{
    if (!remountIfNeeded()) return;

    if (_writeFile)
    {
        fclose(_writeFile);
        _writeFile = nullptr;
    }

    char path[256];
    buildPath((const char*)fileName, path, sizeof(path));

    // "wb" truncates an existing file, creating if needed — matches the
    // old FAT32 behaviour of delete-then-create.
    _writeFile = fopen(path, "wb");
    if (!_writeFile)
    {
        ESP_LOGE(TAG, "fopen(%s, wb) failed: %d", path, errno);
    }
    else
    {
        ESP_LOGI(TAG, "opened %s for writing", path);
    }
}

void SDFAT::writeBufferToFile(uint16_t bytesToWrite)
{
    if (!_writeFile) return;

    size_t written = fwrite(_buf, 1, bytesToWrite, _writeFile);
    if (written != bytesToWrite)
    {
        ESP_LOGE(TAG, "fwrite: wrote %zu of %u bytes", written, bytesToWrite);
    }
}

void SDFAT::updateBlock()
{
    // In the old implementation this re-wrote the current sector for
    // in-place edits.  With stdio there's no sector concept; fflush
    // ensures the data reaches the FATFS layer.
    if (_writeFile) fflush(_writeFile);
    if (_readFile)  fflush(_readFile);
}

void SDFAT::closeFile()
{
    if (_writeFile)
    {
        fclose(_writeFile);
        _writeFile = nullptr;
        ESP_LOGI(TAG, "write file closed");
    }
    if (_readFile)
    {
        fclose(_readFile);
        _readFile = nullptr;
        ESP_LOGI(TAG, "read file closed");
    }
}

// ---------------------------------------------------------------------------
// Buffer
// ---------------------------------------------------------------------------

uint8_t* SDFAT::getBuffer()
{
    return _buf;
}

} // namespace bitfixer
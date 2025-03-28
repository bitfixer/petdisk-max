#ifndef __d64_datasource_h__
#define __d64_datasource_h__

#include <stdint.h>
#include <string.h>
#include "DataSource.h"
#include "Logger.h"

// need to mount a file as a d64 filesystem
// the file will be served from another datasource
// needs to support searching for a file and seeking within the file

#define BLOCK_SIZE 256
#define BAM_SIZE 4*35
#define DISK_TRACKS 40
#define MAX_TRACKS 35

#define DEL_TYPE 0x80
#define SEQ_TYPE 0x81
#define PRG_TYPE 0x82
#define USR_TYPE 0x83
#define REL_TYPE 0x84

typedef struct CBMHeader
{
        uint8_t            nextBlock[2];
        uint8_t            dosVersion;
        uint8_t            unused1;
        uint8_t            bam[BAM_SIZE];
        uint8_t            diskName[16];
        uint8_t            id[2];
        uint8_t            unused2;                    // Usually $A0
        uint8_t            dosType[2];
        uint8_t            unused3[4];                 // Usually $A0
        uint8_t            track40Extended[55];        // Usually $00, except for 40 track format
} CBMHeader;

typedef struct
{
        uint8_t            nextBlock[2];               // Track and block of next directory
                                                    // block. When the first byte is 00
                                                    // that is the last block.             

        uint8_t            fileType;
                                                    // 0x80 = DELeted                       
                                                    // 0x81 = SEQuential                    
                                                    // 0x82 = PROGram
                                                    // 0x83 = USER
                                                    // 0x84 = RELative

        uint8_t            dataBlock[2];               // Track and block of first data block
        uint8_t            fileName[16];               // Filename padded with spaces
        uint8_t            sideSector[2];              // Relative only track and block first side
                                                    // sector.                             

        uint8_t            recordSize;                 // Relative file only. Record Size.                     
        uint8_t            unused[6];                  // Unused bytes                                        

        uint8_t            fileSize[2];                // Number of blocks in file. Low Byte, High Byte.             
} CBMFile_Entry;

typedef struct
{
        CBMFile_Entry   *file;
        int             files;
} CBMDirectory;

typedef struct
{
        CBMHeader       header;
        CBMDirectory    directory;
} CBMDisk;

// ERROR CODES
#define D64_FILE_ERROR      -1
#define D64_OUT_OF_MEMORY   -2
#define D64_OUT_OF_RANGE    -3
#define D64_FILE_NOT_FOUND  -4
#define D64_ERROR           -5
#define D64_FILE_NOT_OPEN   -6
#define D64_FILE_EXISTS     -7

class D64DataSource : public DataSource
{
public:
    D64DataSource() {}
    ~D64DataSource() {}

    bool init() 
    {
        return true;
    }
    
    bool initWithDataSource(DataSource* dataSource, const char* fileName, Logger* logger);

    void openFileForWriting(uint8_t* fileName);
    bool openFileForReading(uint8_t* fileName);
    uint32_t seek(uint32_t position);

    bool openDirectory(const char* dirName);
    uint16_t getNextFileBlock();
    bool isLastBlock();
    bool getNextDirectoryEntry();
    bool isInitialized();

    void writeBufferToFile(uint16_t numBytes);
    void closeFile();
    void openCurrentDirectory();
    uint8_t* getFilename();
    uint8_t* getBuffer();
    uint16_t writeBufferSize();
    uint16_t readBufferSize();
    void processCommandString(int* address);

    DataSource* getFileDataSource();

private:
    DataSource* _fileDataSource;
    CBMFile_Entry* _currentFileEntry;
    uint8_t _fileName[21];
    uint8_t* _cbmBuffer;
    uint32_t* _cbmTrackLayout;
    uint8_t _dirTrackBlock[2];
    uint8_t _fileTrackBlock[2];
    uint8_t _fileFirstTrackBlock[2];
    uint8_t _dirIndexInBuffer;
    uint8_t _cbmBAM[BAM_SIZE];
    uint16_t _blocksInFile;
    bool _directBlockAccess;
    Logger* _logger;

    void prepareNextBlockForWriting();

    void cbmMount();
    void cbmPrintHeader(CBMDisk* disk);
    CBMHeader* cbmLoadHeader();
    void cbmSaveHeader();
    uint32_t cbmBlockLocation(uint8_t* tb);
    uint8_t* cbmReadBlock(uint8_t* tb);
    void cbmWriteBlock(uint8_t* tb);
    uint8_t* cbmEmptyBlockChain(CBMDisk* disk);
    void cbmPrintFileEntry(CBMFile_Entry* entry);
    CBMFile_Entry* cbmGetNextFileEntry();
    CBMFile_Entry* cbmSearch(uint8_t* searchNameA, uint8_t fileType);
    uint8_t* cbmD64StringCString(uint8_t* dest, const uint8_t* source);
    uint8_t* cbmCopyString(uint8_t* dest, const uint8_t* source);
    bool cbmFindEmptyBlock(uint8_t* tb);
    uint8_t cbmSectorsPerTrack(uint8_t track);
    bool cbmIsBlockFree(uint8_t* tb);
    int cbmBAM(uint8_t *tb, char s);
    void cbmCreateFileEntry(uint8_t* fileName, uint8_t fileType, uint16_t blocksInFile);
    bool findEmptyDirectoryBlock(uint8_t* tb);
    bool createNewDirectoryBlock(uint8_t* tb);
    
};

#endif
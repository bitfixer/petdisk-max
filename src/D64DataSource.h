#ifndef __d64_datasource_h__
#define __d64_datasource_h__

#include <stdint.h>
#include "DataSource.h"

// need to mount a file as a d64 filesystem
// the file will be served from another datasource
// needs to support searching for a file and seeking within the file

#define BLOCK_SIZE 256
#define BAM_SIZE 4*35
#define DISK_TRACKS 40

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

class D64DataSource : public DataSource
{
public:
	D64DataSource() {}
	~D64DataSource() {}

	bool init() 
    {
        return true;
    }
    
	bool initWithDataSource(DataSource* dataSource, const char* fileName);

    void openFileForWriting(unsigned char* fileName);
    bool openFileForReading(unsigned char* fileName);
    void seek(unsigned int position);

    bool openDirectory(const char* dirName);
    unsigned int getNextFileBlock();
    bool isLastBlock();
    bool getNextDirectoryEntry();
    bool isInitialized();

    void writeBufferToFile(unsigned int numBytes);
    void closeFile();
    void openCurrentDirectory();
    unsigned char* getFilename();
    unsigned char* getBuffer();
    unsigned int writeBufferSize();

private:
	DataSource* _fileDataSource;
    CBMDisk _cbmDisk;
    uint8_t _cbmBuffer[BLOCK_SIZE];
    uint32_t* _cbmTrackLayout;
    uint8_t _dirTrackBlock[2];
    uint8_t _fileTrackBlock[2];
    uint8_t _dirIndexInBuffer;

    void cbmMount(CBMDisk* disk, char* name);
    void cbmPrintHeader(CBMDisk* disk);
    int cbmLoadHeader(CBMDisk* disk);
    uint32_t cbmBlockLocation(uint8_t* tb);
    uint8_t* cbmReadBlock(uint8_t* tb);
    CBMFile_Entry* cbmGetNextFileEntry();
    CBMFile_Entry* cbmSearch(CBMDisk* disk, uint8_t* searchNameA, uint8_t fileType);

};

#endif
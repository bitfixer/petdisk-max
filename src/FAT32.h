/*
    FAT32.h
    FAT32 filesystem Routines in the PETdisk storage device
    Copyright (C) 2011 Michael Hill

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
    
    Contact the author at bitfixer@bitfixer.com
    http://bitfixer.com 
*/

#ifndef _FAT32_H_
#define _FAT32_H_

#include "SD_routines.h"
#include "SerialLogger.h"
#include "DataSource.h"
#include "helpers.h"
#include <stdint.h>

//Structure to access Directory Entry in the FAT
struct PACKED dir_Structure{
uint8_t name[11];     //0
uint8_t attrib;       //11 //file attributes
uint8_t NTreserved;   //12 //always 0
uint8_t timeTenth;    //13 //tenths of seconds, set to 0 here
uint16_t createTime;    //14 //time file was created
uint16_t createDate;    //16 //date file was created
uint16_t lastAccessDate;//18
uint16_t firstClusterHI;//20 //higher word of the first cluster number
uint16_t writeTime;     //22 //time of last write
uint16_t writeDate;     //24 //date of last write
uint16_t firstClusterLO;//26 //lower word of the first cluster number
uint32_t fileSize;     //28 //size of file in bytes
    //32
};

struct PACKED dir_Longentry_Structure{
    uint8_t LDIR_Ord;
    uint16_t LDIR_Name1[5];
    uint8_t LDIR_Attr;
    uint8_t LDIR_Type;
    uint8_t LDIR_Chksum;
    uint16_t LDIR_Name2[6];
    uint16_t LDIR_FstClusLO;
    uint16_t LDIR_Name3[2];
};

// structure for file read information
typedef struct PACKED _file_stat{
    uint32_t currentCluster;
    uint32_t fileSize;
    uint32_t currentSector;
    uint32_t byteCounter;
    int sectorIndex;
} file_stat;

typedef struct PACKED _file_position {
    uint8_t isLongFilename;
    uint8_t *fileName;
    uint32_t startCluster;
    uint32_t cluster;
    uint32_t dirStartCluster;
    uint8_t sectorIndex;
    uint32_t sector;
    uint32_t fileSize;
    uint32_t byteCounter;
    uint16_t byte;
    uint8_t shortFilename[11];
} file_position;

#define MAX_FILENAME       256

namespace bitfixer {

class FAT32 : public DataSource
{
public:
    FAT32()
    : _sd(NULL)
    , _FatBuffer(NULL)
    , _longEntryString(NULL)
    , _logger(NULL)
    , _currentDirectoryEntry(0)
    , _initialized(false)
    , _rootCluster(0)
    {

    }

    FAT32(SD* sd, uint8_t* fatbuffer, uint8_t* longEntryBuffer, SerialLogger* logger)
    : _sd(sd)
    , _FatBuffer(fatbuffer)
    , _longEntryString(longEntryBuffer)
    , _logger(logger)
    , _currentDirectoryEntry(0)
    , _initialized(false)
    , _rootCluster(0)
    {

    }

    ~FAT32() {}

    bool initWithParams(SD* sd, uint8_t* fatbuffer, uint8_t* longEntryBuffer, SerialLogger* logger);
    bool init();
    bool isInitialized();
    uint8_t* getLongEntryString();

    void openDirectory(uint32_t firstCluster);
    bool getNextDirectoryEntry();
    bool openFileForReading(uint8_t *fileName);
    uint16_t getNextFileBlock();
    bool isLastBlock();

    void openFileForWriting(uint8_t *fileName);
    void writeBufferToFile(uint16_t bytesToWrite);
    void updateBlock();
    void closeFile();
    bool isLongFilename();
    bool isHidden();
    bool isVolumeId();
    bool isDirectory();
    uint8_t* getFilename();
    bool openDirectory(const char* dirName);
    void openCurrentDirectory();
    uint8_t* getBuffer();

    bool findFile(char* fileName);
    void deleteFile();
    uint32_t seek(uint32_t pos);

    uint16_t writeBufferSize()
    {
        return 512;
    }
    
private:
    SD* _sd;
    uint8_t* _FatBuffer;
    uint8_t* _longEntryString;
    SerialLogger* _logger;
    file_position _filePosition;
    struct dir_Structure* _currentDirectoryEntry;
    bool _initialized;

    bool _indexed;
    uint32_t _fileClusterIndex[64];

    uint32_t _firstDataSector;
    uint32_t _rootCluster;
    uint32_t _totalClusters;
    uint16_t _bytesPerSector;
    uint16_t _sectorPerCluster;
    uint16_t _reservedSectorCount;
    uint32_t _unusedSectors;
    uint32_t _appendFileSector;
    uint32_t _appendFileLocation;
    uint32_t _fileSize;
    uint32_t _appendStartCluster;

    //flag to keep track of free cluster count updating in FSinfo sector
    uint8_t _freeClusterCountUpdated;
    uint32_t _fileStartCluster;

    uint32_t _currentDirectoryCluster;

    uint32_t getRootCluster();
    uint8_t getBootSectorData (void);
    uint32_t getFirstSector(uint32_t clusterNumber);
    uint32_t getSetFreeCluster(uint8_t totOrNext, uint8_t get_set, uint32_t FSEntry);
    
    uint32_t getSetNextCluster (uint32_t clusterNumber,uint8_t get_set,uint32_t clusterEntry);
    uint8_t readFile (uint8_t flag, uint8_t *fileName);

    void convertToShortFilename(uint8_t *input, uint8_t *output);
    void writeFile (uint8_t *fileName);
    uint32_t searchNextFreeCluster (uint32_t startCluster);
    void freeMemoryUpdate (uint8_t flag, uint32_t size);
    
    void startFileRead(struct dir_Structure *dirEntry, file_stat *thisFileStat);
    void getCurrentFileBlock(file_stat *thisFileStat);
    uint32_t getNextBlockAddress(file_stat *thisFileStat);

    uint32_t getFirstCluster(struct dir_Structure *dir);
    void makeShortFilename(uint8_t *longFilename, uint8_t *shortFilename);

    uint8_t ChkSum (uint8_t *pFcbName);
    uint8_t isLongFilename(uint8_t *fileName);
    uint8_t numCharsToCompare(uint8_t *fileName, uint8_t maxChars);

    bool findFile(char* fileName, uint32_t firstCluster);
    void indexFileForSeeking();

    void copyShortFilename();
};

}

#endif

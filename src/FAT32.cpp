/*
    FAT32.cpp
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

#include "FAT32.h"
#include "SD_routines.h"
#include <string.h>
#include <ctype.h>
#include "Serial.h"
#include <stdio.h>

//Attribute definitions for file/directory
#define ATTR_READ_ONLY     0x01
#define ATTR_HIDDEN        0x02
#define ATTR_SYSTEM        0x04
#define ATTR_VOLUME_ID     0x08
#define ATTR_DIRECTORY     0x10
#define ATTR_ARCHIVE       0x20
#define ATTR_LONG_NAME     0x0f


#define DIR_ENTRY_SIZE     0x32
#define EMPTY              0x00
#define DELETED            0xe5
#define GET                0
#define SET                1
#define READ               0
#define VERIFY             1
#define ADD                0
#define REMOVE             1
#define LOW                0
#define HIGH               1
#define TOTAL_FREE         1
#define NEXT_FREE          2
#define GET_LIST           0
#define GET_FILE           1
#define DELETE             2
#define FAT_EOF            0x0fffffff

//Structure to access Master Boot Record for getting info about partitions
struct PACKED MBRinfo_Structure{
uint8_t   nothing[446];       //ignore, placed here to fill the gap in the structure
uint8_t   partitionData[64];  //partition records (16x4)
uint16_t    signature;      //0xaa55
};

//Structure to access info of the first partioion of the disk 
struct PACKED partitionInfo_Structure{                 
uint8_t   status;             //0x80 - active partition
uint8_t   headStart;          //starting head
uint16_t    cylSectStart;       //starting cylinder and sector
uint8_t   type;               //partition type 
uint8_t   headEnd;            //ending head of the partition
uint16_t    cylSectEnd;         //ending cylinder and sector
uint32_t   firstSector;        //total sectors between MBR & the first sector of the partition
uint32_t   sectorsTotal;       //size of this partition in sectors
};

//Structure to access boot sector data
struct PACKED BS_Structure{
uint8_t jumpBoot[3]; //default: 0x009000EB   //3
uint8_t OEMName[8];                          //11
uint16_t bytesPerSector; //deafault: 512       //13
uint8_t sectorPerCluster;                    //14
uint16_t reservedSectorCount;                  //16
uint8_t numberofFATs;                        //17
uint16_t rootEntryCount;                       //19
uint16_t totalSectors_F16; //must be 0 for FAT32    //21
uint8_t mediaType;                           //22
uint16_t FATsize_F16; //must be 0 for FAT32    //24
uint16_t sectorsPerTrack;                      //26
uint16_t numberofHeads;                        //28
uint32_t hiddenSectors;                       //32
uint32_t totalSectors_F32;                    //36
uint32_t FATsize_F32; //count of sectors occupied by one FAT   //40
uint16_t extFlags;                              //42
uint16_t FSversion; //0x0000 (defines version 0.0)  //44
uint32_t rootCluster; //first cluster of root directory (=2) //48
uint16_t FSinfo; //sector number of FSinfo structure (=1)   //50
uint16_t BackupBootSector;                      //52
uint8_t reserved[12];                         //64
uint8_t driveNumber;                          //65
uint8_t reserved1;                            //66
uint8_t bootSignature;                        //67
uint32_t volumeID;                             //71
uint8_t volumeLabel[11]; //"NO NAME "         //82
uint8_t fileSystemType[8]; //"FAT32"          //90
uint8_t bootData[420];                        //510
uint16_t bootEndSignature; //0xaa55             //512
};


//Structure to access FSinfo sector data
struct PACKED FSInfo_Structure
{
uint32_t leadSignature; //0x41615252
uint8_t reserved1[480];
uint32_t structureSignature; //0x61417272
uint32_t freeClusterCount; //initial: 0xffffffff
uint32_t nextFreeCluster; //initial: 0xffffffff
uint8_t reserved2[12];
uint32_t trailSignature; //0xaa550000
};

namespace bitfixer {

bool FAT32::initWithParams(SD* sd, uint8_t* fatbuffer, uint8_t* longEntryBuffer, SerialLogger* logger)
{
    _sd = sd;
    _FatBuffer = fatbuffer;
    _longEntryString = longEntryBuffer;
    _logger = logger;
    return init();
}

bool FAT32::init()
{
    _initialized = false;
    if (_sd->init() != 0)
    {
        _initialized = false;
        return _initialized;
    }

    uint8_t res = getBootSectorData();
    if (res == 0)
    {
        _initialized = true;
    }
    
    return _initialized;
}


bool FAT32::isInitialized()
{
    return _initialized;
}
//***************************************************************************
//Function: to read data from boot sector of SD card, to determine important
//parameters like _bytesPerSector, sectorsPerCluster etc.
//Arguments: none
//***************************************************************************
uint8_t FAT32::getBootSectorData (void)
{
    struct BS_Structure *bpb; //mapping the buffer onto the structure
    struct MBRinfo_Structure *mbr;
    struct partitionInfo_Structure *partition;
    uint32_t dataSectors;

    _unusedSectors = 0;

    if (_rootCluster != 0 && _currentDirectoryCluster != _rootCluster)
    {
        return 0;
    }

    _sd->readSingleBlock(0, _FatBuffer);
    bpb = (struct BS_Structure *)_FatBuffer;

    if ( bpb->jumpBoot[0] != 0xE9 && bpb->jumpBoot[0] != 0xEB )   //check if it is boot sector
    {
        mbr = (struct MBRinfo_Structure *) _FatBuffer;       //if it is not boot sector, it must be MBR

        if (mbr->signature != 0xaa55) return 1;       //if it is not even MBR then it's not FAT32
        
        partition = (struct partitionInfo_Structure *)(mbr->partitionData);//first partition
        _unusedSectors = partition->firstSector; //the unused sectors, hidden to the FAT

        _sd->readSingleBlock(partition->firstSector, _FatBuffer);//read the bpb sector
        bpb = (struct BS_Structure *)_FatBuffer;
        if (bpb->jumpBoot[0] != 0xE9 && bpb->jumpBoot[0]!=0xEB) return 1; 
    }

    _bytesPerSector = bpb->bytesPerSector;
    _sectorPerCluster = bpb->sectorPerCluster;
    _reservedSectorCount = bpb->reservedSectorCount;
    _rootCluster = bpb->rootCluster; // + (sector / _sectorPerCluster) +1;
    _firstDataSector = bpb->hiddenSectors + _reservedSectorCount + (bpb->numberofFATs * bpb->FATsize_F32);

    dataSectors = bpb->totalSectors_F32
                  - bpb->reservedSectorCount
                  - ( bpb->numberofFATs * bpb->FATsize_F32);
    _totalClusters = dataSectors / _sectorPerCluster;

    if ((getSetFreeCluster (TOTAL_FREE, GET, 0)) > _totalClusters)  //check if FSinfo free clusters count is valid
    {
         _freeClusterCountUpdated = 0;
    }
    else
    {
         _freeClusterCountUpdated = 1;
    }

    _currentDirectoryCluster = _rootCluster;
    return 0;
}

uint32_t FAT32::getRootCluster()
{
    return _rootCluster;
}

uint8_t* FAT32::getLongEntryString()
{
    return _longEntryString;
}
//***************************************************************************
//Function: to calculate first sector address of any given cluster
//Arguments: cluster number for which first sector is to be found
//return: first sector address
//***************************************************************************

uint32_t FAT32::getFirstSector(uint32_t clusterNumber)
{
    return (((clusterNumber - 2) * _sectorPerCluster) + _firstDataSector);
}

//***************************************************************************
//Function: get cluster entry value from FAT to find out the next cluster in the chain
//or set new cluster entry in FAT
//Arguments: 1. current cluster number, 2. get_set (=GET, if next cluster is to be found or = SET,
//if next cluster is to be set 3. next cluster number, if argument#2 = SET, else 0
//return: next cluster number, if if argument#2 = GET, else 0
//****************************************************************************
uint32_t FAT32::getSetNextCluster (uint32_t clusterNumber,
                                 uint8_t get_set,
                                 uint32_t clusterEntry)
{
    uint16_t FATEntryOffset;
    uint32_t *FATEntryValue;
    uint32_t FATEntrySector;
    uint8_t retry = 0;

    //get sector number of the cluster entry in the FAT
    FATEntrySector = _unusedSectors + _reservedSectorCount + ((clusterNumber * 4) / _bytesPerSector) ;

    //get the offset address in that sector number
    FATEntryOffset = (uint16_t) ((clusterNumber * 4) % _bytesPerSector);

    //read the sector into a buffer
    while(retry < 10)
    { 
        if (!_sd->readSingleBlock(FATEntrySector,_FatBuffer)) 
        {
            break;
        } 
        retry++;
    }

    //get the cluster address from the buffer
    FATEntryValue = (uint32_t *) &_FatBuffer[FATEntryOffset];

    if (get_set == GET)
    {
        return ((*FATEntryValue) & 0x0fffffff);
    }

    *FATEntryValue = clusterEntry;   //for setting new value in cluster entry in FAT

    _sd->writeSingleBlock(FATEntrySector, _FatBuffer);

    return (0);
}

//********************************************************************************************
//Function: to get or set next free cluster or total free clusters in FSinfo sector of SD card
//Arguments: 1.flag:TOTAL_FREE or NEXT_FREE, 
//           2.flag: GET or SET 
//           3.new FS entry, when argument2 is SET; or 0, when argument2 is GET
//return: next free cluster, if arg1 is NEXT_FREE & arg2 is GET
//        total number of free clusters, if arg1 is TOTAL_FREE & arg2 is GET
//        0xffffffff, if any error or if arg2 is SET
//********************************************************************************************
uint32_t FAT32::getSetFreeCluster(uint8_t totOrNext, uint8_t get_set, uint32_t FSEntry)
{
    struct FSInfo_Structure *FS = (struct FSInfo_Structure *) &_FatBuffer;
    
    _sd->readSingleBlock(_unusedSectors + 1, _FatBuffer);

    if ((FS->leadSignature != 0x41615252) || 
        (FS->structureSignature != 0x61417272) || 
        (FS->trailSignature !=0xaa550000))
    {
        return 0xffffffff;
    }

    if (get_set == GET)
    {
        if(totOrNext == TOTAL_FREE)
        {
            return(FS->freeClusterCount);
        }
        else // when totOrNext = NEXT_FREE
        {
            return(FS->nextFreeCluster);
        }
    }
    else
    {
        if (totOrNext == TOTAL_FREE)
        {
            FS->freeClusterCount = FSEntry;
        }
        else // when totOrNext = NEXT_FREE
        {
            FS->nextFreeCluster = FSEntry;
        }

        _sd->writeSingleBlock(_unusedSectors + 1, _FatBuffer);   //update FSinfo
     }
     return 0xffffffff;
}

uint8_t FAT32::isLongFilename(uint8_t *fileName)
{
    //uint8_t filenameLength = strlen(fileName);
    uint8_t filenameLength = 0;
    uint8_t i;
    while (fileName[filenameLength] != 0)
    {
        filenameLength++;
    }
    
    // if file is longer than 12 characters, not possible to be short filename
    if (filenameLength > 12)
    {
        return 1;
    }
    
    // if filename > 8, it has an extension if it's a short filename
    if (filenameLength > 8)
    {
        if (fileName[filenameLength-4] != '.')
        {
            // no extension or extension is an odd length
            // this is a long filename
            return 1;
        }
    }
    
    // check if it contains a space or non-uppercase letters
    for (i = 0; i < filenameLength; i++)
    {
        if (fileName[i] == ' ' || !isupper(fileName[i]))
        {
            // short filenames cannot have spaces or non-uppercase letters
            if (fileName[i] == '.')
            {
                if (i == filenameLength - 4)
                {
                    // allow a dot only preceding a 3 character extension
                    continue;
                }
            }
            return 1;
        }
    }
    
    
    return 0;
}

bool FAT32::isHidden()
{
    if ((_currentDirectoryEntry->attrib & ATTR_HIDDEN) != 0)
    {
        return true;
    }

    return false;
}

bool FAT32::isVolumeId()
{
    if ((_currentDirectoryEntry->attrib & ATTR_VOLUME_ID) != 0)
    {
        return true;
    }

    return false;
}

bool FAT32::isDirectory()
{
    if ((_currentDirectoryEntry->attrib & ATTR_DIRECTORY) != 0)
    {
        return true;
    }

    return false;
}

uint8_t FAT32::numCharsToCompare(uint8_t *fileName, uint8_t maxChars)
{
    uint8_t numChars = 0;
    while(numChars < maxChars && fileName[numChars] != 0 && fileName[numChars] != '*')
    {
        numChars++;
    }
    return numChars;
}

void FAT32::openCurrentDirectory()
{
    openDirectory(_currentDirectoryCluster);
}

void FAT32::openDirectory(uint32_t firstCluster)
{
    // store cluster
    _filePosition.startCluster = firstCluster;
    _filePosition.cluster = firstCluster;
    _filePosition.sectorIndex = 0;
    _filePosition.byteCounter = 0;
    _currentDirectoryEntry = 0;
}

bool FAT32::openDirectory(const char* dirName)
{
    // find the requested directory
    bool gotDir = findFile((char*)dirName, _currentDirectoryCluster);
    if (gotDir)
    {
        _currentDirectoryCluster = getFirstCluster(_currentDirectoryEntry);
        if (_currentDirectoryCluster == 0)
        {
            _currentDirectoryCluster = _rootCluster;
        }
        openDirectory(_currentDirectoryCluster);
        return true;
    }

    return false;
}

void FAT32::deleteFile()
{
    uint32_t sector;
    uint32_t byte;
    struct dir_Structure *dir;
    
    sector = getFirstSector(_filePosition.cluster) + _filePosition.sectorIndex;
    _sd->readSingleBlock(sector, _FatBuffer);
    
    byte = _filePosition.byteCounter-32;
    dir = (struct dir_Structure *) &_FatBuffer[byte];
    dir->name[0] = DELETED;
    _sd->writeSingleBlock(sector, _FatBuffer);
}

bool FAT32::getNextDirectoryEntry()
{
    uint32_t firstSector;
    struct dir_Structure *dir;
    struct dir_Longentry_Structure *longent;
    uint8_t ord;
    uint8_t this_long_filename_length;
    uint8_t k;
    
    // reset long entry info
    memset((void *)_longEntryString, 0, MAX_FILENAME);
    _filePosition.isLongFilename = 0;
    _filePosition.fileName = (uint8_t *)_longEntryString;

    while(1)
    {
        firstSector = getFirstSector(_filePosition.cluster);
        
        for (; _filePosition.sectorIndex < _sectorPerCluster; _filePosition.sectorIndex++)
        {
            _sd->readSingleBlock(firstSector + _filePosition.sectorIndex, _FatBuffer);
            for (; _filePosition.byteCounter < _bytesPerSector; _filePosition.byteCounter += 32)
            {
                // get current directory entry
                dir = (struct dir_Structure *) &_FatBuffer[_filePosition.byteCounter];
                
                if (dir->name[0] == EMPTY)
                {
                    return false;
                }
                
                // this is a valid file entry
                if((dir->name[0] != DELETED) && (dir->attrib != ATTR_LONG_NAME))
                {
                    _filePosition.byteCounter += 32;
                    _currentDirectoryEntry = dir;
                    return true;
                }
                else if (dir->attrib == ATTR_LONG_NAME)
                {
                    _filePosition.isLongFilename = 1;
                    
                    longent = (struct dir_Longentry_Structure *) &_FatBuffer[_filePosition.byteCounter];
                    
                    ord = (longent->LDIR_Ord & 0x0F) - 1;
                    this_long_filename_length = (13*ord);

                    for (k = 0; k < 5; k++)
                    {
                        _longEntryString[this_long_filename_length] = (uint8_t)longent->LDIR_Name1[k];
                        this_long_filename_length++;
                    }
                    
                    for (k = 0; k < 6; k++)
                    {
                        _longEntryString[this_long_filename_length] = (uint8_t)longent->LDIR_Name2[k];
                        this_long_filename_length++;
                    }

                    for (k = 0; k < 2; k++)
                    {
                        _longEntryString[this_long_filename_length] = (uint8_t)longent->LDIR_Name3[k];
                        this_long_filename_length++;
                    }
                }
            }
            // done with this sector
            _filePosition.byteCounter = 0;
        }
        // done with this cluster
        _filePosition.sectorIndex = 0;
        _filePosition.cluster = getSetNextCluster(_filePosition.cluster, GET, 0);
        
        // last cluster on the card
        if (_filePosition.cluster > 0x0ffffff6)
        {
            return false;
        }
        if (_filePosition.cluster == 0)
        {
            //transmitString_F((char *)PSTR("Error in getting cluster"));
            return false;
        }
    }
}

void FAT32::convertToShortFilename(uint8_t *input, uint8_t *output)
{
    uint8_t extPos;
    uint8_t inputLen;
    
    inputLen = (uint8_t)strlen((char *)input);
     
    // set empty chars to space
    memset(output, ' ', 11);
    
    extPos = 0;
    if (inputLen >= 5)
    {
        if (input[inputLen-4] == '.')
        {
            extPos = inputLen-4;
        }
    }
    
    if (extPos > 0)
    {
        memcpy(output, input, extPos);
        memcpy(&output[8], &input[extPos+1], 3);
    }
    else
    {
        memcpy(output, input, inputLen);
    }
}

bool FAT32::findFile(char* fileName, uint32_t firstCluster)
{
    uint8_t cmp_length;
    char* ustr;
    bool gotDir;
    int result;

    strupr(fileName);
    cmp_length = numCharsToCompare((uint8_t*)fileName, strlen(fileName));

    // open the specified directory
    openDirectory(firstCluster);
    do
    {
        gotDir = getNextDirectoryEntry();
        if (!gotDir)
        {
            return false;
        }

        ustr = strupr((char*)getFilename());
        result = strncmp((char*)fileName, ustr, cmp_length);

        if (result == 0)
        {
            return true;
        }
    } while (gotDir);

    return 0;
}

bool FAT32::findFile(char* fileName)
{
    return findFile(fileName, _currentDirectoryCluster);
}

uint32_t FAT32::getFirstCluster(struct dir_Structure *dir)
{
    return (((uint32_t) dir->firstClusterHI) << 16) | dir->firstClusterLO;
}

bool FAT32::openFileForReading(uint8_t *fileName)
{
    bool gotDir;
    
    gotDir = findFile((char*)fileName, _currentDirectoryCluster);
    if (gotDir == false)
    {
        return false;
    }
    
    _filePosition.fileSize = _currentDirectoryEntry->fileSize;
    _filePosition.startCluster = getFirstCluster(_currentDirectoryEntry);
    
    _filePosition.cluster = _filePosition.startCluster;
    _filePosition.byteCounter = 0;
    _filePosition.sectorIndex = 0;
    _filePosition.dirStartCluster = _currentDirectoryCluster;

    _indexed = false;
    
    return true;
}

uint32_t FAT32::getFileSize()
{
    return _filePosition.fileSize;
}

uint32_t FAT32::seek(uint32_t pos)
{
    // given position, figure out which cluster and sector 
    // within the file this position represents.
    if (!_indexed)
    {
        indexFileForSeeking();
        _indexed = true;
    }

    // seeking is quantized to sector boundaries, for simplicity
    uint32_t q_pos = (pos / _bytesPerSector) * _bytesPerSector;
    uint16_t sectors = q_pos / _bytesPerSector;
    uint16_t clusters = sectors / _sectorPerCluster;
    uint16_t sectorInCluster = sectors - (clusters * _sectorPerCluster);

    _filePosition.cluster = _fileClusterIndex[clusters];
    _filePosition.sectorIndex = sectorInCluster;
    _filePosition.byteCounter = q_pos;

    return q_pos;
}

void FAT32::indexFileForSeeking()
{
    bool done = false;

    int clustersInFile = 0;
    _fileClusterIndex[clustersInFile++] = _filePosition.startCluster;
    _filePosition.sectorIndex = 0;
    _filePosition.byteCounter = 0;
    while (!done)
    {   
        if (_filePosition.sectorIndex == _sectorPerCluster)
        {
            _filePosition.sectorIndex = 0;
            _filePosition.cluster = getSetNextCluster(_filePosition.cluster, GET, 0);
            _fileClusterIndex[clustersInFile++] = _filePosition.cluster;
        }

        _filePosition.byteCounter += 512;
        _filePosition.sectorIndex++;

        if (_filePosition.byteCounter >= _filePosition.fileSize)
        {
            done = true;
        }
    }
}

uint16_t FAT32::getNextFileBlock()
{
    uint32_t sector;
    // if cluster has no more sectors, move to next cluster
    if (_filePosition.sectorIndex == _sectorPerCluster)
    {
        _filePosition.sectorIndex = 0;
        _filePosition.cluster = getSetNextCluster(_filePosition.cluster, GET, 0);
    }
    
    sector = getFirstSector(_filePosition.cluster) + _filePosition.sectorIndex;
    
    _sd->readSingleBlock(sector, _FatBuffer);
    _filePosition.byteCounter += 512;
    _filePosition.sectorIndex++;
    
    if (_filePosition.byteCounter > _filePosition.fileSize)
    {
        return _filePosition.fileSize - (_filePosition.byteCounter - 512);
    }
    else
    {
        return 512;
    }
}

bool FAT32::isLastBlock()
{
    if (_filePosition.byteCounter >= _filePosition.fileSize)
    {
        return true;
    }

    return false;
}

bool FAT32::isLongFilename()
{
    return _filePosition.isLongFilename;
}

void FAT32::copyShortFilename()
{
    // copy short name into file name buffer
    memset(_filePosition.fileName, 0, MAX_FILENAME);
    int name_len = 0;
    for (int i = 0; i < 8; i++)
    {
        if (_currentDirectoryEntry->name[i] == ' ')
        {
            break;
        }
        
        _filePosition.fileName[name_len++] = _currentDirectoryEntry->name[i];
    }

    if (_currentDirectoryEntry->name[8] != ' ')
    {
        _filePosition.fileName[name_len++] = '.';
        for (int i = 0; i < 3; i++)
        {
            if (_currentDirectoryEntry->name[8+i] == ' ')
            {
                break;
            }

            _filePosition.fileName[name_len++] = _currentDirectoryEntry->name[8+i];
        }
    }
}

uint8_t* FAT32::getFilename()
{
    //return _filePosition.fileName;
    if (_filePosition.isLongFilename)
    {
        if (_filePosition.fileName[0] == 0)
        {
            copyShortFilename();
        }
        return _filePosition.fileName;
    }
    else
    {
        copyShortFilename();
        return _filePosition.fileName;
    }
}

uint8_t* FAT32::getBuffer()
{
    return _FatBuffer;
}

// open a new file for writing
void FAT32::openFileForWriting(uint8_t *fileName)
{
    // first, check if file already exists
    openCurrentDirectory();
    if (findFile((char*)fileName))
    {
        // remove existing file first
        deleteFile();
    }

    uint32_t cluster;
    uint8_t i;
    
    // use existing buffer for filename
    _filePosition.fileName = (uint8_t *)_longEntryString;
    memset(_filePosition.fileName, 0, MAX_FILENAME);
    
    i = 0;
    while (fileName[i] != 0)
    {
        //transmitHex(CHAR, i);
        _filePosition.fileName[i] = fileName[i];
        i++;
    }

    memset((void *)_filePosition.shortFilename, 0, 11);
    
    // find the start cluster for this file
    cluster = getSetFreeCluster(NEXT_FREE, GET, 0);
    if (cluster > _totalClusters)
    {
        cluster = _rootCluster;
    }
    
    // set the start cluster with EOF
    cluster = searchNextFreeCluster(cluster);
    
    getSetNextCluster(cluster, SET, FAT_EOF);   //last cluster of the file, marked EOF
    
    _filePosition.startCluster = cluster;
    _filePosition.cluster = cluster;
    _filePosition.fileSize = 0;
    _filePosition.sectorIndex = 0;
    _filePosition.dirStartCluster = _currentDirectoryCluster;
}

void FAT32::writeBufferToFile(uint16_t bytesToWrite)
{
    uint32_t nextCluster;
    uint32_t sector;
    // write a block to current file
    sector = getFirstSector(_filePosition.cluster) + _filePosition.sectorIndex;
    
    _sd->writeSingleBlock(sector, _FatBuffer);
    _filePosition.fileSize += bytesToWrite;
    _filePosition.sectorIndex++;
    
    if (_filePosition.sectorIndex == _sectorPerCluster)
    {
        _filePosition.sectorIndex = 0;
        // get the next free cluster
        nextCluster = searchNextFreeCluster(_filePosition.cluster);
        // link the previous cluster
        getSetNextCluster(_filePosition.cluster, SET, nextCluster);
        // set the last cluster with EOF
        getSetNextCluster(nextCluster, SET, FAT_EOF);
        _filePosition.cluster = nextCluster;
    }
}

void FAT32::updateBlock()
{
    // write a block to current file
    uint32_t sector = getFirstSector(_filePosition.cluster) + _filePosition.sectorIndex;
    _sd->writeSingleBlock(sector, _FatBuffer);
}

void FAT32::closeFile()
{
    uint8_t fileCreatedFlag = 0;
    uint8_t sector, j;
    uint32_t prevCluster, firstSector, cluster;
    uint16_t firstClusterHigh, i;
    uint16_t firstClusterLow;
    struct dir_Structure *dir;
    uint8_t checkSum;
    uint8_t islongfilename;
    
    struct dir_Longentry_Structure *longent;
    
    uint8_t fname_len;
    uint8_t fname_remainder;
    uint8_t num_long_entries;
    uint8_t curr_fname_pos;
    uint8_t curr_long_entry;
     
    islongfilename = isLongFilename(_filePosition.fileName);
    num_long_entries = 0;
    fname_len = 0;
    checkSum = 0;
    curr_long_entry = 0;
    
    if (islongfilename == 1)
    {
        memset((void *)_filePosition.shortFilename, ' ', 11);
        makeShortFilename(_filePosition.fileName, (uint8_t *)_filePosition.shortFilename);
        checkSum = ChkSum((uint8_t *)_filePosition.shortFilename);
        
        fname_len = strlen((char *)_filePosition.fileName);
        fname_remainder = fname_len % 13;
        num_long_entries = ((fname_len - fname_remainder) / 13) + 1;
        
        curr_long_entry = num_long_entries;
    }
    else
    {
        // make short filename into FAT format
        convertToShortFilename(_filePosition.fileName, (uint8_t *)_filePosition.shortFilename);
    }
    
    // set next free cluster in FAT
    getSetFreeCluster (NEXT_FREE, SET, _filePosition.cluster); //update FSinfo next free cluster entry
    
    prevCluster = _filePosition.dirStartCluster;
    
    while(1)
    {
        firstSector = getFirstSector (prevCluster);
        
        for(sector = 0; sector < _sectorPerCluster; sector++)
        {
            _sd->readSingleBlock (firstSector + sector, _FatBuffer);
            
            for( i = 0; i < _bytesPerSector; i += 32)
            {
                dir = (struct dir_Structure *) &_FatBuffer[i];
                
                if(fileCreatedFlag)   //to mark last directory entry with 0x00 (empty) mark
                {                     //indicating end of the directory file list
                    dir->name[0] = EMPTY;
                    _sd->writeSingleBlock(firstSector + sector, _FatBuffer);
                    
                    freeMemoryUpdate (REMOVE, _filePosition.fileSize); //updating free memory count in FSinfo sector
                    return;
                }
                
                if (islongfilename == 0)
                {
                    if (dir->name[0] == EMPTY)
                    {
                        memcpy((void *)dir->name, (void *)_filePosition.shortFilename, 11);

                        dir->attrib = ATTR_ARCHIVE; //setting file attribute as 'archive'
                        dir->NTreserved = 0;        //always set to 0
                        dir->timeTenth = 0;         //always set to 0

                        dir->createTime = _timeValue;
                        dir->createDate = _dateValue;
                        dir->lastAccessDate = _dateValue;
                        dir->writeTime = _timeValue;
                        dir->writeDate = _dateValue;
                        
                        firstClusterHigh = (uint16_t) ((_filePosition.startCluster & 0xffff0000) >> 16 );
                        firstClusterLow = (uint16_t) ( _filePosition.startCluster & 0x0000ffff);
                        
                        dir->firstClusterHI = firstClusterHigh;
                        dir->firstClusterLO = firstClusterLow;
                        dir->fileSize = _filePosition.fileSize;
                        
                        _sd->writeSingleBlock (firstSector + sector, _FatBuffer);
                        fileCreatedFlag = 1;
                    }
                }
                else
                {
                    if (dir->name[0] == EMPTY)
                    {
                        // create long directory entry
                        longent = (struct dir_Longentry_Structure *) &_FatBuffer[i];
                        memset(longent, 0xff, 32);
                        
                        // fill in the long entry fields
                        if (curr_long_entry == num_long_entries)
                        {
                            longent->LDIR_Ord = 0x40 | curr_long_entry;
                        }
                        else
                        {
                            longent->LDIR_Ord = curr_long_entry;
                        }
                        
                        curr_long_entry--;
                        curr_fname_pos = curr_long_entry * 13;
                        
                        j = 0;
                        while (curr_fname_pos <= fname_len && j < 5)
                        {
                            longent->LDIR_Name1[j++] = _filePosition.fileName[curr_fname_pos++];
                        }
                        
                        j = 0;
                        while (curr_fname_pos <= fname_len && j < 6)
                        {
                            longent->LDIR_Name2[j++] = _filePosition.fileName[curr_fname_pos++];
                        }
                        
                        j = 0;
                        while (curr_fname_pos <= fname_len && j < 2)
                        {
                            longent->LDIR_Name3[j++] = _filePosition.fileName[curr_fname_pos++];
                        }
                        
                        longent->LDIR_Attr = ATTR_LONG_NAME;
                        longent->LDIR_Type = 0;
                        longent->LDIR_Chksum = checkSum;
                        longent->LDIR_FstClusLO = 0;
                        
                        _sd->writeSingleBlock (firstSector + sector, _FatBuffer);
                        
                        // if there are no long entries remaining, set a flag so the next entry is the FAT short entry
                        if (curr_long_entry == 0)
                        {
                            islongfilename = 0;
                        }
                    }
                }
            }
        }
        
        cluster = getSetNextCluster (prevCluster, GET, 0);
        
        if(cluster > 0x0ffffff6)
        {
            if(cluster == FAT_EOF)   //this situation will come when total files in root is multiple of (32*_sectorPerCluster)
            {  
                cluster = searchNextFreeCluster(prevCluster); //find next cluster for root directory entries
                getSetNextCluster(prevCluster, SET, cluster); //link the new cluster of root to the previous cluster
                getSetNextCluster(cluster, SET, FAT_EOF);  //set the new cluster as end of the root directory
            } 
            
            else
            {   
                //transmitString_F((char *)PSTR("End of Cluster Chain"));
                return;
            }
        }
        if(cluster == 0) 
        {
            //transmitString_F((char *)PSTR("Error in getting cluster"));
            return;
        }
        
        prevCluster = cluster;
    }
}

//***************************************************************************
//Function: to search for the next free cluster in the root directory
//          starting from a specified cluster
//Arguments: Starting cluster
//return: the next free cluster
//****************************************************************
uint32_t FAT32::searchNextFreeCluster (uint32_t startCluster)
{
    uint32_t cluster, *value, sector;
    uint8_t i;
    
    startCluster -=  (startCluster % 128);   //to start with the first file in a FAT sector
    for(cluster =startCluster; cluster <_totalClusters; cluster+=128) 
    {
        sector = _unusedSectors + _reservedSectorCount + ((cluster * 4) / _bytesPerSector);
        _sd->readSingleBlock(sector, _FatBuffer);
        for (i = 0; i < 128; i++)
        {
            value = (uint32_t *) &_FatBuffer[i*4];
            if (((*value) & 0x0fffffff) == 0)
            {
                return(cluster+i);
            }
        }
    }
    return 0;
}

//********************************************************************
//Function: update the free memory count in the FSinfo sector. 
//          Whenever a file is deleted or created, this function will be called
//          to ADD or REMOVE clusters occupied by the file
//Arguments: #1.flag ADD or REMOVE #2.file size in Bytes
//return: none
//********************************************************************
void FAT32::freeMemoryUpdate (uint8_t flag, uint32_t size)
{
  uint32_t freeClusters;
  //convert file size into number of clusters occupied
  if ((size % 512) == 0)
      size = size / 512;
  else
      size = (size / 512) +1;
    
  if ((size % 8) == 0)
      size = size / 8;
  else
      size = (size / 8) +1;

  if(_freeClusterCountUpdated)
  {
    freeClusters = getSetFreeCluster (TOTAL_FREE, GET, 0);
    if(flag == ADD)
       freeClusters = freeClusters + size;
    else  //when flag = REMOVE
       freeClusters = freeClusters - size;
    getSetFreeCluster (TOTAL_FREE, SET, freeClusters);
  }
}

void FAT32::makeShortFilename(uint8_t *longFilename, uint8_t *shortFilename)
{
    // make a short file name from the given long file name
    int i;
    uint8_t thechar;
    for (i = 0; i < 6; i++)
    {
        thechar = longFilename[i];
        
        if (longFilename[i] >= 'a' && longFilename[i] <= 'z')
        {
            thechar = longFilename[i] - 32;
        }
        
        if (thechar < 'A' || thechar > 'Z')
        {
            thechar = '_';
        }
        
        shortFilename[i] = thechar;
    }
     
    shortFilename[6] = '~';
    shortFilename[7] = '1';
    shortFilename[8] = 'P';
    shortFilename[9] = 'R';
    shortFilename[10] = 'G';
}


//-----------------------------------------------------------------------------
//  ChkSum()
//  Returns an unsigned byte checksum computed on an unsigned byte
//  array.  The array must be 11 bytes long and is assumed to contain
//  a name stored in the format of a MS-DOS directory entry.
//  Passed:  pFcbName    Pointer to an unsigned byte array assumed to be
//                          11 bytes long.
//  Returns: Sum         An 8-bit unsigned checksum of the array pointed
//                           to by pFcbName.
//------------------------------------------------------------------------------
uint8_t FAT32::ChkSum (uint8_t *pFcbName)
{
    int FcbNameLen;
    uint8_t Sum;

    Sum = 0;
    for (FcbNameLen=11; FcbNameLen!=0; FcbNameLen--) {
        // NOTE: The operation is an uint8_t rotate right
        Sum = ((Sum & 1) ? 0x80 : 0) + (Sum >> 1) + *pFcbName++;
    }
    return (Sum);
}

void FAT32::setDateTime(int year, int month, int day, int hour, int minute, int second)
{
    // convert unix timestamp to 16 bit date and time values used by fat32.
    _dateValue = (uint16_t)(year - 1980) << 9; // top 7 bits
    _dateValue += (uint16_t)month  << 5;
    _dateValue += (uint16_t)day;

    _timeValue = (uint16_t)hour << 11; // top 5 bits
    _timeValue += (uint16_t)minute << 5;
    _timeValue += (uint16_t)(second / 2);

    _logger->printf("d %X t %X\n", _dateValue, _timeValue);
}

}
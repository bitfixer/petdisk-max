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

#include <avr/io.h>
#include <avr/pgmspace.h>
#include "FAT32_tiny.h"
#include "SD_routines.h"
#include <string.h>
#include <ctype.h>

// test
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
struct MBRinfo_Structure{
unsigned char   nothing[446];       //ignore, placed here to fill the gap in the structure
unsigned char   partitionData[64];  //partition records (16x4)
unsigned int    signature;      //0xaa55
};

//Structure to access info of the first partioion of the disk 
struct partitionInfo_Structure{                 
unsigned char   status;             //0x80 - active partition
unsigned char   headStart;          //starting head
unsigned int    cylSectStart;       //starting cylinder and sector
unsigned char   type;               //partition type 
unsigned char   headEnd;            //ending head of the partition
unsigned int    cylSectEnd;         //ending cylinder and sector
unsigned long   firstSector;        //total sectors between MBR & the first sector of the partition
unsigned long   sectorsTotal;       //size of this partition in sectors
};

//Structure to access boot sector data
struct BS_Structure{
unsigned char jumpBoot[3]; //default: 0x009000EB   //3
unsigned char OEMName[8];                          //11
unsigned int bytesPerSector; //deafault: 512       //13
unsigned char sectorPerCluster;                    //14
unsigned int reservedSectorCount;                  //16
unsigned char numberofFATs;                        //17
unsigned int rootEntryCount;                       //19
unsigned int totalSectors_F16; //must be 0 for FAT32    //21
unsigned char mediaType;                           //22
unsigned int FATsize_F16; //must be 0 for FAT32    //24
unsigned int sectorsPerTrack;                      //26
unsigned int numberofHeads;                        //28
unsigned long hiddenSectors;                       //32
unsigned long totalSectors_F32;                    //36
unsigned long FATsize_F32; //count of sectors occupied by one FAT   //40
unsigned int extFlags;                              //42
unsigned int FSversion; //0x0000 (defines version 0.0)  //44
unsigned long rootCluster; //first cluster of root directory (=2) //48
unsigned int FSinfo; //sector number of FSinfo structure (=1)   //50
unsigned int BackupBootSector;                      //52
unsigned char reserved[12];                         //64
unsigned char driveNumber;                          //65
unsigned char reserved1;                            //66
unsigned char bootSignature;                        //67
unsigned long volumeID;                             //71
unsigned char volumeLabel[11]; //"NO NAME "         //82
unsigned char fileSystemType[8]; //"FAT32"          //90
unsigned char bootData[420];                        //510
unsigned int bootEndSignature; //0xaa55             //512
};


//Structure to access FSinfo sector data
struct FSInfo_Structure
{
unsigned long leadSignature; //0x41615252
unsigned char reserved1[480];
unsigned long structureSignature; //0x61417272
unsigned long freeClusterCount; //initial: 0xffffffff
unsigned long nextFreeCluster; //initial: 0xffffffff
unsigned char reserved2[12];
unsigned long trailSignature; //0xaa550000
};


bool FAT32::init()
{
    unsigned char res = getBootSectorData();
    if (res == 0)
    {
        return true;
    }
    
    return false;
}

//***************************************************************************
//Function: to read data from boot sector of SD card, to determine important
//parameters like _bytesPerSector, sectorsPerCluster etc.
//Arguments: none
//***************************************************************************
unsigned char FAT32::getBootSectorData (void)
{
    struct BS_Structure *bpb; //mapping the buffer onto the structure
    struct MBRinfo_Structure *mbr;
    struct partitionInfo_Structure *partition;
    unsigned long dataSectors;

    _unusedSectors = 0;

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
    _currentDirectoryCluster = _rootCluster;
    return 0;
}

//***************************************************************************
//Function: to calculate first sector address of any given cluster
//Arguments: cluster number for which first sector is to be found
//return: first sector address
//***************************************************************************

unsigned long FAT32::getFirstSector(unsigned long clusterNumber)
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
unsigned long FAT32::getNextCluster (unsigned long clusterNumber, unsigned long clusterEntry)
{
    unsigned int FATEntryOffset;
    unsigned long *FATEntryValue;
    unsigned long FATEntrySector;
    unsigned char retry = 0;

    //get sector number of the cluster entry in the FAT
    FATEntrySector = _unusedSectors + _reservedSectorCount + ((clusterNumber * 4) / _bytesPerSector) ;

    //get the offset address in that sector number
    FATEntryOffset = (unsigned int) ((clusterNumber * 4) % _bytesPerSector);

    //read the sector into a buffer
    while (retry < 10)
    { 
        if(!_sd->readSingleBlock(FATEntrySector,_FatBuffer)) break; retry++;
    }

    //get the cluster address from the buffer
    FATEntryValue = (unsigned long *) &_FatBuffer[FATEntryOffset];

    return ((*FATEntryValue) & 0x0fffffff);
}

void FAT32::openDirectory()
{
    // store cluster
    _filePosition.startCluster = _rootCluster;
    _filePosition.cluster = _rootCluster;
    _filePosition.sectorIndex = 0;
    _filePosition.byteCounter = 0;
    _currentDirectoryEntry = 0;
}

bool FAT32::getNextDirectoryEntry()
{
    unsigned long firstSector;
    struct dir_Structure *dir;
    struct dir_Longentry_Structure *longent;
    unsigned char ord;
    unsigned char this_long_filename_length;
    unsigned char k;
    
    // reset long entry info
    _filePosition.isLongFilename = 0;
    _filePosition.fileName = 0;

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
            }
            // done with this sector
            _filePosition.byteCounter = 0;
        }
        // done with this cluster
        _filePosition.sectorIndex = 0;
        _filePosition.cluster = getNextCluster(_filePosition.cluster, 0);
        
        // last cluster on the card
        if (_filePosition.cluster > 0x0ffffff6)
        {
            return false;
        }
        if (_filePosition.cluster == 0)
        {
            return false;
        }
    }
}

unsigned long FAT32::getFirstCluster(struct dir_Structure *dir)
{
    return (((unsigned long) dir->firstClusterHI) << 16) | dir->firstClusterLO;
}

bool FAT32::openFileForReading()
{
    _filePosition.fileSize = _currentDirectoryEntry->fileSize;
    _filePosition.startCluster = getFirstCluster(_currentDirectoryEntry);
    
    _filePosition.cluster = _filePosition.startCluster;
    _filePosition.byteCounter = 0;
    _filePosition.sectorIndex = 0;
    _filePosition.dirStartCluster = _currentDirectoryCluster;
    
    return true;
}

unsigned int FAT32::getNextFileBlock()
{
    unsigned long sector;
    if (_filePosition.byteCounter > _filePosition.fileSize)
    {
        return 0;
    }

    // if cluster has no more sectors, move to next cluster
    if (_filePosition.sectorIndex == _sectorPerCluster)
    {
        _filePosition.sectorIndex = 0;
        _filePosition.cluster = getNextCluster(_filePosition.cluster, 0);
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

unsigned char* FAT32::getShortFilename()
{
    return _currentDirectoryEntry->name;
}
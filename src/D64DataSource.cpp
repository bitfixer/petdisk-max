#include <stdio.h>
#include <string.h>
#include "D64DataSource.h"
#include "cbmlayout.h"

bool D64DataSource::initWithDataSource(DataSource* dataSource, const char* fileName, Logger* logger)
{
    _fileDataSource = dataSource;
    _logger = logger;
    _cbmTrackLayout = (uint32_t*)LAYOUT_CBM;

    // mount the file as a D64 drive
    bool success = _fileDataSource->openFileForReading((unsigned char*)fileName);
    if (!success)
    {
        return false;
    }

    cbmMount(&_cbmDisk, (char*)fileName);
    return true;
}

void D64DataSource::openFileForWriting(unsigned char* fileName) {}
bool D64DataSource::openFileForReading(unsigned char* fileName) 
{
    CBMFile_Entry* entry = cbmSearch(&_cbmDisk, fileName, 0);
    if (entry == NULL)
    {
        return false;
    }

    _fileTrackBlock[0] = entry->dataBlock[0];
    _fileTrackBlock[1] = entry->dataBlock[1];
    return true;
}


void D64DataSource::seek(unsigned int pos) {}
bool D64DataSource::openDirectory(const char* dirName) 
{
    _dirTrackBlock[0] = 18;
    _dirTrackBlock[1] = 1;
    _dirIndexInBuffer = 0;

    cbmReadBlock(_dirTrackBlock);
}
unsigned int D64DataSource::getNextFileBlock() 
{
    if (_fileTrackBlock[0] == 0)
    {
        return 0;
    }

    cbmReadBlock(_fileTrackBlock);

    _fileTrackBlock[0] = _cbmBuffer[0];
    _fileTrackBlock[1] = _cbmBuffer[1];

    if (_fileTrackBlock[0] == 0)
    {
        return _fileTrackBlock[1] - 1;
    }

    return BLOCK_SIZE - 2;
}


bool D64DataSource::isLastBlock() 
{
    if (_fileTrackBlock[0] == 0)
    {
        return true;
    }
    
    return false;
}

bool D64DataSource::getNextDirectoryEntry() 
{
    _currentFileEntry = cbmGetNextFileEntry();
    if (_currentFileEntry == NULL)
    {
        return false;
    }

    return true;
}

bool D64DataSource::isInitialized() {}

void D64DataSource::writeBufferToFile(unsigned int numBytes) {}
void D64DataSource::closeFile() {}
void D64DataSource::openCurrentDirectory() 
{
    _dirTrackBlock[0] = 18;
    _dirTrackBlock[1] = 1;
    _dirIndexInBuffer = 0;

    cbmReadBlock(_dirTrackBlock);

    _dirTrackBlock[0] = _cbmBuffer[0];
    _dirTrackBlock[1] = _cbmBuffer[1];
    _currentFileEntry = NULL;
}
unsigned char* D64DataSource::getFilename() 
{
    if (_currentFileEntry)
    {
        memset(_fileName, 0, 17);
        cbmCopyString(_fileName, _currentFileEntry->fileName);
        int len = strlen((char*)_fileName);
        if (_currentFileEntry->fileType == 0x81)
        {
            _fileName[len] = '.';
            _fileName[len+1] = 'S';
            _fileName[len+2] = 'E';
            _fileName[len+3] = 'Q';
        }
        else
        {
            _fileName[len] = '.';
            _fileName[len+1] = 'P';
            _fileName[len+2] = 'R';
            _fileName[len+3] = 'G';
        }

        //memcpy(_fileName, _currentFileEntry->fileName, 16);
        //_logger->printf("g %s\r\n", _fileName);
        return _fileName;
    }
}
unsigned char* D64DataSource::getBuffer() 
{
    return _cbmBuffer + 2;
}
unsigned int D64DataSource::writeBufferSize() 
{
    return BLOCK_SIZE - 2;
}

unsigned int D64DataSource::readBufferSize() 
{
    return BLOCK_SIZE - 2;
}

uint32_t D64DataSource::cbmBlockLocation(uint8_t* tb)
{
    uint8_t track = tb[0];
    uint8_t block = tb[1];
    return _cbmTrackLayout[track-1] + (block * BLOCK_SIZE);
}

uint8_t* D64DataSource::cbmReadBlock(uint8_t* tb)
{
    uint32_t loc = cbmBlockLocation(tb);
    
    // seek to the right place in datasource
    uint32_t actualPos = _fileDataSource->seek(loc);
    uint32_t offset = loc - actualPos;

    _fileDataSource->getNextFileBlock();
    uint8_t* buf = _fileDataSource->getBuffer();
    
    // point to the buffer containing this block
    _cbmBuffer = &buf[offset];
    return _cbmBuffer;
}

void D64DataSource::cbmPrintHeader(CBMDisk* disk)
{
    CBMHeader* header = &(disk->header);
    _logger->log("diskName: ");
    char tmp[8];
    for (int i = 0; i < 16; i++)
    {
        sprintf(tmp, "%c", header->diskName[i]);
        _logger->log(tmp);
    }
    _logger->log("\r\n");
}

int D64DataSource::cbmLoadHeader(CBMDisk* disk)
{
    uint8_t tb[]={18,0};
    uint8_t *buffer;

    buffer = cbmReadBlock(tb);
    if (buffer != NULL)
    {
        memcpy(&(disk->header), buffer, sizeof(CBMHeader));
    }

    return 0;
}

void D64DataSource::cbmMount(CBMDisk* disk, char* name)
{
    //cbmLoadHeader(disk);
    //cbmPrintHeader(disk);
}

void D64DataSource::cbmPrintFileEntry(CBMFile_Entry* entry)
{
    char tmp[32];
    _logger->log("fname: ");
    for (int i = 0; i < 16; i++)
    {
        sprintf(tmp, "%c", entry->fileName[i]);
        _logger->log(tmp);
    }
    sprintf(tmp, " \t\ttype: %X\r\ndataBlock %X %X\r\n", entry->fileType, entry->dataBlock[0], entry->dataBlock[1]);
    _logger->log(tmp);
    //printf("dataBlock %X %X\n", entry->dataBlock[0], entry->dataBlock[1]);
    //printf("\n");
}

CBMFile_Entry* D64DataSource::cbmGetNextFileEntry()
{
    int numEntriesInBlock = BLOCK_SIZE / sizeof(CBMFile_Entry);
    if (_dirIndexInBuffer >= numEntriesInBlock)
    {
        if (_dirTrackBlock[0] == 0)
        {
            return NULL;
        }
        // read next block
        cbmReadBlock(_dirTrackBlock);
        _dirTrackBlock[0] = _cbmBuffer[0];
        _dirTrackBlock[1] = _cbmBuffer[1];
        _dirIndexInBuffer = 0;
    }

    CBMFile_Entry* entriesInBlock = (CBMFile_Entry*)_cbmBuffer;
    CBMFile_Entry* entry = &entriesInBlock[_dirIndexInBuffer++];

    return entry;
}

uint8_t* D64DataSource::cbmCopyString(uint8_t* dest, const uint8_t* source)
{
    for (int c = 0; c < 17; c++)
    {
        if (source[c] == 0 || source[c] == 160 || source[c] == '.')
        {
            break;
        }

        dest[c] = source[c];
    }

    return dest;
}

uint8_t* D64DataSource::cbmD64StringCString(uint8_t* dest, const uint8_t* source)
{
    memset(dest, ' ', 17);
    dest[17] = 0;

    return cbmCopyString(dest, source);
}

CBMFile_Entry* D64DataSource::cbmSearch(CBMDisk* disk, uint8_t* searchNameA, uint8_t fileType)
{
    uint8_t fileName[18];
    uint8_t searchName[18];

    cbmD64StringCString(searchName, searchNameA);
    openCurrentDirectory();

    CBMFile_Entry* entry = cbmGetNextFileEntry();
    while (entry != NULL)
    {
        cbmPrintFileEntry(entry);
        cbmD64StringCString(fileName, entry->fileName);

        if (strcmp((const char*)searchName, (const char*)fileName) == 0)
        {
            return entry;
        }

        entry = cbmGetNextFileEntry();
    }

    return NULL;
}
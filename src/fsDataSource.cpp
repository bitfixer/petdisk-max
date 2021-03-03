#include "fsDataSource.h"
#include <string.h>

bool fsDataSource::init() 
{
    printf("fsDataSource init\n");
    return true;
}
void fsDataSource::openFileForWriting(unsigned char* fileName) {}
bool fsDataSource::openFileForReading(unsigned char* fileName) 
{
    printf("fsDataSource openFile %s\n", (char*)fileName);
    strcpy(_fileName, (const char*)fileName);
    printf("!\n");
    _fp = fopen(_fileName, "rb+");
    printf("@\n");

    return true;
}

uint32_t fsDataSource::seek(unsigned int pos) 
{
    uint32_t q_pos = (pos / readBufferSize()) * readBufferSize();
    printf("seek to %d, q %d\n", pos, q_pos);
    fseek(_fp, q_pos, SEEK_SET);
    return q_pos;
}
bool fsDataSource::openDirectory(const char* dirName) {}
unsigned int fsDataSource::getNextFileBlock() 
{
    printf("reading at %d\n", ftell(_fp));
    _bytesInBlock = (int)fread(_buffer, 1, readBufferSize(), _fp);

    return _bytesInBlock;
}
bool fsDataSource::isLastBlock() 
{
    return (_bytesInBlock < readBufferSize()); 
}
bool fsDataSource::getNextDirectoryEntry() {}
bool fsDataSource::isInitialized() {}

void fsDataSource::writeBufferToFile(unsigned int numBytes) 
{
    printf("fs write to file %d\n", numBytes);
    fwrite(_buffer, 1, writeBufferSize(), _fp);
}

void fsDataSource::updateBlock()
{
    printf("update block, writing at %d\n", ftell(_fp));
    fwrite(_buffer, 1, writeBufferSize(), _fp);
}

void fsDataSource::closeFile() 
{

}
void fsDataSource::openCurrentDirectory() {}
unsigned char* fsDataSource::getFilename() 
{
    return (unsigned char*)_fileName;
}
unsigned char* fsDataSource::getBuffer() 
{
    return _buffer;
}
unsigned int fsDataSource::writeBufferSize() 
{
    return 512;
}
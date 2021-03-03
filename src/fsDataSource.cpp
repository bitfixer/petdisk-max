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
    printf("seek to %d\n", pos);
    fseek(_fp, pos, SEEK_SET);
    return pos;
}
bool fsDataSource::openDirectory(const char* dirName) {}
unsigned int fsDataSource::getNextFileBlock() 
{
    //printf("read block.\n");
    _bytesInBlock = (int)fread(_buffer, 1, 512, _fp);

    return _bytesInBlock;
}
bool fsDataSource::isLastBlock() 
{
    return (_bytesInBlock < 512); 
}
bool fsDataSource::getNextDirectoryEntry() {}
bool fsDataSource::isInitialized() {}

void fsDataSource::writeBufferToFile(unsigned int numBytes) 
{
    /*
    for (int i = 0; i < 32; i++)
    {
        printf("%d: %X\n", i, _buffer[i]);
    }
    */

    fwrite(_buffer, 1, numBytes, _fp);
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
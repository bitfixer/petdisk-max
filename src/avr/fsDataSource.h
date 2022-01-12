#ifndef __fs_datasource_h__
#define __fs_datasource_h__

#include <stdio.h>
#include "DataSource.h"

class fsDataSource : public DataSource
{
public:
    fsDataSource() {}
    ~fsDataSource() {}

    bool init();
    void openFileForWriting(unsigned char* fileName);
    bool openFileForReading(unsigned char* fileName);
    uint32_t seek(uint32_t pos);
    bool openDirectory(const char* dirName);
    unsigned int getNextFileBlock();
    bool isLastBlock();
    bool getNextDirectoryEntry();
    bool isInitialized();

    void writeBufferToFile(unsigned int numBytes);
    void updateBlock();
    void closeFile();
    void openCurrentDirectory();
    unsigned char* getFilename();
    unsigned char* getBuffer();
    unsigned int writeBufferSize();

private:
	char _fileName[64];
	unsigned char _buffer[512];
	FILE* _fp;
	int _bytesInBlock;
};

#endif
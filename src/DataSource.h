#ifndef __datasource_h__
#define __datasource_h__

class DataSource
{
public:
    DataSource() {}
    ~DataSource() {}

    virtual bool init() = 0;
    virtual void openFileForWriting(unsigned char* fileName) = 0;
    virtual bool openFileForReading(unsigned char* fileName) = 0;
    virtual bool openDirectory(const char* dirName) = 0;
    virtual unsigned int getNextFileBlock() = 0;
    virtual bool isLastBlock() = 0;
    virtual bool getNextDirectoryEntry() = 0;
    virtual bool isInitialized() = 0;

    virtual void writeBufferToFile(unsigned int numBytes) = 0;
    virtual void closeFile() = 0;
    virtual void openCurrentDirectory() = 0;
    bool isHidden() { return false; }
    bool isVolumeId() { return false; }
    bool isDirectory() { return false; }
    virtual unsigned char* getFilename() = 0;
    virtual unsigned char* getBuffer() = 0;
    virtual unsigned int writeBufferSize() = 0;
};

#endif
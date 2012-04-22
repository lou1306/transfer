/* 
 * File:   SplitFile.h
 */

#ifndef SPLITFILE_H
#define	SPLITFILE_H
#include <sstream>

using namespace std;

class SplitFile {
public:
    SplitFile(string);
    int getChunks() const;
    void setChunks(int);
    unsigned long getChunkSize(int) const;
    unsigned long getSize() const;
    string getFileName() const;
    string getChunk(int,unsigned long,unsigned long);
private:
    string fileName;
    int chunks;
    unsigned long size;
};

#endif	/* SPLITFILE_H */


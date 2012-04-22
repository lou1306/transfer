/* 
 * File:   SplitFile.cpp
 */

#include "SplitFile.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <cstdlib>


SplitFile::SplitFile(string file) {
    this->chunks=1;
    this->fileName = file;
    ofstream File;
    File.open(fileName.c_str(), fstream::binary|fstream::in);
    filebuf *buf;
    buf = File.rdbuf();

    // Dimensione del file
    this->size = buf->pubseekoff(0, ios::end, ios::in);
    File.close();
}

int SplitFile::getChunks() const{
    return this->chunks;
}
unsigned long SplitFile::getChunkSize(int n) const{
    if (n>chunks-1)
        throw("Errore");
    int out = size/chunks;
    if (n==chunks-1)
        out+=size-chunks*out;
    return out;
}

unsigned long SplitFile::getSize() const{
    return size;
}

/* Restituisce parte del chunk: l'utente richiede tempSize byte a partire
 * da "offset", ma può darsi che il metodo si fermi prima (se siamo vicini
 * alla fine del chunk). L'utente deve controllare la size() della stringa
 * restituita. */
string SplitFile::getChunk(int n, unsigned long offset, unsigned long tempSize) {
    if (n>chunks-1)
        throw("Errore");
    if(offset+tempSize>getChunkSize(n)){
        tempSize=getChunkSize(n)-offset;
    }
    char * temp = new char[tempSize];
    string out;
    //Apro il file
    ofstream File(fileName.c_str(), fstream::binary|fstream::in);
    filebuf *buf;
    buf = File.rdbuf();
    buf->pubseekpos((n*getChunkSize(0))+offset, ios::in);
    buf->sgetn(temp, tempSize);
    out.assign(temp, tempSize);
    File.close();
    delete [] temp;
    return out;
}

void SplitFile::setChunks(int c){
    if (c>0)
        this->chunks = c;
    else this->chunks = 1;
}

string SplitFile::getFileName() const{
    return this->fileName;
}

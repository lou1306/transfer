#include <cstdlib>
#include <iostream>
#include <sstream>
#include <fstream>
#include <pthread.h>
#include <libgen.h>
#include "../transfer_server/Socket.h"
#include "SplitFile.h"
#include "../transfer_server/md5.h"

using namespace std;

struct sendThreadInfo {
    int num;
    int * conn;
    string serverIP;
    string port;
    SplitFile * f;
    pthread_mutex_t * mutex;
    pthread_cond_t  * conn_ok;
};

void* sendThread(void *param){
   stringstream s;
   sendThreadInfo *i = (sendThreadInfo *)param;
   SplitFile * f = i->f;
   int n = i->num;
   int * conn = i->conn;
   Socket threadSocket;
   unsigned long chunkSize= f->getChunkSize(n);
   int chunks = f->getChunks();
   try{
       threadSocket.connect(i->serverIP, i->port);
       cout << "Server: " << threadSocket.receiveString()<<endl;
   }catch(string e){
       // In caso di errore 
       cout << e << endl;
       pthread_mutex_lock(i->mutex);
       pthread_cond_broadcast(i->conn_ok);
       pthread_mutex_unlock(i->mutex);
       threadSocket.close();
       pthread_exit(NULL);
   }
   
   // Aspetto che tutte le connessioni siano stabilite.

   pthread_mutex_lock(i->mutex);
        (*conn)++;
        if(*conn == chunks)
            pthread_cond_broadcast(i->conn_ok);
        else{
            pthread_cond_wait(i->conn_ok, i->mutex);
            if(*conn < chunks){
                pthread_mutex_unlock(i->mutex);
                threadSocket.close();
                pthread_exit(NULL);
            }
        }
   pthread_mutex_unlock(i->mutex);
   
   //Invio del chunk
   unsigned long tot=0;
   string a="";
   while(tot<chunkSize){
        a=f->getChunk(n,tot,2048);
        try{
        threadSocket.send(a);
        tot+=a.size();
        }
        catch(string e){
            tot = chunkSize;
            cout << e << endl;
        }
   }
   threadSocket.close();
   pthread_exit(NULL);
}

int main(int argc, char** argv) {
    if (argc != 3){
        cout << "Utilizzo: " << argv[0] << " IP_SERVER NOME_FILE"<<endl;
        exit(1);
    }
    FILE* f=fopen(argv[2],"r");
    if(!f){
        cout << "Il file non esiste."<<endl;
        exit(1);
    }
    else fclose(f);
    
    stringstream r;
    r << argv[2];
    SplitFile s(r.str());
    r.str("");
    r << argv[1];
    string serverIP = r.str();
    
    Socket s1;
    try {
        s1.connect(serverIP, "13690", 10000);
    } catch (string e) {
        cout << e << endl;
        exit(1);
    }
    r.str("");
        r << ::basename((char*) s.getFileName().c_str())
            <<"\n"<< s.getSize()
            <<"\n"<< ::MD5File((char*) s.getFileName().c_str())
            <<"\n\r\n\r\n";
    try {
        s1.send(r.str());
        r.str("");
        r << s1.receiveString();
    } catch (string e) {
        cout<< e << endl;
        exit(1);
    }

    string line;    
    getline(r,line);
    int chunks = atoi(line.c_str());
    string ports[chunks];
    for(int i=0; i<chunks; ++i){
        getline(r,ports[i]);
    }
    s.setChunks(chunks);
      
    pthread_t threads[chunks];
    pthread_mutex_t m;
    pthread_cond_t conn_ok;
    pthread_mutex_init(&m, NULL);
    pthread_cond_init(&conn_ok, NULL);
    int * conn=new int;
    *conn=0;
    sendThreadInfo * i;
    i = new sendThreadInfo[chunks];
    for(int t=0; t<chunks; t++) {
        i[t].num=t;
        i[t].serverIP=serverIP;
        i[t].port = ports[t];
        i[t].f = &s;
        i[t].conn = conn;
        i[t].mutex = &m;
        i[t].conn_ok = &conn_ok;
        int rc = pthread_create(&threads[t], NULL, sendThread, (void *) &i[t]);
    }
    for(int t=0; t<chunks; ++t)
        pthread_join(threads[t], NULL);
    
    delete [] i;
    delete conn;
    r.str("");
    try{
        r << s1.receiveString();
        if (r.str().size()>0)
            cout << "Server: "<< r.str() << endl;
        else
            cout << "Errore di connessione." << endl;
        s1.close();
    } catch (string e) {
        cout << e << endl;
        exit(1);
    }
    return 0;
}

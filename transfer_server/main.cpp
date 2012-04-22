/* 
 * File:   main.cpp
 * Author: luca
 */

#include <cstdlib>
#include <iostream>
#include "Socket.h"
#include "md5.h"
#include <signal.h>
#include <sstream>
#include <fstream>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>

#define PORT 13690

using namespace std;

string setFilename(string in) {
    FILE* fp = fopen(in.c_str(), "r");
    if (fp) {
        // Il file esiste già
        unsigned int i=0;
        stringstream s;
        do {
            fclose(fp);
            // Aggiunge un prefisso numerico
            // fino ad ottenere un nome "valido"
            s.str("");
            s << i++ << "-" << in;
        } while (fp = fopen(s.str().c_str(), "r"));
        in = s.str();
    }
    // Il file viene creato
    fp = fopen(in.c_str(), "w+");
    fclose(fp);
    return in;
}

struct connThreadInfo {
    Socket * accept; // socket creato da accept() nel main()
    int chunks;      // numero dei chunk
};

struct chunkThreadInfo {
    Socket * socket;                //socket da controllare
    int num;                        //numero del thread (0...chunks-1)
    int chunks;                     //numero dei chunk
    const string * filename;
    pthread_mutex_t * Mutex;        //Per gestire le scritture su count
    int * count;                    //Contatore delle connessioni stabilite
    pthread_cond_t * ready;         //Per sincronizzare i thread
};

void* chunkThread(void *param) {
    //Recupero le informazioni passate da conn()
    chunkThreadInfo *i = (chunkThreadInfo *) param;
    Socket * threadSocket = (i->socket);
    int * count = i->count;
    
    threadSocket->listen(5);
    pthread_mutex_lock(i->Mutex);
    ++(*count);
    if (*count == i->chunks) {
        pthread_cond_broadcast(i->ready);
        *count = 0;
    }
    else
        pthread_cond_wait(i->ready, i->Mutex);
    pthread_mutex_unlock(i->Mutex);
    Socket accept(false);
    try {
        // Accetto una connessione entro 5 secondi.
        accept = threadSocket->Accept(5000);
    }    catch (string e) {
        // Se c'è un errore o un timeout, tutti i thread
        // devono terminare.
        pthread_mutex_lock(i->Mutex);
        pthread_cond_broadcast(i->ready);
        pthread_mutex_unlock(i->Mutex);
        cout << e << endl;
        threadSocket->close();
        pthread_exit(NULL);
    }

    // Incremento il contatore delle connessioni.
    pthread_mutex_lock(i->Mutex);
    ++(*count);
    if (*count == i->chunks) {
        // Tutte le connessioni sono state stabilite:
        // possiamo proseguire.
        pthread_cond_broadcast(i->ready);
    } else{
        // Aspetto che qualcuno sblocchi i->ready
        pthread_cond_wait(i->ready, i->Mutex);
        if (*count < i->chunks) {
            // C'è stato un errore: chiudo il socket aperto con accept()
            // e termino il thread
            threadSocket->close();
            pthread_mutex_unlock(i->Mutex);
            pthread_exit(NULL);
        }
    }
    pthread_mutex_unlock(i->Mutex);

    stringstream * st = new stringstream;
    *st << i->num << ": pronto.\r\n\r\n";
    try {
        accept.send(st->str());
    }    catch (string e) {
        cout<<e<<endl;
        accept.close();
        threadSocket->close();
        pthread_exit(NULL);
    }
        st->str("");

    //Ricezione del chunk.
    *st << *(i->filename) << "." << i->num;
    ofstream *out= new ofstream(st->str().c_str(), fstream::out | fstream::binary | fstream::trunc);
    do {
        st->str("");
        try{
            *st << accept.receiveBinary(2048);
            *out << st->str();
        }
        catch (string e){
            cout << e << endl;
            st->str("");
        }
    } while (st->str().size() > 0);
    out->close();
    accept.close();
    threadSocket->close();
    delete st;
    delete out;
    pthread_exit(NULL);
}
void* connThread(void *param) {
    connThreadInfo *i = (connThreadInfo *) param;
    Socket * c = i->accept;
    int chunks = i->chunks;
    clock_t * start = new clock_t;
    *start = clock();
    stringstream * stream = new stringstream;
    
    //Ricezione informazioni sul file
    try {
        *stream << c->receiveString();
    } catch (string e) {
        cout << e << endl;
    }
    string * filename=new string;
    string * s_size=new string;
    string * md5=new string;
    getline(*stream, *filename);
    getline(*stream, *s_size);
    unsigned long * size = new unsigned long;
    *size = atol(s_size->c_str());
    getline(*stream, *md5);
    if (*filename != "" && *s_size != "" && md5->size()==32){
        *filename = setFilename(*filename);
        stream->str("");
        cout << "#### File in arrivo ####" << endl
                << "# nome: " << *filename << endl
                << "# " << *size << " B" << endl
                << "# md5: " << *md5 << endl
                << "########################" << endl;
        pthread_t * threads= new pthread_t[chunks];
        chunkThreadInfo * info = new chunkThreadInfo[chunks];
        pthread_mutex_t m;
        pthread_mutex_init(&m, NULL);
        pthread_cond_t ready;
        pthread_cond_init(&ready, NULL);
        Socket * sockets = new Socket[chunks];
        int * count = new int;
        *count = 0;
        //    Assegnazione delle porte.     

        for (int i = 0; i < chunks; ++i) {
            try {
                sockets[i].bind(0);
            } catch (string e) {
                cout << e << endl;
                exit(1);
            }
        }
        for (int i = 0; i < chunks; ++i) {
            // Creazione dei thread per la ricezione dei chunk
            // Assegnazione casuale della porta
            info[i].num = i;
            info[i].chunks = chunks;
            info[i].count = count;
            info[i].filename = filename;
            info[i].Mutex = &m;
            info[i].ready = &ready;
            info[i].socket = &(sockets[i]);
            pthread_create(&threads[i], NULL, chunkThread, (void *) &info[i]);
        }

        // Invio le informazioni relative ai thread: 
        // numero di chunk in cui il client dovrà dividere il file
        // elenco delle porte cui inviare i singoli blocchi
        pthread_mutex_lock(&m);
        pthread_cond_wait(&ready, &m);
        pthread_mutex_unlock(&m);

        *stream << chunks << "\n";
        for (int i = 0; i < chunks; i++) {
            *stream << sockets[i].getPort() << "\n";
        }
        *stream << "\r\n\r\n";
        try {
            c->send(stream->str());
        } catch (string e) {
            cout << e << endl;
        }
        stream->str("");

        // Aspetto che i thread siano pronti.
        pthread_mutex_lock(&m);
        pthread_cond_wait(&ready, &m);
        pthread_mutex_unlock(&m);
        // Se il client non ha stabilito il numero prefissato di connessioni,
        // stampo un messaggio di errore.
        if (*count < chunks) {
            cout << "Errore di connessione" << endl;
        }
        // Aspetto la chiusura dei thread; libero la memoria usata
        // per gestirli.
        for (int i = 0; i < chunks; ++i)
            pthread_join(threads[i], NULL);
        pthread_mutex_destroy(&m);
        pthread_cond_destroy(&ready);
        delete [] info;
        delete [] sockets;
        delete [] threads;

        // Ricostruzione del file (se la connessione è andata a buon fine)
        if (*count == chunks) {
            double time;
            double speed;
            time = (double) (clock() - *start) / CLOCKS_PER_SEC;
            speed = (double) (*size / 1024) / time;
            ofstream outFile(filename->c_str(), fstream::out | fstream::binary | fstream::trunc);
            ifstream threadFile;
            for (int i = 0; i < chunks; ++i) {
                *stream << *filename << "." << i;
                threadFile.open(stream->str().c_str(), fstream::binary);
                outFile << threadFile.rdbuf();
                threadFile.close();
                ::remove(stream->str().c_str());
                stream->str("");
            }
            outFile.close();
            // Controllo dell'hash MD5
            *stream << ::MD5File((char*) filename->c_str());
            cout << "\nmd5 locale: " << stream->str() << endl;
            if (stream->str() != *md5) {
                stream->str("");
                *stream << *filename << ": errore nella ricezione.";
                ::remove(filename->c_str());
            } else {
                stream->str("");
                *stream << *filename << ": file ricevuto.";
            }
            cout << stream->str() << " (" << time << " s, " << speed << " kB/s)" << endl;
            *stream << "\r\n\r\n";
            try {
                c->send(stream->str());
            } catch (string e) {
                cout << e << endl;
            }
        }
    }
    else{
        c->send("Messaggio malformato\n\r\n\r\n");
    }
    c->close();
    delete filename;
    delete s_size;
    delete size;
    delete md5;
//    delete count;
    delete c;
    delete stream;
    delete start;
    delete (connThreadInfo *) i;
    pthread_exit(NULL);
}

int main(int argc, char** argv) {
    if (argc != 2){
        cout << "utilizzo:\n" << argv[0] << " NUMERO_BLOCCHI"<<endl;
        exit(1);
    }
    const int chunks = atoi(argv[1]);
    
    Socket s;
    try {
        s.bind(PORT);
    } catch (string e) {
        cout << e << endl;
        exit(1);
    }
    
    while (1) {
        cout << "In ascolto sulla porta " << PORT << endl;
        Socket * acc = new Socket(-1);
        try {
            s.listen(10);
            *acc = s.Accept();
            connThreadInfo *cI = new connThreadInfo;
            cI->accept = acc;
            cI->chunks = chunks;
            pthread_t conn;
            pthread_attr_t attr;
            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
            pthread_create(&conn, &attr, connThread, (void *) cI);
            pthread_attr_destroy(&attr);
        }       
        catch (string e) {
            cout << e << endl;
        }
    }
    try {
        s.close();
    } catch (string e) {
        cout << e << endl;
        exit(1);
    }
    cout << "Socket chiuso." << endl;
    return 0;
}
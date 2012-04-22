/* 
 * File:   Socket.h
 */

#include <string>
#include <sys/poll.h>
#ifndef SOCKET_H
#define SOCKET_H
using namespace std;

class Socket {
public:
    Socket();
    Socket(int);
    int checkPoll(int);
    int checkPollIn(int);
    int checkPollOut(int);
    int getId();
    int getPort();
    void bind(const int);
    void bind(const string);
    void close();
    void connect(string, string);
    void connect(string, int);
    void connect(string, string, int);
    void send(string);
    void listen(int);
    Socket Accept();
    Socket Accept(int);
    string receive();
    string receiveString();
    string receiveBinary(unsigned long);
private:
    int id;             //id del socket
    struct pollfd poll;
    void settings();    //configurazione del socket
    void Eccezione(string, int);
};

#endif	/* SOCKET_H */


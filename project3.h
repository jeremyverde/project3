#ifndef PROJECT3_PROJECT3_H
#define PROJECT3_PROJECT3_H

#define MAXDATASIZE 1024 // max number of bytes for buffers
#define PORT "9001"     // the tcp port routers will be connecting to
#define BACKLOG 100     // how many pending connections queue will hold

#include <fstream>
#include <sys/time.h>
#include <iostream>
#include <zconf.h>
#include <wait.h>
#include <vector>
#include <sstream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cassert>
#include <chrono>
#include <fcntl.h>


using namespace std;

class router {
public:
    int startRouter(ofstream &o);

    int get_in_port(struct sockaddr *sa);

    void udpListen(ofstream &o, string &s);

    void getTime(char *stamp, int len);

    string fillPacket(string &c, string &id);

    void writeHeader(ofstream &o);

    void GetPrimaryIp(char *buffer, size_t buflen);

    bool debug = true;
    string myID;
};

class manager {
public:
    struct link {
        int sourceID = 0;
        int destID = 0;
        int cost = 0;
    };

    struct msg {
        int sourceID = 0;
        int destID = 0;
    };

    struct node {
        int id = 0;
        int port = 0;
        int fd = 0;
    };

    static int usage();

    static string fillPacket(string &c, string &id);

    static node *getRouter(int i, vector<node> &Rs);

    //static int getSome(int i, fd_set &m,ofstream &o);

    //static int giveSome(string type, int i,fd_set &m,ofstream &o);

    static void getTime(char *tB, int len);

    static void writeHeader(ofstream &o);

    int manage(ofstream &ostr, int i);

    vector<node> routers;
    const char *killFile = "kill.txt";
    int index{};
};

#endif //PROJECT3_PROJECT3_H

#ifndef PROJECT3_PROJECT3_H
#define PROJECT3_PROJECT3_H

#define MAXDATASIZE 1024 // max number of bytes for buffers
#define PORT "9001"     // the tcp port routers will be connecting to
#define BACKLOG 100     // how many pending connections queue will hold

#include <fstream>
#include <sys/time.h>
#include <iostream>
#include <zconf.h>
#include <utility>
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

class manager {
public:
    manager();

    struct link {
        int sourceID = 0;
        int destID = 0;
        int cost = 0;
        int dPort = 0;
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

    node *getRouter(int i);

    //static void *get_in_addr(struct sockaddr *sa);

    //static int getSome(int i, fd_set &m,ofstream &o);

    //static int giveSome(string type, int i,fd_set &m,ofstream &o);

    static void getTime(char *tB, int len);

    string getLinks(int id, int i);

    static void writeHeader(ofstream &o);

    int manage(ofstream &ostr);

    // structure to hold links
    vector<manager::link> links;
    // structure to hold messages
    vector<manager::msg> mess;
    vector<node> routers;
    const char *killFile = "kill.txt";
    int index{};
};

class router {
public:
    router();

    int startRouter(ofstream &o);

    //int get_in_port(struct sockaddr *sa);

    void udpListen(ofstream &o, string &s);

    string fillPacket(string &c, string &id);

    void writeHeader(ofstream &o);

    void parseLinks(string s);

    void GetPrimaryIp(char *buffer, size_t buflen);

    bool debug = true;
    // structure to hold links
    vector<manager::link> links;
    string myID;
    string adjSend;
    unsigned long nLinks;

};

#endif //PROJECT3_PROJECT3_H

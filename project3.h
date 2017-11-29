#ifndef PROJECT3_PROJECT3_H
#define PROJECT3_PROJECT3_H

#include <fstream>
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


using namespace std;

class router {
public:
    int startRouter(ofstream &o, const char *port);

    void GetPrimaryIp(char *buffer, size_t buflen);

    void *get_in_addr(struct sockaddr *sa);

#define MAXDATASIZE 100 // max number of bytes we can get at once
    bool debug = true;
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

    static int usage();

    static int manage(ofstream &ostr);

    const char *killFile = "kill.txt";

#define PORT "9001"  // the port users will be connecting to
#define BACKLOG 10     // how many pending connections queue will hold
};

#endif //PROJECT3_PROJECT3_H

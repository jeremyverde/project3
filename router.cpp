#include "project3.h"

using namespace std;

bool debug = true;

// get sockaddr, IPv4 or IPv6:
void *router::get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in *) sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *) sa)->sin6_addr);
}


// based on example at: https://stackoverflow.com/questions/212528/get-the-ip-address-of-the-machine
void router::GetPrimaryIp(char *buffer, size_t buflen) {
    assert(buflen >= 16);

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    assert(sock != -1);

    const char *kGoogleDnsIp = "8.8.8.8";
    uint16_t kDnsPort = 53;
    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = inet_addr(kGoogleDnsIp);
    serv.sin_port = htons(kDnsPort);

    int err = connect(sock, (const sockaddr *) &serv, sizeof(serv));
    assert(err != -1);

    sockaddr_in name;
    socklen_t namelen = sizeof(name);
    err = getsockname(sock, (sockaddr *) &name, &namelen);
    assert(err != -1);

    const char *p = inet_ntop(AF_INET, &name.sin_addr, buffer, buflen);
    assert(p);

    close(sock);
}

int router::startRouter(ofstream &ostr, const char *port) {
    pid_t mypid = getpid();
    //cout << "writing pid: " << mypid << " to kill file." << endl;
    ostr << mypid << endl;

    // code based largely off of beej's guide example program, see README
    int sok = 0;     // stone socket descriptor
    //struct sockaddr_storage remoteaddr{}; // client address
    char buf[MAXDATASIZE];
    memset(buf, 0, sizeof(buf));
    ssize_t nbytes;
    string input;
    int yes = 1;        // for setsockopt() SO_REUSEADDR, below
    int status;
    struct addrinfo hints{}, *ai;// will point to the results
    bool empty = true;
    bool sendit = false;
    GetPrimaryIp(buf, sizeof(buf));
    const char *IP = buf;

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me
    cout << "[Router] connecting to IP: " << buf << " Port: " << PORT << endl;
    if ((status = getaddrinfo(IP, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }

    sok = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (sok < 0) {
        // lose the pesky "address already in use" error message
        setsockopt(sok, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    }

    if ((connect(sok, ai->ai_addr, ai->ai_addrlen)) < 0) {
        cerr << "connection failed" << endl;
        return -1;
    } else {
        cout << "[Router] Connected." << endl;
    }

    freeaddrinfo(ai);

    while (true) {
        if (sendit) {
            memset(&buf, 0, sizeof(buf));
            if (debug) {
                cout << "[Router] sending: PORT" << endl;
            }
            if (send(sok, "PORT", 4, 0) == -1) {
                perror("send failed");
                exit(6);
            }
            sendit = false;
        } else {
            sendit = true;
            memset(buf, 0, sizeof(buf));
            if ((nbytes = recv(sok, buf, MAXDATASIZE, 0)) <= 0) {
                // got error or connection closed by server
                if (nbytes == 0) {
                    // connection closed
                    if (debug)
                        printf("socket disconnected\n");
                    return 0;
                } else {
                    perror("recv");
                    exit(6);
                }
            } else {
                if (debug) {
                    cout << "[Router] recv: " << buf << endl;
                }
            }
        }
    }
}
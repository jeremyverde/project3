#include "project3.h"

using namespace std;

void router::getTime(char *stamp, int len) {
    memset(stamp, 0, sizeof(stamp));
    char timeBuf[len];
    string tString;
    struct timeval tVal{};
    gettimeofday(&tVal, nullptr);
    time_t now = tVal.tv_sec;
    if (strftime(timeBuf, sizeof(timeBuf), "[%F %X.", localtime(&now))) {
        //cout << timeBuf << endl;
    } else {
        cerr << "Time's broken" << endl;
    }
    tString = timeBuf;
    tString.append(to_string(tVal.tv_usec) + "]");
    for (int i = 0; i < len; i++) {
        stamp[i] = tString[i];
    }
}

// get sockaddr, IPv4 or IPv6:
void *router::get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in *) sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *) sa)->sin6_addr);
}

// get sockaddr, IPv4 or IPv6:
int router::get_in_port(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return ntohs(((struct sockaddr_in *) sa)->sin_port);
    }

    return ntohs(((struct sockaddr_in6 *) sa)->sin6_port);
}

void router::udpListen(ofstream &o, string &portnum) {
    int sockfd = 0;
    struct addrinfo hints{}, *servinfo, *p;
    int rv;
    ssize_t numbytes;
    struct sockaddr_storage their_addr{};
    char buf[MAXDATASIZE];
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];
    GetPrimaryIp(buf, sizeof(buf));
    string port;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    //hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(buf, to_string(getpid()).c_str(), &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }
    // loop through all the results and bind to the first we can
    for (p = servinfo; p != nullptr; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }
        int pN = get_in_port((struct sockaddr *) &servinfo);
        port = to_string(pN);
        cout << "port number: " << port << endl;
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("listener: bind");
            continue;
        }
        portnum = port;
        break;
    }

    if (p == nullptr) {
        fprintf(stderr, "listener: failed to bind socket\n");
        exit(2);
    }

    freeaddrinfo(servinfo);

    addr_len = sizeof their_addr;
    /*if ((numbytes = recvfrom(sockfd, buf, MAXDATASIZE - 1, 0,
                             (struct sockaddr *) &their_addr, &addr_len)) == -1) {
        perror("recvfrom");
        exit(1);
    }

    printf("listener: got packet from %s\n",
           inet_ntop(their_addr.ss_family,
                     get_in_addr((struct sockaddr *) &their_addr),
                     s, sizeof s));
    printf("listener: packet is %zi bytes long\n", numbytes);
    buf[numbytes] = '\0';
    printf("listener: packet contains \"%s\"\n", buf);
*/
    close(sockfd);
}

// based on example at: https://stackoverflow.com/questions/212528/get-the-ip-address-of-the-machine
void router::GetPrimaryIp(char *buffer, size_t buflen) {
    assert(buflen >= 16);

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    assert(sock != -1);

    const char *kGoogleDnsIp = "8.8.8.8";
    uint16_t kDnsPort = 53;
    struct sockaddr_in serv{};
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = inet_addr(kGoogleDnsIp);
    serv.sin_port = htons(kDnsPort);

    int err = connect(sock, (const sockaddr *) &serv, sizeof(serv));
    assert(err != -1);

    sockaddr_in name{};
    socklen_t namelen = sizeof(name);
    err = getsockname(sock, (sockaddr *) &name, &namelen);
    assert(err != -1);

    const char *p = inet_ntop(AF_INET, &name.sin_addr, buffer, static_cast<socklen_t>(buflen));
    assert(p);

    close(sock);
}

void router::writeHeader(ofstream &o) {
    char stamp[100];
    getTime(stamp, sizeof(stamp));
    o << "Router: " << getpid() << " Log File" << endl;
    o << "Log File Created: " << stamp << endl;
}

int router::startRouter(ofstream &ostr) {
    int mypid = getpid();
    string pid = to_string(mypid);
    string file = "router";
    file.append(pid + ".out");
    ofstream out(file);
    writeHeader(out);
    char stamp[100];
    getTime(stamp, sizeof(stamp));
    out << stamp << "[Router: " << mypid << "] writing pid to kill file." << endl;
    ostr << mypid << endl;

    // code based largely off of beej's guide example program, see README
    int sok = 0;     // stone socket descriptor
    //struct sockaddr_storage remoteaddr{}; // client address
    char buf[MAXDATASIZE];
    memset(&buf, 0, sizeof(buf));
    ssize_t nbytes;
    string input;
    int yes = 1;        // for setsockopt() SO_REUSEADDR, below
    int status;
    struct addrinfo hints{}, *ai;// will point to the results
    bool ready = false;
    bool sendit = true;
    GetPrimaryIp(buf, sizeof(buf));
    const char *IP = buf;
    string portnum;
    string packet;

    // start up UDP socket

    udpListen(out, portnum);


    sleep(1);
    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me
    getTime(stamp, sizeof(stamp));
    out << stamp << "[Router: " << mypid << "] connecting to IP: " << buf << " Port: " << PORT << endl;
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
        cerr << "[Router] connection failed" << endl;
        return -1;
    } else {
        getTime(stamp, sizeof(stamp));
        out << stamp << "[Router: " << mypid << "] Connected." << endl;
    }

    freeaddrinfo(ai);

    while (true) {
        if (ready) {
            exit(0);
        } else {
            if (sendit) {
                memset(&buf, 0, sizeof(buf));
                for (int k = 0; k < portnum.length(); k++) {
                    buf[k] = portnum[k];
                }
                if (debug) {
                    getTime(stamp, sizeof(stamp));
                    out << stamp << "[Router: " << mypid << "] sending: " << buf << endl;
                }
                if (send(sok, buf, sizeof(buf), 0) == -1) {
                    perror("[Router: ] send failed");
                    exit(6);
                }
                sendit = false;
            } else {
                sendit = true;
                ready = false;
                memset(buf, 0, sizeof(buf));
                if ((nbytes = recv(sok, buf, MAXDATASIZE, 0)) <= 0) {
                    // got error or connection closed by server
                    if (nbytes == 0) {
                        // connection closed
                        if (debug) {
                            getTime(stamp, sizeof(stamp));
                            out << stamp << "[Router: " << mypid << "] socket disconnected" << endl;
                        }
                        return 0;
                    } else {
                        perror("recv");
                        exit(6);
                    }
                } else {
                    getTime(stamp, sizeof(stamp));
                    out << stamp << "[Router: " << mypid << "] recv: " << buf << endl;
                    packet = buf;
                    if (packet == "ready") {
                        ready = true;
                    }
                }
            }
        }
    }
}

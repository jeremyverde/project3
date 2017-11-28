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
    int sockfd = 0;
    ssize_t nbytes;
    unsigned char buf[MAXDATASIZE];
    struct addrinfo hints{}, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    bool sendit = false;
    string input;
    const char *in;
    uint16_t mLength;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    char buffer[128];
    size_t buflen = 128;
    GetPrimaryIp(buffer, buflen);
    bool doneskies = false;

    if ((rv = getaddrinfo(buffer, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    printf("router: connecting to manager\n");
    for (p = servinfo; p != nullptr; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("socket");
            continue;
        }
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("connect");
            continue;
        }
        break;
    }

    if (p == nullptr) {
        fprintf(stderr, "failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr(p->ai_addr),
              s, sizeof s);
    printf("Connected to Manager at: %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure

    while (!doneskies) {
        uint16_t vCheck = 0;
        uint16_t lenCheck = 0;
        if (sendit) {
            printf("You: ");
            //getline(cin,input);
            input = "hi";
            mLength = static_cast<uint16_t>(input.length());
            lenCheck = htons(mLength);
            if (mLength > 140) {
                printf("Input too long\n");
                sendit = true;
            } else {
                in = input.c_str();
                buf[0] = 1;
                buf[1] = 201;
                buf[3] = 140;
                if (debug) printf("size of in: %d\n", static_cast<int>(strlen(in)));
                for (unsigned int i = 0; i < strlen(in); i++) {
                    buf[i + 4] = (unsigned char) in[i];
                }
                if (send(sockfd, buf, mLength + 4, 0) == -1) {
                    perror("send");
                    exit(6);
                }
                memset(buf, 0, sizeof(buf));
                sendit = false;
            }
            doneskies = true;
        } else {
            // first pull out version number
            if ((nbytes = recv(sockfd, &vCheck, 2, 0)) <= 0) {
                // got error or connection closed by server
                if (nbytes == 0) {
                    // connection closed
                    printf("socket disconnected\n");
                    exit(5);
                } else {
                    perror("recv");
                    exit(6);
                }
            } else {
                vCheck = ntohs(vCheck);
                if (debug)
                    printf("version: %d\n", vCheck);
                if (vCheck != 457) printf("Incorrect version Number.\n");
            }
            if ((nbytes = recv(sockfd, &lenCheck, 2, 0)) <= 0) {
                // got error or connection closed by server
                if (nbytes == 0) {
                    // connection closed
                    printf("socket disconnected\n");
                    exit(5);
                } else {
                    perror("recv");
                    exit(6);
                }
            } else {
                // set the message length
                lenCheck = ntohs(lenCheck);
            }
            if ((nbytes = recv(sockfd, buf, sizeof(buf), 0)) <= 0) {
                // got error or connection closed by server
                if (nbytes == 0) {
                    // connection closed
                    printf("socket disconnected\n");
                    exit(5);
                } else {
                    perror("recv");
                    exit(6);
                }
            } else {
                // print the message
                printf("Friend: '%s'\n", buf);
                memset(buf, 0, sizeof(buf));
                sendit = true;
            }
        }
    }
}
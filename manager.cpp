#include "project3.h"


using namespace std;

// Print the correct usage in case of user syntax error.
int manager::usage() {
    cout << "Usage: to start router demo, run \"./manager <file name>\"" << endl;
    cout << "See readme for accepted file format" << endl;
    return -1;
}

// based on example at: https://stackoverflow.com/questions/212528/get-the-ip-address-of-the-machine
void GetPrimaryIp(char *buffer, size_t buflen) {
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

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in *) sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *) sa)->sin6_addr);
}

int manager::manage(ofstream &ostr) {
    pid_t mypid = getpid();
    ostr << mypid << endl;
    cout << mypid << ": manager wrote pid to killFile" << endl;
    fd_set master{};    // master file descriptor list
    bool sendit = true; // keeps track of whether or not it's my turn to send
    int listener = 0;     // listening socket descriptor
    int newfd = 0;        // newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr{}; // client address
    socklen_t addrlen;
    unsigned char buf[MAXDATASIZE];
    //struct packet packet1{};
    string input;
    const char *in;
    uint16_t mLength;
    ssize_t nbytes;
    char buffer[128];
    size_t buflen = 128;
    GetPrimaryIp(buffer, buflen);
    ofstream outMan("manager.out");

    char remoteIP[INET6_ADDRSTRLEN];

    int yes = 1;        // for setsockopt() SO_REUSEADDR, below
    int rv;

    struct addrinfo hints{}, *ai, *p;

    outMan << "Manager: Binding Socket\n";
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(nullptr, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }

    for (p = ai; p != nullptr; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) {
            continue;
        }

        // lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    // if we got here, it means we didn't get bound
    if (p == nullptr) {
        fprintf(stderr, "selectserver: failed to bind\n");
        exit(2);
    }

    freeaddrinfo(ai); // all done with this

    // listen
    if (listen(listener, 10) == -1) {
        perror("listen");
        exit(3);
    }

    // add the listener to the master set
    FD_SET(listener, &master);

    // keep track of the biggest file descriptor
    outMan << "Waiting for a connection on: " << buffer << " port: " << PORT << endl;

    // handle new connections
    addrlen = sizeof remoteaddr;
    newfd = accept(listener,
                   (struct sockaddr *) &remoteaddr,
                   &addrlen);

    if (newfd == -1) {
        perror("accept");
    } else {
        FD_SET(newfd, &master); // add to master set
        outMan << "Manager: Router Connected\n";

        inet_ntop(remoteaddr.ss_family,
                  get_in_addr((struct sockaddr *) &remoteaddr),
                  remoteIP, INET6_ADDRSTRLEN);
    }
    for (;;) {
        uint16_t vCheck = 0;
        uint16_t lenCheck = 0;
        if (sendit) {
            input = "hi";
            outMan << "Send: " << input << endl;
            mLength = static_cast<uint16_t>(input.length());
            lenCheck = htons(static_cast<uint16_t>(mLength));
            if (mLength > 140) {
                printf("Input too long\n");
                sendit = true;
            } else {
                in = input.c_str();
                for (unsigned int i = 0; i < strlen(in); i++) {
                    buf[i + 4] = (unsigned char) in[i];
                }
                buf[0] = 1;
                buf[1] = 201;
                buf[3] = 140;
                if (send(newfd, buf, static_cast<size_t>(mLength + 4), 0) == -1) {
                    perror("send");
                    exit(6);
                }
                memset(buf, 0, sizeof(buf));
                sendit = false;
            }
        } else {
            // first pull out version number
            if ((nbytes = recv(newfd, &vCheck, 2, 0)) <= 0) {
                // got error or connection closed by server
                if (nbytes == 0) {
                    // connection closed
                    outMan << "socket disconnected\n";
                    exit(5);
                } else {
                    //perror("recv");
                    exit(0);
                }
            } else {
                vCheck = ntohs(vCheck);
                if (vCheck != 457) printf("Incorrect version Number.\n");
            }
            if ((nbytes = recv(newfd, &lenCheck, 2, 0)) <= 0) {
                // got error or connection closed by server
                if (nbytes == 0) {
                    // connection closed
                    outMan << "socket disconnected\n";
                    exit(5);
                } else {
                    perror("recv");
                    exit(6);
                }
            } else {
                // set the message length
                lenCheck = ntohs(lenCheck);
            }
            if ((nbytes = recv(newfd, buf, sizeof(buf), 0)) <= 0) {
                // got error or connection closed by server
                if (nbytes == 0) {
                    // connection closed
                    outMan << "socket disconnected\n";
                    exit(5);
                } else {
                    perror("recv");
                    exit(6);
                }
            } else {
                // print the message
                outMan << "Received: " << buf << endl;
                memset(buf, 0, sizeof(buf));
                sendit = true;
            }
        }
    }
}

int check(const string &s) {
    // check that the values are valid as int
    int n;
    istringstream strm(s);
    if (!(strm >> n)) {
        cerr << "Invalid number: " << s << endl;
        return -1;
    }
    return n;
}

int main(int argc, char **argv) {
    // seed random for later
    srand(static_cast<unsigned int>(time(nullptr)));
    char *file = nullptr;
    int index, r1n, r2n, costn;
    string r1, r2, cost;
    bool topoRead = false;
    int status = 0;
    pid_t wpid;
    manager m = manager();
    ofstream ostr(m.killFile);

    // check that there is at least one argument before proceeding
    if (argc <= 1) return manager::usage();
    // print out the name of the requested page
    file = argv[1];
    cout << "Reading File: " << file << endl;

    ifstream istr(file);
    if (istr.fail()) {
        cout << "File not read, exiting...." << endl;
        return manager::usage();
    } else {
        cout << "File read Successfully, proceeding with routing demo..." << endl;
    }
    string num;
    istr >> num;
    istringstream inNum(num);
    if (!(inNum >> index)) {
        cerr << "File must begin with number of routers" << endl;
    }
    // read the file and pull all topology info, add info to list
    while (!topoRead) {
        istr >> r1;
        r1n = check(r1);
        if (r1n == -1) {
            topoRead = true;
            break;
        }
        istr >> r2;
        r2n = check(r2);
        istr >> cost;
        costn = check(cost);
        /* TODO do something with the values */

        if (istr.fail() && !istr.eof()) {
            cerr << "file not formatted correctly, exiting." << endl;
            return -1;
        }
    }
    istr.close();

    for (int i = 0; i < index; i++) {
        pid_t pid = fork();

        if (pid == -1) {
            cout << pid << ": error creating child process.. exiting" << endl;
            exit(1);
        } else if (pid == 0) { // This is the child process
            router r = router();
            sleep(2);
            r.startRouter(ostr, PORT);
            exit(0);
        } else if (pid > 0) {
            // do manager stuff (only stuff for each router)
            m.manage(ostr);
        }
    }
    while ((wpid = wait(&status)) > 0);
    cout << getpid() << ": All finished, closing up" << endl;
    ostr.close();
    ifstream istrKill(m.killFile);
    string killID;
    while (!istrKill.eof()) {
        istrKill >> killID;
        if (!istrKill.eof()) {
            int killnum = check(killID);
            cout << "Making sure PID: " << killID << " is dead" << endl;
            kill(killnum, 0);
        }
    }

    return 0;
}
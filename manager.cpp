#include "project3.h"


using namespace std;

// Print the correct usage in case of user syntax error.
int manager::usage() {
    cout << "Usage: to start router demo, run \"./manager <file name>\"" << endl;
    cout << "See readme for accepted file format" << endl;
    return -1;
}

void manager::getTime(char *stamp, int len) {
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

void manager::writeHeader(ofstream &o) {
    char stamp[100];
    manager::getTime(stamp, sizeof(stamp));
    o << "Manager Log File" << endl;
    o << "Log File Created: " << stamp << endl << endl;
}

// based on example at: https://stackoverflow.com/questions/212528/get-the-ip-address-of-the-machine
void GetPrimaryIp(char *buffer, socklen_t buflen) {
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

    const char *p = inet_ntop(AF_INET, &name.sin_addr, buffer, buflen);
    assert(p);

    close(sock);
}

// get sockaddr, IPv4 or IPv6:
void *manager::get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in *) sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *) sa)->sin6_addr);
}

void sigchld_handler(int s) {
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while (waitpid(-1, nullptr, WNOHANG) > 0);

    errno = saved_errno;
}

int manager::manage(ofstream &ostr, int index) {
    pid_t mypid = getpid();
    char buffer[MAXDATASIZE];
    size_t buflen = MAXDATASIZE;
    GetPrimaryIp(buffer, static_cast<socklen_t>(buflen));
    ostr << mypid << endl;
    ofstream outMan("manager.out");
    manager::writeHeader(outMan);
    char stamp[100];
    manager::getTime(stamp, sizeof(stamp));
    outMan << stamp << "[Manager] TCP Server Started" << endl;
    outMan << stamp << "[Manager] IP: " << buffer << "  Port: " << PORT << "    Routers: " << index << endl;

    fd_set master{};
    fd_set read_fds{};
    int fdmax;

    // data structure to maintain each router's info
    vector<node> routers;
    struct node temp{};

    // all tracked stages
    bool allCon = false;
    bool allInfo = false;
    bool allReady = false;
    bool LBDone = false;
    bool dijkDone = false;

    int listener = 0;     // listening socket descriptor
    int new_fd = 0;        // newly accept()ed socket descriptor
    struct sigaction sa{};
    string input;
    struct sockaddr_storage their_addr{};
    int id = 0;
    socklen_t sin_size;
    socklen_t addrlen;
    char s[INET6_ADDRSTRLEN];
    int yes = 1;        // for setsockopt() SO_REUSEADDR, below
    int rv;
    ssize_t nbytes;
    struct addrinfo hints{}, *ai, *p;

    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);

    outMan << stamp << "[Manager] Binding Socket" << endl;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(nullptr, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "[Manager] %s\n", gai_strerror(rv));
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

    if (p == nullptr) {
        fprintf(stderr, "[Manager] failed to bind\n");
        exit(2);
    }

    freeaddrinfo(ai); // all done with this

    if (listen(listener, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }
    FD_SET(listener, &master);
    fdmax = listener;

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, nullptr) == -1) {
        perror("sigaction");
        exit(1);
    }
    manager::getTime(stamp, sizeof(stamp));
    outMan << stamp << "[Manager] waiting for connections..." << endl;

    int count = 0;
    for (;;) {  // main accept() loop
        read_fds = master; // copy it
        if (select(fdmax + 1, &read_fds, nullptr, nullptr, nullptr) == -1) {
            perror("select");
            exit(4);
        }
        for (int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                if (i == listener) {
                    // handle new connections
                    addrlen = sizeof their_addr;
                    new_fd = accept(listener,
                                    (struct sockaddr *) &their_addr,
                                    &addrlen);

                    if (new_fd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(new_fd, &master); // add to master set
                        if (new_fd > fdmax) {    // keep track of the max
                            fdmax = new_fd;
                            temp.fd = i;
                            temp.id = to_string(count).c_str();
                            routers.push_back(temp);
                        }
                        manager::getTime(stamp, sizeof(stamp));
                        outMan << stamp << "[Manager] Router " << count << " connected" << endl;
                        count++;
                    }
                } else {
                    memset(&buffer, 0, sizeof(buffer));
                    if (!allCon) {
                        if ((nbytes = recv(new_fd, buffer, sizeof(buffer), 0)) <= 0) {
                            // got error or connection closed by server
                            if (nbytes == 0) {
                                // connection closed
                                printf("[Manager] router socket closed.");
                            } else {
                                perror("recv");
                            }
                            close(i);
                            FD_CLR(i, &master);
                        } else {

                        }
                        //id++;
                    } else if (!allInfo) {
                        /*TODO construct actual packet here*/
                        string temp = "ack";
                        for (int k = 0; k < temp.length(); k++) {
                            buffer[k] = temp[k];
                        }
                        if (send(i, buffer, sizeof(buffer), 0) == -1) {
                            close(i);
                            cerr << "[Manager] router closed prematurely, exiting" << endl;
                            exit(3);
                        } else {
                            manager::getTime(stamp, sizeof(stamp));
                            outMan << stamp << "[Manager] Sent: " << buffer << endl;
                        }
                        count++;
                        if (count == index) {
                            allInfo = true;
                            manager::getTime(stamp, sizeof(stamp));
                            outMan << stamp << "[Manager] All router Info Sent" << endl;
                            sleep(1);
                            cout << "Temp Finish" << endl;
                            exit(0);
                        }
                    }
                }
            }
        }
        if (!allCon && count == index) {
            manager::getTime(stamp, sizeof(stamp));
            outMan << stamp << "[Manager] All routers connected" << endl;
            allCon = true;
            count = 0;
            //exit(0);
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
    int r1n, r2n, costn;
    // linked list to hold links
    vector<manager::link> links;
    // linked list to hold messages
    vector<manager::msg> mess;
    string r1, r2, cost;
    //bool doneskies = false;
    int status = 0;
    // pid_t wpid;
    manager m = manager();
    ofstream ostr(m.killFile);
    // check that there is at least one argument before proceeding
    if (argc <= 1) return manager::usage();
    // print out the name of the requested page
    file = argv[1];
    cout << "[Demo] Reading File: " << file << endl;

    ifstream istr(file);
    if (istr.fail()) {
        cout << "[Demo] File not read, exiting...." << endl;
        return manager::usage();
    } else {
        cout << "[Demo] File read Successfully, proceeding with routing demo..." << endl;
    }
    string num;
    istr >> num;
    istringstream inNum(num);
    if (!(inNum >> m.index)) {
        cerr << "[Demo] File must begin with number of routers" << endl;
    }
    // read the file and pull all topology info, add info to list
    while (true) {
        istr >> r1;
        r1n = check(r1);
        if (r1n == -1) {
            //doneskies = true;
            break;
        }
        manager::link l;
        istr >> r2;
        r2n = check(r2);
        istr >> cost;
        costn = check(cost);
        l.sourceID = r1n;
        l.destID = r2n;
        l.cost = costn;

        links.push_back(l);

        if (istr.fail() && !istr.eof()) {
            cerr << "[Demo] file not formatted correctly, exiting." << endl;
            return -1;
        }
    }
    //doneskies = false;
    // read the file and pull all packet info, add info to list
    while (true) {
        istr >> r1;
        r1n = check(r1);
        if (r1n == -1) {
            //doneskies = true;
            break;
        }
        manager::msg p;
        istr >> r2;
        r2n = check(r2);
        p.sourceID = r1n;
        p.destID = r2n;

        mess.push_back(p);

        if (istr.fail() && !istr.eof()) {
            cerr << "[Demo] file not formatted correctly, exiting." << endl;
            return -1;
        }
    }
    istr.close();

    for (int i = 0; i < m.index; i++) {
        pid_t pid = fork();

        if (pid == -1) {
            cout << "[Demo] error creating child process.. exiting" << endl;
            exit(1);
        } else if (pid == 0) { // This is the child process
            router *r = new router();
            sleep(2);
            r->startRouter(ostr);
            exit(0);
        } else if (pid > 0) {
            // do manager stuff (only stuff for each router)
        }
    }
    m.manage(ostr, m.index);
    while ((wait(&status)) > 0);
    cout << "[Demo] Demo complete, closing up" << endl;
    ostr.close();
    ifstream istrKill(m.killFile);
    string killID;
    while (!istrKill.eof()) {
        istrKill >> killID;
        if (!istrKill.eof()) {
            int killnum = check(killID);
            kill(killnum, 0);
            cout << "PID: " << killnum << " successfully terminated." << endl;
        }
    }

    return 0;
}
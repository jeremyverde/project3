#include <utility>

#include "project3.h"


using namespace std;

manager::manager() {
    links = vector<link>(11);
    mess = vector<msg>(11);
    routers = vector<node>(11);
    index = 0;
}

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
        cerr << "[Manager] Time's broken" << endl;
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

void sigchld_handler(int s) {
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while (waitpid(-1, nullptr, WNOHANG) > 0);

    errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
void *manager::get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in *) sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *) sa)->sin6_addr);
}

int manager::manage(ofstream &ostr) {
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

    // temporary data structure to maintain each router's info
    struct node temp{};
    struct node *curNode;

    // all tracked stages
    bool allCon = false;
    bool allInfo = false;
    bool allReady = false;
    bool AKRD = false;
    bool LBDone = false;
    bool AKLB = false;
    bool dijkDone = false;
    bool AKDK = false;
    bool msgDone = false;
    bool quit = false;
    bool AKQT = false;
    bool reading = true;

    int listener = 0;     // listening socket descriptor
    int new_fd = 0;        // newly accept()ed socket descriptor
    struct sigaction sa{};
    string input;
    struct sockaddr_storage their_addr{};
    string id;
    socklen_t addrlen;
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
        perror("[Manager] listen");
        exit(1);
    }
    FD_SET(listener, &master);
    fdmax = listener;

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, nullptr) == -1) {
        perror("[Manager] sigaction");
        exit(1);
    }
    manager::getTime(stamp, sizeof(stamp));
    outMan << stamp << "[Manager] waiting for connections..." << endl;

    int count = 0;
    int c = 0;
    string packet, rIN, rID;
    for (;;) {  // main accept() loop
        read_fds = master; // copy it
        if (reading) {
            if (select(fdmax + 1, &read_fds, nullptr, nullptr, nullptr) == -1) {
                perror("[Manager] select");
                exit(4);
            }
        }
        for (int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                if (i == listener) {
                    // handle new connections
                    addrlen = sizeof their_addr;
                    // cout << "new connection.." << endl;
                    if (reading) {
                        new_fd = accept(listener,
                                        (struct sockaddr *) &their_addr,
                                        &addrlen);
                        if (new_fd == -1) {
                            perror("[Manager] accept");
                        } else {
                            FD_SET(new_fd, &master); // add to master set
                            if (new_fd > fdmax) {    // keep track of the max
                                fdmax = new_fd;
                            }
                            manager::getTime(stamp, sizeof(stamp));
                            outMan << stamp << "[Manager] Router " << c << " connected" << endl;
                            temp.fd = new_fd;
                            temp.id = c;
                            routers.at(c) = temp;
                            c++;
                        }
                    }
                } else { //ugly, but its threadsafe enough for now
                    memset(&buffer, 0, sizeof(buffer));
                    curNode = getRouter(i);
                    if (curNode == nullptr) {
                        // couldn't find a matching router
                        return -1;
                    }
                    id = to_string(curNode->id);

                    if (!allCon) {
                        if ((nbytes = recv(i, buffer, sizeof(buffer), 0)) <= 0) {
                            // got error or connection closed by server
                            if (nbytes == 0) {
                                // connection closed
                                printf("[Manager] router socket closed.");
                            } else {
                                perror("[Manager] recv");
                            }
                            close(i);
                            FD_CLR(i, &master);
                        } else {
                            count++;
                            packet = buffer;
                            rIN = packet.substr(0, 4);
                            manager::getTime(stamp, sizeof(stamp));
                            outMan << stamp << "[Manager] recv: " << rIN << " from router " << id << endl;
                            curNode->port = atoi(rIN.c_str());
                            if (count == index) {
                                manager::getTime(stamp, sizeof(stamp));
                                outMan << stamp << "[Manager] All routers connected" << endl;
                                allCon = true;
                                reading = false;
                                count = 0;
                            }
                        }
                    } else if (!allInfo) {
                        string t = "INFO";
                        t = fillPacket(t, id);
                        packet = getLinks(curNode->id, i);
                        //cout << "[Manager] packet: " << packet << endl;
                        t.append(packet);
                        for (int k = 0; k < t.length(); k++) {
                            buffer[k] = t[k];
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
                            count = 0;
                            manager::getTime(stamp, sizeof(stamp));
                            outMan << stamp << "[Manager] All router Info Sent" << endl;
                            reading = true;
                        }
                    } else if (!allReady) {
                        if ((nbytes = recv(i, buffer, sizeof(buffer), 0)) <= 0) {
                            // got error or connection closed by server
                            if (nbytes == 0) {
                                // connection closed
                                printf("[Manager] router socket closed.\n");
                            } else {
                                perror("[Manager] recv");
                            }
                            close(i);
                            FD_CLR(i, &master);
                        } else {
                            count++;
                            packet = buffer;
                            rIN = packet.substr(0, 4);
                            rID = packet.substr(5, 1);
                            manager::getTime(stamp, sizeof(stamp));
                            outMan << stamp << "[Manager] recv: " << rIN << " from router " << rID << endl;
                        }
                        if (count == index) {
                            manager::getTime(stamp, sizeof(stamp));
                            outMan << stamp << "[Manager] All routers ready" << endl;
                            allReady = true;
                            reading = false;
                            count = 0;
                        }
                    } else if (!AKRD) {
                        string t = "AKRD";
                        t = fillPacket(t, id);
                        for (int k = 0; k < t.length(); k++) {
                            buffer[k] = t[k];
                        }
                        //sleep(1);
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
                            AKRD = true;
                            count = 0;
                            manager::getTime(stamp, sizeof(stamp));
                            outMan << stamp << "[Manager] All AKRD sent" << endl;
                            reading = true;
                        }
                    } else if (!LBDone) {
                        if ((nbytes = recv(i, buffer, sizeof(buffer), 0)) <= 0) {
                            // got error or connection closed by server
                            if (nbytes == 0) {
                                // connection closed
                                printf("[Manager] router socket closed.\n");
                            } else {
                                perror("[Manager] recv");
                            }
                            close(i);
                            FD_CLR(i, &master);
                        } else {
                            count++;
                            packet = buffer;
                            rIN = packet.substr(0, 4);
                            rID = packet.substr(5, 1);
                            manager::getTime(stamp, sizeof(stamp));
                            outMan << stamp << "[Manager] recv: " << rIN << " from router " << rID << endl;
                            if (count == index) {
                                manager::getTime(stamp, sizeof(stamp));
                                outMan << stamp << "[Manager] All routers LB complete" << endl;
                                LBDone = true;
                                reading = false;
                                count = 0;
                                //exit(0);
                            }
                        }
                    } else if (!AKLB) {
                        string t = "AKLB";
                        t = fillPacket(t, id);
                        for (int k = 0; k < t.length(); k++) {
                            buffer[k] = t[k];
                        }
                        //sleep(1);
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
                            AKLB = true;
                            count = 0;
                            manager::getTime(stamp, sizeof(stamp));
                            outMan << stamp << "[Manager] All AKLB sent" << endl;
                            reading = true;
                        }
                    } else if (!dijkDone) {
                        if ((nbytes = recv(i, buffer, sizeof(buffer), 0)) <= 0) {
                            // got error or connection closed by server
                            if (nbytes == 0) {
                                // connection closed
                                printf("[Manager] router socket closed.\n");
                            } else {
                                perror("[Manager] recv");
                            }
                            close(i);
                            FD_CLR(i, &master);
                        } else {
                            count++;
                            packet = buffer;
                            rIN = packet.substr(0, 4);
                            rID = packet.substr(5, 1);
                            manager::getTime(stamp, sizeof(stamp));
                            outMan << stamp << "[Manager] recv: " << rIN << " from router " << rID << endl;
                            if (count == index) {
                                manager::getTime(stamp, sizeof(stamp));
                                outMan << stamp << "[Manager] All routers Dijkstra complete" << endl;
                                dijkDone = true;
                                reading = false;
                                count = 0;
                                //exit(0);
                            }
                        }
                    } else if (!AKDK) {
                        string t = "AKDK";
                        t = fillPacket(t, id);
                        for (int k = 0; k < t.length(); k++) {
                            buffer[k] = t[k];
                        }
                        //sleep(1);
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
                            AKDK = true;
                            count = 0;
                            manager::getTime(stamp, sizeof(stamp));
                            outMan << stamp << "[Manager] All AKDK sent" << endl;
                            reading = true;
                        }
                    } else if (!msgDone) {
                        if ((nbytes = recv(i, buffer, sizeof(buffer), 0)) <= 0) {
                            // got error or connection closed by server
                            if (nbytes == 0) {
                                // connection closed
                                printf("[Manager] router socket closed.\n");
                            } else {
                                perror("[Manager] recv");
                            }
                            close(i);
                            FD_CLR(i, &master);
                        } else {
                            count++;
                            packet = buffer;
                            rIN = packet.substr(0, 4);
                            rID = packet.substr(5, 1);
                            manager::getTime(stamp, sizeof(stamp));
                            outMan << stamp << "[Manager] recv: " << rIN << " from router " << rID << endl;
                            if (count == index) {
                                manager::getTime(stamp, sizeof(stamp));
                                outMan << stamp << "[Manager] All routers messaging complete" << endl;
                                msgDone = true;
                                reading = false;
                                count = 0;
                                //exit(0);
                            }
                        }
                    } else if (!quit) {
                        string t = "QUIT";
                        t = fillPacket(t, id);
                        for (int k = 0; k < t.length(); k++) {
                            buffer[k] = t[k];
                        }
                        //sleep(1);
                        if (send(i, buffer, sizeof(buffer), 0) == -1) {
                            close(i);
                            cerr << "[Manager] router closed prematurely, exiting" << endl;
                            exit(3);
                        } else {
                            count++;
                            manager::getTime(stamp, sizeof(stamp));
                            outMan << stamp << "[Manager] Sent: " << buffer << endl;
                            if (count == index) {
                                quit = true;
                                count = 0;
                                manager::getTime(stamp, sizeof(stamp));
                                outMan << stamp << "[Manager] All Quit sent" << endl;
                                reading = true;
                            }
                        }
                    } else if (!AKQT) {
                        if ((nbytes = recv(i, buffer, sizeof(buffer), 0)) <= 0) {
                            // got error or connection closed by server
                            if (nbytes == 0) {
                                // connection closed
                                printf("[Manager] router socket closed.\n");
                            } else {
                                perror("[Manager] recv");
                            }
                            close(i);
                            FD_CLR(i, &master);
                        } else {
                            count++;
                            packet = buffer;
                            rIN = packet.substr(0, 4);
                            rID = packet.substr(5, 1);
                            manager::getTime(stamp, sizeof(stamp));
                            outMan << stamp << "[Manager] recv: " << rIN << " from router " << rID << endl;
                            if (count == index) {
                                manager::getTime(stamp, sizeof(stamp));
                                outMan << stamp << "[Manager] Quitting Routers and Manager" << endl;
                                return 0;
                            }
                        }
                    }
                }
            }
        }
    }
}

string manager::fillPacket(string &c, string &id) {
    string packet = move(c);
    packet.append("|");
    packet.append(id);
    return packet;
}

manager::node *manager::getRouter(int i) {
    for (auto &R : routers) {
        if (R.fd == i) {
            return &R;
        }
    }
    return nullptr;
}

string manager::getLinks(int id, int i) {
    string s;
    node *dest = nullptr;
    for (auto &link : links) {
        for (auto &R : routers) {
            if (R.id == link.destID) {
                dest = &R;
                break;
            }
        }
        if (link.sourceID == id) {
            s.append("|");
            s.append(to_string(link.sourceID));
            s.append(" ");
            s.append(to_string(link.destID));
            s.append(" ");
            s.append(to_string(link.cost));
            s.append(" ");
            s.append(to_string(dest->port));
        } else if (link.destID == id) {
            for (auto &R : routers) {
                if (R.id == link.sourceID) {
                    dest = &R;
                    break;
                }
            }
            if (dest == nullptr) {
                cerr << "[Manager] no such router, exiting." << endl;
                exit(3);
            }
            s.append("|");
            s.append(to_string(link.destID));
            s.append(" ");
            s.append(to_string(link.sourceID));
            s.append(" ");
            s.append(to_string(link.cost));
            s.append(" ");
            s.append(to_string(dest->port));
        }
    }
    return s;
}

/*int manager::getSome(int i,fd_set &m,ofstream &o){
    char buf[INET6_ADDRSTRLEN];
    ssize_t nbytes;
    char stamp[100];

    memset(&buf,0,sizeof(buf));

    if ((nbytes = recv(i, buf, sizeof(buf), 0)) <= 0) {
        // got error or connection closed by server
        if (nbytes == 0) {
            // connection closed
            printf("[Manager] router socket closed.\n");
        } else {
            perror("[Manager] recv");
        }
        close(i);
        FD_CLR(i, &m);
    }else{
        manager::getTime(stamp, sizeof(stamp));
        o << stamp << "[Manager] recv: " << buf << endl;
    }
    return static_cast<int>(nbytes);
}

int manager::giveSome(string t, int i,fd_set &m,ofstream &o){
    char buf[INET6_ADDRSTRLEN];
    ssize_t nbytes;
    char stamp[100];

    memset(&buf,0,sizeof(buf));

    for (int k = 0; k < t.length(); k++) {
        buf[k] = t[k];
    }
    //manager::getTime(stamp, sizeof(stamp));
    //outMan << stamp << "[Manager] Sending info to router: " << buffer << endl;
    sleep(1);
    if (send(i, buf, sizeof(buf), 0) == -1) {
        close(i);
        cerr << "[Manager] router closed prematurely, exiting" << endl;
        exit(3);
    } else {
        manager::getTime(stamp, sizeof(stamp));
        o << stamp << "[Manager] Sent: " << buf << endl;
    }
}
*/

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
    unsigned long nLinks = 0;
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

        if (nLinks >= m.links.size()) {
            m.links.resize(m.links.size() * 2);
        }
        m.links.at(nLinks) = l;
        nLinks++;

        if (istr.fail() && !istr.eof()) {
            cerr << "[Demo] file not formatted correctly, exiting." << endl;
            return -1;
        }
    }
    m.links.resize(nLinks);
    //doneskies = false;
    // read the file and pull all packet info, add info to list
    unsigned long nMess = 0;
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

        if (nMess >= m.mess.size()) {
            m.mess.resize(m.mess.size() * 2);
        }
        m.mess.at(nMess) = p;
        nMess++;
        if (istr.fail() && !istr.eof()) {
            cerr << "[Demo] file not formatted correctly, exiting." << endl;
            return -1;
        }
    }
    m.mess.resize(nMess);

    istr.close();

    for (int i = 0; i < m.index; i++) {
        pid_t pid = fork();

        if (pid == -1) {
            cout << "[Demo] error creating child process.. exiting" << endl;
            exit(1);
        } else if (pid == 0) { // This is the child process
            auto *r = new router();
            sleep(1);
            r->startRouter(ostr);
            exit(0);
        } else if (pid > 0) {
            // do manager stuff (only stuff for each router)
        }
    }
    m.manage(ostr);
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
            cout << "[Demo] PID: [" << killnum << "] successfully terminated." << endl;
        }
    }

    return 0;
}
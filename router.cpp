
#include "project3.h"

using namespace std;

router::router() {
    links = vector<manager::link>(11);
    nLinks = 0;
}

//based on example at: https://www.daniweb.com/programming/software-development/threads/478595/how-to-use-select-in-udp-sockets-to-read-and-write-from-client-to-server
void router::udpListen(ofstream &o, string &portnum) {
    o << endl << "Starting Limited Broadcast\n" << endl;
    char stamp[100];

    fd_set master{};
    fd_set read_fds{};
    fd_set writefds{};
    int fdmax;
    struct timeval tv{};
    int socket_fd = 0;
    ssize_t bytes_read;

    unsigned int address_length;
    char recieve_data[MAXDATASIZE], send_data[MAXDATASIZE];
    struct sockaddr *client_address{};
    if ((socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("Router: socket");
    }
    int flags = fcntl(socket_fd, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(socket_fd, F_SETFL, flags);
    //fcntl(socket_fd, F_SETFL, O_NONBLOCK); //set socket to non-blocking

    // wait until either socket has data ready to be recv()d (timeout 2 secs)
    tv.tv_sec = 2;
    int mypid = getpid();
    struct addrinfo udphints{}, theirhints{}, *theirinfo{}, *servinfo, *p;
    int rv;
    //ssize_t numbytes;
    //struct sockaddr_storage their_addr{};
    char udpbuf[MAXDATASIZE];
    //socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];
    GetPrimaryIp(udpbuf, sizeof(udpbuf));

    memset(&udphints, 0, sizeof udphints);
    udphints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    udphints.ai_socktype = SOCK_DGRAM;
    udphints.ai_flags = AI_PASSIVE; // use my IP

    FD_SET(socket_fd, &read_fds);
    FD_SET(socket_fd, &writefds);

    if ((rv = getaddrinfo(nullptr, portnum.c_str(), &udphints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    if (bind(socket_fd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        close(socket_fd);
        perror("Router: bind");
    }
    address_length = sizeof(struct sockaddr);
    fdmax = socket_fd;
    for (int a = 0; a < 2; a++) {
        read_fds = master; // copy it
        //cout << "try select..." << endl;
        int rec = select(fdmax + 1, &read_fds, &writefds, nullptr, &tv);
        if (rec == -1) {
            perror("[Router] select");
            exit(4);
        } else if (rec == 0) {
            printf("Timeout occurred!  No data after 2 seconds.\n");
        } else {
            //cout << "select went through... " << endl;
            for (int i = 0; i <= fdmax; i++) {
                if (FD_ISSET(i, &read_fds)) {
                    o << "[Router: " << mypid << "] reading " << endl;
                    //FD_CLR(socket_fd, &read_fds);
                    for (auto &link : links) {
                        if ((rv = getaddrinfo(nullptr, to_string(link.dPort).c_str(), &theirhints, &theirinfo)) != 0) {
                            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
                            exit(1);
                        } else {
                            client_address = theirinfo->ai_addr;
                            bytes_read = recvfrom(socket_fd, recieve_data, MAXDATASIZE, 0,
                                                  (struct sockaddr *) &client_address,
                                                  &address_length);
                            if (bytes_read > 0) {
                                manager::getTime(stamp, sizeof(stamp));
                                o << stamp << "[Router: " << mypid << "] recv: " << recieve_data << " from: "
                                  << link.dPort << endl;
                            }
                        }
                    }
                } else if (FD_ISSET(i, &writefds)) {
                    //o << "[Router: " << mypid << "] sending " << endl;
                    for (int k = 0; k < adjSend.length(); k++) {
                        send_data[k] = adjSend[k];
                    }
                    for (auto &link : links) {
                        if ((rv = getaddrinfo(nullptr, to_string(link.dPort).c_str(), &theirhints, &theirinfo)) != 0) {
                            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
                            exit(1);
                        } else {
                            client_address = theirinfo->ai_addr;
                            sendto(socket_fd, send_data, strlen(send_data), 0, client_address,
                                   sizeof(struct sockaddr));
                            manager::getTime(stamp, sizeof(stamp));
                            o << stamp << "[Router: " << mypid << "] send: " << send_data << " to: " << link.dPort
                              << endl;
                        }
                    }
                }
            }
            //close(socket_fd);
        }
    }
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

string router::fillPacket(string &c, string &id) {
    string packet = move(c);
    packet.append("|");
    packet.append(id);
    return packet;
}

void router::writeHeader(ofstream &o) {
    char stamp[100];
    manager::getTime(stamp, sizeof(stamp));
    o << "Router: " << getpid() << " Log File" << endl;
    o << "Log File Created: " << stamp << endl;
}

int router::startRouter(ofstream &ostr) {
    int mypid = getpid();
    string port = to_string((mypid % 6997) + 1100);

    // TCP SECTION
    string pid = to_string(mypid);
    string file = "router";
    file.append(pid + ".out");
    ofstream out(file);
    writeHeader(out);
    char stamp[100];
    manager::getTime(stamp, sizeof(stamp));
    out << stamp << "[Router: " << mypid << "] writing pid to kill file." << endl;
    ostr << mypid << endl;
    out << endl << "[Router] my port is: " << port << endl << endl;
    // code based largely off of beej's guide example program, see README
    int sok = 0;
    char buf[MAXDATASIZE];
    memset(&buf, 0, sizeof(buf));
    ssize_t nbytes = 0;
    string input;
    int yes = 1;        // for setsockopt() SO_REUSEADDR, below
    int status;
    struct addrinfo hints{}, *ai;// will point to the results

    bool ready = false;
    bool done = false;
    bool quit = false;
    bool sendit = true;
    //bool broadcasting = true;

    GetPrimaryIp(buf, sizeof(buf));
    const char *IP = buf;
    string portnum;
    string packet;

    // start up UDP socket
    //udpListen(out, portnum);

    // make sure the manager spins up first
    //sleep(1);

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me
    manager::getTime(stamp, sizeof(stamp));
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
        manager::getTime(stamp, sizeof(stamp));
        out << stamp << "[Router: " << mypid << "] Connected." << endl;
    }
    // Test if the socket is in non-blocking mode:
    if (fcntl(sok, F_GETFL) & O_NONBLOCK) {
        // socket is non-blocking
        cout << "[Router: " << mypid << "] socket is not blocking, set to blocking mode" << endl;
    } else {
        // cout << "[Router: " << mypid << "] socket is blocking" << endl;
    }

    freeaddrinfo(ai);

    string b, in, id, adj;
    while (true) {
        if (ready) {
            memset(&buf, 0, sizeof(buf));
            b = "REDY";
            b = fillPacket(b, myID);
            for (int k = 0; k < b.length(); k++) {
                buf[k] = b[k];
            }
            manager::getTime(stamp, sizeof(stamp));
            out << stamp << "[Router: " << mypid << "] sending: " << buf << endl;
            if (send(sok, buf, sizeof(buf), 0) == -1) {
                perror("[Router: ] send failed");
                exit(6);
            }
            sendit = false;
            ready = false;
            //exit(0);
        } else if (done) {
            memset(&buf, 0, sizeof(buf));
            b = "DONE";
            b = fillPacket(b, myID);
            for (int k = 0; k < b.length(); k++) {
                buf[k] = b[k];
            }
            manager::getTime(stamp, sizeof(stamp));
            out << stamp << "[Router: " << mypid << "] sending: " << buf << endl;
            if (send(sok, buf, sizeof(buf), 0) == -1) {
                perror("[Router: ] send failed");
                exit(6);
            }
            sendit = false;
            done = false;
        } else if (quit) {
            memset(&buf, 0, sizeof(buf));
            b = "QTAK";
            b = fillPacket(b, myID);
            for (int k = 0; k < b.length(); k++) {
                buf[k] = b[k];
            }
            manager::getTime(stamp, sizeof(stamp));
            out << stamp << "[Router: " << mypid << "] sending: " << buf << endl;
            if (send(sok, buf, sizeof(buf), 0) == -1) {
                perror("[Router: ] send failed");
                exit(6);
            }
            //sleep(1);
            return 0;
        } else {
            if (sendit) {
                memset(&buf, 0, sizeof(buf));
                for (int k = 0; k < port.length(); k++) {
                    buf[k] = port[k];
                }
                if (debug) {
                    manager::getTime(stamp, sizeof(stamp));
                    out << stamp << "[Router: " << mypid << "] sending: " << buf << endl;
                }
                if (send(sok, buf, sizeof(buf), 0) == -1) {
                    perror("[Router: ] send failed");
                    exit(6);
                }
                sendit = false;
            } else {
                ready = false;
                memset(buf, 0, sizeof(buf));
                if ((nbytes = recv(sok, buf, MAXDATASIZE, 0)) <= 0) {
                    // got error or connection closed by server
                    if (nbytes == 0) {
                        // connection closed
                        if (debug) {
                            manager::getTime(stamp, sizeof(stamp));
                            cerr << "[Router: " << mypid << "] socket disconnected. Buf: " << buf << endl;
                        }
                        return 0;
                    } else {
                        cerr << "[Router: " << mypid << "] recv error: " << strerror(errno) << " buf: " << buf << endl;
                        exit(6);
                    }
                } else {
                    manager::getTime(stamp, sizeof(stamp));
                    out << stamp << "[Router: " << mypid << "] recv: " << buf << endl;
                    packet = buf;
                    in = packet.substr(0, 4);
                    id = packet.substr(5, 1);
                    if (in == "INFO") {
                        ready = true;
                        myID = id;
                        adj = packet.substr(6);
                        adjSend = adj;
                        if (adj.length() >= 1) {
                            parseLinks(adj);
                        } else {
                            cerr << "[Router: " << mypid << "] has no neighbors! Closing node. " << id << endl;
                            return -1;
                        }
                        out << endl << "[Router: " << mypid << "] my id is: " << id << endl;
                        out << "[Router: " << mypid << "] my adj links are: " << adj << endl << endl;
                    } else if (in == "AKRD") {
                        /*TODO actually do the LB stuff when info comes*/
                        ready = true;
                        cout << "[Router: " << mypid << "] is performing LB... " << endl;
                        udpListen(out, port);
                    } else if (in == "AKLB") {
                        /*TODO actually do the Dijkstra stuff when info comes*/
                        ready = true;
                        cout << "[Router: " << mypid << "] is pretending to do Dijk... " << endl;
                    } else if (in == "AKDK") {
                        /*TODO actually do the Message stuff when info comes*/
                        done = true;
                        cout << "[Router: " << mypid << "] is pretending to send messages... " << endl;
                    } else if (in == "QUIT") {
                        quit = true;
                        cout << "[Router: " << mypid << "] is quitting " << endl;
                    }
                }
            }
        }
    }
}

void router::parseLinks(string s) {
    manager::link temp;
    string toss;
    istringstream ss(s);

    int n = 0;
    while (!ss.eof()) {
        ss >> toss;
        //cout << "toss: " << toss << endl;
        if (toss == "|") {
            ss >> toss;
            n = 0;
        }
        else if (n == 0) temp.sourceID = atoi(toss.c_str());
        else if (n == 1) temp.destID = atoi(toss.c_str());
        else if (n == 2) temp.cost = atoi(toss.c_str());
        else if (n == 3) {
            temp.dPort = atoi(toss.c_str());

            if (nLinks >= links.size()) {
                links.resize(links.size() * 2);
            }
            links.at(nLinks++) = temp;
            n = 0;
        }
        n++;
    }
    links.resize(nLinks);
}

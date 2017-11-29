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

void sigchld_handler(int s) {
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while (waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

int manager::manage(ofstream &ostr) {
    pid_t mypid = getpid();
    ostr << mypid << endl;
    cout << mypid << ": manager wrote pid to killFile" << endl;
    bool sendit = true; // keeps track of whether or not it's my turn to send
    int listener = 0;     // listening socket descriptor
    int new_fd = 0;        // newly accept()ed socket descriptor
    struct sigaction sa{};
    string input;
    struct sockaddr_storage their_addr{};
    char buffer[128];
    size_t buflen = 128;
    GetPrimaryIp(buffer, buflen);
    ofstream outMan("manager.out");
    int id = 0;
    socklen_t sin_size;
    char s[INET6_ADDRSTRLEN];
    int yes = 1;        // for setsockopt() SO_REUSEADDR, below
    int rv;
    ssize_t nbytes;

    struct addrinfo hints{}, *ai, *p;

    outMan << "[Manager] Binding Socket" << endl;
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

    // if we got here, it means we didn't get bound
    if (p == nullptr) {
        fprintf(stderr, "[Manager] failed to bind\n");
        exit(2);
    }

    freeaddrinfo(ai); // all done with this

    if (listen(listener, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, nullptr) == -1) {
        perror("sigaction");
        exit(1);
    }

    outMan << "[Manager] waiting for connections..." << endl;

    while (true) {  // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(listener, (struct sockaddr *) &their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *) &their_addr),
                  s, sizeof s);
        outMan << "[Manager] got connection from " << s << endl;
        outMan << "[Manager] Sending id to router " << id << endl;
        for (int i = 0; i < 2; i++) {
            if (sendit) {
                outMan << "[Manager] sending: ID" << endl;
                if (send(new_fd, "ID", 2, 0) == -1) {
                    perror("send");
                    close(new_fd);
                    outMan.close();
                    exit(0);
                }
                sendit = false;
            } else {
                sendit = true;
                memset(buffer, 0, sizeof(buffer));
                if ((nbytes = recv(new_fd, buffer, MAXDATASIZE, 0)) <= 0) {
                    // got error or connection closed by server
                    if (nbytes == 0) {
                        // connection closed
                        exit(2);
                    } else {
                        perror("recv");
                        exit(6);
                    }
                } else {
                    outMan << "[Manager] recv: " << buffer << endl;
                }
            }
        }
        close(new_fd);
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
    // linked list to hold links
    vector<manager::link> links;
    // linked list to hold messages
    vector<manager::msg> mess;
    string r1, r2, cost;
    bool doneskies = false;
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
    while (!doneskies) {
        istr >> r1;
        r1n = check(r1);
        if (r1n == -1) {
            doneskies = true;
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
            cerr << "file not formatted correctly, exiting." << endl;
            return -1;
        }
    }
    doneskies = false;
    // read the file and pull all packet info, add info to list
    while (!doneskies) {
        istr >> r1;
        r1n = check(r1);
        if (r1n == -1) {
            doneskies = true;
            break;
        }
        manager::msg p;
        istr >> r2;
        r2n = check(r2);
        p.sourceID = r1n;
        p.destID = r2n;

        mess.push_back(p);

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
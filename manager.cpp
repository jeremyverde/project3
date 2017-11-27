#include <iostream>
#include <zconf.h>
#include <wait.h>
#include <sstream>
#include "project3.h"


using namespace std;


/// Print the correct usage in case of user syntax error.
int manager::usage() {
    cout << "Usage: to start router demo, run \"./manager <file name>\"" << endl;
    cout << "See readme for accepted file format" << endl;
    return -1;
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
            r.startRouter(ostr);
            exit(0);
        } else if (pid > 0) {
            // do manager stuff
        }
    }
    while ((wpid = wait(&status)) > 0);
    cout << getpid() << ": All finished, closing up" << endl;
    ostr.close();
    ifstream istrKill(m.killFile);

    return 0;
}
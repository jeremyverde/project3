#include <iostream>
#include <zconf.h>
#include "project3.h"

using namespace std;

int router::startRouter(ofstream &ostr) {
    pid_t mypid = getpid();
    cout << "writing pid: " << mypid << " to kill file." << endl;
    ostr << mypid << endl;
    cout << mypid << ": I'm a router" << endl;
}
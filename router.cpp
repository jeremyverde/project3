#include <iostream>
#include <zconf.h>
#include "project3.h"

using namespace std;

int router::startRouter(ofstream &ostr) {
    pid_t mypid = getpid();
    ostr << mypid;
    cout << mypid << ": I'm a router" << endl;
}
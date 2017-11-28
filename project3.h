#ifndef PROJECT3_PROJECT3_H
#define PROJECT3_PROJECT3_H

#include <fstream>

using namespace std;

class router {
public:
    int startRouter(ofstream &o);
};

class manager {
public:
    static int usage();

    static int manage(ofstream &ostr);

    const char *killFile = "kill.txt";
};
#endif //PROJECT3_PROJECT3_H

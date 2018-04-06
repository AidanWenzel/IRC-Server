#include <iostream>
#include <string>
#include <vector>
#include <regex>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <map>
#include <set>

using namespace std;

class Object{
public:
    string name;
    set<string> members;

    explicit Object(string const & _name){
        name = _name;
    }
};

class User: public Object{
public:
    int connection;
    User(string const & _name, int FD) : Object(_name){
        connection = FD;
    }

    void message(string const &msg){
        write(connection, msg.c_str(), msg.length());
    }
};

int main(){
    User myuser("me", 0);
}
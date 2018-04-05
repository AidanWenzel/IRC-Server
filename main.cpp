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

using namespace std;

class Object{
private:
    string name;
    vector<Object> members;
};

class User: public Object{
public:
    User(string _name){

    }
};

class Channel: public Object{
public:
    Channel(string _name){

    }
};

void userProcess(int client_fd);

/**************Program wide vars***********/
string password = "";
unsigned short port = 12345;
mutex channelUsersInUse;
map<string, User> users;
map<string, Channel> channels;
/******************************************/

int main(int argc, char* argv[]) {
    if(argc == 2){
        regex pass("--opt-pass=.*");
        string input = argv[1];
        if(regex_match(input, pass))
            password = input.substr(11);
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int optval = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;

    bzero((char*)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if(bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr))<0) {
        perror("Failed to attach to port");
        exit(1);
    }
    if(listen(server_fd, 5)<0){
        perror("Error on listen");
        exit(1);
    }
    cout << "Server bound to FD: " << server_fd << endl;
    cout << "Server starting on port: " << port << " with password: " << password << endl;

    while(true){
        pthread_t threadID;
        int newUserConnection;
        newUserConnection = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if( newUserConnection < 0){
            perror("Error on accept");
            exit(1);
        }
        else {
            thread newClient(userProcess, newUserConnection);
            newClient.detach();
        }
    }

    return EXIT_SUCCESS;
}

void userProcess(int client_fd){
    /********Vars for command handling************/
    regex joinStr("JOIN #[a-zA-Z][0-9a-zA-Z]*");
    regex userStr("USER [a-zA-Z][0-9a-zA-Z]*");
    regex listStr("LIST .*");
    regex partStr("PART #[a-zA-Z][0-9a-zA-Z]*");
    regex opStr("OPERATOR .*");
    regex kickStr("KICK #[a-zA-Z][0-9a-zA-Z]* [a-zA-Z][0-9a-zA-Z]*");
    regex msgStr("PRIVMSG #?[a-zA-Z][0-9a-zA-Z]* .*");
    regex quitStr("QUIT");
    regex helpStr("HELP .*");
    /*********************************************/

    if(write(client_fd, "> Welcome to the server. Type HELP for commands\n", 48)<0){
        perror("failed to write to child");
    }

    bool userDeclared = false;
    string userName;
    string msg = "";

    while(true){
        char buffer[512] = {0};

        ssize_t r_val = read(client_fd, buffer, 512);
        buffer[r_val-1] = '\000';

        if(r_val == 0){
            cout << "Connection closed by user" << endl;
            return;
        }

        if(r_val < 0){
            cout << "Error reading from user" << endl;
            return;
        }

        stringstream input(buffer);
        string item;
        input >> item;

        if(regex_match(buffer, userStr)){
            if(!userDeclared){
                input >> userName;
                if(userName.length() > 20){
                    msg = "Usernames can be no longer than 20 characters\n";
                    write(client_fd, msg.c_str(), msg.length());
                    channelUsersInUse.unlock();
                    continue;
                }

                channelUsersInUse.lock();
                //check if user already exists, if not then allow creation
                if(users.find(userName) == users.end()){
                    users.insert(pair<string,User>(userName, User(userName)));
                }
                else{
                    msg = "That user already exists. Please choose a different username\n";
                    write(client_fd, msg.c_str(), msg.length());
                    channelUsersInUse.unlock();
                    continue;
                }
                channelUsersInUse.unlock();
                userDeclared = true;
            } else {
                msg = "> You have already declared a username!\n";
                write(client_fd, msg.c_str(), msg.length());
                continue;
            }
        }

        else if(regex_match(buffer, helpStr)){
            //TODO: write help response
        }

        if(!userDeclared){
            msg = "> Invalid command. You must enter a username first\n";
            write(client_fd, msg.c_str(), msg.length());
            continue;
        }

        if(regex_match(buffer, joinStr)){

        }




        cout << "USERS:" << endl;
        for(map<string,User>::iterator itr = users.begin(); itr != users.end(); itr++){
            cout << itr->first << endl;
        }
    }
}

void removeUserFromChannel(string user, string channel){

}


#include <iostream>
#include <string>
#include <vector>
#include <regex>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <thread>

using namespace std;

class Object{
private:
    string name;
    vector<Object> members;
};

class User: public Object{
    User(string _name){

    }
};

class Channel: public Object{
    Channel(string _name){
        regex channelStr("#[a-zA-Z][0-9a-zA-Z]*");
    }
};

void* userProcess(int client_fd);

/**************Program wide vars***********/
string password = "";
unsigned short port = 12345;

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
    cout << "Server starting on port: " << port << " with password: " << password << endl;

    while(1){
        pthread_t threadID;
        if(int newUserConnection = accept(server_fd, (struct sockaddr*)&client_addr, &client_len) < 0){
            perror("Error on accept");
            exit(1);
        }
        else {
            send(newUserConnection, "Hello from main", 15, 0);
            cout << "Connected to a user! Sending FD: " << newUserConnection << endl;
            thread newClient(userProcess, newUserConnection);
            newClient.detach();
        }
    }

    return EXIT_SUCCESS;
}

void* userProcess(int client_fd){
    cout << "Hello from the user thread" << endl;

    if(write(client_fd, "Hello there", 11)<0){
        perror("failed to write to child");
    }

    cout << "Write complete" << endl;

    while(1){
        char buffer[512] = {0};

        ssize_t r_val = read(client_fd, buffer, 512);

        if(r_val == 0){
            cout << "Connection closed by user" << endl;
            exit(1);
        }

        if(r_val < 0){
            cout << "Error reading from user" << endl;
            exit(1);
        }

        cout << "Hello there" << endl;
    }
}
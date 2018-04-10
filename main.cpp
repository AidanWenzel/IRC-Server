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

class Channel: public Object{
public:
    Channel(string const &_name, string const &user) : Object(_name){
        members.insert(user);
    }
    void broadcast(string const &msg, map<string, User> users){
        for (const auto &member : members) {
            users.find(member)->second.message(msg);
        }
    }
};

void userProcess(int client_fd);
void removeUserFromChannel(string const &user, string const &channel, bool isKick);
void removeFromAllChannels(string const &user);
void deleteUser(string const &user);

/**************Program wide vars***********/
string password;
unsigned short port = 12345;
mutex channelUsersInUse;
map<string, User> userMap;
map<string, Channel> channelMap;
/******************************************/

int main(int argc, char* argv[]) {
    if(argc == 2){
        regex pass("--opt-pass=[a-zA-Z][_0-9a-zA-Z]*");
        string input = argv[1];
        if(regex_match(input, pass) && input.length() <= 20)
            password = input.substr(11);
        else{
            perror("STARTUP FAILED - Invalid password\n");
            exit(-1);
        }
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
            cout << "Connecting to new client on FD: " << newUserConnection << endl;
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
    regex listStr("LIST.*");
    regex partStr("PART #[a-zA-Z][0-9a-zA-Z]*");
    regex partAllStr("PART");
    regex opStr("OPERATOR .*");
    regex kickStr("KICK #[a-zA-Z][0-9a-zA-Z]* [a-zA-Z][0-9a-zA-Z]*");
    regex msgStr("PRIVMSG #?[a-zA-Z][0-9a-zA-Z]* .*");
    regex quitStr("QUIT");
    regex helpStr("HELP");
    regex channelStr("#[a-zA-Z][0-9a-zA-Z]*");
    /*********************************************/

    if(write(client_fd, "> Welcome to the server. Type HELP for commands\n", 48)<0){
        perror("failed to write to child");
    }

    bool userDeclared = false;
    string userName;
    string msg;
    bool isAdmin = false;

    while(true){
        char buffer[512] = {0};

        ssize_t r_val = read(client_fd, buffer, 512);
        buffer[r_val-1] = '\000';

        if(r_val == 0){
            cout << "Connection closed by user" << endl;
            deleteUser(userName);
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
                    msg = "> Usernames can be no longer than 20 characters\n";
                    write(client_fd, msg.c_str(), msg.length());
                    channelUsersInUse.unlock();
                    continue;
                }

                channelUsersInUse.lock();
                //check if user already exists, if not then allow creation
                if(userMap.find(userName) == userMap.end()){
                    userMap.insert(pair<string,User>(userName, User(userName, client_fd)));
                    msg = "> Welcome, " + userName + "\n";
                    write(client_fd, msg.c_str(), msg.length());
                }
                else{
                    msg = "> That user already exists. Please choose a different username\n";
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
            continue;
        }

        else if(regex_match(buffer, helpStr)){
            msg = "> Command options:\n    USER <nickname>\n    LIST [#channel]\n"
                    "    JOIN <#channel>\n    PART [#channel]\n    OPERATOR <password>\n"
                    "    KICK <#channel> <user>\n    PRIVMSG <#channel>|<user> <msg>\n"
                    "    QUIT\n";
            write(client_fd, msg.c_str(), msg.length());
            continue;
        }

        if(!userDeclared){
            msg = "> Invalid command. You must enter a username first\n";
            write(client_fd, msg.c_str(), msg.length());
            continue;
        }

        //ALL OTHER COMMANDS
        if(regex_match(buffer, joinStr)){
            string target;
            input >> target;

            if(target.length() > 20){
                msg = "> That channel name is too long. Please limit channel names to 20 characters\n";
                write(client_fd, msg.c_str(), msg.length());
                continue;
            }

            channelUsersInUse.lock();
            auto targetChannel = channelMap.find(target);
            if(targetChannel == channelMap.end()){
                //if the channel doesn't exist, create it
                channelMap.insert(pair<string,Channel>(target, Channel(target, userName)));
                userMap.find(userName)->second.members.insert(target);
            } else {
                //join the channel
                targetChannel->second.members.insert(userName);
                userMap.find(userName)->second.members.insert(target);
            }
            channelUsersInUse.unlock();

            msg = "> You have joined channel " + target + "\n";
            write(client_fd, msg.c_str(), msg.length());
        }
        else if(regex_match(buffer, listStr)){
            string target;
            input >> target;

            int count = 0;
            string list;

            channelUsersInUse.lock();
            if(target.length() <= 20 && regex_match(target, channelStr) && channelMap.find(target)!=channelMap.end()){
                for (const auto &member : channelMap.find(target)->second.members) {
                    list.append(" " + member);
                    count++;
                }
                msg = "> There are currently " + to_string(count) + " members.\n";
                if(count > 0){msg.append("    " + target + " members:" + list + "\n");}
            }
            else{
                for(const auto &channelItr : channelMap){
                    list.append("    " + channelItr.first + "\n");
                    count++;
                }
                msg = "> There are currently " + to_string(count) + " channels.\n" + list;
            }
            channelUsersInUse.unlock();

            write(client_fd, msg.c_str(), msg.length());
        }
        else if(regex_match(buffer, partStr) || regex_match(buffer, partAllStr)){
            string target;
            input >> target;
            if(channelMap.find(target)!=channelMap.end()){
                removeUserFromChannel(userName, target, false);
            }
            else if(regex_match(buffer, partAllStr)){
                removeFromAllChannels(userName);
            }
            else{
                msg = "> You are not currently a member of " + target + "\n";
                write(client_fd, msg.c_str(), msg.length());
            }
        }
        else if(regex_match(buffer, opStr)){
            string target;
            input >> target;
            if(target == password){
                isAdmin = true;
                msg = "> OPERATOR status bestowed.\n";
            }
            else {
                msg = "> Invalid OPERATOR command.\n";
            }
            write(client_fd, msg.c_str(), msg.length());
        }
        else if(regex_match(buffer, kickStr)){
            string targetChannel, targetUser;
            input >> targetChannel;
            input >> targetUser;

            if(isAdmin){
                channelUsersInUse.lock();
                if(channelMap.find(targetChannel) == channelMap.end() || userMap.find(targetUser) == userMap.end()){
                    channelUsersInUse.unlock();
                    msg = "> That user is not in that channel";
                    write(client_fd, msg.c_str(), msg.length());
                    continue;
                }
                channelUsersInUse.unlock();
                removeUserFromChannel(targetUser, targetChannel, true);
            }
            else{
                msg = "> You do not have authorization for this command\n";
                write(client_fd, msg.c_str(), msg.length());
            }
        }
        else if(regex_match(buffer, msgStr)){
            string target;
            input >> target;

            channelUsersInUse.lock();
            if(target[0] == '#'){
                auto channel = channelMap.find(target);
                if(channel != channelMap.end() && userMap.find(userName)->second.members.find(target) !=
                                                          userMap.find(userName)->second.members.end()){
                    msg = target;
                    msg.append("> ");
                    msg.append(userName + ": ");
                    msg.append(buffer + 9 + target.length());
                    msg.append("\n");
                    channel->second.broadcast(msg, userMap);
                }
                else{
                    msg = "> You cannot message that target\n";
                    write(client_fd, msg.c_str(), msg.length());
                }
            }
            else{
                auto user = userMap.find(target);
                if(user != userMap.end()){
                    msg = userName;
                    msg.append("> ");
                    msg.append(buffer + 9 + target.length());
                    msg.append("\n");
                    user->second.message(msg);
                }
                else{
                    msg = "> The target of the message does not exist\n";
                    write(client_fd, msg.c_str(), msg.length());
                }
            }
            channelUsersInUse.unlock();
        }
        else if(regex_match(buffer, quitStr)){
            write(client_fd, 0, 0);
            close(client_fd);
            deleteUser(userName);
            return;
        }
        else{
            msg = "> That command was not recognized. Type HELP for a command list\n";
            write(client_fd, msg.c_str(), msg.length());
        }

        cout << "USERS:" << endl;
        for (auto &user : userMap) {
            cout << user.first << endl;
        }
    }
}

void removeUserFromChannel(string const &user, string const &channel, bool isKick){
    channelUsersInUse.lock();
    userMap.find(user)->second.members.erase(channel);
    auto targetChannel = channelMap.find(channel);

    string msg;
    if(!isKick){msg = channel + "> " + user + " has left the channel\n";}
    else{msg = channel + "> " + user + " has been kicked from the channel.";}

    targetChannel->second.broadcast(msg, userMap);
    targetChannel->second.members.erase(user);
    channelUsersInUse.unlock();
}

void removeFromAllChannels(string const &user){
    channelUsersInUse.lock();
    set<string> channelList = userMap.find(user)->second.members;
    channelUsersInUse.unlock();
    for (const auto &itr : channelList) {
        removeUserFromChannel(user, itr, false);
    }
}

void deleteUser(string const &user){
    removeFromAllChannels(user);
    channelUsersInUse.lock();
    userMap.erase(user);
    channelUsersInUse.unlock();
}
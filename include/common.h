#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>

#include <windows.h>
#include <Lmcons.h>
#include <iostream>
using std::cerr;
using std::cin;
using std::cout;
using std::endl;
using std::ifstream;
using std::string;

#define MAXDATASIZE 5000 // max number of bytes we can get at once

struct thread_data
{
    string username;
    int sock_fd;
    struct timeval *start1;
    struct timeval *end1;
    int *bytesRead;
    int *bytesWritten;
    pthread_t client_listener;
    pthread_t server_listener;
};

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

string getUsername()
{
    // get username
    char username[UNLEN + 1];
    DWORD username_len = UNLEN + 1;
    GetUserName((TCHAR *)username, &username_len);
    return username;
}

// void signal_callback_handler(int signum)
// {
//     interrupted = true;
//     // Terminate program
//     exit(signum);
// }
#define _XOPEN_SOURCE 700

#include "../include/common.h"

#define PORT "3490"
#define BACKLOG 10

void sigchild_handler(int s)
{
    int saved_errno = errno;

    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;

    errno = saved_errno;
}

void *server_wait(void *arg)
{
    struct thread_data *td = (struct thread_data *)arg;
    int bytesWritten;
    //buffer to send messages with
    char buf[MAXDATASIZE];

    /*
        wait for data from server
    */
    while (1)
    {
        string data;
        getline(cin, data);
        memset(&buf, 0, sizeof(buf)); //clear the buffer
        strcpy(buf, data.c_str());
        if (data == "exit")
        {
            //send to the client that server has closed the connection
            if (send(td->sock_fd, (char *)&buf, strlen(buf), 0) == -1)
                perror("send exit");
            break;
        }
        //send the message to client
        if ((bytesWritten += send(td->sock_fd, (char *)&buf, strlen(buf), 0)) == -1)
        {
            perror("send");
            exit(1);
        }
    }

    td->bytesWritten = &bytesWritten;
    pthread_cancel(td->client_listener);
    pthread_exit(NULL);
}

void *client_wait(void *arg)
{
    struct thread_data *td = (struct thread_data *)arg;
    int bytesRead;
    //buffer to send messages with
    char buf[MAXDATASIZE];

    /*
        wait for data from client
    */
    //cout << "Awaiting client response..." << endl;
    while (1)
    {
        memset(&buf, 0, sizeof(buf)); //clear the buffer
        if ((bytesRead += recv(td->sock_fd, (char *)&buf, sizeof(buf), 0)) == -1)
        {
            perror("recv");
            exit(1);
        }
        if (!strcmp(buf, "exit"))
        {
            cout << "Client has quit the session" << endl;
            break;
        }
        cout << td->username << ": " << buf << endl;
    }

    td->bytesRead = &bytesRead;
    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    int server_fd, new_fd;
    struct sockaddr_storage their_addr;
    struct addrinfo hints, *server_info, *p;
    socklen_t sin_size;
    struct sigaction sa;
    struct thread_data td;
    int yes = 1;
    char s[INET_ADDRSTRLEN];
    int status;

    // get username
    string username = getUsername();

    // set up signal for when someone cancels program
    // signal(SIGINT, signal_callback_handler);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((status = getaddrinfo(NULL, PORT, &hints, &server_info)) != 0)
    {
        cerr << "getaddrinfo: " << gai_strerror(status) << endl;
        return 1;
    }

    for (p = server_info; p != NULL; p = p->ai_next)
    {
        if ((server_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("server: socket");
            continue;
        }

        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        {
            perror("setsockopt");
            exit(1);
        }

        if (bind(server_fd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(server_fd);
            perror("server: bind");
            continue;
        }

        break;
    }
    freeaddrinfo(server_info);

    if (p == NULL)
    {
        cerr << "server: failed to bind" << endl;
    }

    if (listen(server_fd, BACKLOG) == -1)
    {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchild_handler; // reap dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(1);
    }

    cout << "server: waiting for connections..." << endl;

    while (1)
    {
        sin_size = sizeof(their_addr);
        new_fd = accept(server_fd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1)
        {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof(s));

        cout << "server: got connection from " << s << endl;

        if (!fork()) // this is the child process
        {
            close(server_fd);

            struct timeval start1, end1;
            gettimeofday(&start1, NULL);
            //also keep track of the amount of data sent as well
            int bytesRead, bytesWritten = 0;

            // set up thread data
            td.username = username;
            td.sock_fd = new_fd;
            td.start1 = &start1;
            td.end1 = &end1;
            td.bytesRead = &bytesRead;
            td.bytesWritten = &bytesWritten;

            // using multithreading
            pthread_t client_listener, server_listener;
            if (pthread_create(&client_listener, NULL, client_wait, (void *)&td) == -1)
            {
                perror("Unable to create client listener");
                exit(1);
            }
            td.client_listener = client_listener; // pass client_listener into thread data
            if (pthread_create(&server_listener, NULL, server_wait, (void *)&td) == -1)
            {
                perror("Unable to create server listener");
                exit(1);
            }

            pthread_join(client_listener, NULL); // wait until client thread finishes
            pthread_cancel(server_listener);     // exit server listener thread as well until next connection

            gettimeofday(&end1, NULL);

            //we need to close the new socket descriptor after we're all done
            close(new_fd);

            // print stats of session
            cout << "********Session********" << endl;
            cout << "Bytes written: " << bytesWritten << " Bytes read: " << bytesRead << endl;
            cout << "Elapsed time: " << (end1.tv_sec - start1.tv_sec) << " secs" << endl;
            cout << "Connection closed..." << endl;

            exit(0);
        }

        close(new_fd); // parent doesn't need this
    }

    return 0;
}
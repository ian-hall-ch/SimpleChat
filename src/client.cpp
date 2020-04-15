#include "../include/common.h"

void *server_wait(void *arg)
{
    struct thread_data *td = (struct thread_data *)arg;
    int bytesWritten;
    //buffer to send messages with
    char buf[MAXDATASIZE];

    /*
        wait for data from server
    */
    // cout << "Awaiting server response..." << endl;
    while (1)
    {
        memset(&buf, 0, sizeof(buf)); // clear the buffer
        if ((bytesWritten += recv(td->sock_fd, (char *)&buf, sizeof(buf), 0)) == -1)
        {
            perror("recv");
            exit(1);
        }
        if (!strcmp(buf, "exit"))
        {
            cout << "Server has quit the session" << endl;
            break;
        }
        cout << td->username << ": " << buf << endl;
    }

    td->bytesWritten = &bytesWritten;
    pthread_exit(NULL);
}

void *client_wait(void *arg)
{
    struct thread_data *td = (struct thread_data *)arg;
    int bytesWritten;
    //buffer to send messages with
    char buf[MAXDATASIZE];

    /*
        wait for data from client
    */
    //cout << "Awaiting client response..." << endl;
    while (1)
    {
        string data;
        getline(cin, data);
        memset(&buf, 0, sizeof(buf)); //clear the buffer
        strcpy(buf, data.c_str());
        if (data == "exit")
        {
            if (send(td->sock_fd, (char *)&buf, strlen(buf), 0) == -1)
                perror("send exit");
            break;
        }
        // send message to server
        if ((bytesWritten += send(td->sock_fd, (char *)&buf, strlen(buf), 0)) == -1)
        {
            perror("send");
            exit(1);
        }
    }

    td->bytesWritten = &bytesWritten;
    pthread_cancel(td->server_listener);
    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    int client_fd;
    struct addrinfo hints, *server_info, *p;
    struct thread_data td;
    int rv;
    char s[INET_ADDRSTRLEN];

    if (argc != 3)
    {
        cerr << "usage: client hostname|address port" << endl;
        exit(1);
    }

    // get username
    string username = getUsername();

    // set up signal for when someone cancels program
    // signal(SIGINT, signal_callback_handler);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(argv[1], argv[2], &hints, &server_info)) != 0)
    {
        cerr << "getaddrinfo: " << gai_strerror(rv) << endl;
        return 1;
    }

    // loop through all the results and connect to the first we can
    for (p = server_info; p != NULL; p = p->ai_next)
    {
        if ((client_fd = socket(p->ai_family, p->ai_socktype,
                                p->ai_protocol)) == -1)
        {
            perror("client: socket");
            continue;
        }

        if (connect(client_fd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(client_fd);
            perror("client: connect");
            continue;
        }

        break;
    }
    freeaddrinfo(server_info); // all done with this structure

    if (p == NULL)
    {
        cerr << "client: failed to connect\n"
             << endl;
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
              s, sizeof s);
    cout << "client: connecting to " << s << endl;

    struct timeval start1, end1;
    gettimeofday(&start1, NULL);
    //also keep track of the amount of data sent as well
    int bytesRead, bytesWritten = 0;

    // set up thread data
    td.username = username;
    td.sock_fd = client_fd;
    td.start1 = &start1;
    td.end1 = &end1;
    td.bytesRead = &bytesRead;
    td.bytesWritten = &bytesWritten;

    // using multithreading
    pthread_t client_listener, server_listener;
    if (pthread_create(&server_listener, NULL, server_wait, (void *)&td) == -1)
    {
        perror("Unable to create server listener");
        exit(1);
    }
    td.server_listener = server_listener; // pass server_listener into thread data
    if (pthread_create(&client_listener, NULL, client_wait, (void *)&td) == -1)
    {
        perror("Unable to create client listener");
        exit(1);
    }

    pthread_join(server_listener, NULL); // wait until server thread finishes
    pthread_cancel(client_listener);     // exit client listener thread as well until next connection

    gettimeofday(&end1, NULL);

    //we need to close the client socket descriptor after we're all done
    close(client_fd);

    // print stats of session
    cout << "********Session********" << endl;
    cout << "Bytes written: " << bytesWritten << " Bytes read: " << bytesRead << endl;
    cout << "Elapsed time: " << (end1.tv_sec - start1.tv_sec) << " secs" << endl;
    cout << "Connection closed" << endl;

    return 0;
}
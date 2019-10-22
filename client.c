#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <math.h>
#include <signal.h>
#include <sys/signal.h>
#define MAX 80
#define MIN 80
#define TIMEOUT 4
#define MAX_BUFF_SIZE 1025
#define MAXRECVSTRING 255
#define ADDR "239.255.255.250"
#define BROADCAST_ADDR "239.255.255.251"
#define SA struct sockaddr
#define MAX_PENDING 5

void make_client_socket();

int master_socket;
struct sockaddr_in master_addr;
int addrlen;
int opt = 1;
int broadcast_read_fd, broadcast_write_fd, client_fd, heartbeat_fd, other_client_fd, temp_fd;
struct sockaddr_in client_addr, server_addr, broadcast_read_addr, broadcast_write_addr, otherclient_addr, temp_addr;
char *heartbeat_port;
char *broadcast_port;
char *client_port;
char *server_port;
int connected_to_server = 0;
int connected_to_client = 0;
char wanted_file[MAX];
char target_client[MAX];
char founded_file[MAX];
char file_data_to_be_sent[MAX_BUFF_SIZE];
char file_data_to_be_gotten[MAX_BUFF_SIZE];

int doesServerAlive()
{
    char recvString[MAXRECVSTRING + 1];
    int recvStringLen;
    if ((recvStringLen = recvfrom(heartbeat_fd, recvString, MAXRECVSTRING, 0, NULL, 0)) > 0)
    {
        recvString[recvStringLen] = '\0';
        write(1, "server information :", 21);
        write(1, recvString, strlen(recvString));
        write(1, "\n", 1);
        // printf("%s\n", recvString);
        server_port = recvString;
        // printf("%s\n", server_port);
        // fillServerInformation(recvString);

        ////////////////////////////////////////////////////////
        // close(heartbeat_fd);
        ////////////////////////////////////////////////////////
        // sendLocalStatusesToServer();
        return 1;
    }
    return 0;
}

char *read_to_end(int fd)
{
    ssize_t size, nread, rc;
    char *buf, *tmp;
    nread = 0;
    size = MIN;

    buf = malloc(size);

    while ((rc = read(fd, buf + nread, size - nread)))
    {
        if (rc < 0)
        {
            if (errno == EINTR)
                continue;
            goto error;
        }

        nread += rc;

        /* See if we need to expand. */
        if (size - nread < MIN)
        {
            size *= 2;

            /* Make sure realloc doesn't fail. */
            if (!(tmp = realloc(buf, size)))
                goto error;

            buf = tmp;
        }
    }

    /* Shrink if necessary. */
    if (size != nread && !(tmp = realloc(buf, nread)))
        goto error;

    buf = tmp;
    return buf;

error:
    free(buf);
    return NULL;
}

void connect_to_client()
{
    char buf[256];
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(atoi(target_client));
    // int len;
    // len = write(client_fd, "", 0);
    // printf("%d", len);

    // connect the client socket to server socket
    if (connect(client_fd, (SA *)&server_addr, sizeof(server_addr)) != 0)
    {
        // printf("connection with the client failed...\n");
        write(1, "connection with the client failed...\n", 38);
        exit(0);
    }
    else
        // printf("connected to the client ...\n");
        write(1, "connected to the client ...\n", 29);
}

void connect_to_server()
{
    char buf[256];
    // bzero(buf, 256);
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(atoi(server_port));
    // printf("%s\n", server_port);
    // int len;
    // len = write(client_fd, "", 0);
    // printf("%d", len);

    // connect the client socket to server socket
    if (connect(client_fd, (SA *)&server_addr, sizeof(server_addr)) != 0)
    {
        // printf("connection with the server failed...\n");
        write(1, "connection with the server failed...\n", 38);
        exit(0);
    }
    else
        // printf("connected to the server...\n");
        write(1, "connected to the server...\n", 28);
    // connected_to_server = 1;
    // function for chat
    recv(client_fd, &buf, sizeof(buf), 0);
    // printf("%s\n", buf);
}

void init_client_listener()
{
    // master socket to listen for tcp connections
    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    //type of socket created
    master_addr.sin_family = AF_INET;
    master_addr.sin_addr.s_addr = INADDR_ANY;
    master_addr.sin_port = htons(atoi(client_port));

    //bind the socket to localhost port 8888
    if (bind(master_socket, (struct sockaddr *)&master_addr, sizeof(master_addr)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    // printf("Listener on port %d \n", atoi(client_port));
    write(1, "Listener on port ", 18);
    write(1, client_port, strlen(client_port));
    write(1, "\n", 2);

    //try to specify maximum of 3 pending connections for the master socket
    if (listen(master_socket, MAX_PENDING) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    addrlen = sizeof(master_addr);
    // puts("Waiting for connections ...");
    write(1, "Waiting for connections ...\n", 29);
}

int broadcasting = 0;
char broadcast_msg[MAX_BUFF_SIZE];
int want_to_broadcast = 0;
int accept_signal = 0;
int accept_signal_count = 10;

void send_broadcast_msg()
{
    if (accept_signal)
    {
        if (accept_signal_count > 0)
        {
            if (want_to_broadcast)
            {
                if (sendto(broadcast_write_fd, broadcast_msg, strlen(broadcast_msg), 0, (struct sockaddr *)&broadcast_write_addr, sizeof(broadcast_write_addr)) > -1)
                {
                    if (!broadcasting)
                    {
                        write(1, "client is now broadcasting...\n", 29);
                        broadcasting = 1;
                    }
                }
                accept_signal_count--;
                signal(SIGALRM, send_broadcast_msg);
                alarm(1);
            }
        }
    }
    else
    {
        if (want_to_broadcast)
        {
            if (sendto(broadcast_write_fd, broadcast_msg, strlen(broadcast_msg), 0, (struct sockaddr *)&broadcast_write_addr, sizeof(broadcast_write_addr)) > -1)
            {
                if (!broadcasting)
                {
                    write(1, "client is now broadcasting...\n", 29);
                    broadcasting = 1;
                }
            }

            signal(SIGALRM, send_broadcast_msg);
            alarm(1);
        }
    }
}

fd_set readfds;
int max_sd = 0;
int activity;

void func()
{

    while (1)
    {
        FD_ZERO(&readfds);

        //add master socket to set
        // FD_SET(master_socket, &readfds);
        FD_SET(master_socket, &readfds);
        if (max_sd < master_socket)
            max_sd = master_socket;
        FD_SET(STDIN_FILENO, &readfds);
        if (max_sd < STDIN_FILENO)
            max_sd = STDIN_FILENO;
        FD_SET(broadcast_read_fd, &readfds);
        if (max_sd < broadcast_read_fd)
            max_sd = broadcast_read_fd;

        //wait for an activity on one of the sockets , timeout is NULL ,
        //so wait indefinitely
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0 && errno == EINTR)
        {
            // printf("activity error ... \n");
            continue;
        }

        if (FD_ISSET(master_socket, &readfds))
        {
            if ((other_client_fd = accept(master_socket,
                                          (struct sockaddr *)&otherclient_addr, (socklen_t *)&addrlen)) < 0)
            {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            //inform user of socket number - used in send and receive commands
            // printf("New connection , socket fd is %d , ip is : %s , port : %d  \n ", other_client_fd, inet_ntoa(otherclient_addr.sin_addr), ntohs(otherclient_addr.sin_port));
            write(1, "New connection ... \n", 21);
            int file_fd = open(founded_file, O_RDONLY);
            char *file_data = read_to_end(file_fd);
            printf("%s\n", file_data);
            if (send(other_client_fd, file_data, strlen(file_data), 0) != strlen(file_data))
            {
                perror("send");
            }
            // printf("hoooooraaaaa\n");
            write(1, "hoooooraaaaa\n", 14);
            //add new socket to array of sockets
        }

        if (FD_ISSET(broadcast_read_fd, &readfds))
        {
            int i_want_that = 0;
            char req_file_name[1024] = "";
            char req_client_port[1024] = "";
            // printf("hi from broadcast reader ...\n");
            char recvString[MAXRECVSTRING + 1];
            int recvStringLen;
            if ((recvStringLen = recvfrom(broadcast_read_fd, recvString, MAXRECVSTRING, 0, NULL, 0)) > 0)
            {
                int cnt = 0;
                int other_cnt = 0;
                recvString[recvStringLen] = '\0';
                // printf("%s\n", recvString);
                if (recvString[0] == '$')
                {
                    // printf("hahaha\n");
                    i_want_that = 1;
                    cnt = 1;
                }
                while (recvString[cnt] != '$')
                {
                    req_client_port[other_cnt] = recvString[cnt];
                    other_cnt++;
                    cnt++;
                }
                req_client_port[other_cnt] = '\0';
                // printf("%s\n", req_client_port);
                cnt++;
                other_cnt = 0;
                while (recvString[cnt] != '\n' && recvString[cnt] != '\r' && recvString[cnt] != '\0')
                {
                    req_file_name[other_cnt] = recvString[cnt];
                    other_cnt++;
                    cnt++;
                }
                req_file_name[other_cnt] = '\0';
                // printf("%s\n", req_file_name);
                if (strncmp(req_client_port, client_port, 4) == 0)
                {
                    continue;
                }
                else
                {
                    if (i_want_that)
                    {
                        int req_file_fd = open(req_file_name, O_RDONLY);
                        if (req_file_fd == -1)
                        {
                            // printf("file not available on this client! \n");
                            write(1, "file not available on this client! \n", 37);
                            continue;
                        }
                        // printf("file_found on this client!\n");
                        write(1, "file_found on this client!\n", 28);
                        strcpy(founded_file, req_file_name);
                        strcpy(broadcast_msg, client_port);
                        strcat(broadcast_msg, "$");
                        strcat(broadcast_msg, req_file_name);
                        // printf("%s\n", broadcast_msg);
                        accept_signal_count = 10;
                        accept_signal = 1;
                        want_to_broadcast = 1;
                        send_broadcast_msg();
                        continue;
                    }
                    else
                    {
                        int req_file_fd = open(req_file_name, O_RDONLY);
                        // printf("%d\n", req_file_fd);
                        if (req_file_fd != -1)
                        {
                            // printf("file is already available ...\n");
                            write(1, "file is already available ...\n", 31);
                            want_to_broadcast = 0;
                            continue;
                        }
                        if (strcmp(wanted_file, req_file_name) == 0)
                        {
                            want_to_broadcast = 0;
                            strcpy(target_client, req_client_port);
                            // if (!connected_to_client)
                            // {
                            make_client_socket();
                            connect_to_client();
                            recv(client_fd, &file_data_to_be_gotten, sizeof(file_data_to_be_gotten), 0);
                            // printf("%s\n", file_data_to_be_gotten);
                            int dest_file = open(req_file_name, O_WRONLY | O_CREAT, 0777);
                            int appended_size = write(dest_file, file_data_to_be_gotten, strlen(file_data_to_be_gotten));
                            if (appended_size != strlen(file_data_to_be_gotten))
                            {
                                // printf("appending into file failed\n");
                                write(1, "appending into file failed\n", 28);
                                exit(1);
                            }
                            // printf("download completed   :)))\n");
                            write(1, "download completed   :)))\n", 27);
                            close(client_fd);
                            // connected_to_client = 1;
                            // }
                            continue;
                        }
                        else
                        {
                            continue;
                        }
                    }
                }

                // write(1, "client information :", 21);
                // write(1, recvString, strlen(recvString));
                // write(1, "\n", 1);
                // server_port = recvString;
            }
        }

        /////////////////////////////////////////////////////////////////
        if (FD_ISSET(STDIN_FILENO, &readfds))
        {
            // printf("hello from stdin\n");
            char req_to_server[1024];
            char buff[MAX];
            bzero(buff, sizeof(buff));
            char *file_data;
            char temp[MAX_BUFF_SIZE];
            read(STDIN_FILENO, buff, MAX);
            // printf("%s", buff);
            ///////////////////////////////////////////////////////

            char *command;
            char *file_name;
            int cnt = 0;
            while (buff[cnt] != ' ')
            {
                temp[cnt] = buff[cnt];
                cnt++;
            }
            temp[cnt] = '\0';
            command = temp;
            // printf("%d", strlen(command));
            if (strcmp(command, "upload") == 0)
            {
                // printf("upload started\n");
                write(1, "upload started ...\n", 20);
                int temp_cnt = 0;
                while (buff[cnt] == ' ')
                    cnt++;
                while (buff[cnt] != '\n' && buff[cnt] != '\r')
                {
                    temp[temp_cnt] = buff[cnt];
                    cnt++;
                    temp_cnt++;
                }
                temp[temp_cnt] = '\0';
                file_name = temp;
                // printf("%s", file_name);
                int file_fd = open(file_name, O_RDONLY);
                if (file_fd == -1)
                {
                    // printf("file not available! \n");
                    write(1, "file not available\n", 20);
                    exit(1);
                }
                file_data = read_to_end(file_fd);
                if (doesServerAlive())
                {
                    // if (!connected_to_server)
                    make_client_socket();
                    connect_to_server();
                    // printf("%s", file_data);
                    strcpy(req_to_server, "upload");
                    strcat(req_to_server, "$");
                    strcat(req_to_server, file_name);
                    strcat(req_to_server, "$");
                    strcat(req_to_server, file_data);
                    // printf("%s\n", req_to_server);
                    write(client_fd, req_to_server, sizeof(req_to_server));
                    // bzero(buff, sizeof(buff));
                    // read(sockfd, buff, sizeof(buff));
                    // printf("From Server : %s", buff);
                    // printf("file uploaded to server \n");
                    write(1, "file uploaded to server \n", 26);
                    close(client_fd);
                }
                else
                {
                    // printf("unable to connect to server ...\n");
                    write(1, "unable to connect to server ...\n", 33);
                    // send_broadcast_msg();
                    continue;
                    // return;
                }
            }
            else if (strncmp("download", command, 9) == 0)
            {
                char dl_file[1024];
                // printf("download started\n");
                write(1, "download started\n", 18);
                int temp_cnt = 0;
                while (buff[cnt] == ' ')
                    cnt++;
                while (buff[cnt] != '\n' && buff[cnt] != '\r')
                {
                    temp[temp_cnt] = buff[cnt];
                    cnt++;
                    temp_cnt++;
                }
                temp[temp_cnt] = '\0';
                file_name = temp;
                // printf("%s\n", file_name);
                strcpy(wanted_file, file_name);
                if (doesServerAlive())
                {
                    make_client_socket();
                    connect_to_server();
                    strcpy(req_to_server, "download");
                    strcat(req_to_server, "$");
                    strcat(req_to_server, file_name);
                    // printf("%s\n", req_to_server);
                    write(client_fd, req_to_server, sizeof(req_to_server));
                    bzero(dl_file, sizeof(dl_file));
                    read(client_fd, dl_file, sizeof(dl_file));
                    if (strncmp("NO", dl_file, 3) == 0)
                    {
                        // printf("file not available on server\n");
                        write(1, "file not available on server\n", 30);
                        strcpy(broadcast_msg, "$");
                        strcat(broadcast_msg, client_port);
                        strcat(broadcast_msg, "$");
                        strcat(broadcast_msg, file_name);
                        // printf("%s\n", broadcast_msg);
                        want_to_broadcast = 1;
                        accept_signal = 0;
                        send_broadcast_msg();
                        continue;
                    }
                    // printf("%s\n", dl_file);
                    // printf("end of file \n");
                    int dest_file = open(file_name, O_WRONLY | O_CREAT, 0777);
                    if (dest_file == -1)
                    {
                        // printf("error creating file... \n");
                        write(1, "error creating file... \n", 25);
                        exit(1);
                    }
                    // printf("%ld\n", strlen(dl_file));
                    int appended_size = write(dest_file, dl_file, strlen(dl_file));
                    // printf("hi there\n");
                    if (appended_size != strlen(dl_file))
                    {
                        // printf("appending into file failed\n");
                        write(1, "appending into file failed\n", 28);
                        exit(1);
                    }
                    // printf("file downloaded :)))) \n");
                    write(1, "file downloaded :))) \n", 23);
                    close(client_fd);
                    // printf("From Server : %s\n", dl_file);
                }
                else
                {
                    // printf("unable to connect to server ...\n");
                    write(1, "unable to connect to server ...\n", 33);
                    strcpy(broadcast_msg, "$");
                    strcat(broadcast_msg, client_port);
                    strcat(broadcast_msg, "$");
                    strcat(broadcast_msg, file_name);
                    // printf("%s\n", broadcast_msg);

                    want_to_broadcast = 1;
                    accept_signal = 0;
                    send_broadcast_msg();
                    continue;
                    // return;
                }
                // printf("%s", file_data);
            }

            ///////////////////////////////////////////////////////
        }
    }
}

void make_broadcast_read_socket()
{
    struct sockaddr_in addressOfSocket;
    char recvString[MAXRECVSTRING + 1];
    int recvStringLen;
    //Creating a socket using SOCK_DGRAM for UDP
    if ((broadcast_read_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        write(2, "fail to open socket\n", 21);
        return;
    }
    int broadcast = 1;
    if (setsockopt(broadcast_read_fd, SOL_SOCKET, SO_REUSEADDR, &broadcast, sizeof(broadcast)) == -1)
    {
        write(2, "fail to set broadcast\n", 23);
        return;
    }

    //Defining socket address
    addressOfSocket.sin_family = AF_INET; //ipv4 protocol i think
    addressOfSocket.sin_port = htons(atoi(broadcast_port));
    addressOfSocket.sin_addr.s_addr = htonl(INADDR_ANY);

    // binding server
    if (bind(broadcast_read_fd, (struct sockaddr *)&addressOfSocket, sizeof(addressOfSocket)) < 0)
    {
        write(2, "fail to bind", 13);
        return;
    }

    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(BROADCAST_ADDR);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(broadcast_read_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq)) < 0)
    {
        write(2, "setsockopt", 11);
        return;
    }
}

void make_broadcast_write_socket()
{
    //Creating a socket using SOCK_DGRAM for UDP
    if ((broadcast_write_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        write(1, "fail to open socket", 20);
        return;
    }

    //Defining socket address
    broadcast_write_addr.sin_family = AF_INET; //ipv4 protocol i think
    broadcast_write_addr.sin_port = htons(atoi(broadcast_port));
    broadcast_write_addr.sin_addr.s_addr = inet_addr(BROADCAST_ADDR);
}

void make_hearbeat_socket()
{
    struct sockaddr_in addressOfSocket;
    char recvString[MAXRECVSTRING + 1];
    int recvStringLen;
    //Creating a socket using SOCK_DGRAM for UDP
    if ((heartbeat_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        write(2, "fail to open socket\n", 21);
        return;
    }
    int broadcast = 1;
    if (setsockopt(heartbeat_fd, SOL_SOCKET, SO_REUSEADDR, &broadcast, sizeof(broadcast)) == -1)
    {
        write(2, "fail to set broadcast\n", 23);
        return;
    }

    //Defining socket address
    addressOfSocket.sin_family = AF_INET; //ipv4 protocol i think
    addressOfSocket.sin_port = htons(atoi(heartbeat_port));
    addressOfSocket.sin_addr.s_addr = htonl(INADDR_ANY);

    // binding server
    if (bind(heartbeat_fd, (struct sockaddr *)&addressOfSocket, sizeof(addressOfSocket)) < 0)
    {
        write(2, "fail to bind", 13);
        return;
    }

    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(ADDR);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(heartbeat_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq)) < 0)
    {
        write(2, "setsockopt", 11);
        return;
    }
    float timeout = TIMEOUT;
    struct timeval tv;
    tv.tv_sec = timeout;
    tv.tv_usec = timeout * 1000;
    if (setsockopt(heartbeat_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        write(1, "unable to set timeout", 22);
    }
}

void make_temp_socket()
{
    // make_server_socket();
    temp_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (temp_fd == -1)
    {
        // printf("socket creation failed...\n");
        write(1, "socket creation failed \n", 25);
        exit(0);
    }
    else
        // printf("Socket successfully created..\n");
        write(1, "socket successfully built \n", 28);

    if (setsockopt(temp_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = INADDR_ANY;
    client_addr.sin_port = htons(atoi(client_port));

    //bind the socket to localhost port 8888
    if (bind(temp_fd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // printf("client binded successfully\n");
    write(1, "client binded successfully\n", 28);
}

void make_client_socket()
{
    // make_server_socket();
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd == -1)
    {
        // printf("socket creation failed...\n");
        write(1, "socket creation failed \n", 25);
        exit(0);
    }
    else
        // printf("Socket successfully created..\n");
        write(1, "socket successfully built \n", 28);

    if (setsockopt(client_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    temp_addr.sin_family = AF_INET;
    temp_addr.sin_addr.s_addr = INADDR_ANY;
    temp_addr.sin_port = htons(atoi(client_port));

    //bind the socket to localhost port 8888
    // if (bind(client_fd, (struct sockaddr *)&temp_addr, sizeof(temp_addr)) < 0)
    // {
    //     perror("bind failed");
    //     exit(EXIT_FAILURE);
    // }

    // printf("client binded successfully\n");
    write(1, "client binded successfully\n", 28);
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        // printf("not enough arguments");
        write(1, "not enough arguments\n", 22);
        exit(0);
    }

    heartbeat_port = argv[1];
    broadcast_port = argv[2];
    client_port = argv[3];

    // make_client_socket();
    make_hearbeat_socket();
    make_broadcast_read_socket();
    make_broadcast_write_socket();
    // make_temp_socket();
    init_client_listener();
    // make_client_socket();

    // socket create and varification
    // assign IP, PORT

    func();

    // close the socket
    close(client_fd);
}

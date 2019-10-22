#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <math.h>
#include <signal.h>
#include <sys/signal.h>
#include <fcntl.h>

#define TRUE 1
#define FALSE 0
#define MIN 80

#define MAX_PENDING 5
#define MAX_CLIENTS 30
#define LOCALHOST "127.0.0.1"
#define SA struct sockaddr
#define MAX_SIZE 255
#define MAX_BUFF 1025
#define ADDR "239.255.255.250"
#define LISTEN_PORT "7277"

struct sockaddr_in addressOfHearbeat;
int heartbeatSocketFD;
int heartbeating = 0;

char *hearbeat_port;

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
int opt = TRUE;
int master_socket, addrlen, new_socket, client_socket[MAX_CLIENTS], activity, i, valread, sd;
int max_sd;
struct sockaddr_in address;
char buffer[MAX_BUFF]; //data buffer of 1K
char temp[MAX_BUFF];
//set of socket descriptors
fd_set readfds;
char *message = "ECHO Daemon v1.0 \r\n";
void run_server();

void init_server()
{
    //a message
    //initialise all client_socket[] to 0 so not checked
    for (i = 0; i < MAX_CLIENTS; i++)
    {
        client_socket[i] = 0;
    }

    //create a master socket
    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    //set master socket to allow multiple connections ,
    //this is just a good habit, it will work without this
    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt,
                   sizeof(opt)) < 0)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    //type of socket created
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(atoi(LISTEN_PORT));

    //bind the socket to localhost port 8888
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    // printf("Listener on port %d \n", atoi(LISTEN_PORT));
    write(1, "Listener on port ", 18);
    write(1, LISTEN_PORT, 4);
    write(1, "\n", 2);

    //try to specify maximum of 3 pending connections for the master socket
    if (listen(master_socket, MAX_PENDING) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    //accept the incoming connection
    addrlen = sizeof(address);
    // puts("Waiting for connections ...");
    write(1, "Waiting for connections ...", 28);
    run_server();
}

void run_server()
{
    struct timeval timeout = {0.073, 0};
    while (1)
    {
        //clear the socket set
        FD_ZERO(&readfds);

        //add master socket to set
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;

        //add child sockets to set
        for (i = 0; i < MAX_CLIENTS; i++)
        {
            //socket descriptor
            sd = client_socket[i];

            //if valid socket descriptor then add to read list
            if (sd > 0)
                FD_SET(sd, &readfds);

            //highest file descriptor number, need it for the select function
            if (sd > max_sd)
                max_sd = sd;
        }

        //wait for an activity on one of the sockets , timeout is NULL ,
        //so wait indefinitely
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if ((activity < 0) && (errno == EINTR))
        {
            continue;
        }

        //If something happened on the master socket ,
        //then its an incoming connection
        if (FD_ISSET(master_socket, &readfds))
        {
            if ((new_socket = accept(master_socket,
                                     (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
            {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            //inform user of socket number - used in send and receive commands
            // printf("New connection , socket fd is %d , ip is : %s , port : %d  \n ", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));
            write(1, "new connection \n", 17);
            //send new connection greeting message
            if (send(new_socket, message, strlen(message), 0) != strlen(message))
            {
                perror("send");
            }

            // puts("Welcome message sent successfully");
            write(1, "Welcome message sent successfully", 34);
            //add new socket to array of sockets
            for (i = 0; i < MAX_CLIENTS; i++)
            {
                //if position is empty
                if (client_socket[i] == 0)
                {
                    client_socket[i] = new_socket;
                    // printf("Adding to list of sockets as %d\n", i);
                    write(1, "Adding to list of sockets \n", 28);
                    break;
                }
            }
        }

        //else its some IO operation on some other socket
        for (i = 0; i < MAX_CLIENTS; i++)
        {
            sd = client_socket[i];
            // printf("shit shit shit shit\n");

            if (FD_ISSET(sd, &readfds))
            {
                // printf("io operations");
                //Check if it was for closing , and also read the
                //incoming message
                if ((valread = read(sd, buffer, 1024)) == 0)
                {
                    //Somebody disconnected , get his details and print
                    getpeername(sd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
                    // printf("Host disconnected , ip %s , port %d \n",
                    //    inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                    write(1, "Host disconnected \n", 20);
                    //Close the socket and mark as 0 in list for reuse
                    close(sd);
                    client_socket[i] = 0;
                }

                //Echo back the message that came in
                else
                {
                    // printf("%s\n", buffer);
                    char command[1024];
                    char file_name[1024] = "dest/";
                    char file_data[1024];
                    int cnt = 0, tmp_cnt = 0;
                    while (buffer[cnt] != '$')
                    {
                        temp[cnt] = buffer[cnt];
                        cnt++;
                    }
                    temp[cnt] = '\0';
                    cnt++;
                    strcpy(command, temp);
                    // printf("%s\n", command);
                    if (strncmp("upload", command, 7) == 0)
                    {
                        while (buffer[cnt] != '$')
                        {
                            temp[tmp_cnt] = buffer[cnt];
                            tmp_cnt++;
                            cnt++;
                        }
                        cnt++;
                        temp[tmp_cnt] = '\0';
                        strcat(file_name, temp);
                        // printf("%s\n", file_name);
                        tmp_cnt = 0;
                        while (buffer[cnt] != '\n' && buffer[cnt] != '\r')
                        {
                            temp[tmp_cnt] = buffer[cnt];
                            tmp_cnt++;
                            cnt++;
                        }
                        temp[tmp_cnt] = '\0';
                        strcpy(file_data, temp);
                        // printf("%s\n", file_data);

                        int dest_file = open(file_name, O_WRONLY | O_CREAT, 0777);
                        int appended_size = write(dest_file, file_data, strlen(file_data));
                        if (appended_size != strlen(file_data))
                        {
                            // printf("appending into file failed\n");
                            write(1, "appending into file failed\n", 28);
                            exit(1);
                        }
                        // printf("upload completed   :)))\n");
                        write(1, "upload completed   :)))\n", 25);
                        //set the string terminating NULL byte on the end
                        //of the data read
                    }
                    else if (strncmp("download", command, 9) == 0)
                    {
                        // printf("download running on server\n");
                        write(1, "download running on server\n", 28);
                        char *file_data;
                        while (buffer[cnt] != '\n' && buffer[cnt] != '\r' && buffer[cnt] != '\0')
                        {
                            temp[tmp_cnt] = buffer[cnt];
                            tmp_cnt++;
                            cnt++;
                        }
                        cnt++;
                        temp[tmp_cnt] = '\0';
                        strcat(file_name, temp);
                        // printf("%s\n", file_name);
                        int file_fd = open(file_name, O_RDONLY);
                        if (file_fd == -1)
                        {
                            // printf("file not available! \n");
                            write(1, "file not available! \n", 22);
                            send(sd, "NO", 3, 0);
                            continue;
                        }
                        file_data = read_to_end(file_fd);
                        // printf("%s\n", file_data);
                        send(sd, file_data, strlen(buffer), 0);

                        /////////////////////////////////// closing client

                        // getpeername(sd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
                        // printf("Host disconnected , ip %s , port %d \n",
                        //        inet_ntoa(address.sin_addr), ntohs(address.sin_port));

                        // //Close the socket and mark as 0 in list for reuse
                        // close(sd);
                        // client_socket[i] = 0;
                    }
                    // buffer[valread] = '\0';
                    // send(sd, buffer, strlen(buffer), 0);
                }
            }
        }
    }
}

void initializeHeartbeatSocket()
{
    //Creating a socket using SOCK_DGRAM for UDP
    if ((heartbeatSocketFD = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        write(1, "fail to open socket", 20);
        return;
    }

    //Defining socket address
    addressOfHearbeat.sin_family = AF_INET; //ipv4 protocol i think
    addressOfHearbeat.sin_port = htons(atoi(hearbeat_port));
    addressOfHearbeat.sin_addr.s_addr = inet_addr(ADDR);
}

void sendingHeartbeat()
{
    const char *msg = LISTEN_PORT;
    if (sendto(heartbeatSocketFD, msg, strlen(msg), 0, (struct sockaddr *)&addressOfHearbeat, sizeof(addressOfHearbeat)) > -1)
    {
        if (!heartbeating)
        {
            write(1, "Server is now heartbeating.\n", 29);
            heartbeating = 1;
        }
    }

    signal(SIGALRM, sendingHeartbeat);
    alarm(1);
}

int main(int argc, char *argv[])
{
    // if (argc != 2)
    // {
    //     char error[MAX_SIZE] = "error with parameters\n";
    //     write(1, error, sizeof(error));
    //     exit(0);
    // }
    hearbeat_port = argv[1];
    initializeHeartbeatSocket();
    sendingHeartbeat();
    init_server();

    return 0;
}
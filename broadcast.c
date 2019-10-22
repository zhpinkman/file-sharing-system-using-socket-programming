#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

// #include "Globals.h"

#define HEARTBEAT_MSG "127.0.0.1 8000"
#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT 8000
#define ADDR "239.255.255.250"

struct sockaddr_in addressOfHearbeat;
int heartbeatSocketFD;
extern bool heartbeating;
int serverSocketFD;
int clientSockets[10];
extern int maxNumberOfClients;
int connectReqeusts[10];
char clientDatas[10][1025];
fd_set readFDs; //set of socket descriptors
int activity;   //using to store select

bool heartbeating = false;
int maxNumberOfClients = 10;

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
    addressOfHearbeat.sin_port = htons(8080);
    addressOfHearbeat.sin_addr.s_addr = inet_addr(ADDR);
}

void sendingHeartbeat()
{
    const char *msg = HEARTBEAT_MSG;
    if (sendto(heartbeatSocketFD, msg, strlen(msg), 0, (struct sockaddr *)&addressOfHearbeat, sizeof(addressOfHearbeat)) > -1)
        if (!heartbeating)
        {
            write(1, "Server is now heartbeating.\n", 29);
            heartbeating = true;
        }

    signal(SIGALRM, sendingHeartbeat);
    alarm(1);
}

int main()
{
    initializeHeartbeatSocket();
    sendingHeartbeat();
    while (1)
    {
    }
    return 0;
}
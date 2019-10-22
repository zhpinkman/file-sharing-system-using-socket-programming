#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>

#define MAXRECVSTRING 255
#define ADDR "239.255.255.250"
#define CLIENT_ADDR "127.0.0.1"
#define TIMEOUT 4
#define BROADCAST_TIMEOUT 0.1 // about 5 sec
#define SERVER_CONNECT false
#define BROADCAST_CONNECT true
#define HEARTBEAT true
#define BROADCAST false

char *portForListening;
int broadcastFD;
int clientFD;
int infoTransferFD;
int gameSocketFD;

struct sockaddr_in clientAddress;
struct sockaddr_in serverAddress;
char name[128];
char *opponentName;
int map[10][10];

int broadcastingFD;
bool serverAvailability;
pid_t broadcastPID;
char dataToBroadcast[256];
struct sockaddr_in addressOfBroadcasting;
extern bool broadcasting;

void initializeBroadcastSocket()
{
    struct sockaddr_in addressOfSocket;
    char recvString[MAXRECVSTRING + 1];
    int recvStringLen;
    //Creating a socket using SOCK_DGRAM for UDP
    if ((broadcastFD = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        write(2, "fail to open socket\n", 21);
        return;
    }
    int broadcast = 1;
    if (setsockopt(broadcastFD, SOL_SOCKET, SO_REUSEADDR, &broadcast, sizeof(broadcast)) == -1)
    {
        write(2, "fail to set broadcast\n", 23);
        return;
    }

    //Defining socket address
    addressOfSocket.sin_family = AF_INET; //ipv4 protocol i think
    addressOfSocket.sin_port = htons(8080);
    addressOfSocket.sin_addr.s_addr = htonl(INADDR_ANY);

    // binding server
    if (bind(broadcastFD, (struct sockaddr *)&addressOfSocket, sizeof(addressOfSocket)) < 0)
    {
        write(2, "fail to bind", 13);
        return;
    }

    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(ADDR);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(broadcastFD, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq)) < 0)
    {
        write(2, "setsockopt", 11);
        return;
    }
    float timeout = TIMEOUT;
    struct timeval tv;
    tv.tv_sec = timeout;
    tv.tv_usec = timeout * 1000;
    if (setsockopt(broadcastFD, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        write(1, "unable to set timeout", 22);
    }
}

bool doesServerAlive()
{
    char recvString[MAXRECVSTRING + 1];
    int recvStringLen;
    if ((recvStringLen = recvfrom(broadcastFD, recvString, MAXRECVSTRING, 0, NULL, 0)) > 0)
    {
        recvString[recvStringLen] = '\0';
        write(1, "server information :", 21);
        write(1, recvString, strlen(recvString));
        write(1, "\n", 1);
        // fillServerInformation(recvString);
        // close(broadcastFD);
        // sendLocalStatusesToServer();
        return true;
    }
    return false;
}

int main()
{
    initializeBroadcastSocket();
    doesServerAlive();
    return 0;
}
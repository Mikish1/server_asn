
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

#define BACKLOG 10	 // how many pending connections queue will hold
#define MAXBUFLEN 100

void sigchld_handler(int s)
{
    (void)s; // quiet unused variable warning

    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
    int serverfd;  // fd for connecting to server
    int clientfd, fd2; //fds for connecting to client
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    char s[INET6_ADDRSTRLEN];
    int rv;
    int yes = 1;
    fd_set readfds;
    int n, numbytes1, numbytes2;
    struct timeval tv;
    char buf1[256], buf2[256], temp[40]; //Buffers for messages

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    //Ensure the correct argument amount
    if (argc != 3) {
        fprintf(stderr,"usage: client hostname/port\n");
        exit(1);
    }

    //Get the port number
    char *port = argv[2];


    //Check for valid port number
    if((atoi(port) > 40000 || atoi(port) < 30000)) {
        printf("Invalid port number(s)");
        exit(1);
    }

    //Get server info
    if ((rv = getaddrinfo(argv[1], port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can (server
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((serverfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (connect(serverfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("server: connect");
            close(serverfd);
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    //Get client info
    if ((rv = getaddrinfo(argv[1], port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // Loop and connect to the client
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((clientfd = socket(servinfo->ai_family, servinfo->ai_socktype,
                               servinfo->ai_protocol)) == -1) {
            perror("client: socket");
            continue;

        }

        if (setsockopt(clientfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(clientfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("client: connect");
            close(serverfd);
            continue;
        }
        break;
    }


    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(clientfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    freeaddrinfo(servinfo); // all done with this structure

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
        fd2 = accept(clientfd, (struct sockaddr *) &their_addr, &sin_size); //Accept client connection

        if (fd2 == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *) &their_addr),
                  s, sizeof s);
        printf("Proxy: got connection from %s\n", s);


        while (1) {

            //Reset the buffers, refresh select
            memset(buf1, 0, sizeof(buf1));
            memset(buf2, 0, sizeof(buf2));
            FD_ZERO(&readfds);
            FD_SET(serverfd, &readfds);
            FD_SET(fd2, &readfds);
            n = fd2 + 1;

            tv.tv_sec = 10;
            tv.tv_usec = 500000;
            //Check fds for data to read
            rv = select(n, &readfds, NULL, NULL, &tv);
            if (rv == -1) {
                perror("select"); // error occurred in select()
                close(clientfd);
                close(serverfd);
                exit(0);

            } else if (rv == 0) {
                printf("Timeout occurred! No data after 10.5 seconds.\n");

            } else {
                // one or both of the descriptors have data
                if (FD_ISSET(serverfd, &readfds)) {
                    //Receive data from the server
                    numbytes1 = recv(serverfd, buf1, MAXBUFLEN-1, 0);

                }
                if (FD_ISSET(fd2, &readfds)) {
                    //Receive data from the client
                    numbytes2 = recv(fd2, buf2, MAXBUFLEN-1, 0);

                }
                // If the server has data to send to the client
                if (numbytes1 > 0) {
                    // Search for the substring "cmpt" and replace it with "***" if it exists.
                    memset(temp, 0, sizeof(temp));
                    for (char *tok = strtok(buf1," ,"); tok != NULL; tok = strtok(NULL, " ,")){
                        n = strlen(temp);
                        printf("%s", tok);
                        if(strcmp(tok,"cmpt")==0)
                            sprintf(&temp[n],"*** ");
                        else
                            sprintf(&temp[n],"%s ",tok);
                    }
                    strcpy(buf1, temp);
                    buf1[strlen(buf1)-1] = '\0';

                    //Send the data to the client
                    if ((send(fd2, buf1, strlen((const char *) buf1), 0)) < 0) {
                        perror("Proxy: client");
                        exit(1);
                    }
                }
                // If the client has data to send to the server
                if (numbytes2 > 0) {
                    if ((send(serverfd, buf2, strlen((const char *) buf2), 0)) < 0) {
                        perror("Proxy: server");
                        exit(1);
                    }
                }
            }

        }
    }
}

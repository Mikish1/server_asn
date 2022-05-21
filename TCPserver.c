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

#define MAXBUFLEN (256*sizeof(char))

char *dict[256*sizeof (char)];
int len = 0;

int addKey(char *key, char *value){
    if (len == 40){
        return -1;
    }
    for (int x = 0; x < 40; x+=2){
        if (dict[x] != NULL && strcmp(dict[x], key) == 0){
            return -1;
        }

        if (dict[x] == NULL) {
            dict[x] = (char *) malloc(sizeof key);
            strcpy((dict[x]), key);
            dict[x+1] = (char *) malloc(sizeof value);
            strcpy((dict[x+1]), value);
            len+=2;
            return 1;
        }
    }
    return -1;
}

int removeKey(char *key){
    if (len == 0){
        return -1;
    }
    for (int x = 0; x < 40; x+=2){
        if (dict[x] != NULL && strcmp(dict[x], key) == 0) {
            dict[x] = NULL;
            dict[x+1] = NULL;
            len-=2;
            return 1;
        }
    }
    return -1;
}

char *findKey(char *key){
    if (len == 0){
        return NULL;
    }
    for (int x = 0; x < 40; x+=2) {
        if (dict[x] != NULL && strcmp(dict[x], key) == 0)
            return dict[x+1];
    }
    return NULL;
}

char *getAll(){
    if (len == 0){
        return NULL;
    }
    char *all = malloc(256*sizeof(char));
    all[0]=0;
    for (int x = 0; x < 40; x+=2){
        if (dict[x] == NULL) {
            continue;
        }
        else{
            if (dict[x-1] == NULL){
                strcat(all, "\n");
            }
            strcat(all, dict[x]);
            strcat(all, ", ");
            strcat(all, dict[x+1]);
            if (dict[x+2] != NULL){
                strcat(all, " \n ");
            }
        }
    }
    return all;
}

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
    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    int numbytes;
    char buf[MAXBUFLEN];
    char message[256*sizeof (char)];
    int num = 0;

    if (argc != 2) {
        fprintf(stderr,"usage: server port\n");
        exit(1);
    }

    char *port = argv[1];

    if(atoi(port) > 40000 || atoi(port) < 30000){
        printf("Invalid port number");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

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
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr),
                  s, sizeof s);
        printf("server: got connection from %s\n", s);

        while (1) {
            memset(buf, 0, sizeof(buf));
            numbytes = recv(new_fd, buf, MAXBUFLEN-1, 0);
            memset(message, 0, sizeof(message));
            char *command = strtok(buf, " ");

            if(strcmp(command, "add") == 0){

                char *key = strtok(NULL, " ");
                char *value = strtok(NULL, " ");
                if(num == 20){
                    strcpy(message, "Maximum amount of pairs added.");
                }
                else if(addKey(key, value) == 1){
                    strcpy(message, "Key value pair added.");
                    num++;
                }
                else{
                    strcpy(message, "Failed to add key value pair.");
                }
            }

            else if (strcmp(command, "getvalue") == 0){
                char *key = strtok(NULL, " ");
                char *value = findKey(key);
                if (value == NULL){
                    strcpy(message, "Key was not found: ");
                    strcat(message, key);
                }
                else{
                    strcpy(message, value);
                }
            }

            else if(strcmp(command, "remove") == 0) {
                char * key = strtok(NULL, " ");
                if (removeKey(key) == 1){
                    strcpy(message, "Key value pair Removed.");
                    num--;
                }
                else{
                    strcpy(message, "Failed to remove key value pair.");
                }
            }

            else if(strcmp(command, "getall") == 0){
                char * retall = getAll();
                if(retall == NULL){
                    strcpy(message, "No key value pairs to return.");
                }
                else{
                    strcpy(message, retall);
                }
                printf("%s\n",message);
            }

            else{
                strcpy(message, "Invalid or incorrect command.");
            }

            if (send(new_fd, message, strlen(message), 0) == -1) {
                perror("send");
                close(new_fd);
                exit(0);
            }
            if (numbytes == -1){
                perror("recv failed");
                close(new_fd);
                exit(0);
            }
        }
    }
}
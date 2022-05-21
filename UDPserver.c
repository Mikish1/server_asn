

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>


#define MAXBUFLEN 100

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
            printf("%s", all);
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
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    struct sockaddr_storage their_addr;
    char buf[MAXBUFLEN];
    socklen_t addr_len;
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
    hints.ai_family = AF_INET6; // set to AF_INET to use IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("listener: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }


    while(1) {
        memset(buf, 0, sizeof(buf));
        printf("listener: waiting to recvfrom...\n");
        memset(message, 0, sizeof(message));
        addr_len = sizeof their_addr;
        if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN - 1, 0,
                                 (struct sockaddr *) &their_addr, &addr_len)) == -1) {
            perror("recvfrom");
            exit(1);
        }

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
            char *retall = getAll();
            if(retall == NULL){
                strcpy(message, "No key value pairs to return.");
            }
            else{
                for (int x = 0; x <= len; x+=2) {
                    for (char *tok = strtok(retall, "\n"); tok != NULL; tok = strtok(NULL, "\n")) {
                        memset(message, 0, sizeof(message));
                        strcpy(message, tok);
                        strcat(message, "\n");
                        if ((numbytes = sendto(sockfd, &message, strlen(message), 0,
                                               (struct sockaddr *) &their_addr, p->ai_addrlen)) == -1) {
                            perror("talker: sendto");
                            exit(1);
                        }
                    }
                }
            }
            continue;
        }

        else{
            strcpy(message, "Invalid or incorrect command.");
        }

        if ((numbytes = sendto(sockfd, &message, strlen(message), 0,
                               (struct sockaddr *) &their_addr, p->ai_addrlen)) == -1) {
            perror("talker: sendto");
            exit(1);
        }
    }

}
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#define PORT 8000
#define BUFFERSIZE 4096

struct HttpRequest {
    char version[5]; // e.g. "1.0", "1.1"
    int keepalive;
};

struct HttpRequest* parse_request(char* buffer) {
    static struct HttpRequest httprequest;
    httprequest.keepalive = 0; // set default value

    // parse version
    char* headerstart  = strstr(buffer, "\n");
    char majorversion = *(headerstart - 4);
    char minorversion = *(headerstart - 2);
    snprintf(httprequest.version, 5, "%c.%c", majorversion, minorversion);
    
    // parse keepalive
    char* headerend = strstr(buffer, "\r\n\r\n");
    ssize_t headersize = headerend - headerstart;
    for(int offset = 0; offset < headersize;) {
        char* linestart = headerstart + offset;
        char* lineend = strstr(linestart + 1, "\n");
        int lineoffset = lineend - linestart;
        char* contentstart = linestart + 1;
        char* contentend = strstr(linestart, "\r");
        int contentlen = contentend - contentstart;
        char* line = (char*) malloc(contentlen);
        strncpy(line, contentstart, contentlen);
        char* key = strtok(line, ": ");
        char* value = strtok(NULL, " ");

        if (strcmp(key, "Connection") == 0 && strcmp(value, "keep-alive") == 0) {
            httprequest.keepalive = 1;
        }

        memset(line, '\0', contentlen);
        free(line);
        offset += lineoffset;
    }

    return &httprequest;
}

int main() {
    int s = socket(PF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server, client, proxyserver;

    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");

    proxyserver.sin_family = AF_INET;
    proxyserver.sin_port = htons(3000);
    proxyserver.sin_addr.s_addr = inet_addr("127.0.0.1");

    socklen_t serverlen = sizeof(server);
    socklen_t clientlen = sizeof(client);
    bind(s, (struct sockaddr*) &server, serverlen);
    listen(s, 10);
    printf("server listening on port %d\n", PORT);

    while(1) {
        int clientsocket = accept(s, (struct sockaddr*) &client, &clientlen);
        printf("new connection from (%s, %d)\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
        
        int serversocket = socket(PF_INET, SOCK_STREAM, 0); 
        int connected = connect(serversocket, (struct sockaddr*) &proxyserver, sizeof(proxyserver));
        
        while(connected != -1) {
            char* buffer = malloc(BUFFERSIZE);
            memset(buffer, '\0', BUFFERSIZE);
            int bytes = recv(clientsocket, buffer, BUFFERSIZE, 0);
            if(bytes < 1) break;
            struct HttpRequest* httprequest = parse_request(buffer);
            printf("-> *    %dB\n", bytes);
            sendto(serversocket, buffer, bytes, 0, (struct sockaddr*) &proxyserver, sizeof(proxyserver));
            printf("   * -> %dB\n", bytes);
            while(1) {
               uint8_t* resbuffer = malloc(BUFFERSIZE);
                memset(resbuffer, '\0', BUFFERSIZE);
                int resbytes = recv(serversocket, resbuffer, BUFFERSIZE, 0);
                if(resbytes < 1) break;
                printf("   * <- %dB\n", resbytes);
                sendto(clientsocket, resbuffer, resbytes, 0, (struct sockaddr*) &client, clientlen);
                printf("<- *    %dB\n", resbytes);
                free(resbuffer);
            }
    
             free(buffer);
        } 
        
        close(serversocket);
        close(clientsocket);
    }


    return 1;
}

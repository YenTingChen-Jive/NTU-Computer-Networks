#include<stdio.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<sys/ioctl.h>
#include<net/if.h>
#include<unistd.h> 
#include<string.h>
#include<stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/select.h>
#include <signal.h>

#define BUFF_SIZE 1024
#define PORT 8787
#define ERR_EXIT(a){ perror(a); exit(1); }

#define MAX_CLIENT 10
bool available = false;

typedef struct {
    char host[BUFF_SIZE];        // client's host ???
    int fd;                     // fd to talk with client (client_socket[i])
    char data_buf[BUFF_SIZE];    // data sent by/to client
    size_t buf_len;              // bytes used by buf ???
    int id;                      // ???
    char valid[BUFF_SIZE];
    char filename[BUFF_SIZE];
    char username[BUFF_SIZE];
    char inst[BUFF_SIZE];
} request;

request* requestP = NULL;  // point to a list of requests
int maxfd;  // size of open file descriptor table, size of request list

static void init_request(request* reqP) {
    reqP->fd = -1;
    reqP->buf_len = 0;
    reqP->id = 0;
}

static void free_request(request* reqP) {
    /*if (reqP->filename != NULL) {
        free(reqP->filename);
        reqP->filename = NULL;
    }*/
    init_request(reqP);
}

void *child_function(void *arg) {
    while (1) {
        if (available == false)
            sleep(3);
        else
            break;
    }
    int count = 0;
    // TODO
}

int main(int argc, char *argv[]) {
    int status = mkdir("./server_dir", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    int server_sockfd, client_sockfd;
    int read_byte, write_byte;
    struct sockaddr_in server_addr, client_addr;
    int client_addr_len = sizeof(client_addr);
    char message[BUFF_SIZE] = "Hello World from Server";
    int port = atoi(argv[1]);

    char blocklist[BUFF_SIZE][BUFF_SIZE];
    bzero(blocklist, BUFF_SIZE * BUFF_SIZE);
    int list_ptr = 0;

    // Get socket file descriptor
    if((server_sockfd = socket(AF_INET , SOCK_STREAM , 0)) < 0){
        ERR_EXIT("socket failed\n")
    }

    // Set server address information
    bzero(&server_addr, sizeof(server_addr)); // erase the data
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);
    
    // Bind the server file descriptor to the server address
    if(bind(server_sockfd, (struct sockaddr *)&server_addr , sizeof(server_addr)) < 0){
        ERR_EXIT("bind failed\n");
    }
        
    // Listen on the server file descriptor
    if(listen(server_sockfd , 3) < 0){
        ERR_EXIT("listen failed\n");
    }

    maxfd = getdtablesize();
    requestP = (request*) malloc(sizeof(request) * maxfd);
    if (requestP == NULL) {
        ERR_EXIT("out of memory allocating all requests");
    }
    for (int i = 0; i < maxfd; i++) {
        init_request(&requestP[i]);
    }
    requestP[server_sockfd].fd = server_sockfd;
    // strcpy(requestP[server_sockfd].host, svr.hostname);

    int client_socket[MAX_CLIENT];
    for (int i = 0; i < MAX_CLIENT; i++)
        client_socket[i] = 0;

    int playing_video[MAX_CLIENT][4];
    char playing_video_filename[MAX_CLIENT][BUFF_SIZE];
    for (int i = 0; i < MAX_CLIENT; i++)
        playing_video[i][0] = playing_video[i][1] = playing_video[i][2] = playing_video[i][3] = 0;

    fd_set readset;
    FD_ZERO(&readset);
    fd_set readset_sel;
    FD_ZERO(&readset_sel);
    FD_SET(server_sockfd, &readset);
    pthread_t pid[MAX_CLIENT]; 

    // Block sigpipe
    signal(SIGPIPE, SIG_IGN);

    while (1) {
        char server_buf[BUFF_SIZE];
        bzero(server_buf, BUFF_SIZE);
        int max = server_sockfd;
        for (int i = 0; i < MAX_CLIENT; i++) {
            if (client_socket[i] > 0) {
                FD_SET(client_socket[i], &readset);
                // printf("set client_socket[i] = %d on\n", client_socket[i]);
                if (client_socket[i] > max)
                    max = client_socket[i];
            } 
        }
        // printf("here3, max = %d\n", max);
        readset_sel = readset;
        int current_act = select(max+1, &readset_sel, NULL, NULL, NULL);
        if ((current_act < 0) && (errno != EINTR)) {
            printf("select failed.\n");
        }
        // printf("current_act = %d\n", current_act);
        char server_username[BUFF_SIZE];
        if (FD_ISSET(server_sockfd, &readset_sel)) {
            if((client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, (socklen_t*)&client_addr_len)) < 0){
                ERR_EXIT("accept failed\n");
            }
            bzero(server_buf, BUFF_SIZE);
            bzero(server_username, BUFF_SIZE);
            recv(client_sockfd, server_buf, BUFF_SIZE, 0);
            strncpy(server_username, server_buf, strlen(server_buf));
            printf("Accept a new connection on socket [%d]. Login as %s.\n", client_sockfd, server_username);
            bzero(server_buf, BUFF_SIZE);
            char dirname[] = "./server_dir/";
            strcat(dirname, server_username);
            int ret = mkdir(dirname, 0755);
            int ptr = 0;
            while (ptr < MAX_CLIENT) {
                if (client_socket[ptr] == 0) {
                    client_socket[ptr] = client_sockfd;
                    // printf("Adding to the list of sockets as %d, at %d\n", client_sockfd, ptr);
                    requestP[ptr].fd = client_sockfd;
                    strncpy(requestP[ptr].username, server_username, strlen(server_username));
                    FD_SET(client_sockfd, &readset);
                    break;
                }
                ptr += 1;
            }
        }
        for (int i = 0; i < MAX_CLIENT; i++) {
            if (FD_ISSET(client_socket[i], &readset_sel)) {
                //////// check banned ///////
                bzero(requestP[i].data_buf, BUFF_SIZE);
                bool is_banned = false;
                char permission[BUFF_SIZE];
                char ban[] = "ban";
                char not[] = "not";
                bzero(permission, BUFF_SIZE);
                for (int m = 0; m < list_ptr; m++)
                    if (strcmp(blocklist[m], requestP[i].username) == 0)
                        is_banned = true;

                if (is_banned == true) {
                    // printf("banned.\n");
                    strncpy(requestP[i].data_buf, ban, strlen(ban));
                    write(requestP[i].fd, requestP[i].data_buf, BUFF_SIZE);
                }
                else {
                    // printf("allowed.\n");
                    strncpy(requestP[i].data_buf, not, strlen(ban));
                    write(requestP[i].fd, requestP[i].data_buf, BUFF_SIZE);
                }
                
                /////////////////////////////

                bzero(requestP[i].data_buf, BUFF_SIZE);
                bzero(requestP[i].inst, BUFF_SIZE);
                // printf("Ready to read instruction from %d\n", requestP[i].fd);
                if ((read_byte = read(requestP[i].fd, requestP[i].inst, BUFF_SIZE)) <= 0) { 
                    ERR_EXIT("read instruction failed.\n");
                }
                if (requestP[i].inst[0] == 'l' && requestP[i].inst[1] == 's') {   //////// 
                    ///// ls /////
                    // printf("Executing ls instruction...\n");
                    char dirname[] = "./server_dir/";
                    strcat(dirname, requestP[i].username);
                    // printf("dirname = %s\n", dirname);
                    DIR *fd = opendir(dirname);
                    if (fd == NULL) {
                        ERR_EXIT("ls inst failed (open server_dir).\n");
                    }
                    struct dirent *dir;
                    bzero(requestP[i].data_buf, sizeof(char) * BUFF_SIZE);
                    while ((dir = readdir(fd)) != NULL) {
                        if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
                            // printf("d_name = %s\n", dir->d_name);
                            strncpy(requestP[i].data_buf, dir->d_name, strlen(dir->d_name));
                            send(client_socket[i], requestP[i].data_buf, BUFF_SIZE, 0);
                        }
                        bzero(requestP[i].data_buf, sizeof(char) * BUFF_SIZE);
                    }
                    char end_msg[] = "end";
                    strncpy(requestP[i].data_buf, end_msg, strlen(end_msg));
                    send(client_socket[i], requestP[i].data_buf, BUFF_SIZE, 0);
                    bzero(requestP[i].data_buf, BUFF_SIZE);
                    closedir(fd);
                    // printf("Finished ls instruction...\n\n");
                }
                else if (requestP[i].inst[0] == 'p' && requestP[i].inst[1] == 'u' && requestP[i].inst[2] == 't') {    ////////
                    ///// put /////
                    // printf("Executing put instruction...\n");
                    bzero(requestP[i].filename, BUFF_SIZE);
                    if ((read_byte = read(client_socket[i], requestP[i].filename, BUFF_SIZE)) < 0) {
                        ERR_EXIT("put inst failed (read filename).\n");
                    }
                    bzero(requestP[i].valid, BUFF_SIZE);
                    if ((read_byte = read(client_socket[i], requestP[i].valid, BUFF_SIZE)) < 0) {
                        ERR_EXIT("put inst failed (read valid).\n");
                    }
                    if (requestP[i].valid[0] == 'y' && requestP[i].valid[1] == 'e' && requestP[i].valid[2] == 's') {
                        char pathname[BUFF_SIZE] = "./server_dir/";
                        strcat(pathname, requestP[i].username);
                        pathname[strlen(pathname)] = '/';
                        strcat(pathname, requestP[i].filename);
                        FILE *fp;
                        fp = fopen(pathname, "w");
                        if (fp == NULL) {
                            ERR_EXIT("put inst failed (fopen).\n");
                        }
                        bzero(requestP[i].data_buf, BUFF_SIZE);
                        while(1) {
                            char start_msg[] = "start";
                            char end_msg[] = "end";
                            ///// first handshaking /////
                            if ((read_byte = read(client_socket[i], requestP[i].data_buf, BUFF_SIZE)) < 0) {
                                ERR_EXIT("put inst failed (receiving start_msg)");
                            }
                            // printf("Server receive: %s\n", requestP[i].data_buf);
                            bzero(requestP[i].data_buf, BUFF_SIZE);
                            strncpy(requestP[i].data_buf, start_msg, strlen(start_msg));
                            if ((write_byte = write(client_socket[i], requestP[i].data_buf, strlen(start_msg))) < 0) {
                                ERR_EXIT("put inst failed (sending start_msg).\n");
                            }
                            bzero(requestP[i].data_buf, BUFF_SIZE);
                            // printf("Server send: %s\n", start_msg);
                            ///// data processing /////
                            if ((read_byte = read(client_socket[i], requestP[i].data_buf, BUFF_SIZE)) < 0) {
                                ERR_EXIT("put inst failed (read file from client).\n")
                            }
                            // printf("read_byte = %d, and ", read_byte);
                            if (read_byte == 0)
                                break;
                            if (strcmp(requestP[i].data_buf, "finish") == 0)
                                break;
                            // bzero(requestP[i].data_buf, BUFF_SIZE);
                            if ((write_byte = fwrite(requestP[i].data_buf, sizeof(char), strlen(requestP[i].data_buf), fp)) < 0) {
                                ERR_EXIT("put inst failed (write_byte).\n");
                            }
                            // printf("write_byte = %d\n", write_byte);
                            bzero(requestP[i].data_buf, BUFF_SIZE);
                            ///// second handshaking /////
                            strncpy(requestP[i].data_buf, end_msg, strlen(end_msg));
                            if ((write(client_socket[i], requestP[i].data_buf, strlen(end_msg))) < 0) {
                                ERR_EXIT("put inst failed (sending end_msg)\n");
                            }
                            bzero(requestP[i].data_buf, BUFF_SIZE);
                            // printf("Server send: %s\n", end_msg);
                            if ((read_byte = read(client_socket[i], requestP[i].data_buf, BUFF_SIZE)) < 0) {
                                ERR_EXIT("put inst failed (receiving end_msg)");
                            }
                            // printf("Server receive: %s\n", requestP[i].data_buf);
                            bzero(requestP[i].data_buf, BUFF_SIZE);
                        }
                        fclose(fp);
                        if (is_banned == true) {
                            remove(pathname);
                        }
                    }
                    // printf("Finished put instruction...\n\n");
                }
                else if (requestP[i].inst[0] == 'g' && requestP[i].inst[1] == 'e' && requestP[i].inst[2] == 't') {   ////////
                    ///// get /////
                    // printf("Executing get instruction...\n");
                    bzero(requestP[i].filename, BUFF_SIZE);
                    if ((read_byte = read(client_socket[i], requestP[i].filename, BUFF_SIZE)) < 0) {
                        ERR_EXIT("get inst failed (read filename).\n");
                    }
                    // printf("filename = %s\n", filename);
                    bzero(requestP[i].valid, sizeof(char) * BUFF_SIZE);
                    char pathname[] = "./server_dir/";
                    strcat(pathname, requestP[i].username);
                    DIR *fd = opendir(pathname);
                    if (fd == NULL) {
                        ERR_EXIT("get inst failed (open server_file).\n");
                    }
                    struct dirent *dir;
                    bool found = false;
                    // printf("filename = %s\n", filename);
                    while ((dir = readdir(fd)) != NULL) {
                        if (strcmp(requestP[i].filename, dir->d_name) == 0) {
                            found = true;
                            break;
                        }
                    }
                    closedir(fd);
                    if (found == true) {
                        requestP[i].valid[0] = 'y';
                        requestP[i].valid[1] = 'e';
                        requestP[i].valid[2] = 's';
                    }
                    else {
                        requestP[i].valid[0] = 'n';
                        requestP[i].valid[1] = 'o';
                    }
                    send(client_socket[i], requestP[i].valid, BUFF_SIZE, 0);
                    if (requestP[i].valid[0] == 'y' && requestP[i].valid[1] == 'e' && requestP[i].valid[2] == 's') {
                        char pathname[BUFF_SIZE] = "./server_dir/";
                        strcat(pathname, requestP[i].username);
                        pathname[strlen(pathname)] = '/';
                        strcat(pathname, requestP[i].filename);
                        FILE *fp = fopen(pathname, "r");
                        if (fp == NULL) {
                            ERR_EXIT("get inst failed (fopen).\n");
                        }
                        bzero(requestP[i].data_buf, sizeof(char) * BUFF_SIZE);
                        while(1) {
                            char start_msg[] = "start";
                            char end_msg[] = "end";
                            char finished[] = "finish";
                            ///// first handshaking /////
                            strncpy(requestP[i].data_buf, start_msg, strlen(start_msg));
                            if ((write_byte = write(client_socket[i], requestP[i].data_buf, strlen(start_msg))) < 0) {
                                ERR_EXIT("put inst failed (sending start_msg).\n");
                            }
                            // printf("Client send: %s\n", start_msg);
                            bzero(requestP[i].data_buf, BUFF_SIZE);
                            if ((read_byte = read(client_socket[i], requestP[i].data_buf, BUFF_SIZE)) < 0) {
                                ERR_EXIT("put inst failed (receiving start_msg).\n");
                            }
                            // printf("Client receive: %s\n", data_buf);
                            bzero(requestP[i].data_buf, BUFF_SIZE);
                            ///// data processing /////
                            if ((read_byte = fread(requestP[i].data_buf, sizeof(char), BUFF_SIZE, fp)) < 0) {
                                ERR_EXIT("put inst failed (read_byte).\n");
                            }
                            // printf("read_byte = %d, and ", read_byte);
                            if (read_byte == 0) {
                                strncpy(requestP[i].data_buf, finished, strlen(finished));
                                write(client_socket[i], requestP[i].data_buf, strlen(finished));
                                bzero(requestP[i].data_buf, BUFF_SIZE);
                                // printf("done\n");
                                break;
                            }
                            if ((write_byte = write(client_socket[i], requestP[i].data_buf, read_byte)) < 0) {
                                ERR_EXIT("put inst failed (write_byte).\n");
                            }
                            // printf("write_byte = %d\n", write_byte);
                            bzero(requestP[i].data_buf, BUFF_SIZE);
                            ///// second handshaking /////
                            if ((read_byte = read(client_socket[i], requestP[i].data_buf, BUFF_SIZE)) < 0) {
                                ERR_EXIT("put inst failed (receiving end_msg).\n");
                            }
                            // printf("Client receive: %s\n", data_buf);
                            bzero(requestP[i].data_buf, BUFF_SIZE);
                            strncpy(requestP[i].data_buf, end_msg, strlen(end_msg));
                            if ((write_byte = write(client_socket[i], requestP[i].data_buf, strlen(end_msg))) < 0) {
                                ERR_EXIT("put inst failed (sending end_msg).\n");
                            }
                            bzero(requestP[i].data_buf, BUFF_SIZE);
                            // printf("Client send: %s\n", end_msg);
                        }
                    }
                    // printf("Finished get instruction...\n\n");
                }
                else if (requestP[i].inst[0] == 'p' && requestP[i].inst[1] == 'l' && requestP[i].inst[2] == 'a' && requestP[i].inst[3] == 'y') {     ////////
                    ///// play /////
                    bzero(requestP[i].filename, sizeof(char) * BUFF_SIZE);
                    if ((read_byte = read(client_socket[i], requestP[i].data_buf, BUFF_SIZE)) < 0) {
                        ERR_EXIT("get inst failed (read filename).\n");
                    }
                    char valid[BUFF_SIZE];
                    bzero(valid, sizeof(char) * BUFF_SIZE);
                    // find_file(filename, valid)
                    DIR *fd = opendir("./server_dir");
                    if (fd == NULL) {
                        ERR_EXIT("get inst failed (open server_file).\n");
                    }
                    struct dirent *dir;
                    bool found = false;
                    while ((dir = readdir(fd)) != NULL) {
                        if (strcmp(requestP[i].filename, dir->d_name) == 0) {
                            found = true;
                            break;
                        }
                    }
                    closedir(fd);
                    if (found == true)
                        valid[0] == '1';
                    else
                        valid[0] == '0';
                    send(client_socket[i], requestP[i].data_buf, BUFF_SIZE, 0);
                    if (valid[0] == '1') {
                        printf("Playing video...\n");
                        playing_video[i][2] = client_socket[i];      // client_sockfd ???
                        playing_video[i][3] = i;                     // client_socket[i] ???
                        for (int j = 0; j < strlen(requestP[i].filename); j++)
                            playing_video_filename[i][j] = requestP[i].filename[j];
                        int* input = &i;
                        pthread_create(&pid[i], NULL, child_function, (void*) input);
                    }
                }
                else if (strcmp(requestP[i].inst, "ban") == 0) {
                    // printf("Executing ban instruction...\n");
                    char start_msg[] = "start";
                    char end_msg[] = "end";
                    bzero(requestP[i].data_buf, BUFF_SIZE);
                    read(client_socket[i], requestP[i].data_buf, BUFF_SIZE);
                    while (strcmp(requestP[i].data_buf, "finish") != 0) {
                        ///// first handshaking /////
                        // printf("Server receive: %s\n", data_buf);
                        bzero(requestP[i].data_buf, BUFF_SIZE);
                        strncpy(requestP[i].data_buf, start_msg, strlen(start_msg));
                        if ((write_byte = write(client_socket[i], requestP[i].data_buf, strlen(start_msg))) < 0) {
                            ERR_EXIT("put inst failed (sending start_msg).\n");
                        }
                        bzero(requestP[i].data_buf, BUFF_SIZE);
                        // printf("Server send: %s\n", start_msg);
                        ///// data processing /////
                        read(client_socket[i], requestP[i].data_buf, BUFF_SIZE);
                        // printf("receive %s from client.\n", data_buf);
                        bzero(requestP[i].valid, BUFF_SIZE);
                        if (strcmp(requestP[i].data_buf, "admin") == 0) {
                            requestP[i].valid[0] = 'a';
                            requestP[i].valid[1] = 'd';
                            requestP[i].valid[2] = 'm';
                            requestP[i].valid[3] = 'i';
                            requestP[i].valid[4] = 'n';
                            write(client_socket[i], requestP[i].valid, strlen(requestP[i].valid));
                            // printf("send %s back to client.\n", valid);
                        }
                        else {
                            bool exist = false;
                            for (int k = 0; k < list_ptr; k++) {
                                if (strcmp(blocklist[k], requestP[i].data_buf) == 0) {
                                    exist = true;
                                    break;
                                }
                            }
                            if (exist == true) {
                                requestP[i].valid[0] = 'y';
                                requestP[i].valid[1] = 'e';
                                requestP[i].valid[2] = 's';
                                write(client_socket[i], requestP[i].valid, strlen(requestP[i].valid));
                                // printf("send %s back to client.\n", requestP[i].valid);
                            }
                            else {
                                requestP[i].valid[0] = 'n';
                                requestP[i].valid[1] = 'o';
                                write(client_socket[i], requestP[i].valid, strlen(requestP[i].valid));
                                // printf("send %s back to client.\n", requestP[i].valid);
                                if (strcmp(requestP[i].username, "admin") == 0) {
                                    strncpy(blocklist[list_ptr], requestP[i].data_buf, strlen(requestP[i].data_buf));
                                    list_ptr += 1;    
                                }
                            }
                        }
                        bzero(requestP[i].data_buf, BUFF_SIZE);
                        ///// second handshaking /////
                        if ((read_byte = read(client_socket[i], requestP[i].data_buf, BUFF_SIZE)) < 0) {
                            ERR_EXIT("put inst failed (receiving end_msg).\n");
                        }
                        // printf("Server receive: %s\n", data_buf);
                        bzero(requestP[i].data_buf, BUFF_SIZE);
                        strncpy(requestP[i].data_buf, end_msg, strlen(end_msg));
                        if ((write_byte = write(client_socket[i], requestP[i].data_buf, strlen(end_msg))) < 0) {
                            ERR_EXIT("put inst failed (sending end_msg).\n");
                        }
                        bzero(requestP[i].data_buf, BUFF_SIZE);
                        // printf("Server send: %s\n", end_msg);
                        if ((read_byte = read(client_socket[i], requestP[i].data_buf, BUFF_SIZE)) < 0) {
                            ERR_EXIT("put inst failed (receiving start_msg)");
                        }
                        // printf("Server receive: %s\n", requestP[i].data_buf);
                    }
                    // printf("Finished ban instruction...\n\n");
                }
                else if (strcmp(requestP[i].inst, "unban") == 0) {
                    // printf("Executing unban instruction...\n");
                    char start_msg[] = "start";
                    char end_msg[] = "end";
                    bzero(requestP[i].data_buf, BUFF_SIZE);
                    read(client_socket[i], requestP[i].data_buf, BUFF_SIZE);
                    while (strcmp(requestP[i].data_buf, "finish") != 0) {
                        ///// first handshaking /////
                        // printf("Server receive: %s\n", requestP[i].data_buf);
                        bzero(requestP[i].data_buf, BUFF_SIZE);
                        strncpy(requestP[i].data_buf, start_msg, strlen(start_msg));
                        if ((write_byte = write(client_socket[i], requestP[i].data_buf, strlen(start_msg))) < 0) {
                            ERR_EXIT("put inst failed (sending start_msg).\n");
                        }
                        bzero(requestP[i].data_buf, BUFF_SIZE);
                        // printf("Server send: %s\n", start_msg);
                        ///// data processing /////
                        read(client_socket[i], requestP[i].data_buf, BUFF_SIZE);
                        // printf("receive %s from client.\n", requestP[i].data_buf);
                        bzero(requestP[i].valid, BUFF_SIZE);
                        if (strcmp(requestP[i].data_buf, "admin") == 0) {
                            requestP[i].valid[0] = 'a';
                            requestP[i].valid[1] = 'd';
                            requestP[i].valid[2] = 'm';
                            requestP[i].valid[3] = 'i';
                            requestP[i].valid[4] = 'n';
                            write(client_socket[i], requestP[i].valid, strlen(requestP[i].valid));
                            // printf("send %s back to client.\n", requestP[i].valid);
                        }
                        else {
                            bool exist = false;
                            int pos = 0;
                            // printf("now comparing to %s\n", requestP[i].data_buf);
                            for (int k = 0; k < list_ptr; k++) {
                                if (strcmp(blocklist[k], requestP[i].data_buf) == 0) {
                                    exist = true;
                                    pos = k;
                                    break;
                                }
                            }
                            if (exist == true) {
                                requestP[i].valid[0] = 'y';
                                requestP[i].valid[1] = 'e';
                                requestP[i].valid[2] = 's';
                                write(client_socket[i], requestP[i].valid, strlen(requestP[i].valid));
                                // printf("send %s back to client.\n", requestP[i].valid);
                                if (strcmp(requestP[i].username, "admin") == 0)
                                    bzero(blocklist[pos], BUFF_SIZE);
                            }
                            else {
                                requestP[i].valid[0] = 'n';
                                requestP[i].valid[1] = 'o';
                                write(client_socket[i], requestP[i].valid, strlen(requestP[i].valid));
                                // printf("send %s back to client.\n", requestP[i].valid);
                            }
                        }
                        bzero(requestP[i].data_buf, BUFF_SIZE);
                        ///// second handshaking /////
                        if ((read_byte = read(client_socket[i], requestP[i].data_buf, BUFF_SIZE)) < 0) {
                            ERR_EXIT("put inst failed (receiving end_msg).\n");
                        }
                        // printf("Server receive: %s\n", requestP[i].data_buf);
                        bzero(requestP[i].data_buf, BUFF_SIZE);
                        strncpy(requestP[i].data_buf, end_msg, strlen(end_msg));
                        if ((write_byte = write(client_socket[i], requestP[i].data_buf, strlen(end_msg))) < 0) {
                            ERR_EXIT("put inst failed (sending end_msg).\n");
                        }
                        bzero(requestP[i].data_buf, BUFF_SIZE);
                        // printf("Server send: %s\n", end_msg);
                        if ((read_byte = read(client_socket[i], requestP[i].data_buf, BUFF_SIZE)) < 0) {
                            ERR_EXIT("put inst failed (receiving start_msg)");
                        }
                        // printf("Server receive: %s\n", requestP[i].data_buf);
                    }
                    // printf("Finished unban instruction...\n\n");
                }
                else if (strcmp(requestP[i].inst, "blocklist") == 0) {
                    ///// blocklist /////
                    // printf("Executing blocklist instruction...\n");
                    for (int l = 0; l < list_ptr; l++) {
                        send(client_socket[i], blocklist[l], BUFF_SIZE, 0);
                    }
                    char end_msg[] = "end";
                    bzero(requestP[i].data_buf, BUFF_SIZE);
                    strncpy(requestP[i].data_buf, end_msg, strlen(end_msg));
                    send(client_socket[i], requestP[i].data_buf, strlen(end_msg), 0);
                    bzero(requestP[i].data_buf, BUFF_SIZE);
                    // printf("Finished blocklist instruction...\n\n");
                }
                else {
                    printf("%s: Command not found.\n", requestP[i].inst);
                    bzero(requestP[i].inst, BUFF_SIZE);
                }
                for (int j = 0; j < MAX_CLIENT; j++) {
                    if (playing_video[i][0] == 1 && playing_video[i][1] == 1) {
                        printf("Finish playing video\n");
                        close(playing_video[i][2]);
                        client_socket[playing_video[i][3]] = 0;
                        pthread_join(pid[i], NULL);
                        playing_video[i][0] = playing_video[i][1] = playing_video[i][2] = playing_video[i][3] = 0;
                    }
                }
            }
        }
    }
    close(server_sockfd);
    return 0;
}

int original_main(int argc, char *argv[]){
    int server_sockfd, client_sockfd, read_byte, write_byte;
    struct sockaddr_in server_addr, client_addr;
    int client_addr_len = sizeof(client_addr);
    char buffer[2 * BUFF_SIZE] = {};
    char message[BUFF_SIZE] = "Hello World from Server";

    // Get socket file descriptor
    if((server_sockfd = socket(AF_INET , SOCK_STREAM , 0)) < 0){
        ERR_EXIT("socket failed\n")
    }

    // Set server address information
    bzero(&server_addr, sizeof(server_addr)); // erase the data
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);
    
    // Bind the server file descriptor to the server address
    if(bind(server_sockfd, (struct sockaddr *)&server_addr , sizeof(server_addr)) < 0){
        ERR_EXIT("bind failed\n");
    }
        
    // Listen on the server file descriptor
    if(listen(server_sockfd , 3) < 0){
        ERR_EXIT("listen failed\n");
    }
    
    // Accept the client and get client file descriptor
    if((client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, (socklen_t*)&client_addr_len)) < 0){
        ERR_EXIT("accept failed\n");
    }
    
    sprintf(buffer, "%s\n", message);
    if((write_byte = send(client_sockfd, buffer, strlen(buffer), 0)) < 0){
        ERR_EXIT("write failed\n");
    }

    close(client_sockfd);
    close(server_sockfd);
    return 0;
}

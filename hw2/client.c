#include<sys/socket.h> 
#include<arpa/inet.h>
#include<sys/ioctl.h>
#include<net/if.h>
#include<unistd.h> 
#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <dirent.h>
#include <pthread.h>

// localAddr -> addr
// localSocket = sockfd

#define BUFF_SIZE 1024
#define PORT 8787
#define ERR_EXIT(a){ perror(a); exit(1); }

bool available = false;

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

void playing_video(int sockfd) {}        //TODO

int main(int argc , char *argv[]){
    // int status = mkdir("./client_dir", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);   // permission 775
    int status = mkdir("./client_dir", 0775);
    char inst[BUFF_SIZE];
    char filename[BUFF_SIZE];
    char username[BUFF_SIZE];
    char ip[BUFF_SIZE];
    char port_num[BUFF_SIZE];
    strncpy(username, argv[1], strlen(argv[1]));
    for (int i = 0; i < strlen(argv[2]); i++) {
        if (argv[2][i] == ':') {
            for (int j = 0; j < i; j++)
                ip[j] = argv[2][j];
            for (int j = i + 1; j < strlen(argv[2]); j++)
                port_num[j - i - 1] = argv[2][j];
            break;
        }
    }
    int port = atoi(port_num);
    // printf("Current username = %s, ip = %s, port = %d\n", username, ip, port);
    char data_buf[BUFF_SIZE];

    int sockfd, read_byte, write_byte;
    struct sockaddr_in addr;

    // Get socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        ERR_EXIT("socket failed\n");
    }
    bzero(&addr,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(PORT);

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0){
        ERR_EXIT("connect failed\n");
    }

    printf("User %s successfully logged in.\n", username);
    // printf("sockfd = %d\n", sockfd);
    bzero(data_buf, sizeof(char) * BUFF_SIZE);
    strncpy(data_buf, username, strlen(username));
    send(sockfd, data_buf, BUFF_SIZE, 0);
    // printf("send %s to server.\n", data_buf);
    bzero(data_buf, sizeof(char) * BUFF_SIZE);

    while(1) {
        printf("$ ");

        bzero(inst, sizeof(char) * BUFF_SIZE);
        bzero(filename, sizeof(char) * BUFF_SIZE);
        bzero(data_buf, sizeof(char) * BUFF_SIZE);

        //////// check banned ///////
        /*
        if (read(sockfd, data_buf, BUFF_SIZE) < 0) {
            ERR_EXIT("read state error.\n");
        }
        if (strcmp(data_buf, "ban") == 0) {
            printf("You are BANNED!\n");
        }
        else if (strcmp(data_buf, "not") == 0) {
            printf("You are fine.\n");
        }
        */
        /////////////////////////////

        scanf("%s", inst);

        ////////////////////    inst 1: ls    ////////////////////
        if (inst[0] == 'l' && inst[1] == 's' && inst[2] == '\0') {
            if ((write_byte = write(sockfd, inst, BUFF_SIZE)) < 0) {   // sizeof(char) * BUFF_SIZE
                printf("write_byte = %d\n", write_byte);
                ERR_EXIT("ls inst failed (write_byte).\n");
            }
            // printf("send %s to server from %d\n", inst, sockfd);
            if (read(sockfd, data_buf, BUFF_SIZE) < 0) {
                ERR_EXIT("read state error.\n");
            }
            bool allowed;
            if (strcmp(data_buf, "ban") == 0)
                allowed = false;
            else if (strcmp(data_buf, "not") == 0) 
                allowed = true;
            while (1) {
                bzero(data_buf, BUFF_SIZE);
                if ((read_byte = read(sockfd, data_buf, BUFF_SIZE)) <= 0)
                    break;
                if (strcmp(data_buf, "end") == 0)
                    break;
                if (allowed == true)
                    printf("%s\n", data_buf);
            }
            if (allowed == false)
                printf("Permission denied.\n");
        }
        ////////////////////    inst 2: put    ////////////////////
        else if (inst[0] == 'p' && inst[1] == 'u' && inst[2] == 't' && inst[3] == '\0') {
            scanf("%s", filename);
            if ((write_byte = write(sockfd, inst, BUFF_SIZE)) < 0) {
                ERR_EXIT("put inst failed (inst).\n");
            }
            if ((write_byte = write(sockfd, filename, BUFF_SIZE)) < 0) {
                ERR_EXIT("put inst failed (filename).\n");
            }
            if (read(sockfd, data_buf, BUFF_SIZE) < 0) {
                ERR_EXIT("read state error.\n");
            }
            bool allowed;
            if (strcmp(data_buf, "ban") == 0) {
                // printf("banned\n");
                allowed = false;
            }
            else if (strcmp(data_buf, "not") == 0) {
                // printf("allowed\n");
                allowed = true;
            }
            DIR *fd = opendir("./client_dir");
            struct dirent *dir;
            bool found = false;
            char valid[BUFF_SIZE];
            bzero(valid, BUFF_SIZE);
            if (fd) {
                while ((dir = readdir(fd)) != NULL) {
                    if (strcmp(filename, dir->d_name) == 0) {
                        found = true;
                        break;
                    }
                }
                closedir(fd);
            }
            if (found == true) {
                valid[0] = 'y';
                valid[1] = 'e';
                valid[2] = 's';
            }
            else {
                valid[0] = 'n';
                valid[1] = 'o';
            }
            if ((write_byte = write(sockfd, valid, BUFF_SIZE)) < 0) {
                ERR_EXIT("put inst failed (valid).\n");
            }
            if (allowed == true) {
                if (valid[0] == 'y' && valid[1] == 'e' && valid[2] == 's') {
                    printf("putting %s...\n", filename);
                    char pathname[BUFF_SIZE] = "./client_dir/";
                    strcat(pathname, filename);
                    FILE *fp = fopen(pathname, "r");
                    if (fp == NULL) {
                        ERR_EXIT("put inst failed (fopen).\n");
                    }
                    bzero(data_buf, BUFF_SIZE);
                    while (1) {      // while (!feof(fp))
                        char start_msg[] = "start";
                        char end_msg[] = "end";
                        char finished[] = "finish";
                        ///// first handshaking /////
                        if ((write_byte = write(sockfd, start_msg, strlen(start_msg))) < 0) {
                            ERR_EXIT("put inst failed (sending start_msg).\n");
                        }
                        // printf("Client send: %s\n", start_msg);
                        if ((read_byte = read(sockfd, data_buf, BUFF_SIZE)) < 0) {
                            ERR_EXIT("put inst failed (receiving start_msg).\n");
                        }
                        // printf("Client receive: %s\n", data_buf);
                        bzero(data_buf, BUFF_SIZE);
                        ///// data processing /////
                        if ((read_byte = fread(data_buf, sizeof(char), BUFF_SIZE, fp)) < 0) {
                            ERR_EXIT("put inst failed (read_byte).\n");
                        }
                        // printf("read_byte = %d, and ", read_byte);
                        if (read_byte == 0) {
                            write(sockfd, finished, strlen(finished));
                            // printf("done\n");
                            break;
                        }
                        if ((write_byte = write(sockfd, data_buf, read_byte)) < 0) {
                            ERR_EXIT("put inst failed (write_byte).\n");
                        }
                        // printf("write_byte = %d\n", write_byte);
                        bzero(data_buf, BUFF_SIZE);
                        ///// second handshaking /////
                        if ((read_byte = read(sockfd, data_buf, BUFF_SIZE)) < 0) {
                            ERR_EXIT("put inst failed (receiving end_msg).\n");
                        }
                        // printf("Client receive: %s\n", data_buf);
                        bzero(data_buf, BUFF_SIZE);
                        if ((write_byte = write(sockfd, end_msg, strlen(end_msg))) < 0) {
                            ERR_EXIT("put inst failed (sending end_msg).\n");
                        }
                        // printf("Client send: %s\n", end_msg);
                    }
                }
                else if (valid[0] == 'n' && valid[1] == 'o') {
                    printf("%s doesn't exist.\n", filename);
                }    
            }
            else {
                printf("Permission denied.\n");
                if (valid[0] == 'y' && valid[1] == 'e' && valid[2] == 's') {
                    // printf("putting %s...\n", filename);
                    char pathname[BUFF_SIZE] = "./client_dir/";
                    strcat(pathname, filename);
                    FILE *fp = fopen(pathname, "r");
                    if (fp == NULL) {
                        ERR_EXIT("put inst failed (fopen).\n");
                    }
                    bzero(data_buf, BUFF_SIZE);
                    while (1) {      // while (!feof(fp))
                        char start_msg[] = "start";
                        char end_msg[] = "end";
                        char finished[] = "finish";
                        ///// first handshaking /////
                        if ((write_byte = write(sockfd, start_msg, strlen(start_msg))) < 0) {
                            ERR_EXIT("put inst failed (sending start_msg).\n");
                        }
                        // printf("Client send: %s\n", start_msg);
                        if ((read_byte = read(sockfd, data_buf, BUFF_SIZE)) < 0) {
                            ERR_EXIT("put inst failed (receiving start_msg).\n");
                        }
                        // printf("Client receive: %s\n", data_buf);
                        bzero(data_buf, BUFF_SIZE);
                        ///// data processing /////
                        if ((read_byte = fread(data_buf, sizeof(char), BUFF_SIZE, fp)) < 0) {
                            ERR_EXIT("put inst failed (read_byte).\n");
                        }
                        // printf("read_byte = %d, and ", read_byte);
                        if (read_byte == 0) {
                            write(sockfd, finished, strlen(finished));
                            // printf("done\n");
                            break;
                        }
                        if ((write_byte = write(sockfd, data_buf, read_byte)) < 0) {
                            ERR_EXIT("put inst failed (write_byte).\n");
                        }
                        // printf("write_byte = %d\n", write_byte);
                        bzero(data_buf, BUFF_SIZE);
                        ///// second handshaking /////
                        if ((read_byte = read(sockfd, data_buf, BUFF_SIZE)) < 0) {
                            ERR_EXIT("put inst failed (receiving end_msg).\n");
                        }
                        // printf("Client receive: %s\n", data_buf);
                        bzero(data_buf, BUFF_SIZE);
                        if ((write_byte = write(sockfd, end_msg, strlen(end_msg))) < 0) {
                            ERR_EXIT("put inst failed (sending end_msg).\n");
                        }
                        // printf("Client send: %s\n", end_msg);
                    }
                }
                else if (valid[0] == 'n' && valid[1] == 'o') {
                    // printf("%s doesn't exist.\n", filename);
                }    
            }
        }
        ////////////////////    inst 3: get    ////////////////////
        else if (inst[0] == 'g' && inst[1] == 'e' && inst[2] == 't' && inst[3] == '\0') {
            scanf("%s", filename);
            if ((write_byte = write(sockfd, inst, BUFF_SIZE)) < 0) {
                ERR_EXIT("get inst failed (inst).\n");
            }
            if ((write_byte = write(sockfd, filename, BUFF_SIZE)) < 0) {
                ERR_EXIT("get inst failed (filename).\n");
            }
            if (read(sockfd, data_buf, BUFF_SIZE) < 0) {
                ERR_EXIT("read state error.\n");
            }
            bool allowed;
            if (strcmp(data_buf, "ban") == 0) {
                // printf("banned\n");
                allowed = false;
            }
            else if (strcmp(data_buf, "not") == 0) {
                // printf("allowed\n");
                allowed = true;
            }
            char valid[BUFF_SIZE];
            bzero(valid, BUFF_SIZE);
            if ((read_byte = read(sockfd, valid, BUFF_SIZE)) < 0) {
                ERR_EXIT("get inst failed (valid).\n");
            }
            if (valid[0] == 'y' && valid[1] == 'e' && valid[2] == 's') {
                if (allowed == true)
                    printf("getting %s...\n", filename);
                char pathname[BUFF_SIZE] = "./client_dir/";
                strcat(pathname, filename);
                FILE *fp = fopen(pathname, "w");
                if (fp == NULL) {
                    ERR_EXIT("get inst failed (fopen).\n");
                }
                bzero(data_buf, sizeof(char) * BUFF_SIZE);
                while (1) {
                    char start_msg[] = "start";
                    char end_msg[] = "end";
                    ///// first handshaking /////
                    if ((read_byte = read(sockfd, data_buf, BUFF_SIZE)) < 0) {
                        ERR_EXIT("put inst failed (receiving start_msg)");
                    }
                    // printf("Server receive: %s\n", data_buf);
                    bzero(data_buf, BUFF_SIZE);
                    if ((write_byte = write(sockfd, start_msg, strlen(start_msg))) < 0) {
                        ERR_EXIT("put inst failed (sending start_msg).\n");
                    }
                    // printf("Server send: %s\n", start_msg);
                    ///// data processing /////
                    if ((read_byte = read(sockfd, data_buf, BUFF_SIZE)) < 0) {
                        ERR_EXIT("put inst failed (read file from client).\n")
                    }
                    // printf("read_byte = %d, and ", read_byte);
                    if (read_byte == 0)
                        break;
                    if (strcmp(data_buf, "finish") == 0)
                        break;
                    if ((write_byte = fwrite(data_buf, sizeof(char), strlen(data_buf), fp)) < 0) {
                        ERR_EXIT("put inst failed (write_byte).\n");
                    }
                    // printf("write_byte = %d\n", write_byte);
                    bzero(data_buf, BUFF_SIZE);
                    ///// second handshaking /////
                    if ((write(sockfd, end_msg, strlen(end_msg))) < 0) {
                        ERR_EXIT("put inst failed (sending end_msg)\n");
                    }
                    // printf("Server send: %s\n", end_msg);
                    if ((read_byte = read(sockfd, data_buf, BUFF_SIZE)) < 0) {
                        ERR_EXIT("put inst failed (receiving end_msg)");
                    }
                    // printf("Server receive: %s\n", data_buf);
                    bzero(data_buf, BUFF_SIZE);
                }
                if (allowed == false)
                    remove(pathname);
                fclose(fp);
            }
            else if (valid[0] == 'n' && valid[1] == 'o') {
                if (allowed == true)
                    printf("%s doesn't exist.\n", filename);
            }
            if (allowed == false)
                printf("Permission denied.\n");
        }
        ////////////////////    inst 4: play    ////////////////////
        else if (inst[0] == 'p' && inst[1] == 'l' && inst[2] == 'a' && inst[3] == 'y' && inst[4] == '\0') {
            scanf("%s", filename);
            bzero(data_buf, sizeof(char) * BUFF_SIZE);
            bool found = false;
            for (int i = 0; i < strlen(filename); i++) {
                if (i + 3 < strlen(filename) && filename[i] == '.' && filename[i+1] == 'm' && filename[i+2] == 'p' && filename[i+3] == 'g')
                    found = true;
            }
            if (found == false) {
                char notvalid[] = "not_a_mpg_file";
                strncpy(data_buf, notvalid, strlen(notvalid));
                if ((write_byte = write(sockfd, data_buf, BUFF_SIZE)) < 0) {
                    ERR_EXIT("play inst failed (not found).\n");
                }
            }
            else if (found == true) {
                if ((write_byte = write(sockfd, inst, BUFF_SIZE)) < 0) {
                    ERR_EXIT("play inst failed (found write inst).\n");
                }
                if ((write_byte = write(sockfd, filename, BUFF_SIZE)) < 0) {
                    ERR_EXIT("play inst failed (found write filename).\n")
                }
                char valid[BUFF_SIZE];
                bzero(valid, sizeof(char) * BUFF_SIZE);
                if ((read_byte = read(sockfd, valid, BUFF_SIZE)) < 0) {
                    ERR_EXIT("play inst failed (found read_byte).\n");
                }
                if (valid[0] == '1') {
                    pthread_t child;
                    pthread_create(&child, NULL, child_function, NULL);
                    playing_video(sockfd);        //TODO
                    pthread_join(child, NULL);    //TODO
                }
            }
        }
        ////////////////////    inst 5: ban    ////////////////////
        else if (inst[0] == 'b' && inst[1] == 'a' && inst[2] == 'n' && inst[3] == '\0') {
                if ((write_byte = write(sockfd, inst, BUFF_SIZE)) < 0) {
                    ERR_EXIT("get inst failed (inst).\n");
                }
                if (read(sockfd, data_buf, BUFF_SIZE) < 0) {
                    ERR_EXIT("read state error.\n");
                }
                bool allowed;
                if (strcmp(data_buf, "ban") == 0) {
                    // printf("banned\n");
                    allowed = false;
                }
                else if (strcmp(data_buf, "not") == 0) {
                    // printf("allowed\n");
                    allowed = true;
                }
                char banned[BUFF_SIZE] = {};
                int count = 0;
                char namelist[BUFF_SIZE][BUFF_SIZE];
                char resultlist[BUFF_SIZE][5];
                bzero(namelist, BUFF_SIZE * BUFF_SIZE);
                bzero(resultlist, BUFF_SIZE * 5);
                char start_msg[] = "start";
                char end_msg[] = "end";
                char finished[] = "finish";
                char c;
                bool the_end = false;
                while (the_end == false && scanf("%s", banned) != EOF) {
                    scanf("%c", &c);
                    if (c == '\n')
                        the_end = true;
                    ///// first handshaking /////
                    if ((write_byte = write(sockfd, start_msg, strlen(start_msg))) < 0) {
                        ERR_EXIT("put inst failed (sending start_msg).\n");
                    }
                    // printf("Client send: %s\n", start_msg);
                    if ((read_byte = read(sockfd, data_buf, BUFF_SIZE)) < 0) {
                        ERR_EXIT("put inst failed (receiving start_msg)");
                    }
                    // printf("Client receive: %s\n", data_buf);
                    bzero(data_buf, BUFF_SIZE);
                    strncpy(namelist[count], banned, strlen(banned));
                    ///// data processing /////
                    if ((write_byte = write(sockfd, banned, strlen(banned))) < 0) {
                        ERR_EXIT("ban inst failed (send banned to server).\n");
                    }
                    // printf("send %s to server.\n", banned);
                    bzero(banned, BUFF_SIZE);
                    if ((read_byte = read(sockfd, banned, BUFF_SIZE)) < 0) {
                        ERR_EXIT("ban inst failed (receive result from server).\n");
                    }
                    // printf("receive %s from server.\n", banned);
                    strncpy(resultlist[count], banned, strlen(banned));
                    bzero(banned, BUFF_SIZE);
                    ///// second handshaking /////
                    if ((write(sockfd, end_msg, strlen(end_msg))) < 0) {
                        ERR_EXIT("put inst failed (sending end_msg)\n");
                    }
                    // printf("Client send: %s\n", end_msg);
                    if ((read_byte = read(sockfd, data_buf, BUFF_SIZE)) < 0) {
                        ERR_EXIT("put inst failed (receiving end_msg)");
                    }
                    // printf("Client receive: %s\n", data_buf);
                    bzero(data_buf, BUFF_SIZE);
                    count += 1;
                }
                write(sockfd, finished, strlen(finished));
                // printf("Lastly, client send finished to server.\n");
                if (strcmp(username, "admin") == 0) {
                    for (int k = 0; k < count; k++) {
                        if ((strcmp(namelist[k], "admin") == 0))
                            printf("You cannot ban yourself!\n");
                        else if (strcmp(resultlist[k], "yes") == 0)
                            printf("User %s is already on the blocklist!\n", namelist[k]);
                        else
                            printf("Ban %s successfully!\n", namelist[k]);
                    }
                }
                else if (strcmp(username, "admin") != 0) {
                    printf("Permission denied.\n");
                }
        }
        ////////////////////    inst 6: unban    ////////////////////
        else if (inst[0] == 'u' && inst[1] == 'n' && inst[2] == 'b' && inst[3] == 'a' && inst[4] == 'n' && inst[5] == '\0') {
                if ((write_byte = write(sockfd, inst, BUFF_SIZE)) < 0) {
                    ERR_EXIT("get inst failed (inst).\n");
                }
                if (read(sockfd, data_buf, BUFF_SIZE) < 0) {
                    ERR_EXIT("read state error.\n");
                }
                bool allowed;
                if (strcmp(data_buf, "ban") == 0) {
                    // printf("banned\n");
                    allowed = false;
                }
                else if (strcmp(data_buf, "not") == 0) {
                    // printf("allowed\n");
                    allowed = true;
                }
                char banned[BUFF_SIZE] = {};
                int count = 0;
                char namelist[BUFF_SIZE][BUFF_SIZE];
                char resultlist[BUFF_SIZE][5];
                bzero(namelist, BUFF_SIZE * BUFF_SIZE);
                bzero(resultlist, BUFF_SIZE * 5);
                char start_msg[] = "start";
                char end_msg[] = "end";
                char finished[] = "finish";
                char c;
                bool the_end = false;
                while (the_end == false && scanf("%s", banned) != EOF) {
                    scanf("%c", &c);
                    if (c == '\n')
                        the_end = true;
                    strncpy(namelist[count], banned, strlen(banned));
                    ///// first handshaking /////
                    if ((write_byte = write(sockfd, start_msg, strlen(start_msg))) < 0) {
                        ERR_EXIT("put inst failed (sending start_msg).\n");
                    }
                    // printf("Client send: %s\n", start_msg);
                    if ((read_byte = read(sockfd, data_buf, BUFF_SIZE)) < 0) {
                        ERR_EXIT("put inst failed (receiving start_msg)");
                    }
                    // printf("Client receive: %s\n", data_buf);
                    bzero(data_buf, BUFF_SIZE);
                    ///// data processing /////
                    if ((write_byte = write(sockfd, banned, strlen(banned))) < 0) {
                        ERR_EXIT("ban inst failed (send banned to server).\n");
                    }
                    // printf("send %s to server.\n", banned);
                    bzero(banned, BUFF_SIZE);
                    if ((read_byte = read(sockfd, banned, BUFF_SIZE)) < 0) {
                        ERR_EXIT("ban inst failed (receive result from server).\n");
                    }
                    // printf("receive %s from server.\n", banned);
                    strncpy(resultlist[count], banned, strlen(banned));
                    bzero(banned, BUFF_SIZE);
                    ///// second handshaking /////
                    if ((write(sockfd, end_msg, strlen(end_msg))) < 0) {
                        ERR_EXIT("put inst failed (sending end_msg)\n");
                    }
                    // printf("Client send: %s\n", end_msg);
                    if ((read_byte = read(sockfd, data_buf, BUFF_SIZE)) < 0) {
                        ERR_EXIT("put inst failed (receiving end_msg)");
                    }
                    // printf("Client receive: %s\n", data_buf);
                    bzero(data_buf, BUFF_SIZE);
                    count += 1;
                }
                write(sockfd, finished, strlen(finished));
                // printf("Lastly, client send finished to server.\n");
                if (strcmp(username, "admin") == 0) {
                    for (int k = 0; k < count; k++) {
                        if ((strcmp(namelist[k], "admin") == 0) && (strcmp(resultlist[k], "admin") == 0))
                            printf("User admin is not on the blocklist!\n");
                        else if (strcmp(resultlist[k], "yes") == 0)
                            printf("Successfully removed %s from the blocklist!\n", namelist[k]);
                        else {
                            // printf("resultlist[] = %s\n", resultlist[k]);
                            printf("User %s is not on the blocklist!\n", namelist[k]);
                        } 
                    }    
                }
                else {
                    printf("Permission denied.\n");
                }
                
        }
        ////////////////////    inst 7: blocklist    ////////////////////
        else if (inst[0] == 'b' && inst[1] == 'l' && inst[2] == 'o' && inst[3] == 'c' && inst[4] == 'k' && inst[5] == 'l' && inst[6] == 'i' && inst[7] == 's' && inst[8] == 't' && inst[9] == '\0') {
            if ((write_byte = write(sockfd, inst, BUFF_SIZE)) < 0) {   // sizeof(char) * BUFF_SIZE
                printf("write_byte = %d\n", write_byte);
                ERR_EXIT("blocklist inst failed (write_byte).\n");
            }
            if (read(sockfd, data_buf, BUFF_SIZE) < 0) {
                ERR_EXIT("read state error.\n");
            }
            bool allowed;
            if (strcmp(data_buf, "ban") == 0) {
                // printf("banned\n");
                allowed = false;
            }
            else if (strcmp(data_buf, "not") == 0) {
                // printf("allowed\n");
                allowed = true;
            }
            while (1) {
                bzero(data_buf, BUFF_SIZE);
                if ((read_byte = read(sockfd, data_buf, BUFF_SIZE)) <= 0)
                    break;
                if (strcmp(data_buf, "end") == 0)
                    break;
                if (strlen(data_buf) > 0 && (strcmp(username, "admin") == 0))
                    printf("%s\n", data_buf);
            }
            if (strcmp(username, "admin") != 0)
                printf("Permission denied.\n");
        }
        ////////////////////    others    ////////////////////
        else {
            printf("Command not found.\n");
            bzero(data_buf, sizeof(char) * BUFF_SIZE);
            char notvalid[] = "not_a_valid_instruction";
            strncpy(data_buf, notvalid, strlen(notvalid));
            if ((write_byte = write(sockfd, data_buf, BUFF_SIZE)) < 0) {
                ERR_EXIT("others inst failed (write_byte).\n");
            }
        }
        bzero(data_buf, sizeof(char) * BUFF_SIZE);
        // close(sockfd);
    }
    return 0;
}

int original_main() {
    int sockfd, read_byte;
    struct sockaddr_in addr;
    char buffer[BUFF_SIZE] = {};

    // Get socket file descriptor
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        ERR_EXIT("socket failed\n");
    }

    // Set server address
    bzero(&addr,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(PORT);

    // Connect to the server
    if(connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0){
        ERR_EXIT("connect failed\n");
    }
   
    // Receive message from server
    if((read_byte = read(sockfd, buffer, sizeof(buffer) - 1)) < 0){
        ERR_EXIT("receive failed\n");
    }
    printf("Received %d bytes from the server\n", read_byte);
    printf("%s\n", buffer);
    close(sockfd);
    return 0;
}


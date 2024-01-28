#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <iostream>
#include "opencv2/opencv.hpp"
#include <zlib.h>

#define BUFF_SIZE 256

using namespace std;
using namespace cv; 

typedef struct {
	int length;
	int seqNumber;
	int ackNumber;
	int fin;
	int syn;
	int ack;
    unsigned long checksum;
} HEADER;

typedef struct{
	HEADER header;
	char data[1000];
} SEGMENT;

SEGMENT packet_buf[BUFF_SIZE];
int packet_buf_ptr = 0;
int global_count = 0;
int done_packet = 0;

int receiversocket;
char data_buf[BUFF_SIZE] = {};
SEGMENT s_tmp;
struct sockaddr_in sender, agent, receiver, tmp_addr;
socklen_t sender_size, agent_size, recv_size, tmp_size;
char sendIP[50], agentIP[50], recvIP[50], tmpIP[50];
int sendPort, agentPort, recvPort;

void setIP(char *dst, const char *src){
    if(strcmp(src, "0.0.0.0") == 0 || strcmp(src, "local") == 0 || strcmp(src, "localhost") == 0){
        sscanf("127.0.0.1", "%s", dst);
    }
    else{
        sscanf(src, "%s", dst);
    }
    return;
}

SEGMENT receive_packet_from_agent(int target_packet) {
    ///////////   Step 1: 先從 agent 接收 packet    //////////
    memset(&s_tmp, 0, sizeof(s_tmp));
    recvfrom(receiversocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&agent, &agent_size);
    // printf("### recv a packet from agent ###\n");
    int state;      // 1: 收到packet, 0: 發生buffer overflow, -1: 是fin
    if (s_tmp.header.fin == 1) {
        printf("recv\tfin\n");
        state = -1;
    }
    else if (packet_buf_ptr < BUFF_SIZE) {
        packet_buf[packet_buf_ptr] = s_tmp;
        packet_buf_ptr += 1;
        printf("recv\tdata\t#%d\n", s_tmp.header.seqNumber);
        state = 1;
    }
    else {       // buffer overflow
        printf("drop\tdata\t#%d\t(buffer overflow)\n", s_tmp.header.seqNumber);
        state = 0;
    }

    ///////////   Step 2: 向 agent 回送 ack    //////////
    if (state == -1 || state == 1) {
        SEGMENT s_ack;
        memset(&s_ack, 0, sizeof(s_ack));
        s_ack.header.ack = 1;
        if (target_packet == s_tmp.header.seqNumber) {     // 收到我要的packet
            s_ack.header.ackNumber = s_tmp.header.seqNumber;
            done_packet += 1;
        }
        else if (target_packet > s_tmp.header.seqNumber)   // 收到已經收過的
            s_ack.header.ackNumber = s_tmp.header.seqNumber;
        else                                      // 收到超過目標的
            s_ack.header.ackNumber = done_packet;
        
        if (s_tmp.header.fin == 0) {
            s_ack.header.fin = 0;
            sendto(receiversocket, &s_ack, sizeof(s_ack), 0, (struct sockaddr *)&agent, agent_size);
            printf("send\tack\t#%d\n", s_ack.header.ackNumber);
        }
        else {
            s_ack.header.fin = 1;
            sendto(receiversocket, &s_ack, sizeof(s_ack), 0, (struct sockaddr *)&agent, agent_size);
            printf("send\tfinack\n");
        }
    }
    else {       // buffer overflow
        // 回送重複的ack，並flush掉packet_buf
        SEGMENT s_ack;
        memset(&s_ack, 0, sizeof(s_ack));
        s_ack.header.ack = 1;
        s_ack.header.ackNumber = done_packet;
        s_ack.header.fin = 0;
        sendto(receiversocket, &s_ack, sizeof(s_ack), 0, (struct sockaddr *)&agent, agent_size);
        printf("send\tack\t#%d\n", s_ack.header.ackNumber);

        for (int i = 0; i < packet_buf_ptr; i++)
            memset(&packet_buf[i], 0, sizeof(packet_buf[i]));
        packet_buf_ptr = 0;
    }
    return s_tmp;
}

int main(int argc, char* argv[]){
    /*
    int receiversocket;
    char data_buf[BUFF_SIZE] = {};
    SEGMENT s_tmp;
    struct sockaddr_in sender, agent, receiver, tmp_addr;
    socklen_t sender_size, agent_size, recv_size, tmp_size;
    char sendIP[50], agentIP[50], recvIP[50], tmpIP[50];
    int sendPort, agentPort, recvPort;
    */
    
    if(argc != 3){
        fprintf(stderr,"Usage: %s <receiver port> <agent IP>:<agent port>\n", argv[0]);
        // fprintf(stderr, "E.g., ./agent 8888 local:8887 local:8889 0.3\n");
        exit(1);
    }
    else{
        sscanf(argv[1], "%d", &recvPort);                  // receiver
        setIP(recvIP, "local");

        sscanf(argv[2], "%[^:]:%d", tmpIP, &agentPort);    // agent
        setIP(agentIP, tmpIP);
    }

    /* Create UDP socket */
    receiversocket = socket(PF_INET, SOCK_DGRAM, 0);

    /* Configure settings in agent struct */
    agent.sin_family = AF_INET;
    agent.sin_port = htons(agentPort);
    agent.sin_addr.s_addr = inet_addr(agentIP);
    memset(agent.sin_zero, '\0', sizeof(agent.sin_zero));    

    /* Configure settings in receiver struct */
    receiver.sin_family = AF_INET;
    receiver.sin_port = htons(recvPort);
    receiver.sin_addr.s_addr = inet_addr(recvIP);
    memset(receiver.sin_zero, '\0', sizeof(receiver.sin_zero)); 

    /* bind socket */
    bind(receiversocket,(struct sockaddr *)&receiver,sizeof(receiver));

    /* Initialize size variable to be used later on */
    agent_size = sizeof(agent);
    recv_size = sizeof(receiver);
    tmp_size = sizeof(tmp_addr);

    printf("Receiver is ready. Start!\n");     // TODO: remove this line!
    printf("agent info: ip = %s port = %d\n", agentIP, agentPort);
    printf("receiver info: ip = %s port = %d\n", recvIP, recvPort);

    Mat client_img;
    int width = atoi(receive_packet_from_agent(1).data);
    int length = atoi(receive_packet_from_agent(2).data);
    client_img = Mat::zeros(width, length, CV_8UC3);
    if(!client_img.isContinuous()){
         client_img = client_img.clone();
    }

    while (1) {
        int imgSize = atoi(receive_packet_from_agent(3).data);
        if (imgSize < 1)
            break;

        // Allocate a buffer to load the frame (there would be 2 buffers in the world of the Internet)
        uchar *buffer = (uchar *)malloc(sizeof(uchar) * imgSize);
        bzero(buffer, imgSize);
        int ptr = 0;
        int cycle = imgSize / BUFF_SIZE;
        for (int i = 0; i <= cycle; i++) {
            if (i != cycle) {
                SEGMENT s_rec = receive_packet_from_agent(done_packet + 1);
                memcpy(buffer + ptr, packet_buf[packet_buf_ptr].data, BUFF_SIZE);
                ptr += BUFF_SIZE;
            }
            else {
                int last_size = imgSize % BUFF_SIZE;
                SEGMENT s_rec = receive_packet_from_agent(done_packet + 1);
                memcpy(buffer + ptr, packet_buf[packet_buf_ptr].data, BUFF_SIZE);
                ptr += last_size;
            }
        }

        // Copy a frame from the buffer to the container of the client
        uchar *iptr = client_img.data;
        memcpy(iptr, buffer, imgSize);
      
        // show the frame 
        imshow("Video", client_img);  

        // waitKey function means a delay to get the next frame.
        char c = (char)waitKey(1000);
        free(buffer);
    }
    destroyAllWindows();

    /////   最後，送出finack   /////
    SEGMENT s_finack;
    memset(&s_finack, 0, sizeof(s_finack));
    s_finack.header.ack = 1;
    s_finack.header.fin = 1;
    int state = 0;
    // state = 1, 收到一般的packet;
    // state = 0, buffer overflow
    // state = -1, recv fin
    while (state != -1) {
        memset(&s_tmp, 0, sizeof(s_tmp));
        recvfrom(receiversocket, &s_tmp, sizeof(s_tmp), 0, (struct sockaddr *)&agent, &agent_size);
        if (packet_buf_ptr < BUFF_SIZE) {
            packet_buf[packet_buf_ptr] = s_tmp;
            packet_buf_ptr += 1;
            if (s_tmp.header.fin == 0) {
                printf("recv\tdata\t#%d\n", s_tmp.header.seqNumber);
                state = 1;
            }
            else {
                printf("recv\tfin\n");
                state = -1;
            }
        }
        else {
            if (s_tmp.header.fin == 0) {
                printf("drop\tdata\t#%d\t(buffer overflow)\n", s_tmp.header.seqNumber);
                state = 0;
            }
            else {
                packet_buf[packet_buf_ptr] = s_tmp;
                packet_buf_ptr += 1;
                printf("recv\tfin\n");
                state = -1;
            }
        }

        if (state == -1 || state == 1) {
            SEGMENT s_ack;
            memset(&s_ack, 0, sizeof(s_ack));
            s_ack.header.ack = 1;
            if (done_packet + 1 == s_tmp.header.seqNumber) {     // 收到我要的packet
                s_ack.header.ackNumber = s_tmp.header.seqNumber;
                done_packet += 1;
            }
            else if (done_packet + 1 > s_tmp.header.seqNumber)   // 收到已經收過的
                s_ack.header.ackNumber = s_tmp.header.seqNumber;
            else                                      // 收到超過目標的
                s_ack.header.ackNumber = done_packet;
            
            if (s_tmp.header.fin == 0) {
                s_ack.header.fin = 0;
                sendto(receiversocket, &s_ack, sizeof(s_ack), 0, (struct sockaddr *)&agent, agent_size);
                printf("send\tack\t#%d\n", s_ack.header.ackNumber);
            }
            else {
                s_ack.header.fin = 1;
                sendto(receiversocket, &s_ack, sizeof(s_ack), 0, (struct sockaddr *)&agent, agent_size);
                printf("send\tfinack\n");
            }
        }
        else {       // buffer overflow
            // 回送重複的ack，並flush掉packet_buf
            SEGMENT s_ack;
            memset(&s_ack, 0, sizeof(s_ack));
            s_ack.header.ack = 1;
            s_ack.header.ackNumber = done_packet;
            s_ack.header.fin = 0;
            sendto(receiversocket, &s_ack, sizeof(s_ack), 0, (struct sockaddr *)&agent, agent_size);
            printf("send\tack\t#%d\n", s_ack.header.ackNumber);

            for (int i = 0; i < packet_buf_ptr; i++)
                memset(&packet_buf[i], 0, sizeof(packet_buf[i]));
            packet_buf_ptr = 0;
        }
    }
    for (int i = 0; i < BUFF_SIZE; i++)
        memset(&packet_buf[i], 0, sizeof(packet_buf[i]));
    packet_buf_ptr = 0;

    return 0;

}
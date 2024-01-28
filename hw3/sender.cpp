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
#include <sys/time.h>
#include <stdbool.h>

#define BUFF_SIZE 256      // TODO: How much should BUFF_SIZE be?

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

/*
    Default threshold: 16
    Default window size: 1
    Initial ACK count: 0
    Default time out: 1 sec
*/
int ssthresh = 16;                // threshold
int cwnd = 1;                     // window size
int ACKcount = 0;                 // 當前 window size 下 收到多少 ack 了
struct timeval timeout;
SEGMENT window_buf[32768];        // SEGMENT 的 buffer
int window_buf_ptr = 0;           // SEGMENT 目前使用到的大小
bool need_to_be_resent[32768];    // 是否被resent
bool state[32768];                // index 即是 SEGMENT 的 seqNumber
                    // 1: 是resend, 0: 還沒被send過, -1: 傳完了可以被刷掉了
int global_count = 1;             // 目前新的 packet 應該用的 seqNumber
int done_packet = 0;              // 已經完成全程的packet數

int sendersocket;
char filename[BUFF_SIZE] = {};
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

SEGMENT gen_new_segment(char data[], bool is_final) {
    SEGMENT s_new;
    memset(&s_new, 0, sizeof(s_new));
    memcpy(s_new.data, data, strlen(data));
    s_new.header.length = strlen(data);
    s_new.header.seqNumber = global_count;
    global_count += 1;
    s_new.header.ackNumber = 0;
    s_new.header.fin = (is_final)? 1:0;
    s_new.header.syn = 0;
    s_new.header.checksum = crc32(0L, (const Bytef *)s_new.data, 1000);
    window_buf[window_buf_ptr] = s_new;
    window_buf_ptr += 1;
    state[window_buf_ptr] = 0;

    return s_new;
}

void send_packet_to_agent(char data[], bool is_final) {
    if (ACKcount < cwnd) {
        // window 還沒滿 -> 加新的就好
        gen_new_segment(data, is_final);
        ACKcount += 1;
    }
    else if (ACKcount >= cwnd) {
        // window 滿了 -> 執行 go-back-N
        ///////////   Step 1: 先把 packet 都送出 並印出信息   //////////
        ACKcount = 0;
        // int complete = 0;       // 有幾個已經 send 且也收到 ack了
        for (int i = 0; i < cwnd; i++) {
            sendto(sendersocket, &window_buf[i], sizeof(window_buf[i]), 0, (struct sockaddr *)&agent, agent_size);
            if (window_buf[i].header.fin == 1)
                printf("send\tfin\n");
            else if (need_to_be_resent[i] == false)
                printf("send\tdata\t#%d,\twinSize = %d\n", window_buf[i].header.seqNumber, cwnd);
            else
                printf("resnd\tdata\t#%d,\twinSize = %d\n", window_buf[i].header.seqNumber, cwnd);
            state[i] = -1;
        }

        ///////////   Step 2: 從 agent 接收 ack   //////////
        int repeated_ack = window_buf[0].header.seqNumber;
        for (int i = 0; i < cwnd; i++) {
            SEGMENT s_ack;
            memset(&s_ack, 0, sizeof(s_ack));
            int result = recvfrom(sendersocket, &s_ack, sizeof(s_ack), 0, (struct sockaddr *)&agent, &agent_size);
            bool is_time_out = false;
            if (result > 0) {
                if (s_ack.header.fin == 0) {
                    printf("recv\tack\t#%d\n", s_ack.header.ackNumber);
                    /*
                    printf("--> length = %d\n", s_ack.header.length);
                    printf("--> seqNumber = %d\n", s_ack.header.seqNumber);
                    printf("--> ackNumber = %d\n", s_ack.header.ackNumber);
                    printf("--> fin = %d\n", s_ack.header.fin);
                    printf("--> ack = %d\n", s_ack.header.ack);
                    */   
                }
                else if (s_ack.header.fin == 1)
                    printf("recv\tfinack\n");
                else
                    is_time_out = true;
                if (is_time_out == true) {
                    printf("time\tout,\t\tthreshold = %d\n", ssthresh);
                    break;
                }
            }
            else {
                printf("### didn't recv anything...\n");
            }
                
            if (s_ack.header.ackNumber == repeated_ack) {
                repeated_ack += 1;
                done_packet += 1;
            }
        }

        ///////////   Step 3: 收尾階段   //////////
        // 先把已經處理好的packet移出，並移動剩餘的packet
        for (int i = 0; i < window_buf_ptr; i++) 
            state[cwnd + 1 + i] = 0;
        window_buf_ptr -= done_packet;
        for (int i = 0; i < window_buf_ptr; i++) {
            window_buf[i] = window_buf[done_packet + i];
            state[i] = state[done_packet + i];
        }
        // 更新 window size & threshold
        if (done_packet == cwnd) {
            if (cwnd < ssthresh)
                cwnd *= 2;
            else
                cwnd += 1;
        }
        else {
            if ((cwnd / 2) > 1)
                ssthresh = cwnd / 2;
            else 
                ssthresh = 1;
            cwnd = 1;
        }
        // 生成新的 packet ??
    }
}

int main(int argc, char* argv[]){
    /*
    int sendersocket;
    char data_buf[BUFF_SIZE] = {};
    SEGMENT s_tmp;
    struct sockaddr_in sender, agent, receiver, tmp_addr;
    socklen_t sender_size, agent_size, recv_size, tmp_size;
    char sendIP[50], agentIP[50], recvIP[50], tmpIP[50];
    int sendPort, agentPort, recvPort;
    */
    
    if(argc != 4){
        fprintf(stderr,"Usage: %s <sender port> <agent IP>:<agent port> <.mpg filename>\n", argv[0]);
        // fprintf(stderr, "E.g., ./agent 8888 local:8887 local:8889 0.3\n");
        exit(1);
    }
    else{
        sscanf(argv[1], "%d", &sendPort);                 // sender
        setIP(sendIP, "local");

        sscanf(argv[2], "%[^:]:%d", tmpIP, &agentPort);   // agent
        setIP(agentIP, tmpIP);

        sscanf(argv[3], "%s", &filename);                 // .mpg filename
    }

    /* Create UDP socket */
    sendersocket = socket(PF_INET, SOCK_DGRAM, 0);

    /* Configure settings in sender struct */
    sender.sin_family = AF_INET;
    sender.sin_port = htons(sendPort);
    sender.sin_addr.s_addr = inet_addr(sendIP);
    memset(sender.sin_zero, '\0', sizeof(sender.sin_zero)); 

    /* bind socket */
    bind(sendersocket,(struct sockaddr *)&sender,sizeof(sender));

    /* Configure settings in agent struct */
    agent.sin_family = AF_INET;
    agent.sin_port = htons(agentPort);
    agent.sin_addr.s_addr = inet_addr(agentIP);
    memset(agent.sin_zero, '\0', sizeof(agent.sin_zero)); 

    /* Initialize size variable to be used later on */
    sender_size = sizeof(sender);
    agent_size = sizeof(agent);
    tmp_size = sizeof(tmp_addr);

    printf("Sender is ready. Start!\n");     // TODO: remove this line!
    printf("sender info: ip = %s port = %d\n", sendIP, sendPort);
    printf("agent info: ip = %s port = %d\n", agentIP, agentPort);

    //////////   前置作業：將 socket 設 timout   //////////
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if (setsockopt (sendersocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
        printf("setsockopt failed\n");

    //////////    前置作業：以 OpenCV 打開影片    //////////
    Mat server_img;
    VideoCapture cap(filename);
    // Get the resolution of the video
    int width = cap.get(CAP_PROP_FRAME_WIDTH);
    int height = cap.get(CAP_PROP_FRAME_HEIGHT);
    // cout << "Video resolution: " << width << ", " << height << endl;
    // Allocate container to load frames 
    server_img = Mat::zeros(height, width, CV_8UC3);    
    // Ensure the memory is continuous (for efficiency issue.)
    if(!server_img.isContinuous()){
         server_img = server_img.clone();
    }

    //////////     Part 1: 送 width 跟 length 給 receiver     //////////
    bzero(data_buf, BUFF_SIZE);
    sprintf(data_buf, "%d", width);
    send_packet_to_agent(data_buf, false);
    // printf("### send width to agent ###\n");
    bzero(data_buf, BUFF_SIZE);
    sprintf(data_buf, "%d", height);
    send_packet_to_agent(data_buf, false);
    // printf("### send height to agent ###\n");
    bzero(data_buf, BUFF_SIZE);

    while (1) {
        // Get a frame from the video to the container of the server.
        cap >> server_img;
        // Get the size of a frame in bytes 
        int imgSize = server_img.total() * server_img.elemSize();
        // Send imgSize to receiver
        bzero(data_buf, BUFF_SIZE);
        sprintf(data_buf, "%d", imgSize);
        send_packet_to_agent(data_buf, false);
        if (imgSize < 1)
            break;

        // Allocate a buffer to load the frame (there would be 2 buffers in the world of the Internet)
        uchar *buffer = (uchar *)malloc(sizeof(uchar) * imgSize);
        bzero(buffer, imgSize);
        // Copy a frame to the buffer
        memcpy(buffer, server_img.data, imgSize);
        int ptr = 0;
        int cycle = imgSize / BUFF_SIZE;
        for (int i = 0; i <= cycle; i++) {
            if (i != cycle) {
                char buf_img[BUFF_SIZE] = {};
                memcpy(buf_img, buffer + ptr, BUFF_SIZE);
                send_packet_to_agent(buf_img, false);
                ptr += BUFF_SIZE;
            }
            else {
                int last_size = imgSize % BUFF_SIZE;
                char buf_img[last_size] = {};
                memcpy(buf_img, buffer + ptr, last_size);
                send_packet_to_agent(buf_img, false);
                ptr += last_size;
            }
        }
        free(buffer);
    }
    cap.release();
    /////   播放完影片，將剩下的packet全部送出   /////
    for (int i = 0; i < window_buf_ptr; i++)
        send_packet_to_agent(window_buf[i].data, true);

    ////////////////////   送出fin   ////////////////////
    SEGMENT s_fin;
    memset(&s_fin, 0, sizeof(s_fin));
    s_fin.header.length = 0;
    s_fin.header.seqNumber = global_count;
    s_fin.header.ackNumber = 0;
    s_fin.header.fin = 1;
    s_fin.header.syn = 0;
    s_fin.header.ack = 0;
    s_fin.header.checksum = 0;
    sendto(sendersocket, &s_fin, sizeof(s_fin), 0, (struct sockaddr *)&agent, agent_size);
    printf("send\tfin\n");
    while (1) {
        SEGMENT s_finack;
        memset(&s_finack, 0, sizeof(s_finack));
        recvfrom(sendersocket, &s_finack, sizeof(s_finack), 0, (struct sockaddr *)&agent, &agent_size);
        if (s_finack.header.fin == 1) {
            printf("recv\tfinack\n");
            break;
        }
        else {
            printf("recv\tack\t#%d\n", s_finack.header.ackNumber);
        }
    }
    


}
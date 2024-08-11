#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <pthread.h>
#include <fcntl.h>

typedef struct{
    long n_seg_size;
    char *buf;
    int n_th_seg;
    int from_sender;
}file_info;

typedef struct{
    uint16_t port;
    char ip[16]; 
}r_info;

typedef struct{
    file_info *file_i; // 파일 정보
    int seg_count; // 총 segment 개수
    int r_count; // 총 receiver 수
    int n_th_receiver; // 몇번째 receiver인지
    r_info *each_r; // 각 receiver들 정보가 있는 구조체
    int sockfd[30]; // receiver들 끼리 연결된 sockfd
    int serv_sock; //sender와 연결된 디스크립터
    char f_name[30]; // 파일 이름
    long file_size; // 총 파일 크기
    int count;
    int now_f; // 현재 receiver가 다루고 있는 세그먼트
}receiver;

typedef struct{
    int sockfd[30]; //연결된 소켓들의 디스크립터
    char f_name[30]; // 파일 이름
    file_info *file_i; // 각 파일 segment 정보
    int seg_count; // 총  segement 개수
    long file_size; // 총 파일 크기
    int r_count; // receiver 개수
    r_info *each_r; //각 receiver들의 정보
}sender;


void error_handling(char *buf);
void read_file(size_t seg_size, sender *sender);  //파일 읽고 분리
void send_rinfo(sender *se); // receiver들의 정보 send
void read_rinfo(receiver *re, int sd); //receiver 들의 정보 read
void *recv_sender(void *arg); // receiver에서 다른 receiver들과 연결
void *recv_receiver(void *arg); // receiver에서 다른 receiver들과 연결
void *rr_send(void *arg); // 라운드 로빈 형식으로 파일 전송
void *recv_file(void *arg); // 파일 받기
void *recv_send(void *arg); // 다른 receiver들에게 받은 파일 전송
void *recv_thread(void *arg); // receiver로부터 파일 수신

file_info *result; // 최종 결과 파일을 넣어놓을 구조체
pthread_mutex_t mutex; 
int result_count =0; // 받은 총 segment 개수
size_t total_read=0;

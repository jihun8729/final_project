#include "p2p_server.h"

int main(int agrc, char *argv[]) {
    int serv_sock, clnt_sock;
	struct sockaddr_in serv_adr, clnt_adr;
    pthread_mutex_init(&mutex, NULL);
	socklen_t adr_sz;
    int opt;
    int optval = 1;

    int port_num; //각 port번호

    
    int r_count; //receiver 개수
    char f_name[30]; //파일 이름
    size_t seg_size; //segment 크기

    char ip[20]; // sender ip
    int s_port; // sender 포트번호
    int sock; // receiver가 sender와 연결한 소켓디스크립터
    
    int flag_s = 0, flag_r = 0, flag_n = 0, flag_f = 0,flag_g = 0,flag_a = 0,flag_p = 0;

    while((opt = getopt(agrc, argv, "srnfgap")) != -1)  {
        switch(opt){
            case 's' : flag_s = 1; break;
            case 'r' : flag_r = 1; break;
            case 'n' : flag_n = 1; break;
            case 'f' : flag_f = 1; break;
            case 'g' : flag_g = 1; break;
            case 'a' : flag_a = 1; break;
            case 'p' : flag_p = 1; break;
            case '?' : error_handling("Unknown flag");
        }
    }
    //sender
    if(flag_s) {
        sender sender;
        pthread_t sender_handle;

        if(flag_p)
            port_num = atoi(argv[6]);
        if(flag_n)
            r_count = atoi(argv[7]);
        if(flag_f)
            strcpy(sender.f_name,argv[8]);
        if(flag_g)
            seg_size = atoi(argv[9]);
        
        //파일 읽고 쪼개기
        read_file(seg_size, &sender);

        serv_sock = socket(PF_INET, SOCK_STREAM, 0);
        setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

	    memset(&serv_adr, 0, sizeof(serv_adr));
	    serv_adr.sin_family = AF_INET;
	    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	    serv_adr.sin_port = htons(port_num);

        sender.r_count = r_count;
        sender.each_r = malloc(r_count*sizeof(r_info));


        if (bind(serv_sock, (struct sockaddr*) &serv_adr, sizeof(serv_adr)) == -1)
		    error_handling("bind() error");
	
	    if (listen(serv_sock, 5) == -1)
		    error_handling("listen() error");

       

        for(int i = 0; i < r_count; i++) {
            adr_sz = sizeof(clnt_adr);
            clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &adr_sz);
            sender.sockfd[i] = clnt_sock;
            printf("%d\n",sender.sockfd[i]);
            sender.each_r[i].port = ntohs(clnt_adr.sin_port);
            strcpy(sender.each_r[i].ip,inet_ntoa(clnt_adr.sin_addr));
            
        }

        // 각 receiver 정보 전송
        send_rinfo(&sender);
    

        //sender를 따로 thread로
        pthread_create(&sender_handle,NULL,rr_send,&sender); // thread로 관리
        pthread_join(sender_handle,NULL);
        for(int i=0; i<r_count; i++){
            close(sender.sockfd[i]);
        }

    }
    if(flag_r) {
        struct sockaddr_in my_addr;
        pthread_t recv_server, recv_client, recv_f, recv_recv;
        receiver recv;
        FILE *fp;

        if(flag_p)
            port_num = atoi(argv[4]);
        if(flag_a){
            strcpy(ip,argv[5]);
            s_port = atoi(argv[6]);
        }

        sock = socket(PF_INET, SOCK_STREAM, 0);
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
        memset(&my_addr, 0, sizeof(my_addr));
        my_addr.sin_family = AF_INET;
        my_addr.sin_addr.s_addr = INADDR_ANY;
        my_addr.sin_port = htons(port_num); 
	
         if (bind(sock, (struct sockaddr*)&my_addr, sizeof(my_addr)) == -1) {
            error_handling("bind() error");
        }

        memset(&serv_adr, 0, sizeof(serv_adr));
        serv_adr.sin_family = AF_INET;
        serv_adr.sin_addr.s_addr = inet_addr(ip);
        serv_adr.sin_port = htons(s_port);
        
        

        if (connect(sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
		    error_handling("connect() error");
        recv.serv_sock = sock;
        
        
        //sender로 부터 정보 수신
        read_rinfo(&recv,sock);

       
        // receiver 끼리 연결
        pthread_create(&recv_server,NULL,recv_sender,&recv);
        pthread_join(recv_server,NULL);

        pthread_create(&recv_client,NULL,recv_receiver,&recv);
        pthread_join(recv_client,NULL);
        
        
       
        //sender로부터 파일 수신
        pthread_create(&recv_f,NULL,recv_file,&recv);
        pthread_detach(recv_f);
        
        //receiver로부터 파일 수신
        pthread_create(&recv_recv,NULL,recv_thread,&recv);
        pthread_detach(recv_recv);
        
        while(1) {
            //다 수신한경우 파일 작성
            pthread_mutex_lock(&mutex);
             if(result_count == recv.seg_count){
                printf ("\x1b[%dA\r", 2);
                printf("\x1B[J");
                double read_per = ((total_read*100)/recv.file_size);
                printf("Receiving Peer %d [",recv.n_th_receiver);
                for(int j=0; j<read_per/10; j++){
                    printf("*");
                }
                for(int j=0; j<10-read_per/10; j++){
                    printf(" ");
                }
                printf("] %.f%%(%ld/%ldBytes)\n",read_per,total_read,recv.file_size); 
                fflush(stdout);

                fp=fopen(recv.f_name, "wb");
                for(int i = 0; i < recv.seg_count; i++) { 
                     fwrite((void*)result[i].buf, 1, result[i].n_seg_size, fp);
                }
                fclose(fp);
                result_count++;
                pthread_mutex_unlock(&mutex);
            }
            if(result_count>recv.seg_count){
                for(int i = 0; i< recv.r_count; i++){
                    close(recv.sockfd[i]);
                }
                close(recv.serv_sock);
                break;
            }
            pthread_mutex_unlock(&mutex);
        }
    }
}

void error_handling(char *buf) {
	fputs(buf, stderr);
	fputc('\n', stderr);
	exit(1);
}

void read_file(size_t seg_size, sender *sender) {
    FILE *fp;
    fp = fopen(sender->f_name, "rb");
    
    if (fp == NULL) {
        error_handling("파일 열기 실패");
    }

    //파일 크기 확인
    fseek(fp,0,SEEK_END); 
    sender->file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    printf("%ld\n",sender->file_size);
    
    //segement 개수 확인
    if (sender->file_size % seg_size == 0) {
        sender->seg_count = sender->file_size / seg_size;
    } else {
        sender->seg_count = (sender->file_size / seg_size) + 1;
    }

    //segment 개수만큼 할당
    sender->file_i = malloc(sender->seg_count * sizeof(file_info));
    if (sender->file_i == NULL) {
        error_handling("메모리 할당 실패");
    }

    //각 segment에 값 할당
     for (int count = 0; count < sender->seg_count; count++) {
        sender->file_i[count].n_th_seg = count;
        sender->file_i[count].from_sender = 1;

        //각 segment 내용 크기만큼 할당
        sender->file_i[count].buf = malloc(seg_size);
        if (sender->file_i[count].buf == NULL) {
            error_handling("메모리 할당 실패");
        }

        size_t read_cnt = fread(sender->file_i[count].buf, 1, seg_size, fp);
        sender->file_i[count].n_seg_size = read_cnt;
    }
    fclose(fp);
}

void send_rinfo(sender *se) {
    //리시버에게 기본 정보 전송
    for(int i = 0; i < se->r_count; i++) {
        write(se->sockfd[i], &se->seg_count, sizeof(int));
        write(se->sockfd[i], &se->r_count, sizeof(int)); 
        write(se->sockfd[i], &i, sizeof(int));
        write(se->sockfd[i], &se->f_name, sizeof(se->f_name));
        write(se->sockfd[i], &se->file_size, sizeof(long));

        //리시버에게 각각의 리시버 정보 전송
        for(int j = 0; j < se->r_count; j++) {
            send(se->sockfd[i], &se->each_r[j], sizeof(r_info),0);
        }
    }
}

void read_rinfo(receiver *re, int sd) {

    // 기본 정보 수신
    read(sd, &re->seg_count, sizeof(int));
    read(sd, &re->r_count, sizeof(int));
    read(sd, &re->n_th_receiver, sizeof(int));
    read(sd, &re->f_name, sizeof(re->f_name));
    read(sd, &re->file_size, sizeof(long));
    re->each_r = malloc(re->r_count*sizeof(r_info));
    
    // 최종 결과에 segment 수만큼 할당
    pthread_mutex_lock(&mutex);
    
    result = malloc(re->seg_count*sizeof(file_info));
    if(result == NULL){
        error_handling("malloc fail\n");
    }
    
    pthread_mutex_unlock(&mutex);

    //각 receiver들 정보 수신
    for(int i = 0; i < re->r_count; i++) {
        read(sd, &re->each_r[i], sizeof(r_info));
    }
}

void *recv_sender(void *arg) {
    receiver *re = (receiver *)arg;
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    int optval = 1;
    socklen_t adr_sz;

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = inet_addr(re->each_r[re->n_th_receiver].ip);
	serv_adr.sin_port = htons(re->each_r[re->n_th_receiver].port);
    
    
    if (bind(serv_sock, (struct sockaddr*) &serv_adr, sizeof(serv_adr)) == -1)
		error_handling("bind() error");
	
	if (listen(serv_sock, 5) == -1)
		error_handling("listen() error");

    for(int i = 0; i < re->r_count-re->n_th_receiver - 1; i++) {
        adr_sz = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &adr_sz);
        re->sockfd[re->count] = clnt_sock;
        (re->count)++;
        
        printf("connect receiver : %s %d\n",inet_ntoa(clnt_adr.sin_addr),ntohs(clnt_adr.sin_port));
    }
}

void *recv_receiver(void *arg) {
    receiver *re = (receiver *)arg;
    struct sockaddr_in serv_adr;
    int sd;
    int optval = 1;

    //자기보다 작은애들이랑만 connect
    for(int i = 0; i < re->r_count; i++){
        if(i >= re->n_th_receiver)
            break;
        
        sd = socket(PF_INET, SOCK_STREAM, 0);   
        setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

        memset(&serv_adr, 0, sizeof(serv_adr));
        serv_adr.sin_family = AF_INET;
        serv_adr.sin_addr.s_addr = inet_addr(re->each_r[i].ip);
        serv_adr.sin_port = htons(re->each_r[i].port);
       
        printf("%s %d\n",inet_ntoa(serv_adr.sin_addr),htons(serv_adr.sin_port));
             
        if (connect(sd, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
            error_handling("connect() error!");
        else {
            re->sockfd[re->count] = sd;
            re->count++;
            printf("연결 성공\n");
        }
    }
}
void *rr_send(void *arg) {
    sender *se = (sender *)arg;
    int count=0;
    int seek = -1;
    size_t total_send=0;
    double sen_per=0;
    while(count < se->seg_count){
        printf ("\x1b[%dA\r", 1);
        printf("\x1B[J");
        //round robin 방식으로 전송
        seek = (seek+1) % se->r_count;
        
        write(se->sockfd[seek], &se->file_i[count].n_th_seg, sizeof(int));
        write(se->sockfd[seek], &se->file_i[count].n_seg_size, sizeof(size_t));
        write(se->sockfd[seek], &se->file_i[count].from_sender, sizeof(int));
        write(se->sockfd[seek], se->file_i[count].buf, se->file_i[count].n_seg_size);
        total_send+=se->file_i[count].n_seg_size;
        count++;

        sen_per = ((total_send*100)/se->file_size);
        
        printf("Sending Peer [");
        for(int j=0; j<sen_per/10; j++){
            printf("*");
        }
        for(int j=0; j<10-sen_per/10; j++){
            printf(" ");
        }
        printf("] %.f%%(%ld/%ldBytes)\n",sen_per,total_send,se->file_size); 
		fflush(stdout);

    }
    printf("전송 완료\n");
    
}

void *recv_file(void *arg) {
    receiver *re = (receiver *)arg;
    int location;
    pthread_t recv_s;

    re->file_i = malloc(re->seg_count * sizeof(file_info));
    if (re->file_i == NULL) {
        error_handling("malloc fail");
    }

    while(1) {
        printf ("\x1b[%dA\r", 1);
        printf("\x1B[J");
        double read_per = 0;

        read(re->serv_sock,&location,sizeof(int));
        re->now_f = location;
        re->file_i[location].n_th_seg = location;
        read(re->serv_sock,&re->file_i[location].n_seg_size,sizeof(size_t));
        
        re->file_i[location].buf = malloc(re->file_i[location].n_seg_size);
        if (re->file_i[location].buf == NULL){
            error_handling("malloc fail");
        }
    
        read(re->serv_sock,&re->file_i[location].from_sender,sizeof(int));
        read(re->serv_sock,re->file_i[location].buf,re->file_i[location].n_seg_size);   
        
        
        //서버로부터 받은거는 최종 결과에 바로 할당
        pthread_mutex_lock(&mutex);
        result[location].n_th_seg = location;
        result[location].n_seg_size = re->file_i[location].n_seg_size;
        result[location].from_sender = re->file_i[location].from_sender;
        result[location].buf = malloc(result[location].n_seg_size);

        if (result[location].buf == NULL) {
                pthread_mutex_unlock(&mutex);
                error_handling("malloc fail");
        }

        memcpy(result[location].buf,re->file_i[location].buf,result[location].n_seg_size);
        total_read += result[location].n_seg_size;
        result_count++;
        
        read_per = ((total_read*100)/re->file_size);
        printf("Receiving Peer %d [",re->n_th_receiver);
        for(int j=0; j<read_per/10; j++){
            printf("*");
        }
        for(int j=0; j<10-read_per/10; j++){
            printf(" ");
        }
        printf("] %.f%%(%ld/%ldBytes)\n",read_per,total_read,re->file_size); 
		fflush(stdout);
        printf ("\x1b[%dA\r", 1);
        printf("\x1B[J");
        pthread_mutex_unlock(&mutex);

        //server로 부터 받은 segement 다른 receiver에게 전송하는 thread
        pthread_create(&recv_s,NULL,recv_send,re);
        pthread_join(recv_s,NULL);
        

        if(location>=re->seg_count - re->r_count){
            break;
        }

    }
    
}

void *recv_send(void *arg) {
    receiver *re = (receiver *)arg;
    int location = re->now_f;

    for (int i = 0; i < re->r_count-1; i++) {
        if (result[location].buf != NULL) {
            write(re->sockfd[i], &result[location].n_th_seg, sizeof(int));
            write(re->sockfd[i], &result[location].n_seg_size, sizeof(size_t));
            write(re->sockfd[i], result[location].buf, result[location].n_seg_size);
        }
    }
}

void *recv_thread(void *arg) {
    receiver *re = (receiver *)arg;
    int location;
    fd_set reads;
    struct timeval timeout;
    double read_per = 0;
    int fd_num;

    while(1) {
        FD_ZERO(&reads);
        int fd_max=0;

        for(int i=0; i<re->r_count-1; i++){
            FD_SET(re->sockfd[i], &reads);
            if(re->sockfd[i] > fd_max) {
                fd_max = re->sockfd[i];
            }
        }

        timeout.tv_sec = 5; 
        timeout.tv_usec = 5000;

        fd_num = select(fd_max + 1, &reads, NULL, NULL, &timeout);

        if (fd_num < 0) {
            error_handling("select() error");
        }

        for (int i = 0; i < re->r_count - 1; i++ ){
            
            if (FD_ISSET(re->sockfd[i], &reads)) {
                if (read(re->sockfd[i], &location, sizeof(int)) <= 0) {
                        continue;
                }

                if(location>re->seg_count){
                    break;
                }

                pthread_mutex_lock(&mutex);
                
                read(re->sockfd[i], &re->file_i[location].n_seg_size, sizeof(size_t));
                re->file_i[location].buf = malloc(re->file_i[location].n_seg_size);
                
                if (re->file_i[location].buf == NULL) {
                    error_handling("malloc fail");
                }
                read(re->sockfd[i], re->file_i[location].buf, re->file_i[location].n_seg_size);

                result[location].n_seg_size = re->file_i[location].n_seg_size;
                result[location].from_sender = 0;
                
                result[location].buf = malloc(result[location].n_seg_size);
                if (result[location].buf == NULL) {
                    pthread_mutex_unlock(&mutex);
                    error_handling("malloc fail");
                }

                memcpy(result[location].buf, re->file_i[location].buf,result[location].n_seg_size);
                total_read += result[location].n_seg_size;

                result_count++;
                printf ("\x1b[%dA\r", 1);
                printf("\x1B[J");
                read_per = ((total_read*100)/re->file_size);
                printf("Receiving Peer %d [",re->n_th_receiver);
                for(int j=0; j<read_per/10; j++){
                    printf("*");
                }
                for(int j=0; j<10-read_per/10; j++){
                    printf(" ");
                }
                printf("] %.f%%(%ld/%ldBytes)\n",read_per,total_read,re->file_size); 
                fflush(stdout);
                printf ("\x1b[%dA\r", 1);
                printf("\x1B[J");
                if (result_count == re->seg_count) {
                    
                    pthread_mutex_unlock(&mutex);
                    break;
                }
                pthread_mutex_unlock(&mutex);
            }
        }
       
        pthread_mutex_lock(&mutex);
        if (result_count == re->seg_count) {
            pthread_mutex_unlock(&mutex);
            break;
        }
        pthread_mutex_unlock(&mutex);
    }
}

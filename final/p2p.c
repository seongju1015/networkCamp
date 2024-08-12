// sendeer에게 세그먼트 받고 다른 리시빙 피어로 전송은 가능함.
// 그러나 다른 리시버에게 전송할 때 랜덤하게 쓰레기 값이 들어와서 코드에서 오류 발생. -> 해결하지 못함.
// 죄송합니다. 교수님..

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <sys/time.h> 

#define MAX_PEERS 10
#define MAX_SEGMENTS 1000
#define BUF_SIZE 1024

typedef struct {
    long file_size;      // 각 segment 사이즈
    char name[50];
    int n_th_seg;        // n 번째 segment
    int total_seg;       // 총 몇개의 segment 받아야하는지
    int from_sender;     // sender에게서 받은지 아닌지
    char* content;       // 파일 내용 (동적 할당 할 거임.)
} File_t;

typedef struct {
    int sockfd;         // receiver의 소켓 파일디스크립터
    char ip[INET_ADDRSTRLEN];  // receiver의 ip
    int port;           // receiver의 port번호
    int n_th_receiver;  // n번째 receiver
    int total_receiver; // 총 몇개의 receiver가 있는지
    int have_seg;       // 내가 몇개의 세그먼트를 sender로부터 받을 지
} Receiver_t;

typedef struct {
    int listen_sock;
} ThreadArg;

typedef struct {
    Receiver_t* receivers;
    int total_receivers;
    int sock;
    int receiver_index;
} SenderSegmentReceiverArg;

typedef struct {
    Receiver_t receiver;
    int start_segment;
    int num_segments;
    int sock; 
    char* content;
    File_t *segment;
} PeerTransferArg;

void error_handling(char* message);
void parse_arguments(int argc, char* argv[], int* flag_s, int* flag_r, int* max_recv, int* seg_kb, char* send_fileName, struct in_addr* r_ip, int* r_port, int* s_port);
void initialize(int argc, char* argv[], int* flag_s, int* flag_r, int* max_recv, int* seg_kb, char* send_fileName, struct in_addr* r_ip, int* r_port, int* s_port);
int create_server_socket(int port, int max_recv);
File_t* divide_file_into_segments(char* filename, int seg_kb, int* total_segments);
void handle_sender(int serv_sock, int max_recv, const char* send_fileName, int seg_kb);
void handle_receiver(struct in_addr* sender_ip, int sender_port, int receiver_port);
void* receiver_to_other(void* arg);
void* receive_from_sender(void* arg);
void* send_segments_to_peers(void* arg);

// 전역 변수 선언
File_t global_segments[MAX_SEGMENTS];  // 수신된 파일 세그먼트를 저장할 곳
pthread_mutex_t segments_mutex = PTHREAD_MUTEX_INITIALIZER;  // 뮤텍스 초기화
int received_segment_count = 0;  // 수신된 세그먼트의 수 기록

int main(int argc, char* argv[]) {
    int flag_s = 0, flag_r = 0;  // sender로 접속했는 지 receiver로 접속했는 지 판별
    int max_recv = 0, seg_kb = 0;
    char send_fileName[50] = "";  // 전송 파일 이름 초기화
    struct in_addr r_ip;          // IP 주소 초기화
    int r_port = 0;
    int s_port = 0;
    
    // 초기화 및 기본 변수 설정
    initialize(argc, argv, &flag_s, &flag_r, &max_recv, &seg_kb, send_fileName, &r_ip, &r_port, &s_port);

    if (flag_s == 1) {
        int serv_sock = create_server_socket(s_port, max_recv); // 서버 소켓 생성 및 설정
        handle_sender(serv_sock, max_recv, send_fileName, seg_kb); // sender 
    }
    else if (flag_r == 1) {
        handle_receiver(&r_ip, r_port, s_port); // receiver 
    } 
    else {
        //헬프메뉴 출력
        printf("Usage: %s \n  -s : sending peer\n", argv[0]);
        printf("  -r : receiving peer\n");
        printf("  -n : 최대 receving peer 수\n");
        printf("  -f : sending peer의 전송 파일 이름\n");
        printf("  -g : segment 크기 (단위는 kb)\n");
        printf("  -a : receiving peer 에서 입력하는 ip와 port번호\n");
        printf("  -p : sending peer에서 입력하는 listen port번호\n");
        return -1;
    }

    return 0;
}

// handle_sender -> sender역할 하는 함수
void handle_sender(int serv_sock, int max_recv, const char* send_fileName, int seg_kb) {
    int total_segments = 0;
    int clnt_sock;
    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_size;
    fd_set reads, cpy_reads;
    struct timeval timeout;
    int fd_max, fd_num, i;

    // 파일 세그먼트 단위로 분리함.
    File_t* segments = divide_file_into_segments((char *)send_fileName, seg_kb, &total_segments);
    if (segments == NULL) {
        fprintf(stderr, "파일 세그먼트로 분리 실패.\n");
        return;
    }

    // 총 세그먼트 개수 출력
    printf("Total segments: %d\n", total_segments);

    // Receiver_t 동적 할당
    Receiver_t *receivers = malloc(sizeof(Receiver_t) * max_recv);
    if (receivers == NULL) {
        error_handling("Failed to allocate memory for receivers");
    }
    int connected_peers = 0;

    FD_ZERO(&reads);
    FD_SET(serv_sock, &reads);
    fd_max = serv_sock; 

    printf("Waiting for %d clients to connect...\n", max_recv);

    // 반복문에 select 이용해서 이벤트 감지
    while (1) {
        cpy_reads = reads;
        timeout.tv_sec = 5;
        timeout.tv_usec = 5000; // 타임아웃 기준 5.5초

        fd_num = select(fd_max+1, &cpy_reads, 0, 0, &timeout);
        if (fd_num == -1) // select 오류 발생
            break;
        
        if (fd_num == 0) // 타임아웃 발생
            continue;

        for (i = 0; i <= fd_max; i++) { 
            if (FD_ISSET(i, &cpy_reads)) {
                if (i == serv_sock) {  // connection 들어옴
                    clnt_addr_size = sizeof(clnt_addr);
                    clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
                    if (clnt_sock == -1)
                        error_handling("accept() error");
                    
                    FD_SET(clnt_sock, &reads);
                    if (clnt_sock > fd_max)
                        fd_max = clnt_sock;

                    // 연결된 클라이언트의 정보를 받음
                    Receiver_t temp_receiver;
                    int info_size = read(clnt_sock, &temp_receiver, sizeof(Receiver_t));
                    if (info_size <= 0) {
                        perror("Failed to receive receiver info");
                        close(clnt_sock);
                        FD_CLR(clnt_sock, &reads);
                        continue;
                    }

                    // 받은 클라이언트 정보를 receivers 배열에 저장하면서 몇번째 피어인지 기록함.
                    receivers[connected_peers] = temp_receiver;
                    receivers[connected_peers].sockfd = clnt_sock;
                    receivers[connected_peers].n_th_receiver = connected_peers + 1; // 몇번째 피어인지 기록
                    receivers[connected_peers].total_receiver = max_recv; // 총 수신 피어의 수 설정
                    inet_ntop(AF_INET, &clnt_addr.sin_addr, receivers[connected_peers].ip, INET_ADDRSTRLEN);

                    // 각 피어가 받을 세그먼트 개수 계산함
                    receivers[connected_peers].have_seg = total_segments / max_recv + (connected_peers < total_segments % max_recv ? 1 : 0);

                    printf("들어온 리시버 정보 %d: %s:%d\n", receivers[connected_peers].n_th_receiver, receivers[connected_peers].ip, receivers[connected_peers].port);
                    printf("세그먼트 줄 개수 - receiver %d: %d\n", receivers[connected_peers].n_th_receiver, receivers[connected_peers].have_seg);

                    // 각 피어에게 몇번째 피어인지랑 총 수신 피어수 추가해서 다시 피어 정보를 전송
                    write(clnt_sock, &receivers[connected_peers], sizeof(Receiver_t));

                    connected_peers++;
                }
            }
        }

        if (connected_peers >= max_recv) {
            break;
        }
    }

    // 모든 리시버에게 리시버들의 정보를 전송
    for (i = 0; i < connected_peers; i++) {
        write(receivers[i].sockfd, receivers, sizeof(Receiver_t) * connected_peers);
    }

    // 라운드 로빈 방식으로 파일 세그먼트 전송
    for (int seg_index = 0; seg_index < total_segments; seg_index++) {
        int receiver_index = seg_index % max_recv;
                                
        // File_t 구조체의 다른 정보 전송
        int bytes_sent = write(receivers[receiver_index].sockfd, &segments[seg_index], sizeof(File_t) - sizeof(char*));
        if (bytes_sent < sizeof(File_t) - sizeof(char*)) {
            perror("Sender에서 세그먼트 구조체 보내는 거 실패");
            continue;
        }

        // content 데이터를 따로 전송
        bytes_sent = write(receivers[receiver_index].sockfd, segments[seg_index].content, segments[seg_index].file_size);
        if (bytes_sent < segments[seg_index].file_size) {
            perror("Sender에서 세그먼트 내용 보내는 거 실패");
        } else {
            printf("Sent segment %d to receiver %d\n", seg_index + 1, receiver_index + 1);
        }
    }

    // 메모리 해제
    for (int seg_index = 0; seg_index < total_segments; seg_index++) {
        free(segments[seg_index].content);
    }
    free(segments);

    close(serv_sock);
    free(receivers);
}

// 다른 리시버로 부터 파일 받는 함수
void* receiver_to_other(void* arg) {
    ThreadArg* thread_arg = (ThreadArg*)arg;
    int listen_sock = thread_arg->listen_sock;
    File_t *segment;

    // 다른 리시버와 소켓을 accept
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_size = sizeof(peer_addr);
    int peer_sock = accept(listen_sock, (struct sockaddr*)&peer_addr, &peer_addr_size);
    if (peer_sock == -1) {
        perror("accept() error");
        return NULL;
    }
    printf("다른 리시버와 연결(accept)성공 하였습니다\n");

    // 몇 개의 세그먼트를 받아야하는 지 수신
    int num_segments;
    if (read(peer_sock, &num_segments, sizeof(int)) <= 0) {
        perror("세그먼트 수 받기 실패");
        close(peer_sock);
        return NULL;
    }
    printf("나 다른 리시버한테 몇개 받아야해? %d\n", num_segments);

    // 파일 세그먼트를 받음
    for (int i = 0; i < num_segments; i++) {
        char temp_buf[BUF_SIZE];
        int read_cnt;
        char *buf = segment->content;

        // File_t 구조체 메모리 할당
        segment = (File_t *)malloc(sizeof(File_t));
        if (segment == NULL) {
            perror("다른 리시버로부터 받을 파일 구조체 메모리 할당 오류");
            break;
        }

        // 메타데이터 수신
        if (read(peer_sock, segment, sizeof(File_t) - sizeof(char*)) <= 0) {
            perror("다른 리시버로부터 파일 구조체 받는부분 오류");
            free(segment);
            continue;
        }

        // content 데이터 수신을 위한 메모리 할당
        printf("다른 리시버로부터 받은 %d번쨰segment size: %ld\n",segment->n_th_seg,segment->file_size);
        segment->content = (char *)malloc(segment->file_size);
        if (segment->content == NULL) {
            perror("다른 리시버로부터 받을 파일 콘텐츠 메모리 할당 오류");
            free(segment);
            break;
        }

        // 파일 세그먼트 내용(content) 받기
        // 데이터를 버퍼 단위로 받음
        for (int i = 0; i < segment->file_size; ) {
            if (segment->file_size - i >= BUF_SIZE)
                read_cnt = read(peer_sock, temp_buf, BUF_SIZE);
            else
                read_cnt = read(peer_sock, temp_buf, segment->file_size - i);
            
            if (read_cnt <= 0) {
                perror("다른 리시버로부터 파일 콘텐츠 수신 오류");
                free(segment->content);
                free(segment);
                break;
            }
            
            memcpy(buf + i, temp_buf, read_cnt);
            i += read_cnt;
        }
        segment->content = buf;

        // 전역 파일 세그먼트 배열에 기록
        pthread_mutex_lock(&segments_mutex);
        if (received_segment_count < MAX_SEGMENTS) {
            global_segments[received_segment_count++] = *segment;
            printf("Received segment %d/%d from peer\n", segment->n_th_seg, segment->total_seg);
        } else {
            printf("Segment buffer is full!\n");
            free(segment->content);
            free(segment);
        }
        pthread_mutex_unlock(&segments_mutex);
    }

    return NULL;
}


void* receive_from_sender(void* arg) {
    SenderSegmentReceiverArg* args = (SenderSegmentReceiverArg*)arg;
    int sock = args->sock;
    Receiver_t* receivers = args->receivers;
    int total_receivers = args->total_receivers;
    int receiver_index = args->receiver_index;

    // 다른 리시버에게 파일 세그먼트를 전달하기 위한 스레드 관련 변수들 선언
    pthread_t* peer_threads = malloc(sizeof(pthread_t) * (total_receivers - 1));
    int peer_thread_count = 0;
    int peer_socks[total_receivers - 1];
    int peer_sock_index = 0;

    // sender에게 받기 전에 제일 처음으로 다른 리시버와 먼저 연결하고 sender에게 받음. -> 그래야 받은 걸 그대로 바로 보내줄 수 있음.
    // 먼저 다른 리시버에게 연결하고 몇 개의 세그먼트를 보낼지 전송
    for (int i = 0; i < total_receivers; i++) {
        if (i != receiver_index) {
            int peer_sock = socket(PF_INET, SOCK_STREAM, 0);
            if (peer_sock == -1) {
                perror("socket() error");
                continue;
            }

            struct sockaddr_in peer_addr;
            memset(&peer_addr, 0, sizeof(peer_addr));
            peer_addr.sin_family = AF_INET;
            inet_pton(AF_INET, receivers[i].ip, &peer_addr.sin_addr);
            peer_addr.sin_port = htons(receivers[i].port);

            // 다른 리시버와 connect로 연결함.
            if (connect(peer_sock, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) == -1) {
                perror("connect() error");
                close(peer_sock);
                continue;
            }
            printf("다른 리시버와 연결(connect)성공 하였습니다\n");

            // 연결 성공하면, 다른 리시버에게 앞으로 몇 개의 세그먼트를 보낼지 전송
            int segments_to_send = receivers[receiver_index].have_seg;
            if (write(peer_sock, &segments_to_send, sizeof(int)) <= 0) {
                perror("몇개의 세그먼트 보내야하는 지 전송 실패");
                close(peer_sock);
                continue;
            }

            peer_socks[peer_sock_index++] = peer_sock;
        }
    }

    // sender로부터 파일 세그먼트 받는 부분
    while (1) {
        // segment를 동적으로 할당
        File_t* segment = malloc(sizeof(File_t));
        if (segment == NULL) {
            perror("Sender에게 받아야하는 파일 세그먼트 구조체 메모리 할당 실패");
            break;
        }

        // 구조체의 기본 정보 수신
        int bytes_received = read(sock, segment, sizeof(File_t) - sizeof(char*));
        if (bytes_received <= 0) {
            perror("Sender에게 받아야하는 파일 세그먼트 구조체 받기 실패");
            free(segment);
            break;
        }

        // content 데이터 수신을 위한 메모리 할당
        segment->content = (char*)malloc(segment->file_size);
        if (!segment->content) {
            perror("Sender에게 받아야하는 파일 세그먼트 내용(content) 메모리 할당 실패");
            free(segment);
            continue;
        }

        // content 데이터 수신
        bytes_received = read(sock, segment->content, segment->file_size);
        if (bytes_received < segment->file_size) {
            perror("Sender에게 받아야하는 파일 세그먼트 내용(content) 받기 실패");
            free(segment->content);
            free(segment);
            continue;
        }

        // 세그먼트를 전역 파일 세그먼트 배열에 기록
        pthread_mutex_lock(&segments_mutex);
        if (received_segment_count < MAX_SEGMENTS) {
            global_segments[received_segment_count++] = *segment;
            printf("Received segment %d/%d\n", segment->n_th_seg, segment->total_seg);
        } else {
            printf("Segment buffer is full!\n");
            free(segment->content);
            free(segment);
        }
        pthread_mutex_unlock(&segments_mutex);

        // 다른 리시버에게 파일 세그먼트를 전송할 스레드 생성
        for (int i = 0; i < peer_sock_index; i++) {
            PeerTransferArg* peer_arg = malloc(sizeof(PeerTransferArg));
            peer_arg->segment = segment;
            peer_arg->sock = peer_socks[i];
            peer_arg->content = segment->content;  // content 전송

            //다른 리시버에게 파일 세그먼트 보냄.
            if (pthread_create(&peer_threads[peer_thread_count], NULL, send_segments_to_peers, peer_arg) != 0) {
                perror("Failed to create peer transfer thread");
                free(peer_arg);
            } else {
                peer_thread_count++;
            }
        }
    }

    // 모든 전송 스레드가 완료될 때까지 대기
    for (int i = 0; i < peer_thread_count; i++) {
        pthread_join(peer_threads[i], NULL);
    }

    free(peer_threads);
    free(args);
    return NULL;
}

// 다른 리시버에게 파일 세그먼트 보내는 함수.
void* send_segments_to_peers(void* arg) {
    PeerTransferArg* peer_arg = (PeerTransferArg*)arg;
    int sock = peer_arg->sock;
    File_t* segment = peer_arg->segment;

    // 세그먼트 구조체 전송
    //printf("다른 리시버로 구조체 전송하기 전에 %d번쨰 구조체의 사이즈 확인 %ld\n",segment->n_th_seg,segment->file_size);
    if (write(sock, segment, sizeof(File_t) - sizeof(char*)) <= 0) {
        perror("다른 리시버로 세그먼트 구조체 전송 오류");
    }

    // content 데이터 전송
    if (write(sock, peer_arg->content, segment->file_size) <= 0) {
        perror("다른 리시버로 세그먼트 콘텐츠 전송 오류");
    }

    free(arg);
    return NULL;
}

// 리시버 측 코드
void handle_receiver(struct in_addr* sender_ip, int sender_port, int receiver_port) {
    int sock;
    struct sockaddr_in serv_addr, my_addr;
    Receiver_t recvInfo;
    Receiver_t* all_receivers;

    // 기본적인 소켓 정의 (소켓 생성 ~ connect 하기 전까지)
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
        error_handling("socket() error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr = *sender_ip;
    serv_addr.sin_port = htons(sender_port);

    // sending peer에게 connect
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
        perror("connect() error");
        close(sock);
        return;
    }

    // 자기 자신의 정보를 구조체에 저장
    recvInfo.sockfd = sock;
    inet_ntop(AF_INET, &serv_addr.sin_addr, recvInfo.ip, INET_ADDRSTRLEN);
    recvInfo.port = receiver_port;

    // 서버에게 자기 정보 전송
    if (write(sock, &recvInfo, sizeof(recvInfo)) <= 0) {
        perror("Failed to send receiver info to server");
        close(sock);
        return;
    }

    // 서버로부터 자신의 정보 수신
    int self_info_received = read(sock, &recvInfo, sizeof(Receiver_t));
    if (self_info_received <= 0) {
        perror("Failed to receive self info from server");
        close(sock);
        return;
    }

    // 자신의 정보 출력
    printf("나는 Receiver %d 입니다 %s:%d\n 센더에게 받을 세그먼트 수는 : %d입니다\n", recvInfo.n_th_receiver, recvInfo.ip, recvInfo.port, recvInfo.have_seg);

    // 모든 리시버들의 정보를 수신
    all_receivers = malloc(sizeof(Receiver_t) * recvInfo.total_receiver);
    if (all_receivers == NULL) {
        perror("Failed to allocate memory for all receivers");
        close(sock);
        return;
    }

    int all_info_received = read(sock, all_receivers, sizeof(Receiver_t) * recvInfo.total_receiver);
    if (all_info_received <= 0) {
        perror("Failed to receive all receivers info from server");
        free(all_receivers);
        close(sock);
        return;
    }

    // 모든 리시버들의 정보 출력
    printf("Received info of all receivers:\n");
    for (int i = 0; i < recvInfo.total_receiver; i++) {
        printf("모든 리시버 중 %d의 정보: %s:%d, Segments: %d\n", all_receivers[i].n_th_receiver, all_receivers[i].ip, all_receivers[i].port, all_receivers[i].have_seg);
    }

    // 현재 접속 중인 내 소켓 listen으로 만들기 위해 리스닝 소켓 설정
    int listen_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (listen_sock == -1)
        error_handling("socket() error");

    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 모든 인터페이스에서 수신
    my_addr.sin_port = htons(recvInfo.port); // 자신이 사용할 포트

    if (bind(listen_sock, (struct sockaddr*)&my_addr, sizeof(my_addr)) == -1)
        error_handling("bind() error");

    if (listen(listen_sock, 5) == -1)
        error_handling("listen() error");

    printf("Listening on port %d \n", recvInfo.port);

    // 스레드를 총 리시버 개수 -1 만큼 생성
    pthread_t* threads = malloc(sizeof(pthread_t) * (recvInfo.total_receiver - 1));
    ThreadArg* thread_args = malloc(sizeof(ThreadArg) * (recvInfo.total_receiver - 1));

    for (int i = 0; i < recvInfo.total_receiver - 1; i++) {
        thread_args[i].listen_sock = listen_sock;
        if (pthread_create(&threads[i], NULL, receiver_to_other, &thread_args[i]) != 0) {
            perror("Failed to create thread");
        }
    }

    // 스레드 생성해서 sender에게서 파일 세그먼트 받음
    SenderSegmentReceiverArg* sender_args = malloc(sizeof(SenderSegmentReceiverArg));
    sender_args->receivers = all_receivers;
    sender_args->total_receivers = recvInfo.total_receiver;
    sender_args->sock = sock;
    sender_args->receiver_index = recvInfo.n_th_receiver - 1;

    pthread_t sender_thread;
    if (pthread_create(&sender_thread, NULL, receive_from_sender, sender_args) != 0) {
        perror("Failed to create sender thread");
    }

    for (int i = 0; i < recvInfo.total_receiver - 1; i++) {
        pthread_join(threads[i], NULL);
    }
    pthread_join(sender_thread, NULL);

    // 전체 총 받은 파일 세그먼트 확인.
    pthread_mutex_lock(&segments_mutex);
    for (int i = 0; i < received_segment_count; i++) {
        printf("Segment %d: %s\n", global_segments[i].n_th_seg, global_segments[i].content);
    }
    pthread_mutex_unlock(&segments_mutex);

    free(threads);
    free(thread_args);
    free(all_receivers);
    close(sock);
    close(listen_sock);
}

// 초기화 함수: 명령줄 인자 파싱 및 기본 설정
void initialize(int argc, char* argv[], int* flag_s, int* flag_r, int* max_recv, int* seg_kb, char* send_fileName, struct in_addr* r_ip, int* r_port, int* s_port) {
    *flag_s = 0;
    *flag_r = 0;
    *max_recv = 0;
    *seg_kb = 0;
    *r_port = 0;
    *s_port = 0;
    strcpy(send_fileName, "");
    memset(r_ip, 0, sizeof(struct in_addr));

    // 명령행 옵션 파싱
    parse_arguments(argc, argv, flag_s, flag_r, max_recv, seg_kb, send_fileName, r_ip, r_port, s_port);
}

// 명령줄 옵션을 파싱하는 함수
void parse_arguments(int argc, char* argv[], int* flag_s, int* flag_r, int* max_recv, int* seg_kb, char* send_fileName, struct in_addr* r_ip, int* r_port, int* s_port) {
    int opt;

    // getopt를 사용하여 명령줄 옵션을 파싱합니다.
    while ((opt = getopt(argc, argv, "srn:f:g:a:p:")) != -1) {
        switch (opt) {
            case 's':
                *flag_s = 1; // sending peer 플래그 설정
                break;
            case 'r':
                *flag_r = 1; // receiving peer 플래그 설정
                break;
            case 'n':
                *max_recv = atoi(optarg); // 최대 receiving peer 수 설정
                break;
            case 'f':
                strncpy(send_fileName, optarg, 49); // 전송 파일 이름 설정
                send_fileName[49] = '\0'; // 문자열 끝에 NULL 추가
                break;
            case 'g':
                *seg_kb = atoi(optarg); // segment 크기 설정
                break;
            case 'a': {
                // IP 주소 설정
                if (inet_aton(optarg, r_ip) == 0) {
                    fprintf(stderr, "유효하지 않은 IP 주소 : %s\n", optarg);
                    exit(EXIT_FAILURE);
                }

                // IP 주소 다음을 포트 번호로 설정
                if (optind < argc) {
                    *r_port = atoi(argv[optind]);
                    optind++; // 다음 인수로 인덱스를 증가
                } else {
                    fprintf(stderr, "유효하지 않은 IP 와 port 포맷입니다. format: <IP> <Port>\n");
                    exit(EXIT_FAILURE);
                }
                break;
            }
            case 'p':
                *s_port = atoi(optarg); // sending peer의 listen 포트 설정
                break;
            default:
                fprintf(stderr, "Usage: %s -s -n <max_recv> -f <filename> -g <segment_size> -p <port>\n", argv[0]);
                fprintf(stderr, "       %s -r -a <ip> <port>\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
}

// 서버 소켓 생성 및 설정 함수
int create_server_socket(int port, int max_recv) {
    int serv_sock;
    struct sockaddr_in serv_addr;

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1)
        error_handling("socket() error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    if (bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("bind() error");

    if (listen(serv_sock, max_recv) == -1)
        error_handling("listen() error");

    return serv_sock;
}

// 파일을 세그먼트로 나누는 함수
File_t* divide_file_into_segments(char* filename, int seg_kb, int* total_segments) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    int seg_size = seg_kb * 1024;
    *total_segments = (file_size + seg_size - 1) / seg_size;  // 전체 세그먼트 개수 계산

    File_t* segments = (File_t*)malloc(sizeof(File_t) * (*total_segments));
    if (!segments) {
        perror("Failed to allocate memory for segments");
        fclose(file);
        return NULL;
    }

    for (int i = 0; i < *total_segments; i++) {
        segments[i].file_size = seg_size;
        strncpy(segments[i].name, filename, 49);
        segments[i].name[49] = '\0';
        segments[i].content = (char*)malloc(seg_size);
        segments[i].n_th_seg = i + 1;
        segments[i].total_seg = *total_segments;
        segments[i].from_sender = 1;  // Sender로부터 전달받은 데이터

        int bytes_read = fread(segments[i].content, 1, seg_size, file);
        if (bytes_read < seg_size && ferror(file)) {
            perror("Failed to read segment from file");
            free(segments[i].content);
            break;
        }
        segments[i].file_size = bytes_read;  // 실제 읽은 바이트 수
    }

    fclose(file);
    return segments;
}

// 에러 메시지 출력 함수
void error_handling(char* message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

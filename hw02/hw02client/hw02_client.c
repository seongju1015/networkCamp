//sender code 입니다.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <signal.h>

#define BUF_SIZE 30
#define CONTENT_SIZE 4048

typedef struct {
    char name[30]; // 파일 이름
    int size; // 파일 사이즈
    char seq[10]; // seq 번호
    int content_len; // 실제 내용 길이
    char content[CONTENT_SIZE]; // 파일 내용
} FileData;

void error_handling(char *message);
void sendUDP(int sock, struct sockaddr_in *clnt_adr, FileData *filedata);
int receiveUDP(int sock, struct sockaddr_in *clnt_adr, socklen_t *clnt_adrlen, char *seq);

int main(int argc, char *argv[]) {
    int sock;
    char message[BUF_SIZE]; // 여기에 ACK값 받아서 올 거임.
    int str_len, read_cnt;
    socklen_t serv_adr_sz;
    int seq = 0; // seq 초기화
    FILE *fp;
    char path[100] = "/home/s22000523/netprog/hw_client/hw02/testimage.jpg"; // 전송할 파일 경로
    FileData filedata; // 구조체 선언
    struct sockaddr_in serv_adr; // 서버 구조체

    if (argc != 3) {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    fp = fopen(path, "rb"); // 파일 열기
    if (!fp) {
        error_handling("File open error");
    }

    memset(&filedata, 0, sizeof(FileData)); // 구조체 메모리 초기화
    sock = socket(PF_INET, SOCK_DGRAM, 0); // 소켓 생성
    if (sock == -1) {
        error_handling("socket creation error");
    }

    // recvfrom함수에 타임아웃 1초로 설정하는 코드(아래 3줄)
    // 참고 https://blog.naver.com/cjw8349/20167530321
    struct timeval optVal = {1, 0};
    int optLen = sizeof(optVal);
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &optVal, optLen);

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_adr.sin_port = htons(atoi(argv[2]));

    // 서버와의 연결을 위한 메시지 보내기
    strcpy(message, "Hello server\n");
    sendto(sock, message, strlen(message), 0, (struct sockaddr*)&serv_adr, sizeof(serv_adr));
    printf("서버에 연결을 요청했습니다.\n");

    // filedata 구조체에 내용 저장
    struct stat st; // 파일 정보 나타내는 구조체 (헤더파일 <sys/stat.h>)
    stat(path, &st);//st에 파일 정보 저장
    filedata.size = st.st_size; // 파일 사이즈 저장
    strcpy(filedata.name, "testimage.jpg"); // 파일 이름 저장
    sprintf(filedata.seq, "%d", seq); // seq 문자열로 변환

    while (1) {

        // filedata 구조체에 내용 저장
        size_t readByte = fread(filedata.content, 1, sizeof(filedata.content), fp); // 파일 내용 읽어서 content에 저장
        if (readByte <= 0) { // 더 이상 읽을 게 없으면 reciever에게 알리기 위해 EOF 보내고 종료
            strcpy(filedata.content, "EOF"); // 파일 내용에 EOF 넣기
            filedata.content_len = strlen("EOF");
            sendUDP(sock, &serv_adr, &filedata); // 파일 내용 reciever에게 전송
            printf("Reciever에게 EOF 메세지를 보냈습니다. 프로그램 종료.\n");
            break;
        }
        filedata.content_len = readByte;

        int ack_received = 0; //ack 왔는 지 체크
        while (ack_received == 0) { // ack 오면 종료
            // reciever에게 filedata 구조체 보내기
            sendUDP(sock, &serv_adr, &filedata); 
            printf("보내는 seq값: %s 과 내용 크기: %d\n", filedata.seq, filedata.content_len);

            // reciever한테 확인 문자열 seq 받기. 1초이내에 도착하면 타임아웃 x, 1초 초과시 타임아웃
            if ( (read_cnt = recvfrom(sock, message, BUF_SIZE, 0, (struct sockaddr*)&serv_adr, &serv_adr_sz)) < 0) {
                // printf("read_cnt값 : %d\n",read_cnt);
                printf("***Timeout 발생*** -> 다시 전송합니다.\n\n");
                sprintf(filedata.seq, "%d", seq);
            }
            else{
                    // printf("read_cnt값 : %d\n",read_cnt);
                    printf("받은 ACK 값: %s\n", message);
                    message[read_cnt] = '\0';
                    if (strcmp(message, filedata.seq) == 0) { 
                        ack_received = 1; // 확인 seq 값이 일치하면 ack_received를 1로 설정
                        seq++; // 성공하면 seq 증가 
                        sprintf(filedata.seq, "%d", seq); // seq 문자열로 변환
                    }
                }
        }
    }
    fclose(fp);
    close(sock);
    return 0;
}

void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

// reciever에게 파일데이터 구조체 보내는 함수
void sendUDP(int sock, struct sockaddr_in *clnt_adr, FileData *filedata) {
    if (sendto(sock, filedata, sizeof(*filedata), 0, (struct sockaddr*)clnt_adr, sizeof(*clnt_adr)) == -1) {
        error_handling("sendto error\n");
    }
}

// reciever한테 확인 문자열 seq 받기
int receiveUDP(int sock, struct sockaddr_in *clnt_adr, socklen_t *clnt_adrlen, char *seq) {
    int str_len = recvfrom(sock, seq, BUF_SIZE, 0, (struct sockaddr*)clnt_adr, clnt_adrlen);
    if (str_len < 0) {
        return 0;
    }
    seq[str_len] = '\0'; // 문자열 끝에 널 문자 추가 (안해주면 에러남)
    return 1;
}

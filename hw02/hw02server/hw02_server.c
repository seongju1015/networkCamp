// receiver code 입니다.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 30
#define CONTENT_SIZE 4048

typedef struct {
    char name[30]; // 파일 이름
    int size; // 사이즈
    char seq[10]; // seq 번호
    int content_len; // 실제 내용 길이
    char content[CONTENT_SIZE]; // 파일 내용
} FileData;

void error_handling(char *message);
void receiveFileData(int sock, struct sockaddr_in *addr, socklen_t *addr_len, FileData *filedata);
void sendACK(int sock, struct sockaddr_in *addr, socklen_t addr_len, int seq);

int main(int argc, char *argv[]) {
    int sock;
    char message[BUF_SIZE];
    int read_cnt;
    int str_len;
    socklen_t clnt_adr_sz;
    FILE *fp;
    int seq = 0; // seq 초기화
    struct sockaddr_in serv_adr, clnt_adr;
    FileData filedata; // 구조체 선언

    if (argc != 2) {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    fp = fopen("receive.jpg", "wb"); // 받은 파일 저장할 이름
    if (!fp) {
        error_handling("File open error");
    }

    sock = socket(PF_INET, SOCK_DGRAM, 0); // 소켓 생성
    if (sock == -1) {
        error_handling("socket error");
    }

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    if (bind(sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1) {
        error_handling("bind() error");
    }

    // 클라이언트 연결 확인 메세지 받기
    clnt_adr_sz = sizeof(clnt_adr);
    str_len = recvfrom(sock, message, BUF_SIZE, 0, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);
    if (str_len < 0) {
        error_handling("처음 연결 recvfrom error");
    }
    printf("Client 연결완료, %s\n", message);

    while (1) {
        // sender에게서 filedata 구조체 받기
        receiveFileData(sock, &clnt_adr, &clnt_adr_sz, &filedata);
        printf("받은 seq: %s 과 내용 크기: %d\n", filedata.seq, filedata.content_len);

        // 만약 sender한테 EOF 받으면 종료함 
        if (strcmp(filedata.content, "EOF") == 0) {
            printf("Sender로부터 EOF 메세지를 받았습니다. 종료.\n");
            break;
        }

        // sender에게 받은 seq값이 같다면
        if (atoi(filedata.seq) == seq) {
            printf("filedata.seq값 : %d\n",atoi(filedata.seq));
            fwrite(filedata.content, 1, filedata.content_len, fp); // 받은 내용을 파일에 저장
            sendACK(sock, &clnt_adr, clnt_adr_sz, seq); // 확인 메시지 전송
            printf("보내는 ACK 값: %d\n", seq);
            seq++; 
            //printf("마지막으로 바뀐filedata.seq값 : %d\n",atoi(filedata.seq));
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

// filedata 구조체 받는 함수
void receiveFileData(int sock, struct sockaddr_in *addr, socklen_t *addr_len, FileData *filedata) {
    int str_len = recvfrom(sock, filedata, sizeof(*filedata), 0, (struct sockaddr*)addr, addr_len);
    if (str_len < 0) {
        error_handling("recvfrom() error");
    }
}

// seq 값 보내는 함수
void sendACK(int sock, struct sockaddr_in *addr, socklen_t addr_len, int seq) {
    char message[BUF_SIZE];
    sprintf(message, "%d", seq);
    if (sendto(sock, message, strlen(message), 0, (struct sockaddr*)addr, addr_len) == -1) {
        error_handling("sendto() error");
    }
}

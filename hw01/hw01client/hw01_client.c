#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <dirent.h>
#include <time.h>
#define BUF_SIZE 30

typedef struct {
    char name[30];
    int size;
} File;

void error_handling(char *message);

int main(int argc, char *argv[]){

    int sd;
    char buf[BUF_SIZE];
    int read_cnt;
    struct sockaddr_in serv_adr;
    int str_len = 0;
    int idx = 0, read_len = 0;
    char message[100];
    char fileNum[30];

    if (argc != 3) {
        printf("Usage: %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    sd = socket(PF_INET, SOCK_STREAM, 0);

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_adr.sin_port = htons(atoi(argv[2]));

    if (connect(sd, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
		error_handling("connect() error!");
	else
		puts("Connected...........");
	
	while(1){
        str_len = read(sd, message, sizeof(message) - 1);
        if (str_len == -1)
            error_handling("read() error!");

        message[str_len] = 0; // 문자열 끝에 null 추가
        printf("File list from server:\n%s\n", message);

        printf("Choose the file number(q or Q 입력 시 종료): ");
        fgets(fileNum, BUF_SIZE, stdin); // 파일 번호 입력
        if (fileNum[0] == 'q' || fileNum[0] == 'Q') { // q 입력 시 종료
            printf("exit.\n");
            break;
        }
        write(sd, fileNum, strlen(fileNum));// 파일 번호 서버로 전송

        // 서버로부터 파일 정보 받음
        File fileInfo;
        read(sd, &fileInfo, sizeof(fileInfo));

        // 파일 내용을 받아서 파일 생성
        FILE *fp = fopen(fileInfo.name, "wb"); // 받은 파일 이름으로 파일 생성 
        if (fp == NULL) {
            error_handling("file generation fail");
        }

        // 생성한 파일에 내용 넣기
        int total_received = 0;
        while (total_received < fileInfo.size) { 
            int recv_len = read(sd, buf, BUF_SIZE);
            if (recv_len == -1)
                error_handling("file recieve fail");

            fwrite(buf, 1, recv_len, fp);
            total_received += recv_len;
        }

        printf("%s recieve complete\n\n", fileInfo.name);
        fclose(fp);
    }

    close(sd);
    return 0;

}

void error_handling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}

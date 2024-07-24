#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#define BUF_SIZE 30
#define FILE_NAME_LENGTH 256

typedef struct{
    char name[30];
    int size;

} Files[10];

DIR *dir; 
struct dirent *entry;

void error_handling(char *message);
int getFileList(char *path, Files *file);

int main(int argc, char *argv[]){
    
    int serv_sd, clnt_sd;
    char buf[BUF_SIZE];
    int read_cnt;
    struct sockaddr_in serv_adr, clnt_adr;
    socklen_t clnt_adr_sz;
    char path[100] = "/home/s22000523/netporg/hw_server/hw01"; // 서버 경로
    Files *f = (Files *)malloc(sizeof(Files));

    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    int num = getFileList(path,f);
    char testMessage[100]; // 파일 리스트
    for(int i = 0; i < num; i ++){
        strcat(testMessage, f[i]->name);
        strcat(testMessage, "\n"); 
    }

    serv_sd = socket(PF_INET, SOCK_STREAM, 0);

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    bind(serv_sd, (struct sockaddr*)&serv_adr, sizeof(serv_adr));
    listen(serv_sd, 5);

    while(1){
        clnt_adr_sz = sizeof(clnt_adr);
        clnt_sd = accept(serv_sd, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);

        while(1){
            //클라이언트에게 파일 리스트 목록 전달
            write(clnt_sd, testMessage, strlen(testMessage)); // 파일 리스트 전달.

            //클라이언트한테 파일 번호 받기
            char fileNumString[BUF_SIZE]; // 클라이언트가 선택한 파일 번호
            int str_len = read(clnt_sd, fileNumString, BUF_SIZE - 1);
            if (str_len <= 0) {
                printf("클라이언트 연결 종료 \n");
                close(clnt_sd);
                close(serv_sd);
                return 0;
                break;
            }
            printf("클라이언트가 선택한 파일 번호: %s\n", fileNumString);

            //클라이언트한테 받은 번호에 해당하는 파일 구조체 전송
            fileNumString[str_len] = 0;
            int fileNum = atoi(fileNumString) - 1;
            if(fileNum >= 0 ){
                char filePath[100];
                snprintf(filePath, sizeof(filePath), "%s/%s", path, f[fileNum]->name);

                FILE *fp = fopen(filePath, "rb");
                if (fp == NULL) {
                    error_handling("file open fail");
                }
                struct stat st; // 파일 정보 나타내는 구조체 (헤더파일 <sys/stat.h>)
                stat(filePath, &st);
                f[fileNum]->size = st.st_size; // 파일 사이즈 저장 

                write(clnt_sd, f[fileNum], sizeof(*f[fileNum])); // 해당 번호에 해당하는 파일 구조체 보냄.

                // 파일 내용 보냄
                char fileBuf[BUF_SIZE];
                int read_len;
                while ((read_len = fread(fileBuf, 1, BUF_SIZE, fp)) > 0) {
                    write(clnt_sd, fileBuf, read_len);
                }

                fclose(fp);
            }
        }

        close(clnt_sd);
    }
    close(serv_sd);
    return 0;

}

void error_handling(char *message){
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}

int getFileList(char *path, Files *file){
    int index = 0;

    if ((dir = opendir(path)) == NULL) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    while ((entry = readdir(dir)) != NULL){
        strcpy(file[index]->name, entry->d_name);
        index++;
    }

    return index;

}

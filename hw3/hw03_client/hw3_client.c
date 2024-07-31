#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dirent.h>

#define BUF_SIZE 1024

typedef struct {
    char name[50];
    int size;
} Files;

DIR *dir; 
struct dirent *entry;

void error_handling(char *message); // 에러 처리 함수
void show_local_files(const char *path); //현재 클라이언트 위치에 있는 파일 및 폴더 리스트 출력 함수

int main(int argc, char *argv[]) {
    int sock;
    char message[BUF_SIZE];
    char file_content[BUF_SIZE];
    int str_len, send_len;
    struct sockaddr_in serv_adr;

    if (argc != 3) {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
        error_handling("socket() error");

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_adr.sin_port = htons(atoi(argv[2]));

    if (connect(sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1) {
        error_handling("connect() error!");
    } else {
        puts("Connected...........\n");
        puts("*** 파일 및 폴더 리스트 ***");
    }

    while (1) {
        // 처음에 파일 및 폴더 리스트 받아옴.
        char list[BUF_SIZE];
        str_len = read(sock, list, sizeof(list) - 1);
        if (str_len == -1) {
            error_handling("read() error!");
        }
        // 파일 및 폴더 리스트 출력.
        list[str_len] = 0; // 문자열 끝에 null 추가
        printf("\n%s\n", list);

        fputs("번호 입력(Q to quit): ", stdout);
        fgets(message, BUF_SIZE, stdin);

        // 사용자가 입력한 메뉴 번호(1 or 2 or 3) 전송.
        if (!strcmp(message, "q\n") || !strcmp(message, "Q\n")) {
            break;
        }
        send_len = write(sock, message, strlen(message));
        if (send_len == -1) {
            error_handling("write() error!");
        }

        int menu = atoi(message);
        switch (menu) {
            case 1:{ // 폴더 이동 코드

                // 이동할 폴더 입력받음
                char want_folder[BUF_SIZE];
                fputs("원하는 폴더를 선택해주세요.(Q to quit): ", stdout);
                fgets(want_folder, BUF_SIZE, stdin);
                if (!strcmp(want_folder, "q\n") || !strcmp(want_folder, "Q\n")) {
                    break;
                }
                send_len = write(sock, want_folder, strlen(want_folder)); // 폴더 이름 전송

                // 폴더 이동 결과(리스트) 받기
                str_len = read(sock, list, sizeof(list) - 1);
                if (str_len == -1) {
                    error_handling("read() error!");
                }
                list[str_len] = 0; // 문자열 끝에 null 추가
                printf("폴더 이동 결과\n%s\n", list);
                break;
            }

           case 2: { // 파일 다운로드 코드
                
                // 다운로드 원하는 파일 이름 받기
                char want_file[BUF_SIZE];
                fputs("다운로드할 파일을 선택해주세요.(Q to quit): ", stdout);
                fgets(want_file, BUF_SIZE, stdin);
                if (!strcmp(want_file, "q\n") || !strcmp(want_file, "Q\n")) {
                    break;
                }
                send_len = write(sock, want_file, strlen(want_file)); // 파일 이름 전송
                if (send_len == -1) {
                    error_handling("파일 이름 전송 실패!");
                }

                // 파일 정보(구조체) 받기
                Files fileInfo;
                int fileInfo_len = read(sock, &fileInfo, sizeof(fileInfo));
                if (fileInfo_len != sizeof(fileInfo)) {
                    error_handling("파일 정보 수신 실패!");
                }

                // 파일 생성
                FILE *fp = fopen(fileInfo.name, "wb");
                if (fp == NULL) {
                    error_handling("파일 생성 실패!");
                }

                // 파일 내용 받기
                int total_received = 0;
                while (total_received < fileInfo.size) {
                    int recv_len = read(sock, file_content, BUF_SIZE);
                    if (recv_len == -1) {
                        error_handling("파일 수신 실패!");
                    }

                    // 데이터 쓰기
                    fwrite(file_content, 1, recv_len, fp);
                    total_received += recv_len;
                }

                fclose(fp);

                // 파일 수신 완료 신호 전송
                write(sock, "DONE", 4);

                // 서버로부터 결과 리스트 받기
                str_len = read(sock, list, sizeof(list) - 1);
                if (str_len == -1) {
                    error_handling("read() error!");
                }
                list[str_len] = 0; // 문자열 끝에 null 추가
                printf("파일 다운로드 후 결과\n%s\n", list);

                break;
            }

            case 3: { // 파일 업로드 코드

                // 현재 클라이언트 위치에 있는 파일 및 폴더 리스트 출력
                char clnt_path[100] = "/home/s22000523/netprog/hw_client/hw3";
                show_local_files(clnt_path); //현재 위치에 있는 파일 및 폴더 리스트 출력 함수

                // 업로드하고 싶은 파일 이름 입력 받아서 서버로 전송
                char want_file[BUF_SIZE];
                fputs("업로드할 파일을 선택해주세요.(Q to quit): ", stdout);
                fgets(want_file, BUF_SIZE, stdin);
                if (!strcmp(want_file, "q\n") || !strcmp(want_file, "Q\n")) {
                    break;
                }
                want_file[strlen(want_file) - 1] = '\0'; // 개행 문자 제거

                send_len = write(sock, want_file, strlen(want_file));
                if (send_len == -1) {
                    error_handling("파일 이름 전송 실패!");
                }

                // 파일 이름으로 해당 파일 열기
                char filePath[200];
                snprintf(filePath, sizeof(filePath), "%s/%s", clnt_path, want_file);
                FILE *fp = fopen(filePath, "rb");
                if (fp == NULL) {
                    error_handling("파일 열기 실패!");
                }

                // 파일 정보를 구조체에 담아서 서버로 전송
                Files fileInfo;
                strcpy(fileInfo.name, want_file);
                struct stat st;
                if (stat(filePath, &st) == 0) {
                    fileInfo.size = st.st_size;
                } else {
                    fileInfo.size = -1; // 오류 발생 시 파일 크기를 -1로 설정
                    fclose(fp);
                    break;
                }
                write(sock, &fileInfo, sizeof(fileInfo));

                // 파일 내용을 서버에게 전송
                while (!feof(fp)) {
                    int bytesRead = fread(file_content, 1, BUF_SIZE, fp);
                    if (bytesRead > 0) {
                        write(sock, file_content, bytesRead);
                    }
                }
                fclose(fp);

                // EOF 신호 전송
                write(sock, "EOF", 3);

                // 서버로부터 완료 신호 받기
                char done_signal[BUF_SIZE];
                str_len = read(sock, done_signal, sizeof(done_signal) - 1);
                if (str_len == -1) {
                    error_handling("read() error!");
                }
                done_signal[str_len] = 0;

                // DONE이라는 신호 받으면 완료된 거
                if (strcmp(done_signal, "DONE") == 0) {
                    printf("파일 업로드 완료\n");
                }
                break;
            }
            default:
                printf("잘못된 입력입니다. 다시 시도해주세요.\n");
                break;
        }
    }

    close(sock);
    return 0;
}

// 에러 처리 함수
void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

// 현재 디렉토리 파일 및 폴더 리스트 출력 함수
void show_local_files(const char *path) {
    DIR *dir;
    struct dirent *entry;
    struct stat info;
    char full_path[1024];

    if ((dir = opendir(path)) == NULL) {
        perror("opendir() error");
        return;
    }

    printf("\n*** 파일 및 폴더 리스트 ***\n");
    while ((entry = readdir(dir)) != NULL) {
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        if (stat(full_path, &info) == 0) {
            if (S_ISDIR(info.st_mode)) {
                printf("%s [dir]\n", entry->d_name);
            } else {
                printf("%s [file]\n", entry->d_name);
            }
        }
    }
    closedir(dir);
}

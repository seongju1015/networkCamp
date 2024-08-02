#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <termios.h>

#define BUF_SIZE 1024

void error_handling(char *message);
void set_input_mode();

int main(int argc, char *argv[]) {
    int sock;
    struct sockaddr_in serv_addr;
    char input_message[BUF_SIZE] = {0};  // 사용자 입력 저장 버퍼
    char response_message[BUF_SIZE] = {0};  // 서버 응답 저장 버퍼
    int str_len;

    if (argc != 3) {
        printf("Usage: %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1){
        error_handling("socket() error");
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1){
        error_handling("connect() error");
    }
    else{
        puts("Connected.........");
    }

    // 터미널 설정
    set_input_mode();

    while (1) {
        // \033[u - 이게 커서위치 복원 , \033[J - 이게 그 위치로부터 클리어
        printf("\033[u\033[J"); 
        printf("사용자가 입력하는 문자열: %s\n", input_message);
        printf("--------------------------------\n");
        printf("서버 응답: %s\n", response_message);  // 서버 응답 출력
        fflush(stdout);

        // 한 문자 읽기
        char input_char = getchar(); 

        if (input_char == '\n' ) {
            printf("\n");
            continue;
        } 

        // ESC 키 누르면 종료
        else if (input_char == 27) {  
            break;
        } 

        // 백스페이스 처리
        else if (input_char == 127 || input_char == '\b') {  
            int len = strlen(input_message);
            if (len > 0) {
                input_message[len - 1] = '\0';  // 마지막으로 입력되었던 문자 제거
            }
        } 

        // 기본적인 입력받은 경우 문자 추가
        else {
            int len = strlen(input_message);
            if (len < BUF_SIZE - 1) {
                input_message[len] = input_char;
                input_message[len + 1] = '\0';
            }
        }

        // 변경된 전체 메시지를 서버로 전송
        write(sock, input_message, strlen(input_message) + 1);
        memset(response_message, 0, BUF_SIZE);
        str_len = read(sock, response_message, BUF_SIZE-1);
        response_message[str_len] = '\0';
    }

    close(sock);
    return 0;
}

// 터미널 설정
void set_input_mode() {
    struct termios tattr;
    if (!isatty(STDIN_FILENO)) {
        fprintf(stderr, "Not a terminal.\n");
        exit(EXIT_FAILURE);
    }
    tcgetattr(STDIN_FILENO, &tattr); // 선언
    tattr.c_lflag &= ~(ICANON );  // 비표준 입력, 에코 비활성화
    tattr.c_cc[VMIN] = 1; // 최소 1개의 문자가 입력되어야 read로 결과 반환함 
    tattr.c_cc[VTIME] = 0; // 반환하기 전에 대기하는 시간 없앰. 바로 응답할 수 있게.
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &tattr); // 변경 내용 저장 
}

void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

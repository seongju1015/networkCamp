#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>

#define BUFFER_SIZE 1024
#define MAX_WORDS 50
#define RED "\033[1;31m" // 색상 설정
#define RESET "\033[0m"

typedef struct {
    char word[BUFFER_SIZE];
    int count;
} SearchWord;

typedef struct {
    int client_fd;
} ThreadArgs;

SearchWord searchWord[BUFFER_SIZE];
int search_count = 0;

void error_handling(char *buf); // 에러 처리
void load_data(char *fileName); // 파일에서 단어랑 search count 가져오는 함수
void *request_handler(void *arg); // 스레드 생성 때 실행되는 함수
int compare(const void *a, const void *b); // qsort()의 파라매터로 들어가는 함수. 두 수 비교
void get_words(char *input, char *response); // 주어진 단어가 포함된 단어를 찾아서 search count수로 qsort함 

int main(int argc, char *argv[]) {
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    socklen_t clnt_adr_size;
    pthread_t t_id;

    if (argc != 2) {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    // data.txt파일에 있는 단어 구조체에 저장
    load_data("data.txt"); 

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1) error_handling("socket() error");

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");

    if (listen(serv_sock, 20) == -1)
        error_handling("listen() error");

    while (1) {
        clnt_adr_size = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &clnt_adr_size);
        if (clnt_sock == -1) continue; 

        // 스레드 함수 파라매터로 넣을 거 메모리 할당
        ThreadArgs *args = malloc(sizeof(ThreadArgs)); 
        if (args == NULL) continue; 

        // 스레드로 실행
        args->client_fd = clnt_sock;
        pthread_create(&t_id, NULL, request_handler, (void*)args);
        pthread_detach(t_id);
    }

    close(serv_sock);
    return 0;
}

// 에러 처리
void error_handling(char *buf){
	fputs(buf, stderr);
	fputc('\n', stderr);
	exit(1);
}

// 파일에서 단어랑 search count 가져오는 함수
void load_data(char *fileName) {

    int count; // 단어의 search count 수
    FILE *fp = fopen(fileName, "r");
    char line[BUFFER_SIZE];
    if (!fp) error_handling("file open error");

    // 파일 안에 있는 한줄씩(line)단위로 내용 읽음
    while (fgets(line, BUFFER_SIZE, fp) != NULL) {
        char *ptr = strrchr(line, ' ');  // line에서 가장 마지막 공백을 찾음(어짜피 line당 공백은 하나 뿐임.)
        
        if (ptr) {
            *ptr = '\0';  // 찾은 공백을 NULL 문자로 변경하여 문자열을 단어와 searc count로 분리함
            count = atoi(ptr + 1);  // 공백 다음 문자는 search count
            strcpy(searchWord[search_count].word, line);
            searchWord[search_count].count = count;
            search_count++;
        }
    }

    fclose(fp);
}

void *request_handler(void *arg) {
    int client_fd = ((ThreadArgs *)arg)->client_fd;
    char buffer[BUFFER_SIZE] = {0};
    char response[BUFFER_SIZE] = {0};
    int read_bytes;

    while(1) {
        // client로부터 응답 받음
        memset(buffer, 0, BUFFER_SIZE);
        read_bytes = read(client_fd, buffer, BUFFER_SIZE);
        if (read_bytes > 0) {
            buffer[read_bytes] = '\0'; // 마지막 null

            // 클라이언트에게 보내줄 response변수 초기화
            memset(response, 0, BUFFER_SIZE);

            // 클라이언트에게 보내 줄 결과(response) 생성.
            get_words(buffer, response);
            send(client_fd, response, strlen(response), 0); // 클라이언트에게 전송 
            
            printf("클라이언트로부터 받은 메시지: %s\n", buffer);
        }
    }

    close(client_fd);
    free(arg); // 메모리 해제
    return NULL;
}

// qsort()의 파라매터로 들어가는 함수. 두 수 비교
int compare(const void *a, const void *b) {
    SearchWord *swA = (SearchWord *)a;
    SearchWord *swB = (SearchWord *)b;
    return swB->count - swA->count;
}

// 주어진 문자열이 포함된 단어를 찾아서 search count수로 qsort함
void get_words(char *input, char *response) {
    SearchWord s_words[MAX_WORDS];
    int searchingCount = 0;

    // 주어진 문자열이 들어간 단어 있는 지 확인
    for (int i = 0; i < search_count; i++) {
        if (strstr(searchWord[i].word, input) != NULL) {
            s_words[searchingCount++] = searchWord[i];
            if (searchingCount == MAX_WORDS) break;
        }
    }

    // search count 순으로 정렬.
    qsort(s_words, searchingCount, sizeof(SearchWord), compare);

    // 문자열이 포함된 단어 리스트 생성.
    strcpy(response, "\n");
    for (int i = 0; i < searchingCount; i++) {
        char *match = strstr(s_words[i].word, input);
        char highlighted[1024] = {0};

        //match 한다면 match하는 문자열 빨간색으로 표시해서 리스트 만들어줌
        if (match) {
            int prefix_len = match - s_words[i].word;
            memcpy(highlighted, s_words[i].word, prefix_len); // 일치하는 문자열 나오기 전까지 원래 색상
            strcat(highlighted, RED); // 일치하는 문자열은 빨간색으로
            strcat(highlighted, input);
            strcat(highlighted, RESET); // 다시 원래색으로
            strcat(highlighted, match + strlen(input)); // 나머지 문자열 
        } 
        else {
            strcat(highlighted, s_words[i].word);
        }

        strcat(response, highlighted);
        strcat(response, "\n");
    }
}

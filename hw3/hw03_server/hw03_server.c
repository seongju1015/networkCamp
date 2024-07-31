#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pthread.h>

#define BUF_SIZE 100

// 파일 구조체 선언
typedef struct {
    char name[50];
    int size;
} Files;

// 디렉토리 구조체 선언
typedef struct {
    char name[50];
} Directory;

typedef struct {
    int socket;
    char ab_path[200]; // 현재 클라이언트가 있는 절대 경로.
    char list[500]; // 현재 클라이언트가 있는 경로에 파일 및 디렉토리 리스트
} Clnt_info;

//스레드 생성 시 함수의 인자로 쓸 것들 
//스레드 생성 시에는 여러개의 인자를 넣을 수 없어서 구조체로 넘겨줘야함.
typedef struct {
    Clnt_info *clntInfo;
    Files *fp;
    Directory *directory;
} ThreadArgs;

DIR *dir; 
struct dirent *entry;

void error_handling(char *buf);
int getDirList(char *path); // 현재 있는 경로에 디렉토리가 몇개인지 return
int getFileList(char *path); // 현재 있는 경로에 파일 몇개인지 return
void storeDirInfo(char *path, Directory *directory); // Directory 들의 정보를 directory 구조체의 저장
void storeFileInfo(char *path, Files *file); // File 들의 정보를 fp 구조체의 저장
void showList(char *path, char *listMessage, Files *fp, Directory *directory); // 경로에 있는 디렉토리 개수랑 파일 개수 저장.

void *menuHandler(void *arg); // 종합적인 메뉴 관리 툴( 리스트 보여주기, 이동, 다운, 업로드 기능 안내 )
void moveDir(Clnt_info *clntInfo, Files *fp, Directory *directory); // 선택한 폴더로 이동하는 함수
void downloadFile(Clnt_info *clntInfo, Files *fp, Directory *directory); // 선택한 파일 다운로드하는 함수
void uploadFile(Clnt_info *clntInfo, Files *fp, Directory *directory); // 선택한 파일 업로드하는 함수

int main(int argc, char *argv[]) {
	// listen까지는 기본 세팅
	int serv_sock, clnt_sock;
	struct sockaddr_in serv_adr, clnt_adr;
	socklen_t adr_sz;
	Directory *directory;
	Files *fp;
	Clnt_info *clnt_info = (Clnt_info *)malloc(sizeof(Clnt_info)); // 포인터로 선언

	if (argc != 2) {
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
	}

	serv_sock = socket(PF_INET, SOCK_STREAM, 0);
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_adr.sin_port = htons(atoi(argv[1]));
	
	if (bind(serv_sock, (struct sockaddr*) &serv_adr, sizeof(serv_adr)) == -1){
		error_handling("bind() error");
	}
	if (listen(serv_sock, 5) == -1){
		error_handling("listen() error");
	}

	char path[100] = "/home/s22000523/netporg/hw_server/hw03"; // 서버 경로
	strcpy(clnt_info->ab_path, path); //clnt_info 구조체에도 저장.

	while (1)
	{
		adr_sz = sizeof(clnt_adr);
		clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &adr_sz);

		clnt_info->socket = clnt_sock; // 연결된 clnt socket 저장
		printf("connected client: %d \n", clnt_info->socket);
					
		// 스레드 생성 함수에 인자로 쓸 것들 준비. 파라매터로 쓸 것이기 때문에 메모리 할당해서 포인터로.
		// 이때부터 directory저장까지 뮤텍스 걸어야함 아마.
		ThreadArgs *args = (ThreadArgs *)malloc(sizeof(ThreadArgs));
		args->clntInfo = clnt_info;
		args->fp = fp;
		args->directory = directory;
					
		// 접속한 클라이언트에게 스레드로 handler 보내줌.
		pthread_t t_id;
		if (pthread_create(&t_id, NULL, menuHandler, (void *)args) != 0) {
                perror("pthread_create error");
                free(args);
            }
		pthread_detach(t_id); // 스레드 분리
	}
	close(serv_sock);
	return 0;
}

void error_handling(char *buf)
{
	fputs(buf, stderr);
	fputc('\n', stderr);
	exit(1);
}

// 현재 있는 경로에 디렉토리가 몇개인지 return
int getDirList(char *path){
	int index = 0;

	if ((dir = opendir(path)) == NULL) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

	while ((entry = readdir(dir)) != NULL){
		// 디렉토리인 경우
        if (entry->d_type == DT_DIR){
			index++;
		}
    }
    closedir(dir); // 디렉토리 스트림 닫기
    return index;
}

// 현재 있는 경로에 파일 몇개인지 return
int getFileList(char *path){
    int index = 0;

	if ((dir = opendir(path)) == NULL) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    while ((entry = readdir(dir)) != NULL){
		// file인 경우
        if (entry->d_type == DT_REG){
			index++;	
		}
    }
    closedir(dir); // 디렉토리 스트림 닫기
    return index;
}

// directory 들의 정보를 directory 구조체의 저장
void storeDirInfo(char *path, Directory *directory){
	int index = 0;

	if ((dir = opendir(path)) == NULL) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    while ((entry = readdir(dir)) != NULL){
		// 디렉토리인 경우
		if (entry->d_type == DT_DIR){
			strcpy(directory[index].name, entry->d_name); // 디렉토리 이름 저장
        	index++;
		}
    }
    closedir(dir); // 디렉토리 스트림 닫기
}

// File 들의 정보를 fp 구조체의 저장
void storeFileInfo(char *path, Files *file){
	int index = 0;

    if ((dir = opendir(path)) == NULL) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    while ((entry = readdir(dir)) != NULL){
		// file인 경우
        if (entry->d_type == DT_REG){
			strcpy(file[index].name, entry->d_name); // 파일 이름 저장
			struct stat st; // 파일 정보 나타내는 구조체 (헤더파일 <sys/stat.h>)
			stat(path, &st);
			file[index].size = st.st_size; // 파일 사이즈 저장 
			index++;
		}
	}
    closedir(dir); // 디렉토리 스트림 닫기
}

// 경로에 있는 디렉토리 개수랑 파일 개수 저장.
void showList(char *path, char *listMessage, Files *fp, Directory *directory ){
	int dsize = getDirList(path);
	int fsize = getFileList(path);

	// 개수만큼 각각 메모리 할당 후 각각의 구조체의 정보 저장
	directory = (Directory *)malloc(dsize * sizeof(Directory));
    fp = (Files *)malloc(fsize * sizeof(Files));
	storeDirInfo(path, directory);
	storeFileInfo(path, fp);

	//listMessage 에 디렉토리 및 파일 리스트들을 저장.
	for(int i = 0; i < dsize; i++){
		strcat(listMessage, directory[i].name);
        strcat(listMessage, " [dir]\n"); 
	}
    for(int i = 0; i < fsize; i ++){
        strcat(listMessage, fp[i].name);
        strcat(listMessage, " [file]\n"); 
    }

	free(directory); // 메모리 해제
    free(fp); // 메모리 해제
}

// 현재 파일 정보를 가져와서 보여주고 선택할 옵션들을 보여줌
// 스레드 함수에 넣을 파라매터 함수는 'void *' 형식으로 해야함
void *menuHandler(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;
    Clnt_info *clntInfo = args->clntInfo;
    Files *fp = args->fp;
    Directory *directory = args->directory;

	while(1) {
		// 현재 위치의 파일 및 폴더 리스트 보냄.
		strcpy(clntInfo->list,""); // 리스트 초기화
		showList(clntInfo->ab_path, clntInfo->list, fp, directory);
		char infoMessage[100] = "\n실행 할 메뉴를 선택하세요.\n1. 폴더 이동\n2. 파일 다운로드\n3. 파일 업로드\n\n";
		strcat(clntInfo->list,infoMessage);
		write(clntInfo->socket, clntInfo->list, strlen(clntInfo->list));

		// 클라이언트로부터 메뉴 선택 번호 받음
		char menuNumString[BUF_SIZE]; // 클라이언트가 선택한 메뉴 번호
		int str_len = read(clntInfo->socket, menuNumString, BUF_SIZE - 1);
		if (!strcmp(menuNumString,"q\n") || !strcmp(menuNumString,"Q\n")){
			break;
		}
		
		// 메뉴 선택에 따라 다른 로직 수행
		int menuNum = atoi(menuNumString);
		switch(menuNum) {
			case 1: // 폴더 이동
				moveDir(clntInfo, fp, directory);
				break;
			case 2: // 파일 다운로드
				downloadFile(clntInfo, fp, directory);
				break;
			case 3: // 파일 업로드
				uploadFile(clntInfo, fp, directory);
				break;
			default:
				break;
		}
	}

    free(arg); // 동적으로 할당된 메모리 해제
    return NULL;
}


// 선택한 폴더로 이동하는 함수
void moveDir(Clnt_info *clntInfo, Files *fp, Directory *directory){
	// 클라이언트가 선택한 폴더 또는 파일 이름 받음
	char mv_directory[BUF_SIZE];
	int str_len;
	str_len = read(clntInfo->socket,mv_directory,BUF_SIZE-1);
	mv_directory[str_len-1] = '\0';

	// 선택한 게 폴더인지 파일인지 판별
	if(chdir(mv_directory) == 0) {
		// 폴더라면 그에 해당하는 절대경로 저장
		if (getcwd(clntInfo->ab_path, sizeof(clntInfo->ab_path)) == NULL) {
			error_handling("getcwd() error after chdir");
		}
		printf("%s\n",clntInfo->ab_path);
		// 폴더의 절대 경로안에 있는 파일 및 폴더리스트들 저장 후 보내줌.
		strcpy(clntInfo->list,""); // 리스트 초기화
    	showList(clntInfo->ab_path, clntInfo->list, fp, directory);
		write(clntInfo->socket, clntInfo->list, strlen(clntInfo->list));
	}
	// 파일이라면 오류 메세지
	else{
		char errorMessage[75] = "** 선택하신 건 폴더가 아닙니다. 다시 선택해주세요. **\n";
		write(clntInfo->socket, errorMessage, strlen(errorMessage));
	}
}

// 파일 다운로드 함수.
void downloadFile(Clnt_info *clntInfo, Files *fp, Directory *directory) {
    char fileName[BUF_SIZE];
    char filePath[200];
    int str_len;
    char buffer[BUF_SIZE];
    FILE *fpDownload;

    // 클라이언트에게 파일 이름 받기
    if ((str_len = read(clntInfo->socket, fileName, sizeof(fileName) - 1)) == -1) {
        perror("read() error");
        return;
    }
    fileName[str_len - 1] = '\0'; // 문자열 종료 처리
    snprintf(filePath, sizeof(filePath), "%s/%s", clntInfo->ab_path, fileName);

	// 받아온 이름으로 파일 열기
    fpDownload = fopen(filePath, "rb");
    if (fpDownload == NULL) {
        perror("fopen() error");
        write(clntInfo->socket, "File not found.", 15);
        return;
    }

    // 파일 정보를 가져오기 위해 Files 구조체에 저장
    Files file_info;
    strcpy(file_info.name, fileName);
    struct stat st;
    if (stat(filePath, &st) == 0) {
        file_info.size = st.st_size;
    } else {
        file_info.size = -1; // 오류 발생 시 파일 크기를 -1로 설정
    }

    // 파일 정보 구조체를 클라이언트로 전송
    write(clntInfo->socket, &file_info, sizeof(file_info));

    // 파일 내용을 전송
    while (!feof(fpDownload)) {
        int bytesRead = fread(buffer, 1, BUF_SIZE, fpDownload);
        if (bytesRead > 0) {
            write(clntInfo->socket, buffer, bytesRead);
        }
    }
    fclose(fpDownload);

    // EOF 신호 전송
	char end_of_file_signal[] = "EOF";
	write(clntInfo->socket, end_of_file_signal, sizeof(end_of_file_signal));

	// 클라이언트로부터 완료 신호 받기
	char done_signal[BUF_SIZE];
	int str_lens = read(clntInfo->socket, done_signal, sizeof(done_signal) - 1);
	if (str_lens == -1) {
		error_handling("read() error while waiting for DONE signal");
	}
	done_signal[str_lens] = 0;

	if (strcmp(done_signal, "DONE") == 0) {
		// 결과 리스트 전송
		showList(clntInfo->ab_path, clntInfo->list, fp, directory);
		write(clntInfo->socket, clntInfo->list, strlen(clntInfo->list));

	}
}

// 선택한 파일 업로드하는 함수
void uploadFile(Clnt_info *clntInfo, Files *fp, Directory *directory) {
    char fileName[BUF_SIZE];
    char filePath[200];
    int str_len;
    char buffer[BUF_SIZE];
    FILE *fpUpload;

    // 클라이언트가 선택한 파일 이름 받음
    if ((str_len = read(clntInfo->socket, fileName, sizeof(fileName) - 1)) == -1) {
        perror("read() error");
        return;
    }
    fileName[str_len] = '\0'; // 문자열 종료 처리
    snprintf(filePath, sizeof(filePath), "%s/%s", clntInfo->ab_path, fileName);

    // 해당 파일 이름으로 파일 생성
    fpUpload = fopen(filePath, "wb");
    if (fpUpload == NULL) {
        perror("fopen() error");
        return;
    }

    // 클라이언트에게 파일 구조체 받음
    Files file_info;
    int fileInfo_len = read(clntInfo->socket, &file_info, sizeof(file_info));
    if (fileInfo_len != sizeof(file_info)) {
        perror("파일 정보 수신 실패");
        fclose(fpUpload);
        return;
    }

    // 파일 내용을 전송받아 파일 작성
    int total_received = 0;
    while (total_received < file_info.size) {
        int recv_len = read(clntInfo->socket, buffer, BUF_SIZE);
        if (recv_len == -1) {
            perror("파일 수신 실패");
            fclose(fpUpload);
            return;
        }
        fwrite(buffer, 1, recv_len, fpUpload);
        total_received += recv_len;
    }
    fclose(fpUpload);

    // 파일 수신 완료 신호 전송
    write(clntInfo->socket, "DONE", 4);
}

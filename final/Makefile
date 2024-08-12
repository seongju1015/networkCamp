# 컴파일러 및 컴파일러 옵션 설정
CC = gcc
CFLAGS = -Wall -Wextra -O2

# 실행 파일 이름 설정
TARGET = p2p

# 소스 파일 및 객체 파일 설정
SRCS = p2p.c
OBJS = $(SRCS:.c=.o)

# 기본 타겟: 실행 파일 생성
all: $(TARGET)

# 실행 파일 생성 규칙
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# 객체 파일 생성 규칙
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# clean 타겟: 생성된 파일 삭제
clean:
	rm -f $(TARGET) $(OBJS)

# .PHONY 설정: 의존성이 없는 타겟을 설정
.PHONY: all clean

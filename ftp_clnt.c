#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <pthread.h>

#define BUFSIZE 100
#define MAXSIZE 150

#define RED "\x1b[31m"
#define YELLO "\x1b[33m"
#define RESET_COLOR "\x1b[0m"

void error_handling(char *message);
void get_func(int sock, char *ftp_arg);
void put_func(int sock);

void sock_read(int sock);

int main(int argc, char *argv[])
{
    int sock;
    struct sockaddr_in serv_addr;

    char message[BUFSIZE];
    char buf[BUFSIZE];
    char *ftp_cmd;
    char *ftp_arg;
    int str_len = 0;

    if (argc != 3)
    {
        printf( RED "Usage : <IP><port>\n" RESET_COLOR);
        exit(1);
    }

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
        error_handling("socket() error");
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("100 error : server connection error");

    sock_read(sock);

    chdir("client_files");

    while (true)
    {
        memset(message, 0, BUFSIZE);
        printf( YELLO "명령어 :( q : 종료 | ls : 파일 목록 | get : 파일 다운로드 | put : 파일 업로드 )\n" RESET_COLOR);
        printf( YELLO "명령어 입력 : " RESET_COLOR);
        scanf("%[^\n]", message);
        getchar();

        if (!strcmp(message, "q"))
        { /* 'q' 입력 시 종료 */
            printf("서버와의 연결을 종료합니다.\n");
            close(sock);
            exit(0);
        }
        // printf("scanf 값 : %s\n", message); // 디버깅용 코드
        ftp_cmd = strtok(message, " ");

        if (ftp_cmd == NULL)
            continue;

        /* 클라이언트에서의 명령어별 예외처리 및 동작 */
        if (!strcmp(ftp_cmd, "ls"))
        {
            if (strtok(NULL, " ") != NULL)
                printf("ls 명령어는 명령 인수를 사용할 수 없습니다!\n");
            else
            {
                write(sock, ftp_cmd, MAXSIZE);
                sock_read(sock);
            }
        }
        else if (!strcmp(ftp_cmd, "get"))
        {
            memset(buf, 0, BUFSIZE);
            
            write(sock, ftp_cmd, MAXSIZE);
            sock_read(sock);

            while (true)
            {
                printf( YELLO "다운받을 파일명 : " RESET_COLOR);
                scanf("%[^\n]", buf);
                getchar();

                if (!strcmp("", buf))
                    printf(RED "다운받을 파일명을 입력하세요!.\n" RESET_COLOR);
                else
                    break;
                
            }

            write(sock, buf, BUFSIZE);
            get_func(sock, buf);
        }
        else if (!strcmp(ftp_cmd, "put"))
        {
            put_func(sock);
        }
        else
        {
            printf( RED "지원하지 않는 명령어를 입력하였습니다. 다시 입력하세요!\n" RESET_COLOR);
        }
    }
    close(sock);
    return 0;
}

/** 에러 발생시 예외처리를 위한 함수
 * @param   message 예외처리시 출력할 에러 메시지가 담긴 문자열의 시작 주소
 */
void error_handling(char *message)
{
    sprintf(message, RED "%s %s", message, RESET_COLOR);
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

/** 서버가 보낸 파일 관련 데이터를 처리하는 함수
 * @param   sock 서버의 소켓 번호
 * @param   ftp_arg 서버로부터 전송을 요청한 파일의 이름
 */
void get_func(int sock, char *ftp_arg)
{
    struct stat file_info;
    int size = 0;
    int fd;
    char buf[BUFSIZE];
    // char *file_data;

    read(sock, &size, sizeof(int));

    if (!size) // size가 0이면
        printf( RED "200 error : file not found.\n" RESET_COLOR);
    else
    {
        // file_data = malloc(size);
        // read(sock, file_data, size);
        int file_no = 1;
        int down_size;
        char get_file_name[BUFSIZE];
        char *filename;
        char *ext_file;

        strcpy(get_file_name, ftp_arg);

        filename = strtok(ftp_arg, ".");
        ext_file = strtok(NULL, ".");

        while (true) // 동일명 파일 다운 처리
        {
            fd = open(get_file_name, O_CREAT | O_EXCL | O_WRONLY, 0664);
            if (fd == -1)
            {  
                if (ext_file != NULL)
                    sprintf(get_file_name, "%s_%d.%s", filename, file_no, ext_file);
                else if (ext_file == NULL)
                    sprintf(get_file_name, "%s_%d", filename, file_no);
            }
            else
                break;
            file_no++;
        }

        while(true)
        {
            read(sock, buf, BUFSIZE);
            if(!strcmp(buf, ""))
                break;
            else
                write(fd, buf, strlen(buf));  
        }

        stat(get_file_name, &file_info);
        down_size = file_info.st_size;
        if(size == down_size)
        {
            printf("파일 다운로드 완료!\n");
            printf("다운로드된 파일명 : %s\n", get_file_name);
        }
        else
        {
            printf( RED "파일이 정상적으로 다운로드되지 않았습니다!\n" RESET_COLOR);
            printf( RED "다시 다운받아 주세요!\n" RESET_COLOR);
        }

        close(fd);
    }
}

/** 서버로 보낼 파일 관련 데이터를 처리하는 함수
 * @param   sock 파일을 전송할 서버의 소켓 번호
 */
void put_func(int sock)
{
    int size = 0;
    int fd;
    struct stat file_info;
    char buf[BUFSIZE];

    write(sock, "put", MAXSIZE);

    sock_read(sock);

    while (true)
    {
        printf(YELLO "업로드할 파일명 : " RESET_COLOR);
        scanf("%[^\n]", buf);
        getchar();

        if (!strcmp("", buf))
            printf(RED "업로드할 파일명을 입력하세요!.\n" RESET_COLOR);
        else
            break;
    }

    stat(buf, &file_info);
    fd = open(buf, O_RDONLY);
    size = file_info.st_size;

    if (fd == -1) // 파일이 없을때
    {
        size = 0;
        printf(RED "업로드할 파일이 존재하지 않습니다.\n" RESET_COLOR);
        write(sock, &size, sizeof(int));
    }
    else // 파일 존재 시
    {
        write(sock, &size, sizeof(int));
        write(sock, buf, BUFSIZE);
        sendfile(sock, fd, NULL, size);

        sock_read(sock);
    }
}

/** 서버의 전송이 끝날때까지 서버로부터 메시지를 읽는 함수
 * @param   sock 메시지를 읽을 서버의 소켓 번호
 */
void sock_read(int sock)
{
    char buf[BUFSIZE];

    while (true)
    {
        read(sock, buf, BUFSIZE);

        if (!strcmp(buf, "")) // 서버가 전송이 끝났음을 의미
            break;
        printf("%s", buf);

        if (!strcmp(buf, "접속을 종료합니다.\n")) // 서버 접속자가 너무 많을 시
        {
            close(sock);
            exit(0);
        }
    }
}
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include<ctype.h>
#include<signal.h>

void get_ip_port(int *ip, int *port, char **args);
void choose_service(int* choose);
void battleship(int sock);
void compile(int sock);
void print_all(char *buf);
int send_file(int sock);
int receive_compiled_file(int sock);

int main(int argc, char *argv[])
{
    if(argc < 2)
    {
        perror("too less args\n");
        exit(-1);
    }
    int ip;
    int port;
    int sock;
    char buf[BUFSIZ];
    int bytes_read;
    struct sockaddr_in addr;

    get_ip_port(&ip, &port, argv);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("socket");
        exit(1);
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = ip;

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("connect");
        exit(2);
    }

    char ch[10];
    recv(sock, buf, BUFSIZ, 0);
    printf("%s", buf);
    fgets(ch, 10, stdin);
    send(sock, &ch, sizeof(int), 0);
    printf("sth\n");
    if(ch[0] == '2')
    {
        while (1)
        {
            bytes_read = recv(sock, buf, BUFSIZ, 0);
            if (bytes_read == 0)
                break;
            print_all(buf);
            fgets(buf, BUFSIZ, stdin);
            send(sock, buf, BUFSIZ, 0);
        }
    }
    else if(ch[0] == '1')
    {
        send_file(sock);
        receive_compiled_file(sock);
    }
    
    close(sock);
    return 0;
}

void print_all(char* buf)
{
    while(*buf != 0)
    {
        printf("%c", *buf);
        buf++;
    }
}

void get_ip_port(int* ip, int* port, char** args)
{
    int tmp[4];
    char buf[56];
    sscanf(args[1], "%d.%d.%d.%d:%d", tmp, tmp + 1, tmp + 2, tmp + 3, port);
    sprintf(buf, "%d.%d.%d.%d", tmp[0], tmp[1], tmp[2], tmp[3]);
    *ip = inet_addr(buf);
}

int send_file(int sock)
{
    FILE *f = NULL;
    char *er;
    char filename[16];
    char buf[BUFSIZ];
    while(f == NULL)
    {
        printf("write path of file to compile: ");
        scanf("%s", filename);
        printf("filename: %s\n", filename);
        f = fopen(filename, "r");
        if (f == NULL)
        {
            perror("can't open c file\n");
            continue;
        }
    }
    while (!feof(f)) // sending file to compile
    {
        er = fgets(buf, sizeof(buf), f);
        if (er == NULL)
        {
            printf("file is read or error\n");
            break;
        }
        printf("%s\n", buf);
        send(sock, buf, sizeof(buf), 0);
    }
    printf("all c file is sent\n");
    shutdown(sock, SHUT_WR);
    //_sleep(3);
    //send(sock, "fin!", 7, 0);
    system("sleep 2");
    fclose(f);

    return 0;
}

void clean_file(const char* filename)
{
    FILE *f;
    f = fopen(filename, "w");
    if(f == NULL)
    {
        perror("can't clear file, cause can't open it\n");
        return;
    }
    fclose(f);
}

int receive_compiled_file(int sock)
{
    char buf[BUFSIZ];
    char filename[16];
    sprintf(filename, "compiled_file");

    FILE *f;
    if(!access(filename, 0))
        clean_file(filename);
    f = fopen(filename, "a");
    if (f == NULL)
    {
        perror("can't create compiled file!\n");
        exit(-1);
    }
    int er;
    while (1)
    {
        er = recv(sock, buf, BUFSIZ, 0);
        printf("msg (size: %d) received\n", er);
        if (er < 0)
            break;
        if (er == 0)
            break;
        fwrite(buf, sizeof(char), er, f);
    }
    fclose(f);
    return 0;
}
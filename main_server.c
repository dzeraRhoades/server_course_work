#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <ctype.h>
#include <signal.h>
#include "battleship/message.h"

#define MSGKEY_RCV 10
#define MSGKEY_SND 25

int get_msg(int shmid, int semid, char *shm_address, int client);
int snd_msg(int shmid, int semid, char *shm_address, int client);
void send_compiled_file(int sock);
int receive_file(int client);

typedef struct
{
    long mtype;
    int is_file_over;
    char msg_text[BUFSIZ];
}qmsg;


#define SEMKEYPATH "battleship/dev"
#define SEMKEYID 1
#define SHMKEYPATH "battleship/dev"
#define SHMKEYID 1
#define NUMSEMS 2

int battleship(int client);
int compile(int client);

int main(int argc, char* argv[])
{
    int sock, client;
    char ip[16] = "127.0.0.1";
    int port = 1111;
    char choose[10];
    char buf[BUFSIZ];

    struct sockaddr_in addr;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("can't create socket\n");
        exit(-1);
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        exit(2);
    }

    listen(sock, 3);

    // launching servers
    // int servs = fork();
    // if(servs == 0)
    // {
    //     execl("battleships/bs", "battleships/bs", NULL);
    // }

    while (1)
    {
        int flag = 0;
        client = accept(sock, NULL, NULL);
        printf("client accepted! PID: %d\n", getpid());
        if (client < 0)
        {
            perror("client can't be accepted");
            close(sock);
        }
        //set_name(filename);
        //printf("c file name: %s", filename);
        switch (fork())
        {
        case 0:
            break;

        default:
            flag = 1;
            break;
        }
        if (flag)
            break;
    }
    sprintf(buf, "1) Remote compile\n2) Battleship\nChoose the service: ");
    send(client, buf, BUFSIZ, 0);
    recv(client, choose, 10, 0);
    if(choose[0] == '1')
    {
        compile(client);
    }
    else if(choose[0] == '2')
    {
        battleship(client);
    }
    else
        perror("wrong between 1 and 2!\n");
    shutdown(client, SHUT_RDWR);

    return 0;
}

int compile(int client)
{
    if(receive_file(client))
        return -1;
    send_compiled_file(client);
    return 0;
}

int battleship(int client)
{
    struct sembuf operations[2];
    msg *shm_address;
    int semid, shmid, rc;
    key_t semkey, shmkey;
    char buf[BUFSIZ];
    int id = getpid();

    semkey = ftok(SEMKEYPATH, SEMKEYID);
    if (semkey == (key_t)-1)
    {
        perror("client: ftok() for sem failed");
        return -1;
    }
    shmkey = ftok(SHMKEYPATH, SHMKEYID);
    if (shmkey == (key_t)-1)
    {
        perror("client: ftok() for shm failed");
        return -1;
    }
    // Получаем уже существующий ID семафоров, свзанный с ключом.
    semid = semget(semkey, NUMSEMS, 0666);
    if (semid == -1)
    {
        perror("client: semget() failed");
        return -1;
    }
    // Получаем уже существующий ID сегмента разделяемой памяти, свзанный с ключом.
    shmid = shmget(shmkey, sizeof(shm_address), 0666);
    if (shmid == -1)
    {
        perror("client: shmget() failed");
        return -1;
    }
    // Присоединяем сегмент разделяемой памяти к процессу клиенту.
    sprintf(buf, "write anything to start the game\n");
    while (1)
    {
        send(client, buf, BUFSIZ, 0);
        recv(client, buf, BUFSIZ, 0);
        operations[0].sem_num = 0;
        operations[0].sem_op = 0;
        operations[0].sem_flg = 0;

        operations[1].sem_num = 1;
        operations[1].sem_op = -1;
        operations[1].sem_flg = 0;
        rc = semop(semid, operations, 2);
        if (rc == -1)
        {
            perror("client: semop() failed");
            return -1;
        }

        shm_address = (msg*)shmat(shmid, NULL, 0);
        if (shm_address == NULL)
        {
            perror("client: shmat() failed");
            return -1;
        }
        printf("server send msg\n");
        strcpy(shm_address->string, buf);
        shm_address->id = id;
        operations[0].sem_num = 0;
        operations[0].sem_op = 1;
        operations[0].sem_flg = 0;

        operations[1].sem_num = 1;
        operations[1].sem_op = 0;
        operations[1].sem_flg = 0;
        rc = semop(semid, operations, 2);
        if (rc == -1)
        {
            perror("client: semop() failed");
            return -1;
        }

        operations[0].sem_num = 0;
        operations[0].sem_op = -1;
        operations[0].sem_flg = 0;

        operations[1].sem_num = 1;
        operations[1].sem_op = -1;
        operations[1].sem_flg = 0;
        rc = semop(semid, operations, 2);
        if (rc == -1)
        {
            perror("client: semop() failed");
            return -1;
        }
        strcpy(buf, shm_address->string);
        printf("server get msg\n");

        operations[0].sem_num = 0;
        operations[0].sem_op = 0;
        operations[0].sem_flg = IPC_NOWAIT;

        operations[1].sem_num = 1;
        operations[1].sem_op = 1;
        operations[1].sem_flg = IPC_NOWAIT;
        rc = semop(semid, operations, 2);
        if (rc == -1)
        {
            perror("client: semop() failed");
            return -1;
        }
        rc = shmdt(shm_address);
        if (rc == -1)
        {
            perror("client: shmdt() failed");
            return -1;
        }
        if(buf[0] == 'f' && buf[1] == 'n')
        {
            shutdown(client, SHUT_RDWR);
            return 0;
        }
    }
}

int receive_file(int client)
{
    int key = MSGKEY_RCV;
    int msgid;
    qmsg my_msg;
    int er;
    char buf[BUFSIZ];
    msgid = msgget(key, 0666);
    if(msgid == -1)
    {
        perror("msgget error\n");
        return -1;
    }
    my_msg.is_file_over = 0;
    my_msg.mtype = getpid();
    while (1)
    {
        er = recv(client, buf, BUFSIZ, 0);
        if (er < 0)
            break;
        if (er == 0)
            break;
        printf("%s", buf);
        strcpy(my_msg.msg_text, buf);
        if (msgsnd(msgid, &my_msg, BUFSIZ, 0) < 0)
        {
            perror("message can't be sent to compile server!!!");
            shutdown(client, SHUT_RDWR);
            exit(-1);
        }
    }
    my_msg.is_file_over = 1;
    msgsnd(msgid, &my_msg, BUFSIZ, 0);
    return 0;
}

void send_compiled_file(int sock)
{
    int msgid;
    int key = MSGKEY_SND;
    qmsg my_msg;
    //char buf[BUFSIZ];
    //int read_num;
    msgid = msgget(key, 0666);
    if (msgid == -1)
    {
        perror("msgget error\n");
        return;
    }
    while (1)
    {
        msgrcv(msgid, &my_msg, BUFSIZ + sizeof(int), getpid(), 0);
        printf("msg of compiled part received\n");
        if(my_msg.is_file_over)
            break;
        send(sock, my_msg.msg_text, BUFSIZ, 0);
    }
    // strcpy(buf, "fin!");
    // send(sock, buf, 5, 0);
    shutdown(sock, SHUT_RDWR);
    system("sleep 2");
    
}
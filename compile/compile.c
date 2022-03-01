#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <sys/wait.h>

#define MSGKEY_RCV 10
#define MSGKEY_SND 25

typedef struct
{
    long mtype;
    int is_file_over;
    char msg_text[BUFSIZ];
} qmsg;

int compile_and_send(qmsg *my_msg, int msgid);


int main(int argc, char* argv[])
{
    int msg_rcv_id, msg_snd_id;
    int key_rcv = MSGKEY_RCV, key_snd = MSGKEY_SND;
    char filename[16];
    qmsg my_msg;
    msg_rcv_id = msgget(key_rcv, 0666 | IPC_CREAT | IPC_EXCL);
    if(msg_rcv_id < 0)
    {
        perror("msgget_rcv error\n");
        exit(-1);
    }
    msg_snd_id = msgget(key_snd, 0666 | IPC_CREAT | IPC_EXCL);
    if(msg_snd_id < 0)
    {
        perror("msgget_snd error\n");
        exit(-1);
    }
    printf("compile server launched succesfully!\n");

    FILE *f;
    while(1)
    {
        if(msgrcv(msg_rcv_id, &my_msg, BUFSIZ, 0, 0) < 0)
        {
            perror("msgrcv error\n");
            continue;
        }
        sprintf(filename, "%d.c", (int)my_msg.mtype);
        if(my_msg.is_file_over)
        {
            if(fork() == 0)
            {
                printf("all file sent\n");
                compile_and_send(&my_msg, msg_snd_id);
                return 0;
            }
            else
                continue;
        }
        else
        {
            f = fopen(filename, "a");
            if(f == NULL)
            {
                perror("file can't be opened\n");
                continue;
            }
            printf("%s", my_msg.msg_text);
            fprintf(f, "%s", my_msg.msg_text);
            fclose(f);
        }

    }
    

    return 0;
}

int compile_and_send(qmsg* my_msg, int msgid)
{
    char filename[16];
    char without_ext[16];
    char command[56];

    sprintf(without_ext, "%d", (int)my_msg->mtype);
    sprintf(filename, "%d.c", (int)my_msg->mtype);
    sprintf(command, "gcc -o %s %s", without_ext, filename);
    printf("command to execute: %s", command);
    system(command);
    FILE *f;
    f = fopen(without_ext, "r");
    if(f == NULL)
    {
        perror("compiled file can;t be opened\n");
        return -1;
    }
    int read_num;
    my_msg->is_file_over = 0;
    while (1)
    {
        if ((read_num = fread(my_msg->msg_text, sizeof(char), BUFSIZ, f)) != BUFSIZ)
        {
            if (feof(f))
            {
                my_msg->is_file_over = 1;
                if (read_num != 0)
                {
                    if (msgsnd(msgid, my_msg, BUFSIZ, 0) < 0)
                    {
                        perror("msg can't be sent\n");
                        break;
                    }
                    printf("full compiled file is sent\n");
                }
                else
                {
                    printf("read_num = 0\n");
                }
                break;
            }
            else
            {
                perror("error while reading file\n");
                exit(-1);
            }
        }
        if (msgsnd(msgid, my_msg, BUFSIZ, 0) < 0)
        {
            perror("msg can't be sent\n");
            break;
        }
        printf("part of compiled file is sent\n");
    }
    fclose(f);
    return 0;
}
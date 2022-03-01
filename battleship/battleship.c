#include "message.h"
#include "list.h"
#include <string.h>
#include <stdlib.h>
#include<time.h>
#include <ctype.h>

#define SEMKEYPATH "dev"
#define SEMKEYID 1
#define SHMKEYPATH "dev"
#define SHMKEYID 1
#define NUMSEMS 2

int get_msg(int shmid, Node **top);
Game *find_game(Node *top, int id);
int play(Game *game, char *buf);
int start_game(Game *game, char *buf);
int menu(Game *game, char *buf);
int menu_choose(Game *game, char *buf);
int set_ships(Game *game, char *buf);
int place_ship(Game *game, int ax, int ay, int bx, int by, int ship_length);
void bot_field_filling(Game *game);
char *draw_ships(Game *game, char *buf);
char *draw_players_map(Game *game, char *buf);
int main_game(Game *game, char *buf);
int bots_move(Game *game, char *buf);
int is_game_over(Game *game, char *buf, int pl_or_bot);
void get_2args(int *x, int *y, char *buf);
void get_4args(int *x1, int *x2, int *y1, int *y2, char *buf);

int main(int argc, char *argv[])
{
    printf("battleships serv launched\n");
    //int games = 0;
    Node* top;
    top->next = NULL;
    int rc, semid, shmid;
    key_t semkey, shmkey;
    msg *shm_address;
    //msg tmp;
    struct sembuf operations[2];
    struct shmid_ds shmid_struct;
    short sarray[NUMSEMS];
    //char str[BUFSIZ];

    /* Генерируем IPC ключи для набора  семафоров и сегмента разделяемой памяти     */
    semkey = ftok(SEMKEYPATH, SEMKEYID);
    //rc = semctl(semid, 1, IPC_RMID);
    if (semkey == (key_t)-1)
    {
        perror("server: ftok() for sem failed");
        return -1;
    }
    shmkey = ftok(SHMKEYPATH, SHMKEYID);
    if (shmkey == (key_t)-1)
    {
        perror("server: ftok() for shm failed");
        return -1;
    }

    /* Создаем набор семафоров на основе IPC ключа. Количество семафоров равно двум.*/
    //rc = semctl(semid, 1, IPC_RMID);
    semid = semget(semkey, NUMSEMS, 0666 | IPC_CREAT | IPC_EXCL);
    if (semid == -1)
    {
        perror("server: semget() failed");
        return -1;
    }
    sarray[0] = 0;
    sarray[1] = 1;
    rc = semctl(semid, 1, SETALL, sarray);
    if (rc == -1)
    {
        perror("server: semctl() initialization failed");
        return -1;
    }
    /* Создаем сегмент разделяемой памяти постоянного размера.*/
    shmid = shmget(shmkey, sizeof(shm_address), 0666 | IPC_CREAT | IPC_EXCL);
    if (shmid == -1)
    {
        perror("server: shmget() failed");
        return -1;
    }
    /* Присоединяем сегмент разделяемой памяти к процессу серверу*/
    // shm_address = (msg*)shmat(shmid, NULL, 0);
    // if (shm_address == NULL)
    // {
    //     perror("server: shmat() failed");
    //     return -1;
    // }
    while (1)
    {
        operations[0].sem_num = 0;
        operations[0].sem_op = -1;
        operations[0].sem_flg = 0;

        operations[1].sem_num = 1;
        operations[1].sem_op = 0;
        operations[1].sem_flg = 0;
        rc = semop(semid, operations, 2);
        if (rc == -1)
        {
            perror("server: semop() failed");
            return -1;
        }
        printf("bs get msg\n");
        get_msg(shmid, &top);
        printf("bs send msg\n");

        operations[0].sem_num = 0;
        operations[0].sem_op = 1;
        operations[0].sem_flg = IPC_NOWAIT;

        operations[1].sem_num = 1;
        operations[1].sem_op = 1;
        operations[1].sem_flg = IPC_NOWAIT;
        rc = semop(semid, operations, 2);
        if (rc == -1)
        {
            perror("server: semop() failed");
            return -1;
        }
    }
    rc = semctl(semid, 1, IPC_RMID);
    if (rc == -1)
    {
        perror("server: semctl() remove id failed");
        return -1;
    }
    rc = shmdt(shm_address);
    if (rc == -1)
    {
        perror("server: shmdt() failed");
        return -1;
    }
    rc = shmctl(shmid, IPC_RMID, &shmid_struct);
    if (rc == -1)
    {
        perror("server: shmctl() failed");
        return -1;
    }
    return 0;
}

int get_msg(int shmid, Node** top)
{
    int play_res;
    int rc;
    msg *message;
    message = (msg *)shmat(shmid, NULL, 0);
    char buf[BUFSIZ];
    Game *tmp;

    strcpy(buf, message->string);
    printf("Got msg: %s\n", buf);
    tmp = find_game(*top, message->id);
    if (tmp == NULL)
    {
        pushNode(top, message->id);
        play_res = play(&((*top)->game), buf);
    }
    else
        play_res = play(tmp, buf);

    if(play_res)
    {
        eraseNode(message->id, top);
        sprintf(buf, "fn");
        printf("deleting game\n");
    }
    strcpy(message->string, buf);
    rc = shmdt(message);
    if (rc == -1)
    {
        perror("server: shmdt() failed");
        return -1;
    }
    return 0;
}

Game* find_game(Node* top, int id)
{
    if(top == NULL)
        return NULL;
    Node *tmp = top;
    while(tmp->game.id != id)
    {
        tmp = tmp->next;
        if(tmp == NULL)
            return NULL;
    }
    return &tmp->game;
}

int play(Game* game, char* buf)
{
    if(game->status == START)
    {
        start_game(game, buf);
    }
    else if(game->status == MENU)
    {
        menu(game, buf);
    }
    else if(game->status == MENU_CHOOSE)
    {
        menu_choose(game, buf);
    }
    else if(game->status == SET)
    {
        printf("set\n");
        set_ships(game, buf);
    }
    else if(game->status == PLAY)
    {
        main_game(game, buf);
    }
    else if(game->status == FINISHED)
    {
        return 1;
    }
    return 0;
}

int start_game(Game* game, char* buf)
{
    bot_field_filling(game);
    for (int i = 0; i < 10; i++)
    {
        for (int j = 0; j < 10; j++)
            game->map[i][j] = ' ';
    }
    for (int i = 0; i < 4; i++)
        game->ships[i] = 4 - i;

    menu(game, buf);
    return 0;
}

int menu(Game* game, char* buf)
{
    sprintf(buf, "\t\tBattleships\n1) Play\n2) Instruction\nChoose item: ");
    game->status = MENU_CHOOSE;
    return 0;
}

int menu_choose(Game* game, char* buf)
{
    if (buf[0] == '1')
    {
        game->status = SET;
        sprintf(buf, "Place your 1-deck ship on the map (example: a4): ");
        printf("place your ship\n");
    }
    else if (buf[0] == '2')
    {
        sprintf(buf, "You have 4 1-deck ships, 3 2-deck ships, 2 3-deck ships, 1 4-deck ships.\n\
        Set them on your map, but don't try to put them too close to each other.\n\
        Then, when the game starts, you and your enemy (bot) will hame to make a move in turms.\n\
        Your goal is to destroy all enemie's ships faster, then he destroys yours. One heats the\n\
        target will move one more time. Good luck!\nEnter anything to continue\n");
        game->status = MENU;
    }
    else
        return -1;
    return 0;
}

int set_ships(Game* game, char* buf)
{
    printf("set ships in: %s\n", buf);
    int x1, x2, y1, y2;

    if(game->ships[0] != 0)
    {
        
        //sscanf(buf, "%c%d", &y1, &x1);
        get_2args(&x1, &y1, buf);
        if(place_ship(game, x1, y1, x1, y1, 1))
        {
            sprintf(buf, "sth is wrong, try again\n");
            return 0;
        }
        if(game->ships[0] == 0)
            sprintf(draw_players_map(game, buf), "Set 2-deck ship (example: a4 a5): ");
        else
            sprintf(draw_players_map(game, buf), "Set 1-deck ship (example: a7): ");
    }
    else if(game->ships[1] != 0)
    {
        get_4args(&x1, &x2, &y1, &y2, buf);
        if (place_ship(game, x1, y1, x2, y2, 2))
        {
            sprintf(buf, "sth is wrong, try again\n");
            return 0;
        }
        if (game->ships[1] == 0)
            sprintf(draw_players_map(game, buf), "Set 3-deck ship (example: a4 a6): ");
        else
            sprintf(draw_players_map(game, buf), "Set 2-deck ship (example: a7 a8): ");
    }
    else if (game->ships[2] != 0)
    {
        get_4args(&x1, &x2, &y1, &y2, buf);
        if (place_ship(game, x1, y1, x2, y2, 3))
        {
            sprintf(buf, "sth is wrong, try again\n");
            return 0;
        }
        if (game->ships[2] == 0)
            sprintf(draw_players_map(game, buf), "Set 4-deck ship (example: a4 a7): ");
        else
            sprintf(draw_players_map(game, buf), "Set 3-deck ship (example: a4 a6): ");
    }
    else if (game->ships[3] != 0)
    {
        get_4args(&x1, &x2, &y1, &y2, buf);
        if (place_ship(game, x1, y1, x2, y2, 4))
        {
            sprintf(buf, "sth is wrong, try again\n");
            return 0;
        }
        if (game->ships[3] == 0)
        {
            sprintf(draw_ships(game, buf), "All ships are placed, do your move: ");
            game->status = PLAY;
        }
        else
            sprintf(draw_players_map(game, buf), "Set 4-deck ship (example: a4 a7): ");
    }
    return 0;
}

void get_2args(int* x, int* y, char* buf)
{
    int _x = 0;
    int _y = 0;
    _y = buf[0] - 'a';
    _x = buf[1] - '0';
    if(isdigit(buf[2]))
        _x = 10;
    *x = _x - 1;
    *y = _y;
}

void get_4args(int *x1, int *x2, int *y1, int *y2, char *buf)
{
    int _x1 = 0, _x2 = 0;
    int _y1 = 0, _y2 = 0;
    int i = 0;
    _y1 = buf[i] - 'a';
    i++;
    _x1 = buf[i] - '0';
    i++;
    if(isdigit(buf[i]))
    {
        i++;
        _x1 = 10;
    }
    i++;
    _y2 = buf[i] - 'a';
    i++;
    _x2 = buf[i] - '0';
    i++;
    if(isdigit(buf[i]))
    {
        _x2 = 10;
    }
    *x1 = _x1 - 1;
    *x2 = _x2 - 1;
    *y1 = _y1;
    *y2 = _y2;
    printf("x1: %d. x2: %d. y1: %d. y2: %d\n", *x1, *x2, *y1, *y2);
}

int place_ship(Game *game, int ax, int ay, int bx, int by, int ship_length)
{
    if(ax == bx)
    {
        if(abs(ay-by)+1 != ship_length)
        {
            printf("here_1\n");
            return -1;
        }
        if(ay > by)
        {
            int tmp = ay;
            ay = by;
            by = ay;
        }
    }
    else if(ay == by)
    {
        if(abs(ax-bx)+1 != ship_length)
        {
            printf("here_1.1\n");
            return -1;
        }
        if(ax > bx)
        {
            int tmp = ax;
            ax = bx;
            bx = tmp;
        }
    }
    else
        return -1;
    int i, j;
    if (bx > 9 || by > 9)
        return -1;

    for (i = ay - 1; i <= by + 1; i++)
    {
        for (j = ax - 1; j <= bx + 1; j++)
        {
            if (i < 0 || i > 9 || j < 0 || j > 9)
                continue;
            if (game->map[j][i] != ' ')
            {
                printf("here_2\n");
                return -1;
            }
                
        }
    }
    for (i = ay; i <= by; i++)
        for (j = ax; j <= bx; j++)
            game->map[j][i] = 35;

    switch (ship_length)
    {
    case 1:
        game->ships[0]--;
        printf("1-deck ships left: %d\n", game->ships[0]);
        break;
    case 2:
        game->ships[1]--;
        break;

    case 3:
        game->ships[2]--;
        break;

    case 4:
        game->ships[3]--;
        break;
    }
    return 0;
}

void bot_field_filling(Game* game)
{
    int i, j, k, h;
    int mistake = 0;
    int x, y;
    int vh;
    int shipl = 1;
    srand(time(NULL));

    // make all field empty
    for (i = 0; i < 10; i++)
        for (j = 0; j < 10; j++)
            game->bot_map[i][j] = ' ';

    //using rand() we fill all ships
    for (i = 0; i < 4; i++)
    {
        shipl = 4 - i;

        while (shipl)
        {
            // shipl = i;
            mistake = 0;
            x = rand() % 10;
            y = rand() % 10;
            vh = rand() % 2;
            if (vh)
            {
                if (x + i > 9)
                    continue;
                for (h = y - 1; h <= y + 1; h++)
                {
                    if (mistake)
                        break;
                    for (j = x - 1; j <= (x + i) + 1; j++)
                    {
                        if (h < 0 || h > 9 || j < 0 || j > 9)
                            continue;
                        if (game->bot_map[h][j] == '#')
                        {
                            mistake = 1;
                            break;
                        }
                    }
                }
                if (!mistake)
                {
                    for (k = 0; k < i + 1; k++)
                    {
                        game->bot_map[y][x + k] = '#';
                    }
                    shipl--;
                }
            }
            else
            {
                if (y + i > 9)
                    continue;

                for (h = y - 1; h <= (y + i) + 1; h++)
                {
                    if (mistake)
                        break;
                    for (j = x - 1; j <= x + 1; j++)
                    {
                        if (h < 0 || h > 9 || j < 0 || j > 9)
                            continue;
                        if (game->bot_map[h][j] == '#')
                        {
                            mistake = 1;
                            break;
                        }
                    }
                }
                if (!mistake)
                {
                    for (k = 0; k < i + 1; k++)
                    {
                        game->bot_map[y + k][x] = '#';
                    }
                    shipl--;
                }
            }
        }
    }
}

char* draw_ships(Game* game, char* buf)
{
    sprintf(buf, "  1 2 3 4 5 6 7 8 9 10  1 2 3 4 5 6 7 8 9 10");
    int p = strlen(buf);
    for (int i = 0; i < 10; i++)
    {
        buf[p] = '\n';
        p++;
        buf[p] = ' ';
        p++;

        for (int j = 0; j < 22; j++)
        {
            if (j == 0)
                buf[p] = i + 'a';
            else if (j == 11)
            {
                buf[p] = ' ';
                p++;
                buf[p] = i + 'a';
            }
            else if (j < 11)
            {
                buf[p] = game->map[j - 1][i];
                p++;
                buf[p] = ' ';
            }
            else
            {
                buf[p] = game->bot_map[j - 12][i];
                p++;
                buf[p] = ' ';
            }
            p++;
        }
    }
    buf[p] = '\n';
    p++;
    return buf + p;
}
char* draw_players_map(Game* game, char* buf)
{
    sprintf(buf, "  1 2 3 4 5 6 7 8 9 10");
    int p = strlen(buf);
    for (int i = 0; i < 10; i++)
    {
        buf[p] = '\n';
        p++;
        buf[p] = ' ';
        p++;

        for (int j = 0; j < 11; j++)
        {
            if(j == 0)
                buf[p] = i + 'a';
            else
                {
                    buf[p] = game->map[j-1][i];
                    p++;
                    buf[p] = ' ';
                }
            p++;
        }
    }
    buf[p] = '\n';
    p++;
    return buf + p;
}

int main_game(Game* game, char* buf)
{
    int x;
    int y;
    get_2args(&y, &x, buf);
    printf("we shoot in: x: %d, y: %d\n", x, y);

    if(x > 9 || x < 0 || y > 9 || y < 0)
    {
        sprintf(buf, "wrong coordinates, try again: ");
        return 0;
    }
    if(game->bot_map[y][x] == '#')
    {
        game->bot_map[(int)y][x] = 'x';
        if (is_game_over(game, buf, 0))
            return 0;
        sprintf(draw_ships(game, buf), "You got it, move again: ");
        return 0;
    }
    else if (game->bot_map[(int)y][x] == '*' || game->bot_map[(int)y][x] == 'x')
    {
        sprintf(buf, "You've already shooted this place, try again: ");
        return 0;
    }
    else
    {
        game->bot_map[(int)y][x] = '*';
        bots_move(game, buf);
        return 0;
    }
}

int bots_move(Game* game, char* buf)
{
    int x, y;
    int hits = 0;
    srand(time(NULL));
    while (1)
    {
        x = rand() % 10;
        y = rand() % 10;
        if(game->map[y][x] == ' ')
        {
            game->map[y][x] = '*';
            if(hits != 0)
            {
                sprintf(draw_ships(game, buf), "You missed, bot got hit %d times. Make move: ", hits);
                break;
            }
            sprintf(draw_ships(game, buf), "You missed, so is bot. Make move: ");
            break;
        }
        else if (game->map[y][x] == '*' || game->map[y][x] == 'x')
        {
            continue;
        }
        else
        {
            game->map[y][x] = 'x';
            hits++;
            if(is_game_over(game, buf, 1))
                return 0;
            continue;
        }
    }
    return 0;
}

int is_game_over(Game* game, char* buf, int pl_or_bot)
{
    if(pl_or_bot == 0)
    {
        for (int i = 0; i < 10; i++)
        {
            for (int j = 0; j < 10; j++)
            {
                if (game->bot_map[i][j] == '#')
                    return 0;
            }
        }
        printf("You've won the game)\nPrint anuthing to quit\n");
        sprintf(buf, "You've won the game)\nPrint anuthing to quit\n");
        game->status = FINISHED;
        return 1;
    }
    else
    {
        for (int i = 0; i < 10; i++)
        {
            for (int j = 0; j < 10; j++)
            {
                if (game->map[i][j] == '#')
                    return 0;
            }
        }
        sprintf(buf, "You've lost the game(\nPrint anuthing to quit\n");
        game->status = FINISHED;
        return 1;
    }
}
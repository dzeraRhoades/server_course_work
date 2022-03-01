#include <malloc.h>

typedef enum
{
    START,
    MENU,
    MENU_CHOOSE,
    SET,
    PLAY,
    FINISHED
} Status;

typedef struct
{
    int id;
    Status status;
    int ships[4];
    int map[10][10];
    int bot_map[10][10];
} Game;

typedef struct Node
{
    Game game;
    struct Node *next;
} Node;
void pushNode(Node **top, int id)
{
    Node *new_node = (Node*)malloc(sizeof(Node));
    new_node->next = *top;
    new_node->game.id = id;
    *top = new_node;
}

int eraseNode(int id, Node **top)
{
    if (*top == NULL)
        return -1;
    Node *tmp = *top;
    Node *tmp2 = NULL;
    while (tmp->game.id != id)
    {
        tmp2 = tmp;
        tmp = tmp->next;
        if (tmp == NULL)
            return -1;
    }
    if (tmp2 != NULL)
    {
        tmp2->next = tmp->next;
    }
    else
    {
        *top = NULL;
    }
    free(tmp);
    return 0;
}


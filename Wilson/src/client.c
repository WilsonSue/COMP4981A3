#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ncurses.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#define MAXLINE 1024
#define TEN 10
#define MAX_PLAYERS 27
#define TEN_MILL 1000000

typedef struct
{
    int x;
    int y;
} Player;

void update_positions(const char *message, Player *players, int *num_players);
void draw_players(Player *players, int num_players);

void update_positions(const char *message, Player *players, int *num_players)
{
    int         i = 0;
    const char *p = message;
    char       *endPtr;

    while(*p != '\0' && i < MAX_PLAYERS)
    {
        int newX;
        int newY;
        newX = (int)strtol(p, &endPtr, TEN);
        if(p == endPtr)
        {
            break;
        }
        p = endPtr;

        newY = (int)strtol(p, &endPtr, TEN);
        if(p == endPtr)
        {
            break;
        }
        p = endPtr;

        while(*p == ' ' || *p == ',')
        {
            p++;
        }

        players[i].x = newX;
        players[i].y = newY;
        i++;
    }

    *num_players = i;
}

void draw_players(Player *players, int num_players)
{
    clear();
    for(int i = 0; i < num_players; i++)
    {
        mvprintw(players[i].y, players[i].x, "O");
    }
    refresh();
}

int main(int argc, const char *argv[])
{
    int                sockfd;
    struct sockaddr_in servaddr;
    char               buffer[MAXLINE];
    Player             players[MAX_PLAYERS];
    int                num_players = 0;
    long               server_port;
    int                flags;
    int                running;
    struct timespec    delay;

    if(argc != 3)
    {
        fprintf(stderr, "Usage: %s <Server IP> <Server Port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    server_port = strtol(argv[2], NULL, TEN);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_port        = htons((uint16_t)server_port);
    servaddr.sin_addr.s_addr = inet_addr(argv[1]);

    flags = fcntl(sockfd, F_GETFL, 0);
    if(flags == -1)
    {
        perror("fcntl F_GETFL");
        exit(EXIT_FAILURE);
    }

    if(fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        perror("fcntl F_SETFL O_NONBLOCK");
        exit(EXIT_FAILURE);
    }

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);

    running = 1;

    delay.tv_sec  = 0;
    delay.tv_nsec = (__syscall_slong_t)TEN * TEN_MILL;

    while(running)
    {
        ssize_t n;

        int ch = getch();

        switch(ch)
        {
            case KEY_UP:
                snprintf(buffer, MAXLINE, "0 -1");
                break;
            case KEY_DOWN:
                snprintf(buffer, MAXLINE, "0 1");
                break;
            case KEY_LEFT:
                snprintf(buffer, MAXLINE, "-1 0");
                break;
            case KEY_RIGHT:
                snprintf(buffer, MAXLINE, "1 0");
                break;
            case 'q':
                running = 0;
                break;
            default:
                break;    // No need for 'continue' here
        }

        if(ch != ERR)
        {    // Only send if a key was pressed
            if(sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
            {
                perror("sendto failed");
                break;
            }
        }

        // Continuously check for incoming data
        while((n = recvfrom(sockfd, buffer, MAXLINE, 0, NULL, NULL)) > 0)
        {
            buffer[n] = '\0';
            update_positions(buffer, players, &num_players);
            draw_players(players, num_players);
        }

        if(n < 0 && (errno != EAGAIN && errno != EWOULDBLOCK))
        {
            perror("recvfrom failed");
            break;
        }

        nanosleep(&delay, NULL);
    }

    endwin();
    close(sockfd);
    return 0;
}

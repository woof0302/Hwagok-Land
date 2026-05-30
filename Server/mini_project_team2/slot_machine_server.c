// slot_machine_server.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <termios.h>
#include <ctype.h>  
#include <signal.h>
#include <time.h>

#define SERVER_PORT 25003
#define MAX_BUF     256

struct termios original_term;  // 터미널 원래 설정

// 게임 관련 변수
#define CLIENT_INIT_COIN 100
#define CLIENT_GAME_COST 10
int client_coin = CLIENT_INIT_COIN;  // 초기 코인

// =================================================================
// 터미널/시그널 관련 함수
// =================================================================
void restore_terminal_mode(void) 
{
    tcsetattr(STDIN_FILENO, TCSANOW, &original_term);
}

void signal_handler(int signum) 
{
    restore_terminal_mode();
    exit(EXIT_SUCCESS);
}

void set_terminal_mode(void) 
{
    tcgetattr(STDIN_FILENO, &original_term);
    struct termios new_term = original_term;
    new_term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_term);
    atexit(restore_terminal_mode);
    signal(SIGINT, signal_handler);
}

int main() 
{
    int pid = getpid();
    srand(time(NULL));
    printf("Server PID: %d\n", pid);
    
    int sfd, len, ret;
    int reward = 0;
    char buf[MAX_BUF];

    struct sockaddr_in addr_client;
    socklen_t addr_client_len = sizeof(addr_client);

    struct timeval tv           = {0, 500000};
    struct timeval tv_restart   = {0, 500000};

    fd_set fds, restart_fds;
    
    FD_ZERO(&restart_fds);
    FD_SET(sfd, &restart_fds);

    // UDP 소켓 생성 및 바인드
    sfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sfd == -1) 
    {
        printf("UDP socket error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    memset(&addr_client, 0, sizeof(addr_client));
    addr_client.sin_family = AF_INET;
    addr_client.sin_addr.s_addr = htonl(INADDR_ANY);
    addr_client.sin_port = htons(SERVER_PORT);
    if(bind(sfd, (struct sockaddr *)&addr_client, sizeof(addr_client)) == -1) 
    {
        printf("[%d] UDP bind error: %s\n", pid, strerror(errno));
        exit(EXIT_FAILURE);
    }

    for(;;)
    {
        FD_ZERO(&fds);
        FD_SET(sfd, &fds);
        int sel_ret = select(sfd + 1, &fds, NULL, NULL, &tv);
        if (sel_ret > 0 && FD_ISSET(sfd, &fds)) 
        {
            len = recvfrom(sfd, buf, MAX_BUF, 0, (struct sockaddr *)&addr_client, &addr_client_len);
            if(len > 0) 
            {
                buf[len] = '\0';
                printf("[%d] Client detected! : %s\n", pid, buf);
                break;
            }
        }
    }

    printf("[%d] Game Start!\n", pid);
    for(;;)
    {
        if(client_coin <= 0) 
        {
            // UDP 입력 감지
            ret = select(sfd + 1, &restart_fds, NULL, NULL, &tv_restart);
            if (ret > 0 && FD_ISSET(sfd, &restart_fds)) {
                len = recvfrom(sfd, buf, MAX_BUF, 0, (struct sockaddr *)&addr_client, &addr_client_len);
                if (len > 0) 
                {
                    buf[len] = '\0';
                    if (strcasecmp(buf, "r") == 0) 
                    {
                        printf("[%d] Server: Restarting game...\n", pid);

                        client_coin = CLIENT_GAME_COST;
                        sprintf(buf, "%d", client_coin);
                        sendto(sfd, buf, sizeof(buf), 0, (struct sockaddr *)&addr_client, sizeof(addr_client));
                        break;
                    }
                }
            }
        }

        // Game Loop
        for(;;)
        {
            len = recvfrom(sfd, buf, MAX_BUF, 0, (struct sockaddr *)&addr_client, &addr_client_len);
            if (len > 0) 
            {
                buf[len] = '\0';
                printf("%s\n", buf);
                if (strncmp(buf, "s", 1) == 0) 
                {
                    printf("[%d] Server: Slot Stop...\n", pid);

                    client_coin -= 10;
                    sprintf(buf, "%d", client_coin);
                    sendto(sfd, buf, sizeof(buf), 0, (struct sockaddr *)&addr_client, sizeof(addr_client));
                    break;
                }
                else if (strncmp(buf, "q", 1) == 0) 
                {
                    sprintf(buf, "Game End\n");
                    sendto(sfd, buf, sizeof(buf), 0, (struct sockaddr *)&addr_client, sizeof(addr_client));
                    
                    close(sfd);
                    printf("Game Over!\n");
                    return EXIT_SUCCESS;
                }
            }
        }
        
        len = recvfrom(sfd, buf, MAX_BUF, 0, (struct sockaddr *)&addr_client, &addr_client_len);
        if (len > 0) 
        {
            printf("Result : %s\n", buf);

            if(buf[0] == buf[1] && buf[1] == buf[2]) 
            {
                if(buf[0] == 7) { reward = 1000; }
                else            { reward = 100;  } 
            } 
            else if(buf[0] == buf[1] || buf[0] == buf[2] || buf[1] == buf[2]) 
            {
                reward = 10;
            }
            else 
            { 
                reward = 0; 
            }
            
            sprintf(buf, "%d", reward);
            printf("%s\n", buf);
            sendto(sfd, buf, sizeof(buf), 0, (struct sockaddr *)&addr_client, sizeof(addr_client));
            
            client_coin += reward;
        }
    }
        
    close(sfd);
    printf("Game Over!\n");
    return EXIT_SUCCESS;
}
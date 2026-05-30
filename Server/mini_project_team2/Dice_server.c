#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define PORT 25001
#define BUFFER_SIZE 1024

// 주사위를 굴리는 함수 (1~6)
void roll_dice(int *dice1, int *dice2) 
{
    *dice1 = rand() % 6 + 1;
    *dice2 = rand() % 6 + 1;
}

int main() 
{
    int server_socket;
    struct sockaddr_in server_addr, client_addr;
    char buffer[BUFFER_SIZE];
    socklen_t addr_len = sizeof(client_addr);

    srand(time(NULL));

    server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_socket < 0) 
    {
        perror("소켓 생성 실패");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;  
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) 
    {
        perror("바인딩 실패");
        return 1;
    }
    printf("Server running...\n");

    while (1) 
    {
        int recv_len = recvfrom(server_socket, buffer, BUFFER_SIZE - 1, 0,
                                  (struct sockaddr *)&client_addr, &addr_len);
        if (recv_len < 0) 
        {
            perror("recvfrom 실패");
            continue;
        }
        buffer[recv_len] = '\0';
        printf("Received message: %s\n", buffer);

        if (strcmp(buffer, "1") == 0) 
        {
            int s1, s2, c1, c2;
            roll_dice(&s1, &s2);
            roll_dice(&c1, &c2);
            char result[BUFFER_SIZE];
            sprintf(result, "%d %d %d %d", s1, s2, c1, c2);
            int sent = sendto(server_socket, result, strlen(result) + 1, 0,
                              (struct sockaddr *)&client_addr, addr_len);
            
            if (sent < 0)   { perror("sendto 실패");                } 
            else            { printf("Sent result: %s\n", result); }
        } else {
            printf("Unknown message: %s\n", buffer);
        }

        recv_len = recvfrom(server_socket, buffer, BUFFER_SIZE - 1, 0,
            (struct sockaddr *)&client_addr, &addr_len);
        if (recv_len < 0)  { perror("recvfrom 실패");  }
        printf("Game Ends.. \n");

        close(server_socket);

        return 0;
    }

    close(server_socket);

    return 0;
}

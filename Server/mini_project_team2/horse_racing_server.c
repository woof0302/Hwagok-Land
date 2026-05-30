#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define PORT 25500
#define HORSES 8
#define TRACK_LENGTH 360  // 트랙의 한 바퀴 (0~359)

typedef struct 
{
    int id;
    int position;
    int finished;
} Horse;

// 말들의 위치 정보를 클라이언트에게 전송하는 함수
void send_positions(int sockfd, struct sockaddr_in *client_addr, Horse horses[]) 
{
    char buffer[256];
    memset(buffer, 0, sizeof(buffer));

    for (int i = 0; i < HORSES; i++) 
    {
        char temp[16];
        sprintf(temp, "%d ", horses[i].position);
        strcat(buffer, temp);
    }

    sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)client_addr, sizeof(*client_addr));
}

int main() 
{
    int sockfd; 
    int recv_len; int i = 0;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[256];

    // 난수 시드 초기화 (매 실행마다 다른 난수 순서를 위해)
    srand(time(NULL));

    // UDP 소켓 생성
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
    {
        perror("소켓 생성 실패");
        exit(EXIT_FAILURE);
    }

    // 서버 주소 설정
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // 소켓 바인딩
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) 
    {
        perror("바인딩 실패");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("서버 시작됨. 클라이언트의 경주 시작 신호를 기다리는 중...\n");

    // 클라이언트 신호 대기 (클라이언트로부터 "1"을 받을 때까지)
    while (1) 
    {
        recv_len = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &addr_len);
        if (recv_len > 0) 
        {   
            buffer[recv_len] = '\0';
            printf("수신한 시작 신호: %s\n", buffer);
            if (strcmp(buffer, "1") == 0) 
            {
                printf("클라이언트 %s로부터 게임 시작 신호 수신\n", inet_ntoa(client_addr.sin_addr));
                break;
            }
        } 
        else 
        {
            perror("recvfrom 실패");
        }
    }

    // 말 초기화
    Horse horses[HORSES];
    for (int i = 0; i < HORSES; i++) 
    {
        horses[i].id = i;
        horses[i].position = 0;
        horses[i].finished = 0;
    }

    int finished_count = 0;
    int rank = 1;
    int rankings[HORSES] = {0};  // 말들의 순위 저장

    // 경주 진행 (3마리의 말이 완주할 때까지)
    while (finished_count < 3) 
    {
        for (int i = 0; i < HORSES; i++) 
        {
            if (!horses[i].finished) 
            {
                horses[i].position += rand() % 5;  // 0~4 범위 내 랜덤 이동
                
                if (horses[i].position >= TRACK_LENGTH) 
                {
                    horses[i].finished = 1;
                    rankings[i] = rank++;
                    finished_count++;
                }
            }
        }

        // 현재 말들의 위치를 클라이언트에 전송
        send_positions(sockfd, &client_addr, horses);
        usleep(100000);  // 0.1초 대기
    }
    
    printf("경주 종료!\n");

    close(sockfd);
    return 0;
}

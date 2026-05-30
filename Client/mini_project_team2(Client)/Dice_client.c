#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <time.h>

#define SERVER_IP "10.10.141.29"
#define PORT 25001
#define BUFFER_SIZE 1024

// 색상 정의 (RGB 24비트)
#define COLOR_BLACK 0x000000
#define COLOR_WHITE 0xFFFFFF
#define COLOR_RED   0xFF0000
#define COLOR_IVORY 0xFFFFF0  // 아이보리색

// 전역 변수 (프레임버퍼)
struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;
char *fb_map = NULL;
int fb_fd = -1;
int fb_size = 0;

// 프레임버퍼 초기화
int init_framebuffer() {
    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd == -1) {
        perror("프레임버퍼 열기 실패");
        return -1;
    }
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        perror("FBIOGET_VSCREENINFO 실패");
        return -1;
    }
    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) == -1) {
        perror("FBIOGET_FSCREENINFO 실패");
        return -1;
    }
    fb_size = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
    fb_map = mmap(0, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb_map == MAP_FAILED) {
        perror("mmap 실패");
        return -1;
    }
    return 0;
}

void close_framebuffer() {
    if (fb_map)
        munmap(fb_map, fb_size);
    if (fb_fd != -1)
        close(fb_fd);
}

// (x,y)부터 w×h 크기의 사각형을 color 색상으로 그립니다.
void draw_rect(int x, int y, int w, int h, unsigned int color) {
    int xx, yy;
    int location = 0;
    for (yy = y; yy < (y + h); yy++) {
        for (xx = x; xx < (x + w); xx++) {
            location = xx * (vinfo.bits_per_pixel / 8) + yy * finfo.line_length;
            if (vinfo.bits_per_pixel == 32) { // 32bpp
                *(unsigned int *)(fb_map + location) = color;
            } else { // 16bpp
                int r = color >> 16;
                int g = (color >> 8) & 0xff;
                int b = color & 0xff;
                *(unsigned short *)(fb_map + location) =
                    (unsigned short)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
            }
        }
    }
}

// (x,y)에 한 픽셀을 그립니다.
void draw_pixel(int x, int y, unsigned int color) {
    if (x < 0 || x >= vinfo.xres || y < 0 || y >= vinfo.yres)
        return;
    int location = x * (vinfo.bits_per_pixel / 8) + y * finfo.line_length;
    if (vinfo.bits_per_pixel == 32) {
        *(unsigned int *)(fb_map + location) = color;
    } else {
        int r = color >> 16;
        int g = (color >> 8) & 0xff;
        int b = color & 0xff;
        *(unsigned short *)(fb_map + location) =
            (unsigned short)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
    }
}

// 주사위를 그리는 함수  
// x, y: 주사위의 왼쪽 상단 좌표, size: 주사위의 가로/세로 크기, value: 1~6
void draw_dice(int x, int y, int size, int value) {
    // 주사위 배경 (흰색)
    draw_rect(x, y, size, size, COLOR_WHITE);
    
    // 주사위 테두리
    draw_rect(x, y, size, 2, COLOR_BLACK);              // 상단 테두리
    draw_rect(x, y + size - 2, size, 2, COLOR_BLACK);     // 하단 테두리
    draw_rect(x, y, 2, size, COLOR_BLACK);                // 좌측 테두리
    draw_rect(x + size - 2, y, 2, size, COLOR_BLACK);     // 우측 테두리

    int pip_size = size / 6;
    int left   = x + size / 4;
    int right  = x + size - size / 4;
    int top    = y + size / 4;
    int bottom = y + size - size / 4;
    int centerX = x + size / 2;
    int centerY = y + size / 2;
    
    switch (value) {
        case 1:
            draw_rect(centerX - pip_size / 2, centerY - pip_size / 2, pip_size, pip_size, COLOR_BLACK);
            break;
        case 2:
            draw_rect(left - pip_size / 2, top - pip_size / 2, pip_size, pip_size, COLOR_BLACK);
            draw_rect(right - pip_size / 2, bottom - pip_size / 2, pip_size, pip_size, COLOR_BLACK);
            break;
        case 3:
            draw_rect(left - pip_size / 2, top - pip_size / 2, pip_size, pip_size, COLOR_BLACK);
            draw_rect(centerX - pip_size / 2, centerY - pip_size / 2, pip_size, pip_size, COLOR_BLACK);
            draw_rect(right - pip_size / 2, bottom - pip_size / 2, pip_size, pip_size, COLOR_BLACK);
            break;
        case 4:
            draw_rect(left - pip_size / 2, top - pip_size / 2, pip_size, pip_size, COLOR_BLACK);
            draw_rect(right - pip_size / 2, top - pip_size / 2, pip_size, pip_size, COLOR_BLACK);
            draw_rect(left - pip_size / 2, bottom - pip_size / 2, pip_size, pip_size, COLOR_BLACK);
            draw_rect(right - pip_size / 2, bottom - pip_size / 2, pip_size, pip_size, COLOR_BLACK);
            break;
        case 5:
            draw_rect(left - pip_size / 2, top - pip_size / 2, pip_size, pip_size, COLOR_BLACK);
            draw_rect(right - pip_size / 2, top - pip_size / 2, pip_size, pip_size, COLOR_BLACK);
            draw_rect(centerX - pip_size / 2, centerY - pip_size / 2, pip_size, pip_size, COLOR_BLACK);
            draw_rect(left - pip_size / 2, bottom - pip_size / 2, pip_size, pip_size, COLOR_BLACK);
            draw_rect(right - pip_size / 2, bottom - pip_size / 2, pip_size, pip_size, COLOR_BLACK);
            break;
        case 6:
            draw_rect(left - pip_size / 2, top - pip_size / 2, pip_size, pip_size, COLOR_BLACK);
            draw_rect(left - pip_size / 2, centerY - pip_size / 2, pip_size, pip_size, COLOR_BLACK);
            draw_rect(left - pip_size / 2, bottom - pip_size / 2, pip_size, pip_size, COLOR_BLACK);
            draw_rect(right - pip_size / 2, top - pip_size / 2, pip_size, pip_size, COLOR_BLACK);
            draw_rect(right - pip_size / 2, centerY - pip_size / 2, pip_size, pip_size, COLOR_BLACK);
            draw_rect(right - pip_size / 2, bottom - pip_size / 2, pip_size, pip_size, COLOR_BLACK);
            break;
        default:
            break;
    }
}

// 두 개의 주사위를 동시에 애니메이션하는 함수  
// (x1, y)와 (x2, y)에 위치한 주사위를 총 7회 동안 랜덤값으로 바꾸다가 마지막에 최종값(final_value1, final_value2)을 표시함.
// 초반엔 빠르게(50ms) 바뀌고, 반복할수록 딜레이가 늘어납니다.
void animate_two_dice(int x1, int x2, int y, int size, int final_value1, int final_value2) {
    int i;
    for (i = 0; i < 7; i++) {
        int value1, value2;
        if (i == 6) {
            value1 = final_value1;
            value2 = final_value2;
        } else {
            value1 = rand() % 6 + 1;
            value2 = rand() % 6 + 1;
        }
        draw_dice(x1, y, size, value1);
        draw_dice(x2, y, size, value2);
        // 딜레이: 초기 50ms부터 매 반복마다 50ms씩 증가
        usleep(50000 + i * 50000);
    }
}

int main() {
    int client_socket;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    srand(time(NULL));  // 난수 시드 초기화

    if (init_framebuffer() != 0) {
        fprintf(stderr, "프레임버퍼 초기화 실패\n");
        return 1;
    }
    
    // LCD 전체를 아이보리색 배경으로 채움
    draw_rect(0, 0, vinfo.xres, vinfo.yres, COLOR_IVORY);

    client_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (client_socket < 0) {
        perror("소켓 생성 실패");
        close_framebuffer();
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    // 게임 시작 요청 ("1" 메시지 전송)
    if (sendto(client_socket, "1", strlen("1") + 1, 0,
               (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("sendto 실패");
        close(client_socket);
        close_framebuffer();
        return 1;
    }

    // 서버로부터 결과 수신 (서버는 "s1 s2 c1 c2" 형식의 문자열 전송)
    socklen_t addr_len = sizeof(server_addr);
    int recv_len = recvfrom(client_socket, buffer, BUFFER_SIZE - 1, 0,
                            (struct sockaddr *)&server_addr, &addr_len);
    if (recv_len < 0) {
        perror("recvfrom 실패");
        close(client_socket);
        close_framebuffer();
        return 1;
    }
    buffer[recv_len] = '\0';

    // 문자열 파싱: 서버 주사위 2개, 플레이어 주사위 2개
    int s1, s2, c1, c2;
    if (sscanf(buffer, "%d %d %d %d", &s1, &s2, &c1, &c2) != 4) {
        fprintf(stderr, "서버 메시지 파싱 실패\n");
        close(client_socket);
        close_framebuffer();
        return 1;
    }
    int sum_server = s1 + s2;
    int sum_client = c1 + c2;

    // 주사위 위치 계산 (예시: 중앙에 두 열, 두 행)
    int dice_size = 80;
    int spacing = 20;
    int col_start = (vinfo.xres - (2 * dice_size + spacing)) / 2;
    int server_row_y = 100;
    int player_row_y = server_row_y + dice_size + spacing;

    // 터미널에 라벨과 결과 출력 (LCD 텍스트 출력은 하지 않음)
    
    // opponent(서버) 주사위 애니메이션 (두 개 동시에)
    animate_two_dice(col_start, col_start + dice_size + spacing, server_row_y, dice_size, s1, s2);

    // player(클라이언트) 주사위 애니메이션 (두 개 동시에)
    animate_two_dice(col_start, col_start + dice_size + spacing, player_row_y, dice_size, c1, c2);

    // 최종 결과 출력
    printf("Opponent: %d / PLAYER: %d\n", sum_server, sum_client);
    
    // 승리 결과 비교
    if (sum_server > sum_client)
        printf("Opponent wins!\n");
    else if (sum_server < sum_client)
        printf("Player wins!\n");
    else
        printf("Draw!\n");

    // 잠시 대기 후 종료
    sleep(5);

    if (sendto(client_socket, "GAME ENDS", strlen("GAME ENDS") + 1, 0, \
    (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) 
    {
        perror("sendto 실패");
        close(client_socket);
        close_framebuffer();
        return 1;
    }

    close_framebuffer();
    close(client_socket);

    return 0;
}

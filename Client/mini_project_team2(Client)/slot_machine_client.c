// slot_machine_client.c
// slot_machine_client.c

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include <fcntl.h>
#include <linux/fb.h>
#include <linux/gpio.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>

#include <termios.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define FBDEV "/dev/fb0"

#define COLOR_BLACK 0x000000
#define COLOR_WHITE 0xFFFFFF
#define COLOR_SLOT_BG 0x333333
#define COLOR_TITLE 0xFFD700

#define SLOT_WIDTH 100
#define SLOT_HEIGHT 80
#define SLOT_SPACING 60
#define SLOT_SPEED 40    // 슬롯 속도 증가
#define STOP_DELAY 20    // 슬롯 하나씩 멈추기 위한 딜레이 (프레임 단위)

#define GPIODEV "/dev/gpiochip4"
#define GPIO_KEY1 21

#define SERVER_IP "10.10.141.29"
#define SERVER_PORT 25003
#define MAX_BUF 256

// LCD 관련 전역 변수
struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;
char *map;
char *buffer;
int size;
struct termios original_term;  // 터미널 원래 설정

// GPIO 관련 전역 변수
int fd_gpio;
struct gpiohandle_request handle_request_key1;

// UDP 관련 변수
int sfd;
struct sockaddr_in addr_server, addr_server;
socklen_t addr_server_len = sizeof(addr_server);

// 게임 관련 변수
int client_coin = 100;  // 초기 코인

// --- 함수 프로토타입 ---
void restore_terminal_mode(void);
void signal_handler(int signum);
void set_terminal_mode(void);
void draw_rect(char *buffer, int x, int y, int w, int h, unsigned int color);
void draw_char(char *buffer, int x, int y, char c, int color, int scale);
void Lcd_Printf(char *buffer, int x, int y, int color, int scale, char *fmt, ...);
void Lcd_PrintfExt(char *buffer, int x, int y, int color, char *fmt, ...);
void draw_initial_screen(char *buffer);
void draw_ui(char *buffer);
void draw_to_buffer(char *buffer, int x, int y, int current, int next, int offset);
void spin_slots(int *slots, int *offsets, int *stop_flags, int spinning, int *stop_counter);
void draw_coin_info(char *buffer, int coins);
void draw_game_over_screen(char *buffer);
void draw_reward_info(char *buffer, int reward, int coins);
void update_screen(void);
void init_gpio(void);
int read_key(void);
void close_gpio(void);

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

// =================================================================
// LCD 출력 관련 함수
// =================================================================
void draw_rect(char *buffer, int x, int y, int w, int h, unsigned int color) {
    for (int yy = y; yy < y + h; yy++) {
        for (int xx = x; xx < x + w; xx++) {
            int location = (xx + vinfo.xoffset) * (vinfo.bits_per_pixel / 8) +
                           (yy + vinfo.yoffset) * finfo.line_length;
            if (vinfo.bits_per_pixel == 32)
                *(unsigned int *)(buffer + location) = color;
            else
                *(unsigned short *)(buffer + location) =
                    (unsigned short)(((color >> 16)&0x1F)<<11 | ((color >> 8)&0x3F)<<5 | (color & 0x1F));
        }
    }
}

// 기본 8x8 폰트 (초기 화면용)
// "SLOT MACHINE", "PRESS ANY KEY TO START"에 필요한 일부 문자만 정의 (나머지는 공백)
const unsigned char font8x8_basic[95][8] = {
    {0,0,0,0,0,0,0,0}, // ASCII 32 (space)
    [1 ... 32] = {0,0,0,0,0,0,0,0},
    [33] = {0x18,0x24,0x42,0x7E,0x42,0x42,0x42,0x00}, // 'A' (ASCII 65)
    [35] = {0x3C,0x42,0x40,0x40,0x40,0x42,0x3C,0x00}, // 'C' (ASCII 67)
    [36] = {0x78,0x44,0x42,0x42,0x42,0x44,0x78,0x00}, // 'D' (ASCII 68)
    [37] = {0x7E,0x40,0x40,0x7C,0x40,0x40,0x7E,0x00}, // 'E' (ASCII 69)
    [40] = {0x42,0x42,0x42,0x7E,0x42,0x42,0x42,0x00}, // 'H' (ASCII 72)
    [41] = {0x7E,0x18,0x18,0x18,0x18,0x18,0x7E,0x00}, // 'I' (ASCII 73)
    [43] = {0x42,0x44,0x48,0x70,0x48,0x44,0x42,0x00}, // 'K' (ASCII 75)
    [44] = {0x40,0x40,0x40,0x40,0x40,0x40,0x7E,0x00}, // 'L' (ASCII 76)
    [45] = {0x42,0x66,0x5A,0x5A,0x42,0x42,0x42,0x00}, // 'M' (ASCII 77)
    [46] = {0x42,0x62,0x52,0x4A,0x46,0x42,0x42,0x00}, // 'N' (ASCII 78)
    [47] = {0x3C,0x42,0x42,0x42,0x42,0x42,0x3C,0x00}, // 'O' (ASCII 79)
    [48] = {0x7C,0x42,0x42,0x7C,0x40,0x40,0x40,0x00}, // 'P' (ASCII 80)
    [50] = {0x7C,0x42,0x42,0x7C,0x48,0x44,0x42,0x00}, // 'R' (ASCII 82)
    [51] = {0x3C,0x42,0x40,0x3C,0x02,0x42,0x3C,0x00}, // 'S' (ASCII 83)
    [52] = {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, // 'T' (ASCII 84)
    [55] = {0x42,0x42,0x42,0x5A,0x5A,0x66,0x42,0x00}, // 'W' (ASCII 87)
    [57] = {0x42,0x42,0x24,0x18,0x18,0x18,0x18,0x00}, // 'Y' (ASCII 89)

    [39] = {0x3C,0x42,0x40,0x40,0x4E,0x42,0x3C,0x00}, // 'G' (ASCII 71)
    [54] = {0x42,0x42,0x42,0x42,0x42,0x24,0x18,0x00}, // 'V' (ASCII 86)
    [7] = {0x18,0x18,0x18,0x00,0x00,0x00,0x00,0x00}, //  ''' (ASCII 39)

    [59 ... 94] = {0,0,0,0,0,0,0,0}
};

// 슬롯머신 게임용 숫자 폰트 (0~9)
const unsigned char font8x8_digits[10][8] = {
    {0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00},
    {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00},
    {0x3C,0x66,0x06,0x1C,0x30,0x66,0x7E,0x00},
    {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00},
    {0x0C,0x1C,0x3C,0x6C,0x7E,0x0C,0x0C,0x00},
    {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00},
    {0x3C,0x66,0x60,0x7C,0x66,0x66,0x3C,0x00},
    {0x7E,0x66,0x0C,0x18,0x18,0x18,0x18,0x00},
    {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00},
    {0x3C,0x66,0x66,0x3E,0x06,0x66,0x3C,0x00}
};

// =================================================================
// draw_char: 초기 화면용 문자 출력 (font8x8_basic 사용)
// =================================================================
void draw_char(char *buffer, int x, int y, char c, int color, int scale) {
    if (c < ' ' || c > '~') return;
    const unsigned char *glyph = font8x8_basic[c - ' '];
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (glyph[row] & (1 << (7 - col))) {
                draw_rect(buffer, x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

// =================================================================
// Lcd_Printf: 초기 화면용 문자열 출력 (draw_char 사용)
// =================================================================
void Lcd_Printf(char *buffer, int x, int y, int color, int scale, char *fmt, ...) {
    va_list args;
    char temp[256];
    va_start(args, fmt);
    vsnprintf(temp, sizeof(temp), fmt, args);
    va_end(args);
    for (int i = 0; temp[i] != '\0'; i++) {
        draw_char(buffer, x + i * (scale * 10), y, toupper(temp[i]), color, scale);
    }
}

// =================================================================
// draw_char_ext: 슬롯머신 게임용 문자 출력
// 숫자는 font8x8_digits, 그 외는 draw_char (scale=4) 사용
// =================================================================
void draw_char_ext(char *buffer, int x, int y, char c, int color) {
    if (c >= '0' && c <= '9') {
        const unsigned char *glyph = font8x8_digits[c - '0'];
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                if (glyph[row] & (1 << (7 - col))) {
                    draw_rect(buffer, x + col * 4, y + row * 4, 4, 4, color);
                }
            }
        }
    } else {
        draw_char(buffer, x, y, toupper(c), color, 4);
    }
}

// =================================================================
// Lcd_PrintfExt: 슬롯머신 게임용 문자열 출력 (draw_char_ext 사용)
// =================================================================
void Lcd_PrintfExt(char *buffer, int x, int y, int color, char *fmt, ...) {
    va_list args;
    char temp[256];
    va_start(args, fmt);
    vsnprintf(temp, sizeof(temp), fmt, args);
    va_end(args);
    for (int i = 0; temp[i] != '\0'; i++) {
        draw_char_ext(buffer, x + i * 40, y, temp[i], color);
    }
}

// =================================================================
// draw_initial_screen: 초기 화면 출력 (타이틀 및 프롬프트)
// =================================================================
void draw_initial_screen(char *buffer) {
    draw_rect(buffer, 0, 0, vinfo.xres, vinfo.yres, COLOR_BLACK);
    int center_x = vinfo.xres / 2;
    int title_y = vinfo.yres / 3;
    int prompt_y = (vinfo.yres / 3) * 2;
    Lcd_Printf(buffer, center_x - 240, title_y, COLOR_TITLE, 4, "SLOT MACHINE");
    Lcd_Printf(buffer, center_x - 220, prompt_y, COLOR_WHITE, 2, "PRESS ANY KEY TO START");
}

// =================================================================
// draw_ui: 슬롯머신 UI 출력 (슬롯 영역)
// =================================================================
void draw_ui(char *buffer) {
    draw_rect(buffer, 0, 0, vinfo.xres, vinfo.yres, COLOR_BLACK);
    int center_x = vinfo.xres / 2;
    int center_y = vinfo.yres / 2 + 20;
    int total_width = SLOT_WIDTH * 3 + SLOT_SPACING * 2;
    int start_x = center_x - total_width / 2;
    for (int i = 0; i < 3; i++) {
        draw_rect(buffer, start_x + i * (SLOT_WIDTH + SLOT_SPACING), center_y - 40,
                  SLOT_WIDTH, SLOT_HEIGHT, COLOR_SLOT_BG);
    }
}

// =================================================================
// draw_to_buffer: 슬롯 내부 숫자 출력 (현재 숫자와 다음 숫자)
// x 오프셋을 30으로 조정하여 슬롯 내부 중앙 배치 (SLOT_WIDTH=100)
// =================================================================
void draw_to_buffer(char *buffer, int x, int y, int current, int next, int offset) {
    Lcd_PrintfExt(buffer, x + 30, y + offset - 40, COLOR_WHITE, "%d", current);
    Lcd_PrintfExt(buffer, x + 30, y + offset + 40, COLOR_WHITE, "%d", next);
}

// =================================================================
// spin_slots: 슬롯 회전 및 정지 처리 (정지 시 offset을 64로 고정)
// 슬롯 애니메이션(위에서 아래로 스크롤 효과)을 구현
// =================================================================
void spin_slots(int *slots, int *offsets, int *stop_flags, int spinning, int *stop_counter) {
    if (spinning) {
        for (int i = 0; i < 3; i++) {
            offsets[i] += SLOT_SPEED;
            if (offsets[i] >= SLOT_HEIGHT) {
                offsets[i] -= SLOT_HEIGHT;
                slots[i] = rand() % 10;
            }
        }
    } else {
        if (!stop_flags[0]) {
            (*stop_counter)++;
            if (*stop_counter >= STOP_DELAY) {
                offsets[0] = 80;
                stop_flags[0] = 1;
                *stop_counter = 0;
            }
        } else if (stop_flags[0] && !stop_flags[1]) {
            (*stop_counter)++;
            if (*stop_counter >= STOP_DELAY) {
                offsets[1] = 80;
                stop_flags[1] = 1;
                *stop_counter = 0;
            }
        } else if (stop_flags[0] && stop_flags[1] && !stop_flags[2]) {
            (*stop_counter)++;
            if (*stop_counter >= STOP_DELAY) {
                offsets[2] = 80;
                stop_flags[2] = 1;
                *stop_counter = 0;
            }
        }
    }
}

// =================================================================
// draw_coin_info: 우측 상단에 코인 정보 출력 ("COIN:%d"로 간격 축소)
// =================================================================
void draw_coin_info(char *buffer, int coins) {
    char coin_str[64];
    snprintf(coin_str, sizeof(coin_str), "COIN:%d", coins);
    int str_width = strlen(coin_str) * 40;
    int clear_x = vinfo.xres - 300 - 20;
    if (clear_x < 0) clear_x = 0;
    draw_rect(buffer, clear_x, 10, 300, 50, COLOR_BLACK);
    Lcd_PrintfExt(buffer, vinfo.xres - str_width - 20, 10, COLOR_WHITE, "%s", coin_str);
}

// =================================================================
// draw_game_over_screen: GAME OVER 화면 출력
// =================================================================
void draw_game_over_screen(char *buffer) {
    draw_rect(buffer, 0, 0, vinfo.xres, vinfo.yres, COLOR_BLACK); // 화면을 검정색으로 채움
    int center_x = vinfo.xres / 2;
    int center_y = vinfo.yres / 2;

    // "GAME OVER" 문구 중앙 출력
    Lcd_Printf(buffer, center_x - 180, center_y - 40, COLOR_WHITE, 4, "GAME OVER");

    // "PRESS 'R' TO RESTART" 문구 하단 출력
    Lcd_Printf(buffer, center_x - 200, center_y + 40, COLOR_WHITE, 2, "PRESS 'R' TO RESTART");


    update_screen();
}


// =================================================================
// draw_reward_info: 보상 결과 출력 및, 슬롯 정지 후 코인이 0이면 "RETRY" 출력
// =================================================================
void draw_reward_info(char *buffer, int reward, int coins) {
    char reward_str[64];
    snprintf(reward_str, sizeof(reward_str), "REWARD:%d", reward);
    int str_width = strlen(reward_str) * 40;
    Lcd_PrintfExt(buffer, (vinfo.xres - str_width) / 2, vinfo.yres - 80, COLOR_WHITE, "%s", reward_str);
    if (coins == 0) {
        draw_game_over_screen(buffer); // GAME OVER 화면 출력력
    }
}

// =================================================================
// update_screen: 버퍼 내용을 프레임버퍼에 복사
// =================================================================
void update_screen(void) {
    memcpy(map, buffer, size);
}

// =================================================================
// GPIO 초기화
// =================================================================
void init_gpio(void) {
    fd_gpio = open(GPIODEV, O_RDONLY);
    if(fd_gpio == -1) {
        printf("GPIO open error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    handle_request_key1.lineoffsets[0] = GPIO_KEY1;
    handle_request_key1.flags = GPIOHANDLE_REQUEST_INPUT | GPIOHANDLE_REQUEST_BIAS_DISABLE;
    handle_request_key1.lines = 1;
    if(ioctl(fd_gpio, GPIO_GET_LINEHANDLE_IOCTL, &handle_request_key1) == -1) {
        printf("GPIO ioctl error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

// =================================================================
// read_key: GPIO 버튼 읽기 (디바운스 포함)
// =================================================================
int read_key(void) {
    static int prev_state = 1;
    struct gpiohandle_data hdata;
    if(ioctl(handle_request_key1.fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &hdata) == -1) {
        printf("GPIO read error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if(prev_state == 1 && hdata.values[0] == 0) {
        prev_state = 0;
        usleep(50000);
        return 1;
    } else if(hdata.values[0] == 1) {
        prev_state = 1;
    }
    return 0;
}
 
// =================================================================
// close_gpio: GPIO 종료
// =================================================================
void close_gpio(void) {
    close(fd_gpio);
}
 
// =================================================================
// main (서버)
// =================================================================
int main() 
{
    // GPIO 초기화
    init_gpio();
    atexit(close_gpio);
    
    int sfd, len, ret, temp = 0;
    struct sockaddr_in addr_server;
    socklen_t addr_server_len = sizeof(addr_server);

    char buf[MAX_BUF];
    fd_set read_fds, write_fds, fds, fds_temp;
    
    FD_ZERO(&fds);
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);

    FD_SET(STDIN_FILENO  , &read_fds);
    FD_SET(STDOUT_FILENO , &write_fds);

    // 응답 대기 시간간
    struct timeval tv         = {0, 500000};
    struct timeval tv_read    = {0, 50000 };
    struct timeval tv_write   = {0, 50000 };
    struct timeval tv_restart = {0, 500000}; 
    struct timeval tv_udp     = {0, 50000 };

    int key_pressed_home = 0; // 초기화면에서 GPIO 입력 감지
    int blink = 1; // 깜빡이기 상태 (1 = 표시, 0 = 숨김)
    int stop_signal = 0;
    
    printf("Slot Machine Client is running...\n");
    
    sfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sfd == -1) 
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    memset(&addr_server, 0, sizeof(addr_server));
    addr_server.sin_family = AF_INET;
    addr_server.sin_addr.s_addr = inet_addr(SERVER_IP);
    addr_server.sin_port = htons(SERVER_PORT);
    FD_SET(sfd, &fds);
    
    // 초기 화면에서 클라이언트는 아무 키나 입력하면 서버로 시작 신호를 보냄
    printf("Press any key to start the game: ");
    getchar(); // 입력된 키를 읽고 바로 진행

    strcpy(buf, "start"); // 어떤 키를 입력해도 "start" 메시지 전송
    sendto(sfd, buf, strlen(buf), 0, (struct sockaddr *)&addr_server, sizeof(addr_server));
    
    // 프레임버퍼 초기화
    int fd_fb = open(FBDEV, O_RDWR);
    if(fd_fb == -1) 
    {
        printf("Framebuffer open error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if(ioctl(fd_fb, FBIOGET_VSCREENINFO, &vinfo) == -1) 
    {
        printf("FB VSCREENINFO error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if(ioctl(fd_fb, FBIOGET_FSCREENINFO, &finfo) == -1) 
    {
        printf("FB FSCREENINFO error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    size = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
    map = (char *)mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_fb, 0);
    buffer = malloc(size);
    if(map == MAP_FAILED || buffer == NULL) 
    {
        printf("Framebuffer mmap/malloc error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    set_terminal_mode();
        
    printf("waiting for server...\n");

    // LCD에 초기 화면 출력
    draw_initial_screen(buffer);
    update_screen();

    // 클라이언트의 초기 입력대기: 초기 화면에서 클라이언트가 아무 메시지나 GPIO 입력을 보내면 게임 시작
    for(;;)
    {
        // "PRESS ANY KEY TO START" 문구만 깜빡이기
        if (blink) 
        {
            Lcd_Printf(buffer, vinfo.xres / 2 - 220, (vinfo.yres / 3) * 2, COLOR_WHITE, 2, "PRESS ANY KEY TO START");
        } 
        else 
        {
            draw_rect(buffer, vinfo.xres / 2 - 220, (vinfo.yres / 3) * 2, 450, 20, COLOR_BLACK); // 문구 숨김
        }
        update_screen();
        usleep(300000);
        blink = !blink; // 깜빡이기 상태 반전

        // 키보드 또는 GPIO 버튼 입력 감지
        
        key_pressed_home = read_key();
        // -- for test --
        //key_pressed_home = 0;
        if (key_pressed_home) { break; }
    }

    printf("Game Start!\n");
    printf("Current Money : %d\n", client_coin);
    // --- 게임 루프 ---
    for(;;)
    {
        // Game Over
        if(client_coin <= 0) 
        {
            draw_game_over_screen(buffer); // GAME OVER 화면 출력

            fd_set restart_fds;
            FD_ZERO(&restart_fds);
            FD_SET(sfd, &restart_fds);
            int key_pressed_restart = 0;
            key_pressed_restart = read_key();
            ret = select(sfd + 1, &restart_fds, NULL, NULL, &tv_restart);
            if((ret > 0 && FD_ISSET(sfd, &restart_fds)) || key_pressed_restart) 
            {
                char ch;
                ch = getchar();

                if(tolower(ch) == 'r' || key_pressed_restart) 
                { 
                    sendto(sfd, &ch, 1, 0, (struct sockaddr *)&addr_server, sizeof(addr_server)); 
                    
                    len = recvfrom(sfd, buf, sizeof(buf), 0, (struct sockaddr *)&addr_server, &addr_server_len);
                    if (len > 0) 
                    {
                        printf("Restarting game...\n");
                        client_coin = atoi(buf); // 코인 초기화

                        // 게임 재시작 - 게임 화면으로 복귀
                        draw_ui(buffer);
                        draw_coin_info(buffer, client_coin);
                        update_screen();
                        continue; 
                    }
                    else 
                    {
                        printf("Error %d...\n", __LINE__);
                        return -1;
                    }
                }
            }
        }
        
        // 한 라운드 시작: 슬롯 회전 애니메이션
        int slots[3] = {0, 0, 0};
        int offsets[3] = {0, 0, 0};
        int stop_flags[3] = {0, 0, 0};
        int stop_counter = 0;
        int spinning = 1;
        
        printf("input s key or button : ");
        // UDP를 통한 "s" 신호와 GPIO 입력(OR 조건) 대기
        stop_signal = 0;
        while(!stop_signal) 
        {
            spin_slots(slots, offsets, stop_flags, spinning, &stop_counter);
            
            int center_x = vinfo.xres / 2;
            int total_width = SLOT_WIDTH * 3 + SLOT_SPACING * 2;
            int start_x = center_x - total_width / 2;
            
            draw_ui(buffer);
            draw_to_buffer(buffer, start_x, \
                vinfo.yres / 2 - 40, slots[0], (slots[0] + 1) % 10, offsets[0]);
            draw_to_buffer(buffer, start_x +      SLOT_WIDTH + SLOT_SPACING,  \
                vinfo.yres / 2 - 40, slots[1], (slots[1] + 1) % 10, offsets[1]);
            draw_to_buffer(buffer, start_x + 2 * (SLOT_WIDTH + SLOT_SPACING), \
                vinfo.yres / 2 - 40, slots[2], (slots[2] + 1) % 10, offsets[2]);
            draw_coin_info(buffer, client_coin);
            update_screen();
            usleep(50000);
            
            int key_pressed = read_key();
            fds_temp = read_fds;

            ret = select(STDIN_FILENO + 1, &fds_temp, NULL, NULL, &tv_read);
            // printf("%d\n", ret);
            if (ret > 0 || key_pressed > 0)
            {   
                if(key_pressed == 1)
                {
                    len = recvfrom(sfd, buf, sizeof(buf), 0, (struct sockaddr *)&addr_server, &addr_server_len);
                    if (len > 0) 
                    {
                        printf("Stop Slot...\n");
                        client_coin = atoi(buf); // 코인 사용
                        stop_signal = 1; // 's'가 입력되었을 때만 슬롯 정지
                        break;
                    }
                }
                
                buf[0] = fgetc(stdin);
                buf[1] = '\0';
                printf("%s\n", buf);
                if (strncmp(buf, "s", 1) == 0)
                {   
                    sendto(sfd, buf, sizeof(strlen(buf)), 0, (struct sockaddr *)&addr_server, addr_server_len);
                    len = recvfrom(sfd, buf, sizeof(buf), 0, (struct sockaddr *)&addr_server, &addr_server_len);
                    if (len > 0) 
                    {
                        printf("stop Slot...\n");
                        client_coin = atoi(buf); // 코인 사용
                        stop_signal = 1; // 's'가 입력되었을 때만 슬롯 정지
                        break;
                    }
                }
                else if (strncmp(buf, "q", 1) == 0)
                {   
                    sendto(sfd, buf, sizeof(strlen(buf)), 0, (struct sockaddr *)&addr_server, addr_server_len);
                    len = recvfrom(sfd, buf, sizeof(buf), 0, (struct sockaddr *)&addr_server, &addr_server_len);
                    if (len > 0) 
                    {
                        printf("Run!!...\n");
                        
                        munmap(map, size);
                        close(fd_gpio);
                        close(sfd);
                        printf("terminated.\n");
                        return EXIT_SUCCESS;
                    }
                }
                else
                {
                    char msg[] = "Game is running. Press 's' to stop.";
                    printf("%s\n", msg);
                    continue;
                }
            }
        }
        
        // 슬롯 정지 애니메이션: spinning = 0로 전환하여 각 슬롯의 offset을 64로 고정
        spinning = 0;
        int final_stop_counter = 0;
        while(stop_flags[0] == 0 || stop_flags[1] == 0 || stop_flags[2] == 0) 
        {
            spin_slots(slots, offsets, stop_flags, spinning, &final_stop_counter);
            int center_x    = vinfo.xres / 2;
            int total_width = SLOT_WIDTH * 3 + SLOT_SPACING * 2;
            int start_x     = center_x - total_width / 2;

            draw_ui(buffer);
            draw_to_buffer(buffer, start_x, \
                vinfo.yres / 2 - 40, slots[0], (slots[0] + 1) % 10, offsets[0]);
            draw_to_buffer(buffer, start_x +  SLOT_WIDTH + SLOT_SPACING,  \
                vinfo.yres / 2 - 40, slots[1], (slots[1] + 1) % 10, offsets[1]);
            draw_to_buffer(buffer, start_x + (SLOT_WIDTH+SLOT_SPACING) * 2, \ 
                vinfo.yres / 2 - 40, slots[2], (slots[2] + 1) % 10, offsets[2]);
            draw_coin_info(buffer, client_coin);
            update_screen();

            usleep(50000);
        }
        
        // 보상 계산
        sprintf(buf, "%d%d%d", slots[0], slots[1], slots[2]);
        printf("%s\n", buf);
        sendto(sfd, buf, MAX_BUF, 0, (struct sockaddr *)&addr_server, addr_server_len);
        
        len = recvfrom(sfd, buf, MAX_BUF, 0, (struct sockaddr *)&addr_server, &addr_server_len);
        if (len > 0) 
        {
            temp = atoi(buf);
            printf("Result : %d\n", temp);
            client_coin += temp; // 코인 사용
        }
        
        // LCD 하단에 보상 결과 출력 
        draw_reward_info(buffer, temp, client_coin);
        update_screen();
        usleep(1000000);  // 결과 1초 표시
    }
    
    munmap(map, size);
    close(fd_gpio);
    close(sfd);
    printf("terminated.\n");
    return EXIT_SUCCESS;
}

#if 0
#endif
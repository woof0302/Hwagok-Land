#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <linux/gpio.h>


#define SERVER_IP       "128.0.0.1"
#define SERVER_PORT      25002

#define ID_UNKNOWN	     0
#define ID_LED		     1
#define ID_KEY		     2

#define CMD_UNKNOWN	     0

#define CMD_LED_ON	     1
#define CMD_LED_OFF	     2

#define CMD_KEY_STATUS	 1

#define RES_OK		     0
#define RES_ERROR	    -1
#define RES_UNKNOWN_ID	-2
#define RES_UNKNOWN_CMD	-3

#define DATE_SIZE 64
#define MSG_SIZE 64
typedef struct {
	char date[DATE_SIZE];
	char msg[MSG_SIZE];
} info_t;

typedef struct 
{

    // int command_idx;
    // unsigned int mode;
    // int cost;
} game_table;

// 추후 DB까지 운용한다면 사용
typedef struct define
{
    /* data */
    int umount;
    int cost;
} user_amount;

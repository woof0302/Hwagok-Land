#include "define.h"

#define DICE_GAME_NAME 			"./Dice_server"
#define HORSE_RACE_GAME_NAME 	"./horse_racing_server"
#define	SLOT_MACHINE_GAME_NAME	"./slot_machine_server"

#define MAX_BUF 	256

#define MAX_RBUF 	256
#define MAX_WBUF 	256
#define MAX_TBUF 	(256-16-64)
#define MAX_FBUF 	64
#define GAME_COUNT	3

#define BUF_SIZE 	256

typedef void (*sighandler_t)(int);
pid_t pid;

#define     MAX_GAMES       10
#define     MAX_ROUND       3

#define     SLOT_MACHINE    0
#define     DICE_GAME       1
#define     UNNAMED_GAME    2

#define     MODE_CAGINO     0
#define     MODE_GM         1

int new_server(unsigned int inaddr, unsigned short port)
{
	int server;
	struct sockaddr_in addr_server;
	int ret;
	int optval = 1;

	server = socket(AF_INET, SOCK_DGRAM, 0);
	if(server == -1) 
	{
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		exit(EXIT_FAILURE);
	}

	memset(&addr_server, 0, sizeof(addr_server));
	addr_server.sin_family = AF_INET;
	addr_server.sin_addr.s_addr = htonl(inaddr);
	addr_server.sin_port = htons(port);
	ret = bind(server, (struct sockaddr *)&addr_server, sizeof(addr_server));
	if(ret == -1) 
	{
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return -1;
	}
	
	return server;
}

int send_file(int peer, FILE *f, struct sockaddr_in addr_server)
{
	/* Implement code */
	char filebuf[BUF_SIZE + 1];
	int eof_flag = 0;

	for(;;) 
	{
		size_t uret = fread(filebuf, 1, BUF_SIZE, f);
		if(uret < (size_t)BUF_SIZE) 
		{
			if(feof(f) != 0) { eof_flag = 1; }
			else
			{
				printf("[%d] error : %s (%d)\n", pid, "fread failed", __LINE__);
				return -1;
			}
		}
		
		sendto(peer, filebuf, uret, 0, (struct sockaddr *)&addr_server, sizeof(addr_server));
		if(eof_flag) { break; }
	}	

	return 0;
}

int send_path(int peer, char *file, struct sockaddr_in addr_server)
{
	/* Implement code */
	int ret; 
	FILE *f = fopen(file, "r");
	if(f == NULL) 
	{
		printf("[%d] error : %s (%d)\n", pid, strerror(errno), __LINE__);
		return -1;
	}
	
	ret = send_file(peer, f, addr_server);
	if(ret == -1)
	{
		printf("[%d] error : %s (%d)\n", pid, strerror(errno), __LINE__);
		return -1;
	}
		
	fclose(f);

	return 0;
}

int get_pwd(char *buf)
{
	FILE *fp_r;
	int ret;
	size_t ulen;

	fp_r = popen("pwd", "r");
	if(fp_r == NULL) 
	{
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return -1;
	}

	ulen = fread(buf, 1, MAX_RBUF - 1, fp_r);
	if(ulen == 0) 
	{
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return -1;
	}
	buf[ulen - 1] = '\0';

	printf("[debug] : %s\n", buf);

	ret = pclose(fp_r);
	if(ret == -1) 
	{
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return -1;
	}

	return 0;
}

int game_1()
{
    printf("game1 started\n");
	execlp(DICE_GAME_NAME, DICE_GAME_NAME, NULL);

    return 0;
}

int game_2()
{
    printf("game2 started\n");
	execlp(HORSE_RACE_GAME_NAME, HORSE_RACE_GAME_NAME, NULL);

    return 0;
}

int game_3()
{
    printf("game3 started\n");
	execlp(SLOT_MACHINE_GAME_NAME, SLOT_MACHINE_GAME_NAME, NULL);

    return 0;
}

void process_command(int sfd, struct sockaddr_in addr_client)
{
	char rbuf[MAX_RBUF];
	char wbuf[MAX_WBUF];
	char tbuf[MAX_TBUF];
	char fbuf[MAX_FBUF];
	socklen_t addr_client_len = sizeof(addr_client);
	int len;
	int exit_flag = 0;

	static int (*game_executer[GAME_COUNT])() \
	= { game_1, game_2, game_3 };

	for(;;) 
	{
		len = recvfrom(sfd, rbuf, MAX_BUF - 1, 0, (struct sockaddr *)&addr_client, &addr_client_len);
		//len = read(sfd, rbuf, MAX_RBUF);
		if(len <= 0) return;
		rbuf[len] = 0;
		printf("[%d] From : %s\n", pid, inet_ntoa(addr_client.sin_addr));
		printf("[%d] received: %s", pid, rbuf);

		if(strcmp(rbuf, "QUIT\r\n") == 0) 
		{
			sprintf(wbuf, "OK\r\n");
			sendto(sfd, wbuf, strlen(wbuf), 0, (struct sockaddr *)&addr_client, sizeof(addr_client));
			//write(sfd, wbuf, strlen(wbuf));
			exit_flag = 1;
			return;
		}
		else if(strcmp(rbuf, "PWD\r\n") == 0) 
		{
			if(get_pwd(tbuf) == -1) { sprintf(wbuf, "ERR\r\n"); 		}
			else 					{ sprintf(wbuf, "OK %s\r\n", tbuf); }

			sendto(sfd, wbuf, strlen(wbuf), 0, (struct sockaddr *)&addr_client, sizeof(addr_client));
			//write(sfd, wbuf, strlen(wbuf));
		}
		else if(strncmp(rbuf, "GAME ", sizeof("GAME")) == 0) 
		{
			int ret; 
			pid_t pid_temp;

			pid_temp = fork();
			if(pid_temp == -1) { return EXIT_FAILURE; }
			if(pid_temp == 0)
			{
				strcpy(wbuf, strtok(&rbuf[sizeof("GAME")], "\r\n"));
				game_executer[atoi(wbuf) - 1]();

				return EXIT_SUCCESS;
			}
			else
			{
				pid_t pid_wait;
				int status;

				printf("[%d] Wait Exit Game\n", pid);
				pid_wait = wait(&status);
				printf("[%d] End Game\n", pid);
			}

			len = recvfrom(sfd, rbuf, MAX_BUF-1, 0, (struct sockaddr *)&addr_client, &addr_client_len);
			if(len <= 0) return;
			rbuf[len] = 0;
			printf("[%d] received: %s", pid, rbuf);
			if(strcmp(rbuf, "ERR\r\n") == 0) continue;
		}

		if(exit_flag) return;
	}
}

int main(int argc, char **argv)
{
	int ret;
	int len;
	int sfd_server, sfd_client = -1;
	int en = 0;
	
	char buf[MAX_BUF];
	char tbuf[MAX_BUF];
	struct sockaddr_in addr_server;
	struct sockaddr_in addr_client;
	pid_t pid_temp;
	socklen_t addr_client_len;
	sighandler_t sig_ret;
	
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(sfd_client, &fds);
    struct timeval tv_udp = {0, 50000 };

	int optval = 1;

	if(argc != 1) 
	{
		printf("usage: %s\n", argv[0]);
		return EXIT_FAILURE;
	}
	printf("[%d] running %s\n", pid = getpid(), argv[0]);

	/* to prevent child process from becoming zombie */
	/*
	sig_ret = signal(SIGCHLD, SIG_IGN);
	if(sig_ret == SIG_ERR) 
	{
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return EXIT_FAILURE;
	}
	*/
	
	sfd_server = socket(AF_INET, SOCK_DGRAM, 0);
	if(sfd_server == -1) 
	{
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return EXIT_FAILURE;
	}

	/* to prevent "Address already in use" error */
	ret = setsockopt(sfd_server, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if(ret == -1) 
	{
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return EXIT_FAILURE;
	}

	memset(&addr_server, 0, sizeof(addr_server));
	addr_server.sin_family = AF_INET;
	addr_server.sin_addr.s_addr = htonl(INADDR_ANY);
	addr_server.sin_port = htons(SERVER_PORT);
	ret = bind(sfd_server, (struct sockaddr *)&addr_server, sizeof(addr_server));
	if(ret == -1) 
	{
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return EXIT_FAILURE;
	}

	addr_client_len = sizeof(addr_client);
	len = recvfrom(sfd_server, buf, MAX_BUF-1, 0, (struct sockaddr *)&addr_client, &addr_client_len);
	
	for(;;) 
	{ 
#if 1
		process_command(sfd_server, addr_client);
#endif

#if 0
		en = select(sfd_server + 1, &fds, NULL, NULL, &tv_udp);
		if((en > 0)) { printf("wait!!\n"); continue; }
  		addr_client_len = sizeof(addr_client);
		ret = recvfrom(sfd_client, buf, MAX_BUF-1, 0, (struct sockaddr *)&addr_client, &addr_client_len);
		if(ret == -1)
		{
			printf("wait\n");
		}

		pid_temp = fork();

		printf("[%d] connected, %s\n", pid, inet_ntoa(addr_client.sin_addr));

		if(pid_temp == -1) 
		{
			printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
			return EXIT_FAILURE;
		}
		else if(pid_temp == 0)
		{
			pid = getpid();
			
			close(sfd_server);

			/* to prevent pclose() from returning -1 */
			sig_ret = signal(SIGCHLD, SIG_DFL);
			if(sig_ret == SIG_ERR) 
			{
				printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
				return EXIT_FAILURE;
			}

			process_command(sfd_server, addr_client);
		}
		else
		{
			close(sfd_client);
		}

		usleep(1000000);
#endif

	}

	close(sfd_server);
	printf("[%d] terminated\n", pid);

	return EXIT_SUCCESS;

}


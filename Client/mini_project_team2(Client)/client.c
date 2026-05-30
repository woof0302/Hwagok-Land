#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "define.h"

#define DICE_GAME_NAME 			"./Dice_client"
#define HORSE_RACE_GAME_NAME 	"./horse_racing_client"
#define	SLOT_MACHINE_GAME_NAME	"./slot_machine_client"

#define MAX_RBUF 	256
#define MAX_WBUF 	256
#define MAX_TBUF 	(256-16-64)
#define MAX_FBUF 	64
#define GAME_COUNT	3

#define BUF_SIZE 	256

#define MAX_BUF 	256

pid_t pid;

int game_1()
{
    printf("game1 start\n");
	execlp(DICE_GAME_NAME, DICE_GAME_NAME, NULL);

    return 0;
}

int game_2()
{
    printf("game2 start\n");
	execlp(HORSE_RACE_GAME_NAME, HORSE_RACE_GAME_NAME, NULL);

    return 0;
}

int game_3()
{
    printf("game3 start\n");
	execlp(SLOT_MACHINE_GAME_NAME, DICE_GAME_NAME, NULL);

    return 0;
}

int new_client(uint32_t srv_addr, unsigned short port)
{
	int client;
	struct sockaddr_in addr_client;
	socklen_t addr_server_len;
	int ret;

	client = socket(AF_INET, SOCK_DGRAM, 0);
	if(client == -1) 
	{
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		exit(EXIT_FAILURE);
	}

	memset(&addr_client, 0, sizeof(addr_client));
	addr_client.sin_family = AF_INET;
	addr_client.sin_addr.s_addr = htonl(srv_addr);
	addr_client.sin_port = htons(port);
	ret = connect(client, (struct sockaddr *)&addr_client, sizeof(addr_client));
	if(ret == -1) 
	{
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return -1;
	}	

	printf("ip : %s\n", inet_ntoa(addr_client.sin_addr));
	printf("port : %hu\n", htons(addr_client.sin_port));

	return client;
}

int recv_file(int peer, FILE *f, struct sockaddr_in addr_server)
{
	char filebuf[BUF_SIZE + 1];
	int len = 0;
	size_t uret;

	for(;;) 
	{
		// len = read(peer, filebuf, BUF_SIZE);
		socklen_t data_sub_server_len = sizeof(struct sockaddr);
		len = recvfrom(peer, filebuf, MAX_BUF - 1, 0, (struct sockaddr *)&addr_server, &data_sub_server_len);
		if(len == -1) 
		{
			printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
			return -1;
		}
		else if(len == 0) 
		{
			break;
		}

		// printf("test !!!! %d\n", __LINE__);

		uret = fwrite(filebuf, 1, len, f);
		if(uret < len) 
		{
			printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
			return -1;
		}		
		// printf("test : %s\n", filebuf);
	}
	// sendto(peer, "Finished", strlen("Finished"), 0, (struct sockaddr *)&addr_server, sizeof(addr_server));

	return 0;
}

int recv_path(int peer, char *file, struct sockaddr_in addr_server)
{
	int ret;
	FILE *f = fopen(file, "w");
	if(f == NULL) {
		printf("error: %s (%d)\n", strerror(errno), __LINE__);
		return -1;
	}

	ret = recv_file(peer, f, addr_server);

	fclose(f);

	return ret;
}

void handle_command(int sfd, struct sockaddr_in addr_server)
{
	char rbuf[MAX_RBUF];
	char wbuf[MAX_WBUF];
	char tbuf[MAX_TBUF];
	char fbuf[MAX_FBUF];
	struct sockaddr_in data_sub_server;
	socklen_t data_sub_server_len;
	socklen_t addr_server_len;
	int len;
	int exit_flag = 0;
	unsigned short port_num;

	addr_server_len = sizeof(addr_server);

	static int (*game_executer[GAME_COUNT])() \
	= { game_1, game_2, game_3 };

	for(;;) 
	{
		printf("[%d] command> ", pid);

		if(fgets(rbuf, MAX_RBUF, stdin)) 
		{	
			rbuf[strlen(rbuf) - 1] = '\0';

			if(strcmp(rbuf, "quit") == 0) 
			{
				sprintf(wbuf, "QUIT\r\n");
				sendto(sfd, wbuf, strlen(wbuf), 0, (struct sockaddr *)&addr_server, sizeof(addr_server));
				// write(sfd, wbuf, strlen(wbuf));
				len = recvfrom(sfd, rbuf, MAX_BUF-1, 0, (struct sockaddr *)&addr_server, &addr_server_len);
				// len = read(sfd, rbuf, MAX_RBUF - 1);
				if(len <= 0) return;
				rbuf[len] = 0;
				printf("[%d] received: %s", pid, rbuf);
				exit_flag = 1;
			}
			else if(strcmp(rbuf, "pwd") == 0) 
			{
				sprintf(wbuf, "PWD\r\n");
				sendto(sfd, wbuf, strlen(wbuf), 0, (struct sockaddr *)&addr_server, sizeof(addr_server));
				// write(sfd, wbuf, strlen(wbuf));
				len = recvfrom(sfd, rbuf, MAX_BUF-1, 0, (struct sockaddr *)&addr_server, &addr_server_len);
				// len = read(sfd, rbuf, MAX_RBUF-1);
				if(len <= 0) return;
				rbuf[len] = 0;
				printf("[%d] received: %s", pid, rbuf);
			}
			else if(strncmp(rbuf, "game ", sizeof("game")) == 0) 
			{
				pid_t pid_temp;

				printf("GAME START\r\n");

				sscanf(rbuf, "%s %s", tbuf, fbuf);
				sprintf(wbuf, "GAME %s\r\n", fbuf);
				sendto(sfd, wbuf, strlen(wbuf), 0, (struct sockaddr *)&addr_server, sizeof(addr_server));
				
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
				
				sprintf(wbuf, "OK\r\n");
				sendto(sfd, wbuf, strlen(wbuf), 0, (struct sockaddr *)&addr_server, sizeof(addr_server));
			}
		}
		if(exit_flag) return;
	}
}

int main(int argc, char **argv)
{
	int ret, sig_ret;
	int sfd_client;
	struct sockaddr_in addr_client;

	if(argc != 1) 
	{
		printf("usage:\n");
		return EXIT_FAILURE;
	}
	printf("[%d] running %s \n", pid = getpid(), argv[0]);

	/* to prevent child process from becoming zombie */
	/*
	sig_ret = signal(SIGCHLD, SIG_IGN);
	if(sig_ret == SIG_ERR) 
	{
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return EXIT_FAILURE;
	}
	*/

	sfd_client = socket(AF_INET, SOCK_DGRAM, 0);
	if(sfd_client == -1) 
	{
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return EXIT_FAILURE;
	}
	memset(&addr_client, 0, sizeof(addr_client));
	addr_client.sin_family = AF_INET;
	addr_client.sin_addr.s_addr = inet_addr(SERVER_IP);
	addr_client.sin_port = htons(SERVER_PORT);
	ret = connect(sfd_client, (struct sockaddr *)&addr_client, sizeof(addr_client));
	if(ret == -1) 
	{
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return EXIT_FAILURE;
	}

	sendto(sfd_client, "Connection\n", sizeof("Connection\n"), 0, (struct sockaddr *)&addr_client, sizeof(addr_client));
	sendto(sfd_client, "Connection\n", sizeof("Connection\n"), 0, (struct sockaddr *)&addr_client, sizeof(addr_client));

	handle_command(sfd_client, addr_client);

	close(sfd_client);
	printf("[%d] closed\n", pid);
	printf("[%d] terminated\n", pid);

	return EXIT_SUCCESS;
}


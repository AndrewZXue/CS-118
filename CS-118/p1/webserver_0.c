#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <termios.h>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <signal.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>

#define MSG_200 "HTTP/1.1 200 OK\r\n"
#define MSG_404 "HTTP/1.1 404 Not Found\r\n\r\n"
#define ERROR_404 "<h1>Error 404: File Not Found!</h1> <br><br> <h3>File requested must be in same directory as server.</h3>"

int s_fg = 0;

void child_handler(int s) 
{
  	while(waitpid(-1, NULL, WNOHANG) > 0);
}

void send_404(int sock_fd)
{
	send(sock_fd, MSG_404, strlen(MSG_404), 0);
	send(sock_fd, ERROR_404, strlen(ERROR_404), 0);
	printf("Error: 404!\n");
	return;
}

char* get_timelog(){
	struct tm* timelog;
	time_t now;
	time(&now);
	timelog = gmtime(&now);
	char now_timelog[35];
	strftime(now_timelog, 35, "%a, %d %b %Y %T %Z", timelog);

	char tlog[50] = "Date: ";
	strcat(tlog, now_timelog);
	strcat(tlog, "\r\n");
	return tlog;
}

char* get_lasttimelog(char *f, struct stat status){
	struct tm* last_timelog;

	last_timelog = gmtime(&(status.st_mtime));
	char lasttime[35];
	strftime(lasttime, 35, "%a, %d %b %Y %T %Z", last_timelog);

	char last_log[50] = "Last-Modified: ";
	strcat(last_log, lasttime);
	strcat(last_log, "\r\n");
	return last_log;
}


void response(int sock_fd, char *f, size_t size)
{
	char message[512];
	char *connection = "Connection: close\r\n";
	char* msg;
	msg = MSG_200;

	char* now_log = get_timelog();


	char *server = "Server: ZXLY/1.0\r\n";

	struct stat status;
	stat(f, &status);
	char* last_log = get_lasttimelog(f, status);

	char filesize[50] = "Content-Length: ";	
	char length[10];
	sprintf (length, "%d", (unsigned int)filesize);

	strcat(filesize, length);
	strcat(filesize, "\r\n");

	int indicator = 0;
	if (strstr(f, ".html") != NULL) 
		indicator = 0;
	else if (strstr(f, ".txt") != NULL) 
		indicator = 1;
	else if (strstr(f, ".jpeg") != NULL) 
		indicator = 2;
	else if (strstr(f, ".jpg") != NULL) 
		indicator = 3;
	else if (strstr(f, ".gif") != NULL) 
		indicator = 4;

	//by default .txt file
	char *file_type;
	file_type = "Content-Type: text/plain\r\n";
	switch (indicator){
		case 0:
			file_type = "Content-Type: text/html\r\n";
		case 1:
			file_type = "Content-Type: text/plain\r\n";
		case 2:
			file_type = "Content-Type: image/jpeg\r\n";
		case 3:
			file_type = "Content-Type: image/jpg\r\n";
		case 4:
			file_type = "Content-Type: image/gif\r\n";
		default:
			file_type = "Content-Type: text/plain\r\n";
	}

	int offset = strlen(msg);
	memcpy(message, msg, offset);

	memcpy(message+offset, connection, strlen(connection));
	offset+=strlen(connection);

	memcpy(message+offset, now_log, strlen(now_log));
	offset+=strlen(now_log);

	memcpy(message+offset, server, strlen(server));
	offset+=strlen(server);

	memcpy(message+offset, last_log, strlen(last_log));
	offset+=strlen(last_log);

	memcpy(message+offset, length, strlen(length));
	offset+=strlen(length);

	memcpy(message+offset, file_type, strlen(file_type));
	offset+=strlen(file_type);

	memcpy(message+offset, "\r\n\0", 3);

	
	send(sock_fd, message, strlen(message), 0);

	printf("HTTP RESPONSE MESSAGE:\n%s\n", message);

	return;

}

void deliver(int sock_fd){

	int n;
	char buffer[512];
	char file_name[512];
	bzero(buffer,512);
	n = read(sock_fd,buffer,511); // read socket request message into buffer

	if (n < 0) 
		printf("Error: !\n");

	memcpy(file_name, buffer, 512);

	char *tk;
	const char space[2] = " ";
	tk = strtok(file_name, space);
	tk = strtok(NULL, space);
	tk += 1;

	if (strlen(tk) <= 0) 
		tk = "\0";
	printf("HTTP REQUEST MESSAGE:\n%s\n", buffer);

	// ----------------------

	char* buff = NULL;

	if(tk == "\0")
	{
		send_404(sock_fd);
		return;
	}

	FILE* file = fopen(tk, "r");
		if (file == NULL)
	{
		send_404(sock_fd);
		return;
	}
	
	else if (fseek(file, 0L, SEEK_END) == 0)
	{
		if (ftell(file) < 0)
		{
			send_404(sock_fd);
			return;
		}

		if (fseek(file, 0l, SEEK_SET) != 0)
		{
			send_404(sock_fd);
			return;
		}

		long size = ftell(file);
		buff = malloc(sizeof(char) * (size + 1));
		size_t buff_size = fread(buff, sizeof(char), size, file);

		if (buff_size == 0)
		{
			send_404(sock_fd);
			return;
		}

		buff[buff_size] = '\0';

		response(sock_fd, tk, buff_size);

		send(sock_fd, buff, buff_size, 0);

		printf(" \"%s\" delivered!\n", tk);
	}

	fclose(file);
	free(buff);
	
}





int main(int argc, char* argv[]){

	int port;
	//char s_buffer[256];
	//int in_fd = 0;
	//int out_fd = 1;
	if(argc < 2)
	{
		fprintf(stderr, "%s\n", "Error: NO port number in argumet.");
		exit(1);
	}

	static struct option long_ops[] = {
		{"port", required_argument, NULL, 'p'},
		{0, 0, 0, 0}
	};

	int ret = 0;
	while ((ret = getopt_long(argc, argv, "p", long_ops, NULL)) != -1)
	{
		switch(ret)
		{
			case 'p':
				port = atoi(optarg);
				break;
			default:
				fprintf(stderr, "Unrecoganized Argument.\n");
				exit(1);
				break;
		}
	}

	int sock_fd;
	int new_sock_fd;
	struct sockaddr_in serv_addr, cli_addr;

	sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (sock_fd < 0)
	{
		printf("Cannot create socket.\n");
		exit(1);
	}
	memset((void *) &serv_addr, 0, sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port);

	if(bind(sock_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
	{
		perror("Error: Binding Error.\n");
		exit(1);
	}

	listen(sock_fd, 5);

	

	struct sigaction sig;
	sig.sa_handler = child_handler;
	sigemptyset(&sig.sa_mask);
    sig.sa_flags = SA_RESTART;
    if(sigaction(SIGCHLD, &sig, NULL) == -1) 
    {
        printf("%s\n", "Error: Child Process singal action failed.");
        exit(1);
    }

	while(1)
	{
		socklen_t client_len = sizeof(cli_addr);
		new_sock_fd = accept(sock_fd, (struct sockaddr *) &cli_addr, &client_len);
		if(new_sock_fd < 0)
		{
			perror("Error: Socket Connection Failed.\n");
			continue;
		}
		if(!fork())
		{
			close(sock_fd);
			deliver(new_sock_fd);
			exit(0);
		}
		close(new_sock_fd);
	}
	return 0;
}



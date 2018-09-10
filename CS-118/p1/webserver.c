
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <termios.h>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <signal.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>

#define MSG_200 "HTTP/1.1 200 OK\r\n"
#define MSG_404 "HTTP/1.1 404 Not Found\r\n\r\n"
#define ERROR_404 "<h1>Error 404: File Not Found Under Working Directory!</h1>"

int s_fg = 0;
char last_log[52] = "Last-Modified: ";
char now_log[52] =  "Date: ";

const char * oldspace = "%20";
const char * newspace = " ";
const int char_offset = 3;

void space_edit(char *origin){
    char buffer[1024] = {0};// set all zeros
    char *start = buffer;
    char *oldcopy = origin;
    
    while (1) {
        char *temp = strstr(oldcopy, oldspace);
        if(temp == NULL) {
            strcpy(start, oldcopy);
            break;
        }

        memcpy(start, oldcopy, temp - oldcopy);
        start += temp - oldcopy;
        memcpy(start, newspace, 1);
        start ++ ;
        oldcopy = temp + char_offset;
    }
    
    // write altered string back to target
    strcpy(origin, buffer);
}


void handler_404(int sock_fd, char* msg)
{
    write(sock_fd, MSG_404, strlen(MSG_404));
    write(sock_fd, ERROR_404, strlen(ERROR_404));
    printf("%s\n", msg);
    return;
}

void child_handler()
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}


void get_lasttimelog(struct stat status){
    struct tm* last_timelog;
    last_timelog = gmtime(&(status.st_mtime));
    char lasttime[35];
    strftime(lasttime, 35, "%a, %d %b %Y %T %Z", last_timelog);
    strcat(last_log, lasttime);
    strcat(last_log, "\r\n");
    return;
}
void get_firsttimelog(){
    // the part used to get timelog
    struct tm* timelog;
    time_t now;
    time(&now);
    timelog = gmtime(&now);
    char now_timelog[35];
    strftime(now_timelog, 35, "%a, %d %b %Y %T %Z", timelog);
    strcat(now_log, now_timelog);
    strcat(now_log, "\r\n");
    return;
    
}


void response(int sock_fd, char *f, size_t size)
{
    char message[512];
    char *connection = "Connection: close\r\n";
    char* msg;
    msg = MSG_200;
    char *server = "Server: ZXLY/1.0\r\n";
    
    get_firsttimelog();
    
    struct stat status;
    stat(f, &status);
    
    get_lasttimelog(status);
    
    char filesize[50] = "Content-Length: ";
    char length[10];
    sprintf (length, "%d", (unsigned int)size);
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
    file_type = "Type: text/plain\r\n";
    switch (indicator){
        case 0:
            file_type = "Type: text/html\r\n";
            break;
        case 1:
            file_type = "Type: text/plain\r\n";
            break;
        case 2:
            file_type = "Type: image/jpeg\r\n";
            break;
        case 3:
            file_type = "Type: image/jpg\r\n";
            break;
        case 4:
            file_type = "Type: image/gif\r\n";
            break;
        default:
            file_type = "Type: text/plain\r\n";
            break;
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
    
    
    write(sock_fd, message, strlen(message));
    
    printf("HTTP RESPONSE MESSAGE:\n%s\n", message);
    
    return;
    
}

void deliver(int sock_fd){
    
    int n;
    char buffer[512];
    char file_name[512];
    // clear buffer using bzero
    bzero(buffer,512);
    n = read(sock_fd,buffer,511); // read socket request message into buffer
    
    if(n < 0)
        printf("Error: Cannot read from socket!\n");
    
    memcpy(file_name, buffer, 512);
    
    char *tk;
    const char space[2] = " ";
    tk = strtok(file_name, space);
    tk = strtok(NULL, space);
    tk++;
    
    if(strlen(tk) <= 0)
        tk = "\0";
    
    printf("HTTP REQUEST MESSAGE:\n%s\n", buffer);
    
    // ----------------------
    
    char* buff = NULL;
    
    if( strcmp(tk,"\0") == 0 ){
        char mssg[19] = "File Not Specified";
        handler_404(sock_fd, mssg);
        return;
    }
    space_edit(tk);
    FILE* dfile = fopen(tk, "r");
    
    if (dfile == NULL){
        char mssg[19] = "404 File Not Found";
        handler_404(sock_fd, mssg);
        return;
    }
    
    if (fseek(dfile, 0L, SEEK_END) == 0){
        long size = ftell(dfile);
        if(size == -1){
            char mssg[16] = "404 File Error1";
            handler_404(sock_fd, mssg);
            return;
        }
        if(fseek(dfile, 0L, SEEK_SET) != 0){
            char mssg[16] = "404 File Error2";
            handler_404(sock_fd, mssg);
            return;
        }
        
        
        //fprintf(stderr, "%1ld\n", size); debug purpose
        buff = malloc(sizeof(char) * (size + 1));
        size_t buff_size = fread(buff, sizeof(char), size, dfile);
        if(buff_size == 0){
            char mssg[16] = "404 File Error3";
            handler_404(sock_fd, mssg);
            return;
        }
        buff[buff_size] = '\0';
        response(sock_fd, tk, buff_size);
        
        write(sock_fd, buff, buff_size);
        
        printf(" \"%s\" delivered!\n", tk);
    }
    
    fclose(dfile);
    free(buff);
    return;
    
}


int main(int argc, char* argv[]){
    
    int port = 2000;
    int sock_fd;
    int new_sock_fd;
    struct sockaddr_in serv_addr, cli_addr;
    
    
    if(argc < 2){
        fprintf(stderr,"Error: Please specify a port number.\n");
        exit(1);
    }
    
    port = atoi(argv[1]);
    
    
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(sock_fd < 0){
        fprintf(stderr,"Cannot create socket.\n");
        exit(1);
    }
    
    memset((void *) &serv_addr, 0, sizeof(serv_addr));//reset memory
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    
    if(bind(sock_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
        fprintf(stderr, "Bind error.\n");
        exit(1);
    }
    
    if(listen(sock_fd, 5) == 1){
        fprintf(stderr, "Listen error.\n");
        exit(1);
    }
    
    
    struct sigaction sig;
    sig.sa_handler = child_handler;
    sigemptyset(&sig.sa_mask);
    sig.sa_flags = SA_RESTART;
    if(sigaction(SIGCHLD, &sig, NULL) == -1){
        printf("%s\n", "Error: Child Process Error.");
        exit(1);
    }
    
    while(1){
        socklen_t client_len = sizeof(cli_addr);
        new_sock_fd = accept(sock_fd, (struct sockaddr *) &cli_addr, &client_len);
        if(new_sock_fd == - 1){
            fprintf(stderr, "sock creation error.\n");
            continue;
        }
        if(!fork()){
            close(sock_fd);
            deliver(new_sock_fd);
            exit(0);
        }
        close(new_sock_fd);
    }
    return 0;
}

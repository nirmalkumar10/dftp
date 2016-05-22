/***************************************************************************
 *            connections.c
 *
 *  Copyright 2005 Dimitur Kirov
 *  dkirov@gmail.com
 ****************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h> 
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#include <assert.h>
#include <netdb.h>
#include <errno.h>
#include <dirent.h>
#include "defines.h"
#include "cmdparser.h"
#include "fileutils.h"
#include <pthread.h>

int open_connections;
bool max_limit_notify;
int raiseerr(int err_code) {
	printf("Error %d\n",err_code);
	return -1;
}

#define NUMBER_OF_FILES 4
#define MAX_FILENAME 255
#define NUMBER_OF_SERVERS 4
#define MAX_SERVERNAME 20
struct duplicate{
char s1[MAX_SERVERNAME];
char s2[MAX_SERVERNAME];
char filename[MAX_FILENAME];
};

struct duplicate dupl;

struct map{
char filename[MAX_FILENAME];
int numservers;
int load;
char server[NUMBER_OF_SERVERS][MAX_SERVERNAME];
};
char serverfile[NUMBER_OF_SERVERS][MAX_SERVERNAME];
	struct map *dump;
struct clientinfo
{
char client_name[1024];
int cmd;
char request[1024];
};
/** 
 * This is neccessary for future use of glib and gettext based localization.
 */
const char * _(const char* message) {
	return message;
}

char client_info[1024];
/**
 * Guess the transfer type, given the client requested type.
 * Actually in unix there is no difference between binary and 
 * ascii mode when we work with file descriptors.
 * If #type is not recognized as a valid client request, -1 is returned.
 */
int get_type(const char *type) {
	if(type==NULL)
		return -1;
	int len = strlen(type);
	if(len==0)
		return -1;
	switch(type[0]) {
		case 'I':
			return 1;
		case 'A':
			return 2;
		case 'L':
			if(len<3)
				return -1;
			if(type[2]=='7')
				return 3;
			if(type[2]=='8')
				return 4;
	}
	return -1;
}

/**
 * Create a new connection to a (address,port) tuple, retrieved from
 * the PORT command. This connection will be used for data transfers
 * in commands like "LIST","STOR","RETR"
 */
int make_client_connection(int sock_fd,int client_port,const char* client_addr) {
	if(client_port<1) {
		send_repl(sock_fd,REPL_425);
		return -1;
	}
	int sock=-1;
	struct sockaddr_in servaddr;
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(client_addr);
	servaddr.sin_port = htons (client_port);
	if ((sock = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		send_repl(sock,REPL_425);
		raiseerr(15);
		return -1;
	}
	int status = connect (sock, (struct sockaddr *)&servaddr, sizeof (servaddr));
	if(status!=0) {
		send_repl(sock,REPL_425);
		return -1;
	}
	return sock;
}

void transfer(void *dupvar)
{
	int sock;
	struct clientinfo utils2;
	struct clientinfo utils1;  /*file exists */
	utils1.cmd = CMD_SER;
	utils2.cmd = CMD_CLI;
	
	struct duplicate *dup = (struct duplicate *)dupvar;
	printf("s1:%s s2:%s filename:%s\r\n",dup->s1,dup->s2,dup->filename);
	struct sockaddr_in server1,server2;

	memset(&server1,'0',sizeof(server1));
	memset(&server2,'0',sizeof(server2));
	server1.sin_family = server2.sin_family = AF_INET;
	server1.sin_port = server2.sin_port = htons(8200);
	server1.sin_addr.s_addr = inet_addr(dup->s1);    /*file exists */
	server2.sin_addr.s_addr = inet_addr(dup->s2);
	
	strcpy(utils1.client_name,dup->s2);
	strcpy(utils1.request,dup->filename);
	strcpy(utils2.request,dup->filename);

	if ((sock = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP)) < 0)
		printf("could not open socket\r\n");
	if(connect(sock,(struct sockaddr *)&server2,sizeof(server2)) < 0)
		printf("connect failed \r\n");
	write(sock,&utils2,sizeof(utils2));
	close(sock);

	 if ((sock = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP)) < 0)
                printf("could not open socket\r\n");
        if(connect(sock,(struct sockaddr *)&server1,sizeof(server1)) < 0)
                printf("connect failed \r\n");
        write(sock,&utils1,sizeof(utils1));
        close(sock);
}
/**
 * Close the connection to the client and exit the child proccess.
 * Although it is the same as close(sock_fd), in the future it can be used for 
 * logging some stats about active and closed sessions.
 */
struct clientinfo;

int make_server_connection(struct clientinfo data_buff)
{
	int sock,i,j,k,server_not_found =1;
	long double CpuLoad;
	struct clientinfo util;
	util.cmd = CMD_UTIL;
	struct sockaddr_in server;
	pthread_t thread;

	memset(&server,'0',sizeof(server));

	for(i=0;i<NUMBER_OF_FILES;i++){
		if(!strcmp(dump[i].filename,data_buff.request))
		{
			for(j=0;j<dump[i].numservers;j++)
			{ 
				server.sin_family = AF_INET;
				server.sin_port = htons(8200);
				server.sin_addr.s_addr = inet_addr(dump[i].server[j]);
				if ((sock = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP)) < 0)
					printf("could not open socket\r\n");
				printf("Connecting to server:%s\r\n",dump[i].server[j]);
				if(connect(sock,(struct sockaddr *)&server,sizeof(server)) < 0)
					printf("connect failed \r\n");

				write(sock,&util,sizeof(util));
				read(sock,&CpuLoad,sizeof(CpuLoad));
				printf("cpu load:%Lf\r\n",CpuLoad);
				close(sock);
				if(CpuLoad < 1){
				if(dump[i].load != 0){
				dump[i].load--;
				}
				server_not_found = 0;
				break;
				}else if((CpuLoad >=1) && ((j+1) < dump[i].numservers ) ){
				dump[i].load++;
				printf("condload:%d\r\n",dump[i].load);	
				printf("checking next server\r\n",j);
				}else{
				dump[i].load++;
				printf("elseload:%d\r\n",dump[i].load);
				server_not_found = 0;
				break;
				}

			}
		}
		if(server_not_found == 0)
			break;
	}

	memset(&server,'0',sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(8200);
	server.sin_addr.s_addr = inet_addr(dump[i].server[j]);
	if ((sock = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP)) < 0)
		printf("could not open socket\r\n");

	if(connect(sock,(struct sockaddr *)&server,sizeof(server)) < 0)
		printf("connect failed \r\n");


	write(sock,&data_buff,sizeof(data_buff));
	
	strcpy(dupl.filename,dump[i].filename);
	strcpy(dupl.s1,dump[i].server[j]);	
	
	for(k=0;k<NUMBER_OF_SERVERS;k++){
	if(strcmp(dump[i].server[j],serverfile[k])){
	strcpy(dupl.s2,serverfile[k]);
	break;
	}
	}
	printf("load:%d\r\n",dump[i].load);
	if(dump[i].load > 2)
	pthread_create(&thread,NULL,transfer,(void*)&dupl);
	
	return sock;
}

void close_conn(int sock_fd) {
	if (close(sock_fd) < 0) { 
		raiseerr (5);
	}
	exit(0);
}

/**
 * Get the next command from the client socket.
 */
int get_command(int conn_fd,char *read_buff1,char *data_buff) {
	char read_buff[RCVBUFSIZE];
	memset((char *)&read_buff, 0, RCVBUFSIZE);
	read_buff[0]='\0';
	char *rcv=read_buff;
	int cmd_status = -1;
	int recvbuff = recv(conn_fd,read_buff,RCVBUFSIZE,0);
	if(recvbuff<1) {
		return CMD_CLOSE;
	}
	if(recvbuff==RCVBUFSIZE) {
		return CMD_UNKNOWN;
	}
	// printf("Received:%s\n",rcv);
	cmd_status = parse_input(rcv,data_buff);
	return cmd_status;
}

/**
 * A handler, which is called on child proccess exit.
 */
void sig_chld_handler(void) {
	open_connections--;
	while (waitpid(-1, NULL, WNOHANG) > 0);
}

/**
 * Send reply to the client socket, given the reply.
 */
int send_repl(int send_sock,char *msg) {
	if (send(send_sock, msg, strlen(msg),0) < 0) { 
		raiseerr (4);
		close(send_sock);
		exit(0);
	}
	return 0;
}

/**
 * Send single reply to the client socket, given the reply and its length.
 */
int send_repl_client_len(int send_sock,char *msg,int len) {
	if (send(send_sock, msg, len,0) < 0) { 
		raiseerr (4);
		close(send_sock);
	}
	return 0;
}

/*
Izprashtane na edinishen otgovor do dopulnitelnia socket za transfer
*/
int send_repl_client(int send_sock,char *msg) {
	send_repl_client_len(send_sock,msg,strlen(msg));
	return 0;
}

/**
 * Send single reply to the additional transfer socket, given the raply and its length.
 */
int send_repl_len(int send_sock,char *msg,int len) {
	if (send(send_sock, msg, len,0) < 0) { 
		raiseerr (4);
		close(send_sock);
		exit(0);
	}
	return 0;
}

/**
 * Parses the results from the PORT command, writes the
 * address in "client_addt" and returnes the port
 */
int parse_port_data(char *data_buff,char *client_addr) {
	client_addr[0]='\0';
	int len=0;
	int port=0;
	int _toint=0;
	char *result;
	result = strtok(data_buff, PORTDELIM);
	_toint=toint(result,FALSE);
	if(_toint<1 || _toint>254)
		return -1;
	len += strlen(result);
	strcpy(client_addr,result);
	client_addr[len]='\0';
	strcat(client_addr,".");
	len++;

	result = strtok(NULL, PORTDELIM);
	_toint=toint(result,FALSE);
	if(_toint<0 || _toint>254)
		return -1;
	len += strlen(result);
	strcat(client_addr,result);
	client_addr[len]='\0';
	strcat(client_addr,".");
	len++;

	result = strtok(NULL, PORTDELIM);
	if(_toint<0 || _toint>254)
		return -1;
	len += strlen(result);
	strcat(client_addr,result);
	client_addr[len]='\0';
	strcat(client_addr,".");
	len++;

	result = strtok(NULL, PORTDELIM);
	if(_toint<0 || _toint>254)
		return -1;
	len += strlen(result);
	strcat(client_addr,result);
	client_addr[len]='\0';
	
	result = strtok(NULL, PORTDELIM);
	len = toint(result,FALSE);
	if(_toint<0 || _toint>255)
		return -1;
	port = 256*len;
	result = strtok(NULL, PORTDELIM);
	len = toint(result,FALSE);
	if(_toint<0 || _toint>255)
		return -1;
	port +=len;
	return port;
}
void print_help(int sock) {
	send_repl(sock,"    Some help message.\r\n    Probably nobody needs help from telnet.\r\n    See rfc959.\r\n");
}
/**
 * Main cycle for client<->server communication. 
 * This is done synchronously. On each client message, it is parsed and recognized,
 * certain action is performed. After that we wait for the next client message
 * 
 */


int interract(int conn_fd,cmd_opts *opts) {
	static int BANNER_LEN = strlen(REPL_220);
	int userid = opts->userid;
	int client_fd=-1,ssock;
	int len;
	int _type ;
	int type = 2; // ASCII TYPE by default

	struct clientinfo client;

	if(userid>0) {
		int status = setreuid(userid,userid);
		if(status != 0) {
			switch(errno) {
				case EPERM:
					break;
				case EAGAIN:
					break;
				default:
					break;
			}
			close_conn(conn_fd);
		}
		
	}
	if(max_limit_notify) {
		send_repl(conn_fd,REPL_120);
		close_conn(conn_fd);
	}
	char current_dir[MAXPATHLEN];
	char parent_dir[MAXPATHLEN];
	char virtual_dir[MAXPATHLEN];
	char reply[SENDBUFSIZE];
	char data_buff[DATABUFSIZE];
	char read_buff[RCVBUFSIZE];
	char *str;
	bool is_loged = FALSE;
	bool state_user = FALSE;
	char rename_from[MAXPATHLEN];
	
	memset((char *)&current_dir, 0, MAXPATHLEN);
	strcpy(current_dir,opts->chrootdir);
	strcpy(parent_dir,opts->chrootdir);
	free(opts);
	chdir(current_dir);
	if((getcwd(current_dir,MAXPATHLEN)==NULL)) {
		raiseerr(19);
		close_conn(conn_fd);
	}
	memset((char *)&data_buff, 0, DATABUFSIZE);
	memset((char *)&read_buff, 0, RCVBUFSIZE);
	
	reply[0]='\0';
	int client_port = 0;
	char client_addr[ADDRBUFSIZE];

	send_repl_len(conn_fd,REPL_220,BANNER_LEN);
	while(1) {
		data_buff[0]='\0';
		int result = get_command(conn_fd,read_buff,data_buff);
		if(result != CMD_RNFR && result != CMD_RNTO && result != CMD_NOOP)
			rename_from[0]='\0';
		switch(result) {
			case CMD_UNKNOWN:
			case -1:
				send_repl(conn_fd,REPL_500);
				break;
			case CMD_EMPTY:
			case CMD_CLOSE:
				close_conn(conn_fd);
				break;
			case CMD_USER:
				if(data_buff==NULL || strcmp(data_buff,ANON_USER)==0) {
					state_user = TRUE;
					send_repl(conn_fd,REPL_331_ANON);
				}
				else {
					send_repl(conn_fd,REPL_332);
				}
				break;
			case CMD_PASS:
				if(!state_user) {
					send_repl(conn_fd,REPL_503);
				}
				else {
					is_loged = TRUE;
					state_user = FALSE;
					send_repl(conn_fd,REPL_230);
				}
				break;
			case CMD_PORT:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					strcpy(client.client_name,data_buff);
					client_port = parse_port_data(data_buff,client_addr);
					if(client_port<0) {
						send_repl(conn_fd,REPL_501);
						client_port = 0;
					} else {
						send_repl(conn_fd,REPL_200);
					}
				}
				break;
			case CMD_PASV:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					send_repl(conn_fd,REPL_502);
				}
				break;
			case CMD_SYST:
				reply[0]='\0';
				len = sprintf(reply,REPL_215,"UNIX");
				reply[len] = '\0';
				send_repl(conn_fd,reply);
				break;
			case CMD_LIST:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					client.cmd =result;	
					client_fd = make_server_connection(client);//make_client_connection(conn_fd, client_port,client_addr);
					if(client_fd!=-1){
						write_list(conn_fd,client_fd,current_dir);
					}
					client_fd = -1;
				}
				break;
			case CMD_RETR:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					client.cmd  = result;
					strcpy(client.request,data_buff);
					if(data_buff==NULL || strlen(data_buff)==0 || data_buff[0]=='\0') {
						send_repl(conn_fd,REPL_501);
					} else {
						ssock = make_server_connection(client);
						if(ssock != -1){
						send_repl(conn_fd,REPL_150);
						}
						read(ssock,data_buff,sizeof(data_buff));
						send_repl(conn_fd,data_buff);
					}
				}
				break;
			case CMD_STOU:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					int fd = mkstemp("XXXXX");
					client_fd = make_client_connection(conn_fd, client_port,client_addr);
					if(client_fd!=-1){
						stou_file(conn_fd,client_fd, type,fd);
					}
					client_fd = -1;
				}
				break;
			case CMD_STOR:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					client.cmd = result;
					strcpy(client.request,data_buff);
					if(data_buff==NULL || strlen(data_buff)==0 || data_buff[0]=='\0') {
						send_repl(conn_fd,REPL_501);
					} else {
						ssock = make_server_connection(client);
						if(ssock != -1){
							send_repl(conn_fd,REPL_150);
						}
						read(ssock,data_buff,sizeof(data_buff));
						send_repl(conn_fd,data_buff);
					}
				}
				break;
			case CMD_SITE:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					if(data_buff==NULL || strlen(data_buff)==0 || data_buff[0]=='\0') {
						send_repl(conn_fd,REPL_501);
					} else {
						send_repl(conn_fd,REPL_202);
					}
				}
				break;
			case CMD_PWD:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					reply[0]='\0';
					len = sprintf(reply,REPL_257_PWD,current_dir);
					reply[len] = '\0';
					send_repl(conn_fd,reply);
				}
				break;
			case CMD_CDUP:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					change_dir(conn_fd,parent_dir,current_dir,virtual_dir,"..");
				}
				break;
			case CMD_CWD:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					if(data_buff==NULL || strlen(data_buff)==0 || data_buff[0]=='\0') {
						send_repl(conn_fd,REPL_501);
					} else {
						change_dir(conn_fd,parent_dir,current_dir,virtual_dir,data_buff);
					}
				}
				break;
			case CMD_QUIT:
				send_repl(conn_fd,REPL_221);
				if(client_fd!=-1){
					close_conn(client_fd);
				}
				close_conn(conn_fd);
				break;
			case CMD_TYPE:
				_type = get_type(data_buff);
				if(_type ==-1) {
					send_repl(conn_fd,REPL_500);
				}
				else {
					type=_type;
					send_repl(conn_fd,REPL_200);
				}
				break;
			case CMD_STAT:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					if(data_buff==NULL || strlen(data_buff)==0 || data_buff[0]=='\0') {
						send_repl(conn_fd,REPL_501);
					}
					else {
						stat_file(conn_fd,data_buff,reply);
					}
				}
				break;
			case CMD_ABOR:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					if(client_fd!=-1){
						close_connection(client_fd);
					} 
					send_repl(conn_fd,REPL_226);
				}
				break;
			case CMD_MKD:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					if(data_buff==NULL || strlen(data_buff)==0 || data_buff[0]=='\0') {
						send_repl(conn_fd,REPL_501);
					} else {
						make_dir(conn_fd,data_buff,reply);
					}
				}
				break;
			case CMD_RMD:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					if(data_buff==NULL || strlen(data_buff)==0 || data_buff[0]=='\0') {
						send_repl(conn_fd,REPL_501);
					} else {
						remove_dir(conn_fd,data_buff);
					}
				}
				break;
			case CMD_DELE:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					if(data_buff==NULL || strlen(data_buff)==0 || data_buff[0]=='\0') {
						send_repl(conn_fd,REPL_501);
					} else {
						delete_file(conn_fd,data_buff);
					}
				}
				break;
			case CMD_RNFR:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					if(data_buff==NULL || strlen(data_buff)==0 || data_buff[0]=='\0') {
						send_repl(conn_fd,REPL_501);
					} else {
						strcpy(rename_from,data_buff);
						send_repl(conn_fd,REPL_350);
					}
				}
				break;
			case CMD_RNTO:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					if(data_buff==NULL || strlen(data_buff)==0 || data_buff[0]=='\0') {
						send_repl(conn_fd,REPL_501);
					} else {
						if(rename_from==NULL || strlen(rename_from)==0 || rename_from[0]=='\0') {
							send_repl(conn_fd,REPL_501);
						} else {
							rename_fr(conn_fd,rename_from,data_buff);
						}
					}
					rename_from[0]='\0';
				}
				break;
			case CMD_NOOP:
				send_repl(conn_fd,REPL_200);
				break;
			case CMD_STRU:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					if(data_buff==NULL || strlen(data_buff)==0 || data_buff[0]=='\0') {
						send_repl(conn_fd,REPL_501);
					} else {
						switch(data_buff[0]) {
							case 'F':
								send_repl(conn_fd,REPL_200);
								break;
							case 'P':
							case 'R':
								send_repl(conn_fd,REPL_504);
								break;
							default:
								send_repl(conn_fd,REPL_501);
							
						}
					}
				}
				break;
			case CMD_HELP:
			//	if(data_buff==NULL || strlen(data_buff)==0 || data_buff[0]=='\0') {
					send_repl(conn_fd,REPL_214);
					print_help(conn_fd);
					send_repl(conn_fd,REPL_214_END);
			//	}
			// XXX separate HELP without arguments from HELP for a single command
				break;
			case CMD_MODE:
				if(!is_loged) send_repl(conn_fd,REPL_530);
				else {
					if(data_buff==NULL || strlen(data_buff)==0 || data_buff[0]=='\0') {
						send_repl(conn_fd,REPL_501);
					} else {
						switch(data_buff[0]) {
							case 'S':
								send_repl(conn_fd,REPL_200);
								break;
							case 'B':
							case 'C':
								send_repl(conn_fd,REPL_504);
								break;
							default:
								send_repl(conn_fd,REPL_501);
							
						}
					}
				}
				break;
			default:
				send_repl(conn_fd,REPL_502);
		}
	}
	
	free(data_buff);
	free(read_buff);
	free(current_dir);
	free(parent_dir);
	free(virtual_dir);
	free(rename_from);
	close_conn(conn_fd);
}

/**
 * Close a socket and return a statsu of the close operation.
 * Although it is equivalent to close(connection) in the future it can be used
 * for writing logs about opened and closed sessions.
 */
int close_connection(int connection) {
	return close(connection);
}

/**
 * Creates new server listening socket and make the main loop , which waits
 * for new connections.
 */
int create_socket(struct cmd_opts *opts) {
	if(opts==NULL)
		return 10;
	int status = chdir(opts->chrootdir);
	if(status!=0) {
		raiseerr(15);
	}
	int servaddr_len =  0;
	int connection = 0;
	int sock = 0;
	int pid  = 0;
	open_connections=0;
	struct sockaddr_in servaddr;
	pid = getuid();	
	if(pid != 0 && opts->port <= 1024)
	{
		printf(_(" Access denied:\n     Only superuser can listen to ports (1-1024).\n You can use \"-p\" option to specify port, greater than 1024.\n"));
		exit(1);
	}
	memset((char *)&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = PF_INET;
	if(opts->listen_any==TRUE) {
		servaddr.sin_addr.s_addr =  htonl(INADDR_ANY);
	}
	else if(opts->listen_addr==NULL) {
		return 9;
	} else {
		struct hostent *host = gethostbyname(opts->listen_addr);   /* getaddrinfo */
		if(host==NULL) {
			printf(_("Cannot create socket on server address: %s\n"),opts->listen_addr);
			return 11;
		}
		bcopy(host->h_addr, &servaddr.sin_addr, host->h_length);
	}
	servaddr.sin_port = htons (opts->port);
	servaddr_len = sizeof(servaddr);
	if ((sock = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		raiseerr(ERR_CONNECT);
		return 1;
	}
	int flag = 1;
	setsockopt(sock, SOL_SOCKET,SO_REUSEADDR,(char *) &flag, sizeof(int));
	
	// remove the Nagle algorhytm, which improves the speed of sending data.
	setsockopt(sock, IPPROTO_TCP,TCP_NODELAY,(char *) &flag, sizeof(int));
	
	if(bind (sock, (struct sockaddr *)&servaddr, sizeof(servaddr))<0) {
		if(opts->listen_any==FALSE) {
			printf(_("Cannot bind address: %s\n"),opts->listen_addr);
		}else {
			printf(_("Cannot bind on default address\n"));
		}
		return raiseerr(8);
	}
	if(listen(sock,opts->max_conn) <0) {
		return raiseerr(2);
	}
	#ifdef __USE_GNU
		signal(SIGCHLD, (sighandler_t )sig_chld_handler);
	#endif
	#ifdef __USE_BSD
		signal(SIGCHLD, (sig_t )sig_chld_handler);
	#endif

	for (;;) {
		max_limit_notify = FALSE;
		if ((connection = accept(sock, (struct sockaddr *) &servaddr, &servaddr_len)) < 0) {
			raiseerr(3);
			return -1;
		}
		
		pid = fork();
		if(pid==0) {
			if(open_connections >= opts->max_conn)
				max_limit_notify=TRUE;
			interract(connection,opts);
		} else if(pid>0) {
			open_connections++;
			assert(close_connection(connection)>=0);
		}
		else {
			 
			close(connection);
			close(sock);
			assert(0);
		}
	}
}

int update_server_info(struct cmd_opts *opts)
{

	int i,j,read =1,numfiles=0;
	size_t len;
	char *line;
	FILE *fp;
	char *filename,*servername;

	FILE *fsave = fopen(opts->fsave,"rb");
	dump = (struct map *)malloc(sizeof(struct map) * NUMBER_OF_FILES);

	if(fsave == NULL)
	{
		FILE *fp = fopen(opts->filename,"r");
		if(fp != NULL) {
			while(1){
				read = getline(&line,&len,fp);
				if(read == -1)
					break;
				++numfiles;
			}

			rewind(fp);



			for(i=0;i<numfiles;i++)
			{
				getline(&line,&len,fp);
				filename = strtok(line," ");
				strcpy(dump[i].filename,filename);
				//printf("filename:%s\r\n",dump[i].filename);
				for(j=0;j<NUMBER_OF_SERVERS;j++)
				{
					servername = strtok(NULL," ");
					if(servername){
						dump[i].numservers++;
						strcpy(dump[i].server[j],servername);
				//		printf("server:%s\r\n",dump[i].server[j]);
					}else{
						break;
					}
				}
			}

			fclose(fp);
		}else{
			printf("Error opening server details:%s",opts->filename);
		}
	}
	FILE *sp = fopen(opts->sfile,"r");
	if(sp){
		for(i=0;i<NUMBER_OF_SERVERS;i++){
			if(getline(&line,&len,sp) != -1){
				strcpy(serverfile[i],line);
			}else{
				break;
			}
		}
	fclose(sp);
	}
}

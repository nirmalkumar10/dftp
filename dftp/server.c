/* 
 * tcpserver.c - A simple TCP echo server 
 * usage: tcpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <fcntl.h>

#define REPL_226 "226 Closing data connection.\r\n"
#define REPL_150 "150 File status okay; about to open data connection.\r\n"
#define BUFSIZE 1024
#define PORTDELIM ","
#define ADDRBUFSIZE 16
#define SENDBUFSIZE 512
#define FALSE 0
#define TRUE 1
#if 0
/* 
 * Structs exported from in.h
 */

/* Internet address */
struct in_addr {
  unsigned int s_addr; 
};

/* Internet style socket address */
struct sockaddr_in  {
  unsigned short int sin_family; /* Address family */
  unsigned short int sin_port;   /* Port number */
  struct in_addr sin_addr;	 /* IP address */
  unsigned char sin_zero[...];   /* Pad to size of 'struct sockaddr' */
};

/*
 * Struct exported from netdb.h
 */

/* Domain name service (DNS) host entry */
struct hostent {
  char    *h_name;        /* official name of host */
  char    **h_aliases;    /* alias list */
  int     h_addrtype;     /* host address type */
  int     h_length;       /* length of address */
  char    **h_addr_list;  /* list of addresses */
}
#endif
enum {
        CMD_USER,
        CMD_PASS,
        CMD_ACCT,
        CMD_CWD,
        CMD_CDUP,
        CMD_SMNT,
        CMD_QUIT,
        CMD_REIN,
        CMD_PORT,
        CMD_PASV,
        CMD_TYPE,
        CMD_STRU,
        CMD_MODE,
        CMD_RETR,
        CMD_STOR,
        CMD_STOU,
        CMD_APPE,
        CMD_ALLO,
        CMD_REST,
        CMD_RNFR,
        CMD_RNTO,
        CMD_ABOR,
        CMD_DELE,
        CMD_RMD,
        CMD_MKD,
        CMD_PWD,
        CMD_LIST,
        CMD_NLST,
        CMD_SITE,
        CMD_SYST,
        CMD_STAT,
        CMD_HELP,
        CMD_NOOP,
        CMD_UNKNOWN,
        CMD_EMPTY,
        CMD_CLOSE,
	CMD_UTIL,
	CMD_SER,
	CMD_CLI
};

int parse_port_data(char *,char *);
bool stou_file(int,int,int,int);
bool store_file(int,int,int,const char *);
bool  retrieve_file(int,int,int,const char *);
char client_addr[ADDRBUFSIZE];
/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

typedef struct clientinfo{
char client_name[1024];
int cmd;
char request[1024];

}clientinfo;


long double  cpu_load()
{
    long double a[4], b[4], loadavg;
    FILE *fp;
    char dump[50];

        fp = fopen("/proc/stat","r");
        fscanf(fp,"%*s %Lf %Lf %Lf %Lf",&a[0],&a[1],&a[2],&a[3]);
        fclose(fp);
        sleep(1);

        fp = fopen("/proc/stat","r");
        fscanf(fp,"%*s %Lf %Lf %Lf %Lf",&b[0],&b[1],&b[2],&b[3]);
        fclose(fp);

        loadavg = ((b[0]+b[1]+b[2]) - (a[0]+a[1]+a[2])) / ((b[0]+b[1]+b[2]+b[3]) - (a[0]+a[1]+a[2]+a[3]));
        printf("The current CPU utilization is : %Lf\n",loadavg);
	return loadavg;
}

int main(int argc, char **argv) {
  int parentfd; /* parent socket */
  int masterfd; /* child socket */
  int portno; /* port to listen on */
  int clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE]; /* message buffer */
  char hostaddrp[20]; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */
	long double CpuLoad;
	int client_port;
	clientinfo client;
  /* 
   * check command line arguments 
   */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);

  /* 
   * socket: create the parent socket 
   */
  parentfd = socket(AF_INET, SOCK_STREAM, 0);
  if (parentfd < 0) 
    error("ERROR opening socket");

  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  optval = 1;
  setsockopt(parentfd, SOL_SOCKET, SO_REUSEADDR, 
	     (const void *)&optval , sizeof(int));

  /*
   * build the server's Internet address
   */
  bzero((char *) &serveraddr, sizeof(serveraddr));

  /* this is an Internet address */
  serveraddr.sin_family = AF_INET;

  /* let the system figure out our IP address */
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

  /* this is the port we will listen on */
  serveraddr.sin_port = htons((unsigned short)portno);

  /* 
   * bind: associate the parent socket with a port 
   */
  if (bind(parentfd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  /* 
   * listen: make this socket ready to accept connection requests 
   */
  if (listen(parentfd, 5) < 0) /* allow 5 requests to queue up */ 
    error("ERROR on listen");

  /* 
   * main loop: wait for a connection request, echo input line, 
   * then close connection.
   */
  clientlen = sizeof(clientaddr);
  while (1) {

    /* 
     * accept: wait for a connection request 
     */
    masterfd = accept(parentfd, (struct sockaddr *) &clientaddr, &clientlen);
    if (masterfd < 0) 
      error("ERROR on accept");
    
	inet_ntop(AF_INET,&clientaddr.sin_addr.s_addr,hostaddrp,sizeof(hostaddrp));
	printf("%s\r\n",hostaddrp);

    /* 
     * gethostbyaddr: determine who sent the message 
     *//*
    hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
			  sizeof(clientaddr.sin_addr.s_addr), AF_INET);
    if (hostp == NULL)
      error("ERROR on gethostbyaddr");
    hostaddrp = inet_ntoa(clientaddr.sin_addr);
    if (hostaddrp == NULL)
      error("ERROR on inet_ntoa\n");
    printf("server established connection with %s (%s)\n", 
	   hostp->h_name, hostaddrp);
    */
    /* 
     */
	memset(&client,'0',sizeof(client));
    	read(masterfd,&client, sizeof(client));
    	printf("server received client cmd:%d filename: %s", client.cmd, client.request);
	
	switch(client.cmd){
	case CMD_RETR:{	
	client_port = parse_port_data(client.client_name,client_addr);
  	printf("connecting to addr:%s port:%d\r\n",client_addr,client_port); 
	int sock=-1;
	struct sockaddr_in servaddr;
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = inet_addr(client_addr);
        servaddr.sin_port = htons(client_port);
        if ((sock = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
                return -1;
        }
        int status = connect (sock, (struct sockaddr *)&servaddr, sizeof (servaddr));

	retrieve_file(masterfd,sock,0,client.request);
	break;
	}
	case CMD_STOR:{
	client_port = parse_port_data(client.client_name,client_addr);
  	printf("connecting to addr:%s port:%d\r\n",client_addr,client_port); 
	 int sock=-1;
        struct sockaddr_in servaddr;
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = inet_addr(client_addr);
        servaddr.sin_port = htons(client_port);
        if ((sock = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
                return -1;
        }
        int status = connect (sock, (struct sockaddr *)&servaddr, sizeof (servaddr));
	int fd = mkstemp("XXXXX");	
	store_file(masterfd,sock, 0,client.request);	
	break;
	}
	case CMD_UTIL:{
	CpuLoad = cpu_load();
	write(masterfd,&CpuLoad,sizeof(CpuLoad));	
	break;	
	}
	case CMD_SER:{
	char readp[1024];
	printf("asking me to be server to connect to :%s\r\n",client.client_name);
	int isock=-1;
        struct sockaddr_in servaddr;
	FILE *fp;
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = inet_addr(client.client_name);
        servaddr.sin_port = htons(8207);
        if ((isock = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
                return -1;
        }
        int status = connect (isock, (struct sockaddr *)&servaddr, sizeof (servaddr));
	fp =fopen(client.request,"r");
	while(!feof(fp))
	{
	fread(readp,sizeof(readp),1,fp);
	write(isock,readp,sizeof(readp));
	}
	fclose(fp);
	close(isock);
	break;
	}
	}
	
  }
}


int toint(str,strict)
        const char* str ;
        bool strict;
        {
        int len = strlen(str);
        if(len<1) { 
                return -1;
        }
        int i;
        int base=1;
        int retval = 0;
        for(i=len-1;i>=0;i--,base *= 10) {
                if(base>=10000) {
                        return -1;
                }
                if((int)str[i] >47 && (int)str[i]<58) {
                        retval+=base*(str[i]-48);
                } else {
                        if(strict)
                                return -1;
                }
        }
        return retval;
}

int parse_port_data(char *data_buff,char *client_addr) {
        client_addr[0]='\0';
        int len=0;
        int port=0;
        int _toint=0;
        char *result;
        printf("data for port %s",data_buff);
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


int send_repl_client_len(int send_sock,char *msg,int len) {
        if (send(send_sock, msg, len,0) < 0) { 
  //              raiseerr (4);
                close(send_sock);
        }
        return 0;
}

/**
 * Send reply to the client socket, given the reply.
 */
int send_repl(int send_sock,char *msg) {
        if (send(send_sock, msg, strlen(msg),0) < 0) {
                //raiseerr (4);
                close(send_sock);
                exit(0);
        }
        return 0; 
}


bool retrieve_file(int masterfd,int client_sock, int type, const char * file_name) {
        char read_buff[SENDBUFSIZE];
        if(client_sock>0) {
                //close(client_sock);
                //send_repl(sock,REPL_150);
        }
        else {
                close(client_sock);
                //send_repl(sock,REPL_425);
                free(read_buff);
                return FALSE;
        }
        struct stat s_buff;
        int status = stat(file_name,&s_buff);
        if(status!=0) {
                close(client_sock);
                //send_repl(sock,REPL_450);
                free(read_buff);
                return FALSE;
        }
        int b_mask = s_buff.st_mode & S_IFMT;
        if(b_mask != S_IFREG){
                close(client_sock);
                //send_repl(sock,REPL_451);
                free(read_buff);
                return FALSE;
        }
        char mode[3] ="r ";
        switch(type){
                case 1:
                case 3:
                case 4:
                        mode[1]='b';
                        break;
                case 2:
                default:
                        mode[1]='t';
        }

        int fpr = open(file_name,O_RDONLY);
        if(fpr<0) {
                close(client_sock);
                //send_repl(sock,REPL_451);
                free(mode);
                free(read_buff);
                return FALSE;
        }

        // make transfer unbuffered
        int opt = fcntl(client_sock, F_GETFL, 0);
        if (fcntl(client_sock, F_SETFL, opt | O_ASYNC) == -1)
                {
                  //      send_repl(sock,REPL_426);
                        //close_connection(client_sock);
                        free(read_buff);
                        return FALSE;
                }
        while(1){
                int len = read(fpr,read_buff,SENDBUFSIZE);
		printf("read from file:%d\r\n",len);
                if(len>0) {
                        send_repl_client_len(client_sock,read_buff,len);
                }
                else {
                        break;
                }
        }
        close(fpr);
        send_repl(masterfd,REPL_226);
	printf("sent commnad to master\r\n");
	close(client_sock);
	close(masterfd);
        return TRUE;
}


bool stou_file(int masterfd, int client_sock, int type, int fpr) {
        char read_buff[SENDBUFSIZE];
        if(fpr<0) {
                close(client_sock);
               // send_repl(masterfd,REPL_451);
                free(read_buff);        
                return FALSE;           
        }                       
        
	int opt = fcntl(client_sock, F_GETFL, 0);
        if (fcntl(client_sock, F_SETFL, opt | O_ASYNC) == -1) {
                //send_repl(masterfd,REPL_426);
                close(client_sock);
                free(read_buff);
                return FALSE;   
        }                       
        while(1){                       
                                                
                int len = recv(client_sock,read_buff,SENDBUFSIZE,0);
                if(len>0) {              
                        write(fpr,read_buff,len);
                }
                else {
                        break;
                }
        }
        close(client_sock);
        close(fpr);
        send_repl(masterfd,REPL_226);
        return TRUE;
}

bool store_file(int masterfd, int client_sock, int type, const char * file_name) {
        char read_buff[SENDBUFSIZE];
        if(client_sock>0) {
                //close(client_sock);
                //send_repl(sock,REPL_150);
        }
        else {
                close(client_sock);
                //send_repl(masterfd,REPL_425);
                free(read_buff);
                return FALSE;
        }
        struct stat s_buff;
        int status = stat(file_name,&s_buff);
        if(status==0) {
                int b_mask = s_buff.st_mode & S_IFMT;
                if(b_mask != S_IFREG){
                        free(read_buff);
                        close(client_sock);
                  //      send_repl(masterfd,REPL_451);

                        return FALSE;
                }
        }
        char mode[3] ="w ";
        switch(type){
                case 1:
                case 3:
                case 4:
                        mode[1]='b';
                        break;
                case 2:
                default:
                        mode[1]='t';
        }

        int fpr = open(file_name,O_WRONLY|O_CREAT,0644);
        return stou_file(masterfd, client_sock,0 ,fpr);
}

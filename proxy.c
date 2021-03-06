

/* this is not a working program yet, but should help you get started */

#include <stdio.h>
#include "csapp.h"
#include "proxy.h"
#include <pthread.h>

#define   LOG_FILE      "proxy.log"
#define   DEBUG_FILE	"proxy.debug"
#define MAX_OBJECT_SIZE 102400

/*============================================================
 * function declarations
 *============================================================*/

int  find_target_address(char * uri,
			 char * target_address,
			 char * path,
			 int  * port);


void  format_log_entry(char * logstring,
		       int sock,
		       char * uri,
		       int size);
		       
void *webTalk(void* args);
void *secureTalk(int clientfd, rio_t client, char* host, int serverPort);
void *listenfromserver(void* args);
void ignore();

int debug;
int proxyPort;
int debugfd;
int logfd;
pthread_mutex_t mutex;

/* main function for the proxy program */

int main(int argc, char *argv[])
{
  int count = 0;
  int listenfd, connfd, clientlen, optval, serverPort, i;
  struct sockaddr_in clientaddr;
  struct hostent *hp;
  char *haddrp;
  sigset_t sig_pipe; 
  pthread_t tid;
  int* args;
  
  if (argc < 2) {
    printf("Usage: ./%s port [debug] [serverport]\n", argv[0]);
    exit(1);
  }

  proxyPort = atoi(argv[1]);

  /* turn on debugging if user enters a 1 for the debug argument */

  if(argc > 2)
    debug = atoi(argv[2]);
  else
    debug = 0;

  if(argc == 4)
    serverPort = atoi(argv[3]);
  else
    serverPort = 80;

  /* deal with SIGPIPE */

  Signal(SIGPIPE, ignore);
  
  if(sigemptyset(&sig_pipe) || sigaddset(&sig_pipe, SIGPIPE))
    unix_error("creating sig_pipe set failed");

  if(sigprocmask(SIG_BLOCK, &sig_pipe, NULL) == -1)
    unix_error("sigprocmask failed");

  /* important to use SO_REUSEADDR or can't restart proxy quickly */

  listenfd = Open_listenfd(proxyPort);
  optval = 1;
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int)); 
  
  if(debug) debugfd = Open(DEBUG_FILE, O_CREAT | O_TRUNC | O_WRONLY, 0666);

  logfd = Open(LOG_FILE, O_CREAT | O_TRUNC | O_WRONLY, 0666);    
  
  /* protect log file with a mutex */

  pthread_mutex_init(&mutex, NULL);
  

  /* not wait for new requests from browsers */

  while(1) {
    clientlen = sizeof(clientaddr);

    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    
    hp = Gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,
		       sizeof(clientaddr.sin_addr.s_addr), AF_INET);

    haddrp = inet_ntoa(clientaddr.sin_addr);
    args = malloc(2*sizeof(int));
    args[0] = connfd; args[1] = serverPort;

    /* spawn a thread to process the new connection */

    Pthread_create(&tid, NULL, webTalk, (void*) args);
    Pthread_detach(tid);
  }


  /* should never get here, but if we do, clean up */

  Close(logfd);  
  if(debug) Close(debugfd);

  pthread_mutex_destroy(&mutex);
  
}

void parseAddress(char* url, char* host, char** file, int* serverPort)
{
	char buf[MAXLINE];
	char* point1, *point2;
	char *saveptr;
	if(strstr(url, "http://"))
		url = &(url[7]);
	*file = strchr(url, '/');
	
	strcpy(buf, url);
	point1 = strchr(url, ':');

	strcpy(host,url);
	strtok_r(host, ":/", &saveptr);

	if(!point1) {
		*serverPort = 80;
		return;
	}
	*serverPort = atoi(strtok_r(NULL, ":/",&saveptr));
}


/* WebTalk()
 *
 * Once a connection has been established, webTalk handles
 * the communication.
 */


/* this function is not complete */
/* you'll do the bulk of your work here */

void *webTalk(void* args)
{
	int numBytes, serverBytes, serverfd, clientfd, serverPort;
	int serverbytes;
	int tries;
	int byteCount = 0;
	char buf1[MAXLINE], buf2[MAXLINE], buf3[MAXLINE],header[MAX_OBJECT_SIZE];
	char url[MAXLINE], logString[MAXLINE];
	char *token, *cmd, *version, *file;
	char host[MAXLINE];
	char *saveptr;
	rio_t server, client;
	char slash[10];
	strcpy(slash, "/");
	void* ret = (void*) malloc(16);

	clientfd = ((int*)args)[0];
	serverPort = ((int*)args)[1];
	free(args);
	Rio_readinitb(&client, clientfd);
	
	/* Determine whether request is GET or CONNECT */
	numBytes = Rio_readlineb(&client, buf1, MAXLINE);
	if (strlen(buf1)>0){

		cmd = strtok_r(buf1, " \r\n", &saveptr);
		strcpy(url, strtok_r(NULL, " \r\n",&saveptr));
		//sscanf(buf1, "%s %s %s",cmd, url, version);
		//printf("%s\n", buf1);    // for test : it is "GET"
		//printf("%d\n", numBytes);  // for test
		printf("the cmd is %s\n the url is %s\n",cmd,url );
		parseAddress(url, host, &file, &serverPort); // ) {
		if(serverPort==0) serverPort=80;
		if(!file) file = slash;
			if(debug) 
			{	sprintf(buf3, "%s %s %i\n", host, file, serverPort); 
				Write(debugfd, buf3, strlen(buf3));}

		if(!strcmp(cmd, "CONNECT")) {
			secureTalk(clientfd, client, host, serverPort);
			//return NULL; 
		}
		else 
			if(strcmp(cmd, "GET")) {
			if (debug) printf("%s",cmd);
			//app_error("Not GET or CONNECT");
			return NULL;
		}
			


	// if cmd==GET
	/*==========================Part 1 read request===========*/	
		//printf("%s",cmd);	
		//memset(header, 0, sizeof(header));
		memset(buf1, 0, sizeof(buf1));	
		while(numBytes >0){
			numBytes = Rio_readlineb(&client, buf1, MAXLINE);
			//printf("%s", buf1);    // test the head
			if (!strcmp(buf1,"\r\n")) {
				sprintf(header,"%s%s",header,buf1);
				break;
			}
			if (!strcmp(buf1,"Connection: keep-alive\r\n"))
			{
				sprintf(header,"%s%s",header, "Connection: close\r\n");
				
			}
			else{sprintf(header,"%s%s",header,buf1);}
			memset(buf1, 0, sizeof(buf1));
		}
		memset(buf1, 0, sizeof(buf1));
	/*==========================Part 1 read request  end ===========*/	

	/*==========================Part 2 connect to servers ===========*/	
		serverfd=open_clientfd(host,serverPort);
		if(serverfd == -1){
		  close(clientfd);
		  return NULL;
		}
		Rio_readinitb(&server,serverfd);
	    sprintf(buf1, "GET %s HTTP/1.1\r\n%s\r\n\r\n", \
		file, header);

		printf("buf1:\n%s", buf1);   // test
	/*==========================Part 2 connect to servers end===========*/	

	/*==========================Part 3 send request to server ===========*/		
		Rio_writep(serverfd, buf1, strlen(buf1));
		printf("%s\n","success written" );
		memset(buf1, 0, sizeof(buf1));
	/*==========================Part 4 Read response from Server =========*/	
		
		
		printf("%d\n", serverBytes);
		printf("%s\n",buf2);
		
		while(1){
				if (errno == EINTR) continue;
				serverBytes=rio_readn(serverfd, buf2, MAXLINE);
				if(serverBytes <= 0){
				  close(clientfd);
				  return NULL;
				}
				printf("%s\n",buf2 );
				byteCount += serverBytes;
				if (serverBytes<=0) break;
				rio_writen(clientfd, buf2,serverBytes);	
			}
				
		

		memset(buf1, 0, sizeof(buf1));
		//		Close(clientfd);


		/* you should insert your code for processing connections here */

	        /* code below writes a log entry at the end of processing the connection */

		pthread_mutex_lock(&mutex);
		
		format_log_entry(logString, serverfd, url, byteCount);
		Write(logfd, logString, strlen(logString));
		
		pthread_mutex_unlock(&mutex);
		
		/* 
		When EOF is detected while reading from the server socket,
		send EOF to the client socket by calling shutdown(clientfd,1);
		(and vice versa) 
		*/	

		Close(clientfd);
		Close(serverfd);
	}
	return NULL;
}


void *secureTalk (int clientfd, rio_t client, char* host, int serverPort){
	int numBytes, serverfd;
	int serverbytes;
	int flag=0;
	int byteCount = 0;
	int *args;
	char buf1[MAXLINE], buf2[MAXLINE];
	char *buf3="HTTP/1.1 200 OK\r\n\r\n";
	char url[MAXLINE], logString[MAXLINE];
	pthread_t tid2;
	rio_t server;
	void* ret = (void*) malloc(16);
	
	printf("enter the securetalk%d\n", serverPort);
	
	/*==============    Open connection to server=============*/
	serverfd = open_clientfd(host, serverPort);
	if(serverfd == -1){
	  close(clientfd);
	  return NULL;
	}

	//Rio_readinitb(&server,serverfd);
	printf("success Open\n");
	/*==============    Send HTTP/1.1 200 OK to client=============*/
	Rio_writep(clientfd, buf3, strlen(buf3));
	//Rio_writep(clientfd, "HTTP/1.1 200 OK\r\n\r\n", strlen("HTTP/1.1 200 OK\r\n\r\n"));
	printf("success write to client\n");
	// /*========  Create a thread to listen from server and write to client=============*/
	args = malloc(2*sizeof(int));
	args[0] = clientfd; args[1] = serverfd; 
	//args[2]= flag;
	Pthread_create(&tid2, NULL, listenfromserver, (void*) args);
    Pthread_detach(tid2);
	
	
 //    /*========  Create a thread to listen from client and write to server=============*/

	
	while(1)
	{	
		if (errno==EINTR) continue;
		numBytes= rio_readp(clientfd, buf1, MAXLINE);
		
		if (numBytes <= 0) {
			//Rio_writep(serverfd, "\r\n\r\n", strlen("\r\n\r\n"));
			printf("break the while loop\n");
			break;
			
		}
		Rio_writep(serverfd, buf1, numBytes);
	}
	
	//flag=1;

	pthread_mutex_lock(&mutex);

	format_log_entry(logString, serverfd, url, byteCount);
	Write(logfd, logString, strlen(logString));

	pthread_mutex_unlock(&mutex);
	
	close(serverfd);

	//Close(clientfd);
	//Close(serverfd);
	return NULL;

}

void *listenfromserver(void* args){

	int clientfd,serverfd,flag;
	char buf[MAXLINE];
	int n;
	char url[MAXLINE], logString[MAXLINE];
	int byteCount = 0;

	clientfd = ((int*)args)[0];
	serverfd = ((int*)args)[1];
	//flag=((int*)args)[2];
	free(args);

	while(1)
	{	
		printf("listenfromserver works\n" );
		if (errno==EINTR) continue;
		n= rio_readp(serverfd, buf, MAXLINE);
		printf("n:%d serverfd:%d clinetfd:%d\n ", n, serverfd, clientfd);
		if (n <= 0) {
			//Rio_writep(clientfd, "\r\n\r\n", strlen("\r\n\r\n"));
			printf("break the SECOND while loop\n");
			break;
			
		}
		Rio_writep(clientfd, buf, n);
	}
	//flag=1;
	close(clientfd);

	//Close(clientfd);
	//Close(serverfd);
	return NULL;

}


void ignore()
{
	;
}


/*============================================================
 * url parser:
 *    find_target_address()
 *        Given a url, copy the target web server address to
 *        target_address and the following path to path.
 *        target_address and path have to be allocated before they 
 *        are passed in and should be long enough (use MAXLINE to be 
 *        safe)
 *
 *        Return the port number. 0 is returned if there is
 *        any error in parsing the url.
 *
 *============================================================*/

/*find_target_address - find the host name from the uri */
int  find_target_address(char * uri, char * target_address, char * path,
                         int  * port)

{
 

    if (strncasecmp(uri, "http://", 7) == 0) {
	char * hostbegin, * hostend, *pathbegin;
	int    len;
       
	/* find the target address */
	hostbegin = uri+7;
	hostend = strpbrk(hostbegin, " :/\r\n");
	if (hostend == NULL){
	  hostend = hostbegin + strlen(hostbegin);
	}
	
	len = hostend - hostbegin;

	strncpy(target_address, hostbegin, len);
	target_address[len] = '\0';

	/* find the port number */
	if (*hostend == ':')   *port = atoi(hostend+1);

	/* find the path */

	pathbegin = strchr(hostbegin, '/');

	if (pathbegin == NULL) {
	  path[0] = '\0';
	  
	}
	else {
	  pathbegin++;	
	  strcpy(path, pathbegin);
	}
	return 0;
    }
    target_address[0] = '\0';
    return -1;
}



/*============================================================
 * log utility
 *    format_log_entry
 *       Copy the formatted log entry to logstring
 *============================================================*/

void format_log_entry(char * logstring, int sock, char * uri, int size)
{
    time_t  now;
    char    buffer[MAXLINE];
    struct  sockaddr_in addr;
    unsigned  long  host;
    unsigned  char a, b, c, d;
    int    len = sizeof(addr);

    now = time(NULL);
    strftime(buffer, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    if (getpeername(sock, (struct sockaddr *) & addr, &len)) {
	unix_error("Can't get peer name");
    }

    host = ntohl(addr.sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;

    sprintf(logstring, "%s: %d.%d.%d.%d %s %d\n", buffer, a,b,c,d, uri, size);
}







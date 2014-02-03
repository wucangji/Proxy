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
void *secureTalk(int clientfd, rio_t client, char* host, char* verzion, int serverPort);
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
  int args[2];
  
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
	*serverPort = atoi(strtok(NULL, ":/"));
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

	clientfd = ((int*)args)[0];
	serverPort = ((int*)args)[1];

	Rio_readinitb(&client, clientfd);
	
	/* Determine whether request is GET or CONNECT */
	numBytes = Rio_readlineb(&client, buf1, MAXLINE);

	cmd = strtok_r(buf1, " \r\n", &saveptr);
	strcpy(url, strtok_r(NULL, " \r\n",&saveptr));
	//sscanf(buf1, "%s %s %s",cmd, url, version);
	//printf("%s\n", buf1);    // for test : it is "GET"
	//printf("%d\n", numBytes);  // for test
	parseAddress(url, host, &file, &serverPort); // ) {
	if(!file) file = slash;
		if(debug) 
		{	sprintf(buf3, "%s %s %i\n", host, file, serverPort); 
			Write(debugfd, buf3, strlen(buf3));}

	if(!strcmp(cmd, "CONNECT")) {
		secureTalk(clientfd, client, host, version, serverPort);
		//return NULL; 
	}
	else 
		if(strcmp(cmd, "GET")) {
		if (debug) printf("%s",cmd);
		//app_error("Not GET or CONNECT");
		return;
	}
		


// if cmd==GET
/*==========================Part 1 read request===========*/	
	//printf("%s",cmd);	
	//memset(header, 0, sizeof(header));
	memset(buf1, 0, sizeof(buf1));	
	while(numBytes >0){
		numBytes = Rio_readlineb(&client, buf1, MAXLINE);
		printf("%s", buf1);    // test the head
		if (!strcmp(buf1,"\r\n")) break;
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
	serverfd=Open_clientfd(host,serverPort);

	Rio_readinitb(&server,serverfd);
    sprintf(buf1, "GET %s HTTP/1.1\r\n%s\r\n\r\n", \
	file, header);

	printf("buf1:\n%s", buf1);   // test
/*==========================Part 2 connect to servers end===========*/	

/*==========================Part 3 send request to server ===========*/		
	Rio_writen(serverfd, buf1, strlen(buf1));
	printf("%s\n","success written" );
	memset(buf1, 0, sizeof(buf1));
/*==========================Part 4 Read response from Server =========*/	
	
	serverBytes=Rio_readnb(&server, buf2, MAXLINE);
	printf("%d\n", serverBytes);
	printf("%s\n",buf2);
	
	while(serverBytes>0){
		Rio_writen(clientfd, buf2, serverBytes);
		serverBytes=Rio_readnb(&server, buf2, MAXLINE);
		printf("%s\n",buf2 );
		//if (!strcmp(buf2,"\r\n")) break;
		byteCount += serverBytes;
			
		memset(buf2, 0, sizeof(buf2));
			
	} 

	byteCount =0;
	memset(buf1, 0, sizeof(buf1));



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
	//return NULL;
}


void *secureTalk (int clientfd, rio_t client, char* host, char* version, int serverPort){
	int numBytes, serverfd;
	int serverbytes;
	int byteCount = 0;
	char buf1[MAXLINE], buf2[MAXLINE];
	char *buf3="HTTP/1.1 200 OK\r\n\r\n";
	char url[MAXLINE], logString[MAXLINE];
	pthread_t tid2, tid3;
	rio_t server;
	
	printf("%d\n", serverPort);
	
	/*==============    Open connection to server=============*/
	serverfd = Open_clientfd(host, serverPort);
	//Rio_readinitb(&server,serverfd);

	/*==============    Send HTTP/1.1 200 OK to client=============*/
	Rio_writep(clientfd, buf3, sizeof(buf3));

	// /*========  Create a thread to listen from server and write to client=============*/
	 Pthread_create(&tid2, NULL, listenfromserver, (void*) args);
 //    Pthread_detach(tid2);
	
	
 //    /*========  Create a thread to listen from client and write to server=============*/

	// Pthread_create(&tid3, NULL, listenfromclient, (void*) args);
 //    Pthread_detach(tid3);



	pthread_mutex_lock(&mutex);

	format_log_entry(logString, serverfd, url, byteCount);
	Write(logfd, logString, strlen(logString));

	pthread_mutex_unlock(&mutex);
	Close(clientfd);
	Close(serverfd);
	//return NULL;

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

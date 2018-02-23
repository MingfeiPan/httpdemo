/* 
  a copy of J. David's webserver 
  httpd, using only as a practice
 */
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdlib.h>

#define ISspace(x) isspace((int)(x))

#define BUFSIZE 4096
#define CONSIZE 20

#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"

int fd_con[CONSIZE];

void* accept_request(void*);
void bad_request(int);
void cat(int, FILE *);
void cannot_execute(int);
void error_die(const char *);
void execute_cgi(int, const char *, const char *, const char *);
int get_line(int, char *, int);
void headers(int);
void not_found(int);
void serve_file(int, const char *);
int startup(u_short *);
void unimplemented(int);
void runmultiprocess(int);
void runmultithread(int);
void waitchild();
int updateMaxfd(fd_set, int);

int updateMaxfd(fd_set fds, int maxfd) {
  int i;
  int new_maxfd = 0;
  for (i = 0; i <= maxfd; i++) {
    if (FD_ISSET(i, &fds) && i > new_maxfd) {
      new_maxfd = i;
    }
  }
  return new_maxfd;
}


/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */
/**********************************************************************/
void* accept_request(void* pclient)
{
 
 int client = *(int*)pclient;
 char buf[1024];
 int numchars;
 char method[255];
 char url[255];
 char path[512];
 size_t i, j;
 struct stat st;
 int cgi = 0;      /* becomes true if server decides this is a CGI
                    * program */
 char *query_string = NULL;

 numchars = get_line(client, buf, sizeof(buf));
 i = 0; j = 0;
 while (!ISspace(buf[j]) && (i < sizeof(method) - 1))
 {
  method[i] = buf[j];
  i++; j++;
 }
 method[i] = '\0';

 if (strcasecmp(method, "GET"))
 {
  unimplemented(client);
  return NULL;
 }

 i = 0;
 while (ISspace(buf[j]) && (j < sizeof(buf)))
  j++;
 while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf)))
 {
  url[i] = buf[j];
  i++; j++;
 }
 url[i] = '\0';
 headers(client);
 send(client, "hello mingfei", 14, 0);
 printf("HTTP/1.0 200 OK ");
 printf("%s\n", url);

 close(client);
 return NULL;
}

/**********************************************************************/
/* Inform the client that a request it has made has a problem.
 * Parameters: client socket */
/**********************************************************************/
void bad_request(int client)
{
 char buf[1024];

 sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
 send(client, buf, sizeof(buf), 0);
 sprintf(buf, "Content-type: text/html\r\n");
 send(client, buf, sizeof(buf), 0);
 sprintf(buf, "\r\n");
 send(client, buf, sizeof(buf), 0);
 sprintf(buf, "<P>Your browser sent a bad request, ");
 send(client, buf, sizeof(buf), 0);
 sprintf(buf, "such as a POST without a Content-Length.\r\n");
 send(client, buf, sizeof(buf), 0);
}

/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
/**********************************************************************/
void cat(int client, FILE *resource)
{
 char buf[1024];

 fgets(buf, sizeof(buf), resource);
 while (!feof(resource))
 {
  send(client, buf, strlen(buf), 0);
  fgets(buf, sizeof(buf), resource);
 }
}

/**********************************************************************/
/* Inform the client that a CGI script could not be executed.
 * Parameter: the client socket descriptor. */
/**********************************************************************/
void cannot_execute(int client)
{
 char buf[1024];

 sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "Content-type: text/html\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
 send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error. */
/**********************************************************************/
void error_die(const char *sc)
{
 perror(sc);
 exit(1);
}

/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */
/**********************************************************************/
int get_line(int sock, char *buf, int size)
{
 int i = 0;
 char c = '\0';
 int n;

 while ((i < size - 1) && (c != '\n'))
 {
  n = recv(sock, &c, 1, 0);
  /* DEBUG printf("%02X\n", c); */
  if (n > 0)
  {
   if (c == '\r')
   {
    n = recv(sock, &c, 1, MSG_PEEK);
    /* DEBUG printf("%02X\n", c); */
    if ((n > 0) && (c == '\n'))
     recv(sock, &c, 1, 0);
    else
     c = '\n';
   }
   buf[i] = c;
   i++;
  }
  else
   c = '\n';
 }
 buf[i] = '\0';
 
 return(i);
}

/**********************************************************************/
/* Return the informational HTTP headers about a file. */
/* Parameters: the socket to print the headers on
 *             the name of the file */
/**********************************************************************/
void headers(int client)
{
 char buf[1024];

 strcpy(buf, "HTTP/1.0 200 OK\r\n");
 send(client, buf, strlen(buf), 0);
 strcpy(buf, SERVER_STRING);
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "Content-Type: text/html\r\n");
 send(client, buf, strlen(buf), 0);
 strcpy(buf, "\r\n");
 send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Give a client a 404 not found status message. */
/**********************************************************************/
void not_found(int client)
{
 char buf[1024];

 sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, SERVER_STRING);
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "Content-Type: text/html\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "your request because the resource specified\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "is unavailable or nonexistent.\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "</BODY></HTML>\r\n");
 send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
/**********************************************************************/
int startup(u_short *port)
{
 int httpd = 0;
 struct sockaddr_in name;
 int one = 1;

 httpd = socket(PF_INET, SOCK_STREAM, 0);
 if (httpd == -1)
  error_die("socket");

 if(setsockopt(httpd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0)
  error_die("socket opt");

 memset(&name, 0, sizeof(name));
 name.sin_family = AF_INET;
 name.sin_port = htons(*port);
 name.sin_addr.s_addr = htonl(INADDR_ANY);
 if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
  error_die("bind");
 if (*port == 0)  /* if dynamically allocating a port */
 {
  socklen_t namelen = sizeof(name);
  if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)
   error_die("getsockname");
  *port = ntohs(name.sin_port);
 }
 if (listen(httpd, 20) < 0)
  error_die("listen");
 return(httpd);
}

/**********************************************************************/
/* Inform the client that the requested web method has not been
 * implemented.
 * Parameter: the client socket */
/**********************************************************************/
void unimplemented(int client)
{
 char buf[1024];

 sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, SERVER_STRING);
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "Content-Type: text/html\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "</TITLE></HEAD>\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "</BODY></HTML>\r\n");
 send(client, buf, strlen(buf), 0);
}

void waitchild()
{
  while(waitpid(-1, NULL, WNOHANG) > 0);
}

void runmultithread(int server_sock)
{
 int client_sock = -1;
 struct sockaddr_in client_name;
 socklen_t client_name_len = sizeof(client_name);
 pthread_t newthread;

 while (1)
 {
  client_sock = accept(server_sock,
                       (struct sockaddr *)&client_name,
                       &client_name_len);
  if (client_sock == -1)
   error_die("accept");
 /* accept_request(client_sock); */
 if (pthread_create(&newthread , NULL, accept_request, (void*)&client_sock) != 0)
   perror("pthread_create");
 }
}

void runmultiprocess(int server_sock)
{
  signal(SIGCHLD, waitchild);
  int client_sock = -1;
  struct sockaddr_in client_name;
  socklen_t client_name_len = sizeof(client_name);
  int pid;

  while(1)
  {
    client_sock = accept(server_sock, (struct sockaddr *)&client_name, &client_name_len);
    if(client_sock == -1)
      error_die("accept");

    if((pid = fork()) < 0)
    {
      perror("fork error");
    }
    else if(pid == 0)
    {
      close(server_sock);
      accept_request((void*)&client_sock);
      close(client_sock);
      exit(0);
    } 
    else
    {
      close(client_sock);
    }
  }

}

void runselect1(int server_sock)
{

  int client_sock = -1;
  char buf[BUFSIZE];
  struct sockaddr_in client_name;
  socklen_t client_name_len = sizeof(client_name);
  int maxfd;
  fd_set fds, allfds;
  int i;
  int ret;
  maxfd = server_sock;
  int currentcon = 0;
  FD_ZERO(&allfds);
  FD_ZERO(&fds);
  FD_SET(server_sock, &allfds);

  while(1)
  {
    
    fds = allfds;
    currentcon = 0;
    for(i = 0; i < CONSIZE; i++)
    {
      if(fd_con[i] != 0){
        FD_SET(fd_con[i], &fds);  
        if (fd_con[i] > maxfd)
        {
          maxfd = fd_con[i];
        }
        fd_con[currentcon++] = fd_con[i];
      }
    }    
    
    if((ret = select(maxfd+1, &fds, NULL, NULL, NULL)) < 0)
      error_die("select error");

    for (i = 0; i < CONSIZE; i++)
    {
      if(FD_ISSET(fd_con[i], &fds))
      {
        if((ret = recv(fd_con[i], buf, sizeof(buf), 0)) < 0)
          error_die("recv error");
        // else if(ret == 0){
        //   fd_con[i] = 0;
        //   FD_CLR(fd_con[i], &fds);   
        //   close(fd_con[i]);   
        // }
        else
        {
          headers(fd_con[i]);
          send(fd_con[i], "hello mingfei", 14, 0);
          printf("HTTP/1.0 200 OK \n");             
          fd_con[i] = 0;
          // FD_CLR(fd_con[i], &fds);   
          if(close(fd_con[i]) < 0)
            error_die("close error");             
        }
      }
    }

    if(FD_ISSET(server_sock, &fds))
    {
      client_sock = accept(server_sock, (struct sockaddr *)&client_name, &client_name_len);
      if(client_sock == -1)
        error_die("accept");

      if(currentcon <= CONSIZE)
      {
        fd_con[currentcon++] = client_sock;
        // FD_SET(client_sock, &allfds);
        printf("new connection client %d, %s : %d\n", currentcon, inet_ntoa(client_name.sin_addr), ntohs(client_name.sin_port));
        if(client_sock > maxfd)
          maxfd = client_sock;
      }
      else
      {
        printf("max connections arrive, exit\n"); 
        close(client_sock);
        exit(1);
      }      
    }

  }
  for (i = 0; i < CONSIZE; i++)
  {
    if(fd_con[i] != 0)
      close(fd_con[i]);
  }
  return;
}

void runselect(int server_sock)
{

  int client_sock = -1;
  char buf[BUFSIZE];
  struct sockaddr_in client_name;
  socklen_t client_name_len = sizeof(client_name);
  int maxfd;
  fd_set fds, allfds;
  int i;
  int ret;
  maxfd = server_sock;
  int currentcon = 0;
  FD_ZERO(&allfds);
  FD_ZERO(&fds);
  FD_SET(server_sock, &allfds);

  while(1)
  {
    
    fds = allfds;   
    maxfd = updateMaxfd(fds, maxfd);
    if((ret = select(maxfd+1, &fds, NULL, NULL, NULL)) < 0)
      error_die("select error");

    for(i = 0; i <= maxfd; i++)
    {
      if(!FD_ISSET(i, &fds))
        continue;
      if(i == server_sock)
      {
        client_sock = accept(server_sock, (struct sockaddr *)&client_name, &client_name_len);
        if(client_sock == -1)
          error_die("accept");
        printf("new connection client %d, %s : %d\n", client_sock, inet_ntoa(client_name.sin_addr), ntohs(client_name.sin_port));
        if(client_sock > maxfd)
          maxfd = client_sock;
        FD_SET(client_sock, &allfds);
      }
      else
      {
        if((ret = recv(i, buf, sizeof(buf), 0)) < 0)
          error_die("recv error");
        else
        {
        headers(i);
        send(i, "hello mingfei", 14, 0);
        printf("HTTP/1.0 200 OK \n");      
        if(close(i) < 0)
          error_die("close error");
        FD_CLR(i, &allfds);         
        }
      }
    }
  }

  return;
}



int main(void)
{
 int server_sock = -1;
 u_short port = 8080;

 server_sock = startup(&port);
 printf("httpd running on port %d\n", port);

 //runmultithread(server_sock);
 runselect(server_sock);
 //runmultiprocess(server_sock);
 close(server_sock);

 return(0);
}

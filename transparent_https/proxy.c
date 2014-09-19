// Author Phinfinity <rndanish@gmail.com>
/* Transparent HTTP Proxy Wrapper */
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY


#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <linux/netfilter_ipv4.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>

#define HTTPSHDRBUFSZ 256  // used for reading/writing HTTPS headers
int VERBOSITY = 0; // Minimal
const char* PROXY_HOST = "10.4.3.204";
const char* PROXY_PORT = "8080";
const char* PORT = "1125";
int BACKLOG = 20;
#define BUFSZ 256

void display_usage() {
  printf("tproxy: Intermediate transparent HTTPS proxy\n");
  printf("Usage : ./tproxy [-h] [-v] -h proxy_host -a proxy_port -p tproxy_port \n");
  printf("Written by Anish Shankar <rndanish@gmail.com> , http://phinfinity.com\n");
  printf("Before running this add iptables rules to intercept packets and forward them to this server\n");
  //printf("Usage : ./tproxy [-h] [-v] -h proxy_host -a proxy_port -p tproxy_port -b buffer_size\n");
  printf("\t -h/-?\t-  Print help and this usage information\n");
  printf("\t -v\t- Verbosity. Specify once to display error messages, twice to display all connection information\n");
  printf("\t -s\t- Proxy Host. hostname of secondary proxy server to use. defaults to : %s\n", PROXY_HOST);
  printf("\t -a\t- Proxy Port. Port number of proxy service. defaults to : %s\n", PROXY_PORT);
  printf("\t -p\t- Tproxy Port. Port that tproxy should run intermedeate proxy on. defaults to : %s\n", PORT);
  //  printf("\t -b\t- Buffer Size. Buffer Size used to transfer packets. defaults to : %d\n", BUFSZ);
  exit(EXIT_FAILURE);
}

static const char *optstring = "p:s:a:vh?";

void parse_commandline(int argc, char **argv) {
  int opt = getopt(argc, argv, optstring);
  while (opt != -1) {
    switch(opt) {
      case 'p':
	PORT = optarg;
	break;
      case 's':
	PROXY_HOST = optarg;
	break;
      case 'a':
	PROXY_PORT = optarg;
	break;
      case 'v':
	VERBOSITY++;
	break;
      case 'h':
      case '?':
	display_usage();
	break;
      default:
	break;
    }
    opt = getopt(argc, argv, optstring);
  }
}

struct addrinfo hints, *proxy_servinfo;



// Get address in human readable IPV4/IPV6
// Returns port (-1 on error) and IP through dst
int get_addr_name(struct sockaddr *sa, char *dst, socklen_t dst_size) {
  void* addr;  // Will be in_addr or in_addr6
  int port;
  if (sa->sa_family == AF_INET) {
    struct sockaddr_in* sav4 = (struct sockaddr_in*) sa;
    addr = &(sav4->sin_addr);
    port = ntohs(sav4->sin_port);
  } else {
    struct sockaddr_in6* sav6 = (struct sockaddr_in6*) sa;
    addr = &(sav6->sin6_addr);
    port = ntohs(sav6->sin6_port);
  }
  if (!inet_ntop(sa->sa_family, addr, dst, dst_size))
    return -1;
  return port;
}
// Returns port (-1 on error), Human readable IP through dst
int get_original_dst(int fd, char *dst, socklen_t dst_size) {
  struct sockaddr_storage addr;
  socklen_t addr_len = sizeof(addr);
  if (getsockopt(fd, SOL_IP, SO_ORIGINAL_DST, (struct sockaddr*) &addr,  &addr_len) == -1)
    return -1;
  int port = get_addr_name((struct sockaddr *)&addr, dst, dst_size);
  if (port == -1)
    return -1;
  // For some reason ^ above doesn't give correct port chk later
  return port;
}
// Returns port (-1 on error), Human readable IP through dst
int get_peername(int fd, char *dst, socklen_t dst_size) {
  struct sockaddr_storage addr;
  socklen_t addr_len = sizeof(addr);
  if (getpeername(fd, (struct sockaddr*)&addr, &addr_len) == -1)
    return -1;
  return get_addr_name((struct sockaddr*)&addr, dst, dst_size);
}

/* arg is a pointer to an int array of size 2
 * a[0], a[1] give sockfd's for input and output
 */
void *pipe_data(void *arg) {
  int from_sock = ((int*)arg)[0];
  int to_sock = ((int*)arg)[1];
  int l;
  //char *buf = (char*) malloc(BUFSZ);
  char buf[BUFSZ];
  while ((l = recv(from_sock, buf, sizeof(buf), 0)) > 0)
    if (send(to_sock, buf, l, 0) == -1)
      break;
  //free(buf);
  return NULL;
}

// Returns 0 on success, -1 on failure. Prints errors
int wrap_https_connection(int proxy_fd, const char *dst_host, int dst_port) {
  char buf[HTTPSHDRBUFSZ];
  snprintf(buf, sizeof(buf),
      "CONNECT %s:%d HTTP/1.1\r\nHost: %s\r\n\r\n",
      dst_host, dst_port, dst_host);
  if (send(proxy_fd, buf, strlen(buf), 0) == -1) {
    if (VERBOSITY)
      printf("Error writing to proxy connection");
    return -1;
  }
  recv(proxy_fd, buf, sizeof(buf), 0);
  if (strncmp(buf, "HTTP/1.0 200", 12) != 0) {
    buf[sizeof(buf) - 1] = '\0';
    if (VERBOSITY)
      printf("HTTPS wraping failed : %s\n", buf);
    return -1;
  }
  return 0;
}

void* handle_connection(void* sock_arg) {
  char peer_name[INET6_ADDRSTRLEN];
  char dst_host[INET6_ADDRSTRLEN];
  int peer_port;
  int dst_port;

  int sock = (long)(sock_arg);

  peer_port = get_peername(sock, peer_name, sizeof(peer_name));
  if (peer_port == -1) {
    if (VERBOSITY)
      perror("Cannot find client IP!");
    close(sock);
    return NULL;
  }

  dst_port = get_original_dst(sock, dst_host, sizeof(dst_host));
  if (dst_port == -1) {
    if (VERBOSITY)
      perror("Cannot find original destination");
    close(sock);
    return NULL;
  }
  if (VERBOSITY > 1)
    printf("Connection from %s:%d for %s:%d\n", peer_name, peer_port, dst_host, dst_port);

  // Proxy Socket
  int psock = socket(proxy_servinfo->ai_family, proxy_servinfo->ai_socktype,
      proxy_servinfo->ai_protocol);
  if (connect(psock, proxy_servinfo->ai_addr, proxy_servinfo->ai_addrlen) != 0) {
    if (VERBOSITY)
      perror("Cannot connect to proxy server");
    close(sock);
    return NULL;
  }
  if (wrap_https_connection(psock, dst_host, dst_port) == -1) {
    close(psock);
    close(sock);
    return NULL;
  }
  pthread_t t1, t2;
  int a[2][2]={
    {sock, psock},
    {psock, sock}
  };
  pthread_create(&t1, NULL, pipe_data, (void*)(&a[0]));
  pthread_create(&t2, NULL, pipe_data, (void*)(&a[1]));
  pthread_join(t1, NULL);
  pthread_join(t2, NULL);
  close(psock);
  close(sock);
  if (VERBOSITY > 1)
    printf("Closed connection from %s:%d to %s:%d\n", peer_name, peer_port, dst_host, dst_port);
  return NULL;
}

void init() {
  signal(SIGPIPE, SIG_IGN);  // We'll try to manually check write's return
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  if (getaddrinfo(PROXY_HOST, PROXY_PORT, &hints, &proxy_servinfo) != 0) {
    fprintf(stderr, "Can't resolve proxy IP\n");
    exit(1);
  }
}

int main(int argc, char **argv) {
  printf("This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY\n");
  init();
  parse_commandline(argc, argv);
  int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr;  // connector's address information
  socklen_t sin_size;
  int yes = 1;
  int rv;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;  // use my IP

  if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // loop through all the results and bind to the first we can
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype,
	    p->ai_protocol)) == -1) {
      perror("tproxy failed to create socket");
      continue;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
	  sizeof(int)) == -1) {
      perror("tproxy failed to setsockopt");
      exit(1);
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("tproxy failed to bind");
      continue;
    }

    break;
  }

  if (p == NULL)  {
    fprintf(stderr, "server: failed to bind\n");
    return 2;
  }

  freeaddrinfo(servinfo);  // all done with this structure

  if (listen(sockfd, BACKLOG) == -1) {
    perror("listen");
    exit(1);
  }

  printf("Listening on port : %s , secondary proxy : %s:%s\n", PORT, PROXY_HOST, PROXY_PORT);
  printf("server: waiting for connections...\n");

  while (1) {  // main accept() loop
    sin_size = sizeof their_addr;
    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
    if (new_fd == -1) {
      if (VERBOSITY)
	perror("accept");
      continue;
    }
    pthread_t tmp_thread;
    pthread_create(&tmp_thread, NULL, handle_connection, (void*)(long)(new_fd));
  }

  return 0;
}


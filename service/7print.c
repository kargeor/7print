#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <arpa/inet.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/in.h>



#define PORT 7770
#define BUF_SIZE 2048
#define MAX_CLIENTS 20

#define TRY(CMD, ERR) { if((CMD) == -1) { perror(ERR); exit(1); } }

static ssize_t writeX(int fd, const void* buf, size_t size) {
  ssize_t ret;

  do {
    ret = write(fd, buf, size);
  } while ((ret == -1) && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK));

  TRY(ret, "writeX");
  return ret;
}

static ssize_t readX(int fd, void* buf, size_t size) {
  ssize_t ret;

  do {
    ret = read(fd, buf, size);
  } while ((ret == -1) && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK));

  TRY(ret, "readX");
  return ret;
}

void tcpService(void) {
  int opt = 1;
  int listenSocket,
      addrlen,
      newSocket,
      clientSocket[MAX_CLIENTS],
      activity,
      i,
      valread,
      sd;
  int max_sd;
  struct sockaddr_in address;

  char buffer[BUF_SIZE];
  fd_set readfds;
  char *message = "Seven Print!!!\r\n";

  for (i = 0; i < MAX_CLIENTS; i++) {
    clientSocket[i] = 0;
  }

  TRY(listenSocket = socket(AF_INET , SOCK_STREAM , 0), "socket");
  TRY(setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)), "setsockopt");

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  TRY(bind(listenSocket, (struct sockaddr *)&address, sizeof(address)), "bind");
  TRY(listen(listenSocket, MAX_CLIENTS), "listen");

  addrlen = sizeof(address);

  while(1) {
    FD_ZERO(&readfds);
    FD_SET(listenSocket, &readfds);
    max_sd = listenSocket;

    for (i = 0 ; i < MAX_CLIENTS; i++) {
      sd = clientSocket[i];
      if(sd > 0) FD_SET(sd , &readfds);
      if(sd > max_sd) max_sd = sd;
    }

    activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

    if ((activity < 0) && (errno != EINTR)) printf("select error");

    if (FD_ISSET(listenSocket, &readfds)) {
      TRY(newSocket = accept(listenSocket, (struct sockaddr *)&address, (socklen_t*)&addrlen), "accept");

      printf("** New connection [%d] %s:%d\n", newSocket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

      writeX(newSocket, message, strlen(message));

      for (i = 0; i < MAX_CLIENTS; i++) {
        if(clientSocket[i] == 0) {
          clientSocket[i] = newSocket;
          break;
        }
      }
    }

    for (i = 0; i < MAX_CLIENTS; i++) {
      sd = clientSocket[i];

      if (FD_ISSET(sd , &readfds)) {
        if ((valread = readX(sd, buffer, 1024)) == 0) {
          // disconnect
          printf("Disconnected [%d]\n", sd);
          close(sd);
          clientSocket[i] = 0;
        } else {
          writeX(sd, buffer, valread);
        }
      }
    }
  }

  exit(1);
}

void serialService(void) {
  // TODO
}

int main(int argc, char *argv[]) {
  pid_t tcp, serial;

  TRY(tcp = fork(), "fork");
  if (tcp == 0) {
    // Child
    tcpService();
    exit(0);
  }

  TRY(serial = fork(), "fork");
  if (serial == 0) {
    // Child
    serialService();
    exit(0);
  }

  int stat;
  wait(&stat);
  wait(&stat);

  return 0;
}

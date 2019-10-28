#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <termios.h>

#include <arpa/inet.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <netinet/in.h>

#define PROTO_VERSION 1

#define PORT 7770
#define MAX_CLIENTS 20

//
typedef uint8_t FILENAME[64];

typedef struct {
  uint16_t target;  // x10 deg C
  uint16_t current; // x10 deg C
} TEMPERATURE; // 4 bytes

typedef enum {
  SERVER_IDLE     = 1, // no serial connection
  SERVER_READY    = 2, // ready for command
  SERVER_BUSY     = 3,
  SERVER_PRINTING = 4,
  SERVER_ERROR    = 5
} SERVER_STATE_ENUM;

typedef struct {
  // 0
  uint8_t magic[4];    // 4
  uint16_t version;    // 6
  uint16_t reserved;   // 8

  TEMPERATURE bed;     // 12
  TEMPERATURE extr;    // 16

  FILENAME currentFile; // 80
  uint32_t bytesSent;   // 84
  uint32_t bytesLeft;   // 88
  uint32_t zposSent;    // 92 [x100 mm]
  uint32_t zposLeft;    // 96 [x100 mm]

  uint32_t state;       // 100
} SERVER_STATE;
//
typedef enum {
  CMD_PRINT_FILE   = 1,
  CMD_RUN_GCODE    = 2,
  CMD_PAUSE_PRINT  = 3,
  CMD_CANCEL_PRINT = 4
} COMMAND_ENUM;

typedef union {
  FILENAME filename[2];
  uint8_t gCode[128];
} BIG_STRING; // 128 bytes

typedef struct {
  // 0
  uint8_t magic[4];    // 4
  uint16_t version;    // 6
  uint16_t reserved;   // 8

  uint32_t commandId;  // 12

  BIG_STRING s;        // 140
} COMMAND;
//

static_assert(sizeof(SERVER_STATE) == 100, "SERVER_STATE size");
static_assert(sizeof(COMMAND) == 140, "COMMAND size");

SERVER_STATE serverState;
COMMAND cmd;

uint8_t serverStateBuffer[sizeof(SERVER_STATE)];
uint32_t serverStateBufferPos = 0;

uint8_t cmdBuffer[MAX_CLIENTS][sizeof(COMMAND)];
uint32_t cmdBufferPos[MAX_CLIENTS];

#define TRY(CMD, ERR) { if((CMD) == -1) { perror(ERR); exit(1); } }

static ssize_t writeX(int fd, const void* buf, size_t size) {
  ssize_t ret;

  do {
    ret = write(fd, buf, size);
  } while ((ret == -1) && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK));

  TRY(ret, "writeX");

  if (ret != size) {
    printf("WARN: write() return %zd of %zu\n", ret, size);
  }

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

// 0 : nothing
// 1 : disconnect
// 2 : completed
static uint8_t readToBuffer(int fd, uint8_t *buf, uint32_t *bufPos, uint32_t size) {
  uint32_t remaining = size - (*bufPos);
  ssize_t r = readX(fd, &(buf[*bufPos]), remaining);

  if (r == 0) return 1; // end-of-stream
  if (r == remaining) {
    *bufPos = 0;
    return 2;
  }

  *bufPos = (*bufPos) + r;
  return 0;
}

static uint8_t validateCommand(COMMAND *c) {
  if (c->magic[0] != '7') return 0;
  if (c->magic[0] != 'P') return 0;
  if (c->magic[0] != 'R') return 0;
  if (c->magic[0] != 'N') return 0;
  if (c->version != PROTO_VERSION) return 0;

  return 1;
}

void tcpService(int pipeRead, int pipeWrite) {
  int opt = 1;
  int listenSocket,
      addrlen,
      newSocket,
      clientSocket[MAX_CLIENTS],
      activity,
      i,
      sd;
  int max_sd;
  struct sockaddr_in address;
  fd_set readfds;

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

    //
    FD_SET(pipeRead, &readfds);
    if(pipeRead > max_sd) max_sd = pipeRead;
    //

    activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

    if ((activity < 0) && (errno != EINTR)) printf("select error");

    if (FD_ISSET(listenSocket, &readfds)) {
      TRY(newSocket = accept(listenSocket, (struct sockaddr *)&address, (socklen_t*)&addrlen), "accept");

      printf("New [%d] %s:%d\n", newSocket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

      writeX(newSocket, &serverState, sizeof(SERVER_STATE));

      for (i = 0; i < MAX_CLIENTS; i++) {
        if(clientSocket[i] == 0) {
          clientSocket[i] = newSocket;
          cmdBufferPos[i] = 0;
          break;
        }
      }
      // TODO: Close/Report if not available place in clientSocket[]
    }

    for (i = 0; i < MAX_CLIENTS; i++) {
      sd = clientSocket[i];

      if (FD_ISSET(sd, &readfds)) {
        uint8_t res = readToBuffer(sd, cmdBuffer[i], &(cmdBufferPos[i]), sizeof(COMMAND));
        if (res == 1) {
          // disconnect
          printf("Disconnected [%d]\n", sd);
          close(sd);
          clientSocket[i] = 0;
        } else if (res == 2) {
          // client sent command
          printf("Command from [%d]\n", sd);
          if (validateCommand((COMMAND*)cmdBuffer[i])) {
            // write to pipe
            writeX(pipeWrite, cmdBuffer[i], sizeof(COMMAND));
            printf("Command Sent\n");
          } else {
            // handle invalid
          }
        }
      }
    }

    if (FD_ISSET(pipeRead , &readfds)) {
      uint8_t res = readToBuffer(pipeRead, serverStateBuffer, &serverStateBufferPos, sizeof(SERVER_STATE));
      if (res == 2) {
        memcpy(&serverState, serverStateBuffer, sizeof(SERVER_STATE));
        // Send to all clients
        for (i = 0; i < MAX_CLIENTS; i++) {
          if (clientSocket[i]) {
            writeX(clientSocket[i], &serverState, sizeof(SERVER_STATE));
          }
        }
      }
    }
  }

  exit(1);
}

//

/*
  B0
  B50
  B75
  B110
  B134
  B150
  B200
  B300
  B600
  B1200
  B1800
  B2400
  B4800
  B9600
  B19200
  B38400
  B57600
  B115200
  B230400
  // https://code.woboq.org/userspace/glibc/bits/termios.h.html
*/

// TODO: Read portname+baudSpeed from config file
static char *portname = "/dev/ttyACM0";
static int baudSpeed = B115200;
static int serialFd = 0;

static void openSerial(void) {
  TRY(serialFd = open(portname, O_RDWR | O_NOCTTY | O_SYNC), "serial open");

  struct termios tty;
  memset(&tty, 0, sizeof(tty));
  TRY(tcgetattr(serialFd, &tty), "tcgetattr");

  TRY(cfsetospeed(&tty, baudSpeed), "cfsetospeed");
  TRY(cfsetispeed(&tty, baudSpeed), "cfsetispeed");

  // 8-bit
  tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
  // // disable break processing
  tty.c_iflag &= ~IGNBRK;
  tty.c_lflag = 0;
  tty.c_oflag = 0;
  // MIN == 0, TIME == 0 (polling read)
  tty.c_cc[VMIN]  = 0;
  tty.c_cc[VTIME] = 0;
  // no xon/xoff
  tty.c_iflag &= ~(IXON | IXOFF | IXANY);
  // Ignore modem, Enable receiver
  tty.c_cflag |= (CLOCAL | CREAD);
  // no parity
  tty.c_cflag &= ~(PARENB | PARODD);
  // no stop bits
  tty.c_cflag &= ~CSTOPB;
  // no flow control
  tty.c_cflag &= ~CRTSCTS;

  TRY(tcsetattr(serialFd, TCSANOW, &tty), "tcsetattr");
}

void setHighPriority(void) {
  // TODO: Set process priority and io to highest
  /// http://man7.org/linux/man-pages/man2/ioprio_set.2.html
  /// https://github.com/karelzak/util-linux/blob/master/schedutils/ionice.c
  /// https://linux.die.net/man/1/ionice
  /// https://linux.die.net/man/2/setpriority
  /// https://linux.die.net/man/2/getrlimit
  /// https://unix.stackexchange.com/questions/44334/is-there-any-use-for-rlimit-nice
}

void sendState(int pipeWrite) {
  writeX(pipeWrite, &serverState, sizeof(SERVER_STATE));
}

void serialService(int pipeRead, int pipeWrite) {
  setHighPriority();
  openSerial();

  while (1) {
    sendState(pipeWrite);
    serverState.state++;

    writeX(serialFd, "M117 TEST\n", 10);

    sleep(2);
  }
}

//

#define PIPE_READ 0
#define PIPE_WRITE 1

int main(int argc, char *argv[]) {
  pid_t tcp, serial;
  int serial_to_tcp[2];
  int tcp_to_serial[2];

  TRY(pipe(tcp_to_serial), "pipe");
  TRY(pipe(serial_to_tcp), "pipe");

  memset(&serverState, 'S', sizeof(SERVER_STATE));
  memset(&cmd, 'C', sizeof(COMMAND));

  serverState.magic[0] = cmd.magic[0] = '7';
  serverState.magic[1] = cmd.magic[1] = 'P';
  serverState.magic[2] = cmd.magic[2] = 'R';
  serverState.magic[3] = cmd.magic[3] = 'N';
  serverState.version = cmd.version = PROTO_VERSION;
  serverState.reserved = cmd.reserved = 0;

  TRY(tcp = fork(), "fork");
  if (tcp == 0) {
    // Child
    close(tcp_to_serial[PIPE_READ]);
    close(serial_to_tcp[PIPE_WRITE]);

    tcpService(serial_to_tcp[PIPE_READ], tcp_to_serial[PIPE_WRITE]);
    exit(0);
  }

  TRY(serial = fork(), "fork");
  if (serial == 0) {
    // Child
    close(tcp_to_serial[PIPE_WRITE]);
    close(serial_to_tcp[PIPE_READ]);

    serialService(tcp_to_serial[PIPE_READ], serial_to_tcp[PIPE_WRITE]);
    exit(0);
  }

  int stat;
  wait(&stat);
  wait(&stat);

  return 0;
}

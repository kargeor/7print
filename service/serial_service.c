#include "common.h"


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

// Queue management
static int maxQueueCommands = 3;
static int currentQueueCommands = 0;

// return true if we should queue the command
static int commandForQueue(void) {
  return 1;
}

// removes comments and trailing spaces
static void trimCommand(void) {
  //
}

static void openSerial(void) {
  TRY(serialFd = open(portname, O_RDWR | O_NOCTTY | O_SYNC), "serial open");
  return; // TODO: fix this later...

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

static void setHighPriority(void) {
  // TODO: Set process priority and io to highest
  /// http://man7.org/linux/man-pages/man2/ioprio_set.2.html
  /// https://github.com/karelzak/util-linux/blob/master/schedutils/ionice.c
  /// https://linux.die.net/man/1/ionice
  /// https://linux.die.net/man/2/setpriority
  /// https://linux.die.net/man/2/getrlimit
  /// https://unix.stackexchange.com/questions/44334/is-there-any-use-for-rlimit-nice
}

static void sendState(int pipeWrite) {
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

void serialTestSendGcode(FILE *gcodeDebugFile) {
  char line[1024];
  char response[1024];
  int responsePos = 0;
  openSerial();

  fd_set readfds;
  FD_ZERO(&readfds);

  while(1) {

    if (currentQueueCommands < maxQueueCommands) {

      if (fgets(line, 1024, gcodeDebugFile) == NULL) {
        return; // END OF FILE
        // TODO: Handle remaining acks
      }

      // find size
      int lineSize = 0;
      while (line[lineSize] != '\0' &&
             line[lineSize] != '\n' &&
             line[lineSize] != '\r' &&
             lineSize < (1024 - 1)) lineSize++;

      line[lineSize] = '\0';
      printf("[%s] %d\n", line, currentQueueCommands);

      line[lineSize] = '\n';
      writeX(serialFd, line, lineSize + 1);
      currentQueueCommands++;
    }

    // serial read response line
    FD_SET(serialFd, &readfds);
    select(serialFd + 1, &readfds, NULL, NULL, NULL);

    if (FD_ISSET(serialFd, &readfds)) {
      responsePos += readX(serialFd, &(response[responsePos]), 1024 - responsePos);
      printf("responsePos = %d\n", responsePos);

      for (int i = 0; i < responsePos; i++) {
        if (response[i] == '\n') {
          // line found response[0..i]
          response[i] = '\0';
          printf("{%s}\n", response);
          currentQueueCommands--;
          memmove(response, &(response[i+1]), 1024 - (i + 1));
          responsePos = 0;
          break;
        }
      }
    }
  }
}

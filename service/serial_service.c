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

static void openSerial(void) {
  if (args.serialPortOverride) {
    portname = args.serialPort;
    // TODO: read from config
  }

  TRY(serialFd = open(portname, O_RDWR | O_NOCTTY | O_SYNC), "serial open");

  if (args.dontSetSerialConfig) {
    printf_w("Dont-Set-Serial-Config is set\n");
    return;
  }

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

  // Wait for printer to be ready
  // TODO: Find better way
  sleep(5);
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

// gCode Handling

#define G_CODE_BUF_SIZE 1024
static char gCodeLine[G_CODE_BUF_SIZE];
static char responseBuffer[G_CODE_BUF_SIZE];
static char responseLine[G_CODE_BUF_SIZE];
static int responseBufferPos = 0;

// Queue management
static int maxQueueCommands = 4;
static int currentQueueCommands = 0;
static int waitQueueEmpty = 0;

// return true if we should wait and not add more commands
static int isWaitCommand(void) {
  return strncmp(gCodeLine, "M109", 4) == 0
      || strncmp(gCodeLine, "M190", 4) == 0;
}

// removes comments and trailing spaces, return size
static int trimGCodeLine(void) {
  int i = 0;
  while (gCodeLine[i] != '\0' &&
         gCodeLine[i] != '\n' &&
         gCodeLine[i] != '\r' &&
         gCodeLine[i] != ';' &&
         i < (G_CODE_BUF_SIZE - 1)) i++;

  // trim to EOL or comment start
  // remove spaces and tabs
  do {
    gCodeLine[i] = '\0';
    i--;
  } while (i > 0 && (gCodeLine[i] == ' ' || gCodeLine[i] == '\t'));

  return i + 1;
}

// return true if we have next command
static int readNextGCodeLine(void) {
  if (fgets(gCodeLine, G_CODE_BUF_SIZE, args.gcodeDebugFile) == NULL) {
    // EOF
    return 0;
  }

  return 1;
}

static void processResponseLine(void) {
  if (currentQueueCommands > 0) {
    currentQueueCommands--;
  }

  printf_d("Response = [%s] Queue = %d/%d\n", responseLine, currentQueueCommands, maxQueueCommands);
}

static int processResponseBuffer(void) {
  for (int i = 0; i < responseBufferPos; i++) {

    if (responseBuffer[i] == '\r') {
      // to help with debug logs
      responseBuffer[i] = '+';
    } else if (responseBuffer[i] == '\n') {
      responseBuffer[i] = '\0';
      memcpy(responseLine, responseBuffer, i + 1);
      processResponseLine();

      for (int j = i; j < G_CODE_BUF_SIZE - 1; j++) {
        responseBuffer[j - i] = responseBuffer[j + 1];
      }

      responseBufferPos -= i + 1;
      return 1;
    }

  }

  // no more lines
  return 0;
}

void serialTestSendGcode(void) {
  struct timeval timeout = {0, 1000};
  openSerial();

  fd_set readfds;
  FD_ZERO(&readfds);

  while(1) {

    int lineSize = 0;
    waitQueueEmpty = waitQueueEmpty && (currentQueueCommands > 0);

    if (!waitQueueEmpty
        && (currentQueueCommands < maxQueueCommands)
        && readNextGCodeLine()
        && (lineSize = trimGCodeLine())) {

      gCodeLine[lineSize] = '\n';
      writeX(serialFd, gCodeLine, lineSize + 1);
      currentQueueCommands++;

      gCodeLine[lineSize] = '\0';
      printf_d("G-Code = [%s] Queue = %d/%d\n", gCodeLine, currentQueueCommands, maxQueueCommands);

      if (isWaitCommand()) {
        printf_d("Wait for Queue to clear\n");
        waitQueueEmpty = 1;
      }
    }

    // serial read response line
    FD_SET(serialFd, &readfds);
    select(serialFd + 1, &readfds, NULL, NULL, &timeout);

    if (FD_ISSET(serialFd, &readfds)) {
      responseBufferPos += readX(serialFd, &(responseBuffer[responseBufferPos]), G_CODE_BUF_SIZE - responseBufferPos);
      while(processResponseBuffer());
    }
  }
}

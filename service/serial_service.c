#include "common.h"

/*
  https://code.woboq.org/userspace/glibc/bits/termios.h.html

  B0 B50 B75 B110 B134 B150 B200 B300 B600 B1200 B1800 B2400 B4800
  B9600 B19200 B38400 B57600 B115200 B230400
*/

// TODO: Read portname+baudSpeed from config file
static char *portname = "/dev/ttyACM0";
static int baudSpeed = B115200;
static int serialFd = 0;

static int openSerial(void) {
  if (args.serialPortOverride) {
    portname = args.serialPort;
    // TODO: read from config
  }

  serialFd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);

  if (serialFd == -1) {
    return 0; // Failed
  }

  if (args.dontSetSerialConfig) {
    printf_w("Dont-Set-Serial-Config is set\n");
    return 1;
  }

  struct termios tty;
  memset(&tty, 0, sizeof(tty));

  if (tcgetattr(serialFd, &tty) == -1) {
    printf_w("tcgetattr failed\n");
  }

  if (cfsetospeed(&tty, baudSpeed) == -1) {
    printf_w("cfsetospeed failed\n");
  }

  if (cfsetispeed(&tty, baudSpeed) == -1) {
    printf_w("cfsetispeed failed\n");
  }

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

  if (tcsetattr(serialFd, TCSANOW, &tty) == -1) {
    printf_w("tcsetattr failed\n");
  }

  // Wait for printer to be ready
  // TODO: Find better way
  sleep(1);
  return 1;
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

// gCode Handling

#define G_CODE_BUF_SIZE 1024
static char gCodeLine[G_CODE_BUF_SIZE];
static char responseBuffer[G_CODE_BUF_SIZE];
static char responseLine[G_CODE_BUF_SIZE];
static int responseBufferPos = 0;

// Queue management
// Size = 4 works well
// RX_BUFFER_SIZE=128 on printer side
// TODO: experiment with other sizes
static int maxQueueCommands = 4;
static int currentQueueCommands = 0;
static int waitQueueEmpty = 0;

static FILE *fileBeingPrinted = NULL;

// return true if we should wait and not add more commands
static int isWaitCommand(void) {
  return strncmp(gCodeLine, "M109", 4) == 0   // wait for hotend temp
      || strncmp(gCodeLine, "M190", 4) == 0   // wait for bed temp
      || strncmp(gCodeLine, "G28", 3) == 0    // auto-home
      || strncmp(gCodeLine, "G29", 3) == 0    // bed level
      || strncmp(gCodeLine, "G80", 3) == 0;   // mesh bed leveling (non-standard?)
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
  if (!fileBeingPrinted) {
    return 0;
  }

  if (fgets(gCodeLine, G_CODE_BUF_SIZE, fileBeingPrinted) == NULL) {
    // EOF
    return 0;
  }

  return 1;
}

static void processResponseLine(void) {
  if (responseLine[0] == 'o' && responseLine[1] == 'k') {
    if (currentQueueCommands > 0) {
      currentQueueCommands--;
    }
  } else {
    // TODO: handle other messages
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

static void sendLineToSerialIfNeeded(void) {
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
}

void serialTestSendGcode(void) {
  struct timeval timeout = {0, 1000};

  openSerial();
  fileBeingPrinted = args.gcodeDebugFile;

  fd_set readfds;
  FD_ZERO(&readfds);

  while(1) {

    sendLineToSerialIfNeeded();

    // serial read response line
    FD_SET(serialFd, &readfds);
    select(serialFd + 1, &readfds, NULL, NULL, &timeout);

    if (FD_ISSET(serialFd, &readfds)) {
      responseBufferPos += readX(serialFd, &(responseBuffer[responseBufferPos]), G_CODE_BUF_SIZE - responseBufferPos);
      while(processResponseBuffer());
    }
  }
}

// END gCode Handling

static void serialServiceMainLoop(int pipeRead, int pipeWrite) {
  struct timeval timeout = {0, 1000};

  fd_set readfds;
  FD_ZERO(&readfds);

  while (1) {
    sendLineToSerialIfNeeded();

    // read from serial and/or pipe
    FD_SET(serialFd, &readfds);
    FD_SET(pipeRead, &readfds);
    select(MAX(serialFd, pipeRead) + 1, &readfds, NULL, NULL, &timeout);

    if (FD_ISSET(serialFd, &readfds)) {
      responseBufferPos += readX(serialFd, &(responseBuffer[responseBufferPos]), G_CODE_BUF_SIZE - responseBufferPos);
      while(processResponseBuffer());
    }

    if (FD_ISSET(pipeRead, &readfds)) {
      // incoming command
      printf_d("New incoming command\n");
    }
  }
}

void serialService(int pipeRead, int pipeWrite) {
  setHighPriority();

  //
  serverState.bed.target = 0;
  serverState.bed.current = 0;
  serverState.extr.target = 0;
  serverState.extr.current = 0;

  serverState.currentFile[0] = '\0';
  serverState.bytesSent = 0;
  serverState.bytesRemain = 0;
  serverState.zposSent = 0;
  serverState.zposRemain = 0;
  serverState.timeSpent = 0;
  serverState.timeRemain = 1000;
  //

  if (openSerial()) {
    serverState.state = SERVER_READY;
  } else {
    serverState.state = SERVER_NO_CON;
  }

  sendState(pipeWrite);

  // while (1) {
  //   serverState.timeSpent++;
  //   sendState(pipeWrite);
  //   writeX(serialFd, "M117 TEST\n", 10);
  //   sleep(1);
  // }
  serialServiceMainLoop(pipeRead, pipeWrite);
}

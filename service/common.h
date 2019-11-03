#ifndef SEVEN_PRINT_COMMON_H
#define SEVEN_PRINT_COMMON_H

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


static_assert(sizeof(SERVER_STATE) == 100, "SERVER_STATE size");
static_assert(sizeof(COMMAND) == 140, "COMMAND size");

#define TRY(CMD, ERR) { if((CMD) == -1) { perror(ERR); exit(1); } }


ssize_t writeX(int fd, const void* buf, size_t size);
ssize_t readX(int fd, void* buf, size_t size);
uint8_t readToBuffer(int fd, uint8_t *buf, uint32_t *bufPos, uint32_t size);


void serialService(int pipeRead, int pipeWrite);
void tcpService(int pipeRead, int pipeWrite);
void serialTestSendGcode(void);


extern SERVER_STATE serverState;
extern COMMAND cmd;


typedef struct {
  FILE *gcodeDebugFile;
  int sendGcodeDebug;

  int dontSetSerialConfig;
  int serialPortOverride;
  char *serialPort;
} CMD_ARGS;

extern CMD_ARGS args;


#define printf_d(format, ...) { printf("DEBUG: " format, ##__VA_ARGS__); }
#define printf_w(format, ...) { printf("WARN: " format, ##__VA_ARGS__); }


#endif

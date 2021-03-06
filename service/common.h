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


typedef char FILENAME[64];

typedef struct {
  uint16_t target;  // x10 deg C
  uint16_t current; // x10 deg C
} TEMPERATURE; // 4 bytes

typedef enum {
  SERVER_NO_CON   = 1, // no serial connection
  SERVER_READY    = 2, // ready for command
  SERVER_BUSY     = 3, // busy (not printing from file)
  SERVER_PRINTING = 4, // printing from file
  SERVER_PAUSED   = 5, // printing from file: paused
  SERVER_ERROR    = 6
} SERVER_STATE_ENUM;

typedef struct {
  // 0
  uint8_t magic[4];    // 4
  uint16_t version;    // 6
  uint16_t reserved;   // 8

  TEMPERATURE bed;     // 12
  TEMPERATURE extr;    // 16

  // When printing from file
  FILENAME currentFile; // 80
  uint32_t bytesSent;   // 84
  uint32_t bytesRemain; // 88
  uint32_t zposSent;    // 92 [x100 mm]
  uint32_t zposRemain;  // 96 [x100 mm]
  uint32_t timeSpent;   // 100 [seconds]
  uint32_t timeRemain;  // 104 [seconds]
  uint8_t percentDone;  // 105
  uint8_t reserved1_A;  // 106
  uint8_t reserved1_B;  // 107
  uint8_t reserved1_C;  // 108
  // END: When printing from file

  uint32_t state;       // 112
} SERVER_STATE;


typedef enum {
  CMD_PRINT_FILE   = 1, // print file in s.filename[0]
  CMD_RUN_GCODE    = 2, // run g-code in s.gCode
  CMD_PAUSE_PRINT  = 3, // pause current print
  CMD_CANCEL_PRINT = 4  // cancel current print
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


static_assert(sizeof(SERVER_STATE) == 112, "SERVER_STATE size");
static_assert(sizeof(COMMAND) == 140, "COMMAND size");

#define TRY(CMD, ERR) { if((CMD) == -1) { printf("[ERROR %d]\n", errno); perror(ERR); exit(1); } }


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

typedef struct {
  uint8_t apiKey[64];
  uint8_t serialPort[128];
  uint8_t serialBaud[32];
} USER_CONFIG;

extern CMD_ARGS args;
extern USER_CONFIG config;


#define printf_d(format, ...) { printf("DEBUG: " format, ##__VA_ARGS__); }
#define printf_w(format, ...) { printf("WARN: " format, ##__VA_ARGS__); }

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#endif

#include "common.h"

#define PIPE_READ 0
#define PIPE_WRITE 1

int main(int argc, char *argv[]) {
  pid_t tcp, serial;
  int serial_to_tcp[2];
  int tcp_to_serial[2];

  int c;
  FILE *gcodeDebugFile = NULL;
  int sendGcodeDebug = 0;

  while ((c = getopt(argc, argv, "s:")) != -1) {
    switch(c) {
      case 's':
        // send .gcode to serial
        sendGcodeDebug = 1;
        gcodeDebugFile = fopen(optarg, "r");
        break;

      default:
        printf("ERROR: Command line args\n");
        exit(1);
    }
  }

  if (sendGcodeDebug) {
    printf("Send Gcode Debug mode:\n");
    serialTestSendGcode(gcodeDebugFile);
    fclose(gcodeDebugFile);
    exit(1);
  }

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

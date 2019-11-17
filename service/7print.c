#include "common.h"

#define PIPE_READ 0
#define PIPE_WRITE 1

static void readConfigFile(char *fileName, void *target, size_t sz, int addNullToEnd) {
  printf_d("Reading {%s} Max=[%zu]\n", fileName, sz);

  if (addNullToEnd) {
    sz = sz - 1;
  }

  int fd;
  TRY(fd = open(fileName, O_RDONLY), "open config");
  ssize_t r = readX(fd, target, sz);
  close(fd);

  if (addNullToEnd) {
    ((char*)target)[r] = '\0';
  }

  printf_d("Read %zd bytes\n", r);
}

static void readConfig(void) {
  readConfigFile("../config/api_key", config.apiKey, sizeof(config.apiKey), 0);
  readConfigFile("../config/serial_port_name", config.serialPort, sizeof(config.serialPort), 1);
  readConfigFile("../config/serial_port_baud", config.serialBaud, sizeof(config.serialBaud), 1);
}

int main(int argc, char *argv[]) {
  pid_t tcp, serial;
  int serial_to_tcp[2];
  int tcp_to_serial[2];

  readConfig();

  int c;
  memset(&args, 0, sizeof(CMD_ARGS));
  while ((c = getopt(argc, argv, "s:np:")) != -1) {
    switch(c) {
      case 's':
        // send .gcode to serial
        args.sendGcodeDebug = 1;
        args.gcodeDebugFile = fopen(optarg, "r");
        break;

      case 'n':
        args.dontSetSerialConfig = 1;
        break;

      case 'p':
        args.serialPortOverride = 1;
        args.serialPort = optarg;
        break;

      default:
        printf("ERROR: Command line args\n");
        exit(1);
    }
  }

  if (args.sendGcodeDebug) {
    printf("Send Gcode Debug mode:\n");

    serialTestSendGcode();
    fclose(args.gcodeDebugFile);

    exit(0);
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

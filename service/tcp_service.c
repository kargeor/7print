#include "common.h"

static uint8_t serverStateBuffer[sizeof(SERVER_STATE)];
static uint32_t serverStateBufferPos = 0;

static uint8_t cmdBuffer[MAX_CLIENTS][sizeof(COMMAND)];
static uint32_t cmdBufferPos[MAX_CLIENTS];

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

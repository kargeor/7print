#include "common.h"

SERVER_STATE serverState;
COMMAND cmd;
CMD_ARGS args;

ssize_t writeX(int fd, const void* buf, size_t size) {
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

ssize_t readX(int fd, void* buf, size_t size) {
  ssize_t ret;

  do {
    ret = read(fd, buf, size);
  } while ((ret == -1) && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK));

  if ((ret == -1) && (errno == ECONNRESET)) {
    return 0; // disconnected
  }

  TRY(ret, "readX");
  return ret;
}

// 0 : nothing
// 1 : disconnect
// 2 : completed
uint8_t readToBuffer(int fd, uint8_t *buf, uint32_t *bufPos, uint32_t size) {
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

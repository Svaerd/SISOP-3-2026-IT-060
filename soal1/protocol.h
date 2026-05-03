#ifndef PROTOCOL_H
#define PROTOCOL_H

#define PORT 8080
#define MAX_CLIENTS 100
#define MAX_MSG 1024
#define MAX_NAME 50
#define ADMIN_PASS "protocol7"

// Enumerasi state / tipe paket komunikasi
typedef enum {
  CONNECT_REQ = 0,
  CONNECT_RES_OK = 1,
  CONNECT_RES_FAIL = 2,
  CHAT = 3,
  RPC_AUTH = 4,
  RPC_AUTH_RES_OK = 5,
  RPC_AUTH_RES_FAIL = 6,
  RPC_CMD = 7,
  RPC_RES = 8,
  SYSTEM_MSG = 9
} PacketType;

typedef struct {
  PacketType type;
  char name[MAX_NAME];
  char payload[MAX_MSG];
} Packet;

#endif

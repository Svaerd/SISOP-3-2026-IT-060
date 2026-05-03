#include "protocol.h"
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int sock;
char username[MAX_NAME];

// Menangkap sinyal ^C
void handle_sigint(int sig) {
  printf("\n[system] Disconnecting from The Wired...\n");
  Packet pkt = {CHAT, "", "/exit"};
  strcpy(pkt.name, username);
  send(sock, &pkt, sizeof(Packet), 0);
  close(sock);
  exit(0);
}

// Thread: Listener (Hanya untuk entitas biasa)
void *receive_messages(void *arg) {
  Packet pkt;
  while (recv(sock, &pkt, sizeof(Packet), 0) > 0) {
    if (pkt.type == CHAT) {
      printf("\r[%s]: %s\n> ", pkt.name, pkt.payload);
      fflush(stdout);
    } else if (pkt.type == SYSTEM_MSG) {
      printf("\r\n[System]: %s\n", pkt.payload);
      exit(0);
    }
  }
  pthread_exit(NULL);
}

int main() {
  struct sockaddr_in serv_addr;

  // Bind signal interupsi
  signal(SIGINT, handle_sigint);

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    printf("\n Socket creation error \n");
    return -1;
  }

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(PORT);

  // Asumsi server berjalan di Localhost (sesuaikan jika berjalan di environment
  // lain)
  if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
    printf("\nInvalid address/ Address not supported \n");
    return -1;
  }

  if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    printf("\nConnection Failed \n");
    return -1;
  }

  printf("Enter your name: ");
  fgets(username, MAX_NAME, stdin);
  username[strcspn(username, "\n")] = 0;

  // --- ALUR: THE KNIGHTS (ADMIN RPC) ---
  if (strcmp(username, "The Knights") == 0) {
    char password[MAX_MSG];
    printf("Enter Password: ");
    fgets(password, MAX_MSG, stdin);
    password[strcspn(password, "\n")] = 0;

    Packet auth_pkt = {RPC_AUTH, "The Knights", ""};
    strcpy(auth_pkt.payload, password);
    send(sock, &auth_pkt, sizeof(Packet), 0);

    Packet res;
    recv(sock, &res, sizeof(Packet), 0);

    if (res.type == RPC_AUTH_RES_FAIL) {
      printf("[system] %s\n", res.payload);
      close(sock);
      return 0;
    }

    printf("\n[System] %s\n\n", res.payload);
    char cmd[MAX_MSG];

    // Loop CLI Admin (RPC Command diproses secara sinkron)
    while (1) {
      printf("=== THE KNIGHTS CONSOLE ===\n");
      printf("1. Check Active Entities (Users)\n");
      printf("2. Check Server Uptime\n");
      printf("3. Execute Emergency Shutdown\n");
      printf("4. Disconnect\n");
      printf("Command >> ");

      if (fgets(cmd, MAX_MSG, stdin) == NULL)
        break;
      cmd[strcspn(cmd, "\n")] = 0;

      if (strcmp(cmd, "4") == 0) {
        handle_sigint(0);
      }

      Packet cmd_pkt = {RPC_CMD, "The Knights", ""};
      strcpy(cmd_pkt.payload, cmd);
      send(sock, &cmd_pkt, sizeof(Packet), 0);

      if (strcmp(cmd, "3") == 0) {
        handle_sigint(0); // Exit setelah command mematikan server
      }

      Packet cmd_res;
      recv(sock, &cmd_res, sizeof(Packet), 0);
      printf("\n[Result]: %s\n\n", cmd_res.payload);
    }
  }
  // --- ALUR: PENGGUNA UMUM (CHAT ASINKRON) ---
  else {
    Packet reg_pkt = {CONNECT_REQ, "", ""};
    strcpy(reg_pkt.name, username);
    send(sock, &reg_pkt, sizeof(Packet), 0);

    Packet res;
    recv(sock, &res, sizeof(Packet), 0);

    if (res.type == CONNECT_RES_FAIL) {
      printf("[system] %s\n", res.payload);
      close(sock);
      return 0;
    }

    printf("--- Welcome to The Wired, %s ---\n", username);

    // Deklarasi Thread asinkron karena dilarang menggunakan fork
    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receive_messages, NULL);

    char input[MAX_MSG];
    while (1) {
      printf("> ");
      if (fgets(input, MAX_MSG, stdin) == NULL)
        break;
      input[strcspn(input, "\n")] = 0;

      if (strcmp(input, "/exit") == 0) {
        handle_sigint(0);
      } else if (strlen(input) > 0) {
        Packet chat_pkt = {CHAT, "", ""};
        strcpy(chat_pkt.name, username);
        strcpy(chat_pkt.payload, input);
        send(sock, &chat_pkt, sizeof(Packet), 0);
      }
    }
  }
  return 0;
}

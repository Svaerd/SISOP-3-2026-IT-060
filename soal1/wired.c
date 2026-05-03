#include "protocol.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

typedef struct {
  int socket;
  char name[MAX_NAME];
  int is_admin;
} Client;

Client clients[MAX_CLIENTS];
time_t start_time;

// Fungsi Logging Permanen
void write_log(const char *actor, const char *action) {
  FILE *fp = fopen("history.log", "a");
  if (fp == NULL)
    return;

  time_t rawtime;
  struct tm *timeinfo;
  char time_str[80];

  time(&rawtime);
  timeinfo = localtime(&rawtime);

  // Format wajib: [YYYY-MM-DD HH:MM:SS]
  strftime(time_str, sizeof(time_str), "[%Y-%m-%d %H:%M:%S]", timeinfo);

  fprintf(fp, "%s [%s] [%s]\n", time_str, actor, action);
  fclose(fp);
}

// Fungsi Broadcast (Kecuali pengirim dan admin)
void broadcast(Packet *pkt, int sender_fd) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].socket != 0 && clients[i].socket != sender_fd &&
        clients[i].is_admin == 0) {
      send(clients[i].socket, pkt, sizeof(Packet), 0);
    }
  }
}

int main() {
  int master_socket, new_socket, activity, max_sd, sd;
  struct sockaddr_in address;
  fd_set readfds;

  start_time = time(NULL);
  write_log("System", "SERVER ONLINE");
  printf("[System] The Wired server is active on port %d.\n", PORT);

  // Inisialisasi array klien
  for (int i = 0; i < MAX_CLIENTS; i++) {
    clients[i].socket = 0;
  }

  if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  int opt = 1;
  setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt,
             sizeof(opt));

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  if (bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  if (listen(master_socket, 5) < 0) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  while (1) {
    FD_ZERO(&readfds);
    FD_SET(master_socket, &readfds);
    max_sd = master_socket;

    for (int i = 0; i < MAX_CLIENTS; i++) {
      sd = clients[i].socket;
      if (sd > 0)
        FD_SET(sd, &readfds);
      if (sd > max_sd)
        max_sd = sd;
    }

    activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

    // Koneksi Klien Baru Masuk
    if (FD_ISSET(master_socket, &readfds)) {
      int addrlen = sizeof(address);
      if ((new_socket = accept(master_socket, (struct sockaddr *)&address,
                               (socklen_t *)&addrlen)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
      }

      Packet pkt;
      recv(new_socket, &pkt, sizeof(Packet), 0);

      if (pkt.type == CONNECT_REQ) {
        int duplicate = 0;
        for (int i = 0; i < MAX_CLIENTS; i++) {
          if (clients[i].socket != 0 &&
              strcmp(clients[i].name, pkt.name) == 0) {
            duplicate = 1;
            break;
          }
        }

        if (duplicate) {
          Packet res = {CONNECT_RES_FAIL, "system",
                        "The identity is already synchronized in The Wired."};
          send(new_socket, &res, sizeof(Packet), 0);
          close(new_socket);
        } else {
          for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket == 0) {
              clients[i].socket = new_socket;
              strcpy(clients[i].name, pkt.name);
              clients[i].is_admin = 0;
              break;
            }
          }
          Packet res = {CONNECT_RES_OK, "system", ""};
          send(new_socket, &res, sizeof(Packet), 0);

          char log_msg[100];
          sprintf(log_msg, "User '%s' connected", pkt.name);
          write_log("System", log_msg);
        }
      } else if (pkt.type == RPC_AUTH) { // RPC The Knights
        if (strcmp(pkt.payload, ADMIN_PASS) == 0) {
          for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket == 0) {
              clients[i].socket = new_socket;
              strcpy(clients[i].name, pkt.name);
              clients[i].is_admin = 1;
              break;
            }
          }
          Packet res = {RPC_AUTH_RES_OK, "system",
                        "Authentication Successful. Granted Admin privileges."};
          send(new_socket, &res, sizeof(Packet), 0);

          char log_msg[100];
          sprintf(log_msg, "User '%s' connected", pkt.name);
          write_log("System", log_msg);
        } else {
          Packet res = {RPC_AUTH_RES_FAIL, "system", "Authentication Failed."};
          send(new_socket, &res, sizeof(Packet), 0);
          close(new_socket);
        }
      }
    }

    // Klien Mengirim Pesan / Terputus
    for (int i = 0; i < MAX_CLIENTS; i++) {
      sd = clients[i].socket;
      if (FD_ISSET(sd, &readfds)) {
        Packet pkt;
        int valread = recv(sd, &pkt, sizeof(Packet), 0);

        // Kondisi Putus Koneksi (Force exit atau "/exit")
        if (valread == 0 ||
            (pkt.type == CHAT && strcmp(pkt.payload, "/exit") == 0)) {
          char log_msg[100];
          sprintf(log_msg, "User '%s' disconnected", clients[i].name);
          write_log("System", log_msg);
          close(sd);
          clients[i].socket = 0;
        } else {
          if (pkt.type == CHAT) {
            char log_msg[150];
            sprintf(log_msg, "[%s]: %s", pkt.name, pkt.payload);
            write_log("User", log_msg);
            broadcast(&pkt, sd);
          } else if (pkt.type == RPC_CMD) {
            if (strcmp(pkt.payload, "1") == 0) {
              write_log("Admin", "RPC_GET_USERS");
              int count = 0;
              for (int j = 0; j < MAX_CLIENTS; j++) {
                if (clients[j].socket != 0 && clients[j].is_admin == 0)
                  count++;
              }
              Packet res = {RPC_RES, "system", ""};
              sprintf(res.payload, "Active Entities: %d", count);
              send(sd, &res, sizeof(Packet), 0);
            } else if (strcmp(pkt.payload, "2") == 0) {
              write_log("Admin", "RPC_GET_UPTIME");
              time_t now = time(NULL);
              Packet res = {RPC_RES, "system", ""};
              sprintf(res.payload, "Server Uptime: %ld seconds",
                      now - start_time);
              send(sd, &res, sizeof(Packet), 0);
            } else if (strcmp(pkt.payload, "3") == 0) {
              write_log("Admin", "RPC_SHUTDOWN");
              write_log("System", "EMERGENCY SHUTDOWN INITIATED");

              Packet res = {SYSTEM_MSG, "system", "Server is shutting down..."};
              broadcast(&res, sd);

              for (int j = 0; j < MAX_CLIENTS; j++) {
                if (clients[j].socket != 0)
                  close(clients[j].socket);
              }
              close(master_socket);
              exit(0);
            }
          }
        }
      }
    }
  }
  return 0;
}

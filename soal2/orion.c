#include "arena.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// [TAMBAHAN BARU]: Pelacak sesi aktif
char online_users[100][50];
int online_count = 0;

int is_online(const char *username) {
  for (int i = 0; i < online_count; i++) {
    if (strcmp(online_users[i], username) == 0)
      return 1;
  }
  return 0;
}

void set_offline(const char *username) {
  for (int i = 0; i < online_count; i++) {
    if (strcmp(online_users[i], username) == 0) {
      for (int j = i; j < online_count - 1; j++) {
        strcpy(online_users[j], online_users[j + 1]);
      }
      online_count--;
      break;
    }
  }
}

// Fungsi utilitas untuk membaca/menulis data secara terstruktur
int check_user_exists(const char *username, PlayerData *out_data) {
  FILE *file = fopen(DB_FILE, "rb");
  if (!file)
    return 0;
  PlayerData temp;
  while (fread(&temp, sizeof(PlayerData), 1, file)) {
    if (strcmp(temp.username, username) == 0) {
      if (out_data)
        *out_data = temp;
      fclose(file);
      return 1;
    }
  }
  fclose(file);
  return 0;
}

void save_user(PlayerData *data) {
  FILE *file = fopen(DB_FILE, "ab");
  if (file) {
    fwrite(data, sizeof(PlayerData), 1, file);
    fclose(file);
  }
}

int main() {
  printf("Orion is ready (PID: %d)\n", getpid());

  // 1. Setup Message Queue
  int msgid = msgget(MSG_KEY, IPC_CREAT | 0666);
  if (msgid < 0) {
    perror("msgget");
    exit(1);
  }

  // 2. Setup Shared Memory & Semaphore untuk Arena
  int shmid = shmget(SHM_KEY, sizeof(ArenaState) * MAX_ARENA, IPC_CREAT | 0666);
  ArenaState *arenas = (ArenaState *)shmat(shmid, NULL, 0);

  // Inisialisasi status awal untuk SELURUH arena
  for (int i = 0; i < MAX_ARENA; i++) {
    arenas[i].player_count = 0;
    arenas[i].battle_active = 0;
    sem_init(&arenas[i].mutex, 1, 1);
  }

  AuthMsg msg;
  while (1) {
    // Mendengarkan request dari Eternal (Type 1=Reg, Type 2=Login)
    if (msgrcv(msgid, &msg, sizeof(AuthMsg) - sizeof(long), -2, 0) > 0) {
      AuthMsg response;
      response.msg_type = 3; // Response type
      response.status = 0;

      if (msg.msg_type == 1) { // REGISTER
        if (!check_user_exists(msg.username, NULL)) {
          PlayerData new_user = {0};
          strcpy(new_user.username, msg.username);
          strcpy(new_user.password, msg.password);
          new_user.gold = 150; // Default stat
          new_user.level = 1;
          new_user.xp = 0;
          new_user.max_weapon_dmg = 0;
          save_user(&new_user);
          response.status = 1;
        }
      }

      // ... (Kondisi msg_type == 2 / LOGIN sebelumnya) ...
      else if (msg.msg_type == 2) { // LOGIN
        PlayerData user_data;
        if (check_user_exists(msg.username, &user_data)) {
          if (strcmp(user_data.password, msg.password) == 0) {
            // [TAMBAHAN BARU]: Tolak jika sudah online
            if (is_online(msg.username)) {
              response.status = 0;
            } else {
              strcpy(online_users[online_count++],
                     msg.username); // Tandai online
              response.status = 1;
              response.data = user_data;
            }
          }
        }
      }
      // [TAMBAHAN BARU]: Sinkronisasi data persisten
      else if (msg.msg_type == 4) {
        FILE *file = fopen(DB_FILE, "rb+"); // Mode Read & Update
        if (file) {
          PlayerData temp;
          while (fread(&temp, sizeof(PlayerData), 1, file)) {
            if (strcmp(temp.username, msg.data.username) == 0) {
              fseek(file, -sizeof(PlayerData), SEEK_CUR);     // Mundur 1 block
              fwrite(&msg.data, sizeof(PlayerData), 1, file); // Timpa data lama
              break;
            }
          }
          fclose(file);
        }
        continue; // Fire-and-forget: Tidak perlu membalas ke klien agar cepat
      }
      // [TAMBAHAN BARU]: Tangani sinyal Logout
      else if (msg.msg_type == 5) {
        set_offline(msg.username);
        continue; // Fire-and-forget, tidak perlu balas ke client
      }

      // Kirim balasan ke klien yang meminta (hanya untuk Reg/Login)
      msgsnd(msgid, &response, sizeof(AuthMsg) - sizeof(long), 0);
    }
  }
  return 0;
}

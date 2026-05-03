#include "arena.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

int msgid, shmid;
ArenaState *arenas;   // Kumpulan seluruh arena
ArenaState *my_arena; // Arena spesifik yang akan dipakai player ini
PlayerData my_data;
int is_player_1 = 0;

// Fungsi untuk membaca input keyboard secara non-blocking (raw mode)
char getch() {
  char buf = 0;
  struct termios old = {0};
  fflush(stdout);
  if (tcgetattr(0, &old) < 0)
    perror("tcsetattr()");
  old.c_lflag &= ~ICANON;
  old.c_lflag &= ~ECHO;
  old.c_cc[VMIN] = 1;
  old.c_cc[VTIME] = 0;
  if (tcsetattr(0, TCSANOW, &old) < 0)
    perror("tcsetattr ICANON");
  if (read(0, &buf, 1) < 0)
    perror("read()");
  old.c_lflag |= ICANON;
  old.c_lflag |= ECHO;
  if (tcsetattr(0, TCSADRAIN, &old) < 0)
    perror("tcsetattr ~ICANON");
  return buf;
}

// Fungsi untuk merender banner ASCII art dengan warna
void print_banner() {
  // \033[1;36m adalah kode ANSI untuk warna Cyan Bold
  // \033[0m adalah kode ANSI untuk mereset warna ke default
  printf("\033[1;36m");
  printf("  ___    __ _____ _____ _     ___   ___  ___  \n");
  printf(" | _ )  /  \\_   _|_   _| |   | __| / _ \\| __| \n");
  printf(" | _ \\ / /\\ \\| |   | | | |__ | _| | (_) | _|  \n");
  printf(" |___//_/  \\_\\_|   |_| |____||___| \\___/|_|   \n");
  printf("  ___ _____ ___ ___ ___ ___  _  _             \n");
  printf(" | __|_   _| __| _ \\_ _/ _ \\| \\| |            \n");
  printf(" | _|  | | | _||   /| | (_) | .` |            \n");
  printf(" |___| |_| |___|_|_\\___\\___/|_|\\_|            \n");
  printf("\033[0m\n");
}

// Thread khusus untuk membaca input 'a' dan 'u' saat battle
void *battle_input_thread(void *arg) {
  while (my_arena->battle_active) {
    char c = getch();
    if (c == 'a' || c == 'u') {
      sem_wait(&my_arena->mutex);

      int *my_hp = is_player_1 ? &my_arena->p1_hp : &my_arena->p2_hp;
      int *enemy_hp = is_player_1 ? &my_arena->p2_hp : &my_arena->p1_hp;
      int my_dmg = is_player_1 ? my_arena->p1_dmg : my_arena->p2_dmg;
      char *my_name = is_player_1 ? my_arena->p1_name : my_arena->p2_name;
      int has_weapon =
          is_player_1 ? my_arena->p1_has_weapon : my_arena->p2_has_weapon;

      if (*my_hp > 0 && *enemy_hp > 0) {
        // [TAMBAHAN BARU]: Blokir jika tekan 'u' tanpa senjata
        if (c == 'u' && !has_weapon) {
          // Geser log dan masukkan pesan error
          for (int i = 4; i > 0; i--)
            strcpy(my_arena->battle_logs[i], my_arena->battle_logs[i - 1]);
          sprintf(my_arena->battle_logs[0],
                  "[System] %s failed to cast Ultimate (No Weapon)!", my_name);

          sem_post(&my_arena->mutex);
          continue; // Skip eksekusi damage ke bawah
        }

        int final_dmg = my_dmg;
        if (c == 'u' && has_weapon)
          final_dmg *= 3;

        *enemy_hp -= final_dmg;

        for (int i = 4; i > 0; i--)
          strcpy(my_arena->battle_logs[i], my_arena->battle_logs[i - 1]);
        sprintf(my_arena->battle_logs[0], "[%s] dealt %d damage!", my_name,
                final_dmg);

        if (*enemy_hp <= 0)
          my_arena->battle_active = 0;
      }
      sem_post(&my_arena->mutex);

      // Jeda hanya berlaku jika berhasil menyerang (jika kena continue di atas,
      // tidak ada jeda sleep)
      sleep(1);
    }
  }
  return NULL;
}

// Thread khusus untuk menggerakkan BOT
void *bot_attack_thread(void *arg) {
  while (my_arena->battle_active) {
    sleep(3); // Bot menyerang setiap 3 detik

    sem_wait(&my_arena->mutex);

    // Pastikan battle belum selesai dan bot belum mati sebelum menyerang
    if (!my_arena->battle_active || my_arena->p2_hp <= 0 ||
        my_arena->p1_hp <= 0) {
      sem_post(&my_arena->mutex);
      break;
    }

    int bot_dmg = my_arena->p2_dmg;
    my_arena->p1_hp -= bot_dmg;

    // Update battle logs
    for (int i = 4; i > 0; i--)
      strcpy(my_arena->battle_logs[i], my_arena->battle_logs[i - 1]);
    sprintf(my_arena->battle_logs[0], "[%s] dealt %d damage!",
            my_arena->p2_name, bot_dmg);

    // Cek apakah serangan bot membunuh pemain
    if (my_arena->p1_hp <= 0) {
      my_arena->battle_active = 0;
    }

    sem_post(&my_arena->mutex);
  }
  return NULL;
}

void matchmaking_and_battle() {
  my_arena = NULL;
  is_player_1 = 0;

  // ========================================================
  // FASE 1: Prioritas Utama - Cari arena yang sedang menunggu lawan
  // ========================================================
  for (int i = 0; i < MAX_ARENA; i++) {
    sem_wait(&arenas[i].mutex);

    if (arenas[i].player_count == 1 && arenas[i].battle_active == 0) {
      my_arena = &arenas[i];
      my_arena->player_count = 2;
      is_player_1 = 0;
      strcpy(my_arena->p2_name, my_data.username);
      my_arena->p2_hp = 100 + (my_data.xp / 10);
      my_arena->p2_dmg = 10 + (my_data.xp / 50) + my_data.max_weapon_dmg;
      my_arena->p2_has_weapon = (my_data.max_weapon_dmg > 0);
      my_arena->p2_ready = 1;
      sem_post(&my_arena->mutex);
      break; // Langsung keluar loop karena sudah menemukan lawan
    }
    sem_post(&arenas[i].mutex);
  }

  // ========================================================
  // FASE 2: Jika tidak ada yang menunggu, cari arena kosong
  // ========================================================
  if (my_arena == NULL) {
    for (int i = 0; i < MAX_ARENA; i++) {
      sem_wait(&arenas[i].mutex);

      if (arenas[i].player_count == 0 ||
          (arenas[i].player_count == 2 && arenas[i].battle_active == 0)) {
        my_arena = &arenas[i];
        my_arena->player_count = 1;
        is_player_1 = 1;
        strcpy(my_arena->p1_name, my_data.username);
        my_arena->p1_hp = 100 + (my_data.xp / 10);
        my_arena->p1_dmg = 10 + (my_data.xp / 50) + my_data.max_weapon_dmg;
        my_arena->p1_has_weapon = (my_data.max_weapon_dmg > 0);
        my_arena->p1_ready = 1;
        sem_post(&my_arena->mutex);
        break; // Keluar loop karena sudah membuat room baru
      }
      sem_post(&arenas[i].mutex);
    }
  }

  if (my_arena == NULL) {
    printf("All arenas are currently full! Please try again later.\n");
    sleep(2);
    return;
  }

  // 2. LOGIKA PENANTIAN UNTUK PLAYER 1
  if (is_player_1) {
    int wait_time = 0;
    int opponent_found = 0;

    while (wait_time < 35) {
      // [TAMBAHAN BARU]: Cetak countdown dan timpa baris yang sama
      printf("\rMatchmaking... (Waiting for opponent) [%02d/35s]", wait_time);
      fflush(stdout); // Paksa terminal untuk merender teks tanpa newline (\n)

      sem_wait(&my_arena->mutex);
      if (my_arena->player_count == 2) { // Jika ada Player 2 masuk!
        opponent_found = 1;
        sem_post(&my_arena->mutex);
        break;
      }
      sem_post(&my_arena->mutex);

      sleep(1);
      wait_time++;
    }

    // Tambahkan newline agar teks selanjutnya tidak menimpa teks countdown
    printf("\n");

    if (!opponent_found) { // Panggil BOT
      sem_wait(&my_arena->mutex);
      my_arena->player_count = 2;
      strcpy(my_arena->p2_name, "Wild Beast (BOT)");
      my_arena->p2_hp = 150;
      my_arena->p2_dmg = 15;
      my_arena->p2_has_weapon = 0;
      my_arena->p2_ready = 1;
      sem_post(&my_arena->mutex);
      printf("Found opponent: BOT!\n");
      sleep(1);
    } else {
      printf("Found opponent: %s!\n", my_arena->p2_name);
      sleep(1);
    }
  }

  // 3. MULAI BATTLE SINKRON
  my_arena->battle_active = 1;

  // Thread untuk membaca input keyboard pemain (a / u)
  pthread_t tid_input;
  pthread_create(&tid_input, NULL, battle_input_thread, NULL);

  // [TAMBAHAN BARU]: Thread bot AI yang berjalan otonom
  pthread_t tid_bot;
  if (strcmp(my_arena->p2_name, "Wild Beast (BOT)") == 0) {
    pthread_create(&tid_bot, NULL, bot_attack_thread, NULL);
  }

  while (my_arena->battle_active) {
    system("clear");

    char p1_wpn[5] = "";
    char p2_wpn[5] = "";
    if (my_arena->p1_has_weapon)
      strcpy(p1_wpn, "[W] ");
    if (my_arena->p2_has_weapon)
      strcpy(p2_wpn, "[W] ");

    // Gunakan macro warna untuk pemain dan lawan
    const char *c_p1 = is_player_1 ? COLOR_BLUE : COLOR_RED;
    const char *c_p2 = is_player_1 ? COLOR_RED : COLOR_BLUE;

    // Siapkan string polos tanpa ANSI untuk kalkulasi padding yang presisi
    char p1_info[100], p2_info[100];
    sprintf(p1_info, "%s%s (HP: %d)", p1_wpn, my_arena->p1_name,
            my_arena->p1_hp);
    sprintf(p2_info, "%s%s (HP: %d)", p2_wpn, my_arena->p2_name,
            my_arena->p2_hp);

    // Render Kotak Battle Arena (Inner Width = 38 Karakter Absolut)
    printf(C_BLUE "╭───────────── BATTLE ARENA ─────────────╮\n" C_RESET);

    // Render baris info pemain dan lawan dengan warna dinamis
    printf(C_BLUE "│ " C_RESET "%s%-38s" C_RESET C_BLUE " │\n" C_RESET, c_p1,
           p1_info);
    printf(C_BLUE "│ " C_WHITE "%-38s" C_BLUE " │\n" C_RESET,
           "                   VS");
    printf(C_BLUE "│ " C_RESET "%s%-38s" C_RESET C_BLUE " │\n" C_RESET, c_p2,
           p2_info);

    printf(C_BLUE "├───────────── BATTLE LOGS  ─────────────┤\n" C_RESET);

    for (int i = 0; i < 5; i++) {
      char log_pad[50] = "";
      if (strlen(my_arena->battle_logs[i]) > 0) {
        strcpy(log_pad, my_arena->battle_logs[i]);
      }
      // Log dipastikan dipotong rata 38 karakter agar border kanan tidak hancur
      printf(C_BLUE "│ " C_WHITE "%-38s" C_BLUE " │\n" C_RESET, log_pad);
    }

    printf(C_BLUE "╰────────────────────────────────────────╯\n" C_RESET);

    if (my_data.max_weapon_dmg > 0) {
      printf(C_WHITE
             "\n> Action: Press 'a' to attack, 'u' for ultimate\n" C_RESET);
    } else {
      printf(C_WHITE "\n> Action: Press 'a' to attack\n" C_RESET);
    }

    usleep(100000);
  }

  // 4. PENENTUAN MENANG/KALAH
  int current_hp = is_player_1 ? my_arena->p1_hp : my_arena->p2_hp;
  char *enemy_name = is_player_1 ? my_arena->p2_name : my_arena->p1_name;
  int xp_gain = 0;
  char result_str[10];

  if (current_hp > 0) {
    printf("\n\n>>> YOU WIN! <<<\n");
    my_data.xp += 50;
    my_data.gold += 120;
    xp_gain = 50;
    strcpy(result_str, "WIN");
  } else {
    printf("\n\n>>> YOU LOSE... <<<\n");
    my_data.xp += 15;
    my_data.gold += 30;
    xp_gain = 15;
    strcpy(result_str, "LOSS");
  }

  // Tulis History ke file spesifik user
  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  char time_str[10];
  strftime(time_str, sizeof(time_str), "%H:%M", tm);

  char filename[100];
  sprintf(filename, "history_%s.txt", my_data.username);
  FILE *fp = fopen(filename, "a");
  if (fp) {
    fprintf(fp, "%-10s | %-10s | %-6s | +%d XP\n", time_str, enemy_name,
            result_str, xp_gain);
    fclose(fp);
  }

  // 5. RESET ARENA AGAR BISA DIPAKAI LAGI
  sem_wait(&my_arena->mutex);
  my_arena->player_count = 0;
  for (int i = 0; i < 5; i++)
    strcpy(my_arena->battle_logs[i], "");
  sem_post(&my_arena->mutex);

  sleep(3);
}

// Fungsi untuk mengirim data terbaru ke database Orion
void update_player_data() {
  AuthMsg req = {0};
  req.msg_type = 4;
  req.data = my_data;
  msgsnd(msgid, &req, sizeof(AuthMsg) - sizeof(long), 0);
}

void open_armory() {
  int choice;
  while (1) {
    system("clear");
    printf("=== ARMORY ===\n");
    printf("Gold: %d\n\n", my_data.gold);
    printf("1. Wood Sword      | 100 G  | +5 Dmg\n");
    printf("2. Iron Sword      | 300 G  | +10 Dmg\n");
    printf("3. Steel Axe       | 600 G  | +20 Dmg\n");
    printf("4. Demon Blade     | 1500 G | +30 Dmg\n");
    printf("5. God Slayer      | 5000 G | +100 Dmg\n");
    printf("6. Back\nChoice: ");

    if (scanf("%d", &choice) != 1) {
      int c;
      while ((c = getchar()) != '\n' && c != EOF)
        ;
      continue;
    }

    if (choice == 6)
      break;

    int price = 0, dmg = 0;
    switch (choice) {
    case 1:
      price = 100;
      dmg = 5;
      break;
    case 2:
      price = 300;
      dmg = 10;
      break;
    case 3:
      price = 600;
      dmg = 20;
      break;
    case 4:
      price = 1500;
      dmg = 30;
      break;
    case 5:
      price = 5000;
      dmg = 100;
      break;
    default:
      continue;
    }

    if (my_data.gold >= price) {
      my_data.gold -= price;
      // Otomatis pakai senjata terkuat
      if (dmg > my_data.max_weapon_dmg)
        my_data.max_weapon_dmg = dmg;

      printf("\n[Success] Weapon purchased! Max Weapon Dmg is now +%d\n",
             my_data.max_weapon_dmg);
      update_player_data(); // Amankan data ke database
    } else {
      printf("\n[Failed] Not enough gold!\n");
    }
    sleep(1);
  }
}

void open_history() {
  system("clear");
  printf("=== MATCH HISTORY ===\n");
  printf("Time       | Opponent   | Result | XP\n");

  char filename[100];
  sprintf(filename, "history_%s.txt", my_data.username);

  FILE *f = fopen(filename, "r");
  if (f) {
    char line[256];
    while (fgets(line, sizeof(line), f))
      printf("%s", line);
    fclose(f);
  } else {
    printf("\nNo matches played yet.\n");
  }

  printf("\nPress any key...\n");
  int c;
  while ((c = getchar()) != '\n' && c != EOF)
    ;        // Bersihkan buffer input
  getchar(); // Tahan layar sampai user memencet Enter
}

int main() {
  msgid = msgget(MSG_KEY, 0666);
  shmid = shmget(SHM_KEY, sizeof(ArenaState) * MAX_ARENA, 0666);

  // 1. Cek apakah memori IPC eksis
  if (msgid < 0 || shmid < 0) {
    printf("Orion are you there?\n");
    return 1;
  }

  // 2. Cek apakah Orion benar-benar sedang berjalan menggunakan IPC_STAT
  struct shmid_ds shm_info;
  if (shmctl(shmid, IPC_STAT, &shm_info) == 0) {
    // shm_nattch menyimpan jumlah proses yang terkoneksi ke memori ini
    // Jika 0, berarti Orion sudah mati dan ini hanya memori sisa (zombie)
    if (shm_info.shm_nattch == 0) {
      printf("Orion are you there?\n");
      return 1;
    }
  } else {
    // Gagal mengambil status memori
    printf("Orion are you there?\n");
    return 1;
  }

  // Jika aman, baru pasang (attach) memori ke Eternal
  arenas = (ArenaState *)shmat(shmid, NULL, 0);

  int choice;
  while (1) {
    system("clear");
    print_banner();
    printf("\n1. Register\n2. Login\n3. Exit\nChoice: ");

    // 1. Cek apakah scanf gagal membaca angka
    if (scanf("%d", &choice) != 1) {
      // Bersihkan sisa karakter di buffer stdin agar tidak "bocor" ke input
      // selanjutnya
      int c;
      while ((c = getchar()) != '\n' && c != EOF)
        ;

      printf("\n[Error] Input tidak valid! Harap masukkan angka.\n");
      sleep(1);
      continue; // Ulangi loop menu
    }

    // 2. Cek apakah angka yang dimasukkan di luar batas 1-3
    if (choice < 1 || choice > 3) {
      printf("\n[Error] Pilihan tidak tersedia!\n");
      sleep(1);
      continue; // Ulangi loop menu
    }

    if (choice == 3)
      break;

    AuthMsg req = {0};
    req.msg_type = choice;

    printf("Username: ");
    scanf("%s", req.username);

    printf("Password: ");
    scanf("%s", req.password);

    msgsnd(msgid, &req, sizeof(AuthMsg) - sizeof(long), 0);

    AuthMsg res;
    msgrcv(msgid, &res, sizeof(AuthMsg) - sizeof(long), 3, 0);

    if (choice == 1 && res.status == 1)
      printf("Account created!\n");
    else if (choice == 2 && res.status == 1) {
      my_data = res.data;
      while (1) {
        // Kalkulasi Level
        my_data.level = 1 + (my_data.xp / 100);

        // Deklarasi Macro Warna (Bisa diletakkan di atas bersama deklarasi
        // warna battle)

        system("clear");

        print_banner();

        // Format nilai senjata ke dalam string sementara agar paddingnya
        // presisi
        char wpn_str[30];
        if (my_data.max_weapon_dmg > 0) {
          sprintf(wpn_str, "+%d", my_data.max_weapon_dmg);
        } else {
          strcpy(wpn_str, "None");
        }

        // Kalkulasi Lebar Dalam (Inner Width) = 28 Karakter Absolut
        printf(C_BLUE "╭────────── PROFILE ─────────╮\n" C_RESET);

        // Baris 1: Label Name(7) + Value(10) + Label Lvl(6) + Value(5) = 28
        // Karakter
        printf(C_BLUE "│" C_CYAN " Name: " C_WHITE "%-10s" C_MAGENTA
                      " Lvl: " C_WHITE "%-5d" C_BLUE "│\n" C_RESET,
               my_data.username, my_data.level);

        // Baris 2: Label Gold(7) + Value(10) + Label XP(6) + Value(5) = 28
        // Karakter
        printf(C_BLUE "│" C_YELLOW " Gold: " C_WHITE "%-10d" C_GREEN
                      " XP : " C_WHITE "%-5d" C_BLUE "│\n" C_RESET,
               my_data.gold, my_data.xp);

        // Baris 3: Label Wpn(7) + Value(21) = 28 Karakter
        printf(C_BLUE "│" C_RED " Wpn : " C_WHITE "%-21s" C_BLUE "│\n" C_RESET,
               wpn_str);

        printf(C_BLUE "╰────────────────────────────╯\n" C_RESET);

        // Menu Box (Lebar dalam juga 28 Karakter)
        printf(C_BLUE "╭────────────────────────────╮\n" C_RESET);
        printf(C_BLUE "│" C_WHITE " 1. Battle                  " C_BLUE
                      "│\n" C_RESET);
        printf(C_BLUE "│" C_WHITE " 2. Armory                  " C_BLUE
                      "│\n" C_RESET);
        printf(C_BLUE "│" C_WHITE " 3. History                 " C_BLUE
                      "│\n" C_RESET);
        printf(C_BLUE "│" C_WHITE " 4. Logout                  " C_BLUE
                      "│\n" C_RESET);
        printf(C_BLUE "╰────────────────────────────╯\n" C_RESET);
        printf("\n> Choice: ");

        int menu;
        if (scanf("%d", &menu) != 1) { // Proteksi input ngawur
          int c;
          while ((c = getchar()) != '\n' && c != EOF)
            ;
          continue;
        }

        if (menu == 1)
          matchmaking_and_battle();
        else if (menu == 2)
          open_armory();
        else if (menu == 3)
          open_history();
        else if (menu == 4) {
          // [TAMBAHAN BARU]: Beritahu Orion untuk menghapus sesi kita
          AuthMsg logout_req = {0};
          logout_req.msg_type = 5;
          strcpy(logout_req.username, my_data.username);
          msgsnd(msgid, &logout_req, sizeof(AuthMsg) - sizeof(long), 0);

          break; // Keluar dari loop Profile, kembali ke Banner Eterion
        }
      }
    } else {
      printf("Operation failed!\n");
    }
    sleep(1);
  }
  return 0;
}

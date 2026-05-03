#ifndef ARENA_H
#define ARENA_H
#define SHM_KEY 0x0001234
#define DB_FILE "users.dat"
#define MAX_ARENA 10
#define C_BLUE "\033[0;34m"     // Biru gelap untuk border
#define C_CYAN "\033[1;36m"     // Cyan terang untuk Name
#define C_YELLOW "\033[1;33m"   // Kuning untuk Gold
#define C_GREEN "\033[1;32m"    // Hijau untuk XP
#define C_MAGENTA "\033[1;35m"  // Magenta/Ungu untuk Level
#define C_RED "\033[1;31m"      // Merah untuk Weapon
#define C_WHITE "\033[1;37m"    // Putih untuk Nilai (Values)
#define C_RESET "\033[0m"       // Reset
#define COLOR_BLUE "\033[1;34m" // Biru tebal untuk Player
#define COLOR_RED "\033[1;31m"  // Merah tebal untuk Lawan
#define COLOR_RESET "\033[0m"   // Reset warna ke default terminal

#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/types.h>

#define MSG_KEY 0x0005678
#define SHM_KEY 0x0001234
#define DB_FILE "users.dat"

// Struktur Data Pemain Persisten
typedef struct {
  char username[50];
  char password[50];
  int gold;
  int level;
  int xp;
  int max_weapon_dmg; // 0 jika belum punya
} PlayerData;

// Message Queue untuk Autentikasi
typedef struct {
  long msg_type; // 1: Req Reg, 2: Req Login, 3: Res
  char username[50];
  char password[50];
  int status; // 0: Fail, 1: Success
  PlayerData data;
} AuthMsg;

// Shared Memory untuk Arena Matchmaking & Battle
typedef struct {
  sem_t mutex;
  int player_count; // 0, 1, atau 2

  // Status P1
  char p1_name[50];
  int p1_hp;
  int p1_dmg;
  int p1_ready;
  int p1_has_weapon;

  // Status P2 (Bisa Bot)
  char p2_name[50];
  int p2_hp;
  int p2_dmg;
  int p2_ready;
  int p2_has_weapon;

  // Log pertempuran (Top 5)
  char battle_logs[5][100];
  int log_index;
  int battle_active;
} ArenaState;

#endif


## Struktur Repo
```shell
.
├── soal1
│   ├── navi.c
│   ├── protocol.h
│   └── wired.c
└── soal2
    ├── arena.h
    ├── eternal.c
    ├── Makefile
    └── orion.c

3 directories, 7 files
```

## Soal 1
### Penjelasan
Pada soal 1 ini, terdapat 2 program dan 1 header file yang bekerja untuk membentuk sebuah ruang komunikasi anonim.

`wired.c` merupakan program yang bertindak sebagai ***server*** yang memungkinkan terbentuknya sebuah protokol komunikasi yang mampu menghubungkan berbagai entitas di seluruh dunia ke dalam satu kesadaran kolektif yang disebut **The Wired**.

Mendampingi `wired.c`, terdapat `navi.c`, program yang bertindak sebagai aplikasi **NAVI** (***client***) yang mampu menangani fragmentasi identitas. Terdapat 2 alur utama pada program ini:
   * **Alur Pengguna Umum**: Menggunakan `pthread` untuk membuat *thread listener* (`receive_messages`) agar pengguna dapat menerima dan mengirim pesan secara asinkron tanpa terblokir (*non-blocking chat*).
    * **Alur Admin (Knights)**: Klien dapat masuk sebagai admin dengan argumen khusus, lalu dapat mengirim *command* RPC ke [[select-server.c]]

Selain itu, terdapat juga fungsi *signal handling* (`handle_sigint`) untuk keluar dari ketika menekan `Ctrl+C`.

Selain 2 program tadi, terdapat juga header file, yaitu `protocol.h`. mendefinisikan aturan dan struktur protokol komunikasi. Memuat konstanta, batas-batas, tipe-tipe pesan yang bisa dikirimkan, dan struktur data yang dikirimkan.

#### **1. Koneksi yang stabil pada fase awal**

```c
// [File: navi.c] - Fase inisialisasi koneksi klien ke server
if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
  printf("\n Socket creation error \n");
  return -1;
}

serv_addr.sin_family = AF_INET;
serv_addr.sin_port = htons(PORT);
// Konversi IPv4 dari teks ke format biner
if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
  return -1;
}

// Melakukan koneksi ke server
if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
  printf("\nConnection Failed \n");
  return -1;
}
```

Untuk mendapatkan koneksi yang stabil dan terurut (*reliable*), program menggunakan protokol TCP yang direpresentasikan dengan flag `SOCK_STREAM` saat melakukan pembuatan fungsi `socket()`. Fungsi `connect()` akan memulai skema *Three-way Handshake* milik TCP. Jika server (`wired.c`) belum menjalankan fungsi `listen()` dan `accept()`, maka fungsi `connect()` ini akan mengembalikan nilai `< 0` dan menolak klien masuk.

#### **2. Unit NAVI harus mampu menjalankan dua fungsi secara asinkron**

`navi.c` harus bisa mengetik pesan (mengirim) dan menerima pesan baru di saat yang bersamaan tanpa saling menunggu atau terblokir.

```c
// [File: navi.c] - Di dalam alur Pengguna Umum (Chat Asinkron)
// 1. Pembuatan Thread untuk mendengarkan pesan terus-menerus
pthread_t recv_thread;
pthread_create(&recv_thread, NULL, receive_messages, NULL);

char input[MAX_MSG];
while (1) {
  // 2. Main thread terkunci di sini untuk menunggu ketikan keyboard
  printf("> ");
  if (fgets(input, MAX_MSG, stdin) == NULL) break;
  // ... proses kirim pesan ...
}
```

Karena dilarang menggunakan `fork()`, asinkronitas ini dipecahkan menggunakan **Multithreading** (`pthread`). *Thread* utama (*main thread*) akan berhenti sementara/terblokir pada fungsi `fgets()` saat menunggu pengguna mengetik. Di latar belakang, *thread* kedua (`recv_thread`) yang memuat fungsi `receive_messages` akan terus berputar memantau `recv()`. Jika ada pesan masuk dari server, *thread* ini akan langsung mencetaknya ke layar tanpa mengganggu proses mengetik pengguna.

#### **3. Server pusat The Wired dituntut untuk,**
- memiliki skalabilitas tinggi
- tidak terhambat oleh satu pengguna yang lambat
- Server harus mampu mendeteksi aktivitas dari banyak klien
- membedakan antara permintaan koneksi baru dengan pesan masuk
- menangani diskoneksi klien secara bersih
- mengirimkan output pada client.

```c
// [File: wired.c] - Di dalam while(1) loop utama
FD_ZERO(&readfds);
FD_SET(master_socket, &readfds); // Masukkan pintu masuk utama
max_sd = master_socket;

// Masukkan semua klien yang sedang aktif ke dalam himpunan (set) pantauan
for (int i = 0; i < MAX_CLIENTS; i++) {
  sd = clients[i].socket;
  if (sd > 0) FD_SET(sd, &readfds);
  if (sd > max_sd) max_sd = sd;
}

// Tunggu hingga ada aktivitas di salah satu socket
activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

// Cek apakah ada klien lama yang mengirim pesan atau terputus
for (int i = 0; i < MAX_CLIENTS; i++) {
  sd = clients[i].socket;
  if (FD_ISSET(sd, &readfds)) {
    Packet pkt;
    int valread = recv(sd, &pkt, sizeof(Packet), 0);
    // Jika valread 0, berarti koneksi klien terputus secara sepihak
    if (valread == 0 || (pkt.type == CHAT && strcmp(pkt.payload, "/exit") == 0)) {
       // ... proses penutupan socket yang bersih ...
    }
  }
}
```

Alih-alih membuat puluhan *thread* memori untuk setiap pengguna yang masuk, server menggunakan teknik **I/O Multiplexing** dengan fungsi `select()`. Fungsi ini mengumpulkan semua soket aktif ke dalam `readfds` dan menidurkan server. Server hanya akan bangun persis ketika ada *salah satu* klien yang mengetik sesuatu, atau ada koneksi klien baru di `master_socket`. Hal ini mencegah server menghabiskan waktu siklus CPU untuk mengecek pengguna yang diam (menganggur), sehingga skalabilitasnya tinggi.

#### **4. Identitas digital yang unik untuk setiap entitas yang masuk The Wired**

```c
// [File: wired.c] - Penanganan request login pengguna baru
if (pkt.type == CONNECT_REQ) {
  int duplicate = 0;
  // Cek database internal di memori
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].socket != 0 && strcmp(clients[i].name, pkt.name) == 0) {
      duplicate = 1;
      break;
    }
  }

  if (duplicate) {
    Packet res = {CONNECT_RES_FAIL, "system", "The identity is already synchronized in The Wired."};
    send(new_socket, &res, sizeof(Packet), 0);
    close(new_socket); // Tendang pengguna keluar
  } else {
    // ... simpan data klien ke array dan balas OK ...
  }
}
```

Sistem menjaga integritas unik ini melalui validasi di level server. Saat paket registrasi pertama kali mendarat (`CONNECT_REQ`), server melakukan *looping* komparasi string (`strcmp`) terhadap *array* data struktur klien. Jika nama tersebut sudah tercatat pada klien yang soketnya aktif (`!= 0`), server langsung membalas dengan paket penolakan dan memutus paksa jalur TCP tersebut.

#### **5. Distribusi informasi di dalam The Wired harus bersifat kolektif dan menyeluruh.**

```c
// [File: wired.c] - Fungsi penyebaran pesan
void broadcast(Packet *pkt, int sender_fd) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    // Syarat: socket aktif, BUKAN pengirim asli, dan BUKAN admin
    if (clients[i].socket != 0 && clients[i].socket != sender_fd && clients[i].is_admin == 0) {
      send(clients[i].socket, pkt, sizeof(Packet), 0);
    }
  }
}
```

Untuk menyebarkan informasi secara utuh bak ruang *chat* massal, fungsi `broadcast()` melakukan iterasi ke seluruh basis pengguna terhubung di `MAX_CLIENTS`. Filter pengecualian memblokir pengiriman kembali ke `sender_fd` agar pengguna tidak melihat pesan *chat* pantulan dari dirinya sendiri. Aturan tambahan mengecualikan klien berstatus `is_admin == 1` agar Admin/The Knights di dalam server tidak terganggu oleh obrolan entitas biasa.

#### **6. Menyediakan *Admin Console*** 
untuk entitas pengolaan (The Knights) dengan beberapa fungsi:
1. Check Active Entities (Users)
2. Check Server Uptime
3. Execute Emergency Shutdown
4. Disconnect

```c
// [File: navi.c - Klien] - Tampilan konsol admin
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

	  // Exit jika cmd 4 dipilih
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

// [File: wired.c - Server] - Penanganan RPC dari Admin
if (pkt.type == RPC_CMD) {
  if (strcmp(pkt.payload, "1") == 0) {
    // Cek entitas yang login (Bukan admin)
    int count = 0;
    for (int j = 0; j < MAX_CLIENTS; j++) {
      if (clients[j].socket != 0 && clients[j].is_admin == 0) count++;
    }
    Packet res = {RPC_RES, "system", ""};
    sprintf(res.payload, "Active Entities: %d", count);
    send(sd, &res, sizeof(Packet), 0);
  } else if (strcmp(pkt.payload, "3") == 0) {
    // Matikan server
    Packet res = {SYSTEM_MSG, "system", "Server is shutting down..."};
    broadcast(&res, sd); // Peringatkan ke semua orang
    exit(0); // Matikan daemon server
  }
}
```

Mekanisme ini menggunakan skema *Remote Procedure Call* (RPC). Klien Admin secara sinkron mengirim sandi (`"protocol7"`) ke server untuk mendapatkan *flag* `is_admin`. Saat Admin menekan angka 1/2/3, klien tidak menjalankan fungsinya sendiri, ia hanya membungkus angka itu di dalam paket `RPC_CMD` dan menyuruh *server* untuk melakukan pekerjaannya (seperti menghitung jumlah array klien untuk menu 1, atau menghitung `time()` untuk menu 2). Jika Admin menekan angka 3, *server* menembakkan `SYSTEM_MSG` sebagai peringatan *shutdown* ke semua pengguna sebelum menjalankan `exit(0)`.

#### **7. Logging untuk setiap transmisi di dalam The Wired**

```c
// [File: wired.c]
void write_log(const char *actor, const char *action) {
  FILE *fp = fopen("history.log", "a");
  if (fp == NULL) return;

  time_t rawtime;
  struct tm *timeinfo;
  char time_str[80];

  // Mendapatkan waktu sistem yang real-time
  time(&rawtime);
  timeinfo = localtime(&rawtime);

  // Formatting waktu
  strftime(time_str, sizeof(time_str), "[%Y-%m-%d %H:%M:%S]", timeinfo);

  fprintf(fp, "%s [%s] [%s]\n", time_str, actor, action);
  fclose(fp);
}
```

Fungsi independen ini dipanggil berulang kali di seluruh perulangan *server*. Fungsi ini mengandalkan standar *File I/O* pada bahasa C dengan flag `"a"` (*Append*) untuk memastikan baris riwayat yang ditulis sebelumnya tidak tertimpa/terhapus (*persistent logging*). Di dalamnya, library `<time.h>` digunakan dengan fungsi `strftime` untuk menghasilkan struktur format baku secara rapi yaitu `[Tahun-Bulan-Hari Jam:Menit:Detik]` agar setiap *log* percakapan atau *log error* sistem memiliki *timestamp* yang akurat.

### Output


## Soal 2
### Penjelasan
Struktur repo pada soal 2 ini lumayan mirip dengan struktur pada soal 1, 2 *program file* (`orion.c` sebagai server serta `eternal.c` sebagai client), 1 *header file* (arena.h), bedanya disini juga terdapat `Makefile` untuk mempermudah compiling.

#### **Main Menu**
Menu ini akan bertindak sebagai gerbang masuk ketika *eternal* mulai terhubung dengan *orion*. Disini terdapat 3 menu: *register, login, & exit*.

[PLACE HOLDER Screenshot]

```c
// [File: eternal.c]
// Di dalam int main(), loop menu awal
if (scanf("%d", &choice) != 1) {
  // Bersihkan sisa karakter di buffer stdin agar tidak "bocor"
  int c;
  while ((c = getchar()) != '\n' && c != EOF);
  printf("\n[Error] Input tidak valid! Harap masukkan angka.\n");
  sleep(1);
  continue; // Ulangi loop menu
}

if (choice < 1 || choice > 3) {
  printf("\n[Error] Pilihan tidak tersedia!\n");
  sleep(1);
  continue;
}
```


Program menggunakan `scanf` yang mengembalikan jumlah argumen yang berhasil dibaca. Jika pengguna mengetikkan huruf (misal: "sam") pada input angka, `scanf` akan gagal (mengembalikan nilai selain 1) dan membiarkan huruf tersebut tertinggal di _buffer_. Untuk mencegah _infinite loop_ (error berulang-ulang tanpa henti), program menggunakan `getchar()` untuk membuang karakter sampah tersebut sebelum meminta input ulang.

#### **Entry Rejection**
**Komunikasi antara eternal dan orion** sangatlah dalam; *eternal* tidak akan bisa memasuki dunia pertempuran bila *orion* tidak siap menerima komunikasi (client gk bisa start kalau server belum berjalan).

[PLACE HOLDER Screenshot]

```c
// [File: eternal.c]
int main() {
  // Mengambil ID IPC tanpa flag IPC_CREAT
  msgid = msgget(MSG_KEY, 0666);
  shmid = shmget(SHM_KEY, sizeof(ArenaState) * MAX_ARENA, 0666);

  // 1. Cek apakah memori IPC eksis di OS
  if (msgid < 0 || shmid < 0) {
    printf("Orion are you there?\n");
    return 1;
  }

  // 2. Cek apakah Orion benar-benar berjalan menggunakan IPC_STAT
  struct shmid_ds shm_info;
  if (shmctl(shmid, IPC_STAT, &shm_info) == 0) {
    // shm_nattch menyimpan jumlah proses yang terkoneksi ke memori ini
    // Jika 0, berarti Orion sudah mati dan ini hanya memori sisa (zombie)
    if (shm_info.shm_nattch == 0) {
      printf("Orion are you there?\n");
      return 1;
    }
  }
  // ... lanjut attach shared memory ...
}
```

Klien `eternal` menggunakan `msgget` dan `shmget` secara ketat **tanpa** _flag_ `IPC_CREAT`. Hal ini memastikan bahwa `eternal` hanya mencoba "menyambung" ke saluran IPC yang sudah dibuat oleh server `orion`. Jika balasan kembalian `< 0`, dipastikan server belum mengalokasikan memori. Lebih jauh lagi, program memanfaatkan `shmctl` dengan parameter `IPC_STAT` untuk mengecek `shm_nattch`. Jika nilainya `0`, itu menandakan bahwa meskipun blok memori ada di OS, program `orion` sebenarnya sudah mati/crash (memori _zombie_), sehingga `eternal` akan tetap memblokir akses pengguna dan memunculkan _error_.

#### **Register dan Login**
Para **prajurit** bisa mendaftarkan diri mereka dengan memasukkan *username* dan *password*. Setiap identitas pada dunia ini unik, username yang sudah didaftarkan tidak dapat didaftarkan lagi. Semua data yang ada harus disimpan secara persistent.

[PLACE HOLDER Screenshot]

Dengan menggunakan identitas yang sudah didaftarkan ini, *eternal* dapat memasuki dunia pertempuran melalui menu *login*.

[PLACE HOLDER Screenshot]

```c
// [File: orion.c]
// Loop mendengarkan pesan Message Queue dari Client
if (msgrcv(msgid, &msg, sizeof(AuthMsg) - sizeof(long), -2, 0) > 0) {
  AuthMsg response;
  response.msg_type = 3; 

  if (msg.msg_type == 1) { // REGISTER
    if (!check_user_exists(msg.username, NULL)) {
      PlayerData new_user = {0};
      strcpy(new_user.username, msg.username);
      strcpy(new_user.password, msg.password);
      new_user.gold = 150; // Inisialisasi stat awal
      new_user.level = 1;
      new_user.xp = 0;
      save_user(&new_user); // Tulis ke file binary .dat
      response.status = 1;
    }
  } else if (msg.msg_type == 2) { // LOGIN
    PlayerData user_data;
    if (check_user_exists(msg.username, &user_data)) {
      if (strcmp(user_data.password, msg.password) == 0) {
        // Mencegah eksploitasi multi-login (1 akun 2 aplikasi)
        if (is_online(msg.username)) {
          response.status = 0; 
        } else {
          strcpy(online_users[online_count++], msg.username); // Tandai online
          response.data = user_data;
          response.status = 1;
        }
      }
    }
  }
  msgsnd(msgid, &response, sizeof(AuthMsg) - sizeof(long), 0);
}
```


Autentikasi ini menggunakan arsitektur _Message Queue_ di mana klien mengirim pesan bertipe 1 (Register) atau 2 (Login). Server (`orion.c`) akan membaca file lokal secara konstan menggunakan fungsi `check_user_exists` (mengandalkan `fread` mode _binary_). Jika register sukses, data dasar (_base properties_) akan ditulis ke file menggunakan fungsi `save_user` (`fwrite`). Pada saat login, sistem juga mengecek _array_ internal server `online_users` via fungsi `is_online()` untuk menolak pengguna yang mencoba membuka dua terminal `eternal` menggunakan akun yang sama.

#### Default Properties Prajurit

[PLACE HOLDER Screenshot]

Saat entitas pertama kali mendaftar, mereka diberikan status awal sesuai aturan yang ditetapkan.

```c
// [File: orion.c]
// Di dalam blok logika REGISTER
if (!check_user_exists(msg.username, NULL)) {
  PlayerData new_user = {0};
  strcpy(new_user.username, msg.username);
  strcpy(new_user.password, msg.password);
  
  // Set Default Properties
  new_user.gold = 150; 
  new_user.level = 1;
  new_user.xp = 0;
  new_user.max_weapon_dmg = 0; // Belum punya senjata
  
  save_user(&new_user);
  response.status = 1;
}
```


Properti awal ini (seperti `gold` 150, dan senjata kosong) diinisialisasi secara statis pada `struct PlayerData` saat pembuatan akun di server `orion`. Data tersebut kemudian langsung ditulis ke dalam _database binary_ (`users.dat`) via fungsi `save_user`, sehingga akan selalu di-muat (_loaded_) kembali setiap kali pemain tersebut melakukan _login_.

#### Matchmaking
Fase matchmaking akan berjalan selama 35 detik, jika dalam 35 detik tidak menemukan lawan, maka prajurit saat ini akan melawan monster (bot).

```c
// [File: eternal.c]
// Logika saat memilih menu Battle
int arena_idx = -1;
for (int i = 0; i < MAX_ARENA; i++) {
  if (arenas[i].player_count == 1) {
    // Menemukan room yang sedang menunggu lawan
    arena_idx = i;
    arenas[i].player_count = 2;
    is_player_1 = 0; // Menjadi Player 2
    break;
  }
}

if (arena_idx == -1) { // Tidak ada lawan, buat room baru
  for (int i = 0; i < MAX_ARENA; i++) {
    if (arenas[i].player_count == 0) {
      arena_idx = i;
      arenas[i].player_count = 1;
      is_player_1 = 1; // Menjadi Player 1
      break;
    }
  }
  
  // Tunggu maksimal 35 detik
  int wait_time = 0;
  while (arenas[arena_idx].player_count < 2 && wait_time < 35) {
    sleep(1);
    wait_time++;
  }
}
```

Pemain akan memindai _array_ `arenas` di memori yang dibagikan antar proses (_shared memory_). Jika ada arena bernilai `player_count == 1`, pemain tersebut langsung masuk menjadi P2 dan pertarungan dimulai. Jika tidak ada, pemain menempati arena kosong (`player_count == 0`), menjadikannya P1, dan program akan masuk ke dalam loop `sleep(1)` yang bertindak sebagai _timer/countdown_. Jika `wait_time` menyentuh batas 35 detik dan `player_count` masih 1, permainan akan beralih ke mode melawan BOT.

#### Sistem Pertempuran
- Konsep realtime (bukan turn-based), dapat saling menyerang tanpa harus menunggu (asynchronous)
- Tekan "a" untuk menyerang
- Selama pertempuran, tampilkan 5 log teratas dari keadaan pertempuran
- Tampilkan health dari masing-masing prajurit secara realtime
- **Base Damage:** 10
- **Base Health:** 100
- **Cooldown:** 1 detik sebelum bisa menyerang lagi
- Tekan "u" untuk Ultimate (hanya bisa dilakukan jika sudah memiliki senjata/weapon)

```c
// [File: eternal.c]
void *battle_input_thread(void *arg) {
  while (my_arena->battle_active) {
    char c = getch(); // Menggunakan termios untuk raw non-blocking input

    if (c == 'a' || (c == 'u' && my_data.max_weapon_dmg > 0)) {
      sem_wait(&my_arena->mutex); // Kunci arena (Shared Memory)

      // ... Proses kalkulasi pengurangan HP ...
      int damage = (c == 'a') ? base_dmg : (base_dmg * 3);
      *enemy_hp -= damage;
      
      // Update Log Pertempuran
      for (int i = 4; i > 0; i--) 
         strcpy(my_arena->battle_logs[i], my_arena->battle_logs[i - 1]);
      sprintf(my_arena->battle_logs[0], "[%s] dealt %d dmg!", my_data.username, damage);

      sem_post(&my_arena->mutex); // Buka kunci arena
      sleep(1); // Implementasi Cooldown 1 detik
    }
  }
  return NULL;
}
```


Mekanisme _realtime asynchronous_ ini dicapai menggunakan **Multithreading** (`pthread`). Ada satu _thread_ khusus (`battle_input_thread`) yang mendengarkan input kibor terus-menerus menggunakan modifikasi terminal (`termios` dimatikan _ICANON_ dan _ECHO_-nya) sehingga pemain tidak perlu menekan "Enter" setiap kali menekan 'a' atau 'u'.

Karena _bot_ (melalui _thread_ terpisah) dan musuh (klien lain) bisa saja memberikan pengurangan _damage_ secara bersamaan (yang berpotensi merusak kalkulasi variabel), program menggunakan sinkronisasi **Semaphore** (`sem_wait` dan `sem_post`). Proses perhitungan stat baru dan pembaruan log dijamin aman dari _race-condition_.

#### After Battle
setiap prajurit akan terus mendapatkan sebuah pengalaman ketika ia berhasil menyelesaikan pertandingan, baik itu menang ataupun kalah, dan bahkan meningkatkan skill mereka. Berikut adalah formulanya:
- **XP Menang:** +50
- **XP Kalah:** +15
- Level bertambah ketika XP sudah kelipatan 100 (XP tidak terreset)
- **Gold Menang:** +120
- **Gold Kalah:** +30
- **Damage:** BASE DAMAGE + (total xp / 50) + (total bonus dmg weapon)
- **Health:** BASE HEALTH + (total xp / 10)

```c
// [File: eternal.c - di dalam loop state utama setelah kalkulasi XP]
my_data.level = 1 + (my_data.xp / 100);

int base_hp = 100 + (my_data.xp / 10);
int base_dmg = 10 + (my_data.xp / 50) + my_data.max_weapon_dmg;
```


Tidak menggunakan struktur data tabel bersarang yang boros, perhitungan level dan status (HP/Damage) dikalkulasi secara dinamis saat iterasi layar menggunakan deret matematika integer dasar `(xp / nilai_pembagi)`. Karena bahasa C akan membulatkan angka desimal (_floor_) pada operasi integer, level dan penambahan stat hanya akan bertambah (_scaling_) persis ketika total kelipatan _experience point_ tersebut telah terpenuhi.

#### Monster/Bot
Bot dimunculkan jika sistem _matchmaking_ habis waktu (35 detik). Bot memiliki mekanik penyerangan independen. Bot memiliki *flat damage* sebesar 15, dan akan menyerang dengan **interval tetap** 2 detik. Bot tidak memiliki sistem *level*, hanya para **prajurit** yang memiliki kemampuan untuk betumbuh menjadi lebih kuat pada dunia ini.

```c
// [File: eternal.c]
void *bot_attack_thread(void *arg) {
  while (my_arena->battle_active) {
    sleep(3); // Bot memiliki jeda serangan

    sem_wait(&my_arena->mutex);

    // Pastikan battle belum selesai dan bot belum mati sebelum menyerang
    if (!my_arena->battle_active || my_arena->p2_hp <= 0 || my_arena->p1_hp <= 0) {
      sem_post(&my_arena->mutex);
      break;
    }

    int bot_dmg = 15; // Flat damage bot
    my_arena->p1_hp -= bot_dmg;

    // Update battle logs
    for (int i = 4; i > 0; i--)
      strcpy(my_arena->battle_logs[i], my_arena->battle_logs[i - 1]);
    sprintf(my_arena->battle_logs[0], "[BOT] dealt %d damage!", bot_dmg);

    sem_post(&my_arena->mutex);
  }
  return NULL;
}
```


Sama seperti mekanisme input pemain, serangan Bot dipisahkan menjadi _thread_ tersendiri agar dapat berjalan secara asinkron/berbarengan. _Thread_ bot ini menggunakan `sleep()` untuk mensimulasikan jeda serangannya. `bot_dmg` diatur secara _flat_ (tidak memiliki perhitungan penambahan/scaling stat dari level). Mutex/Semaphore digunakan sebelum `my_arena->p1_hp -= bot_dmg` untuk mencegah masalah apabila _player_ dan bot saling serang di milidetik yang persis sama.

#### Armory
Menggunakan gold yang mereka dapat dari pertempuran, **prajurit** dapat meningkatkan kekuatan mereka lebih jauh lagi dengan melengkapi diri mereka dengan senjata pada *armory*.
- Prajurit akan otomatis menggunakan senjata dengan damage terbesar
- Ketika sudah memiliki senjata, dapat menggunakan Ultimate
- **Ultimate:** Total Damage * 3

```c
// [File: eternal.c]
// Menu pembelian senjata di Armory
if (choice == 1 && my_data.gold >= 100) {
    my_data.gold -= 100; // Kurangi Gold
    
    int new_weapon_dmg = 20; // Contoh damage senjata
    if (new_weapon_dmg > my_data.max_weapon_dmg) {
        my_data.max_weapon_dmg = new_weapon_dmg; // Equip otomatis jika lebih besar
    }
    
    // Kirim sinkronisasi ke Orion
    AuthMsg sync_req = {0};
    sync_req.msg_type = 4; // Tipe pesan khusus untuk SAVE data
    sync_req.data = my_data;
    msgsnd(msgid, &sync_req, sizeof(AuthMsg) - sizeof(long), 0);
    printf("Senjata berhasil dibeli!\n");
}
```


Ketika pemain memilih senjata, sistem akan memvalidasi apakah properti `gold` mencukupi. Jika iya, program mengkalkulasi ulang `max_weapon_dmg`. Mekanisme penting di sini adalah _Auto-Equip_: sistem hanya akan mengganti nilai senjata jika `new_weapon_dmg` lebih tinggi dari properti yang dimiliki pemain saat ini. Setelah transaksi, klien `eternal` akan mengirim paket data ter-sinkronisasi (via pesan IPC tipe 4/Save) ke `orion` agar saldo _gold_ dan `damage` yang baru direkam ke file `.dat` secara permanen.

#### Match History
Pada Dunia Eterion, setiap jiwa memiliki ingatan masa lalunya; sistem akan menyimpan catatan pertempuran dari setiap jiwa pada `history_[USERNAME].txt`.

```
// [File: eternal.c]
// Di dalam fungsi after_battle() atau kalkulasi post-match
char history_filename[100];
sprintf(history_filename, "history_%s.txt", my_data.username);

// Buka file dengan mode "a" (Append)
FILE *history_file = fopen(history_filename, "a");
if (history_file != NULL) {
    // Ambil waktu saat ini untuk dicatat
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    
    // Catat status pertandingan
    if (is_win) {
        fprintf(history_file, "[%02d-%02d-%d %02d:%02d:%02d] Result: WIN | Enemy: %s | XP Earned: +50 | Gold Earned: +120\n",
                tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900, tm->tm_hour, tm->tm_min, tm->tm_sec, enemy_name);
    } else {
        fprintf(history_file, "[%02d-%02d-%d %02d:%02d:%02d] Result: LOSE | Enemy: %s | XP Earned: +15 | Gold Earned: +30\n",
                tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900, tm->tm_hour, tm->tm_min, tm->tm_sec, enemy_name);
    }
    fclose(history_file);
}
```


Setiap kali sesi tempur usai, sistem akan menghasilkan nama file spesifik menggunakan fungsi `sprintf(..., "history_%s.txt", ...)`. Program lalu membuka file tersebut melalui bahasa C I/O biasa dengan menggunakan mode `"a"` (_Append_), bukan `"w"` (_Write_). Mode append ini berfungsi untuk memastikan bahwa jika file sudah ada, tulisan log pertarungan baru akan ditambahkan di baris paling bawah tanpa menghapus ingatan pertempuran sebelumnya. Sistem juga mengikutsertakan modul `time.h` untuk merekam stempel waktu (_timestamp_) nyata terjadinya pertarungan.


### Output


## Kendala & Edge Cases

Tentu, berikut adalah potongan kode konkret yang menangani masing-masing *edge case* yang telah kita bahas pada program **Soal 1 ("The Wired" dan "Navi")**:

### Soal1
#### 1. Error Handling di Sisi Klien (`navi.c`)

##### **Interupsi Paksa Pengguna (SIGINT / `Ctrl+C`)**
Sinyal OS dicegat menggunakan `signal()`, lalu dialihkan ke fungsi `handle_sigint` untuk melakukan penutupan yang bersih.
```c
// [Di dalam fungsi main()]
// Bind signal interupsi agar tidak langsung mati
signal(SIGINT, handle_sigint);

// ... 

// Fungsi independen untuk menangani penutupan
void handle_sigint(int sig) {
  printf("\n[system] Disconnecting from The Wired...\n");
  
  // Bungkus paket perpisahan agar server tahu
  Packet pkt = {CHAT, "", "/exit"};
  strcpy(pkt.name, username);
  send(sock, &pkt, sizeof(Packet), 0); // Kirim ke server
  
  close(sock); // Tutup jalur dengan aman
  exit(0);     // Matikan program
}
```

##### **Server Belum Menyala (Connection Refused)**
Fungsi `connect()` akan divalidasi kembaliannya (*return value*-nya).
```c
// Mencoba terhubung ke alamat server
if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
  printf("\nConnection Failed \n");
  return -1; // Keluar dari program dengan status error
}
```

##### **Duplikasi Identitas (Duplicate Name Registration)**
Klien menunggu persetujuan. Jika kembaliannya tipe *Fail*, klien langsung mematikan dirinya.
```c
// Klien mengirim request pendaftaran username...
send(sock, &reg_pkt, sizeof(Packet), 0);

Packet res;
recv(sock, &res, sizeof(Packet), 0); // Menunggu balasan server

if (res.type == CONNECT_RES_FAIL) {
  // Jika ditolak, cetak alasan dan putus koneksi
  printf("[system] %s\n", res.payload);
  close(sock);
  return 0;
}
```

##### **Input Kosong dan EOF (`Ctrl+D`)**
Pencegahan pengiriman *spam* paket kosong atau terminal yang macet saat membaca input paksa.
```c
char input[MAX_MSG];
while (1) {
  printf("> ");
  
  // Jika mengembalikan NULL (EOF/Ctrl+D ditekan), keluar dari loop
  if (fgets(input, MAX_MSG, stdin) == NULL)
    break;
    
  // Hilangkan karakter "Enter" (\n) di akhir input
  input[strcspn(input, "\n")] = 0;

  if (strcmp(input, "/exit") == 0) {
    handle_sigint(0);
  } else if (strlen(input) > 0) { 
    // HANYA kirim paket jika panjang string lebih dari 0 karakter (bukan string kosong)
    Packet chat_pkt = {CHAT, "", ""};
    strcpy(chat_pkt.name, username);
    strcpy(chat_pkt.payload, input);
    send(sock, &chat_pkt, sizeof(Packet), 0);
  }
}
```

---

#### 2. Error Handling di Sisi Server (`wired.c`)

##### **Port 8080 Tersangkut (Address Already in Use)**
Menyisipkan opsi soket `SO_REUSEADDR` sesaat sebelum melakukan *binding*.
```c
if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
  perror("socket failed");
  exit(EXIT_FAILURE);
}

// Paksa OS untuk merelakan port yang masih nyangkut di status TIME_WAIT
int opt = 1;
setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

// ... lanjut ke bind()
```

##### **Pemutusan Koneksi Sepihak oleh Klien (Broken Pipe)**
Validasi saat server membaca paket `recv()` di dalam *looping* `select()`.
```c
Packet pkt;
int valread = recv(sd, &pkt, sizeof(Packet), 0);

// Jika valread 0 (Putus mendadak), ATAU klien sengaja mengirim string "/exit"
if (valread == 0 || (pkt.type == CHAT && strcmp(pkt.payload, "/exit") == 0)) {
  char log_msg[100];
  sprintf(log_msg, "User '%s' disconnected", clients[i].name);
  write_log("System", log_msg); // Catat kejadian ke file log
  
  close(sd);            // Bersihkan memori socket OS
  clients[i].socket = 0; // Kosongkan index agar bisa dipakai pengguna baru
}
```

##### **Pemblokiran Gema Obrolan Sendiri (Self-Echo Prevention)**
Validasi ketat di level pengiriman *broadcast*.
```c
void broadcast(Packet *pkt, int sender_fd) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    // 1. Pastikan socket tujuan tidak kosong (!= 0)
    // 2. Pastikan socket tujuan BUKAN si pengirim aslinya (!= sender_fd)
    // 3. Pastikan tujuan BUKAN admin/Knights (is_admin == 0)
    if (clients[i].socket != 0 && clients[i].socket != sender_fd && clients[i].is_admin == 0) {
      send(clients[i].socket, pkt, sizeof(Packet), 0);
    }
  }
}
```

##### **Protokol Kiamat (Graceful Emergency Shutdown)**
Skema pembersihan sebelum server melakukan `exit()`.
```c
} else if (strcmp(pkt.payload, "3") == 0) {
  write_log("Admin", "RPC_SHUTDOWN");
  write_log("System", "EMERGENCY SHUTDOWN INITIATED");

  // 1. Bungkus pesan darurat ke semua klien
  Packet res = {SYSTEM_MSG, "system", "Server is shutting down..."};
  broadcast(&res, sd); // Klien akan otomatis keluar (exit) jika menerima SYSTEM_MSG

  // 2. Putus semua koneksi klien yang masih nyangkut satu per satu
  for (int j = 0; j < MAX_CLIENTS; j++) {
    if (clients[j].socket != 0)
      close(clients[j].socket);
  }
  
  // 3. Tutup gerbang utama, dan matikan daemon
  close(master_socket);
  exit(0);
}
```
#### Kendala soal1
- [ ] 


### Soal2
#### 1. Error Handling di Sisi Klien (`eternal.c`)

##### **Validasi Input Tipe Data pada Menu Utama**
*   **Edge Case:** Jika program meminta input angka (integer) menggunakan `scanf("%d")`, namun pengguna (secara sengaja atau tidak) mengetikkan karakter huruf (misalnya "sam"), *buffer* masukan (*stdin*) akan kacau. Huruf tersebut tidak akan terbaca oleh `%d`, akan terus tertinggal di *buffer*, dan menyebabkan program masuk ke dalam *infinite loop* (error berulang kali tanpa batas)[cite: 3].
*   **Handling:** Program mengecek nilai kembalian `scanf`. Jika bukan `1` (gagal membaca satu angka), program akan menjalankan perulangan dengan `getchar()` untuk membuang (*flush*) semua sisa karakter di *buffer* sampai menemukan karakter "Enter" (`\n`)[cite: 2].

```c
// [File: eternal.c] - Di dalam loop menu utama
int choice;
// Mengecek apakah scanf berhasil membaca 1 angka
if (scanf("%d", &choice) != 1) {
  // Jika gagal (pengguna memasukkan huruf), bersihkan sisa karakter di buffer
  int c;
  while ((c = getchar()) != '\n' && c != EOF)
    ; // Looping kosong untuk membuang isi buffer

  printf("\n[Error] Input tidak valid! Harap masukkan angka.\n");
  sleep(1);
  continue; // Ulangi loop menu dari awal
}

// Cek batas angka
if (choice < 1 || choice > 3) {
  printf("\n[Error] Pilihan tidak tersedia!\n");
  sleep(1);
  continue; 
}
```

##### **Penggunaan Skill Ultimate ('u') Tanpa Senjata**
*   **Edge Case:** Pengguna menekan tombol 'u' untuk mengeluarkan jurus *ultimate* saat sedang bertarung, padahal ia adalah pemain baru yang belum memiliki senjata[cite: 3]. Sesuai aturan dunia, *ultimate* hanya bisa digunakan jika sudah memiliki senjata[cite: 3]. Jika tidak ditangani, program bisa membiarkan serangan ini masuk atau memberikan *base attack* biasa.
*   **Handling:** Pada *thread* pendengar input (*battle_input_thread*), program memvalidasi apakah karakter yang ditekan adalah 'u' **DAN** apakah stat `max_weapon_dmg` lebih besar dari 0[cite: 2]. Jika syarat tidak terpenuhi, *input* akan diabaikan dan program akan menampilkan pesan peringatan di konsol log pertempuran[cite: 2].

```c
// [File: eternal.c] - Di dalam fungsi *battle_input_thread
char c = getch();

// Cek apakah tombol yang ditekan adalah 'a' ATAU ('u' DENGAN syarat punya senjata)
if (c == 'a' || (c == 'u' && my_data.max_weapon_dmg > 0)) {
   // ... Lanjut kalkulasi serangan ...
} else if (c == 'u' && my_data.max_weapon_dmg == 0) {
   // Jika menekan 'u' tapi senjata masih kosong (0)
   sem_wait(&my_arena->mutex);
   
   // Geser history log untuk menampilkan error
   for (int i = 4; i > 0; i--)
      strcpy(my_arena->battle_logs[i], my_arena->battle_logs[i - 1]);
   strcpy(my_arena->battle_logs[0], "[System] Kamu butuh senjata untuk Ultimate!");
   
   sem_post(&my_arena->mutex);
   // Sengaja memakai continue agar tidak terkena cooldown delay sleep(1)
   continue; 
}
```

##### **Matchmaking Buta (*Blind Matchmaking*) dan Tabrakan Ruang**
*   **Edge Case:** Setelah sebuah pertandingan selesai (Player 1 vs Player 2), variabel *player_count* di memori dikembalikan ke 0. Jika Player 1 segera menekan tombol "Battle" lagi, ia bisa langsung masuk kembali ke ruang arena tersebut. Di sisi lain, jika ada Player 3 masuk dan ruang pertama penuh, ia akan menunggu di ruang kedua. Jika tidak ditangani, P1 dan P3 bisa menunggu di ruang (indeks array) yang berbeda dan saling tidak menyadari keberadaan satu sama lain[cite: 3].
*   **Handling:** Saat *matchmaking*, pemain **diwajibkan** untuk melakukan pemindaian prioritas. Program harus memindai seluruh *array* arena terlebih dahulu untuk mencari ruang yang `player_count`-nya bernilai `1` (ada lawan yang sedang menunggu). Hanya jika tidak ada satupun ruang bernilai `1`, barulah pemain tersebut diizinkan membuat ruang baru di tempat yang bernilai `0`[cite: 2].

```c
// [File: eternal.c] - Saat memilih menu Battle
int arena_idx = -1;

// PRIORITAS 1: Cari ruang yang sudah ada orang menunggu (player_count == 1)
for (int i = 0; i < MAX_ARENA; i++) {
  if (arenas[i].player_count == 1) {
    arena_idx = i;
    arenas[i].player_count = 2; // Tandai penuh
    is_player_1 = 0; // Masuk sebagai Player 2
    break;
  }
}

// PRIORITAS 2: Jika (dan hanya jika) tidak ada yang menunggu, buat ruang baru
if (arena_idx == -1) { 
  for (int i = 0; i < MAX_ARENA; i++) {
    if (arenas[i].player_count == 0) {
      arena_idx = i;
      arenas[i].player_count = 1; // Tandai sedang menunggu
      is_player_1 = 1; // Masuk sebagai Player 1
      break;
    }
  }
  // ... lanjut ke fase tunggu (countdown) 35 detik ...
}
```

#### 2. Error Handling di Sisi Server (`orion.c`)

##### **Eksploitasi Multilogin (Akun Ganda)**
*   **Edge Case:** Sebuah akun dengan identitas unik bisa dieksploitasi dengan cara membuka dua aplikasi `eternal` secara bersamaan dan melakukan *login* dengan *username* yang sama[cite: 3]. Hal ini akan mengacaukan perhitungan XP, uang, serta data di dalam file memori karena ada dua proses klien yang menulisi file `history_[USERNAME].txt` atas nama yang sama.
*   **Handling:** Server `orion` bertindak sebagai penjaga gerbang terpusat[cite: 2]. Ia memelihara *array* internal `online_users`. Setiap kali ada *request* tipe LOGIN masuk, server memanggil fungsi pendeteksi `is_online()` terlebih dahulu[cite: 2]. Jika mengembalikan nilai `true` (nama tersebut sudah di-login-kan di terminal lain), server akan membalas dengan status penolakan (`response.status = 0`) dan membiarkan terminal kedua tetap berada di halaman utama[cite: 2].

```c
// [File: orion.c]
// Tambahan variabel global di server
char online_users[100][50];
int online_count = 0;

int is_online(const char *username) {
  for (int i = 0; i < online_count; i++) {
    if (strcmp(online_users[i], username) == 0) return 1;
  }
  return 0;
}

// ... Di dalam loop msgrcv bagian LOGIN ...
else if (msg.msg_type == 2) { 
  PlayerData user_data;
  if (check_user_exists(msg.username, &user_data)) {
    if (strcmp(user_data.password, msg.password) == 0) {
      // Tolak jika nama pengguna terdeteksi sedang aktif (online)
      if (is_online(msg.username)) {
        response.status = 0; // Kirim penolakan
      } else {
        // Jika aman, tandai sebagai online
        strcpy(online_users[online_count++], msg.username); 
        response.data = user_data;
        response.status = 1; // Kirim persetujuan
      }
    }
  }
}
```
#### Kendala soal2
- [x] bug facing the same bot
- [x] bot not attacking
- [x] pressing "u" without weapon give base attack, should have return error message instead
- [x] after battle, one of the player auto matchmaking
- [x] eternal could start without orion, should have return error message, "Orion are you there?"
- [x] program Entry Menu error silently if enter anything other than available options
```terminal
./eternal
# BATTLE OF ETERION
1. Register
2. Login
3. Exit
Choice: sam
Username: Password: 

```
- [x] countdown
- [x] improve UI
	- [x] no indicator for weapon possession
	- [x] Improve banner
	- [x] give color
- [x] add armory and history
- [x] weapon damage too high
- [x] can login to already logged in user
- [x] player cant detect each other after battle
	- Player 1 & 2 bertarung di `arenas[0]`.
	- Player 3 masuk. Karena `arenas[0]` penuh, ia mengecek `arenas[1]`. Kosong! Maka ia masuk ke sana dan menunggu (`player_count = 1`).
	- Player 1 & 2 selesai. `arenas[0]` di-reset menjadi kosong (`player_count = 0`).
	- Player 1 _matchmaking_ lagi. Ia memindai dari indeks awal. Ia melihat `arenas[0]` kosong. Karena syarat ruang kosong terpenuhi lebih dulu, ia langsung mengklaim `arenas[0]` dan menunggu di sana.
	- Hasilnya: Player 3 menunggu di `arenas[1]`, Player 1 menunggu di `arenas[0]`. Mereka saling "kebutaan" dan akhirnya dua-duanya melawan BOT.

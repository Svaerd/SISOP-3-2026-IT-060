
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

#### **2. Unit NAVI harus mampu menjalankan dua fungsi secara asinkron**

#### **3. Server pusat The Wired dituntut untuk,**
- memiliki skalabilitas tinggi
- tidak terhambat oleh satu pengguna yang lambat
- Server harus mampu mendeteksi aktivitas dari banyak klien
- membedakan antara permintaan koneksi baru dengan pesan masuk
- menangani diskoneksi klien secara bersih
- mengirimkan output pada client.

#### **4. Identitas digital yang unik untuk setiap entitas yang masuk The Wired**

#### **5. Distribusi informasi di dalam The Wired harus bersifat kolektif dan menyeluruh.**

#### **6. Menyediakan *Admin Console*** 
untuk entitas pengolaan (The Knights) dengan beberapa fungsi:
1. Check Active Entities (Users)
2. Check Server Uptime
3. Execute Emergency Shutdown
4. Disconnect

#### **7. Logging untuk setiap transmisi di dalam The Wired**


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


## Kendala
### soal1

### soal2
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

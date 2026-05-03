
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

#### **Entry Rejection**
**Komunikasi antara eternal dan orion** sangatlah dalam; *eternal* tidak akan bisa memasuki dunia pertempuran bila *orion* tidak siap menerima komunikasi (client gk bisa start kalau server belum berjalan).

[PLACE HOLDER Screenshot]

#### **Register dan Login**
Para **prajurit** bisa mendaftarkan diri mereka dengan memasukkan *username* dan *password*. Setiap identitas pada dunia ini unik, username yang sudah didaftarkan tidak dapat didaftarkan lagi. Semua data yang ada harus disimpan secara persistent.

[PLACE HOLDER Screenshot]

Dengan menggunakan identitas yang sudah didaftarkan ini, *eternal* dapat memasuki dunia pertempuran melalui menu *login*.

[PLACE HOLDER Screenshot]

#### Default Properties Prajurit

[PLACE HOLDER Screenshot]

#### Matchmaking
Fase matchmaking akan berjalan selama 35 detik, jika dalam 35 detik tidak menemukan lawan, maka prajurit saat ini akan melawan monster (bot).

#### Sistem Pertempuran
- Konsep realtime (bukan turn-based), dapat saling menyerang tanpa harus menunggu (asynchronous)
- Tekan "a" untuk menyerang
- Selama pertempuran, tampilkan 5 log teratas dari keadaan pertempuran
- Tampilkan health dari masing-masing prajurit secara realtime
- **Base Damage:** 10
- **Base Health:** 100
- **Cooldown:** 1 detik sebelum bisa menyerang lagi
- Tekan "u" untuk Ultimate (hanya bisa dilakukan jika sudah memiliki senjata/weapon)

#### After Battle
setiap prajurit akan terus mendapatkan sebuah pengalaman ketika ia berhasil menyelesaikan pertandingan, baik itu menang ataupun kalah, dan bahkan meningkatkan skill mereka. Berikut adalah formulanya:
- **XP Menang:** +50
- **XP Kalah:** +15
- Level bertambah ketika XP sudah kelipatan 100 (XP tidak terreset)
- **Gold Menang:** +120
- **Gold Kalah:** +30
- **Damage:** BASE DAMAGE + (total xp / 50) + (total bonus dmg weapon)
- **Health:** BASE HEALTH + (total xp / 10)

#### Monster/Bot
Bot memiliki *flat damage* sebesar 15, dan akan menyerang dengan interval tetap 2 detik. Bot tidak memiliki sistem *level*, hanya para **prajurit** yang memiliki kemampuan untuk betumbuh menjadi lebih kuat pada dunia ini.

#### Armory
Menggunakan gold yang mereka dapat dari pertempuran, **prajurit** dapat meningkatkan kekuatan mereka lebih jauh lagi dengan melengkapi diri mereka dengan senjata pada *armory*.
- Prajurit akan otomatis menggunakan senjata dengan damage terbesar
- Ketika sudah memiliki senjata, dapat menggunakan Ultimate
- **Ultimate:** Total Damage * 3

#### Match History
Pada Dunia Eterion, setiap jiwa memiliki ingatan masa lalunya; sistem akan menyimpan catatan pertempuran dari setiap jiwa pada `history_[USERNAME].txt`.


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

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"     
#include <stdlib.h> 
#include <math.h>     
#include <stdio.h>    

// --- KONFIGURASI UMUM ---
#define LEBAR_LAYAR 800
#define TINGGI_LAYAR 600
#define MAX_JEJAK 2000      // Sebelumnya MAX_HISTORY
#define MAX_PANJANG_TUBUH 500 
#define JARAK_SEGMEN 3      // Rapat (Anti-Renggang)

// --- DEFINISI WARNA ---
#define WARNA_JUDUL        (Color){ 190, 33, 55, 255 }  
#define WARNA_TEKS_MENU    (Color){ 230, 230, 230, 255 } 
#define WARNA_TOMBOL_HOVER (Color){ 255, 215, 0, 255 }   
#define NAGA_WARNA_INTI    (Color){ 255, 0, 255, 255 }   
#define NAGA_WARNA_LUAR    (Color){ 40, 0, 60, 255 }     
#define NAGA_WARNA_DURI    (Color){ 255, 215, 0, 255 }   

// --- STATUS PERMAINAN (GAME STATE) ---
typedef enum { 
    MENU_UTAMA, 
    PILIH_KESULITAN, 
    PETUNJUK, 
    SEDANG_MAIN, 
    PERMAINAN_BERAKHIR 
} StatusGame;

// --- STRUKTUR DATA MAKANAN ---
typedef struct {
    Vector2 posisi;
    bool aktif;
    float radius;       // Jari-jari lingkaran
    float waktuAnimasi; 
} Makanan;

// --- STRUKTUR DATA NAGA ---
typedef struct {
    Vector2 posisi;       
    Vector2 arah;       // Sebelumnya: velocity
    float laju;         // Sebelumnya: speed
    float radius;       // Ukuran kepala/badan
    Vector2 jejak[MAX_JEJAK]; // Array untuk menyimpan riwayat posisi
    int jumlahJejak;       
    int panjangTubuh;         
} Naga;

// --- VARIABEL GLOBAL ---
Naga naga = { 0 };
Makanan makanan = { 0 };
int levelSaatIni = 1; 

// Variabel Audio
Music musikMenu;
Music musikGame;
Sound suaraMakan;  // fxEat
Sound suaraKlik;   // fxClick
Sound suaraMati;   // fxDie

// --- MANAJER AUDIO ---
void GantiMusikKe(int tipe) { 
    StopMusicStream(musikMenu);
    StopMusicStream(musikGame);
    
    if (tipe == 1) PlayMusicStream(musikMenu);
    else if (tipe == 2) PlayMusicStream(musikGame);
    // Tipe 0 = Hening (Diam)
}

// --- FUNGSI UTAMA GAME ---
void MunculkanMakanan() {
    makanan.posisi = (Vector2){ 
        (float)GetRandomValue(50, LEBAR_LAYAR - 50), 
        (float)GetRandomValue(50, TINGGI_LAYAR - 50) 
    };
    makanan.aktif = true;
    makanan.radius = 10.0f;
    makanan.waktuAnimasi = 0.0f;
}

void MulaiGameBaru(int kesulitan) {
    naga.posisi = (Vector2){ LEBAR_LAYAR / 2, TINGGI_LAYAR / 2 };
    naga.arah = (Vector2){ 1.0f, 0.0f }; 
    naga.radius = 15.0f;  
    naga.panjangTubuh = 5; 
    naga.jumlahJejak = 0;
    
    // Atur kecepatan berdasarkan level
    if (kesulitan == 1) naga.laju = 120.0f;      
    else if (kesulitan == 2) naga.laju = 190.0f; 
    else naga.laju = 300.0f; 
    
    // Reset jejak agar tidak glitch visual
    for(int i=0; i<MAX_JEJAK; i++) naga.jejak[i] = naga.posisi;
    MunculkanMakanan();
}

void GambarMataNaga(float x, float y, float ukuran, bool kedip) {
    if (kedip) {
        DrawLineEx((Vector2){x-ukuran, y}, (Vector2){x+ukuran, y}, 2, BLACK); 
    } else {
        DrawEllipse(x, y, ukuran, ukuran * 0.6f, RAYWHITE);
        DrawEllipse(x, y, ukuran * 0.3f, ukuran * 0.5f, BLACK); 
        DrawCircle(x + 2, y - 2, 1, WHITE);
    }
}

// Fungsi menggambar tombol yang ada di tengah layar
bool GambarTombolTengah(const char* teks, int y, int ukuranFont) {
    int lebarTeks = MeasureText(teks, ukuranFont);
    int x = (LEBAR_LAYAR - lebarTeks) / 2;
    
    Vector2 posisiMouse = GetMousePosition();
    
    // Cek apakah mouse berada di atas tombol (Hover)
    bool diAtasTombol = (posisiMouse.x > x && posisiMouse.x < x + lebarTeks && 
                         posisiMouse.y > y && posisiMouse.y < y + ukuranFont);
    
    Color warna = diAtasTombol ? WARNA_TOMBOL_HOVER : WARNA_TEKS_MENU;
    
    if (diAtasTombol) {
        // Efek bayangan
        DrawText(teks, x + 2, y + 2, ukuranFont, (Color){0,0,0,100});
        // Garis bawah
        DrawRectangle(x, y + ukuranFont + 2, lebarTeks, 2, WARNA_TOMBOL_HOVER);
    }
    DrawText(teks, x, y, ukuranFont, warna);
    
    // Jika diklik kiri
    if (diAtasTombol && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        // TEKNIK RE-TRIGGER: Stop dulu baru Play agar responsif
        StopSound(suaraKlik);
        PlaySound(suaraKlik); 
        return true; 
    }
    return false;
}

void GambarTeksTengah(const char* teks, int y, int ukuranFont, Color warna) {
    int lebarTeks = MeasureText(teks, ukuranFont);
    int x = (LEBAR_LAYAR - lebarTeks) / 2;
    DrawText(teks, x, y, ukuranFont, warna);
}

// --- PROGRAM UTAMA (MAIN) ---
int main(void) {
    InitWindow(LEBAR_LAYAR, TINGGI_LAYAR, "Dragon Snake");
    InitAudioDevice(); // Wajib untuk suara
    
    // MUAT FILE AUDIO
    // Nama file dalam tanda kutip HARUS sesuai dengan nama file di folder komputer kamu
    musikMenu = LoadMusicStream("bgm_menu.mp3");
    musikGame = LoadMusicStream("bgm_game.mp3");
    
    suaraMakan = LoadSound("eat.wav");   
    suaraKlik  = LoadSound("click.wav"); 
    suaraMati  = LoadSound("die.wav");   

    SetMusicVolume(musikMenu, 1.0f); 
    SetMusicVolume(musikGame, 0.5f);
    
    musikMenu.looping = true;
    musikGame.looping = true;

    SetTargetFPS(60);

    // Muat Gambar Latar
    Image gambarBg = LoadImage("background.png"); 
    ImageResize(&gambarBg, LEBAR_LAYAR, TINGGI_LAYAR);
    ImageBlurGaussian(&gambarBg, 8); 
    Texture2D latarBelakang = LoadTextureFromImage(gambarBg);
    UnloadImage(gambarBg); 

    StatusGame statusSaatIni = MENU_UTAMA;
    PlayMusicStream(musikMenu);
    bool keluarAplikasi = false; 

    // LOOP GAME
    while (!WindowShouldClose() && !keluarAplikasi) {
        float dt = GetFrameTime(); // Waktu per frame (delta time)
        
        // Update stream musik agar terus berputar
        UpdateMusicStream(musikMenu);
        UpdateMusicStream(musikGame);

        // LOGIKA UPDATE BERDASARKAN STATUS
        switch (statusSaatIni) {
            case MENU_UTAMA: break;
            case PILIH_KESULITAN: break;
            case PETUNJUK: break;

            case SEDANG_MAIN:
                if (IsKeyPressed(KEY_X)) keluarAplikasi = true;

                // Kontrol Arah Naga
                if (IsKeyDown(KEY_UP) && naga.arah.y > -0.1f)    naga.arah = (Vector2){ 0, -1 };
                if (IsKeyDown(KEY_DOWN) && naga.arah.y < 0.1f)   naga.arah = (Vector2){ 0, 1 };
                if (IsKeyDown(KEY_LEFT) && naga.arah.x > -0.1f)  naga.arah = (Vector2){ -1, 0 };
                if (IsKeyDown(KEY_RIGHT) && naga.arah.x < 0.1f)  naga.arah = (Vector2){ 1, 0 };

                // Gerakkan Naga
                naga.posisi = Vector2Add(naga.posisi, Vector2Scale(naga.arah, naga.laju * dt));

                // Simpan Jejak (History)
                for (int i = MAX_JEJAK - 1; i > 0; i--) naga.jejak[i] = naga.jejak[i - 1];
                naga.jejak[0] = naga.posisi;
                if (naga.jumlahJejak < MAX_JEJAK) naga.jumlahJejak++;

                // --- CEK KEMATIAN ---
                bool mati = false;
                
                // Tabrakan Dinding
                if (naga.posisi.x < naga.radius || naga.posisi.x > LEBAR_LAYAR - naga.radius ||
                    naga.posisi.y < naga.radius || naga.posisi.y > TINGGI_LAYAR - naga.radius) mati = true;

                // Tabrakan Tubuh Sendiri
                if (naga.panjangTubuh > 15) { 
                    int mulaiCek = 15 * JARAK_SEGMEN; 
                    int maxCekJejak = naga.panjangTubuh * JARAK_SEGMEN;
                    for (int i = mulaiCek; i < maxCekJejak && i < naga.jumlahJejak; i++) {
                        if (CheckCollisionCircles(naga.posisi, naga.radius - 5, naga.jejak[i], 8)) mati = true;
                    }
                }

                if (mati) {
                    // Matikan suara makan paksa jika sedang berbunyi
                    StopSound(suaraMakan); 
                    
                    // Mainkan suara mati
                    StopSound(suaraMati); 
                    PlaySound(suaraMati); 
                    
                    GantiMusikKe(0); // Matikan musik
                    statusSaatIni = PERMAINAN_BERAKHIR;
                } 
                else {
                    // --- CEK MAKAN (Hanya jika TIDAK mati) ---
                    if (CheckCollisionCircles(naga.posisi, naga.radius + 10, makanan.posisi, makanan.radius)) {
                        
                        // Re-trigger suara agar tidak delay
                        StopSound(suaraMakan); 
                        PlaySound(suaraMakan); 
                        
                        MunculkanMakanan();
                        if (naga.panjangTubuh < MAX_PANJANG_TUBUH) naga.panjangTubuh += 2; 
                        
                        // Tambah kecepatan sedikit
                        float kenaikanLaju = (levelSaatIni == 1) ? 1.0f : (levelSaatIni == 2) ? 2.0f : 3.0f;
                        naga.laju += kenaikanLaju; 
                    }
                }
                
                makanan.waktuAnimasi += dt * 5.0f;
                break;
            
            case PERMAINAN_BERAKHIR: break;
        }

        // --- MENGGAMBAR (DRAWING) ---
        BeginDrawing();
            ClearBackground(BLACK);
            DrawTexture(latarBelakang, 0, 0, WHITE); 

            switch (statusSaatIni) {
                case MENU_UTAMA:
                    DrawRectangle(0,0, LEBAR_LAYAR, TINGGI_LAYAR, (Color){0,0,0,150}); 
                    GambarTeksTengah("DRAGON SNAKE", 60, 60, WARNA_JUDUL);
                    
                    if (GambarTombolTengah("MAIN GAME", 280, 40)) statusSaatIni = PILIH_KESULITAN;
                    if (GambarTombolTengah("PETUNJUK", 350, 40)) statusSaatIni = PETUNJUK;
                    if (GambarTombolTengah("KELUAR", 420, 40)) keluarAplikasi = true;
                    
                    GambarTeksTengah("Game aja aku seriusin, apalagi kamu", TINGGI_LAYAR - 30, 20, DARKGRAY);
                    break;

                case PILIH_KESULITAN:
                    DrawRectangle(0,0, LEBAR_LAYAR, TINGGI_LAYAR, (Color){0,0,0,200});
                    GambarTeksTengah("PILIH LEVEL", 50, 50, GOLD);
                    
                    if (GambarTombolTengah("1. MUDAH (Lambat)", 250, 30)) { 
                        GantiMusikKe(2); levelSaatIni=1; MulaiGameBaru(1); statusSaatIni = SEDANG_MAIN; 
                    }
                    if (GambarTombolTengah("2. NORMAL (Sedang)", 320, 30)) { 
                        GantiMusikKe(2); levelSaatIni=2; MulaiGameBaru(2); statusSaatIni = SEDANG_MAIN; 
                    }
                    if (GambarTombolTengah("3. SULIT (Cepat)", 390, 30)) { 
                        GantiMusikKe(2); levelSaatIni=3; MulaiGameBaru(3); statusSaatIni = SEDANG_MAIN; 
                    }
                    if (GambarTombolTengah("< KEMBALI", 500, 20)) statusSaatIni = MENU_UTAMA;
                    break;

                case PETUNJUK:
                    DrawRectangle(0,0, LEBAR_LAYAR, TINGGI_LAYAR, (Color){0,0,0,220});
                    GambarTeksTengah("PETUNJUK BERMAIN", 40, 40, SKYBLUE);
                    GambarTeksTengah("- Gunakan PANAH untuk bergerak -", 180, 20, WHITE);
                    GambarTeksTengah("- Makan ORB bercahaya untuk tumbuh -", 220, 20, WHITE);
                    GambarTeksTengah("- Jangan menabrak dinding atau tubuh sendiri -", 260, 20, WHITE);
                    GambarTeksTengah("- Tekan 'X' saat main untuk KELUAR -", 300, 20, RED);
                    if (GambarTombolTengah("< KEMBALI", 500, 20)) statusSaatIni = MENU_UTAMA;
                    break;

                case SEDANG_MAIN:
                    // Gambar Makanan
                    float denyut = sinf(makanan.waktuAnimasi) * 3.0f; 
                    DrawCircleGradient(makanan.posisi.x, makanan.posisi.y, makanan.radius + 8 + denyut, (Color){0,100,255,100}, BLANK);
                    DrawCircleGradient(makanan.posisi.x, makanan.posisi.y, makanan.radius, WHITE, (Color){0,255,255,255});
                    DrawRing(makanan.posisi, makanan.radius + 2 + denyut, makanan.radius + 4 + denyut, 0, 360, 16, GOLD);

                    // Gambar Tubuh Naga
                    for (int i = naga.panjangTubuh - 1; i >= 0; i--) {
                        int indexJejak = (i + 1) * JARAK_SEGMEN;
                        if (indexJejak < naga.jumlahJejak) {
                            Vector2 pos = naga.jejak[indexJejak];
                            float ukuran = 15.0f; 
                            if (i < 3) ukuran = 6.0f + (i * 3.0f); // Mengecil di ekor
                            
                            // Gambar duri setiap 8 segmen
                            if (i % 8 == 0) DrawPoly((Vector2){pos.x, pos.y - 5}, 3, ukuran + 2, 0, NAGA_WARNA_DURI);
                            DrawCircleGradient(pos.x, pos.y, ukuran, NAGA_WARNA_INTI, NAGA_WARNA_LUAR);
                            DrawCircle(pos.x - 3, pos.y - 3, ukuran/4, (Color){255,255,255,100});
                        }
                    }

                    // Gambar Kepala Naga (Rotasi)
                    float sudut = atan2f(naga.arah.y, naga.arah.x) * RAD2DEG;
                    rlPushMatrix();
                        rlTranslatef(naga.posisi.x, naga.posisi.y, 0);
                        rlRotatef(sudut, 0, 0, 1);
                        
                        DrawTriangle((Vector2){-10, -10}, (Vector2){-10, 10}, (Vector2){-25, 0}, NAGA_WARNA_DURI);
                        DrawEllipse(0, 0, naga.radius + 2, naga.radius, NAGA_WARNA_LUAR);
                        DrawCircleGradient(0, 0, naga.radius - 2, NAGA_WARNA_INTI, BLANK);
                        DrawEllipse(8, 0, 10, 8, NAGA_WARNA_LUAR);
                        
                        // Tanduk samping
                        DrawTriangle((Vector2){-2, -8}, (Vector2){-2, -12}, (Vector2){12, -18}, RAYWHITE);
                        DrawTriangle((Vector2){-2, 8}, (Vector2){12, 18}, (Vector2){-2, 12}, RAYWHITE);
                        
                        // Mata Naga
                        bool kedip = (GetRandomValue(0, 100) > 98);
                        GambarMataNaga(4, -6, 4.5f, kedip); 
                        GambarMataNaga(4, 6, 4.5f, kedip);  
                    rlPopMatrix();
                    
                    // UI Teks
                    DrawText(TextFormat("Level: %d", levelSaatIni), 20, 20, 20, WHITE);
                    DrawText(TextFormat("Skor: %d", (naga.panjangTubuh - 5) / 2), 20, 45, 20, GOLD);
                    break;

                case PERMAINAN_BERAKHIR:
                    DrawRectangle(0, 0, LEBAR_LAYAR, TINGGI_LAYAR, (Color){0,0,0,200}); 
                    GambarTeksTengah("NAGA GUGUR", TINGGI_LAYAR/2 - 60, 40, RED);
                    GambarTeksTengah(TextFormat("Skor Akhir: %d", (naga.panjangTubuh - 5) / 2), TINGGI_LAYAR/2, 30, GOLD);
                    
                    if (GambarTombolTengah("MAIN LAGI", TINGGI_LAYAR/2 + 70, 30)) {
                         StopSound(suaraMati); 
                         MulaiGameBaru(levelSaatIni); 
                         GantiMusikKe(2); 
                         statusSaatIni = SEDANG_MAIN;
                    }
                    if (GambarTombolTengah("KEMBALI KE MENU", TINGGI_LAYAR/2 + 120, 30)) {
                         StopSound(suaraMati); 
                         GantiMusikKe(1); 
                         statusSaatIni = MENU_UTAMA;
                    }
                    break;
            }
            
        EndDrawing();
    }

    // --- BERSIHKAN MEMORI ---
    UnloadMusicStream(musikMenu);
    UnloadMusicStream(musikGame);
    UnloadSound(suaraMakan);
    UnloadSound(suaraKlik);
    UnloadSound(suaraMati);
    UnloadTexture(latarBelakang);
    
    CloseAudioDevice(); 
    CloseWindow();
    return 0;
}
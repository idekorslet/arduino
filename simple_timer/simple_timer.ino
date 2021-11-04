#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>

#define SCREEN_WIDTH 128    // OLED oled width, in pixels
#define SCREEN_HEIGHT 32    // OLED display height, in pixels
#define SCREEN_ADDRESS 0x3C // See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
#define OLED_RESET 4        // Reset pin # (or -1 if sharing Arduino reset pin)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/*
PIN CONNECTION
------------------------------------        ------------------------------------  
| Arduino pin | Rotary Encoder pin |        | Arduino pin  |  OLED display pin |
------------------------------------        ------------------------------------
|      2      |        CLK         |        |      A4      |       SDA         |
|      3      |        DT          |        |      A5      |       SCL         |
|      5      |        SW          |        ------------------------------------
------------------------------------        
*/
// --------------------- VARIABEL UNTUK KURSOR KOTAK ---------------------------
byte page_index = 0;
byte _x, _y, _z = 0;
byte cursor_index = 0;
byte cursor_row_pos = 0;
byte max_cursor_index = 1;

// --------------------- VARIABEL UNTUK ENCODER --------------------------------
#define CLK 2 // CLK = output A
#define DT  3 // DT  = output B
#define SW  5 // SW  = switch --> normally high, ketika ditekan maka nilai outputnya = LOW

#define relay_pin 13

int last_state_CLK;    // status terakhir CLK
int current_state_CLK; // status CLK sekarang
bool paused = false;
bool blinking = false;
bool showCursor = true;
bool timer_active = false;

// -------------------- MENENTUKAN ALAMAT TIMER DI EEPROM -----------------------
const byte HOUR_ADDRESS = 10;
const byte MINUTE_ADDRESS = 12;
const byte SECOND_ADDRESS = 14;

byte jam = 0 ;
byte menit = 0;
byte detik = 0;

byte _time[3];      // variabel array, jika index = 0 --> jam, index = 1 --> menit, index = 2 --> detik
byte temp_time = 0; // variabel temporary / variabel bantu

unsigned long last_blink_time = 0;
unsigned long last_millis_timer = 0;
unsigned long last_time_button_pressed = 0;
int elapsed_time = 0;  // sisa kelebihan waktu setelah 1 detik

void setup() {
    // Atur encoder pin sebagai input
    pinMode(CLK, INPUT);
    pinMode(DT, INPUT);
    pinMode(SW, INPUT_PULLUP); // normally high jika diatur sebagai input pullup
    pinMode(relay_pin, OUTPUT);

    // atur output menjadi low ketika up
    digitalWrite(relay_pin, LOW);

    // load data timer yang tersimpan di EEPROM dan simpan ke variabel array
    _time[0] = EEPROM.read(HOUR_ADDRESS);
    _time[1] = EEPROM.read(MINUTE_ADDRESS);
    _time[2] = EEPROM.read(SECOND_ADDRESS);

    // Setup Serial Monitor
    // Serial.begin(9600);

    // Read the initial state of CLK
    last_state_CLK = digitalRead(CLK);

    // ----------------------------------- OLED Setup ----------------------------------------
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        // Serial.println(F("SSD1306 allocation failed"));
        for(;;); // Don't proceed, loop forever
    }

    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.cp437(true);
    display.setTextWrap(false);
}

void printText(bool clear_display, int col, int row, char *message, bool visible) {
    if (clear_display) display.clearDisplay();

    display.setCursor(col, row);
    display.print(message);

    if (visible) display.display();
}

void drawRectangleCursor(byte row_pos, byte x, byte y, byte z) {
    
    for (byte i = 0; i < y - x; i++) {
        display.drawPixel(x + i, row_pos, SSD1306_WHITE); // garis horizontal atas
        display.drawPixel(x + i, z, SSD1306_WHITE);       // garis horizontal bawah

        if (i < (z - row_pos)) {
            display.drawPixel(x, row_pos + i, SSD1306_WHITE); // garis vertical kiri
            display.drawPixel(y, row_pos + i, SSD1306_WHITE); // garis vertical kanan
        }
    }
}

void updateCursorPos() {
    if (paused || timer_active && !paused) {
        cursor_row_pos = 21;

        if (cursor_index == 0) { // kursor di menu resume
            _x = 9;
            _y = 50;
            _z = 31;
        }
        else { // kursor di menu stop
            _x = 57;
            _y = 86;
            _z = 31;
        }
    }
    else if (paused == false && timer_active == false) {
        if (page_index == 0) {      
            if (cursor_index == 0) cursor_row_pos = 0; else cursor_row_pos = 18;
        }   
        else {
            cursor_row_pos = 0;
            // jika cursor_index 0..2 berarti kursor diposisi timer (0 = jam, 1 = menit, 2 = detik)
            switch (cursor_index) {
                case 0:
                    _x = 2; 
                    _y = 29;
                    _z = 18;
                    break;
                case 1:
                    _x = 38;
                    _y = 65;
                    _z = 18;
                    break;
                case 2:
                    _x = 74;
                    _y = 101;
                    _z = 18;
                    break;
                case 3:
                    cursor_row_pos = 20;
                    _x = 10;
                    _y = 38;
                    _z = 31;
                    break;
                default:
                    cursor_row_pos = 20;
                    _x = 57;
                    _y = 96;
                    _z = 31;
            }
        }
    }
}

void showPageOneMenu() {
    max_cursor_index = 1;
    printText(true, 13, 0, "SETTING", false);
    printText(false, 13, 18, "START", false);    
    printText(false, 0, cursor_row_pos, ">", true);
}

void setMenu(char *left_option, char *right_option) {
    updateAndShowTimer();
    display.setTextSize(1);
    printText(false, 13, 22, left_option, false);
    printText(false, 60, 22, right_option, false);
    display.setTextSize(2);
}

void showPageTwoMenu() {
    max_cursor_index = 4;
    setMenu("save", "cancel");
}

void showPausedMenu() {
    display.clearDisplay();
    max_cursor_index = 1;
    drawRectangleCursor(cursor_row_pos, _x, _y, _z);
    setMenu("resume", "stop");
    display.display();  
}

void runAndShowTimer() {
    display.clearDisplay();
    max_cursor_index = 1;
    drawRectangleCursor(cursor_row_pos, _x, _y, _z);
    setMenu("pause", "stop");
    display.display();
}

void updateAndShowTimer() {
    char timer_char[9];

    // jika timer tidak aktif, maka nilai jam, menit, detik hanya di load untuk sekedar ditampilkan, bukan untuk dikurangi
    if (!timer_active) {
        jam   = _time[0];
        menit = _time[1];
        detik = _time[2];
    }
    else {
        if (millis() - last_millis_timer >= 1000) {
            if (!paused) {
                // simpan sisa kelebihan waktu
                elapsed_time = (millis() - last_millis_timer) - 1000;

                if (detik <= 0) {
                    detik = 60;
                    if (menit > 0) menit--;
                    else if (menit <= 0 && jam > 0) {
                        jam--;
                        menit = 59;
                    }
                }                

                detik--;
                last_millis_timer = millis() - elapsed_time;

                if (jam <= 0 && menit <= 0 && detik <= 0) timer_active = false;                
            }
        }
    }

    sprintf_P(timer_char, PSTR("%02d:%02d:%02d"), jam, menit, detik);
    printText(false, 5, 2, timer_char, false);
}

void showMenu() {
    updateCursorPos();
    
    if (timer_active) {
        if (paused) {
            showPausedMenu(); 
            digitalWrite(relay_pin, LOW);
        }
        else {
            runAndShowTimer();
            digitalWrite(relay_pin, HIGH);
        }
    }
    else {
        paused = false;
        digitalWrite(relay_pin, LOW);

        if (page_index == 0) {
            showPageOneMenu();
            blinking = false;                
        }
        else {
            display.clearDisplay(); // bersihkan layar
            showPageTwoMenu();      // tampilkan menu halaman 2

            // jika blink aktif dan waktu blink >= 300ms, maka tampilkan kotak kursor agar ada efek berkedip
            if (blinking && millis() - last_blink_time >= 300) {
                showCursor = !showCursor;
                last_blink_time = millis();
            }
            
            // jika blink tidak aktif maka tidak perlu ada efek kedip kotak kursornya
            if (!blinking) showCursor = true;
            
            if (showCursor) drawRectangleCursor(cursor_row_pos, _x, _y, _z);

            display.display(); // tampilkan kembali teks dilayar
        }
    }
}

void loop() {
    showMenu();

    // ---------------------- Membaca status putaran encoder, ke kiri atau ke kanan ----------------------------
    // baca status CLK sekarang
    current_state_CLK = digitalRead(CLK);
    
    if (current_state_CLK != last_state_CLK) {
        // baca status DT, jika status DT berbeda dengan status CLK berarti putaran berlawanan arah jarum jam
        int DT_state = digitalRead(DT);
        
        if (DT_state != current_state_CLK) { 
            // jika blinking = true, berarti sedang mengatur nilai timer jam, menit ataupun detik
            if (blinking) { // mengatur nilai timer (penambahan)
                byte max_value = 99;
                //byte temp_time = _time[cursor_index]; // kursor index menentukan apakah sedang mengatur waktu jam, menit atau detik
                
                if (cursor_index > 0) max_value = 59;
                
                temp_time++;
                
                if (temp_time > max_value) temp_time = 0;

                _time[cursor_index] = temp_time;
            }
            else { // mengatur nilai cursor_index
                cursor_index++;

                if (cursor_index > max_cursor_index) cursor_index = 0;
            }
        } 
        else {
            // jika blinking = true, berarti sedang mengatur nilai timer jam, menit ataupun detik
            if (blinking) { // mengatur nilai timer (pengurangan)
                byte max_value = 99;

                if (cursor_index > 0) max_value = 59;
                
                temp_time--;
                if (temp_time > max_value) temp_time = max_value; else if (temp_time < 0) temp_time = max_value;

                _time[cursor_index] = temp_time;
            }
            else {
                cursor_index--;

                if (cursor_index < 0 || cursor_index > max_cursor_index) cursor_index = max_cursor_index;
            }
        }
    }

    // simpan status terakhir CLK
    last_state_CLK = current_state_CLK;

    // ------------------------ Membaca Status Tombol ditekan atau tidak --------------------------------------
    int btn_state = digitalRead(SW);

    // Jika tombol ditekan, signal output = LOW
    if (btn_state == LOW) {
        
        if (millis() - last_time_button_pressed > 50) {
            // Serial.println("Tombol ditekan!");
            if (timer_active) {
                // jika cursor_index = 0 berarti kursor sedang fokus ke menu pause atau resume
                if (cursor_index == 0) {
                    // balik nilai variabel paused, jika sebelumnya true maka jadi false dan sebaliknya
                    if (paused) {
                        paused = false;
                        last_millis_timer = millis();
                    }
                    else
                        paused = true;
                }
                // jika cursor_index = 0 berarti kursor sedang fokus ke menu stop
                else
                    timer_active = false;
            }
            else {
                if (page_index == 0) {
                    // masuk ke page index selanjutnya jika kursor index = 0 atau di menu setting
                    if (cursor_index == 0) 
                        page_index++; 
                    else {
                        // cursor index = 1 --> kursor fokus ke menu START, maka aktifkan timer
                        timer_active = true;

                        // load nilai timer yang tersimpan di EEPROM
                        jam   = _time[0];
                        menit = _time[1];
                        detik = _time[2];

                        last_millis_timer = millis();
                        cursor_index = 0;
                    }
                }
                else {
                    // jika page index = 1 dan cursor index = 3 atau 4, berarti posisi kursor di menu save atau cancel
                    if (cursor_index == 3 || cursor_index == 4) {                    
                        // jika kursor index = 3, berarti tombol ditekan diposisi menu save (simpan data)
                        // jika sebelumnya blinking (mengatur nilai timer) selanjutnya simpan nilai timer terbaru tersebut ke EEPROM
                        if (cursor_index == 3) {
                            EEPROM.write(HOUR_ADDRESS, _time[0]);
                            EEPROM.write(MINUTE_ADDRESS, _time[1]);
                            EEPROM.write(SECOND_ADDRESS, _time[2]);
                        }
                        
                        // kembali ke page index sebelumnya dan set cursor index menjadi 0
                        page_index--;
                        cursor_index = 0;
                    }
                    else {
                        // jika tombol ditekan dan posisi kursor di timer jam, menit ataupun detik, berarti mau mengatur timer
                        if (!blinking) {
                            temp_time = _time[cursor_index]; // load nilai di variabel array ke temp_time
                            blinking = true;
                        }
                        else
                            blinking = false;
                    }                        
                }
            } 
        }

        // simpan waktu terakhir tombol ditekan
        last_time_button_pressed = millis();
    }   
}

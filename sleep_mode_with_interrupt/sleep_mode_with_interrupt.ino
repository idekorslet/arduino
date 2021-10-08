//#include <avr/interrupt.h>

int off_pin = 8; // pin 14 di chip atmega, pin 8 di arduino
int on_pin = 11; // pin 17 di chip atmega, pin 11 di arduino
int indicator_pin = 2; // pin 4 di chip atmega, pin 2 di arduino

volatile bool check_state = false;
unsigned long last_millis = 0;

void setup() {
    /* ------------------------------------------------------ INFO ----------------------------------------------------
    info tambahan mengenai interrupt: https://thewanderingengineer.com/2014/08/11/arduino-pin-change-interrupts/
    info tambahan mengenai register: https://youtu.be/nZKLHvEdlGk, https://electronoobs.com/eng_arduino_tut130.php
    referensi tentang sleep mode: https://www.youtube.com/watch?v=urLSDi7SD8M
    
    langkah-langkah mengaktifkan interrupt:
    1. Non-aktifkan / stop interrupt sebelum mengatur interrupt menjadi aktif dengan perintah "cli()"
    2. Atur port dan pin di port sebagai interrupt sesuai keperluan, seperti langkah berikut ini:
        1. Tentukan port mana yang akan dijadikan sebagai interrupt --> PCICR
        2. Tentukan pin mana di port yang sudah dipilih sebagai interrupt --> PCMSK --> 0=PORT B, 1=PORT C, 2=PORT D
        - jika port b yang akan diaktifkan sebagai interrupt maka: 
            # PCMSK0 --> port b
            # 0b00000100 --> pin 16 di chip atau PCINT2
        - PCMSK0 |= 0b00000100 --> aktifkan pin 16 yang ada di port B sebagai interrupt
        3. Tentukan kode yang akan di eksekusi di ISR (Interrupt Service Routine)
        - Variabel harus bertipe "volatile"
        - Usahakan jangan ada function delay di dalam ISR
        - kode:
                ISR(PCINT0_vect){}    // Port B, PCINT0 - PCINT7
                ISR(PCINT1_vect){}    // Port C, PCINT8 - PCINT14
                ISR(PCINT2_vect){}    // Port D, PCINT16 - PCINT23
    3. Aktifkan kembali interrupt dengan perintah "sei()"

    Untuk menggunakan perintah "cli()" dan "sei()" harus menambahkan librari <avr/interrupt.h> di header. tetapi ketika dicoba
    tanpa librari tersebut ternyata juga bisa. :confuse
    
    ---------------------------------------------- PIN CHANGE INTERRUPT ---------------------------------------------------*/    
    cli(); // stop all interrupt

    //PCICR = Pin Change Interrupt Control Register
    PCICR |= 0b00000001; // mengaktifkan PORT B sebagai interrupt (dari pin 14-19 (PB0-PB5) di chip atmega / dari pin 8-13 di arduino)

    // PCMSK = Pin Change Mask Register --> untuk menandai pin mana yang akan dijadikan sebagai interrupt di port yang sudah ditentukan
    // pin 16 = PCINT2 --> 0b00000100, pin 17 = PCINT3 --> 0b00001000
    PCMSK0 |= 0b00000001; // mengaktifkan pin 14 di chip atmega sbg pin interrupt 
    PCMSK0 |= 0b00001000; // mengaktifkan pin 17 di chip atmega sbg pin interrupt 
    // bisa juga menggunakan satu baris kode untuk mengaktifkan beberapa pin sebagai interrupt.
    // tergantung dari nilai / posisi bit 1 nya, seperti ini:
    // PCMSK0 |= 0b00001001; --> akan mengaktifkan pin 14 dan pin 17 di chip atmega sebagai interrrupt

    sei(); // enable global interrupt
    // ---------------------------------------- MENGATUR PORT SEBAGAI INPUT / OUTPUT ------------------------------------------
    // Data Direction Register = DDR 
    // - DDRB = Data Direction Register untuk port B
    // - DDRC = Data Direction Register untuk port C
    // - DDRD = Data Direction Register untuk port D
    
    // DDRB &= !B00000001; // Mengaktifkan pin 8 di arduino atau pin 14 di chip atmega (PORT B) 
    // DDRB &= !B00001000; // Mengaktifkan pin 11 di arduino atau pin 17 di chip atmega (PORT B)
    // Jika nilainya 0, berarti digunakan sebagai output, sama seperti ini: pinMode(8, OUTPUT)
    // Jika nilainya 1, berarti digunakan sebagai input, sama seperti ini: pinMode(8, INPUT)

    // Note: 
    // Nilai awal pin 8 adalah b00000001
    // gunakan tanda "&" atau operator "AND" untuk penggunaan pin sebagai INPUT
    // gunakan tanda "|" atau operator "OR" untuk penggunaan pin sebagai OUTPUT    
    // jika digunakan sebagai OUTPUT, maka nilai bit di portnya harus 1 (satu)
    // jika digunakan sebagai INPUT, maka nilai bit di portnya harus 0 (nol)
    // untuk lebih mudahnya gunakan saja operator "!" untuk meng-invert / membalik nilai bitnya

    // ------------------------------------------------------------------
    // nilai bit awal pin 8 |   invert   | hasil / nilai bit sebenarnya |
    // ------------------------------------------------------------------
    //      B00000001       | !B00000001 |        B11111110             |
    // ------------------------------------------------------------------

    // menggunakan register akan mempercepat proses eksekusi program dan memperkecil penggunaan memori
    // dari pada menggunakan function seperti pinMode atau digitalWrite
    
    DDRB &= !B00000001; // sama dengan DDRB &= B11111110 --> pinMode(on_pin, INPUT);  --> bisa dihapus, krn standard pin arduino = INPUT
    DDRB &= !B00001000; // sama dengan DDRB &= B11110111 --> pinMode(off_pin, INPUT); --> bisa dihapus, krn standard pin arduino = INPUT    

    // mengaktifkan bit ke3 di port D (PD2 / pin 4 di atmega / pin 2 di arduino) sebagai OUTPUT
    DDRD |= B00000100; // --> pinMode(indicator_pin, OUTPUT);

    put_to_sleep();
}

void put_to_sleep()
{
    /*------------------------------------------------------------------------------------------------------------------------ 
    // SMCR = Sleep Mode Control Register
    // Sleep Mode terdiri dari: 
    ------------------------------------------
    | SM2 | SM1 | SM0 |     Sleep Mode       |
    ------------------------------------------
    |  0  |  0  |  0  | Idle                 |
    |  0  |  0  |  1  | ADC Noise Reduction  |
    |  0  |  1  |  0  | Power Down           |
    |  0  |  1  |  1  | Power Save           |
    |  1  |  0  |  0  | Reserved             |
    |  1  |  0  |  1  | Reseved              |
    |  1  |  1  |  0  | Standby              |
    |  1  |  1  |  1  | External Standby     |
    ------------------------------------------
    Sleep Mode "Power Down" adalah yang paling rendah konsumsi dayanya. untuk mengaktifkan Mode Power Down, maka nilai SM1 = 1.
    Sebelum mengeksekusi perintah SLEEP, pastikan sudah menulis perintah SE (Sleep Enable).
    contoh:
    sei   --> set Global Interrupt Enable        --> bahasa assembly
    sleep --> enter sleep, waiting for interrupt --> bahasa assembly

    __enable_interrupt(); --> set Global Interrupt Enable        --> bahasa C
    __enable_sleep();     --> enter sleep, waiting for interrupt --> bahasa C

    contoh kode:
    SMCR |= (1 << 2); --> menggeser nilai 1 sebanyak 2x ke kiri, sehingga nilai SM1 = 1, artinya mode Power Down
    SMCR |= 1;        --> enable sleep
    __asm__ __volatile__("sleep");

    Jika ingin lebih rendah lagi konsumsi daya chip atmega-nya, maka bisa nonaktifkan pin analog to digital.
    ADCSRA = ADC Control and Status Register A (23.9.2 on datasheet)
    Bit 7 - ADEN = ADC Enable --> jika nilainya 0, maka ADC non-aktif, jika nilainya 1, maka ADC aktif
    ADCSRA &= ~(1 << 7) --> bit ke 7 diatur nilainya menjadi 1, kemudian di invert menggunakan simbol "~"
    ADCSRA &= B01111111 --> bisa juga menggunakan biner
    
    Jika masih ingin lebih rendah lagi konsumsi dayanya, maka bisa menonaktifkan Brown-out Detection (BOD)
    MCUCR = MCU Control Register (10.11.2 on datasheet)
    BODS = BOD Sleep, BODSE = BOD Sleep Enabled

    ------------------------------------------------------------------------------------------------------------------------*/

    // disable ADC
    ADCSRA &= ~(1 << 7);
    
    // enable sleep
    SMCR |= (1 << 2); // power down mode
    SMCR |= 1;        // enable sleep

    // disable BOD (optional)
    MCUCR |= (3 << 5);
    MCUCR = (MCUCR & ~(1 << 5)) | (1 << 6);

    __asm__ __volatile__("sleep");
}

void loop() {
  
  if (check_state) {
    byte button_on = (PINB >> 3 & B00001000 >> 3); // sama dengan digitalRead(on_pin);
    byte button_off = (PINB & B00000001); // sama dengan digitalRead(off_pin);
    
    // jika nilai button_on = 1 dan button_off = 0 (tidak ditekan)
    if (button_on == 1 && button_off == 0) {
      PORTD |= B00000100; // nyalakan indicator led
    }
    else if (button_on == 0 && button_off == 1) {
      PORTD &= !B00000100; // matikan indicator led
    }

    // jika tidak ada tombol yang ditekan, kemudian hitung waktu sampai 2 detik,
    if (button_on == 0 && button_off == 0) {
        // jika >= 2 detik masih tidak ada terdeteksi tombol ditekan, maka masuk ke sleep mode
        if (millis() - last_millis > 2000) {
            PORTD &= !B00000100; // matikan indicator led
            check_state = false;
            put_to_sleep();        
        }
    }
    else {
        last_millis = millis();
    }
  }
}

// Interrupt vector, pin 14 dan 17 di chip atmega di atur sebagai interrupt, dan di function ISR ini akan mendeteksi
// perubahan signal di pin 14 dan 17 tersebut apakah HIGH atau LOW, jika ada perubahan signal, maka akan mentrigger interrupt
// ISR for port B
ISR (PCINT0_vect) {
    if (check_state == false) {        
        cli();              // Stop interrupts for a while
        check_state = true;
    }
}

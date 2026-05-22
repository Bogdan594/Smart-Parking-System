#ifndef F_CPU
#define F_CPU 16000000UL
#endif
#include <avr/io.h>
#include <util/delay.h>
#define PRAG_LUMINA       500
#define NUMAR_LOCURI      4

// Pini LED-uri 
#define LED1              PORTD2
#define LED2              PORTD3
#define LED3              PORTD4
#define LED4              PORTD5

// Pini Buton si Buzzer 
#define BUTON_BARIERA     PORTD6
#define BUZZER            PORTD7

// 1. DRIVER SIMPLU I2C PENTRU ECRAN 

void I2C_Init(void) {
    TWBR = 72; 
}

void I2C_Start(void) {
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
}

void I2C_Write(uint8_t data) {
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
}

void LCD_Write_I2C(uint8_t data, uint8_t mode) {
    uint8_t adresa_lcd = 0x4E; 
    uint8_t highnib = data & 0xF0;
    uint8_t lownib = (data << 4) & 0xF0;
    
    I2C_Start();
    I2C_Write(adresa_lcd);
    
    I2C_Write(highnib | mode | 0x08 | 0x04); _delay_us(1);
    I2C_Write(highnib | mode | 0x08); _delay_us(50);
    
    I2C_Write(lownib | mode | 0x08 | 0x04); _delay_us(1);
    I2C_Write(lownib | mode | 0x08); _delay_us(50);
}

void LCD_Cmd(uint8_t cmd)   { LCD_Write_I2C(cmd, 0); }
void LCD_Data(uint8_t data) { LCD_Write_I2C(data, 1); }

void LCD_Init(void) {
    _delay_ms(50);
    LCD_Cmd(0x02); 
    LCD_Cmd(0x28); 
    LCD_Cmd(0x0C); 
    LCD_Cmd(0x01); 
    _delay_ms(2);
}

void LCD_Afiseaza_Locuri(uint8_t locuri_libere) {
    LCD_Cmd(0x80); 
    
    char msg1[] = "Locuri Libere: ";
    for (uint8_t i = 0; msg1[i] != '\0'; i++) LCD_Data(msg1[i]);
    
    LCD_Data(locuri_libere + '0');

    LCD_Cmd(0xC0); 
    if (locuri_libere == 0) {
        char msg2[] = "  PARCARE PLINA ";
        for (uint8_t i = 0; msg2[i] != '\0'; i++) LCD_Data(msg2[i]);
    } else {
        char msg2[] = "   BINE ATI VENIT";
        for (uint8_t i = 0; msg2[i] != '\0'; i++) LCD_Data(msg2[i]);
    }
}

// 2. ADC

void ADC_Init(void) {
    ADMUX = (1 << REFS0); // Tensiune de referință 5V (AVCC)
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // Prescaler 128
}

uint16_t ADC_Read(uint8_t canal) {
    ADMUX = (ADMUX & 0xF8) | (canal & 0x07);
    ADCSRA |= (1 << ADSC); // Pornim conversia
    while (ADCSRA & (1 << ADSC)); // Așteptăm finalizarea
    return ADC;
}

// 3. GENERAL

void Hardware_Init(void) {
    // LED1-LED4 si BUZZER ca iesiri
    DDRD |= (1 << DDD2) | (1 << DDD3) | (1 << DDD4) | (1 << DDD5) | (1 << DDD7);
    
    // BUTON_BARIERA ca intrare 
    DDRD &= ~(1 << BUTON_BARIERA);
    // rezistenta interna de Pull-up pentru buton 
    PORTD |= (1 << BUTON_BARIERA);

    
    ADC_Init();
    I2C_Init();
    LCD_Init();
}

// 4. LOGICA SI ACTIUNI PARCARE

uint8_t Actualizeaza_Locuri_Si_Numara(void) {
    uint8_t numarate_libere = 0;
    uint16_t valori_ldr[NUMAR_LOCURI];

    // Citim cele 4 LDR-uri (PC0, PC1, PC2, PC3)
    for (uint8_t i = 0; i < NUMAR_LOCURI; i++) {
        valori_ldr[i] = ADC_Read(i);
    }

    // Locul 1 (PC0 -> LED1/PD2)
    if (valori_ldr[0] < PRAG_LUMINA) { PORTD &= ~(1 << LED1); } 
    else { PORTD |= (1 << LED1); numarate_libere++; }

    // Locul 2 (PC1 -> LED2/PD3)
    if (valori_ldr[1] < PRAG_LUMINA) { PORTD &= ~(1 << LED2); } 
    else { PORTD |= (1 << LED2); numarate_libere++; }

    // Locul 3 (PC2 -> LED3/PD4)
    if (valori_ldr[2] < PRAG_LUMINA) { PORTD &= ~(1 << LED3); } 
    else { PORTD |= (1 << LED3); numarate_libere++; }

    // Locul 4 (PC3 -> LED4/PD5)
    if (valori_ldr[3] < PRAG_LUMINA) { PORTD &= ~(1 << LED4); } 
    else { PORTD |= (1 << LED4); numarate_libere++; }

    return numarate_libere;
}

void Buzzer_Sunet_Eroare(void) {
    // Sunet sacadat de eroare (Beep Beep) când parcare e plină
    for (uint8_t i = 0; i < 2; i++) {
        PORTD |= (1 << BUZZER);
        _delay_ms(150);
        PORTD &= ~(1 << BUZZER);
        _delay_ms(100);
    }
}

void Bariera_Deschide(void) {
    // Sunet de confirmare deschidere barieră
    PORTD |= (1 << BUZZER);
    _delay_ms(500); 
    PORTD &= ~(1 << BUZZER);
    
    // Bariera rămâne deschisă 5 secunde pentru a trece mașina
    _delay_ms(5000); 
}

// 5. MAIN

int main(void) {
    Hardware_Init();
    
    uint8_t locuri_libere = NUMAR_LOCURI;

    while (1) {
        // 1. senzorii LDR și actualizam LED-urile + numarul de locuri
        locuri_libere = Actualizeaza_Locuri_Si_Numara();

        // 2. Trimitem numarul calculat pe ecranul LCD
        LCD_Afiseaza_Locuri(locuri_libere);

        // 3. Verificam dacă este apasat butonul (activ pe 0V / LOW)
        if (!(PIND & (1 << BUTON_BARIERA))) {
            if (locuri_libere > 0) {
                Bariera_Deschide(); // Avem locuri libere -> se ridica bariera
            } else {
                Buzzer_Sunet_Eroare(); // Parcare plina -> sunet de alerta
            }
            
            // așteptam sa se elibereze butonul inainte de a continua
            while (!(PIND & (1 << BUTON_BARIERA)));
            _delay_ms(50);
        }

        _delay_ms(100);
    }

    return 0;
}

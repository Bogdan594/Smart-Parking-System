#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdio.h>

#define PRAG_LUMINA 250
#define LCD_ADDR 0x27 

// Pinii virtuali ai expandorului I2C pentru LCD
#define RS 0
#define RW 1
#define EN 2
#define BL 3

volatile uint8_t bariera_activa = 0;
volatile uint8_t locuri_libere = 4;
volatile uint8_t parcare_plina_bip = 0;
volatile uint8_t directie_iesire = 0; 

void I2C_init(void)
{
  TWSR = 0x00;
  TWBR = 72;
}

void I2C_start(void)
{
  TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
  while (!(TWCR & (1 << TWINT)))
    ;
}

void I2C_stop(void)
{
  TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
  _delay_us(100);
}

void I2C_write(uint8_t data)
{
  TWDR = data;
  TWCR = (1 << TWINT) | (1 << TWEN);
  while (!(TWCR & (1 << TWINT)))
    ;
}

void LCD_send_internal(uint8_t data)
{
  I2C_start();
  I2C_write(LCD_ADDR << 1);
  I2C_write(data);
  I2C_stop();
}

void LCD_pulse(uint8_t data)
{
  LCD_send_internal(data | (1 << EN) | (1 << BL));
  _delay_us(1);
  LCD_send_internal((data & ~(1 << EN)) | (1 << BL));
  _delay_us(50);
}

void LCD_send(uint8_t val, uint8_t mode)
{
  uint8_t high_nibble = (val & 0xF0) | mode | (1 << BL);
  uint8_t low_nibble = ((val << 4) & 0xF0) | mode | (1 << BL);
  LCD_pulse(high_nibble);
  LCD_pulse(low_nibble);
}

void LCD_command(uint8_t cmd)
{
  LCD_send(cmd, 0);
}

void LCD_char(char data)
{
  LCD_send(data, (1 << RS));
}

void LCD_init(void)
{
  _delay_ms(50);
  LCD_pulse(0x30);
  _delay_ms(5);
  LCD_pulse(0x30);
  _delay_us(150);
  LCD_pulse(0x30);
  _delay_ms(1);
  LCD_pulse(0x20);
  _delay_ms(1);

  LCD_command(0x28);
  LCD_command(0x0C);
  LCD_command(0x06);
  LCD_command(0x01);
  _delay_ms(2);
}

void LCD_print(const char *str)
{
  while (*str)
    LCD_char(*str++);
}

void LCD_clear(void)
{
  LCD_command(0x01);
  _delay_ms(2);
}

void ADC_init(void)
{
  ADMUX = (1 << REFS0);
  ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

uint16_t ADC_read(uint8_t canal)
{
  ADMUX = (ADMUX & 0xF0) | (canal & 0x0F);
  ADCSRA |= (1 << ADSC);
  while (ADCSRA & (1 << ADSC))
    ;
  return ADC;
}

void Interrupt_init(void)
{
  // Configurare PD2 (Buton Intrare) și PD3 (Buton Ieșire) ca intrari
  DDRD &= ~((1 << PD2) | (1 << PD3)); // PD2, PD3 intrari
  PORTD |= (1 << PD2) | (1 << PD3); // Pull-up pe ambele 

  //Falling Edge pentru ambele
  EICRA |= (1 << ISC01) | (1 << ISC11);
  EICRA &= ~((1 << ISC00) | (1 << ISC10));

  EIMSK |= (1 << INT0) | (1 << INT1); // ambele intreruperi
}

// ISR INT0 - Buton Intrare
ISR(INT0_vect)
{
  if (locuri_libere > 0)
  {
    if (bariera_activa == 0)
    {
      directie_iesire = 0; 
      bariera_activa = 1;
    }
  }
  else
  {
    parcare_plina_bip = 1;
  }
}

// ISR INT1 - Buton Iesire
ISR(INT1_vect)
{
  if (bariera_activa == 0)
  {
    directie_iesire = 1; 
    bariera_activa = 1;
  }
}

void servo_pozitie_jos(void)
{
  PORTB |= (1 << PB4);
  _delay_us(1500);
  PORTB &= ~(1 << PB4);
  _delay_us(18500);
}

void servo_pozitie_sus(void)
{
  PORTB |= (1 << PB4);
  _delay_us(1000);
  PORTB &= ~(1 << PB4);
  _delay_us(19000);
}

int main(void)
{
  // Configurare Iesiri
  // Port B: PB0-PB3 (LED-uri), PB4 (Servo), PB5 (Buzzer)
  DDRB |= (1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB3) | (1 << PB4) | (1 << PB5);
  // Port D: PD4-PD7 (LED 3 si 4)
  DDRD |= (1 << PD4) | (1 << PD5) | (1 << PD6) | (1 << PD7);

  I2C_init();
  LCD_init();
  ADC_init();
  Interrupt_init();

  sei(); 

  char bufferText[16];

  while (1)
  {
    uint8_t contor_locuri = 4;

    if (ADC_read(0) > PRAG_LUMINA)
    {
      PORTB |= (1 << PB1);
      PORTB &= ~(1 << PB0);
    }
    else
    {
      PORTB |= (1 << PB0);
      PORTB &= ~(1 << PB1);
      contor_locuri--;
    }

    if (ADC_read(1) > PRAG_LUMINA)
    {
      PORTB |= (1 << PB3);
      PORTB &= ~(1 << PB2);
    }
    else
    {
      PORTB |= (1 << PB2);
      PORTB &= ~(1 << PB3);
      contor_locuri--;
    }

    if (ADC_read(2) > PRAG_LUMINA)
    {
      PORTD |= (1 << PD5);
      PORTD &= ~(1 << PD4);
    }
    else
    {
      PORTD |= (1 << PD4);
      PORTD &= ~(1 << PD5);
      contor_locuri--;
    }

    if (ADC_read(3) > PRAG_LUMINA)
    {
      PORTD |= (1 << PD7);
      PORTD &= ~(1 << PD6);
    }
    else
    {
      PORTD |= (1 << PD6);
      PORTD &= ~(1 << PD7);
      contor_locuri--;
    }

    locuri_libere = contor_locuri;

    if (bariera_activa == 0 && parcare_plina_bip == 0)
    {
      LCD_command(0x80);
      LCD_print("Sistem Parcare  ");

      LCD_command(0xC0);
      if (locuri_libere > 0)
      {
        sprintf(bufferText, "Locuri libere: %d", locuri_libere);
        LCD_print(bufferText);
      }
      else
      {
        LCD_print("PARCARE PLINA!  ");
      }
    }

    if (parcare_plina_bip == 1)
    {
      LCD_clear();
      LCD_command(0x80);
      LCD_print("PARCARE PLINA!");
      LCD_command(0xC0);
      LCD_print("ACCES RESPINS! ");

      for (uint8_t j = 0; j < 3; j++)
      {
        PORTB |= (1 << PB5);
        _delay_ms(80);
        PORTB &= ~(1 << PB5);
        _delay_ms(80);
      }
      _delay_ms(1000);
      LCD_clear();
      parcare_plina_bip = 0;
    }

    if (bariera_activa == 1)
    {
      LCD_clear();
      if (directie_iesire == 0)
      {
        LCD_command(0x80);
        LCD_print("BINE ATI VENIT! ");
        LCD_command(0xC0);
        LCD_print("ACCES PERMIS... ");
      }
      else
      {
        LCD_command(0x80);
        LCD_print("DRUM BUN!       ");
        LCD_command(0xC0);
        LCD_print("VA MAI ASTEPTAM!");
      }

      PORTB |= (1 << PB5);
      _delay_ms(200);
      PORTB &= ~(1 << PB5);

      for (int i = 0; i < 50; i++)
      {
        servo_pozitie_sus();
      }

      for (int i = 0; i < 250; i++)
      {
        servo_pozitie_sus();
      }

      for (int i = 0; i < 50; i++)
      {
        servo_pozitie_jos();
      }

      LCD_clear();
      bariera_activa = 0;
    }
    else
    {
      servo_pozitie_jos(); 
    }

    _delay_ms(20);
  }
}
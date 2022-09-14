/*
 * File:   main.c
 * Author: Sakal
 *
 * Created on 3 November 2020, 9:28 PM
 */

#pragma config FOSC = INTIO67   // Oscillator Selection bits (Internal oscillator with port function on RA6 and RA7)
#pragma config WDTEN = OFF      // Watchdog Timer Enable bit (WDT disabled)
#pragma config PWRT = OFF       // Power-up Timer Enable bit (PWRT enabled)
#pragma config BOREN = ON       // Brown-out Reset Enable bit (BOR enabled)
#pragma config LVP = OFF        // Low-Voltage (Single-Supply) In-Circuit Serial Programming Enable bit (RB3 is digital I/O, HV on MCLR must be used for programming)
#pragma config CPD = OFF        // Data EEPROM Memory Code Protection bit (Data EEPROM code protection off)
#pragma config WRT0 = OFF       // Flash Program Memory Write Enable bits (Write protection off; all program memory may be written to by EECON control)
#pragma config WRT1 = OFF
#pragma config WRTC = OFF
#pragma config WRTB = OFF
#pragma config WRTD = OFF

#define _XTAL_FREQ 16000000
#define HUMSENSOR PORTAbits.RA0
#define HUMSENSORCTRL TRISAbits.RA0
#define D4 PORTDbits.RD4
#define D5 PORTDbits.RD5
#define D6 PORTDbits.RD6
#define D7 PORTDbits.RD7
#define RS PORTDbits.RD3
#define EN PORTDbits.RD2
#define DQ PORTDbits.RD0
#define DQMODE TRISDbits.RD0
#define INPUT 1
#define OUTPUT 0


#include <xc.h>

int temperatureWhole = 0, firstTempDecimal = 0, secondTempDecimal = 0;
int humidityWhole = 0, rawHumidity = 0, adcHigh = 0, adcLow = 0, firstHumidityDecimal = 0, secondHumidityDecimal = 0;
float rawTemp = 0.00;
float humidityDecimal = 0.00, temperatureDecimal = 0.00, humidityDigital = 0.00, humidityPercentage = 0.00, humidityAnalog = 0.00;
unsigned char asciiLookup[10] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};

void Lcd_SetBit(char data_bit) //Based on the Hex value Set the Bits of the Data Lines
{
    if(data_bit& 1) 
        D4 = 1;
    else
        D4 = 0;

    if(data_bit& 2)
        D5 = 1;
    else
        D5 = 0;

    if(data_bit& 4)
        D6 = 1;
    else
        D6 = 0;

    if(data_bit& 8) 
        D7 = 1;
    else
        D7 = 0;
}

void Lcd_Cmd(char a)
{
    RS = 0;           
    Lcd_SetBit(a); //Incoming Hex value
    EN  = 1;         
    __delay_ms(4);
    EN  = 0;         
}

void Lcd_Clear()
{
    Lcd_Cmd(0); //Clear the LCD
    Lcd_Cmd(1); //Move the curser to first position
}

void Lcd_Set_Cursor(char a, char b)
{
    char temp,z,y;
    if(a== 1)
    {
      temp = 0x80 + b - 1; //80H is used to move the curser
        z = temp>>4; //Lower 8-bits
        y = temp & 0x0F; //Upper 8-bits
        Lcd_Cmd(z); //Set Row
        Lcd_Cmd(y); //Set Column
    }
    else if(a== 2)
    {
        temp = 0xC0 + b - 1;
        z = temp>>4; //Lower 8-bits
        y = temp & 0x0F; //Upper 8-bits
        Lcd_Cmd(z); //Set Row
        Lcd_Cmd(y); //Set Column
    }
}

void Lcd_Start()
{
  Lcd_SetBit(0x00);
  for(int i=1065244; i<=0; i--)  NOP();  
  Lcd_Cmd(0x03);
    __delay_ms(5);
  Lcd_Cmd(0x03);
    __delay_ms(11);
  Lcd_Cmd(0x03); 
  Lcd_Cmd(0x02); //02H is used for Return home -> Clears the RAM and initializes the LCD
  Lcd_Cmd(0x02); //02H is used for Return home -> Clears the RAM and initializes the LCD
  Lcd_Cmd(0x08); //Select Row 1
  Lcd_Cmd(0x00); //Clear Row 1 Display
  Lcd_Cmd(0x0C); //Select Row 2
  Lcd_Cmd(0x00); //Clear Row 2 Display
  Lcd_Cmd(0x06);
}

void Lcd_Print_Char(char data)  //Send 8-bits through 4-bit mode
{
   char Lower_Nibble,Upper_Nibble;
   Lower_Nibble = data&0x0F;
   Upper_Nibble = data&0xF0;
   RS = 1;             // => RS = 1
   Lcd_SetBit(Upper_Nibble>>4);             //Send upper half by shifting by 4
   EN = 1;
   for(int i=2130483; i<=0; i--)  NOP(); 
   EN = 0;
   Lcd_SetBit(Lower_Nibble); //Send Lower half
   EN = 1;
   for(int i=2130483; i<=0; i--)  NOP();
   EN = 0;
}

void Lcd_Print_String(char *a)
{
    int i;
    for(i=0;a[i]!='\0';i++)
       Lcd_Print_Char(a[i]);  //Split the string using pointers and call the Char function 
}

void reset()
{
    DQ = 0;            
    DQMODE = OUTPUT;    
    __delay_us(500);    
    DQMODE = INPUT;     
    __delay_us(5);
}

void waitForPresence()
{        
    __delay_us(200);
}

void write0()
{
    DQ = 0;
    DQMODE = OUTPUT;
    __delay_us(70);
    DQMODE = INPUT;
    __delay_us(1);
}

void write1()
{
    DQ = 0;
    DQMODE = OUTPUT;
    __delay_us(1);      
    DQMODE = INPUT;
    __delay_us(70);
}

void writeByte(int value)
{
    int i;
    
    for(i = 0; i < 8; i++)
    {
        if(value & (1 << i))
            write1();
        else
            write0();
    }    
    
}

void skipROM()
{
    writeByte(0xCC);    // Skip ROM command
}

void convertT()
{
    writeByte(0x44);    // Convert temperature command
    __delay_ms(1000);   // Wait until conversion is completed
      
}

void writeScratchPad()
{
    writeByte(0x4E);    // Write scratchpad command
    writeByte(0x00);    // TH register
    writeByte(0x00);    // TL register
    writeByte(0x78);    // 12-bit resolution
}

void readScratchPad()
{
    int i = 0, tempH = 0, tempL = 0;
    DQ = 0;
    writeByte(0xBE);    // Read scratchpad command   
    
    for (i = 0; i < 8; i++)
    {
        DQMODE = OUTPUT;
        __delay_us(1);
        DQMODE = INPUT;
        __delay_us(10);
        tempL |= (DQ << i);   
        __delay_us(55);
    }    
    
    for (i = 0; i < 8; i++)
    {
        DQMODE = OUTPUT;
        __delay_us(1);
        DQMODE = INPUT;
        __delay_us(10);
        tempH |= (DQ << i);
        __delay_us(55);        
    }    
    
    rawTemp = (tempH << 8) | tempL;
}

void convertToDegrees()
{
    if(rawTemp / 16)
        temperatureWhole = rawTemp / 16;
    
    else
        temperatureWhole = 0;
    
    temperatureDecimal = ((float)((int)rawTemp % 16) / 16) * 100;
    
    if(temperatureDecimal >= 10)
        firstTempDecimal = temperatureDecimal / 10;
    else
        firstTempDecimal = 0;    
    
    secondTempDecimal = (int)temperatureDecimal % 10;
}

void convertToPercentage()
{        
    humidityDigital = rawHumidity - 211.2495;           // Subtract 0.826V offset
    humidityPercentage = humidityDigital / 8.05177725;  // 8.05177725 is digital number equivalent for 1% RH    
    humidityWhole = (int) humidityPercentage;           
    humidityDecimal = (humidityPercentage - humidityWhole) * 100;
    firstHumidityDecimal = (int)humidityDecimal / 10;
    secondHumidityDecimal = (int)humidityDecimal % 10;    
}

void main()
{                
    OSCCON = 0x76;                          // Sleep mode disabled, 16MHz, Internal Oscillator, HFINTOSC stable, Internal oscillator block
    __delay_ms(10);
    HUMSENSORCTRL = 1;                      // Disable RA0 output driver
    ANSELbits.ANS0 = 1;                     // Disable RA0 digital input driver
    ADCON0 = 0x00;                          // Channel AN0, disable ADC for now
    ADCON1 = 0x30;                          // Use external +Vref and -Vref
    ADCON2 = 0xBE;                          // Right justified, 20TAD, FOSC/64
    ADCON0bits.ADON = 1;                    // Enable ADC
    
    TRISD &= 0x03;                          // Sets ports RD2 ... RD7 as outputs
    Lcd_Start(); 
    Lcd_Clear();
    Lcd_Set_Cursor(1,1);
    Lcd_Print_String("Temp: ");  
    Lcd_Set_Cursor(2,1);
    Lcd_Print_String("Humidity: ");
    
// -----------------------------------------------------------------------------   
// Setup serial port - For debugging 
// -----------------------------------------------------------------------------    
//    SPBRG = 25;                            // Set 9600 baud rate
//    TXSTAbits.TXEN = 1;                     // Enable transmitter
//    TXSTAbits.SYNC = 0;                     // Asynchronous mode
//    BAUDCONbits.BRG16 = 0;                  // 8-bit baud rate
//    RCSTAbits.SPEN = 1;                     // Enable serial port   
// -----------------------------------------------------------------------------
    
    while(1)
    {          
        reset();
        waitForPresence();
        skipROM();
        convertT();
        reset();
        waitForPresence();
        skipROM();
        readScratchPad();
        convertToDegrees();
        
        ADCON0bits.GO = 1;                  // Start the conversion
        while(ADCON0bits.GO)                // Wait until the conversion is done
            __delay_ms(1);
        adcHigh = ADRESH;
        adcLow = ADRESL;
        rawHumidity = adcHigh << 8;         // Make a copy of the result
        rawHumidity |= adcLow;
        convertToPercentage();
        
// -----------------------------------------------------------------------------   
// Output raw humidity data to serial - For debugging 
// ----------------------------------------------------------------------------- 
//        TXREG = (temperatureWhole / 10) + 48;
//        __delay_ms(10);
//        TXREG = (temperatureWhole % 10) + 48;
//        __delay_ms(10);
//        TXREG = '.';
//        __delay_ms(10);
//        TXREG = firstTempDecimal + 48;
//        __delay_ms(10);
//        TXREG = secondTempDecimal + 48;
//        __delay_ms(10);
//        TXREG = 'C';
//        __delay_ms(10);
//        TXREG = 10;
//        __delay_ms(10);
//        TXREG = 13;   
//        __delay_ms(10);
//
//        
//        for(int i = 9; i >= 0; i--)
//        {
//            if (rawHumidity & (1 << i))
//                TXREG = '1';
//            else
//                TXREG = '0';
//            __delay_ms(10);
//        }
//        
//        __delay_ms(10);
//        TXREG = 10;
//        __delay_ms(10);
//        TXREG = 13;
//        __delay_ms(10);
// -----------------------------------------------------------------------------
        
        Lcd_Set_Cursor(1,10);
        Lcd_Print_Char(asciiLookup[temperatureWhole / 10]);
        Lcd_Print_Char(asciiLookup[temperatureWhole % 10]);
        Lcd_Print_String(".");
        Lcd_Print_Char(asciiLookup[firstTempDecimal]);
        Lcd_Print_Char(asciiLookup[secondTempDecimal]);        
        Lcd_Print_Char(0xDF);                           // Degrees symbol
        Lcd_Print_Char(0x43);                           // Celsius
        Lcd_Set_Cursor(2,11);
        Lcd_Print_Char(asciiLookup[humidityWhole / 10]);
        Lcd_Print_Char(asciiLookup[humidityWhole % 10]);
        Lcd_Print_String(".");
        Lcd_Print_Char(asciiLookup[firstHumidityDecimal]);
        Lcd_Print_Char(asciiLookup[secondHumidityDecimal]);        
        Lcd_Print_Char(0x25);                           // Degrees symbol
        __delay_ms(1000);
    }   
    
}    


    
    
    
    
    
    
    
    


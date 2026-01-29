#include "stm32f4xx.h"

/* ================= LCD PIN DEFINITIONS (GPIOC) =================
   PC0 -> RS (Register Select)
   PC1 -> EN (Enable)
   PC2 -> D4
   PC3 -> D5
   PC4 -> D6
   PC5 -> D7
*/
#define LCD_RS  0
#define LCD_EN  1
#define LCD_D4  2
#define LCD_D5  3
#define LCD_D6  4
#define LCD_D7  5

/* ================= DEVICE STATES =================
   LT1, LT2, LT3 -> Lights
   FAN -> Fan
*/
volatile unsigned char LT1=0, LT2=0, LT3=0, FAN=0;

/* ================= SIMPLE DELAY USING SYSTICK =================
   SysTick is configured to generate 1ms interrupts.
   msTicks increments in SysTick_Handler.
*/
volatile unsigned int msTicks;

void SysTick_Config_Custom(void)
{
    // SysTick->LOAD: reload value for 1ms tick
    SysTick->LOAD = 16000000/1000 - 1;  // 1 ms tick @ 16 MHz (adjust for your clock)
    SysTick->VAL = 0;                   // Clear current value
    SysTick->CTRL = (1U<<2) | (1U<<1) | (1U<<0); // CLKSOURCE, TICKINT, ENABLE
}

void SysTick_Handler(void)
{
    msTicks++;
}

void delay_ms(unsigned int ms)
{
    unsigned int start = msTicks;
    while((msTicks - start) < ms);
}

/* ================= GPIO INITIALIZATION ================= */
void GPIO_Init(void)
{
    // RCC->AHB1ENR: Enable GPIOC and GPIOD clocks
    RCC->AHB1ENR |= (1U << 3) | (1U << 2);

    /* Configure PD0–PD5 as outputs for lights and fan */
    GPIOD->MODER |= (1U<<0)|(1U<<2)|(1U<<4)|(1U<<6)|(1U<<8)|(1U<<10); // Output mode
    GPIOD->OTYPER &= ~(0x3F);                                        // Push-pull
    GPIOD->OSPEEDR |= (3U<<0)|(3U<<2)|(3U<<4)|(3U<<6)|(3U<<8)|(3U<<10); // High speed

    /* Configure PC0–PC5 as outputs for LCD */
    GPIOC->MODER |= (1U<<0)|(1U<<2)|(1U<<4)|(1U<<6)|(1U<<8)|(1U<<10);
    GPIOC->OTYPER &= ~(0x3F);
    GPIOC->OSPEEDR |= (3U<<0)|(3U<<2)|(3U<<4)|(3U<<6)|(3U<<8)|(3U<<10);

    /* Configure PC6/PC7 for USART6 (AF8) */
    GPIOC->MODER &= ~((3U<<(6*2))|(3U<<(7*2))); // Clear mode
    GPIOC->MODER |= ((2U<<(6*2))|(2U<<(7*2)));  // Alternate function
    GPIOC->AFR[0] &= ~((0xFU<<(6*4))|(0xFU<<(7*4)));
    GPIOC->AFR[0] |= ((8U<<(6*4))|(8U<<(7*4))); // AF8 = USART6
}

/* ================= LCD FUNCTIONS ================= */
void LCD_Pulse(void)
{
    GPIOC->ODR |= (1U << LCD_EN);
    delay_ms(1);
    GPIOC->ODR &= ~(1U << LCD_EN);
}

void LCD_Send4(unsigned char data)
{
    GPIOC->ODR &= ~((1U<<LCD_D4)|(1U<<LCD_D5)|(1U<<LCD_D6)|(1U<<LCD_D7));
    if (data & 0x01) GPIOC->ODR |= (1U << LCD_D4);
    if (data & 0x02) GPIOC->ODR |= (1U << LCD_D5);
    if (data & 0x04) GPIOC->ODR |= (1U << LCD_D6);
    if (data & 0x08) GPIOC->ODR |= (1U << LCD_D7);
    LCD_Pulse();
}

void LCD_Cmd(unsigned char cmd)
{
    GPIOC->ODR &= ~(1U << LCD_RS); // RS=0 for command
    LCD_Send4(cmd >> 4);
    LCD_Send4(cmd & 0x0F);
    delay_ms(2);
}

void LCD_Data(unsigned char data)
{
    GPIOC->ODR |= (1U << LCD_RS); // RS=1 for data
    LCD_Send4(data >> 4);
    LCD_Send4(data & 0x0F);
    delay_ms(2);
}

void LCD_Init(void)
{
    delay_ms(40);
    LCD_Send4(0x03); delay_ms(15);
    LCD_Send4(0x03); delay_ms(15);
    LCD_Send4(0x03); delay_ms(15);
    LCD_Send4(0x02); // 4-bit mode
    LCD_Cmd(0x28);   // Function set: 4-bit, 2 lines
    LCD_Cmd(0x0C);   // Display ON, cursor OFF
    LCD_Cmd(0x06);   // Entry mode: increment
    LCD_Cmd(0x01);   // Clear display
    delay_ms(15);
}

void LCD_Print(char *s)
{
    while(*s) LCD_Data(*s++);
}

void LCD_DrawStatic(void)
{
    LCD_Cmd(0x80);
    LCD_Print("LT1:    LT2:   ");
    LCD_Cmd(0xC0);
    LCD_Print("LT3:    FAN:  ");
}

void LCD_UpdateStatus(void)
{
    LCD_Cmd(0x80 + 4);  LCD_Print(LT1 ? "OFF" : "ON ");
    LCD_Cmd(0x80 + 12); LCD_Print(LT2 ? "OFF" : "ON ");
    LCD_Cmd(0xC0 + 4);  LCD_Print(LT3 ? "OFF" : "ON ");
    LCD_Cmd(0xC0 + 12); LCD_Print(FAN ? "ON " : "OFF");
}

/* ================= RTC BACKUP REGISTERS =================
   Used to persist states across resets.
   RCC->APB1ENR: enable PWR clock
   PWR->CR: DBP bit allows backup domain access
   RTC->BKP0R–BKP3R: store LT1, LT2, LT3, FAN
*/
void Backup_Init(void)
{
    RCC->APB1ENR |= (1<<28); // Enable PWR clock
    PWR->CR |= (1<<8);       // Enable backup domain access
}

void SaveStates(void)
{
    RTC->BKP0R = LT1;
    RTC->BKP1R = LT2;
    RTC->BKP2R = LT3;
    RTC->BKP3R = FAN;
}

void RestoreStates(void)
{
    LT1 = RTC->BKP0R & 0xFF;
    LT2 = RTC->BKP1R & 0xFF;
    LT3 = RTC->BKP2R & 0xFF;
    FAN = RTC->BKP3R & 0xFF;

    if(LT1) GPIOD->ODR |= (1U<<0); else GPIOD->ODR &= ~(1U<<0);
    if(LT2) GPIOD->ODR |= (1U<<1); else GPIOD->ODR &= ~(1U<<1);
    if(LT3) GPIOD->ODR |= (1U<<2); else GPIOD->ODR &= ~(1U<<2);
    if(FAN) { GPIOD->ODR |= (1U<<3)|(1U<<5); GPIOD->ODR &= ~(1U<<4); }
    else    { GPIOD->ODR &= ~((1U<<3)|(1U<<4)|(1U<<5)); }
}
/* ================= USART6 =================
   RCC->APB2ENR: enable USART6 clock
   USART6->BRR: baud rate register
   USART6->CR1: control register (TX, RX, RXNEIE, UE)
   NVIC: enable interrupt
*/
void USART6_Init(unsigned int baud)
{
    // Baud rate calculation: BRR = (mantissa << 4) | fraction
    float baud_div = 16000000.0f / (16.0f * baud); // APB2 assumed 16 MHz here
    unsigned int integer_part = (unsigned int)baud_div;
    unsigned int fractional_part = (unsigned int)((baud_div - integer_part) * 16.0f + 0.5f);

    RCC->APB2ENR |= (1U << 5); // Enable USART6 clock
    USART6->BRR = (integer_part << 4) | (fractional_part & 0x0F);

    // Enable RX, TX, RXNE interrupt
    USART6->CR1 |= (1U << 2);  // RE: Receiver enable
    USART6->CR1 |= (1U << 3);  // TE: Transmitter enable
    USART6->CR1 |= (1U << 5);  // RXNEIE: RX interrupt enable

    USART6->CR1 &= ~(1U << 10); // Disable parity
    USART6->CR1 &= ~(1U << 12); // 1 start bit, 8 data bits, 1 stop bit
    USART6->CR1 |= (1U << 13);  // UE: USART enable

    NVIC_SetPriority(USART6_IRQn, 1);
    NVIC_EnableIRQ(USART6_IRQn);

    __enable_irq();
}

/* ================= COMMAND HANDLER =================
   Single-character commands:
   '1'/'2' -> LT1 ON/OFF
   '3'/'4' -> LT2 ON/OFF
   '5'/'6' -> LT3 ON/OFF
   'F'/'f' -> FAN ON/OFF
*/
void processChar(char c)
{
    if(c=='1') { GPIOD->ODR |= (1U<<0); LT1=1; }
    else if(c=='2') { GPIOD->ODR &= ~(1U<<0); LT1=0; }
    else if(c=='3') { GPIOD->ODR |= (1U<<1); LT2=1; }
    else if(c=='4') { GPIOD->ODR &= ~(1U<<1); LT2=0; }
    else if(c=='5') { GPIOD->ODR |= (1U<<2); LT3=1; }
    else if(c=='6') { GPIOD->ODR &= ~(1U<<2); LT3=0; }
    else if(c=='F') {
        GPIOD->ODR |= (1U<<3);
        GPIOD->ODR &= ~(1U<<4);
        GPIOD->ODR |= (1U<<5);
        FAN=1;
    }
    else if(c=='f') {
        GPIOD->ODR &= ~(1U<<3);
        GPIOD->ODR &= ~(1U<<4);
        GPIOD->ODR &= ~(1U<<5);
        FAN=0;
    }
    else {
        return; // Ignore unknown commands
    }

    LCD_UpdateStatus(); // Update LCD feedback
    SaveStates();       // Persist new state in RTC backup
}

/* ================= USART6 INTERRUPT HANDLER =================
   Triggered when RXNE flag is set in USART6->SR.
   Reads received character from USART6->DR.
*/
void USART6_IRQHandler(void)
{
    char c;
    if(USART6->SR & (1U << 5))  // RXNE flag
    {
        c = (char)(USART6->DR & 0xFF);
        if(c != '\r' && c != '\n') // Ignore CR/LF
            processChar(c);
    }
}

/* ================= MAIN FUNCTION ================= */
int main(void)
{
    GPIO_Init();             // Configure GPIOC, GPIOD
    SysTick_Config_Custom(); // Configure SysTick for delays
    Backup_Init();           // Enable backup domain
    RestoreStates();         // Restore saved states from RTC
    USART6_Init(9600);       // Initialize USART6 at 9600 baud
    LCD_Init();              // Initialize LCD
    LCD_DrawStatic();        // Draw static labels
    LCD_UpdateStatus();      // Show current states

    while(1)
    {
        delay_ms(100); // Keep CPU alive, background loop
    }
}


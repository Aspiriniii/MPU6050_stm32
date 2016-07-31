#include "stm32f10x.h"

#define BITBAND(addr, bitnum) ((addr & 0xF0000000) + 0x2000000 + ((addr & 0xFFFFF) << 5) + (bitnum<<2))
#define BIT_ADDR(addr, bitnum) *((volatile unsigned long *)(BITBAND(addr, bitnum)))

#define GPIOA_ODR_Addr (GPIOA_BASE + 12)

#define SCL BIT_ADDR((GPIOB_BASE+12), 15)
#define SDA BIT_ADDR((GPIOB_BASE+12), 13)
#define AD0 BIT_ADDR((GPIOC_BASE+12), 6)

#define SDA_IN() {GPIOB->CRH&=0xFF0FFFFF;GPIOB->CRH|=0x00700000;}
#define SDA_OUT() {GPIOB->CRH&=0xFF0FFFFF;GPIOB->CRH|=0x00700000;}

#define GPIOB_IDR_Addr (GPIOB_BASE + 8)
#define READ_SDA BIT_ADDR(GPIOB_IDR_Addr, 13)

//=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_
#define LED0 BIT_ADDR((GPIOA_BASE+12), 8)
#define LED1 BIT_ADDR((GPIOD_BASE+12), 2)
//=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_

//=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_

#define MPU6050_ADDR 0xD0

#define SMPLRT_DIV      0x19
#define CONFIG 0x1A
#define GYRO_CONFIG     0x1B
#define ACCEL_CONFIG    0x1C

#define ACCEL_XOUT_H    0x3B
#define ACCEL_XOUT_L    0x3C
#define ACCEL_YOUT_H    0x3D
#define ACCEL_YOUT_L    0x3E
#define ACCEL_ZOUT_H    0x3F
#define ACCEL_ZOUT_L    0x40

#define TEMP_OUT_H      0x41
#define TEMP_OUT_L      0x42

#define GYRO_XOUT_H     0x43
#define GYRO_XOUT_L     0x44
#define GYRO_YOUT_H     0x45
#define GYRO_YOUT_L     0x46
#define GYRO_ZOUT_H     0x47
#define GYRO_ZOUT_L     0x48

#define PWR_MGMT_1      0x6B
#define WHO_AM_I        0x75

//=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_=_

unsigned char cache[14];
short GYRO_x, GYRO_y, GYRO_z;

void initLED() {
    RCC->APB2ENR |= 1<<2;
    GPIOA->CRH &= 0xFFFFFFF0;
    GPIOA->CRH |= 0x00000003;

    RCC->APB2ENR |= 1<<5;
    GPIOD->CRL &= 0xFFFFF0FF;
    GPIOD->CRL |= 0x00000300;
}
void delay(volatile unsigned int count) {
    for(count *= 12000; count!=0; count--);
}

void IIC_init() {
    RCC->APB2ENR |= 1<<3;
    GPIOB->CRH &= 0x0F0FFFFF;
    GPIOB->CRH |= 0x70700000;
    GPIOB->ODR |= 5<<5;

    RCC->APB2ENR |= 1<<4;
    GPIOB->CRL &= 0xF0FFFFFF;
    GPIOB->CRL |= 0x03000000;
    AD0 = 0;
}
void IIC_Start() {
    SDA_OUT();
    SDA = 1;
    SCL = 1;
    delay(5);
    SDA = 0;
    delay(5);
    SCL = 0;
}
void IIC_Stop() {
    SDA_OUT();
    SCL = 0;
    delay(5);
    SDA = 0;
    delay(5);
    SCL = 1;
    delay(5);
    SDA = 1;
    delay(5);
}
void IIC_Ack(unsigned char ack) {
    SDA_OUT();

    SCL = 0;
    delay(5);
    SDA = ack;
    delay(5);
    SCL = 1;
    delay(5);
    SCL = 0;
    delay(5);
}
unsigned char IIC_Wait_Ack() {
    SDA_IN();

    SCL = 0;
    delay(5);
    SDA = 1;
    delay(5);
    SCL = 1;
    delay(5);
    if(READ_SDA) {
        SCL = 0;
        delay(5);
        return 0;
    }

    SCL = 0;
    LED1 = 0;
    return 1;

}

void IIC_Send(unsigned char dat) {
    unsigned char i;
    SDA_OUT();
    SCL = 0;
    for(i = 0; i < 8; i++) {
        delay(5);
        SDA = (dat&0x80)>>7;
        dat <<= 1;
        delay(5);
        SCL = 1;
        delay(5);
        SCL = 0;
    }
}

unsigned char IIC_Read() {
    unsigned char i, dat = 0;
    SDA_IN();
    SDA = 1;
    for(i = 0; i < 8; i++) {
        SCL = 0;
        delay(5);
        SCL = 1;
        delay(5);
        dat<<=1;
        dat |= READ_SDA;
    }
    SCL = 0;
    return dat;
}

void sendData_uart(unsigned char data);
void MPU_Sigle_Write(unsigned char reg_addr, unsigned char reg_data) {
    IIC_Start();
    IIC_Send(MPU6050_ADDR);
    if(!IIC_Wait_Ack()) LED1 = 1;

    IIC_Send(reg_addr);
    IIC_Wait_Ack();

    IIC_Send(reg_data);
    IIC_Wait_Ack();

    IIC_Stop();

    delay(5);
}
unsigned char MPU_Sigle_Read(unsigned reg_addr) {
    unsigned char reg_data;
    IIC_Start();
    IIC_Send(MPU6050_ADDR);
    IIC_Wait_Ack();

    IIC_Send(reg_addr);
    IIC_Wait_Ack();

    IIC_Start();
    IIC_Send(MPU6050_ADDR+1);
    IIC_Wait_Ack();

    reg_data = IIC_Read();
    IIC_Ack(1);
    IIC_Stop();
    return reg_data;
}
short MPU_GetData(unsigned char REG_Addr) {
    unsigned char H, L;
    H = MPU_Sigle_Read(REG_Addr);
    L = MPU_Sigle_Read(REG_Addr+1);
    return (short)(H<<8)+L;
}
void MPU_init() {
    IIC_init();
    delay(500);
    MPU_Sigle_Write(PWR_MGMT_1, 0x00);
    MPU_Sigle_Write(SMPLRT_DIV, 0x07);
    MPU_Sigle_Write(CONFIG, 0x06);
    MPU_Sigle_Write(GYRO_CONFIG, 0x18);
    MPU_Sigle_Write(ACCEL_CONFIG, 0x01);
}

void initUART(unsigned int pclk2, unsigned int bound) {
    float temp;
    unsigned short mantissa;
    unsigned short fraction;
    temp = (float)(pclk2*1000000)/(bound*16);
    mantissa = temp;
    fraction = (temp - mantissa) * 16;
    mantissa <<= 4;
    mantissa += fraction;
    RCC->APB2ENR |= 1<<2;
    RCC->APB2ENR |= 1<<14;

    GPIOA->CRH &= 0xFFFFF00F;
    GPIOA->CRH |= 0x000008B0;

    RCC->APB2RSTR |= 1<<14;
    RCC->APB2RSTR &= ~(1<<14);

    USART1->BRR = mantissa;
    USART1->CR1 |= 0x200C;

    USART1->CR1 |= 1<<8;
    USART1->CR1 |= 1<<5;
}
void sendData_uart(unsigned char data) {
    USART1->DR = data;
    while((USART1->SR & 0x40) == 0);
}
void showData(short k) {
    unsigned char a, b, c, d, e;

    sendData_uart(k<0?'-':'+');
    if(k<0) k=-k;
    e = (unsigned char)(k % 10);
    d = (unsigned char)(k/10) % 10;
    c = (unsigned char)(k/100) % 10;
    b = (unsigned char)(k/1000) % 10;
    a = (unsigned char)(k/10000);

    sendData_uart(a+0x30);
    sendData_uart(b+0x30);
    sendData_uart(c+0x30);
    sendData_uart(d+0x30);
    sendData_uart(e+0x30);
    sendData_uart('.');
}

int main() {
    initLED();
    initUART(72, 9600);

    MPU_init();


    //USART1->DR = 'A';
    //while((USART1->SR & 0x40)==1);

    while(1) {
        GYRO_x = MPU_GetData(ACCEL_XOUT_H);
        GYRO_x /= 16.4;
        sendData_uart('X');
        sendData_uart(':');
        showData(GYRO_x);

        GYRO_y = MPU_GetData(ACCEL_YOUT_H);
        GYRO_y /= 16.4;
        sendData_uart('Y');
        sendData_uart(':');
        showData(GYRO_y);

        GYRO_z = MPU_GetData(ACCEL_ZOUT_H);
        GYRO_z /= 16.4;
        sendData_uart('Z');
        sendData_uart(':');
        showData(GYRO_z);

        sendData_uart(0x0D);
        sendData_uart(0x0A);

        short tem = MPU_GetData(TEMP_OUT_H);
        tem = 35 + ((double) (tem + 13200)) / 200;
        showData(tem);
        sendData_uart('T');
        sendData_uart(':');
        sendData_uart(0x0D);
        sendData_uart(0x0A);

    }
}
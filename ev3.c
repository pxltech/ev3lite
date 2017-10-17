#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include "font10x16.c"

struct UI
{
  int8_t Pressed[6];
};

struct MOTORDATA
{
  int32_t TachoCounts;
  int8_t Speed;
  int32_t TachoSensor;
};

struct DEVCON
{
  int8_t Connection[4];
  int8_t Type[4];
  int8_t Mode[4];
};

struct TYPES
{
  int8_t Name[12];
  int8_t Type;
  int8_t Connection;
  int8_t Mode;
  int8_t DataSets;
  int8_t Format;
  int8_t Figures;
  int8_t Decimals;
  int8_t Views;
  float RawMin;
  float RawMax;
  float PctMin;
  float PctMax;
  float SiMin;
  float SiMax;
  uint16_t InvalidTime;
  uint16_t IdValue;
  int8_t Pins;
  int8_t Symbol[5];
  uint16_t Align;
};

struct UART
{
  struct TYPES TypeData[4][8];
  uint16_t Repeat[4][300];
  int8_t Raw[4][300][32];
  uint16_t Actual[4];
  uint16_t LogIn[4];
  int8_t Status[4];
  int8_t Output[4][32];
  int8_t OutputLength[4];
};

struct IIC
{
  struct TYPES TypeData[4][8];
  uint16_t Repeat[4][300];
  int8_t Raw[4][300][32];
  uint16_t Actual[4];
  uint16_t LogIn[4];
  int8_t Status[4];
  int8_t Changed[4];
  int8_t Output[4][32];
  int8_t OutputLength[4];
};

struct COLORSTRUCT
{
  uint32_t Calibration[3][4];
  uint16_t CalLimits[2];
  uint16_t Crc;
  uint16_t ADRaw[4];
  uint16_t SensorRaw[4];
};

struct ANALOG
{
  int16_t InPin1[4];
  int16_t InPin6[4];
  int16_t OutPin5[4];
  int16_t BatteryTemp;
  int16_t MotorCurrent;
  int16_t BatteryCurrent;
  int16_t Cell123456;
  int16_t Pin1[4][300];
  int16_t Pin6[4][300];
  uint16_t Actual[4];
  uint16_t LogIn[4];
  uint16_t LogOut[4];
  struct COLORSTRUCT NxtCol[4];
  int16_t OutPin5Low[4];
  int8_t Updated[4];
  int8_t InDcm[4];
  int8_t InConn[4];
  int8_t OutDcm[4];
  int8_t OutConn[4];
  uint16_t PreemptMilliSeconds;
};

int ev3_fd_pwm, ev3_fd_fb, ev3_fd_ui, ev3_fd_uart, ev3_fd_analog;
uint32_t *ev3_fb;
struct UI *ev3_ui;
struct UART *ev3_uart;
struct ANALOG *ev3_analog;

void ev3_init()
{
  if((ev3_fd_pwm = open("/dev/lms_pwm", O_RDWR | O_SYNC)) < 0)
  {
    perror("open");
    exit(EXIT_FAILURE);
  }

  if((ev3_fd_fb = open("/dev/fb0", O_RDWR | O_SYNC)) < 0)
  {
    perror("open");
    exit(EXIT_FAILURE);
  }

  if((ev3_fd_ui = open("/dev/lms_ui", O_RDWR | O_SYNC)) < 0)
  {
    perror("open");
    exit(EXIT_FAILURE);
  }

  if((ev3_fd_uart = open("/dev/lms_uart", O_RDWR | O_SYNC)) < 0)
  {
    perror("open");
    exit(EXIT_FAILURE);
  }

  if((ev3_fd_analog = open("/dev/lms_analog", O_RDWR | O_SYNC)) < 0)
  {
    perror("open");
    exit(EXIT_FAILURE);
  }

  ev3_fb = mmap(NULL, 7680, PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED, ev3_fd_fb, 0);
  ev3_ui = mmap(NULL, sizeof(struct UI), PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED, ev3_fd_ui, 0);
  ev3_uart = mmap(NULL, sizeof(struct UART), PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED, ev3_fd_uart, 0);
  ev3_analog = mmap(NULL, sizeof(struct ANALOG), PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED, ev3_fd_analog, 0);
}

void ev3_fb_clean()
{
  memset(ev3_fb, 0, 7680);
}

void ev3_fb_print(uint16_t x, uint16_t y, uint8_t *buffer, uint8_t size)
{
  uint8_t i, j;
  uint16_t code;
  for(i = 0; i < size; ++i)
  {
    if(buffer[i] == 0) return;
    code = buffer[i] - 32;
    for(j = 0; j < 16; ++j)
    {
      ev3_fb[y * 240 + j * 15 + x + i] = font10x16[code * 16 + j];
    }
  }
}

void ev3_uart_mode(int port, int mode, int conn, int type)
{
  struct DEVCON buffer;
  buffer.Mode[port] = mode;
  buffer.Connection[port] = conn;
  buffer.Type[port] = type;
  ioctl(ev3_fd_uart, _IOWR('u', 0, struct DEVCON), &buffer);
}

int ev3_output_test(int mask)
{
  char buffer[11];
  int test1, test2;
  memset(buffer, 0, 11);
  read(ev3_fd_pwm, buffer, 10);
  sscanf(buffer, "%d %d", &test1, &test2);
  return (mask & test2) ? 1 : 0;
}

void ev3_output_wait(int mask)
{
  while(1)
  {
    usleep(1000);
    if(!ev3_output_test(mask)) break;
  }
}

void ev3_output_stop(int mask, int brake)
{
  int8_t buffer[3];
  buffer[0] = 0xA3;
  buffer[1] = mask;
  buffer[2] = brake;
  write(ev3_fd_pwm, buffer, 3);
}

void ev3_output_speed(int mask, int speed)
{
  int8_t buffer[3];
  buffer[0] = 0xA5;
  buffer[1] = mask;
  buffer[2] = speed;
  write(ev3_fd_pwm, buffer, 3);
}

void ev3_output_start(int mask)
{
  int8_t buffer[2];
  buffer[0] = 0xA6;
  buffer[1] = mask;
  write(ev3_fd_pwm, buffer, 2);
}

void ev3_output_polarity(int mask, int polarity)
{
  int8_t buffer[3];
  buffer[0] = 0xA7;
  buffer[1] = mask;
  buffer[2] = polarity;
  write(ev3_fd_pwm, buffer, 3);
}

void ev3_output_step_speed(int mask, int speed, int step1, int step2, int step3, int brake)
{
  struct {int8_t command; int8_t mask; int8_t speed; int32_t step1; int32_t step2; int32_t step3; int8_t brake;} buffer;
  buffer.command = 0xAE;
  buffer.mask = mask;
  buffer.speed = speed;
  buffer.step1 = step1;
  buffer.step2 = step2;
  buffer.step3 = step3;
  buffer.brake = brake;
  write(ev3_fd_pwm, &buffer, sizeof(buffer));
}

void ev3_output_step_sync(int mask, int speed, int turn, int step, int brake)
{
  struct {int8_t command; int8_t mask; int8_t speed; int16_t turn; int32_t step; int8_t brake;} buffer;
  buffer.command = 0xB0;
  buffer.mask = mask;
  buffer.speed = speed;
  buffer.turn = turn;
  buffer.step = step;
  buffer.brake = brake;
  write(ev3_fd_pwm, &buffer, sizeof(buffer));
}

void ev3_output_clear(int mask)
{
  int8_t buffer[2];
  buffer[0] = 0xB2;
  buffer[1] = mask;
  write(ev3_fd_pwm, buffer, 2);
}
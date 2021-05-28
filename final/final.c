/*
CSCI212
Final Project
Follow The Leader
Code by: Adam Amer, Timothy Liu, Cody McKinney and Nathan White
5/28/2021
*/

/* 
compile with
------------
gcc -pthread -o leader fin.c pca9685/pca9685.c -lwiringPi

commands:
---------
^         = MOVE FORWARD
v         = MOVE BACKWARD
<         = TURN LEFT
>         = TURN RIGHT
||        = STOP
F1        = SPEED UP
F3        = SLOW DOWN
obstacle  = OBSTACLE AVOID
tracking  = FOLLOW
ctrl + c  = QUIT THE PROGRAM

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include "pca9685/pca9685.h"
#include <wiringPi.h>
#include <signal.h>
#include <pthread.h>
#define PIN_BASE 300
#define MAX_PWM 4096
#define HERTZ 50
char udp_receiver(void);
void udp_handler(char);
int fd;
#define BUFLEN 512 //Max length of buffer
#define PORT 8888 //The port on which to listen for incoming data

/* define L298N control pins in wPi system */
#define ENA 0 //left motor speed pin ENA connect to PCA9685 port 0
#define ENB 1 //right motor speed pin ENB connect to PCA9685 port 1
#define IN1 4 //Left motor IN1 connect to wPi pin# 4 (Physical 16,BCM GPIO 23)
#define IN2 5 //Left motor IN2 connect to wPi pin# 5 (Physical 18,BCM GPIO 24)
#define IN3 2 //right motor IN3 connect to wPi pin# 2 (Physical 13,BCM GPIO 27)
#define IN4 3 //right motor IN4 connect to wPi pin# 3 (Physical 15,BCM GPIO 22)

/* set servo values 180 degrees = 110 - 640 */
#define SERVO_PIN 15
#define SERVO_LEFT 470 //ultrasonic sensor facing right 585
#define SERVO_CENTER 350 //ultrasonic sensor facing front 350
#define SERVO_RIGHT 230 //ultrasonic sensor facing left 135
#define TRIGGER 28
#define ECHO 29
#define OBSTACLE 30
#define FOLLOW_RANGE 25
#define TOO_CLOSE 1210
#define high_speed 4000  // Max pulse length out of 4096
#define mid_speed 3000  // Max pulse length out of 4096
#define low_speed 2000  // Max pulse length out of 4096
#define short_delay 200
#define long_delay  300
#define extra_long_delay 400


int leftDistance = 0;
int centerDistance = 0;
int rightDistance = 0;

/* sensor values */
char sts[3] = { '0', '0', '0' };
char sensorval[3];

/* udp input handler buffers */
int speed = 2000; // Max pulse length out of 4096
char cur_status = 'E';
char pre_status;

/* preprocessor */
int increase_speed(int s);
int decrease_speed(int s);

/* setup GPIO pins */
void setup(void)
{
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(TRIGGER, OUTPUT);
  pinMode(ECHO, INPUT);
  digitalWrite(TRIGGER, LOW);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

/* initialize sonic sensor servo */
void servo_init(void)
{
  pca9685PWMWrite(fd, SERVO_PIN, 0, SERVO_LEFT);
  delay(1000);
  pca9685PWMWrite(fd, SERVO_PIN, 0, SERVO_CENTER);
  delay(1000);
  pca9685PWMWrite(fd, SERVO_PIN, 0, SERVO_RIGHT);
  delay(1000);
  pca9685PWMWrite(fd, SERVO_PIN, 0, SERVO_CENTER);
  delay(1000);
}

/* de-initialize sonic sensor servo */
void servo_deinit(void)
{
  pca9685PWMWrite(fd, SERVO_PIN, 0, SERVO_CENTER);
  pca9685PWMWrite(fd, 0, 0, 0);
}


/* measure distance */
int distance() {
  /* send trig pulse */
  digitalWrite(TRIGGER, HIGH);
  delayMicroseconds(20);
  digitalWrite(TRIGGER, LOW);

  /* wait for echo start */
  while(digitalRead(ECHO) == LOW);

  /* wait for echo end */
  long startTime = micros();
  while(digitalRead(ECHO) == HIGH);
  long travelTime = micros() - startTime;

  /* get distance in cm */
  int distance = travelTime / 58;
  if (distance >= TOO_CLOSE) distance = 1;
  printf("distance = %d\n",distance);
  return distance;
}

/* check left sonic sensor value */
void check_left(void)
{
  printf("Left ");
  pca9685PWMWrite(fd, SERVO_PIN, 0, SERVO_LEFT);
  leftDistance = distance();
  if (leftDistance < OBSTACLE) { sts[0] = '1'; } else { sts[0] = '0'; }
  delay(long_delay);
}

/* check center sonic sensor */
void check_center(void)
{
  printf("Center ");
  pca9685PWMWrite(fd, SERVO_PIN, 0, SERVO_CENTER);
  centerDistance = distance();
  if (centerDistance < OBSTACLE) { sts[1] = '1'; } else { sts[1] = '0'; }
  delay(long_delay);
}

/* check right sonic sensor */
void check_right(void)
{
  printf("Right ");
  pca9685PWMWrite(fd, SERVO_PIN, 0, SERVO_RIGHT);
  rightDistance = distance();
  if (rightDistance < OBSTACLE) { sts[2] = '1'; } else { sts[2] = '0'; }
  delay(long_delay);
}

/* join sensorval */
void join(char sarr[])
{ sprintf(sensorval, "%c%c%c", sarr[0], sarr[1], sarr[2]); }

/* check all sensor values */
void get_sensorval(void)
{
  check_left();
  check_center();
  check_right();
  join(sts);
  pca9685PWMWrite(fd, SERVO_PIN, 0, SERVO_CENTER);
}

/* go forward */
void forward(int fd, int l_speed, int r_speed)
{
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  pca9685PWMWrite(fd, ENA, 0, l_speed - 200);
  pca9685PWMWrite(fd, ENB, 0, r_speed);
  printf("Go forward\n");
}

/* go backward */
void back(int fd, int l_speed, int r_speed)
{
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  pca9685PWMWrite(fd, ENA, 0, l_speed - 200);
  pca9685PWMWrite(fd, ENB, 0, r_speed);
  printf("Go back!\n");
}

/* go left */
void left(int fd, int l_speed, int r_speed)
{
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  pca9685PWMWrite(fd, ENA, 0, l_speed - 200);
  pca9685PWMWrite(fd, ENB, 0, r_speed);
  printf("Go left!\n");
}

/* go right */
void right(int fd, int l_speed, int r_speed)
{
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  pca9685PWMWrite(fd, ENA, 0, l_speed - 200);
  pca9685PWMWrite(fd, ENB, 0, r_speed);
  printf("Go right!\n");
}

/* stop car */
void stop(int fd)
{
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  pca9685PWMWrite(fd, ENA, 0, 0);
  pca9685PWMWrite(fd, ENB, 0, 0);
  printf("Stop!\n");
}

/* ctrl-C key event handler */
void my_handler(int s)
{
  stop(fd);
  printf("Ctrl C detected %d\n", s);
  exit(1);
}

/* UDP signal struct */
struct sigaction sigIntHandler;
struct sockaddr_in si_me, si_other;
int s;
int i;
int slen = sizeof(si_other);
int recv_len;
char buf[BUFLEN];

/* die */
void die(char *s)
{ perror(s); exit(1); }

/* avoid function */
void avoid(void)
{
  printf("Avoid\n");
  get_sensorval();
  if (strcmp(sensorval, "100") == 0 ||
      strcmp(sensorval, "110") == 0)
  {
    printf("Sensorval: %s, ", sensorval);
    right(fd, speed, speed);
    printf("\n");
    delay(160);
    stop(fd);
    delay(short_delay);
  }
  if (strcmp(sensorval, "001") == 0 ||
      strcmp(sensorval, "011") == 0 ||
      strcmp(sensorval, "101") == 0 ||
      strcmp(sensorval, "111") == 0 ||
      strcmp(sensorval, "010") == 0)
  {
    printf("Sensorval: %s, ", sensorval);
    left(fd, speed, speed);
    printf("\n");
    delay(160);
    stop(fd);
    delay(short_delay);
  }
  if (strcmp(sensorval, "000") == 0)
  {
    printf("Sensorval: %s, ", sensorval);
    forward(fd, speed, speed);
    delay(400);
    stop(fd);
    delay(short_delay);
  }
}

void follow() {
  printf("Follow\n");
  pca9685PWMWrite(fd, 15, 0, SERVO_LEFT);
  delay(300);
  leftDistance = distance();

  pca9685PWMWrite(fd, 15, 0, SERVO_CENTER);
  delay(300);
  centerDistance = distance();

  pca9685PWMWrite(fd, 15, 0, SERVO_RIGHT);
  delay(300);
  rightDistance = distance

  if (centerDistance <= leftDistance && centerDistance <= rightDistance) {
    if (centerDistance < FOLLOW_RANGE) {
      back(fd, low_speed, low_speed);
      delay(long_delay);
      stop(fd);
      delay(short_delay);
    }
    else if (centerDistance > (FOLLOW_RANGE + 10)) {
      forward(fd, high_speed, high_speed);
      delay(long_delay);
      stop(fd);
      delay(short_delay);
    }
    else if (centerDistance > FOLLOW_RANGE + 5) {
      forward(fd, mid_speed, mid_speed);
      delay(long_delay);
      stop(fd);
      delay(short_delay);
    }
    else if (centerDistance >= FOLLOW_RANGE) {
      forward(fd, low_speed, low_speed);
      delay(long_delay);
      stop(fd);
      delay(short_delay);
    }
  }
  if (leftDistance < centerDistance && leftDistance < rightDistance) {
    printf("slight left");
    left(fd, 0, high_speed);
    delay(long_delay);
    stop(fd);
    delay(short_delay);
  }
  if (rightDistance < centerDistance && rightDistance < leftDistance) {
    printf("slight right\n");
    right(fd, high_speed, 0);
    delay(long_delay);
    stop(fd);
    delay(short_delay);
  }
}


/* main function */
int main(void)
{
  /* set up wiringPi GPIO */
  if(wiringPiSetup() == -1)
  {
    printf("setup wiringPi failed!\n");
    printf("please check your setup\n");
    return -1;
  }

  /* set up GPIO pin mode */
  setup();

  /* set up PCA9685 with pinbase 300 and i2c location 0x40 */
  fd = pca9685Setup(PIN_BASE, 0x40, HERTZ);
  if (fd < 0)
  { printf("Error in setup\n"); return fd; }

  /* following 4 lines define ctrl-C events */
  sigIntHandler.sa_handler = my_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);

  /* set up a UDP socket */
  if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
  { die("socket"); }

  /* zero out the structure */
  memset((char *) &si_me, 0, sizeof(si_me));
  si_me.sin_family = AF_INET;
  si_me.sin_port = htons(PORT);
  si_me.sin_addr.s_addr = htonl(INADDR_ANY);

  /* bind socket to port */
  if (bind(s, (struct sockaddr*)&si_me, sizeof(si_me)) == -1)
  { die("bind"); }

  /* Set up and initialize the sonic sensor servo */
  pca9685PWMFreq(fd, 60);
  servo_init();
  
  printf("Enter a character to control robot.\n");
  printf("^  = MOVE FORWARD\n");
  printf("v  = MOVE BACKWARD\n");
  printf("<  = TURN LEFT\n");
  printf(">  = TURN RIGHT\n");
  printf("|| = STOP\n");
  printf("F1 = SPEED UP\n");
  printf("F3 = SLOW DOWN\n");
  printf("obstacle = OBSTACLE AVOID\n");
  printf("tracking = FOLLOW\n");
  printf("ctrl + c = QUIT THE PROGRAM\n\n");
  printf("Waiting for APP command...\n");

  /* prompt statements */
  printf("Waiting for APP command...\n");
  pthread_t th1, th2;

  /* create udp receiver and udp handler threads */
  pthread_create (&th2, NULL, (void*)udp_handler, NULL);
  pthread_create (&th1, NULL, (void*)udp_receiver, NULL);
  while (1);
  close(s);
  return 0;
}

void udp_handler(char p){
  while(1)
  {
    /* input commands from phone app */
    switch(cur_status)
    {
      case 'A': forward(fd, speed, speed); cur_status = p; break;
      case 'B': back(fd, speed, speed); cur_status = p; break;
      case 'L': left(fd, speed, speed); delay(200); stop(fd); cur_status = p; break;
      case 'R': right(fd, speed, speed); delay(200); stop(fd); cur_status = p; break;
      case 'E': stop(fd); cur_status = p; break;
      case 'O': avoid(); break;
      case 'T': follow(); break;
      case 'F': speed = increase_speed(speed); cur_status = p; break;
      case 'G': stop(fd); cur_status = p; break;
      case 'H': stop(fd); cur_status = p; break;
      case 'I': speed = decrease_speed(speed); cur_status = p; break;
      case 'J': stop(fd); cur_status = p; break;
      case 'K': stop(fd); cur_status = p; break;
    }
  }
}

char udp_receiver(void){
  while(1)
  {
    fflush(stdout);

    /* try to receive some data, this is a blocking call */
    if ((recv_len = recvfrom(s, buf, BUFLEN, 0,
    (struct sockaddr *) &si_other, &slen)) == -1)
    { die("recvfrom()"); }
    cur_status = buf[0];
    pre_status = buf[0];

    /* print details of the client/peer and the data received */
    //printf("Received packet from %s:%d\n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));
    printf("Data: %c, " , cur_status);

    /* reply the client with the same data */
    /*
    if (sendto(s, buf, recv_len, 0, (struct sockaddr*) &si_other, slen) == -1)
    { die("sendto()"); }
    */
  }
  return pre_status;
}











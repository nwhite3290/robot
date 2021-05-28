#include <stdio.h>

int increase_speed(int speed)
{
  printf("speed increase\n");
  if (speed < 3900)
  {
    speed += 200;
    printf("speed is now %d\n", speed);
  }
  else
  {
    printf("speed is max at 4000\n");
  }
  return speed;
}

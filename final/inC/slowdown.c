#include <stdio.h>

int decrease_speed(int speed)
{
  printf("speed decrease\n");
  if (speed > 200)
  {
    speed -= 200;
    printf("speed is now %d\n", speed);
  }
  else
  {
    printf("speed is at minimum\n");
  }
  return speed;
}

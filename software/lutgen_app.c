#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main()
{
  int i;  
  double freqToPhaseIncerement = 65536.0 / 12500;
  
  for(i=21;i<108;i++)
  {
    double tmp;

    tmp = i-69;
    tmp = tmp / 12.0;
    tmp = pow(2,tmp);
    tmp = tmp * 440.0;    
    tmp = tmp * freqToPhaseIncerement;
    printf("%6d,",(uint16_t)tmp);
    if(((i%10) == 9) && (i != 0))
    {
      printf("\n");     
    }
  }

  printf("\n");

  return 0;
}
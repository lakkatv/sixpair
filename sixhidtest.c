/*
 * sixhidtest version 2008-05-15
 * Compile with: gcc -o sixhidtest sixhidtest.c
 * Run with: sixhidtest < /dev/hidrawX
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  unsigned char buf[128];
  int nr;

  while ( (nr=read(0, buf, sizeof(buf))) ) {
    if ( nr < 0 )
      { perror("read(stdin)"); exit(1); }
    if ( nr != 49 ) {
      fprintf(stderr, "Unsupported report length %d."
	      " Wrong hidraw device or kernel<2.6.26 ?\n", nr);
      exit(1);
    }
    int ax = buf[41]<<8 | buf[42];
    int ay = buf[43]<<8 | buf[44];
    int az = buf[45]<<8 | buf[46];
    int rz = buf[47]<<8 | buf[48]; // Needs another patch.
    printf("ax=%4d ay=%4d az=%4d\n", ax, ay, az);
    fflush(stdout);
  }
  return 0;
}

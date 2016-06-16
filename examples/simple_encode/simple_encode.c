#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gibraltar.h>
#include <time.h>
#include <sys/time.h>

#define BLOCKSIZE 104960

double get_time(void) {
    struct timeval t;
    gettimeofday(&t, NULL);
    return((double)(t.tv_sec) + (double)(t.tv_usec) * 0.001 * 0.001);
}

int main(int argc, char *argv[]) {
  double time_start_total = get_time();
  int i, j;
  int k = atoi(argv[1]);
  int m = atoi(argv[2]);
  int blocksize = BLOCKSIZE;
  int chunksize = blocksize * k;
  int paritysize = blocksize * m;
  double time_start, time_end;
  double time_start_alloc, time_start_encode;

  char *encoded;
  gib_context gc;

  time_start = get_time();
  gib_init(k, m, &gc);
  time_end = get_time();
  printf("gib_init,%10.20f\n", time_end - time_start);

  time_start = get_time();
  FILE *fin = fopen(argv[3], "rb");
  fseek(fin, 0, SEEK_END);
  long int filesize = ftell(fin);
  fseek(fin, 0, SEEK_SET);
  time_end = get_time();

  time_start_alloc = time_start = get_time();
  gib_alloc((void **)&(encoded), blocksize, &blocksize, gc, filesize);
  time_end = get_time();
  printf("gib_alloc,%10.20f\n", time_end - time_start);

  time_start_encode = time_start = get_time();
  fread(encoded, sizeof(char) * filesize, 1, fin);
  gib_copy_to_device_memory(encoded, filesize, gc);
  time_end = get_time();
  printf(" gib_copy_to_device_memory,%10.20f\n", time_end - time_start);

  fclose(fin);

  time_start = get_time();
  for (j = 0; j < filesize / chunksize; j++) {
    gib_generate(NULL, blocksize, gc, j, filesize);
  }
  time_end = get_time();
  printf(" gib_generate(core),%10.20f\n", time_end - time_start);

  FILE *fout;
  char outname[256];
  time_start = get_time();
  gib_copy_from_device_memory(encoded, filesize, gc);

/*
  for (j = 0; j < filesize / chunksize; j++) {
    for (i = 0; i < k + m; i++) {
      sprintf(outname, "%s/out.%02d", argv[4], i);
      if (j == 0) fout = fopen(outname, "wb");
      else fout = fopen(outname, "ab");
      if (i < k) fwrite(&encoded[j * chunksize + i * blocksize], blocksize, 1, fout);
      else fwrite(&encoded[j * paritysize + (i - k) * blocksize + filesize], blocksize, 1, fout);
      fclose(fout);
    }
  }
*/

  time_end = get_time();
  printf(" gib_copy_from_device_memory,%10.20f\n", time_end - time_start);
  printf("gib_generate,%10.20f\n", time_end - time_start_encode);
  printf("gib_alloc + gib_generate,%10.20f\n", time_end - time_start_alloc);

  double time_end_total = get_time();
  printf("total,%10.20f\n", time_end_total - time_start_total);

  return 0;
}

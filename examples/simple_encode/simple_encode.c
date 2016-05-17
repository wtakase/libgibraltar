#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gibraltar.h>
#include <time.h>
#include <sys/time.h>

#define CHUNKSIZE 1049600
#define BLOCKSIZE 104960

double get_time(void) {
    struct timeval t;
    gettimeofday(&t, NULL);
    return((double)(t.tv_sec) + (double)(t.tv_usec) * 0.001 * 0.001);
}

int main(int argc, char *argv[]) {
  double time_start_total = get_time();
  int i;
  int k = atoi(argv[1]);
  int m = atoi(argv[2]);
  int blocksize = BLOCKSIZE;
  double time_start, time_end;

  char *encoded;
  gib_context gc;

  time_start = get_time();
  gib_init(k, m, &gc);
  time_end = get_time();
  printf("gib_init,%10.20f\n", time_end - time_start);

  time_start = get_time();
  gib_alloc((void **)&(encoded), blocksize, &blocksize, gc);
  time_end = get_time();
  printf("gib_alloc,%10.20f\n", time_end - time_start);

  time_start = get_time();
  FILE *fin = fopen(argv[3], "rb");
  fseek(fin, 0, SEEK_END);
  long int filesize = ftell(fin);
  fseek(fin, 0, SEEK_SET);
  time_end = get_time();

  int j;
  FILE *fout;
  char outname[256];

  time_start = get_time();
  for (j = 0; j < filesize / CHUNKSIZE; j++) {
    memset(encoded, 0, CHUNKSIZE);
    for (i = 0; i < k; i++)
      fread(&encoded[i * blocksize], sizeof(char) * blocksize, 1, fin);
    gib_generate(encoded, blocksize, gc);

    for (i = 0; i < k + m; i++) {
      sprintf(outname, "%s/out.%02d", argv[4], i);
      if (j == 0) fout = fopen(outname, "wb");
      else fout = fopen(outname, "ab");
      fwrite(&encoded[i * blocksize], blocksize, 1, fout);
      fclose(fout);
    }

  }
  time_end = get_time();
  printf("gib_generate,%10.20f\n", time_end - time_start);

  fclose(fin);

  double time_end_total = get_time();
  printf("total,%10.20f\n", time_end_total - time_start_total);

  return 0;
}

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gibraltar.h>
#include <time.h>
#include <sys/time.h>

#define CHUNKSIZE 1049600
#define FETCHSIZE 512
#define STREAM_NUM 10

double get_time(void) {
    struct timeval t;
    gettimeofday(&t, NULL);
    return((double)(t.tv_sec) + (double)(t.tv_usec) * 0.001 * 0.001);
}

int aligned(int size, int align) {
  return ceil((double)size / align) * align;
}

long int alignedl(long int size, int align) {
  return ceill((double)size / align) * align;
}

int main(int argc, char *argv[]) {
  double time_start_total = get_time();
  int i, j, s;
  int k = atoi(argv[1]);
  int m = atoi(argv[2]);
  int blocksize = CHUNKSIZE / k;
  double time_start, time_end;
  double time_start_alloc;
  bool out_flag = true;
  if (argc == 6) out_flag = false;

  char *encoded;
  gib_context gc;

  time_start = get_time();
  gib_init(k, m, &gc);
  time_end = get_time();
  printf("gib_init,%10.20f\n", time_end - time_start);

  blocksize = aligned(blocksize, FETCHSIZE);
  int chunksize = blocksize * k;
  int paritysize = blocksize * m;

  time_start = get_time();
  FILE *fin = fopen(argv[3], "rb");
  fseek(fin, 0, SEEK_END);
  long int filesize = ftell(fin);
  fseek(fin, 0, SEEK_SET);
  time_end = get_time();

  int stream_num = (filesize < chunksize * STREAM_NUM) ? 1 : STREAM_NUM;

  time_start_alloc = time_start = get_time();
  gib_alloc((void **)&(encoded), blocksize, &blocksize, gc, stream_num);
  time_end = get_time();
  printf("gib_alloc,%10.20f\n", time_end - time_start);

  filesize = alignedl(alignedl(filesize, blocksize), chunksize * stream_num);

  FILE *fout;
  char outname[256];

  time_start = get_time();
  for (j = 0; j < filesize / (chunksize * stream_num); j++) {
    memset(encoded, 0, (chunksize + paritysize) * stream_num);
    fread(encoded, sizeof(char) * chunksize * stream_num, 1, fin);
    gib_generate_async(encoded, blocksize, gc);

    if (out_flag) {
      for (s = 0; s < stream_num; s++) {
        for (i = 0; i < k + m; i++) {
          sprintf(outname, "%s/out.%02d", argv[4], i);
          if (j == 0 && s == 0) fout = fopen(outname, "wb");
          else fout = fopen(outname, "ab");
          if (i < k) fwrite(&encoded[s * chunksize + i * blocksize], blocksize, 1, fout);
          else fwrite(&encoded[chunksize * stream_num + s * paritysize + (i - k) * blocksize], blocksize, 1, fout);
          fclose(fout);
        }
      }
    }
  }
  time_end = get_time();
  printf("gib_generate,%10.20f\n", time_end - time_start);
  printf("gib_alloc + gib_generate,%10.20f\n", time_end - time_start_alloc);

  fclose(fin);

  double time_end_total = get_time();
  printf("total,%10.20f\n", time_end_total - time_start_total);

  return 0;
}

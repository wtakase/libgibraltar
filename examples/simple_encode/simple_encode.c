#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gibraltar.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define CHUNKSIZE 1049600
#define FETCHSIZE 512
#define STREAM_NUM 10
#define GPU_DEVICE_NUM 4

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

gib_context gc[GPU_DEVICE_NUM];
char *encoded[GPU_DEVICE_NUM];

typedef struct {
  int k;
  int m;
  int gpu_id;
} gib_init_arg;

void *gib_init_wrapper(void *arg) {
  gib_init_arg *this_arg = (gib_init_arg *)arg;
  gib_init(this_arg->k, this_arg->m, &gc[this_arg->gpu_id], this_arg->gpu_id);
  return 0;
}

typedef struct {
  int blocksize;
  int stream_num;
  int gpu_id;
} gib_alloc_arg;

void *gib_alloc_wrapper(void *arg) {
  gib_alloc_arg *this_arg = (gib_alloc_arg *)arg;
  gib_alloc((void **)&(encoded[this_arg->gpu_id]), this_arg->blocksize, &(this_arg->blocksize), gc[this_arg->gpu_id], this_arg->stream_num);
  return 0;
}

typedef struct {
  int blocksize;
  int gpu_id;
} gib_generate_async_arg;

void *gib_generate_async_wrapper(void *arg) {
  gib_generate_async_arg *this_arg = (gib_generate_async_arg *)arg;
  gib_generate_async(encoded[this_arg->gpu_id], this_arg->blocksize, gc[this_arg->gpu_id]);
  return 0;
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

  int gpu_id;
  pthread_t pth[GPU_DEVICE_NUM];
  gib_init_arg init_arg[GPU_DEVICE_NUM];

  time_start = get_time();
  for (gpu_id = 0; gpu_id < GPU_DEVICE_NUM; gpu_id++) {
    init_arg[gpu_id].k = k;
    init_arg[gpu_id].m = m;
    init_arg[gpu_id].gpu_id = gpu_id;
    pthread_create(&pth[gpu_id], NULL, gib_init_wrapper, (void *)&init_arg[gpu_id]);
  }
  for (gpu_id = 0; gpu_id < GPU_DEVICE_NUM; gpu_id++) {
    pthread_join(pth[gpu_id], NULL);
  }
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

  int stream_num = (filesize  < chunksize * STREAM_NUM * GPU_DEVICE_NUM) ? 1 : STREAM_NUM;

  gib_alloc_arg alloc_arg[GPU_DEVICE_NUM];
  time_start_alloc = time_start = get_time();
  for (gpu_id = 0; gpu_id < GPU_DEVICE_NUM; gpu_id++) {
    alloc_arg[gpu_id].blocksize = blocksize;
    alloc_arg[gpu_id].stream_num = stream_num;
    alloc_arg[gpu_id].gpu_id = gpu_id;
    pthread_create(&pth[gpu_id], NULL, gib_alloc_wrapper, (void *)&alloc_arg[gpu_id]);
  }
  for (gpu_id = 0; gpu_id < GPU_DEVICE_NUM; gpu_id++) {
    pthread_join(pth[gpu_id], NULL);
  }
  time_end = get_time();
  printf("gib_alloc,%10.20f\n", time_end - time_start);

  filesize = alignedl(alignedl(alignedl(filesize, blocksize), chunksize * stream_num), GPU_DEVICE_NUM);

  FILE *fout;
  char outname[256];

  gib_generate_async_arg generate_async_arg[GPU_DEVICE_NUM];
  time_start = get_time();
  for (gpu_id = 0; gpu_id < GPU_DEVICE_NUM; gpu_id++) {
    generate_async_arg[gpu_id].blocksize = blocksize;
    generate_async_arg[gpu_id].gpu_id = gpu_id;
  }
  for (j = 0; j < filesize / (chunksize * stream_num * GPU_DEVICE_NUM); j++) {
    for (gpu_id = 0; gpu_id < GPU_DEVICE_NUM; gpu_id++) {
      memset(encoded[gpu_id], 0, (chunksize + paritysize) * stream_num);
      fread(encoded[gpu_id], sizeof(char) * chunksize * stream_num, 1, fin);
      pthread_create(&pth[gpu_id], NULL, gib_generate_async_wrapper, (void *)&generate_async_arg[gpu_id]);
    }
    for (gpu_id = 0; gpu_id < GPU_DEVICE_NUM; gpu_id++) {
      pthread_join(pth[gpu_id], NULL);
    }
    if (out_flag) {
      for (gpu_id = 0; gpu_id < GPU_DEVICE_NUM; gpu_id++) {
        for (s = 0; s < stream_num; s++) {
          for (i = 0; i < k + m; i++) {
            sprintf(outname, "%s/out.%02d", argv[4], i);
            if (j == 0 && s == 0 && gpu_id == 0) fout = fopen(outname, "wb");
            else fout = fopen(outname, "ab");
            if (i < k) fwrite(&encoded[gpu_id][s * chunksize + i * blocksize], blocksize, 1, fout);
            else fwrite(&encoded[gpu_id][chunksize * stream_num + s * paritysize + (i - k) * blocksize], blocksize, 1, fout);
            fclose(fout);
          }
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

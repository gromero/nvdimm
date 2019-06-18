#define _LARGEFILE64_SOURCE /* for lseek64() */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <string.h>

#define SIZE 		64*1024 // (1 page)

#define MAP_SYNC 0x80000
#define MAP_SHARED_VALIDATE 0x03

int main(int argc, char *argv[])
{
  int pid;
  char hsperfdir[] = "/mnt/pmem/test";
  char hsperffile[128];
  int fd;
  int r;
  struct stat sinfo;
  char *mmaped_address;

  pid = 706; // fake pid
  sprintf(hsperffile, "%s/%d", hsperfdir, pid);

  // check if /hsperdata_jvmtests exists in /tmp; if not, create it
  r = stat(hsperfdir, &sinfo);
  if (r == -1) {
    if (errno == ENOENT) {
      r = mkdir(hsperfdir, 0755);
      if (r == -1) {
        perror("mkdir()");
        exit(1);
      }
    } else { // another error so don't continue
      perror("stat()");
      exit(1);
    }
  }

  printf("Opening file %s ...\n", hsperffile);
  fd = open(hsperffile, O_RDWR|O_CREAT|O_NOFOLLOW, S_IRUSR|S_IWUSR);
  if (fd == -1) {
    perror("open()");
    exit(1);
  }

  r = ftruncate(fd, (off_t)0);      // zero file in case it's not empty
  if (r == -1) {
    perror("ftruncate(): 1");
    exit(1);
  }

  r = ftruncate(fd, (off_t)SIZE);   // set actual size
  if (r == -1) {
    perror("ftruncate(): 2");
    exit(1);
  }

  // touch block by writing zero
  int zero = 0;
  r = lseek64(fd, 0, SEEK_SET);
  if (r == -1) {
    perror("lseek64");
    exit(1);
  }
  r = write(fd, &zero, 1);
  if (r == -1) {
    perror("write()");
    exit(1);
  }

  mmaped_address = mmap(NULL, SIZE, PROT_READ|PROT_WRITE, MAP_SYNC|MAP_SHARED_VALIDATE, fd, 0);
  if (mmaped_address == MAP_FAILED) {
    perror("mmap()");
    exit(1);
  }
  printf("File mmap'ed to address = %p\n", mmaped_address);

  r = close(fd);
  if (r == -1) {
    perror("close()");
    exit(1);
  }

  // zero mmap'ed address
  memset((void*) mmaped_address, 0, SIZE);

  asm (
       "li 3, 'x';"
       "std 3, 0(%[pmem]);"
       "li 3, 0;"
       :
       : [pmem] "r" (mmaped_address)
       :
      );  

  exit(0);
}

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sched.h>
#include <stdint.h>
#include <sys/syscall.h>

char buf[512];

int threadTest();
int statsTest();
int dirTest();
int pipeTest();

int main(int argc, char *argv[], char *envp[]) {
   int len = 32;

   printf("Test app\n");

   threadTest();
   statsTest();
   pipeTest();
   dirTest();
 
   while(1) {
      printf("Please enter some text: ");

      char *str = fgets(buf, len, stdin);
      //int i = strchr(str, '\n');
      int len = strlen(str);
      if(len > 0) {
         str[len-1] = 0;
      }
      printf("You entered: \"%s\"\n", str);

      if(!strcmp(str, "exit")) {
         exit(0);
      }
   }

   return 0;
}


#include <pthread.h>

void * thread_fn(void * arg) {

	printf("New thread running (tid = %ld)\n", syscall(SYS_gettid));

	return 0xffffeeaa;
}



int threadTest()
{
	pthread_t thread;
	void * retval = NULL;

	pthread_create(&thread, NULL, thread_fn, NULL);

	pthread_join(thread, &retval);

	printf("Joined (%p)\n", retval);

	return 0;

}

uint8_t new_stack[8192];

int new_fn(void * arg)
{
	printf("New task running (tid = %ld)\n", syscall(SYS_gettid));
	return 0;
}



int statsTest() {
   printf("MY PROCESS ID IS: %d\n", getpid());

   return 0;
}

int pipeTest() {
   struct dirent *dp;
   int pipefd[2] = {0};
   int ret = pipe(pipefd);

   printf("TESTING PIPES\n");

   printf("Returned: (%d,%d) = %d\n", pipefd[0], pipefd[1], ret);

   char buffer[128];
   sprintf(buffer, "/proc/%d/fd", getpid());
   DIR *dir = opendir(buffer);
   printf("Opened the dir (%s): %p\n", buffer, dir);
   if(dir != NULL) {
      while ((dp=readdir(dir)) != NULL) {
         printf("%s\n", dp->d_name);
      }
      closedir(dir);
   }

   FILE *readFile = fdopen(pipefd[0], "r");
   FILE *writeFile = fdopen(pipefd[1], "w");

   printf("fdopen(): (%p,%p)\n", readFile, writeFile);

   if(readFile != NULL && writeFile != NULL) {
      int one = 0;
      int two = 0;

      fprintf(writeFile, "%d and %d\n", 234, 345);
      fflush(writeFile);
      //fclose(writeFile);

      fscanf(readFile, "%d and %d", &one, &two);
      printf("Read from pipe: %d, %d\n", one, two);

      fprintf(writeFile, "more");

      fclose(readFile);
      fclose(writeFile);
   } else {
      printf("Could not do fopen()\n");
   }

   return 0;
}

int dirTest() {
   struct dirent *dp;
   DIR *dir = opendir("/");

   printf("TESTING DIRS\n");

   printf("Opened the dir: %p\n", dir);
   while ((dp=readdir(dir)) != NULL) {
      //char *tmp;
      //tmp = path_cat(dir_path, dp->d_name);
      //printf("%s\n", tmp);
      printf("%s\n", dp->d_name);
      //free(tmp);
      //tmp=NULL;
   }
   closedir(dir);

   dir = opendir("/test");
   if(dir == NULL) {
   printf("Dir did not exist! Creating!\n");
   mkdir("/test", 0x0777);
   } else {
      printf("DIR /test exists!\n");
   }

   char buffer[128];
   sprintf(buffer, "/proc/%d", getpid());
   dir = opendir(buffer);
   printf("Opened the dir (%s): %p\n", buffer, dir);
   while ((dp=readdir(dir)) != NULL) {
      //char *tmp;
      //tmp = path_cat(dir_path, dp->d_name);
      //printf("%s\n", tmp);
      printf("%s\n", dp->d_name);
      //free(tmp);
      //tmp=NULL;
   }
   closedir(dir);

   FILE *f = fopen("/test/file.txt", "w+");
   if(f == NULL) {
      printf("ERROR: could not open file for writing!!\n");
   } else {
      fprintf(f, "before 13");
      FILE *rd = fopen("/test/file.txt", "r");
      fprintf(f, " after 14");
      fflush(f);
      if(rd == NULL) {
         printf("ERROR: could not open file for reading!!\n");
      } else {
         int before = 0;
         int after = 0;
         int ret = fscanf(rd, "before %d after %d", &before, &after);
         printf("SUCCESS: read %d, %d from 'pipe' (%d)\n", before, after, ret);
         fclose(rd);
      }
      fclose(f);
   }

   return 0;
}

#include <stdio.h>

char buf[512];

int main(int argc, char *argv[], char *envp[]) {
   int len = 32;

   while(1) {
      printf("Enter some text: ");

      char *str = fgets(buf, len, stdin);
      printf("You entered: \"%s\"\n", str);
   }

   return 0;
}

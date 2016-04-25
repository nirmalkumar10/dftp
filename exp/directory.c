#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>

int main()
{

char line[100];

getcwd(line,100);

printf("Current directory :%s\r\n",line);

DIR *dir = opendir(line);

if(dir == NULL)
printf("Not a directory");

while(1){
struct dirent *d_next = readdir(dir);
if(d_next == NULL)
break;
printf("d name:%s\r\n",d_next->d_name);
}


}

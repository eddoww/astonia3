#include <unistd.h>
#include <fcntl.h>

int main(void)
{
  int handle,val;
  char buf[256];
  
  handle=open("vers.c",O_RDWR);
  read(handle,buf,250);
  val=atoi(buf+12);
  sprintf(buf,"int VERSION=%d;\nchar *VER_DATE=__DATE__;\nchar *VER_TIME=__TIME__;\n",val+1);
  lseek(handle,0,0);
  write(handle,buf,strlen(buf));
  close(handle);
  return 0;
}

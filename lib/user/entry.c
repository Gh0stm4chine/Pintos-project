#include <syscall.h>
#include <string.h>

int main (int, char *[]);
void _start (int argc, char *argv[]);

void
_start (int argc, char *argv[]) 
{ 
  exit (main (argc, argv));
}

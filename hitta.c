#include "ncidd.h"
int main(int argc, char *argv[]) {
  char name[CIDSIZE];
  bzero(&name, sizeof(name));
  hittaAlias(name, argv[1]);
  printf("%s - %s\n", argv[1], name);
  return 0;
}
#include <iostream>
#include <cstring>
using namespace std;

#include "Disk_Class/Disk.h"
#include "define/constants.h"
#include "Buffer/BlockBuffer.h"
#include "FrontendInterface/FrontendInterface.h"

int main(int argc, char *argv[]) {
  Disk disk_run;
  StaticBuffer buffer;

  BlockBuffer blockBuffer(700);
  HeadInfo head;

  // First access
  int ret = blockBuffer.getHeader(&head);
  if (ret != SUCCESS) {
    cout << "Block access failed\n";
    return ret;
  }
  cout << "Block loaded successfully\n";

  return 0;
}
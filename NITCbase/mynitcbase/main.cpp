#include <iostream>
#include <cstring>
using namespace std;

#include "Disk_Class/Disk.h"
#include "define/constants.h"
#include "FrontendInterface/FrontendInterface.h"
#include "Buffer/BlockBuffer.h"

#include "Cache/RelCacheTable.h"
#include "Cache/AttrCacheTable.h"
#include "Cache/OpenRelTable.h"

int main(int argc, char *argv[]) {
  Disk disk_run;
  StaticBuffer buffer;
  OpenRelTable cache;

  return FrontendInterface::handleFrontend(argc, argv);
}
#include <iostream>
#include <cstring>

#include "Disk_Class/Disk.h"
#include "define/constants.h"

int main(int argc, char *argv[]) {

    Disk disk_run;

    unsigned char buffer[BLOCK_SIZE];

    // Read the first Block Allocation Map block
    Disk::readBlock(buffer, 0);

    std::cout << "First 20 entries of Block Allocation Map:\n";

    for(int i = 0; i < 20; i++)
    {
        std::cout << (int)buffer[i] << " ";
    }

    std::cout << std::endl;

    return 0;
}
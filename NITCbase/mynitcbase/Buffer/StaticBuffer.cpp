#include "StaticBuffer.h"
// the declarations for this class can be found at "StaticBuffer.h"

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];
unsigned char StaticBuffer::blockAllocMap[DISK_BLOCKS];


StaticBuffer::StaticBuffer() {
  // copy Block Allocation Map blocks from disk
  for (int i = 0; i < BLOCK_ALLOCATION_MAP_SIZE; ++i) {
    Disk::readBlock(blockAllocMap + (i* BLOCK_SIZE), i);
  }

  // initialise all blocks as free
  for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
    metainfo[bufferIndex].free = true;
    metainfo[bufferIndex].dirty = false;
    metainfo[bufferIndex].blockNum = -1;
    metainfo[bufferIndex].timeStamp = -1;
  }
}

/* At this stage, we are writing back all modified blocks on system exit */

StaticBuffer::~StaticBuffer() {
  // write-back blockAllocMap to disk
  for (int i = 0; i < BLOCK_ALLOCATION_MAP_SIZE; ++i) {
    Disk::writeBlock(blockAllocMap + (i* BLOCK_SIZE), i);
  }

  /* Iterate through all the buffer blocks and write back
     blocks which are occupied and dirty.
  */
  for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
    if (!metainfo[bufferIndex].free &&
        metainfo[bufferIndex].dirty == true) {
      
      Disk::writeBlock(blocks[bufferIndex], metainfo[bufferIndex].blockNum);
    }
  }
}


/* Get the buffer index where a particular block is stored
   or E_BLOCKNOTINBUFFER otherwise
*/
int StaticBuffer::getBufferNum(int blockNum) {
  // Check if blockNum is valid (between zero and DISK_BLOCKS) and return E_OUTOFBOUND if not valid.
  if (blockNum < 0 || blockNum > DISK_BLOCKS) {
    return E_OUTOFBOUND;
  }

  // find and return the bufferIndex which corresponds to blockNum (check metainfo)
  for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
    if (!metainfo[bufferIndex].free &&
        metainfo[bufferIndex].blockNum == blockNum)
        return bufferIndex;
  }

  return E_BLOCKNOTINBUFFER;  // if block is not in the buffer
}


int StaticBuffer::setDirtyBit(int blockNum){
    // find the buffer index corresponding to the block using getBufferNum().
    int bufferNum = StaticBuffer::getBufferNum(blockNum);

    // if block is not present in the buffer
    if (bufferNum == E_BLOCKNOTINBUFFER) {
      return E_BLOCKNOTINBUFFER;
    }

    // if blockNum is out of bound
    if (bufferNum == E_OUTOFBOUND) {
      return E_OUTOFBOUND;
    }

    metainfo[bufferNum].dirty = true;
    return SUCCESS;
}


int StaticBuffer::getFreeBuffer(int blockNum){
    // Check if blockNum is valid
    if (blockNum < 0 || blockNum >= DISK_BLOCKS) {
      return E_OUTOFBOUND;
    }

    // increase the timeStamp in metaInfo of all occupied buffers
    for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
      if (!metainfo[bufferIndex].free)
        metainfo[bufferIndex].timeStamp++;
    }

    int bufferNum = -1;  // buffer number free/freed buffer.

    // Case 1: iterate through metainfo and check if there is any buffer free
    for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
      if (metainfo[bufferIndex].free) {
        bufferNum = bufferIndex;
        break;
      }
    }

    // Case 2: if a free buffer is not available,
    if (bufferNum == -1) {
      int maxTimeStamp = -1;

      // find the buffer with the largest timestamp
      for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
        if (metainfo[bufferIndex].timeStamp > maxTimeStamp) {
          maxTimeStamp = metainfo[bufferIndex].timeStamp;
          bufferNum = bufferIndex;
        }
      }
      // IF IT IS DIRTY, write back to the disk using Disk::writeBlock()
      if (metainfo[bufferNum].dirty) {
        Disk::writeBlock(blocks[bufferNum], metainfo[bufferNum].blockNum);
      }
    }

    // update the metaInfo entry corresponding to bufferNum
    metainfo[bufferNum].free = false;
    metainfo[bufferNum].dirty = false;
    metainfo[bufferNum].blockNum = blockNum;
    metainfo[bufferNum].timeStamp = 0;

    return bufferNum;
}
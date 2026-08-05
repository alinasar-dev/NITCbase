#include <iostream>
#include <cstring>

#include "Disk_Class/Disk.h"
#include "define/constants.h"
#include "Buffer/BlockBuffer.h"
#include "FrontendInterface/FrontendInterface.h"

int main(int argc, char *argv[]) {
  Disk disk_run;

  // create objects for the relation catalog and attribute catalog
  RecBuffer relCatBuffer(RELCAT_BLOCK);

  HeadInfo relCatHeader;
  HeadInfo attrCatHeader;

  // load the headers of both the blocks into relCatHeader and attrCatHeader.
  // (we will implement these functions later)
  relCatBuffer.getHeader(&relCatHeader);

  for (int i = 0; i < relCatHeader.numEntries; i++) {

    Attribute relCatRecord[RELCAT_NO_ATTRS]; // will store the record from the relation catalog
    relCatBuffer.getRecord(relCatRecord, i);

    printf("Relation: %s\n", relCatRecord[RELCAT_REL_NAME_INDEX].sVal);

    
    // UPDATED CODE FOR STAGE 2 EXERCISE: (read across multiple blocks of the attribute catalog)
    int currentBlock = ATTRCAT_BLOCK;
    
    while (currentBlock != INVALID_BLOCKNUM)
    {
        RecBuffer attrCatBuffer(currentBlock);
        HeadInfo attrCatHeader;

        attrCatBuffer.getHeader(&attrCatHeader);

        for (int j = 0; j < attrCatHeader.numEntries; j++) {
            Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
            attrCatBuffer.getRecord(attrCatRecord, j);

            if (strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal,
                    relCatRecord[RELCAT_REL_NAME_INDEX].sVal) == 0) 
            {
                // Modification for STAGE 2 Exercise Q2: 
                // In "Students" relation, Change the attribute name "Class" to "Batch"
                if (strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, "Students") == 0 &&
                    strcmp(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, "Class") == 0)
                {
                    strcpy(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, "Batch");
                    attrCatBuffer.setRecord(attrCatRecord, j);
                }

                const char *attrType = attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal == NUMBER ? "NUM" : "STR";
                printf("  %s: %s\n", attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attrType);
            }
        }
        currentBlock = attrCatHeader.rblock;
    }
    printf("\n");
  }
  return 0;
}
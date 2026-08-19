#include "BlockAccess.h"

#include <cstring>

RecId BlockAccess::linearSearch(int relId, char attrName[ATTR_SIZE], union Attribute attrVal, int op) {
    // get the previous search index of the relation relId from the relation cache
    // (use RelCacheTable::getSearchIndex() function)
    RecId prevRecId;
    int ret = RelCacheTable::getSearchIndex(relId, &prevRecId);
    if (ret != SUCCESS)
        return {-1, -1};

    // let block and slot denote the record id of the record being currently checked
    int block, slot;

    // if the current search index record is invalid(i.e. both block and slot = -1)
    if (prevRecId.block == -1 && prevRecId.slot == -1) {
        // (no hits from previous search; search should start from the first record itself)
        // get the first record block of the relation from the relation cache

        RelCatEntry relCatEntry;
        ret = RelCacheTable::getRelCatEntry(relId, &relCatEntry);
        if (ret != SUCCESS)
            return {-1, -1};

        // Start from the first record block of the relation.
        block = relCatEntry.firstBlk;
        slot = 0;
    }
    else {
        // (there is a hit from previous search; search should start from the record next to the search index record)
        block = prevRecId.block;

        // Start searching from next slot
        slot = prevRecId.slot +1;
    }


    /* The following code searches for the next record in the relation that satisfies the given condition
       We start from the record id (block, slot) and iterate over the remaining records of the relation
    */
    while (block != -1)
    {
        /* create a RecBuffer object for block (use RecBuffer Constructor for existing block) */
        RecBuffer recBuffer(block);

        // get the record with id (block, slot) using RecBuffer::getRecord()
        // get header of the block using RecBuffer::getHeader() function
        HeadInfo head;
        ret = recBuffer.getHeader(&head);
        if (ret != SUCCESS)
            return {-1, -1};

        // get slot map of the block using RecBuffer::getSlotMap() function
        unsigned char slotMap[SLOTMAP_SIZE_RELCAT_ATTRCAT];
        ret = recBuffer.getSlotMap(slotMap);
        if (ret != SUCCESS)
            return {-1, -1};

        // If slot >= the number of slots per block(i.e. no more slots in this block)
        // move to the next block and start from slot 0
        if (slot >= head.numSlots) {
            block = head.rblock;    // update block = right block of block
            slot = 0;   // update slot = 0
            continue;  // continue to the beginning of this while loop
        }

        // if slot is free skip the loop
        if (slotMap[slot] == SLOT_UNOCCUPIED) {
            slot++;
            continue;
        }

        // compare record's attribute value to the the given attrVal as below:
        /*
            firstly get the attribute offset for the attrName attribute from the attribute cache entry 
            of the relation using AttrCacheTable::getAttrCatEntry()
        */
        RelCatEntry relCatEntry;
        RelCacheTable::getRelCatEntry(relId, &relCatEntry);
        if (ret != SUCCESS)
            return {-1, -1};
        
        AttrCatEntry attrCatEntry;
        ret = AttrCacheTable::getAttrCatEntry( relId, attrName, &attrCatEntry);
        if (ret != SUCCESS)
            return {-1, -1};
        
        /* use the attribute offset to get the value of the attribute from
            current record */
        Attribute record[relCatEntry.numAttrs];
        ret = recBuffer.getRecord(record, slot);
        if (ret != SUCCESS)
            return {-1, -1};
        
        Attribute recordAttr = record[attrCatEntry.offset];

        // set cmpVal using compareAttrs() -> will store the difference between the attributes
        int cmpVal = compareAttrs(
            recordAttr,
            attrVal,
            attrCatEntry.attrType
        );

        /* Next task is to check whether this record satisfies the given condition.
           It is determined based on the output of previous comparison and the op value received.
        */
        if ((op == NE && cmpVal != 0) ||    // if op is "not equal to"
            (op == LT && cmpVal < 0) ||     // if op is "less than"
            (op == LE && cmpVal <= 0) ||    // if op is "less than or equal to"
            (op == EQ && cmpVal == 0) ||    // if op is "equal to"
            (op == GT && cmpVal > 0) ||     // if op is "greater than"
            (op == GE && cmpVal >= 0)       // if op is "greater than or equal to"
        ) {
            /*
            set the search index in the relation cache as the record id of the record that satisfies the given condition
            (use RelCacheTable::setSearchIndex function)
            */
            RecId searchIndex = {block, slot};
            ret = RelCacheTable::setSearchIndex(relId, &searchIndex);
            if (ret != SUCCESS)
                return {-1, -1};

            return RecId{block, slot};
        }

        slot++;
    }

    // no record in the relation with Id relid satisfies the given condition
    return RecId{-1, -1};
}
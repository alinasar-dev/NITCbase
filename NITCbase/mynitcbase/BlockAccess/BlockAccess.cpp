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
        unsigned char slotMap[head.numSlots];
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
        ret = RelCacheTable::getRelCatEntry(relId, &relCatEntry);
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



int BlockAccess::renameRelation(char oldName[ATTR_SIZE], char newName[ATTR_SIZE]) {
    // reset the searchIndex of the relation catalog
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute newRelationName;    // set newRelationName with newName
    strcpy(newRelationName.sVal, newName);

    // search the relation catalog for an entry with "RelName" = newRelationName
    RecId recId = BlockAccess::linearSearch(
        RELCAT_RELID,
        (char *) "RelName",
        newRelationName,
        EQ
    );

    // If relation with name newName already exists (result of linearSearch is not {-1, -1})
    if (recId.block != -1 && recId.slot != -1)
        return E_RELEXIST;


    // reset the searchIndex of the relation catalog
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute oldRelationName;    // set oldRelationName with oldName
    strcpy(oldRelationName.sVal, oldName);

    // search the relation catalog for an entry with "RelName" = oldRelationName
    recId = BlockAccess::linearSearch(
        RELCAT_RELID,
        (char *) "RelName",
        oldRelationName,
        EQ
    );

    // If relation with name oldName does not exist
    if (recId.block == -1 && recId.slot == -1)
        return E_RELNOTEXIST;
    

    /* get the relation catalog record of the relation to rename */
    RecBuffer relCatBlock(recId.block);
    Attribute relCatEntryRecord[RELCAT_NO_ATTRS];

    int ret = relCatBlock.getRecord(relCatEntryRecord, recId.slot);
    if (ret != SUCCESS) {
        return ret;
    }

    
    // Save the number of attributes before modifying the record
    int numAttrs = (int) relCatEntryRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal;

    /* update the relation name attribute in the record with newName */
    strcpy(relCatEntryRecord[RELCAT_REL_NAME_INDEX].sVal, newName);

    // set back the record value using RecBuffer.setRecord
    ret = relCatBlock.setRecord(relCatEntryRecord, recId.slot);
    if (ret != SUCCESS) {
        return ret;
    }


    /*
    update all the attribute catalog entries in the attribute catalog corresponding
    to the relation with relation name oldName to the relation name newName
    */

    // reset the searchIndex of the attribute catalog
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    for (int i = 0; i < numAttrs; i++) {
        // Search for the next Attribute belongs to oldName
        RecId attrRecId = BlockAccess::linearSearch(
            ATTRCAT_RELID,
            (char *) "RelName",
            oldRelationName,
            EQ
        );

        if (attrRecId.block == -1 && attrRecId.slot == -1)
            break;  //exit loop
        
        // Read the corresponding Attribute Catalog record
        RecBuffer attrCatBlock(attrRecId.block);
        Attribute attrCatEntryRecord[ATTRCAT_NO_ATTRS];

        ret = attrCatBlock.getRecord(attrCatEntryRecord, attrRecId.slot);
        if (ret != SUCCESS) {
            return ret;
        }

        // Update the relation name in the Attribute Catalog
        strcpy(attrCatEntryRecord[ATTRCAT_REL_NAME_INDEX].sVal, newName);

        ret = attrCatBlock.setRecord(attrCatEntryRecord,attrRecId.slot);
        if (ret != SUCCESS) {
            return ret;
        }
    }

    return SUCCESS;
}


int BlockAccess::renameAttribute(char relName[ATTR_SIZE], char oldName[ATTR_SIZE], char newName[ATTR_SIZE]) {
    // reset the searchIndex of the relation catalog
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute relationName;    // set relationName with relName
    strcpy(relationName.sVal, relName);

    // search the relation catalog for an entry with "RelName" = relationName
    RecId relCatRecId = BlockAccess::linearSearch(
        RELCAT_RELID,
        (char *) "RelName",
        relationName,
        EQ
    );

    // If relation with name relName does not exist
    if (relCatRecId.block == -1 && relCatRecId.slot == -1)
        return E_RELNOTEXIST;


    // reset the searchIndex of the attribute catalog
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    // RecId of the attribute catalog entry to rename
    RecId attrToRenameRecId = {-1, -1};
    Attribute attrCatEntryRecord[ATTRCAT_NO_ATTRS];

    /*
    search the attribute catalog for the attribute with name oldName
    corresponding to the relation with name relName
    */

    while (true) {
        // Search for the next Attribute belongs to relName
        RecId attrCatRecId = BlockAccess::linearSearch(
            ATTRCAT_RELID,
            (char *) "RelName",
            relationName,
            EQ
        );

        // If there are no more attributes left to check
        if (attrCatRecId.block == -1 && attrCatRecId.slot == -1)
            break;

        // Read the corresponding Attribute Catalog record
        RecBuffer attrCatBlock(attrCatRecId.block);

        int ret = attrCatBlock.getRecord(
            attrCatEntryRecord,
            attrCatRecId.slot
        );

        if (ret != SUCCESS) {
            return ret;
        }

        // If Attribute Name is oldName, save its RecId
        if (strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, oldName) == 0) {
            attrToRenameRecId = attrCatRecId;
        }

        // If Attribute Name is newName, return attribute already exists
        if (strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,newName) == 0) {
            return E_ATTREXIST;
        }
    }


    // If attribute with name oldName does not exist
    if (attrToRenameRecId.block == -1 && attrToRenameRecId.slot == -1)
        return E_ATTRNOTEXIST;


    /* get the attribute catalog record of the attribute to rename */
    RecBuffer attrCatBlock(attrToRenameRecId.block);

    int ret = attrCatBlock.getRecord(attrCatEntryRecord, attrToRenameRecId.slot
    );

    if (ret != SUCCESS) {
        return ret;
    }


    /* update the attribute name in the record with newName */
    strcpy(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newName);

    // set back the record value using RecBuffer.setRecord
    ret = attrCatBlock.setRecord(attrCatEntryRecord, attrToRenameRecId.slot);
    if (ret != SUCCESS) {
        return ret;
    }

    return SUCCESS;
}
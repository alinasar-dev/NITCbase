#include "BPlusTree.h"

#include <iostream>
#include <queue>
using namespace std;

extern int comparisonsPerBlock[DISK_BLOCKS];

#include <cstring>

// Stage 10, Q2: Count comparisons in B+ Tree search.
int bPlusComparisons = 0;

RecId BPlusTree::bPlusSearch(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int op) {
    
    // Get the previous search position for this attribute.
    IndexId searchIndex;
    int ret = AttrCacheTable::getSearchIndex(relId, attrName, &searchIndex);
    if (ret != SUCCESS) {
        return {-1, -1};
    }

    // Get the attribute catalog entry.
    AttrCatEntry attrCatEntry;
    ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    if (ret != SUCCESS)
        return {-1, -1};

    // declare variables block and index which will be used during search
    int block, index;
    int attrType = attrCatEntry.attrType;

    // Print the root block of the B+ Tree
    static bool rootPrinted = false;

    if (!rootPrinted) {

        int rootBlock = attrCatEntry.rootBlock;

        cout << "\nRoot Block: " << rootBlock << endl;

        int rootType = StaticBuffer::getStaticBlockType(rootBlock);
        HeadInfo rootHead;

        if (rootType == IND_INTERNAL) {
            IndInternal root(rootBlock);
            root.getHeader(&rootHead);

            for (int i = 0; i < rootHead.numEntries; i++) {
                InternalEntry entry;
                root.getEntry(&entry, i);

                cout << "Key: ";

                if (attrType == NUMBER)
                    cout << entry.attrVal.nVal;
                else
                    cout << entry.attrVal.sVal;

                cout << endl;
            }
        }
        else if (rootType == IND_LEAF) {
            IndLeaf root(rootBlock);
            root.getHeader(&rootHead);

            for (int i = 0; i < rootHead.numEntries; i++) {
                Index entry;
                root.getEntry(&entry, i);

                cout << "Key: ";

                if (attrType == NUMBER)
                    cout << entry.attrVal.nVal;
                else
                    cout << entry.attrVal.sVal;

                cout << endl;
            }
        }

        rootPrinted = true;
    }

    if (searchIndex.block == -1 && searchIndex.index == -1) {
        // (search is done for the first time)
        // start the search from the first entry of root.
        block = attrCatEntry.rootBlock;
        index = 0;

        if (block == -1) {      // attrName doesn't have a B+ tree
            return RecId {-1, -1};
        }
    }
    else {
        /* a valid searchIndex points to an entry in the leaf index of the attribute's
           B+ Tree which had previously satisfied the op for the given attrVal. */

        // Continue after the previous matching leaf entry.
        block = searchIndex.block;
        index = searchIndex.index + 1;  // search is resumed from the next index.

        IndLeaf leaf(block);    // load block into leaf
        HeadInfo leafHead;      // used to hold the header of leaf.

        // load header into leafHead
        ret = leaf.getHeader(&leafHead);
        if (ret != SUCCESS)
            return {-1, -1};

        if (index >= leafHead.numEntries) {
            /* (all the entries in the block has been searched); 
            search from the beginning of the next leaf index block. */
            block = leafHead.rblock;
            index = 0;

            if (block == -1) {
                // (end of linked list reached - the search is done.)
                return RecId{-1, -1};
            }
        }
    }

    
    /* Traverse internal nodes:
        * LT, LE and NE start at the leftmost leaf.
        * EQ, GT and GE find the appropriate child for attrVal.
    */
    /* This section is only needed when:
        - search restarts from the root block
        - root is not a leaf
        (If there was a valid search index, then we are already at a leaf block
        and the test condition in the following loop will fail)
    */
    while(true) {
        int blockType = StaticBuffer::getStaticBlockType(block);

        if (blockType == IND_LEAF) break;
        if (blockType != IND_INTERNAL) return {-1, -1};

        // load the block & header
        IndInternal internalBlock(block);
        HeadInfo internalHead;

        ret = internalBlock.getHeader(&internalHead);
        if (ret != SUCCESS)
            return {-1, -1};
        
        if (internalHead.numEntries <= 0)
            return {-1, -1};

        
        if (op == NE || op == LT || op == LE) {
            /*
                - NE: need to search the entire linked list of leaf indices of the B+ Tree,
                starting from the leftmost leaf index. Thus, always move to the left.

                - LT and LE: attribute values are in ascending order in the leafs of B+ Tree. 
                Values that satisfy these conditions (if exists), will always be found in the 
                left-most leaf index. Thus, always move to the left.
            */

            // load entry in the first slot
            InternalEntry firstEntry;

            ret = internalBlock.getEntry(&firstEntry, 0);
            if (ret != SUCCESS)
                return {-1, -1};

            block = firstEntry.lChild;
            continue;
        }
        else {
            /*
              - EQ, GT and GE: move to the left child of the first entry that is greater than (or equal to) attrVal
                (we are trying to find first entry that satisfies the condition. since the values are in ascending order 
                we move to the left child which might contain more entries that satisfy the condition)
            */

            /*
             traverse through all entries of internalBlk and find an entry that atisfies the condition.
             - if op == EQ or GE, then intEntry.attrVal >= attrVal
             - if op == GT, then intEntry.attrVal > attrVal
             Hint: the helper function compareAttrs() can be used for comparing
            */

            bool found = false;

            for (int i = 0; i < internalHead.numEntries; i++) {
                InternalEntry intEntry;

                ret = internalBlock.getEntry(&intEntry, i);
                if (ret != SUCCESS)
                    return {-1, -1};

                // Internal-node comparison
                bPlusComparisons++;
                comparisonsPerBlock[block]++;

                int cmpVal = compareAttrs(intEntry.attrVal, attrVal, attrType);

                if (((op == EQ || op == GE) && cmpVal >= 0) ||
                    (op == GT && cmpVal > 0)) {

                    // (if found) move to the left child of that entry
                    found = true;
                    block = intEntry.lChild;
                    break;
                }
            }

            if (!found) {
                // move to the right child of the last entry of the block
                // i.e numEntries - 1 th entry of the block
                InternalEntry lastEntry;
                ret = internalBlock.getEntry(&lastEntry, internalHead.numEntries - 1);
                if (ret != SUCCESS)
                    return {-1, -1};
                
                block =  lastEntry.rChild;
            }
        }
    }

    // NOTE: `block` now has the block number of a leaf index block.

    /******  Identify the first leaf index entry from the current 
             position that satisfies our condition (moving right)   ******/

    while (block != -1) {
        // load the block into leafBlk
        IndLeaf leafBlk(block);
        HeadInfo leafHead;

        // load the header to leafHead
        ret = leafBlk.getHeader(&leafHead);
        if (ret != SUCCESS)
            return {-1, -1};

        Index leafEntry;    // used to store an entry from leafBlk

        while (index < leafHead.numEntries) {
            // load entry corresponding to block and index into leafEntry
            ret = leafBlk.getEntry(&leafEntry, index);
            if (ret != SUCCESS)
                return {-1, -1};

            // Leaf-entry comparison
            bPlusComparisons++;
            comparisonsPerBlock[block]++;
            
            int cmpVal = compareAttrs(leafEntry.attrVal, attrVal, attrType);

            if (
                (op == EQ && cmpVal == 0) ||
                (op == LE && cmpVal <= 0) ||
                (op == LT && cmpVal < 0) ||
                (op == GT && cmpVal > 0) ||
                (op == GE && cmpVal >= 0) ||
                (op == NE && cmpVal != 0)
            ) {
                // (entry satisfying the condition found)

                // set search index to {block, index}
                IndexId nextSearchIndex = {block, index};

                ret = AttrCacheTable::setSearchIndex(relId, attrName, &nextSearchIndex);
                if (ret != SUCCESS)
                    return {-1, -1};

                // return the recId
                return RecId{leafEntry.block, leafEntry.slot};

            } else if ((op == EQ || op == LE || op == LT) && cmpVal > 0) {
                /* future entries will not satisfy EQ, LE, LT since the values
                   are arranged in ascending order in the leaves */

                return RecId{-1, -1};
            }

            ++index;    // search next index.
        }

        // Only NE needs to scan every leaf looking for a match.
        if (op != NE) {
            break;
        }
        
        // next block in the linked list
        block = leafHead.rblock;
        index = 0;
    }

    return RecId{-1, -1};
}



int BPlusTree::bPlusCreate(int relId, char attrName[ATTR_SIZE]) {
    // Catalog relations cannot have indexes
    if (relId == RELCAT_RELID || relId == ATTRCAT_RELID) {
        return E_NOTPERMITTED;
    }

    // get the attribute catalog entry of attribute `attrName`
    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    if (ret != SUCCESS) {
        return ret;
    }

    // If an index already exists, nothing more to do
    if (attrCatEntry.rootBlock != -1) {
        return SUCCESS;
    }


    /****** Creating a new B+ Tree ******/

    // get a free leaf block using constructor 1 to allocate a new block
    IndLeaf rootBlockBuf;
    int rootBlock = rootBlockBuf.getBlockNum();

    if (rootBlock == E_DISKFULL) {    // if no disk space for creating an index
        return E_DISKFULL;
    }

    // Update the root block in the attribute cache
    attrCatEntry.rootBlock = rootBlock;
    ret = AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);
    if (ret != SUCCESS) {
        rootBlockBuf.releaseBlock();
        return ret;
    }

    // load the relation catalog entry into relCatEntry
    RelCatEntry relCatEntry;
    ret = RelCacheTable::getRelCatEntry(relId, &relCatEntry);
    
    if (ret != SUCCESS) {
        BPlusTree::bPlusDestroy(rootBlock);
        attrCatEntry.rootBlock = -1;
        AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);
        return ret;
    }
    
    /***** Traverse all the blocks in the relation and insert them one by one into the B+ Tree *****/
    int block = relCatEntry.firstBlk;

    while (block != -1) {
        // declare a RecBuffer object for `block`
        RecBuffer recBuffer(block);

        unsigned char slotMap[relCatEntry.numSlotsPerBlk];
        ret = recBuffer.getSlotMap(slotMap);
        if (ret != SUCCESS) {
            return ret;
        }

        // visit each occupied slot of the block
        for (int slot = 0; slot < relCatEntry.numSlotsPerBlk; slot++) {

            if (slotMap[slot] == SLOT_OCCUPIED) {
                // Read the record
                Attribute record[relCatEntry.numAttrs];

                ret = recBuffer.getRecord(record, slot);
                if (ret != SUCCESS) {
                    return ret;
                }
    
                // Record ID of this record
                RecId recId = {block, slot};
    
                // Insert the attribute value into the B+ tree
                // (NOTE: bPlusInsert will destroy any existing bplus tree if insert fails i.e when disk is full)
                ret = BPlusTree::bPlusInsert(relId, attrName, record[attrCatEntry.offset], recId);
    
                if (ret == E_DISKFULL) {    // (unable to get enough blocks to build B+ Tree)
                    return E_DISKFULL;
                }
            }
        }

        // Move to the next record block
        HeadInfo blockHeader;
        recBuffer.getHeader(&blockHeader);

        block = blockHeader.rblock;
    }

    return SUCCESS;
}



int BPlusTree::bPlusDestroy(int rootBlockNum) {
    if (rootBlockNum < 0 || rootBlockNum >= DISK_BLOCKS) {
        return E_OUTOFBOUND;
    }

    // Get the type of this block
    int blockType = StaticBuffer::getStaticBlockType(rootBlockNum);

    if (blockType == IND_LEAF) {
        // declare an instance of IndLeaf for rootBlockNum
        IndLeaf leafBlock(rootBlockNum);
        // release the block
        leafBlock.releaseBlock();
        
        return SUCCESS;

    }
    else if (blockType == IND_INTERNAL) {
        // declare an instance of IndInternal for rootBlockNum
        IndInternal internalBlock(rootBlockNum);
        HeadInfo blockHeader;

        int ret = internalBlock.getHeader(&blockHeader);
        if (ret != SUCCESS) {
            return ret;
        }

        /* iterate through all the entries of the internalBlk and destroy the lChild
        of the first entry and rChild of all entries using BPlusTree::bPlusDestroy(). */
        
        // An internal block with no entries has no child pointers
        if (blockHeader.numEntries > 0) {
            // Destroy the leftmost child
            InternalEntry entry;
            ret = internalBlock.getEntry(&entry, 0);
            if (ret != SUCCESS) {
                return ret;
            }

            ret = BPlusTree::bPlusDestroy(entry.lChild);
            if (ret != SUCCESS) {
                return ret;
            }

            // Destroy the right child of each entry
            for (int i = 0; i < blockHeader.numEntries; i++) {

                ret = internalBlock.getEntry(&entry, i);
                if (ret != SUCCESS) {
                    return ret;
                }

                ret = BPlusTree::bPlusDestroy(entry.rChild);
                if (ret != SUCCESS) {
                    return ret;
                }
            }
        }

        // All children have been destroyed; release this block
        internalBlock.releaseBlock();

        return SUCCESS;
    }
    else {
        // (block is not an index block.)
        return E_INVALIDBLOCK;
    }
}


/* -------------------- B+ Tree: Insert functions -------------------- */

int BPlusTree::bPlusInsert(int relId, char attrName[ATTR_SIZE], Attribute attrVal, RecId recId) {
    // Get the attribute catalog entry
    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    if (ret != SUCCESS) {
        return ret;
    }

    int rootBlock = attrCatEntry.rootBlock;

    if (rootBlock == -1) {
        return E_NOINDEX;
    }

    // find the leaf block to which insertion is to be done
    int leafBlock = BPlusTree::findLeafToInsert(rootBlock, attrVal, attrCatEntry.attrType);

    // insert attrVal & recId to the leaf block at blockNum using insertIntoLeaf() function.

    // Prepare the index entry
    Index indexEntry;
    indexEntry.attrVal = attrVal;
    indexEntry.block = recId.block;
    indexEntry.slot = recId.slot;

    ret = BPlusTree::insertIntoLeaf(relId, attrName, leafBlock, indexEntry);

    // If disk is full, destroy the incomplete index
    if (ret == E_DISKFULL) {
        // destroy the existing B+ tree
        BPlusTree::bPlusDestroy(rootBlock);

        // update the rootBlock
        attrCatEntry.rootBlock = -1;
        AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);

        return E_DISKFULL;
    }

    return ret;
}


int BPlusTree::findLeafToInsert(int rootBlock, Attribute attrVal, int attrType) {
    int blockNum = rootBlock;

    // Traverse internal nodes until a leaf is reached
    while (StaticBuffer::getStaticBlockType(blockNum) != IND_LEAF) {

        // declare an IndInternal object for block
        IndInternal internalBlock(blockNum);

        HeadInfo blockHeader;
        int ret = internalBlock.getHeader(&blockHeader);
        if (ret != SUCCESS) {
            return ret;
        }

        /* iterate through all the entries, to find the first entry whose
             attribute value >= value to be inserted. */
        bool found = false;

        for (int i = 0; i < blockHeader.numEntries; i++) {
            InternalEntry entry;
            ret = internalBlock.getEntry(&entry, i);
            if (ret != SUCCESS) {
                return ret;
            }

            if (compareAttrs(entry.attrVal, attrVal, attrType) >= 0) {
                // Move to left child of this entry
                blockNum = entry.lChild;

                found = true;
                break;
            }
        }

        if (!found) {
            // set blockNum = rightmost child of the block
            InternalEntry lastEntry;
            ret = internalBlock.getEntry(&lastEntry, blockHeader.numEntries -1);
            if (ret != SUCCESS) {
                return ret;
            }

            blockNum = lastEntry.rChild;
        }
    }

    return blockNum;
}


int BPlusTree::insertIntoLeaf(int relId, char attrName[ATTR_SIZE], int blockNum, Index indexEntry) {
    // get the attribute cache entry corresponding to attrName
    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    if (ret != SUCCESS) {
        return ret;
    }

    // declare an IndLeaf instance for the block using appropriate constructor
    IndLeaf leafBlock(blockNum);

    HeadInfo blockHeader;
    ret = leafBlock.getHeader(&blockHeader);
    if (ret != SUCCESS) {
        return ret;
    }

    // the following variable will be used to store a list of index entries with
    // existing indices + the new index to insert (in sorted order)
    Index indices[blockHeader.numEntries + 1];

    /*
    Iterate through all the entries in the block and copy them to the array indices.
    Also insert `indexEntry` at appropriate position in the indices array maintaining
    the ascending order.
    */

    int i = 0, j = 0;

    while (i < blockHeader.numEntries) {
        Index currentEntry;
        ret = leafBlock.getEntry(&currentEntry, i);
        if (ret != SUCCESS) {
            return ret;
        }

        if (compareAttrs(indexEntry.attrVal, currentEntry.attrVal, attrCatEntry.attrType) < 0) {
            indices[j++] = indexEntry;
            break;
        }
        indices[j++] = currentEntry;
        i++;
    }

    // if the new entry belongs at the end
    if (i == blockHeader.numEntries) {
        indices[j++] = indexEntry;
    }

    // copy remaining existing entries
    while (i < blockHeader.numEntries) {
        ret = leafBlock.getEntry(&indices[j], i);
        if (ret != SUCCESS)
            return ret;
        
        i++; j++;
    }


    // CASE 1: leaf block has not reached max limit
    
    if (blockHeader.numEntries != MAX_KEYS_LEAF) {
        // increment blockHeader.numEntries and update the header of block
        blockHeader.numEntries++;

        ret = leafBlock.setHeader(&blockHeader);
        if (ret != SUCCESS) {
            return ret;
        }
        
        // populate the leaf with sorted entries
        for (int k = 0; k < blockHeader.numEntries; k++) {
            ret = leafBlock.setEntry(&indices[k], k);

            if (ret != SUCCESS) {
                return ret;
            }
        }

        return SUCCESS;
    }


    // CASE 2: Max limit exceeded for a single leaf index block. 
    // (Therefore, we will need to split the entries)

    int newRightBlk = splitLeaf(blockNum, indices);

    if (newRightBlk == E_DISKFULL) {
        return E_DISKFULL;
    }

    /* the current leaf block was not the root */
    if (blockHeader.pblock != -1) {
        // insert the middle value from `indices` into the parent block
        // (i.e the last value of the left block)

        // the middle value will be at index 31 (given by constant MIDDLE_INDEX_LEAF)

        InternalEntry newEntry;
        newEntry.attrVal = indices[MIDDLE_INDEX_LEAF].attrVal;
        newEntry.lChild = blockNum;
        newEntry.rChild = newRightBlk;

        // insert into the parent block
        ret = BPlusTree::insertIntoInternal(relId, attrName, blockHeader.pblock, newEntry);

    } else {
        // the current block was the root block and is now split. a new internal index
        // block needs to be allocated and made the root of the tree.

        ret = createNewRoot(relId, attrName, indices[MIDDLE_INDEX_LEAF].attrVal, blockNum, newRightBlk);
    }

    if (ret != SUCCESS)
        return ret;

    return SUCCESS;
}


int BPlusTree::splitLeaf(int leafBlockNum, Index indices[]) {
    // declare rightBlk, an instance of IndLeaf using constructor 1 to obtain new
    // leaf index block that will be used as the right block in the splitting
    IndLeaf rightBlk;

    // declare leftBlk, an instance of IndLeaf using constructor 2 to read from existing leaf block
    IndLeaf leftBlk(leafBlockNum);

    int rightBlkNum = rightBlk.getBlockNum();
    int leftBlkNum = leftBlk.getBlockNum();

    if (rightBlkNum == E_DISKFULL) {
        //(failed to obtain a new leaf index block because the disk is full)
        return E_DISKFULL;
    }

    // get the headers of left block and right block
    HeadInfo leftBlkHeader, rightBlkHeader;
    int ret = leftBlk.getHeader(&leftBlkHeader);
    if (ret != SUCCESS) {
        rightBlk.releaseBlock();
        return ret;
    }

    ret = rightBlk.getHeader(&rightBlkHeader);
    if (ret != SUCCESS) {
        rightBlk.releaseBlock();
        return ret;
    }

    // 1) set the right block header
    rightBlkHeader.numEntries = (MAX_KEYS_LEAF + 1) / 2;
    rightBlkHeader.pblock = leftBlkHeader.pblock;
    rightBlkHeader.lblock = leftBlkNum;
    rightBlkHeader.rblock = leftBlkHeader.rblock;

    ret = rightBlk.setHeader(&rightBlkHeader);

    if (ret != SUCCESS) {
        rightBlk.releaseBlock();
        return ret;
    }

    // 2) set leftBlkHeader with the following values
    leftBlkHeader.numEntries = (MAX_KEYS_LEAF + 1) / 2;
    leftBlkHeader.rblock = rightBlkNum;

    ret = leftBlk.setHeader(&leftBlkHeader);

    if (ret != SUCCESS) {
        rightBlk.releaseBlock();
        return ret;
    }

    // set the first 32 entries of leftBlk = the first 32 entries of indices array
    // and set the first 32 entries of newRightBlk = the next 32 entries
    for (int i = 0; i < (MAX_KEYS_LEAF + 1) / 2; i++) {
        ret = leftBlk.setEntry(&indices[i], i);
        if (ret != SUCCESS) {
            return ret;
        }
    }

    for (int i = (MAX_KEYS_LEAF + 1) / 2; i < MAX_KEYS_LEAF + 1; i++) {

        ret = rightBlk.setEntry(&indices[i], i - (MAX_KEYS_LEAF + 1) / 2);
        if (ret != SUCCESS) {
            return ret;
        }
    }

    return rightBlkNum;
}


int BPlusTree::insertIntoInternal(int relId, char attrName[ATTR_SIZE], int intBlockNum, InternalEntry intEntry) {
    // get the attribute cache entry corresponding to attrName
    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    if (ret != SUCCESS) {
        return ret;
    }

    // declare intBlk, an instance of IndInternal using constructor 2 for the block
    IndInternal intBlk(intBlockNum);

    // load blockHeader with header of intBlk using BlockBuffer::getHeader().
    HeadInfo blockHeader;
    ret = intBlk.getHeader(&blockHeader);
    if (ret != SUCCESS) {
        return ret;
    }

    // declare internalEntries to store all existing entries + the new entry
    InternalEntry internalEntries[blockHeader.numEntries + 1];

    /*
    Iterate through all the entries in the block and copy them to the array`internalEntries`. 
    Insert `indexEntry` at appropriate position in the array maintaining the ascending order.
        - use IndInternal::getEntry() to get the entry
        - use compareAttrs() to compare two structs of type Attribute

    Update the lChild of the internalEntry immediately following the newly added
    entry to the rChild of the newly added entry.
    */

    int i = 0, j = 0;

    while (i < blockHeader.numEntries) {

        InternalEntry currentEntry;
        ret = intBlk.getEntry(&currentEntry, i);
        if (ret != SUCCESS) {
            return ret;
        }

        if (compareAttrs(intEntry.attrVal, currentEntry.attrVal, attrCatEntry.attrType) < 0) {
            internalEntries[j++] = intEntry;
            break;
        }
        internalEntries[j++] = currentEntry;
        i++;
    }

    // Insert at the end if no larger entry was found
    if (i == blockHeader.numEntries) {
        internalEntries[j++] = intEntry;
    }

    // Copy the remaining existing entries
    while (i < blockHeader.numEntries) {

        ret = intBlk.getEntry(&internalEntries[j], i);
        if (ret != SUCCESS) {
            return ret;
        }
        i++; j++;
    }

    // IMP: Update the left child of the entry following the new entry
    for (int k = 0; k < blockHeader.numEntries+1; k++) {

        if (compareAttrs(internalEntries[k].attrVal, intEntry.attrVal, attrCatEntry.attrType) == 0 &&
            internalEntries[k].lChild == intEntry.lChild &&
            internalEntries[k].rChild == intEntry.rChild)
        {
            if (k+1 < blockHeader.numEntries+1) {
                internalEntries[k+1].lChild = intEntry.rChild;
            }
            break;
        }
    }


    // CASE 1: (internal index block has not reached max limit)

    if (blockHeader.numEntries != MAX_KEYS_INTERNAL) {
        // Increment numEntries and then update header
        blockHeader.numEntries++;

        ret = intBlk.setHeader(&blockHeader);
        if (ret != SUCCESS) {
            return ret;
        }

        // Write all entries back
        for (int k = 0; k < blockHeader.numEntries; k++) {

            ret = intBlk.setEntry(&internalEntries[k], k);
            if (ret != SUCCESS) {
                return ret;
            }
        }
        return SUCCESS;
    }

    // CASE 2: `internalEntries` array has more than entries than can fit in a single internal index block. 
    // (we need to split between two internal index blocks)

    int newRightBlk = splitInternal(intBlockNum, internalEntries);

    if (newRightBlk == E_DISKFULL) {
        // Destroy the newly disconnected right subtree
        BPlusTree::bPlusDestroy(intEntry.rChild);
        return E_DISKFULL;
    }
    
    /* the current block was not the root */
    if (blockHeader.pblock != -1) {
        // insert the middle value from `internalEntries` into the parent block
        InternalEntry newEntry;
        newEntry.attrVal = internalEntries[MIDDLE_INDEX_INTERNAL].attrVal;

        newEntry.lChild = intBlockNum;
        newEntry.rChild = newRightBlk;

        BPlusTree::insertIntoInternal(relId, attrName, blockHeader.pblock, newEntry);

        if (ret != SUCCESS)
            return ret;

    } else {
        // the current block was the root block and is now split. a new internal index
        // block needs to be allocated and made the root of the tree.

        ret = createNewRoot(relId, attrName, internalEntries[MIDDLE_INDEX_INTERNAL].attrVal, intBlockNum, newRightBlk);
    }

    if (ret != SUCCESS)
        return ret;

    return SUCCESS;
}


int BPlusTree::splitInternal(int intBlockNum, InternalEntry internalEntries[]) {
    // Allocate a new internal block for the right side
    IndInternal rightBlk;

    // Load the existing internal block
    IndInternal leftBlk(intBlockNum);

    int rightBlkNum = rightBlk.getBlockNum();
    int leftBlkNum = leftBlk.getBlockNum();

    if (rightBlkNum == E_DISKFULL) {
        return E_DISKFULL;
    }

    // get the headers of left block and right block
    HeadInfo leftBlkHeader, rightBlkHeader;

    int ret = leftBlk.getHeader(&leftBlkHeader);
    if (ret != SUCCESS) {
        rightBlk.releaseBlock();
        return ret;
    }

    ret = rightBlk.getHeader(&rightBlkHeader);
    if (ret != SUCCESS) {
        rightBlk.releaseBlock();
        return ret;
    }

    // set rightBlkHeader with the following values & update the header of rightBlk
    rightBlkHeader.numEntries = (MAX_KEYS_INTERNAL + 1) / 2 - 1;
    rightBlkHeader.pblock = leftBlkHeader.pblock;

    ret = rightBlk.setHeader(&rightBlkHeader);
    if (ret != SUCCESS) {
        rightBlk.releaseBlock();
        return ret;
    }

    // set leftBlkHeader with the following values & update the header
    leftBlkHeader.numEntries = (MAX_KEYS_INTERNAL + 1) / 2 - 1;

    ret = leftBlk.setHeader(&leftBlkHeader);
    if (ret != SUCCESS) {
        rightBlk.releaseBlock();
        return ret;
    }

    /*
    - set the first 50 entries of leftBlk = index 0 to 49 of internalEntries array
    - set the first 50 entries of newRightBlk = entries from index 51 to 100 of internalEntries array
      (index 50 will be moving to the parent internal index block)
    */
    for (int i = 0; i < MIDDLE_INDEX_INTERNAL; i++) {
        ret = leftBlk.setEntry(&internalEntries[i], i);
        if (ret != SUCCESS)
            return ret;
    }

    // The first left child of the right block is the promoted entry's right child
    internalEntries[MIDDLE_INDEX_INTERNAL + 1].lChild = internalEntries[MIDDLE_INDEX_INTERNAL].rChild;

    for (int i = MIDDLE_INDEX_INTERNAL + 1; i < MAX_KEYS_INTERNAL + 1; i++) {

        ret = rightBlk.setEntry(&internalEntries[i], i - MIDDLE_INDEX_INTERNAL - 1);
        if (ret != SUCCESS) {
            return ret;
        }
    }

    /* For each child block of the new right block, 
       update pblock of the block to rightBlkNum    */

    for (int i = 0; i < rightBlkHeader.numEntries; i++) {
        // declare an instance of BlockBuffer to access the child block
        InternalEntry entry;
        ret = rightBlk.getEntry(&entry, i);
        if (ret != SUCCESS) {
            return ret;
        }

        // Update the left child's parent pointer
        BlockBuffer leftChild(entry.lChild);
        HeadInfo childHeader;

        ret = leftChild.getHeader(&childHeader);
        if (ret != SUCCESS) {
            return ret;
        }

        childHeader.pblock = rightBlkNum;

        ret = leftChild.setHeader(&childHeader);
        if (ret != SUCCESS) {
            return ret;
        }

        // Update the right child's parent pointer
        BlockBuffer rightChild(entry.rChild);

        ret = rightChild.getHeader(&childHeader);
        if (ret != SUCCESS) {
            return ret;
        }

        childHeader.pblock = rightBlkNum;

        ret = rightChild.setHeader(&childHeader);
        if (ret != SUCCESS) {
            return ret;
        }
    }

    return rightBlkNum;
}



int BPlusTree::createNewRoot(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int lChild, int rChild) {
    // Get the attribute cache entry
    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    if (ret != SUCCESS) {
        return ret;
    }

    // Allocate a new internal block
    IndInternal newRootBlk;
    int newRootBlkNum = newRootBlk.getBlockNum();

    if (newRootBlkNum == E_DISKFULL) {
        // Destroy the newly disconnected right subtree
        BPlusTree::bPlusDestroy(rChild);
        return E_DISKFULL;
    }

    // update the header of the new block with numEntries = 1
    HeadInfo rootHeader;

    ret = newRootBlk.getHeader(&rootHeader);
    if (ret != SUCCESS) {
        newRootBlk.releaseBlock();
        return ret;
    }

    rootHeader.numEntries = 1;

    ret = newRootBlk.setHeader(&rootHeader);
    if (ret != SUCCESS) {
        newRootBlk.releaseBlock();
        return ret;
    }

    // Create the root entry
    InternalEntry rootEntry;

    rootEntry.lChild = lChild;
    rootEntry.attrVal = attrVal;
    rootEntry.rChild = rChild;

    ret = newRootBlk.setEntry(&rootEntry, 0);
    if (ret != SUCCESS) {
        newRootBlk.releaseBlock();
        return ret;
    }


    // Update the parent pointer of the left child
    HeadInfo childHeader;
    BlockBuffer leftChild(lChild);

    ret = leftChild.getHeader(&childHeader);
    if (ret != SUCCESS) {
        return ret;
    }

    childHeader.pblock = newRootBlkNum;

    ret = leftChild.setHeader(&childHeader);
    if (ret != SUCCESS) {
        return ret;
    }

    // Update the parent pointer of the right child
    BlockBuffer rightChild(rChild);

    ret = rightChild.getHeader(&childHeader);
    if (ret != SUCCESS) {
        return ret;
    }

    childHeader.pblock = newRootBlkNum;

    ret = rightChild.setHeader(&childHeader);
    if (ret != SUCCESS) {
        return ret;
    }

    // Update the root block for the entry corresponding to `attrName` 
    // in the attribute catalog cache
    attrCatEntry.rootBlock = newRootBlkNum;

    ret = AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);
    if (ret != SUCCESS) {
        return ret;
    }

    return SUCCESS;
}


void BPlusTree::printTreeByLevel(int relId, char attrName[ATTR_SIZE]) {

    AttrCatEntry attrCatEntry;
    if (AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry) != SUCCESS) {
        cout << "Unable to load attribute info\n";
        return;
    }

    int rootBlock = attrCatEntry.rootBlock;

    if (rootBlock == -1) {
        cout << "B+ Tree is empty / index doesn't exist!\n";
        return;
    }

    int attrType = attrCatEntry.attrType;

    queue<pair<int, int>> q;
    q.push({rootBlock, 0});

    int currentLevel = -1;

    cout << "\n========== B+ TREE ==========\n";

    while (!q.empty()) {

        int block = q.front().first;
        int level = q.front().second;
        q.pop();

        if (level != currentLevel) {
            currentLevel = level;
            cout << "\nLevel " << level << ":\n";
        }

        int type = StaticBuffer::getStaticBlockType(block);

        if (type == IND_INTERNAL) {

            IndInternal node(block);
            HeadInfo header;
            node.getHeader(&header);

            cout << "Internal Node (Block " << block << "): [";

            for (int i = 0; i < header.numEntries; i++) {
                InternalEntry entry;
                node.getEntry(&entry, i);

                if (attrType == NUMBER)
                    cout << entry.attrVal.nVal;
                else
                    cout << entry.attrVal.sVal;

                if (i != header.numEntries - 1)
                    cout << ", ";
            }

            cout << "]\n";

            if (header.numEntries > 0) {
                InternalEntry entry;

                node.getEntry(&entry, 0);
                if (entry.lChild != -1)
                    q.push({entry.lChild, level + 1});

                for (int i = 0; i < header.numEntries; i++) {
                    node.getEntry(&entry, i);

                    if (entry.rChild != -1)
                        q.push({entry.rChild, level + 1});
                }
            }
        }

        else if (type == IND_LEAF) {

            IndLeaf node(block);
            HeadInfo header;
            node.getHeader(&header);

            cout << "Leaf Node (Block " << block << "): [";

            for (int i = 0; i < header.numEntries; i++) {
                Index entry;
                node.getEntry(&entry, i);

                if (attrType == NUMBER)
                    cout << entry.attrVal.nVal;
                else
                    cout << entry.attrVal.sVal;

                if (i != header.numEntries - 1)
                    cout << ", ";
            }

            cout << "]\n";
        }
    }

    cout << "\n============================\n";
}
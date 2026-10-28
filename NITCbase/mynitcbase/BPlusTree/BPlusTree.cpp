#include "BPlusTree.h"

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
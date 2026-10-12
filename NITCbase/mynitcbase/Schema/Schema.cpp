#include "Schema.h"

#include <cmath>
#include <cstring>


int Schema::createRel(char relName[], int nAttrs, char attrs[][ATTR_SIZE], int attrtype[]) {

    //STEP 1: Check whether the relation already exists
    Attribute relNameAsAttribute;
    strcpy(relNameAsAttribute.sVal, relName);

    // declare targetRelId of type RecId & search for the given relation in relation catalog
    RelCacheTable::resetSearchIndex(RELCAT_RELID);
    RecId targetRelId = BlockAccess::linearSearch(RELCAT_RELID, (char *) RELCAT_ATTR_RELNAME, relNameAsAttribute, EQ);

    // if a relation with name `relName` already exists
    if (targetRelId.block != -1 && targetRelId.slot != -1)
      return E_RELEXIST;

    
    //STEP 2: Check for duplicate attribute names
    // compare every pair of attributes of attrNames[] array
    for (int i = 0; i < nAttrs; i++)
      for (int j = i+1; j < nAttrs; j++) {
        if (strcmp(attrs[i], attrs[j]) == 0)
          return E_DUPLICATEATTR;
      }

    //STEP 3: Construct the Relation Catalog record
    Attribute relCatRecord[RELCAT_NO_ATTRS];

    strcpy(relCatRecord[RELCAT_REL_NAME_INDEX].sVal, relName);
    relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal = nAttrs;
    relCatRecord[RELCAT_NO_RECORDS_INDEX].nVal = 0;
    relCatRecord[RELCAT_FIRST_BLOCK_INDEX].nVal = -1;
    relCatRecord[RELCAT_LAST_BLOCK_INDEX].nVal = -1;
    // number of slots calculated as in the physical layer docs
    relCatRecord[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal = (int) floor(2016.0 / (16 * nAttrs +1));


    //STEP 4: Insert the relation into RELATIONCAT
    int ret = BlockAccess::insert(RELCAT_RELID, relCatRecord);
    if (ret != SUCCESS) {
      return ret;  // fail if no more space in relation catalog

    }

    // Insert each attribute into ATTRIBUTECAT
    for (int i = 0; i < nAttrs; i++) {
      /* declare Attribute attrCatRecord[6] to store the attribute catalog
         record corresponding to i'th attribute of the argument passed */
      Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

      strcpy(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, relName);
      strcpy(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attrs[i]);
      attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal = attrtype[i];
      attrCatRecord[ATTRCAT_PRIMARY_FLAG_INDEX].nVal = -1;
      attrCatRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal = -1;
      attrCatRecord[ATTRCAT_OFFSET_INDEX].nVal = i;

      ret = BlockAccess::insert(ATTRCAT_RELID, attrCatRecord);
      if (ret != SUCCESS) {
        /* if attribute catalog insert fails:
            delete the relation by calling deleteRel(targetrel) of schema layer
            return E_DISKFULL
            // (this is necessary because we had already created the
            //  relation catalog entry which needs to be removed)
        */
        return ret;
      }
    }
    return SUCCESS;
}


int Schema::openRel(char relName[ATTR_SIZE]) {
  int ret = OpenRelTable::openRel(relName);

  // the OpenRelTable::openRel() function returns the rel-id if successful
  // a valid rel-id will be within the range 0 <= relId < MAX_OPEN and any error codes will be negative
  if(ret >= 0){
    return SUCCESS;
  }

  //otherwise it returns an error message
  return ret;
}

int Schema::closeRel(char relName[ATTR_SIZE]) {
  /* If it is relation or attribute catalog -> don't close */
  if (strcmp(relName, "RELATIONCAT")==0 || strcmp(relName, "ATTRIBUTECAT")==0) {
    return E_NOTPERMITTED;
  }

  // this function returns the rel-id of a relation if it is open or E_RELNOTOPEN if it is not.
  int relId = OpenRelTable::getRelId(relName);

  if (relId == E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  return OpenRelTable::closeRel(relId);
}


int Schema::deleteRel(char *relName) {
    // Catalogs cannot be deleted
    if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0)
      return E_NOTPERMITTED;

    // get the rel-id using 
    // if relation is opened in open relation table, return E_RELOPEN
    int relId = OpenRelTable::getRelId(relName);
    if (relId != E_RELNOTOPEN) {
      return E_RELOPEN;
    }

    // Delete the relation and its catalog entries.
    return BlockAccess::deleteRelation(relName);

    /* the only that should be returned from deleteRelation() is E_RELNOTEXIST.
       The deleteRelation call may return E_OUTOFBOUND from the call to
       loadBlockAndGetBufferPtr, but if your implementation so far has been
       correct, it should not reach that point. That error could only occur
       if the BlockBuffer was initialized with an invalid block number.
    */
   return SUCCESS;
}


int Schema::renameRel(char oldRelName[ATTR_SIZE], char newRelName[ATTR_SIZE]) {
    // if the oldRelName or newRelName is either Relation Catalog or Attribute Catalog, return E_NOTPERMITTED
    if (strcmp(oldRelName, RELCAT_RELNAME)==0 || strcmp(newRelName, RELCAT_RELNAME)==0 ||
        strcmp(oldRelName, ATTRCAT_RELNAME)==0 || strcmp(newRelName, ATTRCAT_RELNAME)==0)
      return E_NOTPERMITTED;

    // check if the relation is open
    int relId = OpenRelTable::getRelId(oldRelName);
    if (relId != E_RELNOTOPEN) {
      return E_RELOPEN;
    }

    return BlockAccess::renameRelation(oldRelName, newRelName);
}

int Schema::renameAttr(char *relName, char *oldAttrName, char *newAttrName) {
    // if the relName is either Relation Catalog or Attribute Catalog, return E_NOTPERMITTED
    if (strcmp(relName, RELCAT_RELNAME)==0 || strcmp(relName, ATTRCAT_RELNAME)==0)
      return E_NOTPERMITTED;

    // check if the relation is open
    int relId = OpenRelTable::getRelId(relName);
    if (relId != E_RELNOTOPEN) {
      return E_RELOPEN;
    }

    return BlockAccess::renameAttribute(relName, oldAttrName, newAttrName);
}
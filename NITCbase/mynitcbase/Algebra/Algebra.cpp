#include "Algebra.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
using namespace std;

extern int blockAccessCount;

// No. of Comparisons per block
extern int comparisonsPerBlock[DISK_BLOCKS];

// Counters defined in BlockAccess.cpp and BPlusTree.cpp.
extern int linearComparisons;
extern int bPlusComparisons;

bool isNumber(char *str);


int Algebra::insert(char relName[ATTR_SIZE], int nAttrs, char record[][ATTR_SIZE]) {

    // Relation & Attribute Catalog cannot modify directly
    if (strcmp(relName, "RELATIONCAT") == 0 || strcmp(relName, "ATTRIBUTECAT") == 0)
      return E_NOTPERMITTED;

    // get the relation's rel-id
    int relId = OpenRelTable::getRelId(relName);
    if (relId == E_RELNOTOPEN) {
      return E_RELNOTOPEN;    // if relation is not open
    }

    // get the relation catalog entry from relation cache
    RelCatEntry relCatEntry;
    int ret = RelCacheTable::getRelCatEntry(relId, &relCatEntry);
    if (ret != SUCCESS) {
      return ret;
    }

    // Check for number of attributes
    if (relCatEntry.numAttrs != nAttrs) {
      return E_NATTRMISMATCH;
    }

    // Convert 2D char array of record values to union Attribute array
    Attribute recordValues[nAttrs];

    for (int i = 0; i < nAttrs; i++) {
        // get the attr-cat entry for the i'th attribute from the attr-cache
        AttrCatEntry attrCatEntry;
        ret = AttrCacheTable::getAttrCatEntry(relId, i, &attrCatEntry);
        if (ret != SUCCESS) {
          return ret;
        }

        int type = attrCatEntry.attrType;

        if (type == NUMBER)
        {
          if (isNumber(record[i]))
            recordValues[i].nVal = atof(record[i]);
          else
            return E_ATTRTYPEMISMATCH;
        }
        else if (type == STRING) {
          strcpy(recordValues[i].sVal, record[i]);
        }
    }

    return BlockAccess::insert(relId, recordValues);
}


/* used to select all the records that satisfy a condition.
the arguments of the function are
- srcRel - the source relation we want to select from
- targetRel - the relation we want to select into. (ignore for now)
- attr - the attribute that the condition is checking
- op - the operator of the condition
- strVal - the value that we want to compare against (represented as a string)
*/
int Algebra::select(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], char attr[ATTR_SIZE], int op, char strVal[ATTR_SIZE]) {
  
  int srcRelId = OpenRelTable::getRelId(srcRel);      // we'll implement this later
  if (srcRelId == E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  // get the attribute catalog entry for attr, using AttrCacheTable::getAttrcatEntry()
  //    return E_ATTRNOTEXIST if it returns the error
  AttrCatEntry attrCatEntry;
  int ret = AttrCacheTable::getAttrCatEntry(
      srcRelId,
      attr,
      &attrCatEntry
  );
  if (ret != SUCCESS) {
    return E_ATTRNOTEXIST;
  }

  /*** Convert strVal (string) to an attribute of data type NUMBER or STRING ***/
  Attribute attrVal;
  int type = attrCatEntry.attrType;

  if (type == NUMBER) {
    if (isNumber(strVal)) {       // the isNumber() function is implemented below
      attrVal.nVal = atof(strVal);
    } else {
      return E_ATTRTYPEMISMATCH;
    }
  } else if (type == STRING) {
    strcpy(attrVal.sVal, strVal);
  }
  

  /*** Selecting records from the source relation ***/

  // Get the source relation's schema.
  RelCatEntry srcRelCatEntry;
  ret = RelCacheTable::getRelCatEntry(srcRelId, &srcRelCatEntry);
  if (ret != SUCCESS)
    return ret;

  int src_nAttrs = srcRelCatEntry.numAttrs;
  char attrNames[src_nAttrs][ATTR_SIZE];
  int attrTypes[src_nAttrs];

  for (int i = 0; i < src_nAttrs; i++) {
    AttrCatEntry entry;
    ret = AttrCacheTable::getAttrCatEntry(srcRelId, i, &entry);
    if (ret != SUCCESS)
      return ret;
    
      strcpy(attrNames[i], entry.attrName);
      attrTypes[i] = entry.attrType;
  }


  /*** Creating and opening the target relation ***/

  // Create the relation for target relation by calling Schema::createRel()
  ret = Schema::createRel(targetRel, src_nAttrs, attrNames, attrTypes);
  if (ret != SUCCESS) {
    return ret;
  }

  // Open the target relation.
  int targetRelId = OpenRelTable::openRel(targetRel);
  if (targetRelId < 0) {  //if opening fails
    Schema::deleteRel(targetRel);
    return targetRelId;
  }


  /*** Selecting and inserting records into the target relation ***/

  // Start searching the source relation from the beginning.
  ret = RelCacheTable::resetSearchIndex(srcRelId);
  if (ret != SUCCESS) {
    Schema::closeRel(targetRel);
    Schema::deleteRel(targetRel);
    return ret;
  }
  
  /*
    The BlockAccess::search() function can either do a linearSearch or a B+ tree search. 
    Hence, reset the search index of the relation in the relation cache using RelCacheTable::resetSearchIndex().
    Also, reset the search index in the attribute cache for the select condition attribute 
    with name given by the argument `attr`. Use AttrCacheTable::resetSearchIndex().
    Both these calls are necessary to ensure that search begins from the first record.
  */
  // AttrCacheTable::resetSearchIndex(/* fill arguments */);
  
  // Reset the B+ Tree search index for the condition attribute.
  ret = AttrCacheTable::resetSearchIndex(srcRelId, attr);
  if (ret != SUCCESS) {
    Schema::closeRel(targetRel);
    Schema::deleteRel(targetRel);
    return ret;
  }

  Attribute record[src_nAttrs];

  // Reset counters at the start of this SELECT query.
  linearComparisons = 0;
  bPlusComparisons = 0;

  for (int i = 0; i < DISK_BLOCKS; i++) {
    comparisonsPerBlock[i] = 0;
  }

  while (true) {
    ret = BlockAccess::search(srcRelId, record, attr, attrVal, op);
    if (ret == E_NOTFOUND)
      break;

    if (ret != SUCCESS) {
      Schema::closeRel(targetRel);
      Schema::deleteRel(targetRel);
      return ret;
    }

    ret = BlockAccess::insert(targetRelId, record);

    // if insert fails
    if (ret != SUCCESS) {
      Schema::closeRel(targetRel);
      Schema::deleteRel(targetRel);
      return ret;
    }
  }

  cout << "Number of block accesses: " << blockAccessCount << endl;

  // print B+ Tree used for the condition attribute
  // BPlusTree::printTreeByLevel(srcRelId, attr);

  // Display the total comparisons for this SELECT query.
  printf("\n--- Search Comparison Count ---\n");
  printf("Linear search comparisons: %d\n", linearComparisons);
  printf("B+ Tree comparisons: %d\n", bPlusComparisons);

  /*
  printf("\nComparisons per block:\n");

  for (int i = 0; i < DISK_BLOCKS; i++) {
    if (comparisonsPerBlock[i] > 0) {
      printf("Block %d: %d comparisons\n",
            i, comparisonsPerBlock[i]);
    }
  }
  */

  // Close the target relation after filling it.
  ret = Schema::closeRel(targetRel);
  if (ret != SUCCESS) {
    return ret;
  }
  
  return SUCCESS;
}


/* ------------------------------------------
 * Returns true if a string can be parsed completely as a floating-point number.
 */
bool isNumber(char *str) {
  int len;
  float ignore;
  /*
    sscanf returns the number of elements read, so if there is no float matching
    the first %f, ret will be 0, else it'll be 1

    %n gets the number of characters read. this scanf sequence will read the
    first float ignoring all the whitespace before and after. and the number of
    characters read that far will be stored in len. if len == strlen(str), then
    the string only contains a float with/without whitespace. else, there's other
    characters.
  */
  int ret = sscanf(str, "%f %n", &ignore, &len);
  return ret == 1 && len == strlen(str);
}



/* --- PROJECTION: Copy all attributes and records --- */

int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE]) {
  
    int srcRelId = OpenRelTable::getRelId(srcRel);
    if (srcRelId == E_RELNOTOPEN)
      return E_RELNOTOPEN;

    // get RelCatEntry of srcRel
    RelCatEntry srcRelCatEntry;
    int ret = RelCacheTable::getRelCatEntry(srcRelId, &srcRelCatEntry);
    if (ret != SUCCESS)
      return ret;

    int numAttrs = srcRelCatEntry.numAttrs;
    char attrNames[numAttrs][ATTR_SIZE];

    for (int i = 0; i < numAttrs; i++) {
      AttrCatEntry entry;
      ret = AttrCacheTable::getAttrCatEntry(srcRelId, i, &entry);
      if (ret != SUCCESS)
        return ret;

      strcpy(attrNames[i], entry.attrName);
    }

    return Algebra::project(srcRel, targetRel, numAttrs, attrNames);
}


/* --- PROJECTION: selected attributes === */

int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE],
                     int tar_nAttrs, char tar_Attrs[][ATTR_SIZE]) {

    int srcRelId = OpenRelTable::getRelId(srcRel);
    if (srcRelId == E_RELNOTOPEN)
      return E_RELNOTOPEN;

    // get RelCatEntry of srcRel
    RelCatEntry relCatEntry;
    int ret = RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);
    if (ret != SUCCESS)
      return ret;

    // get the no. of attributes present in relation from the fetched RelCatEntry.
    int src_nAttrs = relCatEntry.numAttrs;


    /*** Checking if attributes of target are present in source relation
         and storing their offsets and types ***/

    // attr_offset[i] = store the offset in a record of srcRel for i-th attribute in the target relation.
    // attr_types[i] = store the type of the i-th attribute in the target relation.
    int attr_offset[tar_nAttrs];
    int attr_types[tar_nAttrs];

    if (tar_nAttrs <= 0)
      return E_INVALID;

    for (int i = 0; i < tar_nAttrs; i++) {
        // Reject duplicate target attribute names.
        for (int j = 0; j < i; j++) {
          if (strcmp(tar_Attrs[i], tar_Attrs[j]) == 0)
            return E_DUPLICATEATTR;
        }

        AttrCatEntry attrCatEntry;
        ret = AttrCacheTable::getAttrCatEntry(
            srcRelId, tar_Attrs[i], &attrCatEntry
        );

        if (ret != SUCCESS)        //if attribute not found
          return E_ATTRNOTEXIST;

        attr_offset[i] = attrCatEntry.offset;
        attr_types[i] = attrCatEntry.attrType;
    }

    /*** Creating and opening the target relation ***/

    // Create a relation for target relation
    ret = Schema::createRel(
        targetRel, tar_nAttrs, tar_Attrs, attr_types
    );
    if (ret != SUCCESS)
        return ret;

    // Open the newly created target relation
    int targetRelId = OpenRelTable::openRel(targetRel);
    if (targetRelId < 0) {    // If opening fails
        Schema::deleteRel(targetRel);
        return targetRelId;
    }

    /*** Inserting projected records into the target relation ***/

    // Reset the searchIndex before calling the project function
    ret = RelCacheTable::resetSearchIndex(srcRelId);
    if (ret != SUCCESS) {
        Schema::closeRel(targetRel);
        Schema::deleteRel(targetRel);
        return ret;
    }

    Attribute record[src_nAttrs];

    while ((ret = BlockAccess::project(srcRelId, record)) == SUCCESS) {
        // the variable `record` will contain the next record
        Attribute proj_record[tar_nAttrs];

        for (int i = 0; i < tar_nAttrs; i++) {
            proj_record[i] = record[attr_offset[i]];
        }

        // Insert the record into target relation
        ret = BlockAccess::insert(targetRelId, proj_record);
        if (ret != SUCCESS) {
            Schema::closeRel(targetRel);
            Schema::deleteRel(targetRel);
            return ret;
        }
    }

    if (ret != E_NOTFOUND) {
        Schema::closeRel(targetRel);
        Schema::deleteRel(targetRel);
        return ret;
    }

    // Close the targetRel
    ret = Schema::closeRel(targetRel);
    if (ret != SUCCESS)
      return ret;

    return SUCCESS;
}


int Algebra::join(char srcRelation1[ATTR_SIZE], char srcRelation2[ATTR_SIZE], char targetRelation[ATTR_SIZE], char attribute1[ATTR_SIZE], char attribute2[ATTR_SIZE]) {

  // Get the relation IDs of both source relations.
  int srcRelId1 = OpenRelTable::getRelId(srcRelation1);
  int srcRelId2 = OpenRelTable::getRelId(srcRelation2);

  // if either of the two source relations is not open
  if (srcRelId1 == E_RELNOTOPEN || srcRelId2 == E_RELNOTOPEN)
      return E_RELNOTOPEN;


  // get the attribute catalog entries for the following from the attribute cache
  AttrCatEntry attrCatEntry1, attrCatEntry2;

  int ret = AttrCacheTable::getAttrCatEntry(srcRelId1, attribute1, &attrCatEntry1);
  if (ret != SUCCESS)
      return E_ATTRNOTEXIST;

  ret = AttrCacheTable::getAttrCatEntry(srcRelId2, attribute2, &attrCatEntry2);
  if (ret != SUCCESS)
      return E_ATTRNOTEXIST;

  // if attribute1 and attribute2 are of different types return E_ATTRTYPEMISMATCH
  if (attrCatEntry1.attrType != attrCatEntry2.attrType)
      return E_ATTRTYPEMISMATCH;

  
  // iterate through all the attributes in both the source relations and check if
  // there are any other pair of attributes other than join attributes
  RelCatEntry relCatEntry1, relCatEntry2;

  ret = RelCacheTable::getRelCatEntry(srcRelId1, &relCatEntry1);
  if (ret != SUCCESS)
      return ret;

  ret = RelCacheTable::getRelCatEntry(srcRelId2, &relCatEntry2);
  if (ret != SUCCESS)
      return ret;

  int numOfAttributes1 = relCatEntry1.numAttrs;
  int numOfAttributes2 = relCatEntry2.numAttrs;

  for (int i = 0; i < numOfAttributes1; i++) {

    AttrCatEntry entry1;
    ret = AttrCacheTable::getAttrCatEntry(srcRelId1, i, &entry1);
    if (ret != SUCCESS)
        return ret;
    
    for (int j = 0; j < numOfAttributes2; j++) {

      AttrCatEntry entry2;
      ret = AttrCacheTable::getAttrCatEntry(srcRelId2, j, &entry2);
      if (ret != SUCCESS)
          return ret;

      if (i == attrCatEntry1.offset && j == attrCatEntry2.offset)
          continue;

      if (strcmp(entry1.attrName, entry2.attrName) == 0)
          return E_DUPLICATEATTR;
      }
  }


  // if rel2 does not have an index on attr2, create it using BPlusTree:bPlusCreate()
  if (attrCatEntry2.rootBlock == -1) {
    ret = BPlusTree::bPlusCreate(srcRelId2, attribute2);
    if (ret != SUCCESS)
      return ret;
  }

  /* store the details of the target relation in the following arrays */
  int numOfAttributesInTarget = numOfAttributes1 + numOfAttributes2 - 1;

  char targetRelAttrNames[numOfAttributesInTarget][ATTR_SIZE];
  int targetRelAttrTypes[numOfAttributesInTarget];

  // Copy all attributes from the first source relation.
  for (int i = 0; i < numOfAttributes1; i++) {
    AttrCatEntry entry;
    ret = AttrCacheTable::getAttrCatEntry(srcRelId1, i, &entry);
    if (ret != SUCCESS)
        return ret;

    strcpy(targetRelAttrNames[i], entry.attrName);
    targetRelAttrTypes[i] = entry.attrType;
  }

  // Copy all attributes from the second source relation, excluding its join attribute.
  int targetOffset = numOfAttributes1;

  for (int i = 0; i < numOfAttributes2; i++) {
    if (i == attrCatEntry2.offset)
        continue;

    AttrCatEntry entry;
    ret = AttrCacheTable::getAttrCatEntry(srcRelId2, i, &entry);
    if (ret != SUCCESS)
        return ret;

    strcpy(targetRelAttrNames[targetOffset], entry.attrName);
    targetRelAttrTypes[targetOffset] = entry.attrType;

    targetOffset++;
  }

  // create the target relation using the Schema::createRel() function
  ret = Schema::createRel(targetRelation, numOfAttributesInTarget, targetRelAttrNames, targetRelAttrTypes);
  if (ret != SUCCESS)
    return ret;

  // Open the target relation.
  int targetRelId = OpenRelTable::openRel(targetRelation);
  if (targetRelId < 0) {
    Schema::deleteRel(targetRelation);
    return targetRelId;
  }


  Attribute record1[numOfAttributes1];
  Attribute record2[numOfAttributes2];
  Attribute targetRecord[numOfAttributesInTarget];

  // Reset the search index of the first relation.
  ret = RelCacheTable::resetSearchIndex(srcRelId1);
  if (ret != SUCCESS) {
    OpenRelTable::closeRel(targetRelId);
    Schema::deleteRel(targetRelation);
    return ret;
  }

  // this loop is to get every record of the srcRelation1 one by one
  while (BlockAccess::project(srcRelId1, record1) == SUCCESS) {

    // reset the search index of `srcRelation2` in the relation cache
    RelCacheTable::resetSearchIndex(srcRelId2);

    // reset the search index of `attribute2` in the attribute cache
    AttrCacheTable::resetSearchIndex(srcRelId2, attribute2);

    // this loop is to get every record of the srcRelation2 which satisfies:
    // record1.attribute1 = record2.attribute2 (i.e. Equi-Join condition)
    while (BlockAccess::search(
        srcRelId2, record2, attribute2, 
        record1[attrCatEntry1.offset], EQ
    ) == SUCCESS ) {

      // copy 1st & 2nd relation's attribute values (except join attribute)
      // from record1 and record2 to targetRecord
      for (int i = 0; i < numOfAttributes1; i++) {
        targetRecord[i] = record1[i];
      }

      int targetOffset = numOfAttributes1;

      for (int i = 0; i < numOfAttributes2; i++) {
        if (i == attrCatEntry2.offset)
            continue;

        targetRecord[targetOffset] = record2[i];
        targetOffset++;
      }

      // insert the current record into the target relation
      ret = BlockAccess::insert(targetRelId, targetRecord);

      if(ret != SUCCESS) {
        OpenRelTable::closeRel(targetRelId);
        Schema::deleteRel(targetRelation);
        return E_DISKFULL;
      }
    }
  }

  // close the target relation
  OpenRelTable::closeRel(targetRelId);
  return SUCCESS;
}
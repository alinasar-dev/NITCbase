#include "Algebra.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

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
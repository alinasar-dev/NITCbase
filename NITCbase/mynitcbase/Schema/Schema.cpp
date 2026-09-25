#include "Schema.h"

#include <cmath>
#include <cstring>

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
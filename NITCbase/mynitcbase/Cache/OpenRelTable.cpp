#include <cstring>
#include <cstdlib>

#include "OpenRelTable.h"

// Stores whether a relation is already open or not
OpenRelTableMetaInfo OpenRelTable::tableMetaInfo[MAX_OPEN];


int OpenRelTable::getRelId(char relName[ATTR_SIZE]) {
  // Traverse all entries in the table
  for (int i = 0; i < MAX_OPEN; i++) {
    if (!tableMetaInfo[i].free) {
      if (strcmp(relName, tableMetaInfo[i].relName)==0)
        return i;
    }
  }

  // if found return the relation id, else indicate that the relation do not
  // have an entry in the Open Relation Table.
  return E_RELNOTOPEN;
}

int OpenRelTable::getFreeOpenRelTableEntry() {

  /* traverse through the tableMetaInfo array,
    find a free entry in the Open Relation Table.*/
  for (int i = 0; i < MAX_OPEN; i++) {
    if (tableMetaInfo[i].free)
      return i;
  }

  // if found return the relation id, else return E_CACHEFULL.
  return E_CACHEFULL;
}


/*
 Constructor:
 Initializes the Relation Cache and Attribute Cache
*/
OpenRelTable::OpenRelTable() {

  // Initialize all cache and metadata entries
  for (int i = 0; i < MAX_OPEN; ++i) {
    RelCacheTable::relCache[i] = nullptr;
    AttrCacheTable::attrCache[i] = nullptr;

    tableMetaInfo[i].free = true;
    strcpy(tableMetaInfo[i].relName, "");
  }


  /************ Setting up Relation Cache entries ************/
  // (we need to populate relation cache with entries for the relation catalog and attribute catalog.)
  RecBuffer relCatBlock(RELCAT_BLOCK);
  Attribute relCatRecord[RELCAT_NO_ATTRS];

  /**** setting up Relation Catalog relation in the Relation Cache Table****/
  relCatBlock.getRecord(
      relCatRecord,
      RELCAT_SLOTNUM_FOR_RELCAT
  );

  struct RelCacheEntry relCacheEntry;

  RelCacheTable::recordToRelCatEntry(
      relCatRecord,
      &relCacheEntry.relCatEntry
  );
  relCacheEntry.recId.block = RELCAT_BLOCK;
  relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_RELCAT;

  // allocate this on the heap because we want it to persist outside this function
  RelCacheTable::relCache[RELCAT_RELID] = 
          (struct RelCacheEntry*) malloc(sizeof(RelCacheEntry));
  *(RelCacheTable::relCache[RELCAT_RELID]) = relCacheEntry;



  /**** setting up Attribute Catalog relation in the Relation Cache Table ****/

  // set up the relation cache entry for the attribute catalog similarly from the record at RELCAT_SLOTNUM_FOR_ATTRCAT
  relCatBlock.getRecord(
      relCatRecord,
      RELCAT_SLOTNUM_FOR_ATTRCAT
  );

  RelCacheTable::recordToRelCatEntry(
      relCatRecord,
      &relCacheEntry.relCatEntry
  );
  relCacheEntry.recId.block = RELCAT_BLOCK;
  relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_ATTRCAT;

  // set the value at RelCacheTable::relCache[ATTRCAT_RELID]
  RelCacheTable::relCache[ATTRCAT_RELID] = 
          (struct RelCacheEntry *) malloc(sizeof(RelCacheEntry));
  *(RelCacheTable::relCache[ATTRCAT_RELID]) = relCacheEntry;




  /************ Setting up Attribute cache entries ************/
  // (we need to populate attribute cache with entries for the relation catalog and attribute catalog.)

  /**** setting up Relation Catalog relation in the Attribute Cache Table ****/
  RecBuffer attrCatBlock(ATTRCAT_BLOCK);
  Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

  // iterate through all the attributes of the relation catalog and create a linked list of AttrCacheEntry (slots 0 to 5)
  // for each of the entries, set
  //    attrCacheEntry.recId.block = ATTRCAT_BLOCK;
  //    attrCacheEntry.recId.slot = i   (0 to 5)
  //    and attrCacheEntry.next appropriately
  // (NOTE: allocate each entry dynamically using malloc)

  AttrCacheEntry *listHead = nullptr;
  AttrCacheEntry *prevEntry = nullptr;
  
  for (int i = 0; i < RELCAT_NO_ATTRS; ++i) {
    attrCatBlock.getRecord(attrCatRecord, i);

    AttrCacheEntry *attrCacheEntry = (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));

    AttrCacheTable::recordToAttrCatEntry(
        attrCatRecord,
        &attrCacheEntry->attrCatEntry
    );
    
    attrCacheEntry->recId.block = ATTRCAT_BLOCK;
    attrCacheEntry->recId.slot = i;

    attrCacheEntry->next = nullptr;

    if (listHead == nullptr)
      listHead = attrCacheEntry;
    else
      prevEntry->next = attrCacheEntry;

    prevEntry = attrCacheEntry;
  }

  // set the next field in the last entry to nullptr -> DONE!!

  AttrCacheTable::attrCache[RELCAT_RELID] = listHead;



  /**** setting up Attribute Catalog relation in the Attribute Cache Table ****/

  // set up the attributes of the attribute cache similarly.
  // read slots 6-11 from attrCatBlock and initialise recId appropriately

  listHead = nullptr;
  prevEntry = nullptr;

  for (int i = RELCAT_NO_ATTRS; i < RELCAT_NO_ATTRS + ATTRCAT_NO_ATTRS; ++i) {

    attrCatBlock.getRecord(attrCatRecord, i);

    AttrCacheEntry *attrCacheEntry = (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));

    AttrCacheTable::recordToAttrCatEntry(
        attrCatRecord,
        &attrCacheEntry->attrCatEntry
    );

    attrCacheEntry->recId.block = ATTRCAT_BLOCK;
    attrCacheEntry->recId.slot = i;

    attrCacheEntry->next = nullptr;

    if (listHead == nullptr)
      listHead = attrCacheEntry;
    else
      prevEntry->next = attrCacheEntry;

    prevEntry = attrCacheEntry;
  }

  // set the value at AttrCacheTable::attrCache[ATTRCAT_RELID]
  AttrCacheTable::attrCache[ATTRCAT_RELID] = listHead;


  /**** Setting up Open Relation Table metadata ****/
  // (RELCAT and ATTRCAT are permanently open always)
  tableMetaInfo[RELCAT_RELID].free = false;
  strcpy(tableMetaInfo[RELCAT_RELID].relName, RELCAT_RELNAME);

  tableMetaInfo[ATTRCAT_RELID].free = false;
  strcpy(tableMetaInfo[ATTRCAT_RELID].relName, ATTRCAT_RELNAME);
}


int OpenRelTable::openRel(char relName[ATTR_SIZE]) {

  int relId = OpenRelTable::getRelId(relName);

  if (relId != E_RELNOTOPEN) {
    return relId;  //the relation is already open
  }

  /* find a free slot in the Open Relation Table */
  relId = OpenRelTable::getFreeOpenRelTableEntry();

  if (relId == E_CACHEFULL) {
    return E_CACHEFULL;  //free slot not available
  }


  /****** Setting up Relation Cache entry for the relation ******/

  /* search for the entry with relation name, relName, in the Relation Catalog using
      BlockAccess::linearSearch().
      We MUST reset the searchIndex of the RELCAT_RELID before calling linearSearch().
  */
  RelCacheTable::resetSearchIndex(RELCAT_RELID);

  // relcatRecId stores the rec-id of the relation `relName` in the Relation Catalog.
  RecId relcatRecId = BlockAccess::linearSearch(
    RELCAT_RELID,
    (char *) "RelName",
    *(Attribute *) relName,
    EQ
  );

  if (relcatRecId.block == -1 && relcatRecId.slot == -1) {
    // (the relation is not found in the Relation Catalog.)
    return E_RELNOTEXIST;
  }

  // Read the corresponding Relation Catalog record.
  RecBuffer relCatBlock(relcatRecId.block);
  Attribute relCatRecord[RELCAT_NO_ATTRS];

  int ret = relCatBlock.getRecord(
      relCatRecord,
      relcatRecId.slot
  );

  if (ret != SUCCESS) {
    return ret;
  }

  // Allocate memory for the new RelCacheEntry
  RelCacheEntry *relCacheEntry =
      (RelCacheEntry *)malloc(sizeof(RelCacheEntry));

  RelCacheTable::recordToRelCatEntry(
      relCatRecord,
      &relCacheEntry->relCatEntry
  );

  relCacheEntry->recId = relcatRecId;
  RelCacheTable::relCache[relId] = relCacheEntry;



  /****** Setting up Attribute Cache entry for the relation ******/

  // let listHead be used to hold the head of the linked list of attrCache entries.
  AttrCacheEntry *listHead = nullptr;
  AttrCacheEntry *tail = nullptr;

  RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

  while (true) {
    RecId attrcatRecId = BlockAccess::linearSearch(
        ATTRCAT_RELID,
        (char *)"RelName",
        *(Attribute *)relName,
        EQ
    );

    // No more attributes belonging to this relation
    if (attrcatRecId.block == -1 && attrcatRecId.slot == -1) {
      break;
    }


    // Read the Attribute Catalog record
    RecBuffer attrCatBlock(attrcatRecId.block);
    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

    ret = attrCatBlock.getRecord(
        attrCatRecord,
        attrcatRecId.slot
    );

    if (ret != SUCCESS) {
      /*
       * Something went wrong while reading ATTRCAT.
       * Free everything already allocated for this relation before returning the error.
       */

      AttrCacheEntry *entry = listHead;

      while (entry != nullptr) {
        AttrCacheEntry *next = entry->next;
        free(entry);
        entry = next;
      }

      free(RelCacheTable::relCache[relId]);
      RelCacheTable::relCache[relId] = nullptr;

      return ret;
    }

    // Allocate memory for this attribute-cache entry
    AttrCacheEntry *attrCacheEntry =
        (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));

    AttrCacheTable::recordToAttrCatEntry(
        attrCatRecord,
        &attrCacheEntry->attrCatEntry
    );

    attrCacheEntry->recId = attrcatRecId;
    attrCacheEntry->next = nullptr;

    // Add entry to the linked list
    if (listHead == nullptr) {
      listHead = attrCacheEntry;
      tail = attrCacheEntry;
    }
    else {
      tail->next = attrCacheEntry;
      tail = attrCacheEntry;
    }
  }

  // Store the head of the linked list in the Attribute Cache at the newly allocated rel-id
  AttrCacheTable::attrCache[relId] = listHead;


  /****** Setting up metadata in the Open Relation Table for the relation******/
  tableMetaInfo[relId].free = false;
  strcpy(tableMetaInfo[relId].relName, relName);

  return relId;
}


int OpenRelTable::closeRel(int relId) {
  if (relId == RELCAT_RELID || relId == ATTRCAT_RELID) {
    return E_NOTPERMITTED;
  }

  if (relId < 0 || relId >= MAX_OPEN) {
    return E_OUTOFBOUND;
  }

  if (tableMetaInfo[relId].free) {
    return E_RELNOTOPEN;
  }

  /*** free the memory allocated in the relation and attribute caches ***/

  // free Relation Cache Entry
  if (RelCacheTable::relCache[relId] != nullptr) {
    free(RelCacheTable::relCache[relId]);
    RelCacheTable::relCache[relId] = nullptr;
  }

  // free Attribute Cache Linked-list
  AttrCacheEntry *entry = AttrCacheTable::attrCache[relId];

  while (entry != nullptr) {
    AttrCacheEntry *next = entry->next;
    free(entry);

    entry = next;
  }
  AttrCacheTable::attrCache[relId] = nullptr;  //The AttCache linked list no longer exists


  // update `tableMetaInfo` to set `relId` as a free slot
  // update `relCache` and `attrCache` to set the entry at `relId` to nullptr
  tableMetaInfo[relId].free = true;
  strcpy(tableMetaInfo[relId].relName, "");

  return SUCCESS;
}



/*
 Destructor: free all the memory that we allocated in the constructor
*/
OpenRelTable::~OpenRelTable() {

  // close all normal relations
  for (int i = 2; i < MAX_OPEN; ++i) {
    if (!tableMetaInfo[i].free)
        OpenRelTable::closeRel(i);
  }
  
  // free Relational Cache entries
  if (RelCacheTable::relCache[RELCAT_RELID] != nullptr) {
    free(RelCacheTable::relCache[RELCAT_RELID]);
    RelCacheTable::relCache[RELCAT_RELID] = nullptr;
  }

  if (RelCacheTable::relCache[ATTRCAT_RELID] != nullptr) {
    free(RelCacheTable::relCache[ATTRCAT_RELID]);
    RelCacheTable::relCache[ATTRCAT_RELID] = nullptr;
  }


  // free the Attribute Cache linked list for RELATIONCAT
  AttrCacheEntry *entry = AttrCacheTable::attrCache[RELCAT_RELID];

  while (entry != nullptr) {
    AttrCacheEntry *next = entry->next;
    free(entry);
    entry = next;
  }

  AttrCacheTable::attrCache[RELCAT_RELID] = nullptr;


  // free the Attribute Cache linked list for ATTRIBUTECAT
  entry = AttrCacheTable::attrCache[ATTRCAT_RELID];

  while (entry != nullptr) {
    AttrCacheEntry *next = entry->next;
    free(entry);
    entry = next;
  }

  AttrCacheTable::attrCache[ATTRCAT_RELID] = nullptr;
}
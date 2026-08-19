#include <cstring>
#include <cstdlib>

#include "OpenRelTable.h"


/* Opens a relation named `relName`.
  For now, we hardcode it. Later, we will loop through all relations. */

int OpenRelTable::getRelId(char relName[ATTR_SIZE]) {
  // if relname is RELCAT_RELNAME, return RELCAT_RELID
  if (strcmp(relName, RELCAT_RELNAME) == 0) {
    return RELCAT_RELID;
  }

  // if relname is ATTRCAT_RELNAME, return ATTRCAT_RELID
  if (strcmp(relName, ATTRCAT_RELNAME) == 0) {
    return ATTRCAT_RELID;
  }

  return E_RELNOTOPEN;
}


/*
 Constructor:
 Initializes the Relation Cache and Attribute Cache
*/
OpenRelTable::OpenRelTable() {

  // initialize relCache and attrCache with nullptr
  for (int i = 0; i < MAX_OPEN; ++i) {
    RelCacheTable::relCache[i] = nullptr;
    AttrCacheTable::attrCache[i] = nullptr;
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




  /* --------------------------
  🗒️🖊️ Stage 3 EXERCISE: Print catalog entries for "Students"
  -------------------------- */
  HeadInfo relCatHeader;
  relCatBlock.getHeader(&relCatHeader);

  int studentsSlot = -1;

  for (int i = 0; i < relCatHeader.numEntries; i++) {
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    relCatBlock.getRecord(relCatRecord, i);

    if (strcmp(relCatRecord[RELCAT_REL_NAME_INDEX].sVal, "Students") == 0) {
      studentsSlot = i;
      break;
    }
  }

  int studentsRelId = 2;  // Cache Students using rel-id 2

  if (studentsSlot != -1) {
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    relCatBlock.getRecord(relCatRecord, studentsSlot);

    RelCacheEntry *entry = (RelCacheEntry *) malloc(sizeof(RelCacheEntry));

    RelCacheTable::recordToRelCatEntry(
        relCatRecord,
        &entry->relCatEntry
    );

    entry->recId.block = RELCAT_BLOCK;
    entry->recId.slot = studentsSlot;

    RelCacheTable::relCache[studentsRelId] = entry;


    // Cache Students' attributes
    RecBuffer attrCatBlock(ATTRCAT_BLOCK);

    HeadInfo attrCatHeader;
    attrCatBlock.getHeader(&attrCatHeader);

    AttrCacheEntry *head = nullptr;
    AttrCacheEntry *tail = nullptr;

    for (int i = 0; i < attrCatHeader.numEntries; i++) {

      Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

      attrCatBlock.getRecord(attrCatRecord, i);

      if (strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, "Students") != 0)
        continue;

      AttrCacheEntry *attrEntry = (AttrCacheEntry *) malloc(sizeof(AttrCacheEntry));

      AttrCacheTable::recordToAttrCatEntry(
          attrCatRecord,
          &attrEntry->attrCatEntry
      );

      attrEntry->recId.block = ATTRCAT_BLOCK;
      attrEntry->recId.slot = i;
      attrEntry->next = nullptr;

      if (head == nullptr)
        head = attrEntry;
      else
        tail->next = attrEntry;

      tail = attrEntry;
    }

    AttrCacheTable::attrCache[studentsRelId] = head;
  }
}


/*
 Destructor:
*/
OpenRelTable::~OpenRelTable() {
  // free all the memory that you allocated in the constructor
  
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
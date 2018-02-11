#include "TraversalManager.h"

extern CProxy_Main mainProxy;

void TraversalManager::getNext(CProxy_TreeElements tree_array, Key curr_key, int trav_type) {
  if (trav_type == UP_ONLY) {
    if (curr_key == 1) mainProxy.doneCentroid();
    else {
      // being called 8 times fam // 0???? 
      //tree_array[curr_key >> 3].exist(false);
      //tree_array[curr_key >> 3].leaf(); // bro we aint even interacting no mo
      //CkPrintf("just made a chare of index %d\n", curr_key >> 3); 
      //CkPrintf("next round fam! %d\n", curr_key);
      tree_array[curr_key].node(curr_key >> 3);
    }
  }
}

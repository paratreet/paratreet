#!/bin/bash -x

DECL_H="paratreet.decl.h"
DEF_H="paratreet.def.h"

function regex { gawk 'match($0,/'$1'/, ary) {print ary['${2:-'0'}']}'; }

function replace {
    lineNum="$(awk "/$1/{ print NR; exit }" $2)"
    if test -z "$lineNum"; then
        lineNum="$(grep -n "$1" "$2" | head -n 1 | cut -d: -f1)"
    fi
    if test -z "$lineNum"; then
        >&2 echo "ERROR: Could not find match for pattern: $1"
        exit 1 # terminate and indicate error
    else
        sed -i "$lineNum""s/$3/$4/g" "$2"
    fi
}

replace "void recursive_pup_impl<GroupMeshStreamer<std::pair<Key,int>" "$DECL_H" "<Data>" "<CentroidData>"
replace "void recursive_pup_impl<GroupMeshStreamer<std::pair<Key,int>" "$DEF_H" "<Data>" "<CentroidData>"
replace "obj->GroupMeshStreamer<std::pair<Key,int>" "$DEF_H" "<Data>" "<CentroidData>"
replace "requestNodes_marshall.*TramAggregator->insertData" "$DEF_H" "<false>" " "

replace "void recursive_pup_impl<GroupMeshStreamer<MultiData" "$DECL_H" "<Data>" "<CentroidData>"
replace "void recursive_pup_impl<GroupMeshStreamer<MultiData" "$DEF_H" "<Data>" "<CentroidData>"
replace "obj->GroupMeshStreamer<MultiData" "$DEF_H" "<Data>" "<CentroidData>"
replace "addCache_marshall.*TramAggregator->insertData" "$DEF_H" "<false>" " "

requestNodes="$(cat "$DECL_H" | regex "GroupMeshStreamer.*_callmarshall_requestNodes_marshall([0-9]+)" 1 | head -n1)"
sed -i "s/__REQUEST_NODES_PLACEHOLDER__/\&CkIndex_CacheManagerTRAM<CentroidData>::_callmarshall_requestNodes_marshall$requestNodes/g" "$DEF_H"

addCache="$(cat "$DECL_H" | regex "GroupMeshStreamer.*_callmarshall_addCache_marshall([0-9]+)" 1 | head -n1)"
sed -i "s/__ADD_CACHE_PLACEHOLDER__/\&CkIndex_CacheManagerTRAM<CentroidData>::_callmarshall_addCache_marshall$addCache/g" "$DEF_H"

# TODO fix these?
#replace "// REG: void addCache(const MultiData<Data> &impl_noname_c);" "$DEF_H" "\/\/.*" "CkIndex_GroupMeshStreamer<MultiData<CentroidData>, CacheManagerTRAM<CentroidData>, SimpleMeshRouter, \&CkIndex_CacheManagerTRAM<CentroidData>::_callmarshall_addCache_marshall$addCache>::idx_GroupMeshStreamer_marshall1();"
#replace "// REG: void receiveSubtree(const MultiData<Data> &impl_noname_3, const PPHolder<Data> &impl_noname_4);" "$DEF_H" "\/\/.*" "CkIndex_GroupMeshStreamer<std::pair<unsigned long, int>, CacheManagerTRAM<CentroidData>, SimpleMeshRouter, \&CkIndex_CacheManagerTRAM<CentroidData>::_callmarshall_requestNodes_marshall$requestNodes>::idx_GroupMeshStreamer_marshall1();"
#!/bin/bash -x

DECL_H="paratreet.decl.h"
DEF_H="paratreet.def.h"

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

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

replace "void recursive_pup_impl<ArrayMeshStreamer<Key, TreePiece <" "$DECL_H" "Data" "CentroidData"
replace "void recursive_pup_impl<ArrayMeshStreamer<Key, TreePiece <" "$DEF_H" "Data" "CentroidData"
replace "obj->ArrayMeshStreamer<Key, TreePiece <" "$DEF_H" "Data" "CentroidData"
replace "goDown_marshall.*TramAggregator->insertData" "$DEF_H" "<false>" " "

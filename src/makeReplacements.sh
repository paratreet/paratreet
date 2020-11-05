#!/bin/bash -x

DECL_H="paratreet.decl.h"
DEF_H="paratreet.def.h"

lineNum="$(grep -n "void recursive_pup_impl<ArrayMeshStreamer<Key, TreePiece <" "$DECL_H" | head -n 1 | cut -d: -f1)"
sed -i "$lineNum""s/Data/CentroidData/g" "$DECL_H"

lineNum="$(grep -n "void recursive_pup_impl<ArrayMeshStreamer<Key, TreePiece <" "$DEF_H" | head -n 1 | cut -d: -f1)"
sed -i "$lineNum""s/Data/CentroidData/g" "$DEF_H"

lineNum="$(grep -n "obj->ArrayMeshStreamer<Key, TreePiece <" "$DEF_H" | head -n 1 | cut -d: -f1)"
sed -i "$lineNum""s/Data/CentroidData/g" "$DEF_H"

lineNum="$(grep -n "goDown_marshall.*TramAggregator->insertData" "$DEF_H" | head -n 1 | cut -d: -f1)"
sed -i "$lineNum""s/<false>/ /g" "$DEF_H"

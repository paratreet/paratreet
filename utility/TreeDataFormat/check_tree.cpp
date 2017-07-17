//check_tree.cpp
// Given a tree file and a file of positions, check that the
// particles are all in the right places( i.e. that the tree was
// built correctly.

#include <iostream>
#include <iomanip>
#include <cstdlib>

#include "tree_xdr.h"
#include "OrientedBox.h"
#include "SFC.h"

using namespace std;
using namespace SFC;
using namespace TypeHandling;

int numBuckets;

void check_tree(OrientedBox<float> nodeBoundingBox, const u_int64_t numNodesUnder, const u_int64_t numParticlesUnder, const BasicTreeNode* node, XDR* posxdrs, const int axis) {
	//cout << "Box: " << nodeBoundingBox << endl;
	//cout << "Node: numNodesLeft: " << node->numNodesLeft << " numNodesRight: " << (numNodesUnder - node->numNodesLeft) << endl;
	//cout << "numParticlesLeft: " << node->numParticlesLeft << " numParticlesRight: " << (numParticlesUnder - node->numParticlesLeft) << endl;

	if(node->numNodesLeft == 0) { //left child is a bucket
		if(node->numParticlesLeft > 0)
			numBuckets++;
		Vector3D<float> position;
		OrientedBox<float> newBox = cutBoxLeft(nodeBoundingBox, axis);
		for(u_int64_t i = 0; i < node->numParticlesLeft; ++i) {
			if(!xdr_template(posxdrs, &position)) {
				cerr << "Problem reading a position!" << endl;
				return;
			}
			if(!newBox.contains(position)) {
				OrientedBox<float> b = newBox;
				bumpBox(b, HUGE_VAL);
				if(!b.contains(position))
					cerr << "lParticle really not in the box!\nBox: " << newBox << "\nDelta-Box: " << (newBox.greater_corner - newBox.lesser_corner) << "\nPosition: " << position << endl;
				else
					cerr << "Saved by last two bits!" << endl;
			}
		}
	} else //check the left child
		check_tree(cutBoxLeft(nodeBoundingBox, axis), node->numNodesLeft - 1, node->numParticlesLeft, node + 1, posxdrs, (axis + 1) % 3);
	
	if(numNodesUnder - node->numNodesLeft == 0) { //right child is a bucket
		if(numParticlesUnder - node->numParticlesLeft > 0)
			numBuckets++;
		Vector3D<float> position;
		nodeBoundingBox = cutBoxRight(nodeBoundingBox, axis);
		for(u_int64_t i = 0; i < (numParticlesUnder - node->numParticlesLeft); ++i) {
			if(!xdr_template(posxdrs, &position)) {
				cerr << "Problem reading a position!" << endl;
				return;
			}
			if(!nodeBoundingBox.contains(position)) {
				OrientedBox<float> b = nodeBoundingBox;
				bumpBox(b, HUGE_VAL);
				if(!b.contains(position))
					cerr << "lParticle really not in the box!\nBox: " << nodeBoundingBox << "\nDelta-Box: " << (nodeBoundingBox.greater_corner - nodeBoundingBox.lesser_corner) << "\nPosition: " << position << endl;
				else
					cerr << "Saved by last two bits!" << endl;
			}
		}
	} else //check the right child
		check_tree(cutBoxRight(nodeBoundingBox, axis), numNodesUnder - node->numNodesLeft - 1, numParticlesUnder - node->numParticlesLeft, node + node->numNodesLeft + 1, posxdrs, (axis + 1) % 3);
}

int main(int argc, char** argv) {
	if(argc < 3) {
		cerr << "Usage: " << argv[0] << " treefile posfile" << endl;
		return 1;
	}
	
	FILE* infile;
	XDR xdrs;
	TreeHeader h;
	
	infile = fopen(argv[1], "rb");
	if(!infile) {
		cerr << "Couldn't open tree file!" << endl;
		return 2;
	}
	xdrstdio_create(&xdrs, infile, XDR_DECODE);
	xdr_template(&xdrs, &h);
	if(h.magic != TreeHeader::MagicNumber) {
		cerr << "Tree file appears corrupt (magic number is wrong)!" << endl;
		return 3;
	}
	
	cout << "Tree header: " << h << endl;
	
	BasicTreeNode* tree = new BasicTreeNode[h.numNodes];
	for(u_int64_t i = 0; i < h.numNodes; ++i) {
		if(!xdr_template(&xdrs, tree + i)) {
			cerr << "Problem reading tree file" << endl;
			delete[] tree;
			return 2;
		}
	}
	xdr_destroy(&xdrs);
	fclose(infile);
	
	infile = fopen(argv[2], "rb");
	if(!infile) {
		cerr << "Couldn't open positions file!" << endl;
		return 4;
	}
	
	xdrstdio_create(&xdrs, infile, XDR_DECODE);
	FieldHeader fh;
	xdr_template(&xdrs, &fh);
	
	if(fh.magic != FieldHeader::MagicNumber) {
		cerr << "Positions field file appears corrupt (magic number is wrong)!" << endl;
		return 5;
	}
	
	if(fh.time != h.time || fh.numParticles != h.numParticles) {
		cerr << "Tree and positions don't match (time or number of particles)!" << endl;
		return 6;
	}
	
	if(fh.dimensions != 3 || fh.code != float32) {
		cerr << "Position file contains wrong data type!" << endl;
		return 7;
	}
	
	cout << "Position field header: " << fh << endl;
	
	OrientedBox<float> boundingBox;
	if(!xdr_template(&xdrs, &boundingBox.lesser_corner) || !xdr_template(&xdrs, &boundingBox.greater_corner)) {
		cerr << "Couldn't read bounding box from positions file!" << endl;
		return 8;
	}
	if(boundingBox.lesser_corner == boundingBox.greater_corner) {
		cerr << "All particles at same position, don't know what to do!" << endl;
		return 9;
	}
	
	//cout.setf(ios::scientific);
	//cerr.setf(ios::scientific);
	
	numBuckets = 0;
	check_tree(h.boundingBox, h.numNodes - 1, h.numParticles, tree, &xdrs, 0);
	
	xdr_destroy(&xdrs);
	fclose(infile);
	
	cout << "Tree had " << numBuckets << " buckets" << endl;
	
	delete[] tree;
	
	cerr << "Done." << endl;
}

/** @file tipsy2tree.cpp
 Convert a tipsy format file of particle data into tree-based
 set of files.
 @author Graeme Lufkin (gwl@u.washington.edu)
 @date Created February 12, 2003
 @version 2.0
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef HAVE_LIBPOPT
#include <popt.h>
#endif

#include "TipsyFile.h"
#include "OrientedBox.h"
#include "tree_xdr.h"
#include "aggregate_xdr.h"
#include "SFC.h"

int peanoKey=1; /* used by SFC.h */

using namespace std;
using namespace Tipsy;
using namespace SFC;
using namespace TypeHandling;

int verbosity;
int bucketSize;

MAKE_AGGREGATE_WRITER(tipsyOrder)
MAKE_AGGREGATE_WRITER(mass)
MAKE_AGGREGATE_WRITER(pos)
MAKE_AGGREGATE_WRITER(vel)
MAKE_AGGREGATE_WRITER(eps)
MAKE_AGGREGATE_WRITER(phi)
MAKE_AGGREGATE_WRITER(rho)
MAKE_AGGREGATE_WRITER(temp)
MAKE_AGGREGATE_WRITER(hsmooth)
MAKE_AGGREGATE_WRITER(metals)
MAKE_AGGREGATE_WRITER(tform)

template <typename BaseParticle>
class TreeParticle : public BaseParticle {
public:
	
	Key key;
	unsigned int tipsyOrder;
	
	TreeParticle(Key k = 0) : key(k) { }
	
	TreeParticle(const BaseParticle& p) : BaseParticle(p) { }
	
	/** Comparison operator, used for a sort based on key values. */
	bool operator<(const TreeParticle<BaseParticle>& p) const {
		return key < p.key;
	}
};

class MyTreeNode {
public:
	
	MyTreeNode* leftChild;
	MyTreeNode* rightChild;
	
	Key key;
	unsigned char level;
	
	unsigned int numNodesLeft;
	unsigned int numNodesUnder;
	unsigned int numParticlesLeft;
	unsigned int numParticlesUnder;
	
	MyTreeNode() : leftChild(0), rightChild(0), key(0), level(0), numNodesLeft(0), numNodesUnder(0), numParticlesLeft(0), numParticlesUnder(0) { }
	
	MyTreeNode* createLeftChild() {
		MyTreeNode* child = new MyTreeNode;
		child->key = key;
		child->level = level + 1;
		leftChild = child;
		return child;
	}
	
	MyTreeNode* createRightChild() {
		MyTreeNode* child = new MyTreeNode;
		child->key = key | (static_cast<Key>(1) << (62 - level));
		child->level = level + 1;
		rightChild = child;
		return child;
	}
	
	bool isBucket() const {
		return (leftChild == 0 && rightChild == 0);
	}
};

template <typename ParticleType>
class TreeBuilder {
public:
	ParticleType* leftBoundary;
	ParticleType* rightBoundary;
	int maxBucketSize;
	
	unsigned int numBuckets;
	unsigned int numNodes;
	unsigned int numParticles;
	
	TreeBuilder(ParticleType* leftB, ParticleType* rightB, int bucketSize) : leftBoundary(leftB), rightBoundary(rightB), maxBucketSize(bucketSize), numBuckets(0), numNodes(0), numParticles(0) { }

	
	void buildTree(MyTreeNode* node) {
		buildTree(node, leftBoundary, rightBoundary);
	}
	
private:
	void buildTree(MyTreeNode* node, ParticleType* leftParticle, ParticleType* rightParticle) {
		numNodes++;
		
		//check if we should bucket these particles
		if(rightParticle - leftParticle < maxBucketSize) {
			//can't bucket until we've cut at the boundary
			if((leftParticle != leftBoundary) && (rightParticle != rightBoundary)) {
				numBuckets++;
				node->numNodesLeft = 0;
				node->numNodesUnder = 0;
				node->numParticlesLeft = rightParticle - leftParticle + 1;
				node->numParticlesUnder = node->numParticlesLeft;
				numParticles += node->numParticlesLeft;
				return;
			}
		} else if(node->level == 63) {
			cerr << "This tree has exhausted all the bits in the keys.  Super double-plus ungood!" << endl;
			return;
		}

		//this is the bit we are looking at
		Key currentBitMask = static_cast<Key>(1) << (62 - node->level);
		//we need to know the bit values at the left and right
		Key leftBit = leftParticle->key & currentBitMask;
		Key rightBit = rightParticle->key & currentBitMask;
		MyTreeNode* child;

		if(leftBit < rightBit) { //a split at this level
			//find the split by looking for where the key with the bit switched on could go
			ParticleType* splitParticle = lower_bound(leftParticle, rightParticle + 1, node->key | currentBitMask);
			if(splitParticle == leftBoundary + 1) {
				child = node->createRightChild();
				buildTree(child, splitParticle, rightParticle);
			} else if(splitParticle == rightBoundary) {
				child = node->createLeftChild();
				buildTree(child, leftParticle, splitParticle - 1);
			} else {
				child = node->createLeftChild();
				buildTree(child, leftParticle, splitParticle - 1);
				child = node->createRightChild();
				buildTree(child, splitParticle, rightParticle);
			}
		} else if(leftBit & rightBit) { //both ones, make a right child
			child = node->createRightChild();
			buildTree(child, leftParticle, rightParticle);
		} else if(leftBit > rightBit) {
			cerr << "This should never happen, the keys must not be sorted right!" << endl;
			return;
		} else { //both zeros, make a left child
			child = node->createLeftChild();
			buildTree(child, leftParticle, rightParticle);
		}

		if(node->leftChild) {
			node->numNodesLeft = node->leftChild->numNodesUnder + (node->leftChild->isBucket() ? 0 : 1);
			node->numParticlesLeft = node->leftChild->numParticlesUnder;
		} else {
			node->numNodesLeft = 0;
			node->numParticlesLeft = 0;
		}
		node->numNodesUnder = node->numNodesLeft;
		node->numParticlesUnder = node->numParticlesLeft;
		if(node->rightChild) {
			node->numNodesUnder += node->rightChild->numNodesUnder + (node->rightChild->isBucket() ? 0 : 1);
			node->numParticlesUnder += node->rightChild->numParticlesUnder;
		}
	}
};
		
int xdr_convert_tree(XDR* xdrs, MyTreeNode* node) {
	if(!node->isBucket()) {
		u_int64_t numNodesLeft = node->numNodesLeft;
		u_int64_t numParticlesLeft = node->numParticlesLeft;
		int result = xdr_template(xdrs, &numNodesLeft) && xdr_template(xdrs, &numParticlesLeft);
		if(node->leftChild)
			result = result && xdr_convert_tree(xdrs, node->leftChild);
		if(node->rightChild)
			result = result && xdr_convert_tree(xdrs, node->rightChild);
		return result;
	} else
		return 1;
}

void print_tree(OrientedBox<double> box, MyTreeNode* node, int axis) {
	if(node->isBucket())
		return;
	cout << "Box: " << box << endl;
	cout << "Node: numNodesLeft: " << node->numNodesLeft << " numNodesRight: " << (node->numNodesUnder - node->numNodesLeft) << endl;
	cout << "numParticlesLeft: " << node->numParticlesLeft << " numParticlesRight: " << (node->numParticlesUnder - node->numParticlesLeft) << endl;
	if(node->leftChild)
		print_tree(cutBoxLeft(box, axis), node->leftChild, (axis + 1) % 3);
	if(node->rightChild)
		print_tree(cutBoxRight(box, axis), node->rightChild, (axis + 1) % 3);
}

template <typename ParticleType>
void createAndWriteTree(const string& filename, const unsigned int numParticles, ParticleType* particles, const OrientedBox<float>& boundingBox, bool periodic, double time) {
	if(verbosity > 2)
		cerr << "Calculating keys ..." << endl;
	for(unsigned int i = 0; i < numParticles; ++i)
		particles[i].key = generateKey(particles[i].pos, boundingBox);

	if(verbosity > 2)
		cerr << "Sorting particles ..." << endl;
	ParticleType dummy;
	dummy.key = firstPossibleKey;
	particles[numParticles] = dummy;
	dummy.key = lastPossibleKey;
	particles[numParticles + 1] = dummy;
	sort(particles, particles + numParticles + 2);

	if(verbosity > 2)
		cerr << "Building the tree ..." << endl;
	MyTreeNode* root = new MyTreeNode;
	root->key = firstPossibleKey;
	TreeBuilder<ParticleType> builder(particles, particles + numParticles + 2 - 1, bucketSize);
	builder.buildTree(root);
	if(builder.numParticles != numParticles) {
		cerr << "Number of particles different: " << builder.numParticles << " vs " << numParticles << endl;
	}
	if(builder.numNodes - builder.numBuckets != root->numNodesUnder + 1) {
		cerr << "Number of nodes different: " << builder.numNodes << " vs " << (root->numNodesUnder + 1) << endl;
	}
	if(verbosity > 2)
		cerr << "Tree has " << (builder.numNodes - builder.numBuckets) << " internal nodes and " << builder.numBuckets << " buckets" << endl;

	TreeHeader th;
	th.time = time;
	th.numNodes = builder.numNodes - builder.numBuckets;
	th.numParticles = numParticles;
	th.boundingBox = boundingBox;
	if(periodic)
		th.flags |= TreeHeader::PeriodicFlag;

	//write tree file
	FILE* outfile = fopen(filename.c_str(), "wb");
	XDR xdrs;
	xdrstdio_create(&xdrs, outfile, XDR_ENCODE);
	xdr_template(&xdrs, &th);
	xdr_convert_tree(&xdrs, root);
	xdr_destroy(&xdrs);
	fclose(outfile);
	
	delete root;
}

bool determineBoundingBox(OrientedBox<float>& boundingBox, const int numParticles) {
	OrientedBox<float> unitCube(Vector3D<float>(-0.5, -0.5, -0.5), Vector3D<float>(0.5, 0.5, 0.5));
	float diagonal = (boundingBox.greater_corner - boundingBox.lesser_corner).length();
	float epsilon = max(1E-2, 1.0 / numParticles);
	bool periodic = false;
	if((boundingBox.lesser_corner - unitCube.lesser_corner).length() / diagonal < epsilon
			&& (boundingBox.greater_corner - unitCube.greater_corner).length() / diagonal < epsilon) {
		if(verbosity > 1) {
			cerr << "The bounding box for this file appears to be a unit cube about the origin, using this for key generation" << endl;
			cerr << "Setting the tree to have periodic boundary conditions." << endl;
		}
		periodic = true;
		boundingBox = unitCube;
	} else {
		if(verbosity > 1)
			cerr << "The bounding box for this file is " << boundingBox << ", using longest axis to generate keys" << endl;
		cubize(boundingBox);
		bumpBox(boundingBox, HUGE_VAL);
		if(verbosity > 1)
			cerr << "Resized bounding box is " << boundingBox << endl;
	}

	return periodic;
}

bool convertGasParticles(const string& filenamePrefix, TipsyReader& r) {
	int numParticles = r.getHeader().nsph;
	TipsyStats stats;
	OrientedBox<float> velocityBox;
	OrientedBox<float> boundingBox;
	
	//need two extra particles for boundaries
	TreeParticle<gas_particle>* particles = new TreeParticle<gas_particle>[numParticles + 2];
	gas_particle p;
	
	//read in the gas particles from the tipsy file
	for(int i = 0; i < numParticles; ++i) {
		if(!r.getNextGasParticle(p)) {
			cerr << "Error reading tipsy file!" << endl;
			return false;
		}
		stats.contribute(p);
		velocityBox.grow(p.vel);
		particles[i] = p;
		particles[i].tipsyOrder = i;
	}

	stats.finalize();
	boundingBox = stats.bounding_box;

	bool periodic = determineBoundingBox(boundingBox, numParticles);
	
	//make gas subdirectory
	if(mkdir("gas", 0755))
		return false;
	
	createAndWriteTree("gas/tree", numParticles, particles, boundingBox, periodic, r.getHeader().time);
	
	//write out uid, mass, pos, vel, rho, temp, metals, eps, phi
	FILE* outfile;
	XDR xdrs;
	
	FieldHeader fh;
	fh.time = r.getHeader().time;
	fh.numParticles = numParticles;
	
	fh.dimensions = 1;
	fh.code = TypeHandling::uint32;
	unsigned int minUID = 0;
	unsigned int maxUID = numParticles - 1;
	outfile = fopen("gas/uid", "wb");
	xdrstdio_create(&xdrs, outfile, XDR_ENCODE);
	writeAggregateMember_tipsyOrder(&xdrs, fh, particles + 1, minUID, maxUID);
	xdr_destroy(&xdrs);
	fclose(outfile);	
	
	fh.dimensions = 1;
	fh.code = float32;
	outfile = fopen("gas/mass", "wb");
	xdrstdio_create(&xdrs, outfile, XDR_ENCODE);
	writeAggregateMember_mass(&xdrs, fh, particles + 1, stats.gas_min_mass, stats.gas_max_mass);
	xdr_destroy(&xdrs);
	fclose(outfile);	

	fh.dimensions = 3;
	fh.code = float32;
	outfile = fopen("gas/position", "wb");
	xdrstdio_create(&xdrs, outfile, XDR_ENCODE);
	writeAggregateMember_pos(&xdrs, fh, particles + 1, boundingBox.lesser_corner, boundingBox.greater_corner);
	xdr_destroy(&xdrs);
	fclose(outfile);	

	fh.dimensions = 3;
	fh.code = float32;
	outfile = fopen("gas/velocity", "wb");
	xdrstdio_create(&xdrs, outfile, XDR_ENCODE);
	writeAggregateMember_vel(&xdrs, fh, particles + 1, velocityBox.lesser_corner, velocityBox.greater_corner);
	xdr_destroy(&xdrs);
	fclose(outfile);	

	fh.dimensions = 1;
	fh.code = float32;
	outfile = fopen("gas/density", "wb");
	xdrstdio_create(&xdrs, outfile, XDR_ENCODE);
	writeAggregateMember_rho(&xdrs, fh, particles + 1, stats.gas_min_rho, stats.gas_max_rho);
	xdr_destroy(&xdrs);
	fclose(outfile);	

	fh.dimensions = 1;
	fh.code = float32;
	outfile = fopen("gas/temperature", "wb");
	xdrstdio_create(&xdrs, outfile, XDR_ENCODE);
	writeAggregateMember_temp(&xdrs, fh, particles + 1, stats.gas_min_temp, stats.gas_max_temp);
	xdr_destroy(&xdrs);
	fclose(outfile);	

	fh.dimensions = 1;
	fh.code = float32;
	outfile = fopen("gas/softening", "wb");
	xdrstdio_create(&xdrs, outfile, XDR_ENCODE);
	writeAggregateMember_hsmooth(&xdrs, fh, particles + 1, stats.gas_min_hsmooth, stats.gas_max_hsmooth);
	xdr_destroy(&xdrs);
	fclose(outfile);	

	fh.dimensions = 1;
	fh.code = float32;
	outfile = fopen("gas/metals", "wb");
	xdrstdio_create(&xdrs, outfile, XDR_ENCODE);
	writeAggregateMember_metals(&xdrs, fh, particles + 1, stats.gas_min_metals, stats.gas_max_metals);
	xdr_destroy(&xdrs);
	fclose(outfile);	

	fh.dimensions = 1;
	fh.code = float32;
	outfile = fopen("gas/potential", "wb");
	xdrstdio_create(&xdrs, outfile, XDR_ENCODE);
	writeAggregateMember_phi(&xdrs, fh, particles + 1, stats.gas_min_phi, stats.gas_max_phi);
	xdr_destroy(&xdrs);
	fclose(outfile);	

	delete[] particles;
	
	return true;
}

bool convertDarkParticles(const string& filenamePrefix, TipsyReader& r) {
	int numParticles = r.getHeader().ndark;
	int numParticlesBefore = r.getHeader().nsph;
	TipsyStats stats;
	OrientedBox<float> velocityBox;
	OrientedBox<float> boundingBox;
	
	//need two extra particles for boundaries
	TreeParticle<dark_particle>* particles = new TreeParticle<dark_particle>[numParticles + 2];
	dark_particle p;
	
	//read in the gas particles from the tipsy file
	for(int i = 0; i < numParticles; ++i) {
		if(!r.getNextDarkParticle(p)) {
			cerr << "Error reading tipsy file!" << endl;
			return false;
		}
		stats.contribute(p);
		velocityBox.grow(p.vel);
		particles[i] = p;
		particles[i].tipsyOrder = i + numParticlesBefore;
	}

	stats.finalize();
	boundingBox = stats.bounding_box;

	bool periodic = determineBoundingBox(boundingBox, numParticles);
	
	//make dark subdirectory
	if(mkdir("dark", 0775))
		return false;
	
	createAndWriteTree("dark/tree", numParticles, particles, boundingBox, periodic, r.getHeader().time);
	
	//write out uid, mass, pos, vel, eps, phi
	FILE* outfile;
	XDR xdrs;
	
	FieldHeader fh;
	fh.time = r.getHeader().time;
	fh.numParticles = numParticles;
	
	fh.dimensions = 1;
	fh.code = TypeHandling::uint32;
	unsigned int minUID = numParticlesBefore;
	unsigned int maxUID = numParticlesBefore + numParticles - 1;
	outfile = fopen("dark/uid", "wb");
	xdrstdio_create(&xdrs, outfile, XDR_ENCODE);
	writeAggregateMember_tipsyOrder(&xdrs, fh, particles + 1, minUID, maxUID);
	xdr_destroy(&xdrs);
	fclose(outfile);	
	
	fh.dimensions = 1;
	fh.code = float32;
	outfile = fopen("dark/mass", "wb");
	xdrstdio_create(&xdrs, outfile, XDR_ENCODE);
	writeAggregateMember_mass(&xdrs, fh, particles + 1, stats.dark_min_mass, stats.dark_max_mass);
	xdr_destroy(&xdrs);
	fclose(outfile);	

	fh.dimensions = 3;
	fh.code = float32;
	outfile = fopen("dark/position", "wb");
	xdrstdio_create(&xdrs, outfile, XDR_ENCODE);
	writeAggregateMember_pos(&xdrs, fh, particles + 1, boundingBox.lesser_corner, boundingBox.greater_corner);
	xdr_destroy(&xdrs);
	fclose(outfile);	

	fh.dimensions = 3;
	fh.code = float32;
	outfile = fopen("dark/velocity", "wb");
	xdrstdio_create(&xdrs, outfile, XDR_ENCODE);
	writeAggregateMember_vel(&xdrs, fh, particles + 1, velocityBox.lesser_corner, velocityBox.greater_corner);
	xdr_destroy(&xdrs);
	fclose(outfile);	

	fh.dimensions = 1;
	fh.code = float32;
	outfile = fopen("dark/softening", "wb");
	xdrstdio_create(&xdrs, outfile, XDR_ENCODE);
	writeAggregateMember_eps(&xdrs, fh, particles + 1, stats.dark_min_eps, stats.dark_max_eps);
	xdr_destroy(&xdrs);
	fclose(outfile);	

	fh.dimensions = 1;
	fh.code = float32;
	outfile = fopen("dark/potential", "wb");
	xdrstdio_create(&xdrs, outfile, XDR_ENCODE);
	writeAggregateMember_phi(&xdrs, fh, particles + 1, stats.dark_min_phi, stats.dark_max_phi);
	xdr_destroy(&xdrs);
	fclose(outfile);	

	delete[] particles;
	
	return true;
}

bool convertStarParticles(const string& filenamePrefix, TipsyReader& r) {
	int numParticles = r.getHeader().nstar;
	int numParticlesBefore = r.getHeader().nsph + r.getHeader().ndark;
	TipsyStats stats;
	OrientedBox<float> velocityBox;
	OrientedBox<float> boundingBox;
	
	//need two extra particles for boundaries
	TreeParticle<star_particle>* particles = new TreeParticle<star_particle>[numParticles + 2];
	star_particle p;
	
	//read in the gas particles from the tipsy file
	for(int i = 0; i < numParticles; ++i) {
		if(!r.getNextStarParticle(p)) {
			cerr << "Error reading tipsy file!" << endl;
			return false;
		}
		stats.contribute(p);
		velocityBox.grow(p.vel);
		particles[i] = p;
		particles[i].tipsyOrder = i + numParticlesBefore;
	}

	stats.finalize();
	boundingBox = stats.bounding_box;

	bool periodic = determineBoundingBox(boundingBox, numParticles);
	
	//make star subdirectory
	if(mkdir("star", 0775))
		return false;
	
	createAndWriteTree("star/tree", numParticles, particles, boundingBox, periodic, r.getHeader().time);
	
	//write out uid, mass, pos, vel, metals, tform, eps, phi
	FILE* outfile;
	XDR xdrs;
	
	FieldHeader fh;
	fh.time = r.getHeader().time;
	fh.numParticles = numParticles;
	
	fh.dimensions = 1;
	fh.code = TypeHandling::uint32;
	unsigned int minUID = numParticlesBefore;
	unsigned int maxUID = numParticlesBefore + numParticles - 1;
	outfile = fopen("star/uid", "wb");
	xdrstdio_create(&xdrs, outfile, XDR_ENCODE);
	writeAggregateMember_tipsyOrder(&xdrs, fh, particles + 1, minUID, maxUID);
	xdr_destroy(&xdrs);
	fclose(outfile);	
	
	fh.dimensions = 1;
	fh.code = float32;
	outfile = fopen("star/mass", "wb");
	xdrstdio_create(&xdrs, outfile, XDR_ENCODE);
	writeAggregateMember_mass(&xdrs, fh, particles + 1, stats.star_min_mass, stats.star_max_mass);
	xdr_destroy(&xdrs);
	fclose(outfile);	

	fh.dimensions = 3;
	fh.code = float32;
	outfile = fopen("star/position", "wb");
	xdrstdio_create(&xdrs, outfile, XDR_ENCODE);
	writeAggregateMember_pos(&xdrs, fh, particles + 1, boundingBox.lesser_corner, boundingBox.greater_corner);
	xdr_destroy(&xdrs);
	fclose(outfile);	

	fh.dimensions = 3;
	fh.code = float32;
	outfile = fopen("star/velocity", "wb");
	xdrstdio_create(&xdrs, outfile, XDR_ENCODE);
	writeAggregateMember_vel(&xdrs, fh, particles + 1, velocityBox.lesser_corner, velocityBox.greater_corner);
	xdr_destroy(&xdrs);
	fclose(outfile);	

	fh.dimensions = 1;
	fh.code = float32;
	outfile = fopen("star/metals", "wb");
	xdrstdio_create(&xdrs, outfile, XDR_ENCODE);
	writeAggregateMember_metals(&xdrs, fh, particles + 1, stats.star_min_metals, stats.star_max_metals);
	xdr_destroy(&xdrs);
	fclose(outfile);	

	fh.dimensions = 1;
	fh.code = float32;
	outfile = fopen("star/formationtime", "wb");
	xdrstdio_create(&xdrs, outfile, XDR_ENCODE);
	writeAggregateMember_tform(&xdrs, fh, particles + 1, stats.star_min_tform, stats.star_max_tform);
	xdr_destroy(&xdrs);
	fclose(outfile);	

	fh.dimensions = 1;
	fh.code = float32;
	outfile = fopen("star/softening", "wb");
	xdrstdio_create(&xdrs, outfile, XDR_ENCODE);
	writeAggregateMember_eps(&xdrs, fh, particles + 1, stats.star_min_eps, stats.star_max_eps);
	xdr_destroy(&xdrs);
	fclose(outfile);	

	fh.dimensions = 1;
	fh.code = float32;
	outfile = fopen("star/potential", "wb");
	xdrstdio_create(&xdrs, outfile, XDR_ENCODE);
	writeAggregateMember_phi(&xdrs, fh, particles + 1, stats.star_min_phi, stats.star_max_phi);
	xdr_destroy(&xdrs);
	fclose(outfile);	

	delete[] particles;
	
	return true;
}

int main(int argc, char** argv) {

	bucketSize = 12;
	
#ifdef HAVE_LIBPOPT
	poptOption optionsTable[] = {
		{"verbose", 'v', POPT_ARG_NONE | POPT_ARGFLAG_ONEDASH | POPT_ARGFLAG_SHOW_DEFAULT, 0, 1, "be verbose about what's going on", "verbosity"},
		{"bucket", 'b', POPT_ARG_INT | POPT_ARGFLAG_ONEDASH | POPT_ARGFLAG_SHOW_DEFAULT, &bucketSize, 0, "maximum number of particles in a bucket", "bucketsize"},
		POPT_AUTOHELP
		POPT_TABLEEND
	};
	
	poptContext context = poptGetContext("tipsy2tree", argc, argv, optionsTable, 0);
	
	poptSetOtherOptionHelp(context, " [OPTION ...] tipsyfile");
	
	int rc;
	while((rc = poptGetNextOpt(context)) >= 0) {
		switch(rc) {
			case 1: //increase verbosity
				verbosity++;
				break;
		}
	}
	
	if(rc < -1) {
		cerr << "Argument error: " << poptBadOption(context, POPT_BADOPTION_NOALIAS) << " : " << poptStrerror(rc) << endl;
		poptPrintUsage(context, stderr, 0);
		return 1;
	}
	
	const char* fname = poptGetArg(context);
	
	if(fname == 0) {
		cerr << "You must provide a tipsy file name" << endl;
		poptPrintUsage(context, stderr, 0);
		return 2;
	}
	poptFreeContext(context);
#else
	const char *optstring = "b:v";
	int c;
	while((c=getopt(argc,argv,optstring))>0){
		if(c == -1){
			break;
		}
		switch(c){
			case 'v':
				verbosity++;
				break;
		case 'b':
		    bucketSize = atoi(optarg);
		    break;
		};
	}
	const char *fname;
	if(optind  < argc){
		fname = argv[optind];
	}else{
		cerr << "You must provide a simulation list file" << endl;
		exit(-1);
	}

#endif
	
	if(verbosity > 1)
		cerr << "Loading tipsy file ..." << endl;
	
	TipsyReader r(fname);
	if(!r.status()) {
		cerr << "Couldn't open tipsy file correctly!  Maybe it's not a tipsy file?" << endl;
		return 2;
	}
		
	string basename(fname);
	int slashPos = basename.find_last_of("/");
	if(slashPos != string::npos)
		basename.erase(0, slashPos + 1);
	cerr << "Basename for tree-based files: \"" << basename << "\"" << endl;
	string dirname = basename + ".data";
	
	//make directory for files
	if(mkdir(dirname.c_str(), 0775) || chdir(dirname.c_str())) {
		cerr << "Could not create and move into a directory for the converted files, maybe you don't have permission?" << endl;
		return 3;
	}
	//make output a string for output index
	
	//write xml
	ofstream xmlfile("description.xml");
	xmlfile << "<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>\n<simulation>\n";

	header h = r.getHeader();
	if(verbosity > 2)
		cout << "Tipsy header:\n" << h << endl;
	
	if(h.nsph > 0) {
		convertGasParticles(basename, r);
		xmlfile << "\t<family name=\"gas\">\n";
		xmlfile << "\t\t<attribute name=\"tree\" link=\"gas/tree\"/>\n";
		xmlfile << "\t\t<attribute name=\"uid\" link=\"gas/uid\"/>\n";
		xmlfile << "\t\t<attribute name=\"mass\" link=\"gas/mass\"/>\n";
		xmlfile << "\t\t<attribute name=\"position\" link=\"gas/position\"/>\n";
		xmlfile << "\t\t<attribute name=\"velocity\" link=\"gas/velocity\"/>\n";
		xmlfile << "\t\t<attribute name=\"softening\" link=\"gas/softening\"/>\n";
		xmlfile << "\t\t<attribute name=\"potential\" link=\"gas/potential\"/>\n";
		xmlfile << "\t\t<attribute name=\"density\" link=\"gas/density\"/>\n";
		xmlfile << "\t\t<attribute name=\"temperature\" link=\"gas/temperature\"/>\n";
		xmlfile << "\t\t<attribute name=\"metals\" link=\"gas/metals\"/>\n";
		xmlfile << "\t</family>\n";
	}
	
	if(h.ndark > 0) {
		convertDarkParticles(basename, r);
		xmlfile << "\t<family name=\"dark\">\n";
		xmlfile << "\t\t<attribute name=\"tree\" link=\"dark/tree\"/>\n";
		xmlfile << "\t\t<attribute name=\"uid\" link=\"dark/uid\"/>\n";
		xmlfile << "\t\t<attribute name=\"mass\" link=\"dark/mass\"/>\n";
		xmlfile << "\t\t<attribute name=\"position\" link=\"dark/position\"/>\n";
		xmlfile << "\t\t<attribute name=\"velocity\" link=\"dark/velocity\"/>\n";
		xmlfile << "\t\t<attribute name=\"softening\" link=\"dark/softening\"/>\n";
		xmlfile << "\t\t<attribute name=\"potential\" link=\"dark/potential\"/>\n";
		xmlfile << "\t</family>\n";
	}
	
	if(h.nstar > 0) {
		convertStarParticles(basename, r);
		xmlfile << "\t<family name=\"star\">\n";
		xmlfile << "\t\t<attribute name=\"tree\" link=\"star/tree\"/>\n";
		xmlfile << "\t\t<attribute name=\"uid\" link=\"star/uid\"/>\n";
		xmlfile << "\t\t<attribute name=\"mass\" link=\"star/mass\"/>\n";
		xmlfile << "\t\t<attribute name=\"position\" link=\"star/position\"/>\n";
		xmlfile << "\t\t<attribute name=\"velocity\" link=\"star/velocity\"/>\n";
		xmlfile << "\t\t<attribute name=\"softening\" link=\"star/softening\"/>\n";
		xmlfile << "\t\t<attribute name=\"potential\" link=\"star/potential\"/>\n";
		xmlfile << "\t\t<attribute name=\"metals\" link=\"star/metals\"/>\n";
		xmlfile << "\t\t<attribute name=\"formationtime\" link=\"star/formationtime\"/>\n";
		xmlfile << "\t</family>\n";
	}
	
	xmlfile << "</simulation>\n";
	xmlfile.close();
	
	cerr << "Done." << endl;
}

#ifndef TREE_NODE_H
#define TREE_NODE_H
#include <iostream>
#include <string>
#include "common.h"
#include "Particle.h"

enum NodeType {
  Invalid = 0,

  // All particles under the node are on this PE
  Internal,
  // This is a leaf
  Leaf,
  // This is a leaf with no particles in it
  EmptyLeaf,

  /* 
   * Some of the particles underneath this node are
   * owned by TreePieces that are not on this PE
  */
  Boundary,

  /*
   * None of the particles underneath this node are owned 
   * by TreePieces on this PE, although they may have been
   * fetched from a remote source.
   */
  Remote,
  // Same as above, but this is a leaf
  RemoteLeaf,
  // Same as above, but this is a leaf with no particles in it
  RemoteEmptyLeaf,
};


class Node {
  private:
  Key key_;
  NodeType type_;
  int depth_;
  int ownerStart_;
  int ownerEnd_;
  int home_;

  Particle *particles_;
  int numParticles_;
 
  int numChildren_;
  Node *children_;
  Node *parent_;

  int pending_;

  // useful while debugging
  // tracks the original tree piece that this node
  // belonged to, when it was submitted for merging 
  // -1 means that it was created by the merging PE
  // during the merge procedure. 0 and above gives the
  // original owner tree piece
  int tp_;

  // constructors
  public:
  Node(Key key, int depth, Particle *particles, int numParticles);
  Node();

  Node(const Node &other);

  // get-set methods
  public:
  Node &operator=(const Node &other);
  Node &operator=(Node &other);

  void copyEverythingButParticles(const Node &other);

  Key getKey() const;
  NodeType getType() const;
  int getDepth() const;
  int getOwnerStart() const;
  int getOwnerEnd() const;
  int &home();
  const int &home() const;
  Particle *getParticles();
  const Particle *getParticles() const;
  int getNumParticles() const;
  int getNumChildren() const;
  Node *getChildren();
  Node *getChild(int i);
  const Node *getChild(int i) const;
  const Node *getChildren() const;
  Node *getParent();
  const Node *getParent() const;
  const int &pending() const;
  int &pending();
  const int &tp() const;
  int &tp();

  void setKey(Key k);
  void setType(NodeType t);
  void setDepth(int depth);
  void setOwnerStart(int start);
  void setOwnerEnd(int end);
  void setParticles(Particle *particles);
  void setNumParticles(int np);
  void setChildren(Node *children);
  void setNumChildren(int nc);
  void setParent(Node *parent);

  void allocateChildren(int nChildren);
  void dot(std::ostream &oss) const;

  static NodeType makeRemote(NodeType type);

  // operations on tree nodes
  public:
  void deleteBeneath();

  private:
  // Initialize child fields based on those of parent.
  void initChild(int i, int *splitters, Key childKey, int childDepth);
  void reset();

  public:
  // to flatten to transport over network
  void serialize(Node *placeInBuf, Node *&emptyBuf, int subtreeDepth);
  // to revive pointers from flattened representation
  void deserialize(Node *start, int nn);

  // utilities
  public:
  static std::string TypeString(NodeType type){
    switch(type){
      case Invalid:                     return "Invalid";
      case Internal:                    return "Internal";
      case Leaf:                        return "Leaf";
      case EmptyLeaf:                   return "EmptyLeaf";
      case Boundary:                    return "Boundary";
      case Remote:                      return "Remote";
      case RemoteLeaf:                  return "RemoteLeaf";
      case RemoteEmptyLeaf:             return "RemoteEmptyLeaf";
      default:                          return "BadNode";
    }
  }

  static std::string TypeDotColor(NodeType type){
    switch(type){
      case Invalid:                     return "firebrick1";
      case Internal:                    return "darkolivegreen1";
      case Leaf:                        return "darkolivegreen3";
      case EmptyLeaf:                   return "darksalmon";
      case Boundary:                    return "darkkhaki";
      case Remote:                      return "deepskyblue1";
      case RemoteLeaf:                  return "dodgerblue4";
      case RemoteEmptyLeaf:             return "deeppink";
      default:                          return "black";
    }
  }

  static bool IsInvalid(NodeType type){
    return type == Invalid;
  }

  static bool IsLeaf(NodeType type){
    return (type == Leaf || 
            type == EmptyLeaf || 
            type == RemoteLeaf || 
            type == RemoteEmptyLeaf); 
  }

  static bool IsLocal(NodeType type){
    return (type == Leaf || type == EmptyLeaf || type == Internal);
  }

  static bool IsLocalLeaf(NodeType type){
    return (type == Leaf || type == EmptyLeaf);
  }

  static bool IsInternal(NodeType type){
    return (type == Internal);
  }

  static bool IsBoundary(NodeType type){
    return (type == Boundary);
  }

  static bool IsRemote(NodeType type){
    return (type == Remote || type == RemoteLeaf || type == RemoteEmptyLeaf);
  }

  static bool IsRemoteLeaf(NodeType type){
    return (type == RemoteLeaf || type == RemoteEmptyLeaf);
  }

  static bool IsEmpty(NodeType type){
    return (type == EmptyLeaf || 
            type == RemoteEmptyLeaf);
  }
};


// gives user access to the leaves assigned to a 
// particular tree piece that submitted a tree for
// merging
class LeafIterator {
  CkVec<Node *> *allLeaves_;
  int firstLeafIndex_;
  int nLeaves_;

  int currentLeafOffset_;

  public:
  LeafIterator() : 
    allLeaves_(NULL),
    firstLeafIndex_(-1),
    nLeaves_(0),
    currentLeafOffset_(0)
  {}

  LeafIterator(CkVec<Node *> *allLeaves, int firstLeafIndex) : 
    allLeaves_(allLeaves),
    firstLeafIndex_(firstLeafIndex),
    nLeaves_(0),
    currentLeafOffset_(0)
  {}

  void reset(){
    currentLeafOffset_ = 0;
  }

  Node *next(){
    if(currentLeafOffset_ == nLeaves()) return NULL;

    return (*allLeaves_)[firstLeafIndex_ + currentLeafOffset_++];
  }

  // should be used only by Manager to set number 
  // of leaves for tree piece
  int &nLeaves(){
    return nLeaves_;
  }
};


#endif // TREE_NODE_H

// Implementation of Node functionalities
#include "Node.h"

Node::Node(Key key, int depth, Particle *particles, int numParticles) : 
  key_(key),
  depth_(depth),
  particles_(particles),
  numParticles_(numParticles),
  type_(Invalid),
  ownerStart_(-1),
  ownerEnd_(-1),
  home_(-1),
  numChildren_(0),
  children_(NULL),
  parent_(NULL),
  pending_(-1),
  tp_(-1)
{
}

Node::Node() : 
  key_(Key(0)),
  depth_(-1),
  particles_(NULL),
  numParticles_(-1),
  type_(Invalid),
  ownerStart_(-1),
  ownerEnd_(-1),
  home_(-1),
  numChildren_(0),
  children_(NULL),
  parent_(NULL),
  pending_(-1),
  tp_(-1)
{
}

// copy everything except for:
// 1. children
// 2. parent
Node::Node(const Node &other) : 
  children_(NULL),
  parent_(NULL)
{
  *this = other;
}

// we can't promise to do nothing to particles
// in const version, so don't copy them
Node &Node::operator=(const Node &other){
  copyEverythingButParticles(other);
  return *this;
}

// in non-const version, since we make no 
// promises about doing nothing to particles, 
// we copy everything, including particles
Node &Node::operator=(Node &other){
  copyEverythingButParticles(other);
  particles_ = other.getParticles();
  return *this;
}

void Node::copyEverythingButParticles(const Node &other){
  key_ = other.getKey();
  type_ = other.getType();
  depth_ = other.getDepth();
  ownerStart_ = other.getOwnerStart();
  ownerEnd_ = other.getOwnerEnd();

  // have to copy this, in case it is not -1
  // note that the home is -1 for local nodes, and
  // non-negative for remote ones (since 'home' is 
  // initialized to -1, and we have only reset it to
  // a non-negative value for remote nodes fetched
  // from their owners during tree building).
  // moreover, while unpacking, we must check whether
  // the home is >= 0, and if so retain its value
  home_ = other.home();

  //particles_ = other.getParticles();
  numParticles_ = other.getNumParticles();

  numChildren_ = other.getNumChildren();

  pending_ = other.pending();
  tp_ = other.tp();
}




int Node::getNumParticles() const {
  return numParticles_;
}

Key Node::getKey() const {
  return key_;
}

Particle *Node::getParticles(){
  return particles_;
}

const Particle *Node::getParticles() const {
  return particles_;
}



/*
template<typename TreeDataInterface>
int Node<TreeDataInterface>::getParticleStartIndex() const {
  return particleStartIndex_;
}
*/

Node *Node::getChild(int i){
  CkAssert(i < numChildren_);
  return children_ + i;
}

const Node *Node::getChild(int i) const {
  CkAssert(i < numChildren_);
  return children_ + i;
}

Node *Node::getParent(){
  return parent_;
}

const Node *Node::getParent() const {
  return parent_;
}

Node *Node::getChildren(){
  return children_;
}

const Node *Node::getChildren() const {
  return children_;
}

int Node::getNumChildren() const {
  return numChildren_;
}

NodeType Node::getType() const {
  return type_;
}

int Node::getDepth() const {
  return depth_;
}

int Node::getOwnerStart() const {
  return ownerStart_;
}

int Node::getOwnerEnd() const {
  return ownerEnd_;
}

int &Node::home(){
  return home_;
}

const int &Node::home() const {
  return home_;
}

const int &Node::pending() const {
  return pending_;
}

int &Node::pending(){
  return pending_;
}

const int &Node::tp() const {
  return tp_;
}

int &Node::tp(){
  return tp_;
}

void Node::setKey(Key key){
  key_ = key;
}

void Node::setParent(Node *parent){
  parent_ = parent;
}

void Node::setChildren(Node *children){
  children_ = children;
}

void Node::setNumChildren(int numChildren){
  numChildren_ = numChildren;
}

void Node::setType(NodeType type){
  type_ = type;
}

void Node::setOwnerStart(int ownerStart){
  ownerStart_ = ownerStart;
}

void Node::setOwnerEnd(int ownerEnd){
  ownerEnd_ = ownerEnd;
}

void Node::setNumParticles(int np){
  numParticles_ = np;
}

void Node::allocateChildren(int nChildren){
  children_ = new Node[nChildren];
  numChildren_ = nChildren;
}

void Node::deleteBeneath(){
  if(getChildren() == NULL){
    return;
  }

  CkAssert(getNumChildren() > 0);
  for(int i = 0; i < getNumChildren(); i++){
    getChild(i)->deleteBeneath();
  }

  delete[] getChildren();
  setChildren(NULL);
}



void Node::dot(std::ostream &out) const {
  out << key_ << " [";

  out << "label=\"";
  out << key_ << ", ";
  out << numParticles_ << ", ";
  out << "[" << ownerStart_ << ", " << ownerEnd_ << ")";
  out << "\\n" << tp_; 
  out << "\",";

  out << "color=\"" << TypeDotColor(type_) << "\", ";
  out << "style=\"filled\"";

  out << "];" << std::endl;

  if(IsLocal(getType())) return;

  if(getChildren() == NULL) return;

  for(int i = 0; i < numChildren_; i++){
    const Node *child = getChild(i);
    out << key_ << " -> " << child->getKey() << ";" << std::endl; 
    child->dot(out);
  }
}

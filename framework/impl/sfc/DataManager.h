#ifndef PARATREET_SFC_DATAMANAGER_H
#define PARATREET_SFC_DATAMANAGER_H

#include "sfc.h"

#include <vector>
#include <string>
#include <fstream>

namespace paratreet { namespace sfc {

template <class Meta>
struct DataManager : public CBase_DataManager<Meta> {
    typedef typename Meta::Element Element;
    typedef typename Meta::Reader  Reader;

    DataManager();
    void load(const std::string filename, const CkCallback& cb);
    void countElements(const CkCallback& cb);
    void computeBoundingBox(const CkCallback& cb);

    void _load(const std::string filename);
    Box<3> _computeBoundingBox();
    void _assignKeys();


private:
    std::vector<Element>    elements;
    std::vector<Key>        keys;
    std::vector<Vector<3>>  positions;
    std::size_t             n_elements;
};

// Entry methods
template <class Meta>
DataManager<Meta>::DataManager() : n_elements(0) {

}

template <class Meta>
void DataManager<Meta>::load(const std::string filename, const CkCallback& cb) {
    _load(filename);
    this->contribute(cb);
}

template <class Meta>
void DataManager<Meta>::countElements(const CkCallback& cb) {
    this->contribute(sizeof(std::size_t), &n_elements, Reducer::sum_size, cb);
}

template <class Meta>
void DataManager<Meta>::computeBoundingBox(const CkCallback& cb) {
    Box<3> box = _computeBoundingBox();
    this->contribute(sizeof(Box<3>), &box, Reducer::sum_box3, cb);
}

// Normal methods
template <class Meta>
void DataManager<Meta>::_load(const std::string filename) {
    int err;
    Reader r(filename);
    if (r.error()) {
        log::info(AINFO(DataManager), "Failed to open file ", filename);
        CkExit();
    }

    std::size_t n_pes = CkNumPes();
    std::size_t total = r.getNum();
    n_elements = total / n_pes;
    std::size_t start = n_elements * this->thisIndex;
    std::size_t remainder = total % n_pes;
    if (this->thisIndex < remainder) {
        n_elements += 1;
        start += this->thisIndex;
    } else {
        start += remainder;
    }

    err = r.seek(start);
    if (err) {
        log::info(AINFO(DataManager), "Failed to seek ", start, " of file ", filename);
        CkExit();
    }

    elements.resize(n_elements);
    for (std::size_t i = start; i < start + n_elements; i++) {
        Element &e = elements[i-start];
        err = r.getNext(e);
        if (err) {
            log::info(AINFO(DataManager), "Failed to read element at ", i, " from file ", filename);
            CkExit();
        }
    }
    log::debug(AINFO(DataManager), "Loaded ", n_elements, " elements from file ", filename);
}

template <class Meta>
Box<3> DataManager<Meta>::_computeBoundingBox() {
    Box<3> box;
    for (typename std::vector<Element>::iterator it = elements.begin(); it != elements.end(); ++it) {
        box.add(it->position());
    }
    return box;
}


template <class Meta>
void DataManager<Meta>::_assignKeys() {
    keys.resize(n_elements);
    for (std::size_t i = 0; i < elements.size(); i++) {

    }
}


} } // paratreet::sfc

#endif
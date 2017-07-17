//aggregate_xdr.h

#include "tree_xdr.h"

#define name2(a,b) name2_hidden(a,b)
#define name2_hidden(a,b) a ## b 

#define MAKE_XDR_AGGREGATE(AGGREGATENAME) \
template <typename Aggregate> \
bool name2(xdr_template_aggregate_member_,AGGREGATENAME)(XDR* xdrs, Aggregate* a) { \
	return xdr_template(xdrs, &(a->AGGREGATENAME)); \
}

#define MAKE_AGGREGATE_WRITER(AGGREGATENAME) \
template <typename Aggregate, typename ValueType> \
bool name2(writeAggregateMember_,AGGREGATENAME)(XDR* xdrs, FieldHeader& fh, Aggregate* array, ValueType minValue, ValueType maxValue) { \
	if(!xdr_template(xdrs, &fh) || !xdr_template(xdrs, &minValue) || !xdr_template(xdrs, &maxValue)) \
		return false; \
	for(u_int64_t i = 0; i < fh.numParticles; ++i) { \
		if(!xdr_template(xdrs, &(array[i].AGGREGATENAME))) \
			return false; \
	} \
	return true; \
}

#define MAKE_AGGREGATE_WRITER_REORDER(AGGREGATENAME) \
template <typename Aggregate, typename ValueType, typename IndexType> \
bool name2(writeAggregateMemberReorder_,AGGREGATENAME)(XDR* xdrs, FieldHeader& fh, Aggregate* array, ValueType minValue, ValueType maxValue, IndexType* indexArray) { \
	if(!xdr_template(xdrs, &fh) || !xdr_template(xdrs, &minValue) || !xdr_template(xdrs, &maxValue)) \
		return false; \
	for(u_int64_t i = 0; i < fh.numParticles; ++i) { \
		if(!xdr_template(xdrs, &(array[indexArray[i]].AGGREGATENAME))) \
			return false; \
	} \
	return true; \
}

//Example Usage
//MAKE_AGGREGATE_WRITER(mass)
//writeAggregateMember_mass(xdrs, fh, gas_particles, stats.min_gas_mass, stats.max_gas_mass);
		

#ifndef HILBERT_HINCLUDED
#define HILBERT_HINCLUDED

#if defined(__cplusplus)
extern "C" {
#endif

uint64_t hilbert2d(float x,float y);
uint64_t hilbert3d(float x,float y,float z);
void ihilbert3d(uint64_t s,float *px,float *py,float *pz);
void ihilbert2d(uint64_t s,float *px,float *py,float *pz);

#ifdef BIGKEYS
__uint128_t hilbert2d_double(double x,double y);
__uint128_t hilbert3d_double(double x,double y,double z);
#endif

#if defined(__cplusplus)
}
#endif

#endif

#include "GravityInteraction.h"

#define PARTICLES_PER_THREAD 1
#define BLOCK_SIZE 16

#define gpuSafe(retval) gpuPrintError((retval), __FILE__, __LINE__)
#define gpuCheck() gpuPrintError(cudaGetLastError(), __FILE__, __LINE__)

inline void gpuPrintError(cudaError_t err, const char *file, int line) {
  if (err != cudaSuccess)
    fprintf(stderr,"CUDA Error: %s at %s:%d\n", cudaGetErrorString(err), file, line);
}

__global__ void leafKernel(Particle* from_particles, int from_n_particles,
    Particle* on_particles, int on_n_particles, Vector3D<Real>* sum_forces,
    Real gconst, int n_threads) {
  int gx = blockDim.x * blockIdx.x + threadIdx.x;
  int gy = blockDim.y * blockIdx.y + threadIdx.y;
  int gi = gy * (gridDim.x * blockDim.x) + gx; // Linearized global thread index

  if (gi < n_threads) {
    // TODO: Performance is probably abysmal without shared memory
    for (int i = gi * PARTICLES_PER_THREAD; i < (gi + 1) * PARTICLES_PER_THREAD
        && i < on_n_particles; i++) {
      for (int j = 0; j < from_n_particles; j++) {
        if (on_particles[i].key == on_particles[j].key) {
          continue;
        }

        Vector3D<Real>& from_pos = from_particles[j].position;
        Vector3D<Real>& on_pos = on_particles[i].position;

        Real rsq = (from_pos - on_pos).lengthSquared();
        sum_forces[i] += (from_pos - on_pos) * gconst * from_particles[j].mass
          * on_particles[i].mass / (rsq * sqrt(rsq));
      }
    }
  }
}

__global__ void nonLeafKernel(Vector3D<Real> from_centroid, Real from_sum_mass,
    Particle* on_particles, int on_n_particles, Vector3D<Real>* sum_forces,
    Real gconst, int n_threads) {
  int gx = blockDim.x * blockIdx.x + threadIdx.x;
  int gy = blockDim.y * blockIdx.y + threadIdx.y;
  int gi = gy * (gridDim.x * blockDim.x) + gx; // Linearized global thread index

  if (gi < n_threads) {
    // TODO: Performance is probably abysmal without shared memory
    for (int i = gi * PARTICLES_PER_THREAD; i < (gi + 1) * PARTICLES_PER_THREAD
        && i < on_n_particles; i++) {
      Vector3D<Real>& on_pos = on_particles[i].position;

      Real rsq = (from_centroid - on_pos).lengthSquared();
      sum_forces[i] += (from_centroid - on_pos) * gconst * from_sum_mass
        * on_particles[i].mass / (rsq * sqrt(rsq));
    }
  }
}

void invokeKernel(Particle* from_particles, int from_n_particles,
    const Vector3D<Real>& from_centroid, Real from_sum_mass, Particle* on_particles,
    int on_n_particles, Vector3D<Real>* sum_forces, Real gconst, bool is_leaf) {
  Particle* d_from_particles;
  Vector3D<Real>* d_from_centroid;
  Particle* d_on_particles;
  Vector3D<Real>* d_sum_forces;

  // TODO: Move cudaMalloc and cudaFrees out if possible (important for multiple iterations)
  gpuSafe(cudaMalloc(&d_from_particles, sizeof(Particle) * from_n_particles));
  //gpuSafe(cudaMalloc(&d_from_centroid, sizeof(Vector3D<Real>)));
  gpuSafe(cudaMalloc(&d_on_particles, sizeof(Particle) * on_n_particles));
  gpuSafe(cudaMalloc(&d_sum_forces, sizeof(Vector3D<Real>) * on_n_particles));

  int n_threads = ceil((double)on_n_particles / PARTICLES_PER_THREAD);
  int n_threads_per_dim = ceil(sqrt((double)n_threads));
  int grid_size = ceil((double)n_threads_per_dim / BLOCK_SIZE);

  dim3 block_dim(BLOCK_SIZE, BLOCK_SIZE);
  dim3 grid_dim(grid_size, grid_size);

  if (is_leaf) {
    // TODO: Only transfer necessary portions of particles
    // TODO: Use cudaMemcpyAsync instead
    gpuSafe(cudaMemcpy(d_from_particles, from_particles, sizeof(Particle)
          * from_n_particles, cudaMemcpyHostToDevice));
    //gpuSafe(cudaMemcpy(d_from_centroid, &from_centroid, sizeof(Vector3D<Real>),
    //      cudaMemcpyHostToDevice));
    gpuSafe(cudaMemcpy(d_on_particles, on_particles, sizeof(Particle)
          * on_n_particles, cudaMemcpyHostToDevice));
    gpuSafe(cudaMemcpy(d_sum_forces, sum_forces, sizeof(Vector3D<Real>)
          * on_n_particles, cudaMemcpyHostToDevice));

    // TODO: Use CUDA stream
    leafKernel<<<grid_dim, block_dim>>>(d_from_particles, from_n_particles,
        d_on_particles, on_n_particles, d_sum_forces, gconst, n_threads);
    gpuCheck();

    gpuSafe(cudaMemcpy(sum_forces, d_sum_forces, sizeof(Vector3D<Real>)
          * on_n_particles, cudaMemcpyDeviceToHost));
  }
  else {
    // TODO: Only transfer necessary portions of particles
    // TODO: Use cudaMemcpyAsync instead
    //gpuSafe(cudaMemcpy(d_from_centroid, &from_centroid, sizeof(Vector3D<Real>),
    //      cudaMemcpyHostToDevice));
    gpuSafe(cudaMemcpy(d_on_particles, on_particles, sizeof(Particle)
          * on_n_particles, cudaMemcpyHostToDevice));
    gpuSafe(cudaMemcpy(d_sum_forces, sum_forces, sizeof(Vector3D<Real>)
          * on_n_particles, cudaMemcpyHostToDevice));

    // TODO: Use CUDA stream
    nonLeafKernel<<<grid_dim, block_dim>>>(from_centroid, from_sum_mass,
        d_on_particles, on_n_particles, d_sum_forces, gconst, n_threads);
    gpuCheck();

    gpuSafe(cudaMemcpy(sum_forces, d_sum_forces, sizeof(Vector3D<Real>)
          * on_n_particles, cudaMemcpyDeviceToHost));
  }

  gpuSafe(cudaFree(d_from_particles));
  //gpuSafe(cudaFree(d_from_centroid));
  gpuSafe(cudaFree(d_on_particles));
  gpuSafe(cudaFree(d_sum_forces));
}

#ifndef HISTOGRAM_H
#define HISTOGRAM_H

#ifdef __cplusplus
extern "C" {
#endif

/// Simple parallel histogram implementation.
void histogram(MPI_Comm comm, double* data, long sz, int bins);

#ifdef __cplusplus
}
#endif

#endif

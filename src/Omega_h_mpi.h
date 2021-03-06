#ifndef OMEGA_H_MPI_H
#define OMEGA_H_MPI_H

#include <Omega_h_c.h>

#ifdef OMEGA_H_USE_MPI

OMEGA_H_SYSTEM_HEADER

/* on BlueGene/Q the default install
 * defines MPICH2_CONST to empty if we
 * don't define it first, causing tons
 * of compile errors.
 *
 * in addition, the mpicxx.h header is full
 * of "const MPICH2_CONST", probably as a workaround
 * for the first mistake above.
 * as a result, properly defining MPICH2_CONST to const
 * also causes compile errors.
 * luckily, we can avoid including mpicxx.h with
 * MPICH_SKIP_MPICXX.
 */
#ifdef __bgq__
#define MPICH2_CONST const
#define MPICH_SKIP_MPICXX
#endif

#include <mpi.h>

#endif  // ifdef OMEGA_H_USE_MPI

#endif  // ifndef OMEGA_H_MPI_H

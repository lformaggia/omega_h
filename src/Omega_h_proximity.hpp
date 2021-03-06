#ifndef OMEGA_H_PROXIMITY_HPP
#define OMEGA_H_PROXIMITY_HPP

#include "Omega_h.hpp"

namespace Omega_h {

Reals get_pad_isos(
    Mesh* mesh, Int pad_dim, Real factor, Read<I8> edges_are_bridges);
}  // namespace Omega_h

#endif

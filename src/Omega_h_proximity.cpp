#include "Omega_h_proximity.hpp"

#include "Omega_h_array_ops.hpp"
#include "Omega_h_confined.hpp"
#include "Omega_h_map.hpp"
#include "Omega_h_metric.hpp"
#include "Omega_h_shape.hpp"
#include "Omega_h_simplex.hpp"

namespace Omega_h {

template <Int dim>
static Reals get_edge_pad_isos(
    Mesh* mesh, Real factor, Read<I8> edges_are_bridges) {
  auto coords = mesh->coords();
  auto edges2verts = mesh->ask_verts_of(EDGE);
  auto out = Write<Real>(mesh->nedges(), 0.0);
  auto f = OMEGA_H_LAMBDA(LO edge) {
    if (!edges_are_bridges[edge]) return;
    auto eev2v = gather_verts<2>(edges2verts, edge);
    auto eev2x = gather_vectors<2, dim>(coords, eev2v);
    auto h = norm(eev2x[1] - eev2x[0]) * factor;
    out[edge] = metric_eigenvalue_from_length(h);
  };
  parallel_for(mesh->nedges(), f);
  return out;
}

template <Int dim>
static Reals get_tri_pad_isos(
    Mesh* mesh, Real factor, Read<I8> edges_are_bridges) {
  auto coords = mesh->coords();
  auto tris2verts = mesh->ask_verts_of(TRI);
  auto tris2edges = mesh->ask_down(TRI, EDGE).ab2b;
  auto out = Write<Real>(mesh->ntris(), 0.0);
  auto f = OMEGA_H_LAMBDA(LO tri) {
    auto ttv2v = gather_verts<3>(tris2verts, tri);
    auto ttv2x = gather_vectors<3, dim>(coords, ttv2v);
    auto tte2e = gather_down<3>(tris2edges, tri);
    auto tte2b = gather_scalars<3>(edges_are_bridges, tte2e);
    for (Int ttv = 0; ttv < 3; ++ttv) {
      if (!(tte2b[(ttv + 0) % 3] && tte2b[(ttv + 2) % 3])) continue;
      auto o = ttv2x[ttv];
      auto a = ttv2x[(ttv + 1) % 3];
      auto b = ttv2x[(ttv + 2) % 3];
      auto oa = a - o;
      auto ab = b - a;
      auto nabsq = norm_squared(ab);
      auto proj = ab * ((ab * oa) / nabsq);
      auto d = oa - proj;
      auto lambda = ((ab * d) - (ab * oa)) / nabsq;
      if (!((0 <= lambda) && (lambda <= 1.0))) continue;
      auto h = norm(d) * factor;
      out[tri] = metric_eigenvalue_from_length(h);
    }
  };
  parallel_for(mesh->ntris(), f);
  return out;
}

static Reals get_tet_pad_isos(
    Mesh* mesh, Real factor, Read<I8> edges_are_bridges) {
  auto coords = mesh->coords();
  auto tets2verts = mesh->ask_verts_of(TET);
  auto tets2edges = mesh->ask_down(TET, EDGE).ab2b;
  auto out = Write<Real>(mesh->ntets(), 0.0);
  auto f = OMEGA_H_LAMBDA(LO tet) {
    auto ttv2v = gather_verts<4>(tets2verts, tet);
    auto ttv2x = gather_vectors<4, 3>(coords, ttv2v);
    auto tte2e = gather_down<6>(tets2edges, tet);
    auto tte2b = gather_scalars<6>(edges_are_bridges, tte2e);
    auto nb = sum(tte2b);
    if (nb == 0) return;
    if (nb == 4) {
      for (Int tte = 0; tte < 3; ++tte) {
        if (tte2b[tte]) continue;
        auto opp = opposite_template(TET, EDGE, tte);
        if (tte2b[opp]) continue;
        // at this point we have edge-edge nearness
        auto a = ttv2x[down_template(TET, EDGE, tte, 0)];
        auto b = ttv2x[down_template(TET, EDGE, tte, 1)];
        auto c = ttv2x[down_template(TET, EDGE, opp, 0)];
        auto d = ttv2x[down_template(TET, EDGE, opp, 1)];
        auto ab = b - a;
        auto cd = d - c;
        auto n = normalize(cross(ab, cd));
        auto h = (a - c) * n;
        // project onto the normal plane
        a = a - (n * (n * a));
        b = b - (n * (n * b));
        c = c - (n * (n * c));
        d = d - (n * (n * d));
        if (!((get_triangle_normal(a, b, c) * get_triangle_normal(a, b, d)) <
                    0 &&
                (get_triangle_normal(c, d, a) * get_triangle_normal(c, d, b)) <
                    0)) {
          break;
        }
        out[tet] = metric_eigenvalue_from_length(h);
        return;  // edge-edge implies no plane-vertex
      }
    }
    Real l = 0;  // multiple vertex-planes may occur
    for (Int ttv = 0; ttv < 4; ++ttv) {
      Few<Int, 3> vve2tte;
      Few<Int, 3> vve2wd;
      for (Int vve = 0; vve < 3; ++vve) {
        vve2tte[vve] = up_template(TET, VERT, ttv, vve).up;
        vve2wd[vve] = up_template(TET, VERT, ttv, vve).which_down;
      }
      Few<I8, 3> vve2b;
      for (Int vve = 0; vve < 3; ++vve) vve2b[vve] = tte2b[vve2tte[vve]];
      if (sum(vve2b) != 3) continue;
      // at this point, we have vertex-plane nearness
      auto o = ttv2x[ttv];
      Few<Int, 3> vve2ttv;
      for (Int vve = 0; vve < 3; ++vve) {
        vve2ttv[vve] = down_template(TET, EDGE, vve2tte[vve], 1 - vve2wd[vve]);
      }
      Few<Vector<3>, 3> vve2x;
      for (Int vve = 0; vve < 3; ++vve) vve2x[vve] = ttv2x[vve2ttv[vve]];
      auto a = vve2x[0];
      auto b = vve2x[1];
      auto c = vve2x[2];
      auto ab = b - a;
      auto ac = c - a;
      auto n = normalize(cross(ab, ac));
      auto oa = a - o;
      auto od = n * (n * oa);
      Matrix<3, 2> basis;
      basis[0] = ab;
      basis[1] = ac;
      auto inv_basis = pseudo_invert(basis);
      auto ad = od - oa;
      auto xi = form_barycentric(inv_basis * ad);
      if (!is_barycentric_inside(xi)) continue;
      auto h = norm(od) * factor;
      l = max2(l, metric_eigenvalue_from_length(h));
    }
    out[tet] = l;
  };
  parallel_for(mesh->ntets(), f);
  return out;
}

Reals get_pad_isos(
    Mesh* mesh, Int pad_dim, Real factor, Read<I8> edges_are_bridges) {
  if (pad_dim == EDGE) {
    if (mesh->dim() == 3)
      return get_edge_pad_isos<3>(mesh, factor, edges_are_bridges);
    if (mesh->dim() == 2)
      return get_edge_pad_isos<2>(mesh, factor, edges_are_bridges);
  } else if (pad_dim == TRI) {
    if (mesh->dim() == 3)
      return get_tri_pad_isos<3>(mesh, factor, edges_are_bridges);
    if (mesh->dim() == 2)
      return get_tri_pad_isos<2>(mesh, factor, edges_are_bridges);
  } else if (pad_dim == TET) {
    return get_tet_pad_isos(mesh, factor, edges_are_bridges);
  }
  OMEGA_H_NORETURN(Reals());
}

Reals get_proximity_isos(Mesh* mesh, Real factor) {
  OMEGA_H_CHECK(mesh->owners_have_all_upward(VERT));
  auto edges_are_bridges = find_bridge_edges(mesh);
  auto verts_are_bridged = mark_down(mesh, EDGE, VERT, edges_are_bridges);
  auto bridged_verts = collect_marked(verts_are_bridged);
  auto nbv = bridged_verts.size();
  auto bv2m = Reals(nbv, 0.0);
  for (Int pad_dim = EDGE; pad_dim <= mesh->dim(); ++pad_dim) {
    auto v2p = mesh->ask_graph(VERT, pad_dim);
    auto bv2p = unmap_graph(bridged_verts, v2p);
    auto p2m = get_pad_isos(mesh, pad_dim, factor, edges_are_bridges);
    auto bv2m_tmp = graph_reduce(bv2p, p2m, 1, OMEGA_H_MAX);
    bv2m = max_each(bv2m, bv2m_tmp);
  }
  auto v2m = map_onto(bv2m, bridged_verts, mesh->nverts(), 0.0, 1);
  return mesh->sync_array(VERT, v2m, 1);
}
}  // namespace Omega_h

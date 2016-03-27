#include "gtest/gtest.h"
#include <boost/mpl/equal.hpp>
#include <stencil-composition/stencil-composition.hpp>
#include "tools/verifier.hpp"
#include "unstructured_grid.hpp"

using namespace gridtools;
using namespace enumtype;

namespace soc {

    using backend_t = ::gridtools::backend< Host, icosahedral, Naive >;
    using icosahedral_topology_t = ::gridtools::icosahedral_topology< backend_t >;

    typedef gridtools::interval< level< 0, -1 >, level< 1, -1 > > x_interval;
    typedef gridtools::interval< level< 0, -2 >, level< 1, 1 > > axis;

    struct test_on_cells_functor {
        typedef in_accessor< 0, icosahedral_topology_t::cells, extent< 1 > > in;
        typedef inout_accessor< 1, icosahedral_topology_t::cells > out;
        typedef in_accessor< 2, icosahedral_topology_t::cells, extent< 1 > > ipos;
        typedef in_accessor< 3, icosahedral_topology_t::cells, extent< 1 > > cpos;
        typedef in_accessor< 4, icosahedral_topology_t::cells, extent< 1 > > jpos;
        typedef in_accessor< 5, icosahedral_topology_t::cells, extent< 1 > > kpos;
        typedef boost::mpl::vector6< in, out, ipos, cpos, jpos, kpos > arg_list;

        template < typename Evaluation >
        GT_FUNCTION static void Do(Evaluation const &eval, x_interval) {
            auto ff = [](const double _in, const double _res) -> double { return _in + _res; };

            /**
               This interface checks that the location types are compatible with the accessors
             */
            eval(out()) = eval(on_cells(ff, 0.0, in()));
        }
    };

    bool test(uint_t x, uint_t y, uint_t z, uint_t t_steps) {

        uint_t d1 = x;
        uint_t d2 = y;
        uint_t d3 = z;

        using cell_storage_type = typename backend_t::storage_t<icosahedral_topology_t::cells, double>;

        const uint_t halo_nc = 1;
        const uint_t halo_mc = 1;
        const uint_t halo_k = 0;
//        const uint_t d3 = 6 + halo_k * 2;
//        const uint_t d1 = 6 + halo_nc * 2;
//        const uint_t d2 = 6 + halo_mc * 2;
        icosahedral_topology_t icosahedral_grid(d1, d2, d3);

        auto in_cells = icosahedral_grid.make_storage< icosahedral_topology_t::cells, double >("in");
        auto i_cells = icosahedral_grid.make_storage< icosahedral_topology_t::cells, double >("i");
        auto j_cells = icosahedral_grid.make_storage< icosahedral_topology_t::cells, double >("j");
        auto c_cells = icosahedral_grid.make_storage< icosahedral_topology_t::cells, double >("c");
        auto k_cells = icosahedral_grid.make_storage< icosahedral_topology_t::cells, double >("k");
        auto out_cells = icosahedral_grid.make_storage< icosahedral_topology_t::cells, double >("out");
        auto ref_cells = icosahedral_grid.make_storage< icosahedral_topology_t::cells, double >("ref");

        for (int i = 1; i < d1 - 1; ++i) {
            for (int c = 0; c < icosahedral_topology_t::cells::n_colors::value; ++c) {
                for (int j = 1; j < d2 - 1; ++j) {
                    for (int k = 0; k < d3; ++k) {
                        in_cells(i, c, j, k) =
                            in_cells.meta_data().index(array< uint_t, 4 >{(uint_t)i, (uint_t)c, (uint_t)j, (uint_t)k});
                        i_cells(i, c, j, k) = i;
                        c_cells(i, c, j, k) = c;
                        j_cells(i, c, j, k) = j;
                        k_cells(i, c, j, k) = k;
                    }
                }
            }
        }
        out_cells.initialize(0.0);
        ref_cells.initialize(0.0);

        typedef arg< 0, cell_storage_type > p_in_cells;
        typedef arg< 1, cell_storage_type > p_out_cells;
        typedef arg< 2, cell_storage_type > p_i_cells;
        typedef arg< 3, cell_storage_type > p_c_cells;
        typedef arg< 4, cell_storage_type > p_j_cells;
        typedef arg< 5, cell_storage_type > p_k_cells;

        typedef boost::mpl::vector< p_in_cells, p_out_cells, p_i_cells, p_c_cells, p_j_cells, p_k_cells >
            accessor_list_t;

        gridtools::domain_type< accessor_list_t > domain(
            boost::fusion::make_vector(&in_cells, &out_cells, &i_cells, &c_cells, &j_cells, &k_cells));
        array< uint_t, 5 > di = {halo_nc, halo_nc, halo_nc, d1 - halo_nc - 1, d1};
        array< uint_t, 5 > dj = {halo_mc, halo_mc, halo_mc, d2 - halo_mc - 1, d2};

        gridtools::grid< axis, icosahedral_topology_t > grid_(icosahedral_grid, di, dj);
        grid_.value_list[0] = 0;
        grid_.value_list[1] = d3 - 1;

#ifdef CXX11_ENABLED
        auto
#else
#ifdef __CUDACC__
        gridtools::computation *
#else
        boost::shared_ptr< gridtools::computation >
#endif
#endif
            stencil_ = gridtools::make_computation< backend_t >(
                domain,
                grid_,
                gridtools::make_mss // mss_descriptor
                (execute< forward >(),
                    gridtools::make_esf< test_on_cells_functor, icosahedral_topology_t, icosahedral_topology_t::cells >(
                        p_in_cells(), p_out_cells(), p_i_cells(), p_c_cells(), p_j_cells(), p_k_cells())));
        stencil_->ready();
        stencil_->steady();
        stencil_->run();

#ifdef __CUDACC__
        out_cells.d2h_update();
        in_cells.d2h_update();
#endif

        unstructured_grid ugrid(d1, d2, d3);
        for (uint_t i = halo_nc; i < d1 - halo_nc; ++i) {
            for (uint_t c = 0; c < icosahedral_topology_t::cells::n_colors::value; ++c) {
                for (uint_t j = halo_mc; j < d2 - halo_mc; ++j) {
                    for (uint_t k = 0; k < d3; ++k) {
                        auto neighbours =
                            ugrid.neighbours_of< icosahedral_topology_t::cells, icosahedral_topology_t::cells >(
                                {i, c, j, k});
                        for (auto iter = neighbours.begin(); iter != neighbours.end(); ++iter) {
                            ref_cells(i, c, j, k) += in_cells(*iter);
                        }
                    }
                }
            }
        }

        verifier ver(1e-10);

        array< array< uint_t, 2 >, 4 > halos = {{{halo_nc, halo_nc}, {0, 0}, {halo_mc, halo_mc}, {halo_k, halo_k}}};
        bool result = ver.verify(grid_, ref_cells, out_cells, halos);

#ifdef BENCHMARK
        for(uint_t t=1; t < t_steps; ++t){
            stencil_->run();
        }
        stencil_->finalize();
        std::cout << stencil_->print_meter() << std::endl;
#endif

        return result;
    }

} //namespace soc

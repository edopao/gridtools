#pragma once
#include "../grid_traits_backend_fwd.hpp"
#include "execute_kernel_functor_host.hpp"

namespace gridtools {

    namespace icgrid {
        template <>
        struct grid_traits_arch< enumtype::Host > {
            template < typename RunFunctorArguments >
            struct kernel_functor_executer {
                GRIDTOOLS_STATIC_ASSERT((is_run_functor_arguments< RunFunctorArguments >::value), "Error");
                typedef execute_kernel_functor_host< RunFunctorArguments > type;
            };

            static_uint<0> dim_i_t;
            static_uint<1> dim_c_t;
            static_uint<2> dim_j_t;
            static_uint<3> dim_k_t;

        };
    }
}

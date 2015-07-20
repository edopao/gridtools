/*
 * esf_metafunctions.hpp
 *
 *  Created on: Jul 20, 2015
 *      Author: cosuna
 */

#pragma once

#include <stencil-composition/esf.hpp>
#include <common/generic_metafunctions/is_there_in_sequence.hpp>

namespace gridtools {

template<typename Arg>
struct esf_has_parameter_h{
    template<typename Esf>
    struct apply{
        typedef typename is_there_in_sequence<typename Esf::args_t, Arg>::type type;
    };
};

};


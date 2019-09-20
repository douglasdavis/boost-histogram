// Copyright 2018-2019 Henry Schreiner and Hans Dembinski
//
// Distributed under the 3-Clause BSD License.  See accompanying
// file LICENSE or https://github.com/scikit-hep/boost-histogram for details.

#include <boost/histogram/python/pybind11.hpp>

#include <boost/histogram/accumulators.hpp>
#include <boost/histogram/histogram.hpp>
#include <boost/histogram/make_histogram.hpp>
#include <boost/histogram/python/axis.hpp>
#include <boost/histogram/python/histogram.hpp>
#include <boost/histogram/python/kwargs.hpp>
#include <boost/histogram/python/storage.hpp>
#include <boost/histogram/python/try_cast.hpp>
#include <boost/mp11.hpp>
#include <pybind11/operators.h>
#include <vector>

void register_make_histogram(py::module &m, py::module &hist) {
    m.def(
        "_make_histogram",
        [](py::args t_args, py::kwargs kwargs) -> py::object {
            py::list args = py::cast<py::list>(t_args);
            py::object storage
                = optional_arg(kwargs, "storage", py::cast(storage::double_{}));
            finalize_args(kwargs);

            // Allow a user to forget to add () when calling bh.storage.item
            // HD: I am very conflicted over this. It is handy, but blurs the
            // distinction between classes and objects. For pedagogical reasons, I am
            // against that. "Cuteness hurts",
            // http://www.gotw.ca/publications/advice97.htm, and this looks too cute.
            // Furthermore, there is a reason why I require the user to pass an instance
            // and not just a type or enum. A storage may be run-time configurable.
            // Passing an instance allows to pass options via the ctor of the storage.
            try {
                storage = storage();
            } catch(const py::error_already_set &) {
            }

            // Process the args as necessary for extra shortcuts
            for(size_t i = 0; i < args.size(); i++) {
                // If length three tuples are provided, make regular bins
                if(py::isinstance<py::tuple>(args[i])) {
                    py::tuple arg = py::cast<py::tuple>(args[i]);
                    if(arg.size() == 3) {
                        args[i] = py::cast(
                            new axis::_regular_uoflow(py::cast<unsigned>(arg[0]),
                                                      py::cast<double>(arg[1]),
                                                      py::cast<double>(arg[2]),
                                                      py::str()),
                            py::return_value_policy::take_ownership);
                    } else if(arg.size() == 4) {
                        try {
                            py::cast<double>(arg[3]);
                            throw py::type_error("The fourth argument (metadata) in "
                                                 "the tuple cannot be numeric!");
                        } catch(const py::cast_error &) {
                        }

                        args[i] = py::cast(
                            new axis::_regular_uoflow(py::cast<unsigned>(arg[0]),
                                                      py::cast<double>(arg[1]),
                                                      py::cast<double>(arg[2]),
                                                      py::cast<metadata_t>(arg[3])),
                            py::return_value_policy::take_ownership);
                    } else {
                        throw py::type_error("Only (bins, start, stop) and (bins, "
                                             "start, stop, metadata) tuples accepted");
                    }
                }
            }

            auto axes = py::cast<vector_axis_variant>(args);

            return try_cast<storage::unlimited,
                            storage::double_,
                            storage::int_,
                            storage::atomic_int,
                            storage::weight,
                            storage::profile,
                            storage::weighted_profile>(
                storage, [&axes](auto &&storage) {
                    return py::cast(bh::make_histogram_with(storage, axes),
                                    py::return_value_policy::move);
                });
        },
        "Make any histogram");

    // This factory makes a class that can be used to create histograms and also be used
    // in is_instance
    py::object factory_meta_py
        = py::module::import("boost_histogram.utils").attr("FactoryMeta");

    m.attr("histogram") = factory_meta_py(m.attr("_make_histogram"),
                                          py::make_tuple(hist.attr("_any_double"),
                                                         hist.attr("_any_int"),
                                                         hist.attr("_any_atomic_int"),
                                                         hist.attr("_any_unlimited"),
                                                         hist.attr("_any_weight")));
}
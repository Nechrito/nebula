#include "foundation/stdneb.h"
#include "util/string.h"
#include "math/matrix44.h"
#include "math/float4.h"
#include "math/vector.h"
#include "math/point.h"
#include "game/entity.h"
#include "pybind11/embed.h"
#include "scripting/bindings.h"
#include "pybind11/numpy.h"
#include "basegamefeature/components/transformcomponent.h"

namespace py = pybind11;

//
//

PYBIND11_EMBEDDED_MODULE(foundation, m)
{
#if 0 
    // i dont think its needed?
    py::class_<Util::String>(m, "string")
        .def(py::init<const char*>());
    py::implicitly_convertible<py::str, Util::String>();
#endif
}

PYBIND11_EMBEDDED_MODULE(game, m)
{
    py::class_<Game::Entity>(m, "entity")
        .def(py::init([](Ids::Id32 id) {
        Game::Entity e = id; return e;
    }))
        .def_property("worldtransform",
            [](const Game::Entity &id) -> Math::matrix44
    {
        return Game::TransformComponent::GetWorldTransform(id);
    },
            [](const Game::Entity &id, const Math::matrix44& mat)
    {
        Game::TransformComponent::SetWorldTransform(id, mat);
    });
}


PYBIND11_EMBEDDED_MODULE(nmath, f) {
    py::class_<Math::float4>(f, "float4", py::buffer_protocol())
        .def(py::init([](py::array_t<float> b)
    {
        py::buffer_info info = b.request();
        if (info.size != 4) n_warning("wrong number of elements");
        Math::float4 f;
        f.loadu((float*)info.ptr);
        return f;
    }))
        .def("__getitem__", [](Math::float4&f, ssize_t i)
    {
        return f[i];
    })
        .def("__setitem__", [](Math::float4&f, ssize_t i, float v)
    {
        f[i] = v;
    })
        //FIXME this is probably broken
        .def_buffer([](Math::float4 &m) -> py::buffer_info {
        return py::buffer_info(
            &m[0],                               /* Pointer to buffer */
            { 1, 4 },                 /* Buffer dimensions */
            { 4 * sizeof(float),     /* Strides (in bytes) for each index */
              sizeof(float) }
        );
    });

    py::class_<Math::vector>(f, "vector", py::buffer_protocol())
        .def(py::init([](py::array_t<float> b)
    {
        py::buffer_info info = b.request();
        if (info.size != 3) n_warning("wrong number of elements");
        Math::vector f;
        f.load_float3((float*)info.ptr, 0.0f);
        return f;
    }))
        .def("__getitem__", [](Math::vector&f, ssize_t i)
    {
        return f[i];
    })
        .def("__setitem__", [](Math::vector&f, ssize_t i, float v)
    {
        f[i] = v;
    })
        //FIXME this is probably broken
        .def_buffer([](Math::vector &m) -> py::buffer_info {
        return py::buffer_info(
            &m[0],                               /* Pointer to buffer */
            { 1, 4 },                 /* Buffer dimensions */
            { 4 * sizeof(float),     /* Strides (in bytes) for each index */
              sizeof(float) }
        );
    });

    py::class_<Math::point>(f, "point", py::buffer_protocol())
        .def(py::init([](py::array_t<float> b)
    {
        py::buffer_info info = b.request();
        if (info.size != 3) n_warning("wrong number of elements");
        Math::point f;
        f.load_float3((float*)info.ptr, 1.0f);
        return f;
    }))
        .def("__getitem__", [](Math::point&f, ssize_t i)
    {
        return f[i];
    })
        .def("__setitem__", [](Math::point&f, ssize_t i, float v)
    {
        f[i] = v;
    })
        //FIXME this is probably broken
        .def_buffer([](Math::point &m) -> py::buffer_info {
        return py::buffer_info(
            &m[0],                               /* Pointer to buffer */
            { 1, 4 },                 /* Buffer dimensions */
            { 4 * sizeof(float),     /* Strides (in bytes) for each index */
              sizeof(float) }
        );
    });

    py::class_<Math::matrix44>(f, "matrix", py::buffer_protocol())
        .def(py::init([](py::array_t<float> b)
    {
        py::buffer_info info = b.request();
        if (info.size != 16 || info.ndim != 2 || info.shape[0] != 4 || info.shape[1] != 4) n_warning("wrong number of elements");
        Math::matrix44 f;
        if (info.strides[0] == 4 * sizeof(float))
        {
            f.loadu(static_cast<float*>(info.ptr));
        }
        else
        {
            n_assert("FIXME matrix with non-linear memory layout");
        }
        return f;
    }))
        .def_static("identity", &Math::matrix44::identity)
        .def("__repr__", [](const Math::matrix44&m)// ->py::nstr
    {
        return Util::String::FromMatrix44(m);
    })
        .def("__getitem__", [](Math::matrix44&f, std::pair<ssize_t, ssize_t> i)
    {
        switch (i.first)
        {
        case 0:return f.getrow0()[i.second]; break;
        case 1:return f.getrow1()[i.second]; break;
        case 2:return f.getrow2()[i.second]; break;
        case 3:return f.getrow3()[i.second]; break;
        }
        return 0.0f;
    })
        .def("__setitem__", [](Math::matrix44&f, std::pair<ssize_t, ssize_t> i, float v)
    {
        switch (i.first)
        {
        case 0:f.row0()[i.second] = v; break;
        case 1:f.row1()[i.second] = v; break;
        case 2:f.row2()[i.second] = v; break;
        case 3:f.row3()[i.second] = v; break;
        }
    });
};
/*
py::class_<Util::Array<int>>(f, "ia", py::buffer_protocol())
    .def("__init__", [](Util::Array<int>&a, py::buffer b)
{
    py::buffer_info info = b.request();
    a.Reserve(info.ndim);
    Memory::Copy(info.ptr, a.Begin(), info.ndim * sizeof(int));
}

    )
    .def_buffer([](Util::Array<int>&a)->py::buffer_info
    {
        return py::buffer_info(a.Begin(),
            sizeof(int),
            py::format_descriptor<int>::format(),
            1,
            {1,a.Size()},
            {sizeof(int),a.Size()*sizeof(int)}
        );
    });
}
*/
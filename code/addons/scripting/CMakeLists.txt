#-------------------------------------------------------------------------------
# scripting
#-------------------------------------------------------------------------------

fips_begin_module(scripting)
fips_libs(foundation application)

fips_files(
    scriptconfig.h
    scriptserver.cc
    scriptserver.h
    )
fips_dir(api)
    fips_files(
        application.h
        application.cc
        audio.h
        audio.cc
        render.h
        render.cc
        game.h
        game.cc
    )
fips_dir(python)
    fips_files(
        pythonserver.cc
        pythonserver.h
        deargui.cpp
        pybindings.h
        pybindings.cc
        )
fips_dir(mono)
    fips_files(
        monoserver.cc
        monoserver.h
        monobindings.h
        monobindings.cc
        )
fips_dir(mono/conversion)
    fips_files(
        monoconfig.h
        matrix44.h
        matrix44.cc
    )
fips_end_module()

fips_begin_sharedlib(xna_math)
    enable_language(CSharp)
    fips_dir(mono/xna)
        fips_files(
            BoundingBox.cs
            BoundingFrustum.cs
            BoundingSphere.cs
            Color.cs
            ContainmentType.cs
            MathHelper.cs
            Matrix.cs
            Plane.cs
            PlaneIntersectionType.cs
            Point.cs
            Quaternion.cs
            Ray.cs
            Rectangle.cs
            Vector2.cs
            Vector3.cs
            Vector4.cs
        )
fips_end_sharedlib()
    target_compile_options(xna_math
        PRIVATE "/langversion:6"
        )
    set_target_properties(xna_math PROPERTIES 
        VS_DOTNET_REFERENCES "System.Runtime.Serialization"
        )
    

target_link_libraries(scripting ${PYTHON_LIBRARY_RELEASE} ${MONO_LIBRARIES})
target_link_options(scripting PUBLIC "/WHOLEARCHIVE:scripting")
# target_include_directories(scripting PUBLIC ${MONO_INCLUDE_DIRS})
target_compile_options(scripting PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)

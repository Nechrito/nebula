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
        vector4.h
        vector4.cc
        vector3.h
        vector3.cc
        vector2.h
        vector2.cc
        quaternion.h
        quaternion.cc
        bbox.h
        bbox.cc
        entity.h
        entity.cc
    )
fips_end_module()

fips_begin_sharedlib(nebula_mono)
    enable_language(CSharp)
    fips_dir(mono/csharp/)
        fips_files(
            Main.cs
        )
    fips_dir(mono/csharp/util)
        fips_files(
            String.cs
        )
    fips_dir(mono/csharp/engine)
        fips_files(
            Api.cs
            ConsoleHook.cs
        )
    fips_dir(mono/csharp/xna)
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
            Quaternion.cs
            Ray.cs
            Rectangle.cs
            Vector2.cs
            Vector3.cs
            Vector4.cs
            Point.cs
        )
fips_end_sharedlib()
    target_compile_options(nebula_mono
        PRIVATE "/langversion:6"
        PRIVATE "/unsafe"
        )
    set_target_properties(nebula_mono PROPERTIES 
        VS_DOTNET_REFERENCES "System.Runtime.Serialization"
        )
    # we need to convert .pdb to .mdb files to load debugging symbols correctly
    add_custom_command(TARGET nebula_mono
        COMMAND pdb2mdb ${FIPS_PROJECT_DEPLOY_DIR}/nebula_mono${CMAKE_SHARED_LIBRARY_SUFFIX}
        VERBATIM
        POST_BUILD
        )
    

target_link_libraries(scripting ${PYTHON_LIBRARY_RELEASE} ${MONO_LIBRARIES})
target_link_options(scripting PUBLIC "/WHOLEARCHIVE:scripting")
# target_include_directories(scripting PUBLIC ${MONO_INCLUDE_DIRS})
target_compile_options(scripting PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)

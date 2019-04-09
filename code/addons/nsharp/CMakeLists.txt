#-------------------------------------------------------------------------------
# nsharp
#-------------------------------------------------------------------------------

fips_begin_module(nsharp)
fips_libs(foundation application)

fips_files(
    monoconfig.h
    monoserver.cc
    monoserver.h
    monobindings.h
    monobindings.cc
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
fips_dir(conversion)
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

fips_begin_sharedlib(NebulaEngine)
    enable_language(CSharp)
    fips_dir(NebulaEngine/)
        fips_files(
            Main.cs
        )
    fips_dir(NebulaEngine/util)
        fips_files(
            String.cs
        )
    fips_dir(NebulaEngine/engine)
        fips_files(
            Api.cs
            ConsoleHook.cs
        )
    fips_dir(NebulaEngine/xna)
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
    target_compile_options(NebulaEngine
        PRIVATE "/langversion:6"
        PRIVATE "/unsafe"
        )
    set_target_properties(NebulaEngine PROPERTIES 
        VS_DOTNET_REFERENCES "System.Runtime.Serialization"
        )
    # we need to convert .pdb to .mdb files to load debugging symbols correctly
    add_custom_command(TARGET NebulaEngine
        COMMAND pdb2mdb ${FIPS_PROJECT_DEPLOY_DIR}/NebulaEngine${CMAKE_SHARED_LIBRARY_SUFFIX}
        VERBATIM
        POST_BUILD
        )

target_link_libraries(nsharp ${MONO_LIBRARIES})
target_link_options(nsharp PUBLIC "/WHOLEARCHIVE:nsharp")
# target_include_directories(nsharp PUBLIC ${MONO_INCLUDE_DIRS})
target_compile_options(nsharp PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)

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
fips_dir(python)
    fips_files(
        pythonserver.cc
        pythonserver.h
        deargui.cpp
        pybindings.h
        pybindings.cc
        )
fips_end_module()

SET(SCRIPTING_LIBRARIES ${PYTHON_LIBRARY_RELEASE})

target_link_libraries(scripting ${SCRIPTING_LIBRARIES})
target_link_options(scripting PUBLIC "/WHOLEARCHIVE:scripting")
# target_include_directories(scripting PUBLIC ${MONO_INCLUDE_DIRS})
target_compile_options(scripting PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)

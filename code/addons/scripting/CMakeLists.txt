#-------------------------------------------------------------------------------
# scripting
#-------------------------------------------------------------------------------

fips_begin_module(scripting)
fips_libs(foundation application)

fips_files(scriptserver.cc scriptserver.h bindings.h bindings.cc	
)
fips_dir(python)
    fips_files(pythonserver.cc pythonserver.h deargui.cpp)
fips_dir(mono)
    fips_files(monoserver.cc monoserver.h)
fips_end_module()

# message ( STATUS "" )
# message ( ---------------------------- )
# message ( STATUS "" )
# message ( STATUS "MONO_INCLUDE_DIRS = " ${MONO_INCLUDE_DIRS} )
# message ( STATUS "MONO_LIBRARIES = " ${MONO_LIBRARIES} )
# message ( STATUS "MONO_LIBRARY_DIRS = "${MONO_LIBRARY_DIRS} )
# message ( STATUS "MONO_EXECUTABLE = "${MONO_EXECUTABLE} )
# message ( STATUS "" )
# message ( ---------------------------- )
# message ( STATUS "" )

target_link_libraries(scripting ${PYTHON_LIBRARY_RELEASE} ${MONO_LIBRARIES})
target_link_options(scripting PUBLIC "/WHOLEARCHIVE:scripting")
# target_include_directories(scripting PUBLIC ${MONO_INCLUDE_DIRS})
target_compile_options(scripting PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)

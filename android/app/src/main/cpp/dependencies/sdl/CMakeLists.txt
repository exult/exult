set(DEPENDENCY SDL3)
include(../build_dependency.cmake)

# SDL depends on Vorbis
set_target_properties(${DEPENDENCY} PROPERTIES INTERFACE_LINK_LIBRARIES vorbis)

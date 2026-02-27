set(DEPENDENCY fluidlite)
include(../build_dependency.cmake)

# FluidLite depends on Ogg and vorbis (only if SF3 support is enabled which it isn't for now)
set_target_properties(${DEPENDENCY} PROPERTIES INTERFACE_LINK_LIBRARIES "ogg;vorbis")

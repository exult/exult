set(DEPENDENCY exult-${EXULT_VERSION})
include(../build_dependency.cmake)

# Exult depends on SDL and mt32emu
set_target_properties(${DEPENDENCY} PROPERTIES INTERFACE_LINK_LIBRARIES "SDL2;mt32emu;fluidlite")

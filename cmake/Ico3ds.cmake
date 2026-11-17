# cmake/Ico3ds.cmake: the Nintendo 3DS build (ICO_3DS=ON, the 3ds preset).
#
# Included by the top-level CMakeLists.txt before any target exists, so the
# definitions below reach every source. docs/3DS.md describes the build;
# port/3ds/ holds the 3DS platform files.
#
# The build is cross-compiled with devkitARM through devkitPro's CMake
# toolchain file ($DEVKITPRO/cmake/3DS.cmake), and the program is packaged
# as a .3dsx for the Homebrew Launcher with devkitPro's tools.

if(NOT ICO_3DS)
    return()
endif()

if(NOT DEFINED ENV{DEVKITPRO} AND NOT DEFINED DEVKITPRO)
    message(FATAL_ERROR "ICO_3DS=ON needs devkitPro: set DEVKITPRO and configure with the 3ds "
                        "preset (docs/3DS.md)")
endif()
if(NOT DEFINED DEVKITPRO)
    set(DEVKITPRO "$ENV{DEVKITPRO}")
endif()

# The first milestone is the headless build: the game runs, nothing is drawn
# (docs/3DS.md, "Milestones").
if(NOT ICO_HEADLESS)
    message(FATAL_ERROR "ICO_3DS=ON is headless only for now: configure with -DICO_HEADLESS=ON")
endif()

# ICO_3DS: the 3DS branches of the shared port code.
# The rest are the shared headers' tunables (each one is #ifndef-guarded
# where it is defined), sized for the 3DS's 32-bit address space and its
# 64 MB (Old 3DS) of application memory:
#   ICO_ARENA_PIN_BASE=0      port/platform/arena.h: no pinned arena (the
#                             pinned base only serves save states)
#   ICO_FIBER_POOL_BASE=0     port/platform/fiber.h: no pinned fiber pool
#   ICO_FIBER_STACK_SIZE      128 KB per game thread instead of 256 KB
#   ICO_FIBER_HOST_STACK_SIZE 1 MB for the host fiber instead of 32 MB
#   ICO_FIBER_SLOT_ALIGN      pool slots rounded to a page, not 64 KB
#                             (256 slots of address space: 35 MB, not 80)
add_compile_definitions(
    ICO_3DS=1
    ICO_ARENA_PIN_BASE=0
    ICO_FIBER_POOL_BASE=0
    ICO_FIBER_STACK_SIZE=131072
    ICO_FIBER_HOST_STACK_SIZE=1048576
    ICO_FIBER_SLOT_ALIGN=4096)

# The 3DS sources of the program (the shared ones come from CMakeLists.txt;
# port/3ds/vm_reserve_3ds.c replaces port/platform/vm_reserve.c in
# ico_platform, port/platform/CMakeLists.txt).
set(ICO_3DS_PROGRAM_SOURCES
    port/3ds/main_3ds.c
    port/3ds/heap_3ds.c
    port/3ds/log_3ds.c
    port/3ds/status_3ds.c)

# ico_3ds_package(target): the target's ELF packaged as ico.3dsx, with the
# Homebrew Launcher's title, description, author and icon (an .smdh file).
# The target `ico_3dsx` builds it: cmake --build --preset 3ds.
function(ico_3ds_package target)
    find_program(ICO_SMDHTOOL smdhtool HINTS "${DEVKITPRO}/tools/bin" REQUIRED)
    find_program(ICO_3DSXTOOL 3dsxtool HINTS "${DEVKITPRO}/tools/bin" REQUIRED)
    set(icon "${CMAKE_SOURCE_DIR}/port/3ds/icon.png")
    if(NOT EXISTS "${icon}")
        # libctru's default icon until the port has its own
        set(icon "${DEVKITPRO}/libctru/default_icon.png")
    endif()
    set(smdh "${CMAKE_BINARY_DIR}/ico.smdh")
    set(out "${CMAKE_BINARY_DIR}/ico.3dsx")
    add_custom_command(
        OUTPUT "${smdh}"
        COMMAND "${ICO_SMDHTOOL}" --create "ICO" "ICO for the Nintendo 3DS (unofficial port)"
                "ico_3ds contributors" "${icon}" "${smdh}"
        DEPENDS "${icon}"
        VERBATIM)
    add_custom_command(
        OUTPUT "${out}"
        COMMAND "${ICO_3DSXTOOL}" "$<TARGET_FILE:${target}>" "${out}" "--smdh=${smdh}"
        DEPENDS ${target} "${smdh}"
        VERBATIM)
    add_custom_target(ico_3dsx ALL DEPENDS "${out}")
endfunction()

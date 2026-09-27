idf_build_get_property(obg_target IDF_TARGET)

set(reqs freertos)
# QEMU/hardware builds use lwIP sockets; the Linux host build uses the host stack.
if(NOT obg_target STREQUAL "linux")
    list(APPEND reqs lwip)
endif()

idf_component_register(
    SRCS "sim_link.c"
    INCLUDE_DIRS "include"
    REQUIRES ${reqs}
)

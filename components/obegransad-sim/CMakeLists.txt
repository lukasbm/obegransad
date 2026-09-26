set(reqs freertos)
# QEMU/hardware builds use lwIP sockets; the Linux host build uses the host stack.
if(NOT CONFIG_IDF_TARGET_LINUX)
    list(APPEND reqs lwip)
endif()

idf_component_register(
    SRCS "sim_link.c"
    INCLUDE_DIRS "include"
    REQUIRES ${reqs}
)

set(PUBLIC_SOURCES audio.c
                   config.c
                   crypto.c
                   logger.c
                   proto.c
                   socket.c
                   stream.c
                   string.c
                   types.c
                   utils.c)
set(PRIVATE_SOURCES private/thread.c)

if(WIN32)
  set(ARCH_SOURCES win32/socket.c win32/thread.c win32/types.c)
elseif(UNIX)
  set(ARCH_SOURCES unix/socket.c unix/thread.c)
endif()

add_library(lib STATIC ${PUBLIC_SOURCES}
                       ${PRIVATE_SOURCES}
                       ${ARCH_SOURCES})

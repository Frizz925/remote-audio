cmake_minimum_required(VERSION 3.0.0)
project(remote-audio VERSION 0.1.0)

if(UNIX)
    include_directories(/usr/local/include)
    link_directories(/usr/local/lib)
endif()

add_executable(remote-audio
    src/audio.c
    src/inet.c
    src/http.c
    src/client.c
    src/server.c
)
target_link_libraries(remote-audio soundio)

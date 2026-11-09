# fiaisdk_compiler_options: warning/sanitizer flags for every target built by this project.
# Link it PRIVATE; never add warning flags to individual targets.

add_library(fiaisdk_compiler_options INTERFACE)

if(MSVC)
    target_compile_options(fiaisdk_compiler_options INTERFACE
        /W4 /permissive- /utf-8 /Zc:__cplusplus
        $<$<BOOL:${FIAISDK_WARNINGS_AS_ERRORS}>:/WX>
    )
else()
    target_compile_options(fiaisdk_compiler_options INTERFACE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
        $<$<COMPILE_LANGUAGE:CXX>:-Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual>
        $<$<BOOL:${FIAISDK_WARNINGS_AS_ERRORS}>:-Werror>
    )
    if(FIAISDK_ENABLE_SANITIZERS)
        target_compile_options(fiaisdk_compiler_options INTERFACE
            -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
        target_link_options(fiaisdk_compiler_options INTERFACE -fsanitize=address,undefined)
    endif()
endif()

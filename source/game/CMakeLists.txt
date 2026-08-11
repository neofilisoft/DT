add_executable(Domaintic main.cpp Game.cpp)

target_link_libraries(Domaintic PRIVATE dt_core dt_runtime dt_renderer dt_simulation dt_audio)

set_target_properties(Domaintic PROPERTIES FOLDER "game")

if(DT_PLATFORM_WINDOWS)
    # Console subsystem: M1 is a console harness (ConsoleRenderer writes to
    # stdout). Switching to WIN32 subsystem happens in M2 once a real
    # SDL3/Vulkan-backed window replaces ConsoleRenderer.
    set_target_properties(Domaintic PROPERTIES WIN32_EXECUTABLE FALSE)
endif()

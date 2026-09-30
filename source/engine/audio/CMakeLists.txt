add_library(lacrima_audio STATIC
    AudioEngine.cpp
)

target_include_directories(lacrima_audio PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(lacrima_audio PUBLIC
    lacrima_core
    lacrima_miniaudio
    lacrima_ogg
)

target_compile_features(lacrima_audio PUBLIC cxx_std_20)
set_target_properties(lacrima_audio PROPERTIES FOLDER "engine")



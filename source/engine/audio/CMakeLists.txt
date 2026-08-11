add_library(dt_audio STATIC
    AudioEngine.cpp
)

target_include_directories(dt_audio PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(dt_audio PUBLIC
    dt_core
    dt_miniaudio
)

target_compile_features(dt_audio PUBLIC cxx_std_20)
set_target_properties(dt_audio PROPERTIES FOLDER "engine")

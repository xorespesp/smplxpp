set(TARGET_NAME example-amass)

add_executable(${TARGET_NAME})
target_sources(${TARGET_NAME}
    PRIVATE
        "src/example_amass_main.cc"
        "src/smpl_sequence.cc"
        "src/smpl_sequence.hh"
        "src/smpl_sequence_config.hh"
)

target_compile_features(${TARGET_NAME} PUBLIC cxx_std_17)

target_compile_definitions(${TARGET_NAME} PUBLIC
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

include(FetchTriengine)

target_link_libraries(${TARGET_NAME} PRIVATE 
    Triengine::triengine
    Smplxpp::smplxpp
    example-common
    cnpy)

install(
    TARGETS ${TARGET_NAME}
    RUNTIME DESTINATION "bin")
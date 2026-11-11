cmake_minimum_required(VERSION 3.16)
project(sm_hdlc VERSION 0.1.0 LANGUAGES C)

add_library(sm_hdlc_decoder STATIC src/hdlc_decoder.c)
add_library(sm_hdlc_encoder STATIC src/hdlc_encoder.c)
add_library(sm_hdlc_framing STATIC src/hdlc_framing.c)
target_link_libraries(sm_hdlc_decoder PUBLIC sm_hdlc_framing)
target_link_libraries(sm_hdlc_encoder PUBLIC sm_hdlc_framing)
foreach(target IN ITEMS sm_hdlc_decoder sm_hdlc_encoder sm_hdlc_framing)
    target_include_directories(${target} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ports
    )
    target_compile_features(${target} PUBLIC c_std_99)
    set_target_properties(${target} PROPERTIES C_EXTENSIONS OFF)
endforeach()

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    add_executable(hdlc_decoder_fixed_buffer examples/decoder_fixed_buffer.c)
    target_link_libraries(hdlc_decoder_fixed_buffer PRIVATE sm_hdlc_decoder)
    set_target_properties(hdlc_decoder_fixed_buffer PROPERTIES C_EXTENSIONS OFF)

    add_executable(hdlc_decoder_frame_gc examples/decoder_frame_gc.c)
    target_link_libraries(hdlc_decoder_frame_gc PRIVATE sm_hdlc_decoder)
    set_target_properties(hdlc_decoder_frame_gc PROPERTIES C_EXTENSIONS OFF)

    add_executable(hdlc_encoder_fixed_buffer examples/encoder_fixed_buffer.c)
    target_link_libraries(hdlc_encoder_fixed_buffer PRIVATE sm_hdlc_encoder)
    set_target_properties(hdlc_encoder_fixed_buffer PROPERTIES C_EXTENSIONS OFF)

    add_executable(hdlc_encoder_frame_gc examples/encoder_frame_gc.c)
    target_link_libraries(hdlc_encoder_frame_gc PRIVATE sm_hdlc_encoder)
    set_target_properties(hdlc_encoder_frame_gc PROPERTIES C_EXTENSIONS OFF)

    include(CTest)
    if(BUILD_TESTING)
        add_test(NAME hdlc_decoder_fixed_buffer COMMAND hdlc_decoder_fixed_buffer)
        add_test(NAME hdlc_decoder_frame_gc COMMAND hdlc_decoder_frame_gc)
        add_test(NAME hdlc_encoder_fixed_buffer COMMAND hdlc_encoder_fixed_buffer)
        add_test(NAME hdlc_encoder_frame_gc COMMAND hdlc_encoder_frame_gc)

        add_executable(hdlc_decoder_test tests/hdlc_decoder_test.c)
        target_link_libraries(hdlc_decoder_test PRIVATE sm_hdlc_decoder)
        add_test(NAME hdlc_decoder COMMAND hdlc_decoder_test)

        add_executable(hdlc_decoder_carrier_test
            tests/hdlc_decoder_carrier_test.c)
        target_link_libraries(hdlc_decoder_carrier_test PRIVATE sm_hdlc_decoder)
        add_test(NAME hdlc_decoder_carrier COMMAND hdlc_decoder_carrier_test)

        add_executable(hdlc_encoder_test tests/hdlc_encoder_test.c)
        target_link_libraries(hdlc_encoder_test PRIVATE sm_hdlc_encoder)
        add_test(NAME hdlc_encoder COMMAND hdlc_encoder_test)

        add_executable(hdlc_encoder_fsm_test tests/hdlc_encoder_fsm_test.c)
        target_link_libraries(hdlc_encoder_fsm_test PRIVATE sm_hdlc_encoder)
        add_test(NAME hdlc_encoder_fsm COMMAND hdlc_encoder_fsm_test)

        add_executable(hdlc_encoder_lifecycle_test
            tests/hdlc_encoder_lifecycle_test.c)
        target_link_libraries(hdlc_encoder_lifecycle_test PRIVATE
            sm_hdlc_encoder)
        add_test(NAME hdlc_encoder_lifecycle
            COMMAND hdlc_encoder_lifecycle_test)

        add_executable(hdlc_roundtrip_test tests/hdlc_roundtrip_test.c)
        target_link_libraries(hdlc_roundtrip_test PRIVATE
            sm_hdlc_decoder sm_hdlc_encoder)
        add_test(NAME hdlc_roundtrip COMMAND hdlc_roundtrip_test)

        add_executable(hdlc_framing_test tests/hdlc_framing_test.c)
        target_link_libraries(hdlc_framing_test PRIVATE
            sm_hdlc_decoder sm_hdlc_encoder)
        add_test(NAME hdlc_framing COMMAND hdlc_framing_test)

        add_executable(hdlc_port_test
            tests/hdlc_port_test.c
            src/hdlc_decoder.c
            src/hdlc_encoder.c
        )
        target_include_directories(hdlc_port_test PRIVATE include ports tests)
        target_link_libraries(hdlc_port_test PRIVATE sm_hdlc_framing)
        target_compile_features(hdlc_port_test PRIVATE c_std_99)
        target_compile_definitions(hdlc_port_test PRIVATE
            HDLC_PORT_HEADER="hdlc_test_port.h"
            NDEBUG
        )
        add_test(NAME hdlc_port COMMAND hdlc_port_test)
    endif()
endif()

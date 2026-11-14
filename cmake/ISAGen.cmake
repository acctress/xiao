function(xiao_isagen target)
    cmake_parse_arguments(ARG "" "HEADER;TESTS" "INPUTS" ${ARGN})

    if (NOT ARG_HEADER OR NOT ARG_TESTS OR NOT ARG_INPUTS)
        message(FATAL_ERROR "xiao_isagen(${target}) needs HEADER, TESTS and INPUTS")
    endif()

    get_filename_component(header_dir "${ARG_HEADER}" DIRECTORY)
    get_filename_component(tests_dir "${ARG_TESTS}" DIRECTORY)

    add_custom_command(
        OUTPUT  "${ARG_HEADER}" "${ARG_TESTS}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${header_dir}" "${tests_dir}"
        COMMAND xiao-isagen --header "${ARG_HEADER}" --tests "${ARG_TESTS}" ${ARG_INPUTS}
        DEPENDS xiao-isagen ${ARG_INPUTS}
        COMMENT "xiao-isagen: generating ${target}"
        VERBATIM
    )

    add_custom_target(${target} DEPENDS "${ARG_HEADER}" "${ARG_TESTS}")
    set_target_properties(${target} PROPERTIES FOLDER generated)
endfunction()

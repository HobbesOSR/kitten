IF(NOT GENGETOPT_EXECUTABLE)
        MESSAGE(STATUS "Looking for gengetopt")
        FIND_PROGRAM(GENGETOPT_EXECUTABLE gengetopt)
        IF(GENGETOPT_EXECUTABLE)
                EXECUTE_PROCESS(COMMAND "${GENGETOPT_EXECUTABLE}" --version OUTPUT_VARIABLE _version)
                STRING(REGEX MATCH "[0-9.]+" GENGETOPT_VERSION ${_version})
                SET(GENGETOPT_FOUND TRUE)
        ENDIF(GENGETOPT_EXECUTABLE)
ELSE(NOT GENGETOPT_EXECUTABLE)
        EXECUTE_PROCESS(COMMAND "${GENGETOPT_EXECUTABLE}" --version OUTPUT_VARIABLE _version)
        STRING(REGEX MATCH "[0-9.]+" GENGETOPT_VERSION ${_version})
        SET(GENGETOPT_FOUND TRUE)
ENDIF(NOT GENGETOPT_EXECUTABLE)

IF(GENGETOPT_FOUND)
        MESSAGE(STATUS "Found gengetopt: ${GENGETOPT_EXECUTABLE} (${GENGETOPT_VERSION})")

        IF(NOT GENGETOPT_FLAGS)
                SET(GENGETOPT_FLAGS "")
        ENDIF(NOT GENGETOPT_FLAGS)

        MACRO(ADD_GENGETOPT_FILES SRC_FILES OUT_FILES)
                set(NEW_SOURCE_FILES)
                FOREACH (CURRENT_FILE ${SRC_FILES})
                        GET_FILENAME_COMPONENT(SRCPATH "${CURRENT_FILE}" PATH)
                        GET_FILENAME_COMPONENT(SRCBASE "${CURRENT_FILE}" NAME_WE)
                        SET(OUT "${CMAKE_CURRENT_BINARY_DIR}/${SRCPATH}/${SRCBASE}.c")
                        SET(HEADER "${CMAKE_CURRENT_BINARY_DIR}/${SRCPATH}/${SRCBASE}.h")
                        SET(INFILE "${CMAKE_CURRENT_SOURCE_DIR}/${CURRENT_FILE}")
                        set_source_files_properties( ${OUT} PROPERTIES GENERATED TRUE )
                        set_source_files_properties( ${HEADER} PROPERTIES GENERATED TRUE )
                        ADD_CUSTOM_COMMAND(
                                OUTPUT ${OUT} ${HEADER}
                                COMMAND ${GENGETOPT_EXECUTABLE}
                                ARGS
                                        -C
                                        --input=${INFILE}
                                DEPENDS ${CURRENT_FILE}
                                WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${SRCPATH}"
                                COMMENT "Generating ${OUT} and ${HEADER} from ${CURRENT_FILE}"
                                VERBATIM
                        )

                        LIST(APPEND NEW_SOURCE_FILES ${OUT})
                        LIST(APPEND NEW_SOURCE_FILES ${HEADER})
                        message(STATUS ${NEW_SOURCE_FILES})
                        set_directory_properties( PROPERTIES ADDITONAL_MAKE_CLEAN_FILES
                                "${NEW_SOURCE_FILES}"  )
                ENDFOREACH (CURRENT_FILE)
                SET(${OUT_FILES} ${NEW_SOURCE_FILES})
        ENDMACRO(ADD_GENGETOPT_FILES)

ELSE(GENGETOPT_FOUND)
        MESSAGE(FATAL_ERROR "Could not find gengetopt")
ENDIF(GENGETOPT_FOUND)

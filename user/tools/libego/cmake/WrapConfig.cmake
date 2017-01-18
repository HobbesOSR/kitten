#
# @file WrapConfig.cmake
#       Contains macros for using the wrap script in a build environment.
#
# @author Todd Gamblin
# @date 19 May 2011
if(NOT DEFINED WRAP)
  message(FATAL_ERROR
    "WRAP variable must be set to location of wrap.py before including WrapConfig.cmake!")
endif()

if (NOT Wrap_CONFIG_LOADED)
  set(Wrap_CONFIG_LOADED TRUE)

  # This variable allows users to use the wrap.py script directly, if desired.
  set(Wrap_EXECUTABLE ${WRAP})

  # add_wrapped_file(file_name wrapper_name [flags])
  #
  # This macro adds a command to generate <file_name> from <wrapper_name> to the
  # build.  Properties on <file_name> are also set so that CMake knows that it
  # is generated.
  #
  # Optionally, flags may be supplied to pass to the wrapper generator.
  #
  function(add_wrapped_file file_name wrapper_name)
    set(file_path    ${CMAKE_CURRENT_BINARY_DIR}/${file_name})
    set(wrapper_path ${CMAKE_CURRENT_SOURCE_DIR}/${wrapper_name})

    # Play nice with FindPythonInterp -- use the interpreter if it was found,
    # otherwise use the script directly.
    if (PYTHON_EXECUTABLE)
      set(command ${PYTHON_EXECUTABLE})
      set(script_arg ${Wrap_EXECUTABLE})
    else()
      set(command ${Wrap_EXECUTABLE})
      set(script_arg "")
    endif()

    # Backward compatibility for old FindMPIs that did not have MPI_C_INCLUDE_PATH
    if (NOT MPI_C_INCLUDE_PATH)
      set(MPI_C_INCLUDE_PATH ${MPI_INCLUDE_PATH})
    endif()
    if (NOT MPI_C_COMPILER)
      set(MPI_C_COMPILER ${MPI_COMPILER})
    endif()

    # Play nice with FindMPI.  This will deduce the appropriate MPI compiler to use
    # for generating wrappers
    if (MPI_C_INCLUDE_PATH)
      set(wrap_includes "")
      foreach(include ${MPI_C_INCLUDE_PATH})
        set(wrap_includes ${wrap_includes} -I ${include})
      endforeach()
    endif()
    # Detect the source code language
    set(wrap_lang C)
    get_filename_component(src_ext ${file_name} EXT)
    foreach(cppext .cpp .cxx .cc .C .hpp)
      if ("${src_ext}" STREQUAL "${cppext}")
        set(wrap_lang CXX)
      endif()
    endforeach(cppext)
    if ("${src_ext}" MATCHES ".[Ff][0-9][0-9]")
      set(wrap_lang Fortran)
    endif()
    foreach(fext .f .for .ftn .fpp .F .FOR .FTN .FPP)
      if ("${src_ext}" STREQUAL "${fext}")
        set(wrap_lang Fortran)
      endif()
    endforeach(fext)
    if (CMAKE_${wrap_lang}_COMPILER)
      set(wrap_compiler   -c ${CMAKE_${wrap_lang}_COMPILER})
    else()
      set(wrap_compiler   -c ${CMAKE_C_COMPILER})
    endif()
    if (MPI_${wrap_lang}_COMPILER)
      set(wrap_compiler -c ${MPI_${wrap_lang}_COMPILER})
    endif()

    if (ARGN)
      # Prefer directly passed in flags.
      list(GET ARGN 0 wrap_flags)
    else()
      # Otherwise, look in the source file properties
      get_source_file_property(wrap_flags ${wrapper_name} WRAP_FLAGS)
      if (wrap_flags STREQUAL NOTFOUND)
        # If no spefific flags, grab them from the WRAP_FLAGS environment variable.
        set(wrap_flags "")
        if (NOT WRAP_FLAGS STREQUAL "")
          set(wrap_flags "${WRAP_FLAGS}")
        endif()
      endif()
    endif()

    # Mark target file as generated so the build system knows what to do w/it
    set_source_files_properties(${file_path} PROPERTIES GENERATED TRUE)

    MESSAGE( "** wrap_flags: ${wrap_flags} **" )

    # Add a command to automatically wrap files.
    add_custom_command(
      OUTPUT  ${file_path}
      COMMAND ${command}
      ARGS    ${script_arg} ${wrap_compiler} ${wrap_includes} ${wrap_flags} ${wrapper_path} -o ${file_path}
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
      DEPENDS ${wrapper_path}
      COMMENT "Generating ${wrap_lang} code for ${file_name} from ${wrapper_name}"
      VERBATIM)

    # Add generated files to list of things to be cleaned for the directory.
    get_directory_property(cleanfiles ADDITIONAL_MAKE_CLEAN_FILES)
    list(APPEND cleanfiles ${file_name})
    set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${cleanfiles}")
  endfunction()

endif()

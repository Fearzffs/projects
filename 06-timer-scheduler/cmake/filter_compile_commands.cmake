# Filter CMAKE_EXPORT_COMPILE_COMMANDS output so clangd/IntelliSense do not
# match portfolio headers to googletest/_deps TUs (missing our -I paths).
#
# Usage:
#   cmake -DCOMPILE_COMMANDS_IN=... -DCOMPILE_COMMANDS_OUT=... -P filter_compile_commands.cmake

if(NOT EXISTS "${COMPILE_COMMANDS_IN}")
  return()
endif()

find_program(_python NAMES python3 python)
if(NOT _python)
  message(WARNING "filter_compile_commands: python3 not found; leaving compile_commands.json unchanged")
  return()
endif()

get_filename_component(_script_dir "${CMAKE_SCRIPT_MODE_FILE}" DIRECTORY)
execute_process(
  COMMAND "${_python}" "${_script_dir}/filter_compile_commands.py"
          "${COMPILE_COMMANDS_IN}" "${COMPILE_COMMANDS_OUT}"
  RESULT_VARIABLE _filter_rc
  OUTPUT_VARIABLE _filter_out
  ERROR_VARIABLE _filter_err
)

if(NOT _filter_rc EQUAL 0)
  message(WARNING "filter_compile_commands failed: ${_filter_err}")
else()
  message(STATUS "${_filter_out}")
endif()

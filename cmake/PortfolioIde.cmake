# Include from a portfolio project CMakeLists.txt:
#   include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/PortfolioIde.cmake")
#
# Regenerates the workspace-root compile_commands.json used by clangd so
# tests/*.cpp always see project includes, cross-folder deps, and gtest.

get_filename_component(_PORTFOLIO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_PORTFOLIO_GEN_SCRIPT "${_PORTFOLIO_ROOT}/scripts/generate_compile_commands.py")

find_program(_PORTFOLIO_PYTHON NAMES python3 python)
if(_PORTFOLIO_PYTHON AND EXISTS "${_PORTFOLIO_GEN_SCRIPT}")
  execute_process(
    COMMAND "${_PORTFOLIO_PYTHON}" "${_PORTFOLIO_GEN_SCRIPT}" "${_PORTFOLIO_ROOT}"
    RESULT_VARIABLE _portfolio_gen_rc
    OUTPUT_VARIABLE _portfolio_gen_out
    ERROR_VARIABLE _portfolio_gen_err
  )
  if(_portfolio_gen_rc EQUAL 0)
    message(STATUS "Portfolio IDE: ${_portfolio_gen_out}")
  else()
    message(WARNING "Portfolio IDE compile_commands generation failed: ${_portfolio_gen_err}")
  endif()
else()
  message(WARNING "Portfolio IDE: python3 or generate_compile_commands.py not found")
endif()

# Include from a portfolio project CMakeLists.txt (via PortfolioIde or directly):
#   include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/PortfolioSanitizers.cmake")
#
# Configure with one of:
#   -DPORTFOLIO_ASAN=ON
#   -DPORTFOLIO_TSAN=ON
# Do not enable both at once (Clang/GCC reject that).

option(PORTFOLIO_ASAN "Build with AddressSanitizer" OFF)
option(PORTFOLIO_TSAN "Build with ThreadSanitizer" OFF)

if(PORTFOLIO_ASAN AND PORTFOLIO_TSAN)
  message(FATAL_ERROR "Enable only one of PORTFOLIO_ASAN or PORTFOLIO_TSAN")
endif()

if(PORTFOLIO_ASAN)
  add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=address)
  message(STATUS "Portfolio sanitizers: AddressSanitizer ON")
endif()

if(PORTFOLIO_TSAN)
  add_compile_options(-fsanitize=thread -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=thread)
  message(STATUS "Portfolio sanitizers: ThreadSanitizer ON")
endif()

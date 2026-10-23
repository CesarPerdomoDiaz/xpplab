# xppaut-headless: the original XPPAUT command line, batch mode only.
add_executable(xppaut-headless xppaut_headless.c)
target_link_libraries(xppaut-headless PRIVATE xppengine)

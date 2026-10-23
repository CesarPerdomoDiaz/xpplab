find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
  # Every XPPAUT example model must give bit-identical output to the
  # unmodified XPPAUT 8.0 engine.
  add_test(NAME golden_examples
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/golden.py check
            --exe $<TARGET_FILE:xppaut-headless>)
  set_tests_properties(golden_examples PROPERTIES TIMEOUT 1800)
endif()

find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
  # Every XPPAUT example model must give bit-identical output to the
  # unmodified XPPAUT 8.0 engine.
  add_test(NAME golden_examples
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/golden.py check
            --exe $<TARGET_FILE:xppaut-headless>)
  set_tests_properties(golden_examples PROPERTIES TIMEOUT 1800)
endif()

add_executable(api_smoke api_smoke.c)
target_link_libraries(api_smoke PRIVATE xppengine)

if(Python3_FOUND)
  foreach(model ode/lecar ode/lorenz ode/fhn tstauto/bob)
    string(REPLACE "/" "_" name ${model})
    add_test(NAME api_${name}
      COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/api_check.py
              $<TARGET_FILE:api_smoke> $<TARGET_FILE:xppaut-headless>
              ${PROJECT_SOURCE_DIR}/engine/xppaut/${model}.ode)
  endforeach()
endif()

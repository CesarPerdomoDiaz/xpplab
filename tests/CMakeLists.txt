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

add_executable(auto_smoke auto_smoke.c)
target_link_libraries(auto_smoke PRIVATE xppengine)

if(Python3_FOUND)
  # Headless AUTO: steady states of Morris-Lecar in iapp, then periodic
  # orbits from the first Hopf point.
  add_test(NAME auto_lecar_hopf
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/auto_check.py
            $<TARGET_FILE:auto_smoke>
            ${PROJECT_SOURCE_DIR}/engine/xppaut/ode/lecar.ode iapp hopf
            ${CMAKE_CURRENT_SOURCE_DIR}/golden/auto/lecar_hopf_iapp.txt)
endif()

if(TARGET xpplab)
  # The app's plots: when they follow their data and when the user's zoom
  # holds (the decision only, from the app's headers; no window).
  add_executable(plot_view_check plot_view_check.c)
  target_include_directories(plot_view_check PRIVATE
    ${PROJECT_SOURCE_DIR}/app/src ${PROJECT_SOURCE_DIR}/engine/api
    ${PROJECT_SOURCE_DIR}/app/third_party/cimgui
    ${PROJECT_SOURCE_DIR}/app/third_party/cimplot)
  add_test(NAME app_plot_view COMMAND plot_view_check)
endif()

if(Python3_FOUND AND TARGET xpplab)
  # A scripted --screenshot or --record run exits non-zero and writes
  # <file>.err when it fails, and exits 0 when it works.
  add_test(NAME app_exit_status
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/app_check.py
            $<TARGET_FILE:xpplab> ${PROJECT_SOURCE_DIR}/examples/lecar.ode)
  set_tests_properties(app_exit_status PROPERTIES TIMEOUT 120
                        SKIP_RETURN_CODE 77)
endif()

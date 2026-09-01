add_executable(WienerBoardLayoutTest
  WienerBoardLayoutTest.cpp
  ${REPO_ROOT}/src/wiener/WienerBoardLayout.cpp
)

target_include_directories(WienerBoardLayoutTest PRIVATE
  ${REPO_ROOT}/src/wiener
)

target_link_libraries(WienerBoardLayoutTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(WienerBoardLayoutTest)

add_executable(WienerBoardTextTest
  WienerBoardTextTest.cpp
  ${REPO_ROOT}/src/wiener/WienerBoardText.cpp
  ${REPO_ROOT}/src/wiener/WienerBoardIcons.cpp
  ${REPO_ROOT}/src/wiener/WienerDisplayFont.cpp
)

target_include_directories(WienerBoardTextTest PRIVATE
  ${REPO_ROOT}/src/wiener
  ${REPO_ROOT}/test/wiener_board_icons/stub
)

target_link_libraries(WienerBoardTextTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(WienerBoardTextTest)

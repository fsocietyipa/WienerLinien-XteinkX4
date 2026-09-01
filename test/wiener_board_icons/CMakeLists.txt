add_executable(WienerBoardIconsTest
  WienerBoardIconsTest.cpp
  ${REPO_ROOT}/src/wiener/WienerBoardIcons.cpp
)

target_include_directories(WienerBoardIconsTest PRIVATE
  ${REPO_ROOT}/src/wiener
  ${REPO_ROOT}/test/wiener_board_icons/stub
)

target_link_libraries(WienerBoardIconsTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(WienerBoardIconsTest)

add_executable(WienerLinienParserTest
  WienerLinienParserTest.cpp
  ${REPO_ROOT}/src/wiener/WienerLinienParser.cpp
  ${REPO_ROOT}/lib/JsonParser/StreamingJsonParser.cpp
)

target_include_directories(WienerLinienParserTest PRIVATE
  ${REPO_ROOT}/src/wiener
  ${REPO_ROOT}/lib/JsonParser
)

target_link_libraries(WienerLinienParserTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(WienerLinienParserTest)

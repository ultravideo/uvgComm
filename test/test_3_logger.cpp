#include "../src/logger.h"

#include <gtest/gtest.h>


TEST(LoggerTest, message) {
  Logger::getLogger()->printNormal("LoggerTest", "Test",
                                   {"Test name"}, {"Test Value"});
}

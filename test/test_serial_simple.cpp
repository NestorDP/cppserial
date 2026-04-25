// Copyright 2020-2025 Nestor Neto

#include <gtest/gtest.h>
#include <memory>
#include <string>

// Include libserial headers
#include "libserial/serial.hpp"
#include "libserial/serial_exception.hpp"

// Simple unit tests that don't require actual hardware
class SerialTest : public ::testing::Test {
protected:
void SetUp() override {
  // Test setup if needed
}

void TearDown() override {
  // Test cleanup if needed
}
};

TEST_F(SerialTest, DefaultConstructor) {
  EXPECT_NO_THROW({
    libserial::Serial serial;
  });
}

TEST_F(SerialTest, ConstructorWithInvalidPort) {
  EXPECT_THROW({
    libserial::Serial serial("/dev/nonexistent");
  }, libserial::SerialException);
}

TEST_F(SerialTest, WriteWithEmptyStringView) {
  libserial::Serial serial;

  EXPECT_THROW({
    // Test that write function rejects empty string_view input
    std::string_view empty_message;
    serial.write(empty_message);
  }, libserial::SerialException);
}

TEST_F(SerialTest, APIExists) {
  libserial::Serial serial;

  // close() should not throw when no port is open (safe operation)
  EXPECT_NO_THROW(serial.close());

  // These should all throw exceptions since no port is open,
  // but they test that the API methods exist and are callable
  EXPECT_THROW(serial.flushInputBuffer(), libserial::SerialException);
  EXPECT_THROW(serial.setBaudRate(9600), libserial::SerialException);
  EXPECT_THROW(serial.getAvailableData(), libserial::SerialException);
  EXPECT_THROW(serial.setCanonicalMode(libserial::CanonicalMode::ENABLE),
               libserial::SerialException);


  // Verify read APIs remain available and report unopened-port errors
  std::string buffer;
  EXPECT_THROW(serial.read(buffer), libserial::IOException);
}

TEST_F(SerialTest, CloseWithInvalidFd) {
  libserial::Serial serial;
  serial.setFdForTest(-2);  // Inject invalid fd to force error
  try {
    serial.close();
    FAIL() << "Expected libserial::SerialException";
  }
  catch (const libserial::SerialException& e) {
    std::string msg = e.what();
    EXPECT_EQ(msg, "Error closing port: Bad file descriptor");
  }
}



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

TEST_F(SerialTest, WriteWithSharedPtr) {
  libserial::Serial serial;

  // Test that write function accepts shared_ptr
  auto message = std::make_shared<std::string>("Test message");

  // This will throw since no port is opened, but tests the API
  EXPECT_THROW({
    serial.write(message);
  }, libserial::SerialException);
}

TEST_F(SerialTest, WriteWithNullPtr) {
  libserial::Serial serial;

  // Test that write function handles null pointer
  std::shared_ptr<std::string> null_message;

  EXPECT_THROW({
    serial.write(null_message);
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


  // Test new shared pointer read API
  auto buffer = std::make_shared<std::string>();
  EXPECT_THROW(serial.read(buffer), libserial::IOException);
  EXPECT_THROW(serial.readUntil(buffer, '\n'), libserial::IOException);
}

TEST_F(SerialTest, ReadWithNullSharedPtr) {
  libserial::Serial serial;

  // Test that read function handles null shared pointer
  std::shared_ptr<std::string> null_buffer;

  EXPECT_THROW({ serial.read(null_buffer); }, libserial::SerialException);
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

TEST_F(SerialTest, WriteRawNullBuffer) {
  libserial::Serial serial_port;
  EXPECT_THROW({
    try {
      serial_port.writeRaw(nullptr, 10);
    } catch (const libserial::IOException& e) {
      EXPECT_STREQ("Invalid buffer passed to writeRaw", e.what());
      throw;
    }
  }, libserial::IOException);
}

TEST_F(SerialTest, WriteRawZeroSize) {
  libserial::Serial serial_port;
  uint8_t dummy = 0;
  EXPECT_THROW({
    try {
      serial_port.writeRaw(&dummy, 0);
    } catch (const libserial::IOException& e) {
      EXPECT_STREQ("Invalid buffer passed to writeRaw", e.what());
      throw;
    }
  }, libserial::IOException);
}

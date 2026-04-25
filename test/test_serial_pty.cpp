// Copyright 2020-2025 Nestor Neto

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "libserial/serial.hpp"
#include "libserial/serial_exception.hpp"

// Integration test using pseudo-terminals
class PseudoTerminalTest : public ::testing::Test {
protected:
int master_fd_{-1};
int slave_fd_{-1};
std::string slave_port_;

void SetUp() override {
  // Create pseudo-terminal pair
  master_fd_ = posix_openpt(O_RDWR | O_NOCTTY);
  if (master_fd_ == -1) {
    FAIL() << "Failed to open master pseudo-terminal";
    return;
  }

  if (grantpt(master_fd_) == -1 || unlockpt(master_fd_) == -1) {
    close(master_fd_);
    FAIL() << "Failed to setup master pseudo-terminal";
    return;
  }

  char *slave_name = ptsname(master_fd_);
  if (!slave_name) {
    close(master_fd_);
    FAIL() << "Failed to get slave pseudo-terminal name";
    return;
  }

  slave_port_ = std::string(slave_name);

  // Open slave end for internal testing
  slave_fd_ = open(slave_name, O_RDWR | O_NOCTTY);
  if (slave_fd_ == -1) {
    close(master_fd_);
    FAIL() << "Failed to open slave pseudo-terminal";
    return;
  }

  errors_poll_ = {
    {EAGAIN, "Resource temporarily unavailable"},
    {ENOMEM, "Cannot allocate memory"},
    {EINVAL, "Invalid argument"},
    {EPERM, "Operation not permitted"},
    {EBADF, "Bad file descriptor"},
    {EEXIST, "File exists"},
    {ENOENT, "No such file or directory"},
    {EINTR, "Interrupted system call"}
  };

  errors_read_ = {
    {EBADF, "Bad file descriptor"},
    {EIO, "Input/output error"},
    {EINTR, "Interrupted system call"},
    {EAGAIN, "Resource temporarily unavailable"},
    {EWOULDBLOCK, "Resource temporarily unavailable"},
    {EISDIR, "Is a directory"}
  };
}

void TearDown() override {
  if (master_fd_ != -1) close(master_fd_);
  if (slave_fd_ != -1) close(slave_fd_);
}

std::vector<std::pair<int, std::string> > errors_poll_;
std::vector<std::pair<int, std::string> > errors_read_;
};

TEST_F(PseudoTerminalTest, OpenClosePort) {
  libserial::Serial serial_port;

  // Test opening the port
  EXPECT_NO_THROW({ serial_port.open(slave_port_); });

  // Test closing the port
  EXPECT_NO_THROW({ serial_port.close(); });
}

TEST_F(PseudoTerminalTest, ParameterizedConstructor) {
  libserial::Serial serial_port(slave_port_);
}

TEST_F(PseudoTerminalTest, SetAndGetBaudRate) {
  libserial::Serial serial_port;

  serial_port.open(slave_port_);

  // Set baud rate using int
  EXPECT_NO_THROW({ serial_port.setBaudRate(9600); });

  // Get baud rate and verify
  int baud_rate = 0;
  EXPECT_NO_THROW({ baud_rate = serial_port.getBaudRate(); });
  EXPECT_EQ(baud_rate, 9600);

  // Set baud rate using BaudRate enum
  EXPECT_NO_THROW({ serial_port.setBaudRate(libserial::BaudRate::BAUD_RATE_115200); });

  // Get the baud rate and verify
  EXPECT_NO_THROW({ baud_rate = serial_port.getBaudRate(); });
  EXPECT_EQ(baud_rate, 115200);

  serial_port.close();
}

TEST_F(PseudoTerminalTest, SetGetReadTimeout) {
  libserial::Serial serial_port;

  serial_port.open(slave_port_);

  // Set read timeout
  std::chrono::milliseconds timeout_set{1500};
  EXPECT_NO_THROW({ serial_port.setReadTimeout(timeout_set); });

  // Get read timeout and verify
  std::chrono::milliseconds timeout_get{0};
  EXPECT_NO_THROW({ timeout_get = serial_port.getReadTimeout(); });
  EXPECT_EQ(timeout_get.count(), timeout_set.count());

  serial_port.close();
}

TEST_F(PseudoTerminalTest, SetGetMinNumberCharRead) {
  libserial::Serial serial_port;

  serial_port.open(slave_port_);

  // Set minimum number of characters to read
  uint16_t min_chars_set{5};
  EXPECT_NO_THROW({ serial_port.setMinNumberCharRead(min_chars_set); });

  // Get minimum number of characters to read and verify
  uint16_t min_chars_get{0};
  EXPECT_NO_THROW({ min_chars_get = serial_port.getMinNumberCharRead(); });
  EXPECT_EQ(min_chars_get, min_chars_set);

  serial_port.close();
}

TEST_F(PseudoTerminalTest, SetParity) {
  libserial::Serial serial_port;

  serial_port.open(slave_port_);

  // Set parity to ENABLE
  EXPECT_NO_THROW({ serial_port.setParity(libserial::Parity::ENABLE); });

  // Set parity to ODD
  EXPECT_NO_THROW({ serial_port.setParity(libserial::Parity::DISABLE); });

  serial_port.close();
}

TEST_F(PseudoTerminalTest, SetStopBits) {
  libserial::Serial serial_port;

  serial_port.open(slave_port_);

  // Set stop bits to 1
  EXPECT_NO_THROW({ serial_port.setStopBits(libserial::StopBits::ONE); });

  // Set stop bits to 2
  EXPECT_NO_THROW({ serial_port.setStopBits(libserial::StopBits::TWO); });

  serial_port.close();
}

TEST_F(PseudoTerminalTest, GetAvailableData) {
  libserial::Serial serial_port;

  serial_port.open(slave_port_);
  serial_port.setBaudRate(9600);

  // First check that no data is available initially
  int initial_available{0};
  EXPECT_NO_THROW({ initial_available = serial_port.getAvailableData(); });
  EXPECT_EQ(initial_available, 0);

  const std::string test_message = "Hello World!\n";
  ssize_t bytes_written = write(master_fd_, test_message.c_str(), test_message.length());
  ASSERT_GT(bytes_written, 0) << "Failed to write to master end";

  // Force flush and give more time for data to propagate
  fsync(master_fd_);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Now check available data again - should match what we wrote
  int available{0};
  EXPECT_NO_THROW({ available = serial_port.getAvailableData(); });
  EXPECT_EQ(available, bytes_written);
}

TEST_F(PseudoTerminalTest, WriteTest) {
  libserial::Serial serial_port;

  serial_port.open(slave_port_);
  serial_port.setBaudRate(115200);

  std::string_view test_data("Test Write Data");

  EXPECT_NO_THROW({ serial_port.write(test_data); });

  // Give time for data to propagate
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // Try to read from master end to verify
  char buffer[100] = {0};
  ssize_t bytes_read = read(master_fd_, buffer, sizeof(buffer) - 1);

  std::string received(buffer, bytes_read);

  EXPECT_EQ(received, std::string(test_data));
}

TEST_F(PseudoTerminalTest, WriteRawBasic) {
  libserial::Serial serial_port;

  serial_port.open(slave_port_);
  serial_port.setCanonicalMode(libserial::CanonicalMode::DISABLE);
  serial_port.setBaudRate(115200);

  std::vector<uint8_t> data = {0x00, 0xFF, 0x10, 0x41, 0x00};

  EXPECT_NO_THROW({
    ssize_t written = serial_port.writeRaw(data.data(), data.size());
    EXPECT_EQ(written, data.size());
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  uint8_t buffer[100] = {0};
  ssize_t bytes_read = read(master_fd_, buffer, sizeof(buffer));

  ASSERT_EQ(bytes_read, data.size());
  EXPECT_EQ(std::vector<uint8_t>(buffer, buffer + bytes_read), data);
}

TEST_F(PseudoTerminalTest, WriteRawPartialWrites) {
  libserial::Serial serial_port;
  serial_port.open(slave_port_);
  serial_port.setCanonicalMode(libserial::CanonicalMode::DISABLE);

  std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6};

  size_t call_count = 0;

  serial_port.setWriteSystemFunction(
    [&call_count](int, const void* buf, size_t len) -> ssize_t {
    call_count++;

    // Simulate partial writes (2 bytes per call)
    size_t to_write = std::min<size_t>(2, len);
    return to_write;
  });

  ssize_t written = serial_port.writeRaw(data.data(), data.size());

  EXPECT_EQ(written, data.size());
  EXPECT_GT(call_count, 1);  // ensure loop was used
}

TEST_F(PseudoTerminalTest, WriteRawWithEINTR) {
  libserial::Serial serial_port;
  serial_port.open(slave_port_);
  serial_port.setCanonicalMode(libserial::CanonicalMode::DISABLE);

  std::vector<uint8_t> data = {1, 2, 3};

  int call_count = 0;

  serial_port.setWriteSystemFunction(
    [&call_count](int, const void*, size_t len) -> ssize_t {
    if (call_count++ == 0) {
      errno = EINTR;
      return -1;
    }
    return len;
  });

  EXPECT_NO_THROW({
    ssize_t written = serial_port.writeRaw(data.data(), data.size());
    EXPECT_EQ(written, data.size());
  });
}

TEST_F(PseudoTerminalTest, WriteRawWithError) {
  libserial::Serial serial_port;
  serial_port.open(slave_port_);
  serial_port.setCanonicalMode(libserial::CanonicalMode::DISABLE);

  std::vector<uint8_t> data = {1, 2, 3};

  serial_port.setWriteSystemFunction(
    [](int, const void*, size_t) -> ssize_t {
    errno = EIO;
    return -1;
  });

  EXPECT_THROW({
    try {
      serial_port.writeRaw(data.data(), data.size());
    }
    catch (const libserial::IOException& e) {
      EXPECT_STREQ("Error writing raw data: Input/output error", e.what());
      throw;
    }
  }, libserial::IOException);
}

TEST_F(PseudoTerminalTest, WriteRawLargeBuffer) {
  libserial::Serial serial_port;
  serial_port.open(slave_port_);
  serial_port.setCanonicalMode(libserial::CanonicalMode::DISABLE);

  EXPECT_NO_THROW({
    std::vector<uint8_t> data(4096, 0xAA);
    ssize_t written = serial_port.writeRaw(data.data(), data.size());
    EXPECT_EQ(written, data.size());
  });
}

TEST_F(PseudoTerminalTest, WriteRawPollTimeout) {
  libserial::Serial serial;

  serial.setFdForTest(slave_fd_);
  serial.setCanonicalMode(libserial::CanonicalMode::DISABLE);
  serial.setWriteTimeout(std::chrono::milliseconds(100));

  // poll always times out
  serial.setPollSystemFunction(
    [](struct pollfd*, nfds_t, int) {
    return 0;
  });

  uint8_t data[10] = {0};

  ssize_t written = serial.writeRaw(data, sizeof(data));

  EXPECT_EQ(written, 0);
}

TEST_F(PseudoTerminalTest, WriteRawNullBuffer) {
  libserial::Serial serial_port;
  serial_port.open(slave_port_);  // Open a valid port to avoid fd errors
  serial_port.setCanonicalMode(libserial::CanonicalMode::DISABLE);
  EXPECT_THROW({
    try {
      serial_port.writeRaw(nullptr, 10);
    }
    catch (const libserial::IOException& e) {
      EXPECT_STREQ("Invalid buffer passed to writeRaw", e.what());
      throw;
    }
  }, libserial::IOException);
}

TEST_F(PseudoTerminalTest, WriteRawZeroSize) {
  libserial::Serial serial_port;
  serial_port.open(slave_port_);  // Open a valid port to avoid fd errors
  serial_port.setCanonicalMode(libserial::CanonicalMode::DISABLE);
  uint8_t dummy = 0;
  EXPECT_THROW({
    try {
      serial_port.writeRaw(&dummy, 0);
    }
    catch (const libserial::IOException& e) {
      EXPECT_STREQ("Invalid buffer passed to writeRaw", e.what());
      throw;
    }
  }, libserial::IOException);
}

TEST_F(PseudoTerminalTest, ReadCanonicalMode) {
  libserial::Serial serial_port;

  serial_port.open(slave_port_);
  serial_port.setBaudRate(9600);

  const std::string test_message{"Read canonical mode test!\n"};

  ssize_t bytes_written = write(master_fd_, test_message.c_str(), test_message.length());
  ASSERT_GT(bytes_written, 0) << "Failed to write to master end";

  // Give time for data to propagate
  fsync(master_fd_);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  {
    // Test reading with shared pointer
    std::string read_buffer;
    size_t bytes_read = 0;

    EXPECT_NO_THROW({ bytes_read = serial_port.read(read_buffer); });

    EXPECT_EQ(bytes_read, test_message.length());
    EXPECT_EQ(read_buffer, test_message);
  }
}

TEST_F(PseudoTerminalTest, ReadNonCanonicalMode) {
  libserial::Serial serial_port;

  serial_port.open(slave_port_);
  serial_port.setBaudRate(9600);
  serial_port.setCanonicalMode(libserial::CanonicalMode::DISABLE);

  const std::string test_message{"Non-Canonical Test\n"};

  ssize_t bytes_written = write(master_fd_, test_message.c_str(), test_message.length());
  ASSERT_GT(bytes_written, 0) << "Failed to write to master end";

  // Give time for data to propagate
  fsync(master_fd_);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  EXPECT_THROW({
    try {
      // Attempt to read using read() - should throw exception
      std::string read_buffer;
      serial_port.read(read_buffer);
    }
    catch (const libserial::IOException& e) {
      EXPECT_STREQ(
        "read() is not supported in non-canonical mode; use readBytes(), readUntil() or readRaw() instead",
        e.what());
      throw;
    }
  }, libserial::IOException);
}

TEST_F(PseudoTerminalTest, ReadTimeout) {
  libserial::Serial serial_port;

  serial_port.open(slave_port_);
  serial_port.setBaudRate(9600);

  // Set a short read timeout
  int time_out_ms = 100;
  serial_port.setReadTimeout(std::chrono::milliseconds(time_out_ms));

  auto expected_what = "Read operation timed out after " + std::to_string(time_out_ms) +
                       " milliseconds";

  EXPECT_THROW({
    try {
      std::string read_buffer;
      serial_port.read(read_buffer);
    }
    catch (const libserial::IOException& e) {
      EXPECT_STREQ(expected_what.c_str(), e.what());
      throw;
    }
  }, libserial::IOException);
}

TEST_F(PseudoTerminalTest, ReadWithReadFail) {
  libserial::Serial serial_port;
  std::string read_buffer;

  for (const auto& [error_num, error_msg] : errors_read_) {
    serial_port.setPollSystemFunction(
      [](struct pollfd*, nfds_t, int) -> int {
      return 1;
    });
    serial_port.setReadSystemFunction(
      [error_num](int, void*, size_t) -> ssize_t {
      errno = error_num;
      return -1;
    });

    auto expected_what = "Error reading from serial port: " + error_msg;

    EXPECT_THROW({
      try {
        serial_port.read(read_buffer);
      }
      catch (const libserial::IOException& e) {
        EXPECT_STREQ(expected_what.c_str(), e.what());
        throw;
      }
    }, libserial::IOException);
  }
}

TEST_F(PseudoTerminalTest, ReadWithPollFail) {
  libserial::Serial serial_port;
  std::string read_buffer;

  for (const auto& [error_num, error_msg] : errors_poll_) {
    serial_port.setPollSystemFunction(
      [error_num](struct pollfd*, nfds_t, int) -> int {
      errno = error_num;
      return -1;
    });

    auto expected_what = "Error in poll(): " + error_msg;

    EXPECT_THROW({
      try {
        serial_port.read(read_buffer);
      }
      catch (const libserial::IOException& e) {
        EXPECT_STREQ(expected_what.c_str(), e.what());
        throw;
      }
    }, libserial::IOException);
  }
}

TEST_F(PseudoTerminalTest, ReadBytesNonCanonicalMode) {
  libserial::Serial serial_port;

  serial_port.open(slave_port_);
  serial_port.setBaudRate(9600);
  serial_port.setCanonicalMode(libserial::CanonicalMode::DISABLE);

  const std::string test_message{"ReadBytes Test!"};

  ssize_t bytes_written = write(master_fd_, test_message.c_str(), test_message.length());
  ASSERT_GT(bytes_written, 0) << "Failed to write to master end";

  // Give time for data to propagate
  fsync(master_fd_);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Test reading with shared pointer
  std::string read_buffer;
  size_t bytes_read = 0;

  EXPECT_NO_THROW({ bytes_read = serial_port.readBytes(read_buffer, test_message.length()); });

  EXPECT_EQ(bytes_read, test_message.length());
  EXPECT_EQ(read_buffer, test_message);
}

TEST_F(PseudoTerminalTest, ReadBytesWithInvalidNumBytes) {
  libserial::Serial serial_port;

  serial_port.open(slave_port_);
  serial_port.setBaudRate(9600);
  serial_port.setCanonicalMode(libserial::CanonicalMode::DISABLE);

  EXPECT_THROW({
    try {
      std::string read_buffer;
      serial_port.readBytes(read_buffer, 0);
    }
    catch (const libserial::IOException& e) {
      EXPECT_STREQ("Number of bytes requested must be greater than zero", e.what());
      throw;
    }
  }, libserial::IOException);
}

TEST_F(PseudoTerminalTest, ReadBytesWithReadFail) {
  libserial::Serial serial_port;

  serial_port.open(slave_port_);
  serial_port.setBaudRate(9600);
  serial_port.setCanonicalMode(libserial::CanonicalMode::DISABLE);

  std::string read_buffer;

  for (const auto& [error_num, error_msg] : errors_read_) {
    serial_port.setReadSystemFunction(
      [error_num](int, void*, size_t) -> ssize_t {
      errno = error_num;
      return -1;
    });

    auto expected_what = "Error reading from serial port: " + error_msg;

    EXPECT_THROW({
      try {
        serial_port.readBytes(read_buffer, 10);
      }
      catch (const libserial::IOException& e) {
        EXPECT_STREQ(expected_what.c_str(), e.what());
        throw;
      }
    }, libserial::IOException);
  }
}

TEST_F(PseudoTerminalTest, ReadBytesCanonicalMode) {
  libserial::Serial serial_port;

  serial_port.open(slave_port_);
  serial_port.setBaudRate(9600);
  serial_port.setCanonicalMode(libserial::CanonicalMode::ENABLE);

  EXPECT_THROW({
    try {
      std::string read_buffer;
      serial_port.readBytes(read_buffer, 5);
      ADD_FAILURE() << "Expected SerialException but no exception was thrown";
    }
    catch (const libserial::IOException& e) {
      EXPECT_STREQ(
        "readBytes() is not supported in canonical mode; use read() or readUntil() instead",
        e.what());
      throw;
    }
  }, libserial::IOException);
}

TEST_F(PseudoTerminalTest, ReadUntil) {
  libserial::Serial serial_port;

  serial_port.open(slave_port_);
  serial_port.setBaudRate(9600);

  const std::string test_message = "Read Until! Test!\n";

  ssize_t bytes_written = write(master_fd_, test_message.c_str(), test_message.length());
  ASSERT_GT(bytes_written, 0) << "Failed to write to master end";

  // Give time for data to propagate
  fsync(master_fd_);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Test reading with shared pointer - only read what's available
  std::string read_buffer;

  EXPECT_NO_THROW({serial_port.readUntil(read_buffer, '!'); });

  EXPECT_EQ(read_buffer, "Read Until!");
}

TEST_F(PseudoTerminalTest, ReadUntilTimeout) {
  libserial::Serial serial_port;

  serial_port.open(slave_port_);
  serial_port.setBaudRate(9600);

  const std::string test_message = "Read Until Test";

  ssize_t bytes_written = write(master_fd_, test_message.c_str(), test_message.length());
  ASSERT_GT(bytes_written, 0) << "Failed to write to master end";

  // Give time for data to propagate
  fsync(master_fd_);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  EXPECT_THROW({
    // Test reading with shared pointer - only read what's available
    std::string read_buffer;
    serial_port.readUntil(read_buffer, '!');
  }, libserial::IOException);
}

TEST_F(PseudoTerminalTest, ReadUntilWithReadFail) {
  libserial::Serial serial_port;

  for (const auto& [error_num, error_msg] : errors_read_) {
    if (error_num == EAGAIN || error_num == EWOULDBLOCK) {
      // Skip these as they are handled differently in readUntil
      continue;
    }
    serial_port.setPollSystemFunction(
      [](struct pollfd*, nfds_t, int) -> int {
      return 1;
    });
    serial_port.setReadSystemFunction(
      [error_num](int, void*, size_t) -> ssize_t {
      errno = error_num;
      return -1;
    });

    auto expected_what = "Error reading from serial port: " + error_msg;

    EXPECT_THROW({
      try {
        std::string read_buffer;
        serial_port.readUntil(read_buffer, '!');
      }
      catch (const libserial::IOException& e) {
        EXPECT_STREQ(expected_what.c_str(), e.what());
        throw;
      }
    }, libserial::IOException);
  }
}

TEST_F(PseudoTerminalTest, ReadUntilWithPollFail) {
  libserial::Serial serial_port;

  for (const auto& [error_num, error_msg] : errors_poll_) {
    serial_port.setPollSystemFunction(
      [error_num](struct pollfd*, nfds_t, int) -> int {
      errno = error_num;
      return -1;
    });

    auto expected_what = "Error in poll(): " + error_msg;

    EXPECT_THROW({
      try {
        std::string read_buffer;
        serial_port.readUntil(read_buffer, '!');
      }
      catch (const libserial::IOException& e) {
        EXPECT_STREQ(expected_what.c_str(), e.what());
        throw;
      }
    }, libserial::IOException);
  }
}

TEST_F(PseudoTerminalTest, ReadUntilWithOverflowBuffer) {
  libserial::Serial serial_port;

  serial_port.open(slave_port_);
  serial_port.setBaudRate(9600);
  EXPECT_NO_THROW(serial_port.setMaxSafeReadSize(10));  // Set max safe read size to 10 bytes

  std::string test_message(15, 'a');
  test_message.push_back('\n');

  ssize_t bytes_written = write(master_fd_, test_message.c_str(), test_message.length());
  ASSERT_GT(bytes_written, 0) << "Failed to write to master end";

  // Give time for data to propagate
  fsync(master_fd_);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  auto expected_what = "Read buffer exceeded maximum size limit of " +
                       std::to_string(serial_port.getMaxSafeReadSize()) +
                       " bytes without finding terminator";

  EXPECT_THROW({
    try {
      std::string read_buffer;
      serial_port.readUntil(read_buffer, '!');
    }
    catch (const libserial::IOException& e) {
      EXPECT_STREQ(expected_what.c_str(), e.what());
      throw;
    }
  }, libserial::IOException);
}

TEST_F(PseudoTerminalTest, ReadRawCanonicalMode) {
  libserial::Serial serial;

  serial.open(slave_port_);
  serial.setBaudRate(9600);

  // Enable canonical mode
  serial.setCanonicalMode(libserial::CanonicalMode::ENABLE);

  EXPECT_THROW({
    try {
      std::vector<uint8_t> buffer(10);
      serial.readRaw(buffer.data(), buffer.size());
    }
    catch (const libserial::IOException& e) {
      EXPECT_STREQ(
        "readRaw() is not supported in canonical mode; use read() or readUntil() instead",
        e.what());
      throw;
    }
  }, libserial::IOException);
}

TEST_F(PseudoTerminalTest, ReadRawFullRead) {
  libserial::Serial serial;
  serial.open(slave_port_);
  serial.setCanonicalMode(libserial::CanonicalMode::DISABLE);
  serial.setReadTimeout(std::chrono::milliseconds(500));

  const std::string msg = "HelloRaw";
  write(master_fd_, msg.data(), msg.size());

  std::vector<uint8_t> buffer(msg.size());

  ssize_t n = serial.readRaw(buffer.data(), buffer.size());

  EXPECT_EQ(n, msg.size());
  EXPECT_EQ(std::string(buffer.begin(), buffer.end()), msg);
}

TEST_F(PseudoTerminalTest, ReadRawPartialTimeout) {
  libserial::Serial serial;
  serial.open(slave_port_);
  serial.setCanonicalMode(libserial::CanonicalMode::DISABLE);
  serial.setReadTimeout(std::chrono::milliseconds(100));

  const std::string msg = "ABC";
  write(master_fd_, msg.data(), msg.size());

  std::vector<uint8_t> buffer(10);

  ssize_t n = serial.readRaw(buffer.data(), buffer.size());

  EXPECT_EQ(n, msg.size());
}

TEST_F(PseudoTerminalTest, ReadRawTimeoutNoData) {
  libserial::Serial serial;
  serial.open(slave_port_);
  serial.setCanonicalMode(libserial::CanonicalMode::DISABLE);
  serial.setReadTimeout(std::chrono::milliseconds(100));

  std::vector<uint8_t> buffer(10);

  ssize_t n = serial.readRaw(buffer.data(), buffer.size());

  EXPECT_EQ(n, 0);
}

TEST_F(PseudoTerminalTest, ReadRawPollTimeoutSimulated) {
  libserial::Serial serial;
  serial.setFdForTest(slave_fd_);
  serial.setCanonicalMode(libserial::CanonicalMode::DISABLE);
  serial.setPollSystemFunction(
    [](struct pollfd*, nfds_t, int) {
    return 0;    // timeout
  });

  std::vector<uint8_t> buffer(10);

  ssize_t n = serial.readRaw(buffer.data(), buffer.size());

  EXPECT_EQ(n, 0);
}

TEST_F(PseudoTerminalTest, ReadRawPollError) {
  libserial::Serial serial;
  serial.setFdForTest(slave_fd_);
  serial.setCanonicalMode(libserial::CanonicalMode::DISABLE);
  serial.setPollSystemFunction(
    [](struct pollfd*, nfds_t, int) {
    errno = EINVAL;
    return -1;
  });

  EXPECT_THROW({
    try {
      std::vector<uint8_t> buffer(10);
      serial.readRaw(buffer.data(), buffer.size());
    }
    catch (const libserial::IOException& e) {
      EXPECT_STREQ(
        std::string("Error in poll(): " + std::string(strerror(EINVAL))).c_str(),
        e.what());
      throw;
    }
  }, libserial::IOException);
}

TEST_F(PseudoTerminalTest, ReadRawReadError) {
  libserial::Serial serial;
  serial.setFdForTest(slave_fd_);
  serial.setCanonicalMode(libserial::CanonicalMode::DISABLE);
  serial.setPollSystemFunction(
    [](struct pollfd*, nfds_t, int) {
    return 1;
  });

  serial.setReadSystemFunction(
    [](int, void*, size_t) -> ssize_t {
    errno = EIO;
    return -1;
  });

  EXPECT_THROW({
    try {
      std::vector<uint8_t> buffer(10);
      serial.readRaw(buffer.data(), buffer.size());
    }
    catch (const libserial::IOException& e) {
      EXPECT_STREQ(
        std::string("Error reading raw data: " + std::string(strerror(EIO))).c_str(),
        e.what());
      throw;
    }
  }, libserial::IOException);
}

TEST_F(PseudoTerminalTest, ReadRawMultipleChunks) {
  libserial::Serial serial;
  serial.setFdForTest(slave_fd_);
  serial.setCanonicalMode(libserial::CanonicalMode::DISABLE);

  serial.setPollSystemFunction(
    [](struct pollfd*, nfds_t, int) {
    return 1;
  });

  int call = 0;

  serial.setReadSystemFunction(
    [&call](int, void* buf, size_t) -> ssize_t {
    uint8_t* b = static_cast<uint8_t*>(buf);

    if (call == 0) {
      b[0] = 'A';
      call++;
      return 1;
    }
    else {
      b[0] = 'B';
      return 1;
    }
  });

  std::vector<uint8_t> buffer(2);

  ssize_t n = serial.readRaw(buffer.data(), buffer.size());

  EXPECT_EQ(n, 2);
  EXPECT_EQ(buffer[0], 'A');
  EXPECT_EQ(buffer[1], 'B');
}

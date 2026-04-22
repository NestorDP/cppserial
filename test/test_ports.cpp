// Copyright 2020-2025 Nestor Neto

#include <gtest/gtest.h>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <unistd.h>
#include <memory>
#include <string>
#include <iostream>
#include <vector>

#include "libserial/ports.hpp"
#include "libserial/serial_exception.hpp"

// Simple unit tests that don't require actual hardware
class PortsTest : public ::testing::Test {
protected:
void SetUp() override {
  char temp_template[] = "/tmp/fake_serial_XXXXXX";
  ASSERT_NE(mkdtemp(temp_template), nullptr) << "Failed to create temp directory";
  temp_dir_ = temp_template;
}

void TearDown() override {
  // Clean up all symlinks in temp_dir_
  DIR* dir = opendir(temp_dir_.c_str());
  if (dir) {
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
      if (entry->d_name[0] != '.') {
        std::string path = temp_dir_ + "/" + entry->d_name;
        unlink(path.c_str());
      }
    }
    closedir(dir);
  }
  rmdir(temp_dir_.c_str());
}

std::string temp_dir_;
};

TEST_F(PortsTest, DefaultConstructor) {
  EXPECT_NO_THROW({
    libserial::Ports ports;
  });
}

TEST_F(PortsTest, ScanPortsThrowsWhenPathMissing) {
  // Use an unlikely path to exist to trigger the exception path
  const char* missing_path = "/this/path/should/not/exist/serial/by-id";
  libserial::Ports ports(missing_path);

  try {
    (void)ports.scanPorts();
    FAIL() << "Expected libserial::SerialException to be thrown";
  }
  catch (const libserial::SerialException& e) {
    // Optionally assert something about the message:
    EXPECT_NE(std::string(e.what()).find("Error while reading"), std::string::npos);
  }
  catch (...) {
    FAIL() << "Expected libserial::SerialException, but got a different exception";
  }
}

TEST_F(PortsTest, ScanPortsWithFakeDevices) {
  // Create fake device symlinks
  std::string fake_device1 = std::string(temp_dir_) + "/usb-FTDI_FT232R_USB_UART_A1B2C3D4";
  std::string fake_device2 = std::string(temp_dir_) + "/usb-Arduino_Uno_12345678";

  // Create symlinks pointing to fake /dev/ttyUSB* paths
  // The actual target doesn't need to exist for scanPorts to process it
  ASSERT_EQ(symlink("../../ttyUSB0", fake_device1.c_str()), 0);
  ASSERT_EQ(symlink("../../ttyUSB1", fake_device2.c_str()), 0);

  // Test scanPorts with the fake directory
  libserial::Ports ports(temp_dir_.c_str());
  uint16_t count = 0;
  EXPECT_NO_THROW({
    count = ports.scanPorts();
  });

  EXPECT_EQ(count, 2) << "Should find 2 fake devices";

  EXPECT_EQ(ports.findName(1).value(), "usb-FTDI_FT232R_USB_UART_A1B2C3D4");
  EXPECT_EQ(ports.findName(0).value(), "usb-Arduino_Uno_12345678");
  EXPECT_EQ(ports.findPortPath(1).value(), "/dev/ttyUSB0");
  EXPECT_EQ(ports.findPortPath(0).value(), "/dev/ttyUSB1");
  EXPECT_EQ(ports.findBusPath(1).value(), "/dev/ttyUSB0");
  EXPECT_EQ(ports.findBusPath(0).value(), "/dev/ttyUSB1");
}

TEST_F(PortsTest, GetDevicesPopulatesList) {
  // Create fake device symlinks
  std::string fake_device1 = std::string(temp_dir_) + "/usb-Device_One_0001";
  std::string fake_device2 = std::string(temp_dir_) + "/usb-Device_Two_0002";

  ASSERT_EQ(symlink("../../ttyUSB2", fake_device1.c_str()), 0);
  ASSERT_EQ(symlink("../../ttyUSB3", fake_device2.c_str()), 0);

  libserial::Ports ports(temp_dir_.c_str());
  EXPECT_NO_THROW({
    ports.scanPorts();
  });

  std::vector<libserial::Device> devices;
  EXPECT_NO_THROW({
    ports.getDevices(devices);
  });

  std::vector<std::string> names;
  for (const auto& device : devices) {
    names.push_back(device.getName());
  }

  EXPECT_EQ(devices.size(), 2);
  
  EXPECT_NE(std::find(names.begin(), names.end(), "usb-Device_One_0001"), names.end());
  EXPECT_NE(std::find(names.begin(), names.end(), "usb-Device_Two_0002"), names.end());
}



// @ Copyright 2025 Nestor Neto

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <string>

#include "libserial/serial.hpp"
#include "libserial/serial_exception.hpp"

void testBaudRate(libserial::Serial& serial, libserial::BaudRate baud) {
  try {
    serial.setBaudRate(baud);
    int actual = serial.getBaudRate();
    std::cout << "  Set: " << static_cast<int>(baud)
              << " -> Got: " << actual
              << (actual == static_cast<int>(baud) ? " ✓" : " ✗")
              << std::endl;
  } catch (const libserial::SerialException& e) {
    std::cout << "  Failed to set " << static_cast<int>(baud) << ": " << e.what() << std::endl;
  }
}

int main(int argc, const char* argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <serial_port>" << std::endl;
    std::cerr << "Example: " << argv[0] << " /tmp/ttyV0" << std::endl;
    return 1;
  }

  std::string port = argv[1];
  try {
    libserial::Serial serial;

    std::cout << "=== Baud Rate Testing Example ===" << std::endl;
    std::cout << "Opening serial port: " << port << std::endl;
    serial.open(port);

    std::cout << "\nTesting various baud rates:" << std::endl;

    // Test standard baud rates
    std::vector<libserial::BaudRate> baud_rates = {
      libserial::BaudRate::BAUD_RATE_1200,
      libserial::BaudRate::BAUD_RATE_2400,
      libserial::BaudRate::BAUD_RATE_4800,
      libserial::BaudRate::BAUD_RATE_9600,
      libserial::BaudRate::BAUD_RATE_19200,
      libserial::BaudRate::BAUD_RATE_38400,
      libserial::BaudRate::BAUD_RATE_57600,
      libserial::BaudRate::BAUD_RATE_115200,
      libserial::BaudRate::BAUD_RATE_230400
    };

    for (auto baud : baud_rates) {
      testBaudRate(serial, baud);
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Test communication at different speeds
    std::cout << "\n=== Communication Test ===" << std::endl;
    std::vector<libserial::BaudRate> test_speeds = {
      libserial::BaudRate::BAUD_RATE_9600,
      libserial::BaudRate::BAUD_RATE_115200
    };

    for (auto speed : test_speeds) {
      std::cout << "\nTesting communication at " << static_cast<int>(speed) << " baud:" << std::endl;
      serial.setBaudRate(speed);

      // Send test message
      std::string test_msg = "Speed test at " + std::to_string(static_cast<int>(speed));
      auto start = std::chrono::high_resolution_clock::now();
      serial.write(test_msg);
      auto end = std::chrono::high_resolution_clock::now();

      auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
      std::cout << "  Write time: " << duration.count() << " μs" << std::endl;
      std::cout << "  Message: '" << test_msg << "'" << std::endl;

      // Small delay between tests
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "\n=== Test Complete ===" << std::endl;
    std::cout << "Baud rate testing finished successfully!" << std::endl;
  } catch (const libserial::SerialException& e) {
    std::cerr << "Serial error: " << e.what() << std::endl;
    return 1;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}

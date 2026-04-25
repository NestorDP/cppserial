// @ Copyright 2025 Nestor Neto

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <string>

#include "libserial/serial.hpp"
#include "libserial/serial_exception.hpp"

int main(int argc, const char* argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <serial_port>" << std::endl;
    std::cerr << "Example: " << argv[0] << " /tmp/ttyV0" << std::endl;
    std::cerr << std::endl;
    std::cerr << "To create virtual ports with socat:" << std::endl;
    std::cerr << "socat -d -d pty,link=/tmp/ttyV0,raw,echo=0 pty,link=/tmp/ttyV1,raw,echo=0" << std::endl;
    return 1;
  }

  std::string port = argv[1];

  try {
    // Create and configure serial port
    libserial::Serial serial;

    std::cout << "Opening serial port: " << port << std::endl;
    serial.open(port);

    std::cout << "Setting baud rate to 115200..." << std::endl;
    serial.setBaudRate(libserial::BaudRate::BAUD_RATE_115200);

    // Verify configuration
    int current_baud = serial.getBaudRate();
    std::cout << "Current baud rate: " << current_baud << std::endl;

    // Send a message
    std::string message = "Hello from libserial!";
    std::cout << "Sending message: '" << message << "'" << std::endl;
    serial.write(message);

    // Wait a bit for potential response
    std::cout << "Waiting for response..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Check for available data
    int available = serial.getAvailableData();
    std::cout << "Available data: " << available << " bytes" << std::endl;

    if (available > 0) {
      std::string buffer;
      size_t bytes_read = serial.read(buffer);
      std::cout << "Received (" << bytes_read << " bytes): '" << buffer << "'" << std::endl;
    } else {
      std::cout << "No response received." << std::endl;
      std::cout << "You can send data to the other end of the virtual port." << std::endl;
    }

    // Interactive mode
    std::cout << std::endl;
    std::cout << "Interactive mode - type messages (Ctrl+C to exit):" << std::endl;
    std::string input;

    while (true) {
      std::cout << "> ";
      std::getline(std::cin, input);

      if (input.empty()) continue;

      // Send user input
      std::string user_message = input;
      serial.write(user_message);
      std::cout << "Sent: '" << input << "'" << std::endl;

      // Check for response
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      available = serial.getAvailableData();

      if (available > 0) {
        std::string buffer;
        size_t bytes_read = serial.read(buffer);
        std::cout << "Received (" << bytes_read << " bytes): '" << buffer << "'" << std::endl;
      }
    }
  } catch (const libserial::SerialException& e) {
    std::cerr << "Serial error: " << e.what() << std::endl;
    return 1;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}

// Copyright 2025 Nestor Neto

// Simple CLI for libserial: list serial ports
#include <iostream>
#include <string>

#include "libserial/ports.hpp"
#include "libserial/device.hpp"

void print_help(const char* prog) {
  std::cout << "Usage: " << prog << " [--list] [--help] [--version]\n";
  std::cout << "Options:\n";
  std::cout << "  --list     List available serial ports\n";
  std::cout << "  --version  Print program/library version\n";
  std::cout << "  --help     Show this help message\n";
}

int main(int argc, char** argv) {
  if (argc <= 1) {
    print_help(argv[0]);
    return 0;
  }

  std::string arg = argv[1];
  if (arg == "--help" || arg == "-h") {
    print_help(argv[0]);
    return 0;
  }

  if (arg == "--version") {
    std::cout << "libserial CLI\n";
    return 0;
  }

  if (arg == "--list") {
    try {
      libserial::Ports ports;
      uint16_t num = ports.scanPorts();
      std::cout << "Found " << (num + 1) << " entries (index 0.." << num << ")\n";
      for (uint16_t i = 0; i <= num; ++i) {
        auto name = ports.findName(i);
        auto port = ports.findPortPath(i);
        auto bus = ports.findBusPath(i);
        std::cout << "[" << i << "] "
                  << name.value_or("unknown") << " -> "
                  << port.value_or("unknown") << " (bus: "
                  << bus.value_or("unknown") << ")\n";
      }
      return 0;
    } catch (const std::exception& e) {
      std::cerr << "Error listing ports: " << e.what() << std::endl;
      return 2;
    }
  }

  print_help(argv[0]);
  return 1;
}

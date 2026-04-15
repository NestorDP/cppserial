// @ Copyright 2020

#include "libserial/serial.hpp"

#include <iostream>
#include <string>
#include <memory>
#include <poll.h>

namespace libserial {

Serial::Serial(const std::string& port) {
  this->open(port);
  this->setBaudRate(BaudRate::BAUD_RATE_9600);
}

Serial::~Serial() {
  if (fd_serial_port_ != -1) {
    ::close(fd_serial_port_);
    fd_serial_port_ = -1;
  }
}

void Serial::open(const std::string& port) {
  fd_serial_port_ = ::open(port.c_str(), O_RDWR | O_NOCTTY | O_NDELAY | O_NONBLOCK);

  if (fd_serial_port_ == -1) {
    throw SerialException("Error opening port " + port + ": " + strerror(errno));
  }
  else {
    fcntl(fd_serial_port_, F_SETFL, 0);
  }
}

void Serial::close() {
  if (fd_serial_port_ != -1) {
    ssize_t error = ::close(fd_serial_port_);
    if (error < 0) {
      throw SerialException("Error closing port: " + std::string(strerror(errno)));
    }
    fd_serial_port_ = -1;
  }
}

void Serial::write(std::shared_ptr<std::string> data) {
  if (!data) {
    throw IOException("Null pointer passed to write function");
  }

  ssize_t bytes_written = ::write(fd_serial_port_, data->c_str(), data->size());

  if (bytes_written < 0) {
    throw IOException("Error writing to serial port: " + std::string(strerror(errno)));
  }
}

ssize_t Serial::writeRaw(const uint8_t* data, size_t size) {
  if (!data || size == 0) {
    throw IOException("Invalid buffer passed to writeRaw");
  }

  size_t total_written = 0;

  while (total_written < size) {
    ssize_t ret = ::write(fd_serial_port_,
                          data + total_written,
                          size - total_written);

    if (ret < 0) {
      if (errno == EINTR) continue;
      throw IOException("Error writing raw data: " + std::string(strerror(errno)));
    }

    total_written += static_cast<size_t>(ret);
  }

  return static_cast<ssize_t>(total_written);
}

ssize_t Serial::writeRaw(const std::vector<uint8_t>& data) {
  if (data.empty()) {
    throw IOException("Data vector is empty");
  }
  return writeRaw(data.data(), data.size());
}

size_t Serial::read(std::shared_ptr<std::string> buffer) {
  if (canonical_mode_ == CanonicalMode::DISABLE) {
    throw IOException(
            "read() is not supported in non-canonical mode; use readBytes() or readUntil() instead");
  }

  if (!buffer) {
    throw IOException("Null pointer passed to read function");
  }

  buffer->clear();
  buffer->resize(max_safe_read_size_);

  struct pollfd fd_poll;
  fd_poll.fd = fd_serial_port_;
  fd_poll.events = POLLIN;

  // 0 => no wait (immediate return), -1 => block forever, positive => wait specified milliseconds
  int timeout_ms = static_cast<int>(read_timeout_ms_.count());
  int pr = poll_(&fd_poll, 1, timeout_ms);
  if (pr < 0) {
    throw IOException(std::string("Error in poll(): ") + strerror(errno));
  }
  if (pr == 0) {
    throw IOException("Read operation timed out after " + std::to_string(timeout_ms) +
                      " milliseconds");
  }

  // Data available: do the read
  ssize_t bytes_read = read_(fd_serial_port_, const_cast<char*>(buffer->data()),
                             max_safe_read_size_);
  if (bytes_read < 0) {
    throw IOException(std::string("Error reading from serial port: ") + strerror(errno));
  }
  buffer->resize(static_cast<size_t>(bytes_read));
  return static_cast<size_t>(bytes_read);
}

size_t Serial::readBytes(std::shared_ptr<std::string> buffer, size_t num_bytes) {
  if (canonical_mode_ == CanonicalMode::ENABLE) {
    throw IOException(
            "readBytes() is not supported in canonical mode; use read() or readUntil() instead");
  }

  if (!buffer) {
    throw IOException("Null pointer passed to readBytes function");
  }

  if (num_bytes == 0) {
    throw IOException("Number of bytes requested must be greater than zero");
  }

  buffer->clear();
  buffer->resize(num_bytes);

  ssize_t bytes_read = read_(fd_serial_port_, buffer->data(), num_bytes);  // codacy-ignore[buffer-boundary]

  if (bytes_read < 0) {
    throw IOException("Error reading from serial port: " + std::string(strerror(errno)));
  }

  buffer->resize(static_cast<size_t>(bytes_read));
  return static_cast<size_t>(bytes_read);
}

size_t Serial::readUntil(std::shared_ptr<std::string> buffer, char terminator) {
  if (!buffer) {
    throw IOException("Null pointer passed to readUntil function");
  }

  buffer->clear();
  char temp_char = '\0';

  auto start_time = std::chrono::steady_clock::now();

  while (temp_char != terminator) {
    // Check buffer size limit to prevent excessive memory usage
    if (buffer->size() >= max_safe_read_size_) {
      throw IOException("Read buffer exceeded maximum size limit of " +
                        std::to_string(max_safe_read_size_) +
                        " bytes without finding terminator");
    }
    // Check timeout if enabled (0 means no timeout)
    if (read_timeout_ms_.count() > 0) {
      auto current_time = std::chrono::steady_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time -
                                                                           start_time).count();

      if (elapsed >= static_cast<int64_t>(read_timeout_ms_.count())) {
        throw IOException("Read timeout exceeded while waiting for terminator");
      }

      // Use poll() to check if data is available with remaining timeout.
      // poll() does not have the FD_SETSIZE limitation that select() has
      // and is more robust for larger file descriptor values.
      struct pollfd pfd;
      pfd.fd = fd_serial_port_;
      pfd.events = POLLIN;

      int64_t remaining_timeout = read_timeout_ms_.count() - elapsed;
      int timeout_ms = static_cast<int>(remaining_timeout);

      int poll_result = poll_(&pfd, 1, timeout_ms);
      if (poll_result < 0) {
        throw IOException("Error in poll(): " + std::string(strerror(errno)));
      }
      else if (poll_result == 0) {
        throw IOException("Read timeout exceeded while waiting for data");
      }
    }

    // Data is available, perform the read
    ssize_t bytes_read = read_(fd_serial_port_, &temp_char, 1);

    if (bytes_read < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // Non-blocking read, no data available right now
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }
      throw IOException("Error reading from serial port: " + std::string(strerror(errno)));
    }
    else if (bytes_read == 0) {
      // End of file or connection closed
      throw IOException("Connection closed while reading: no terminator found");
    }

    // Add the character to buffer (including terminator)
    buffer->push_back(temp_char);
  }

  return buffer->size();
}

void Serial::flushInputBuffer() {
  if (ioctl_(fd_serial_port_, TCFLSH, TCIFLUSH) != 0) {
    throw SerialException("Error flushing input buffer: " + std::string(strerror(errno)));
  }
}

void Serial::setTermios2() {
  ssize_t error = ioctl_(fd_serial_port_, TCSETS2, &options_);
  if (error < 0) {
    throw SerialException("Error set Termios2: " + std::string(strerror(errno)));
  }
}

void Serial::setBaudRate(unsigned int baud_rate) {
  this->getTermios2();
  options_.c_cflag &= ~CBAUD;
  options_.c_cflag |= BOTHER;
  options_.c_ispeed = baud_rate;
  options_.c_ospeed = baud_rate;
  this->setTermios2();
}

void Serial::setBaudRate(BaudRate baud_rate) {
  this->setBaudRate(static_cast<unsigned int>(baud_rate));
}

void Serial::setReadTimeout(std::chrono::milliseconds timeout) {
  read_timeout_ms_ = timeout;
  this->setTimeOut(static_cast<uint16_t>(timeout.count() / 100));
}

void Serial::setWriteTimeout(std::chrono::milliseconds timeout) {
  write_timeout_ms_ = timeout;
}

void Serial::setDataLength(DataLength nbits) {
  this->getTermios2();
  options_.c_cflag &= ~CSIZE;
  switch (nbits) {
  case DataLength::FIVE:
    options_.c_cflag |= CS5;
    break;
  case DataLength::SIX:
    options_.c_cflag |= CS6;
    break;
  case DataLength::SEVEN:
    options_.c_cflag |= CS7;
    break;
  case DataLength::EIGHT:
    options_.c_cflag |= CS8;
    break;
  }
  this->setTermios2();
}

void Serial::setParity(Parity parity) {
  this->getTermios2();
  switch (parity) {
  case Parity::DISABLE:
    options_.c_cflag &= ~PARENB;
    break;
  case Parity::ENABLE:
    options_.c_cflag |= PARENB;
    break;
  }
  this->setTermios2();
}

void Serial::setStopBits(StopBits stop_bits) {
  this->getTermios2();
  switch (stop_bits) {
  case StopBits::ONE:
    options_.c_cflag &= ~CSTOP;
    break;
  case StopBits::TWO:
    options_.c_cflag |= CSTOP;
    break;
  }
  this->setTermios2();
}

void Serial::setFlowControl([[maybe_unused]] FlowControl flow_control) {
//   this->getTermios2();
//   switch (flow_control) {
//   case FlowControl::Software:
//     // options_.c_cflag &= ~CRTSCTS;
//     // options_.c_oflag |= (OPOST | ONLCR);
//     // options_.c_iflag |= (IXON | IXOFF );
//   options_.c_cflag &= ~PARENB; // Clear parity bit, disabling parity (most common)
//   options_.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used in communication (most common)
//   options_.c_cflag &= ~CSIZE; // Clear all bits that set the data size
//   options_.c_cflag |= CS8; // 8 bits per byte (most common)
//   options_.c_cflag &= ~CRTSCTS; // DISABLE RTS/CTS hardware flow control (most common)
//   options_.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)

//   options_.c_lflag &= ~ICANON;
//   options_.c_lflag &= ~ECHO; // DISABLE echo
//   options_.c_lflag &= ~ECHOE; // DISABLE erasure
//   options_.c_lflag &= ~ECHONL; // DISABLE new-line echo
//   options_.c_lflag &= ~ISIG; // DISABLE interpretation of INTR, QUIT and SUSP
//   options_.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
//   options_.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // DISABLE any special handling of received bytes

//   options_.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
//   options_.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed
//   // options_.c_oflag &= ~OXTABS; // Prevent conversion of tabs to spaces (NOT PRESENT ON LINUX)
//   // options_.c_oflag &= ~ONOEOT; // Prevent removal of C-d chars (0x004) in output (NOT PRESENT ON LINUX)

//   options_.c_cc[VTIME] = 10;    // Wait for up to 1s (10 deciseconds), returning as soon as any data is received.
//   options_.c_cc[VMIN] = 0;
//   // options_.c_cc[VEOF] = '\r';

//     break;
//   case FlowControl::Hardware:
//     options_.c_cflag |= CRTSCTS;
//     options_.c_iflag &= ~(IXON | IXOFF | IXANY);
//   default:
//     options_.c_cflag &= ~CRTSCTS;
//     break;
//   }
//   this->setTermios2();
}

void Serial::setCanonicalMode(CanonicalMode mode) {
  canonical_mode_ = mode;
  this->getTermios2();
  switch (canonical_mode_) {
  case CanonicalMode::ENABLE:
    options_.c_lflag |=  (ICANON);
    break;
  case CanonicalMode::DISABLE:
    options_.c_lflag &= ~(ICANON);
    break;
  }
  this->setTermios2();
}

void Serial::setTerminator(Terminator term) {
  terminator_ = term;
}

void Serial::setTimeOut(uint16_t time) {
  this->getTermios2();
  options_.c_cc[VTIME] = time;
  this->setTermios2();
}

void Serial::setMinNumberCharRead(uint16_t num) {
  min_number_char_read_ = num;
  this->getTermios2();
  options_.c_cc[VMIN] = min_number_char_read_;
  this->setTermios2();
}

void Serial::setMaxSafeReadSize(size_t size) {
  max_safe_read_size_ = size;
}

size_t Serial::getMaxSafeReadSize() const {
  return max_safe_read_size_;
}

int Serial::getAvailableData() const {
  int bytes_available;
  if (ioctl_(fd_serial_port_, FIONREAD, &bytes_available) < 0) {
    throw SerialException("Error getting available data: " + std::string(strerror(errno)));
  }
  return bytes_available;
}

int Serial::getBaudRate() const {
  this->getTermios2();
  return (static_cast<int>(options_.c_ispeed));
}

DataLength Serial::getDataLength() const {
  this->getTermios2();
  switch (options_.c_cflag & CSIZE) {
  case CS5: return DataLength::FIVE;
  case CS6: return DataLength::SIX;
  case CS7: return DataLength::SEVEN;
  case CS8: return DataLength::EIGHT;
  default: return DataLength::EIGHT;
  }
}

std::chrono::milliseconds Serial::getReadTimeout() const {
  this->getTermios2();
  return std::chrono::milliseconds(options_.c_cc[VTIME] * 100);
}

uint16_t Serial::getMinNumberCharRead() const {
  this->getTermios2();
  return static_cast<uint16_t>(options_.c_cc[VMIN]);
}

void Serial::getTermios2() const {
  ssize_t error = ioctl_(fd_serial_port_, TCGETS2, &options_);
  if (error < 0) {
    throw SerialException("Error get Termios2: " + std::string(strerror(errno)));
  }
}

}  // namespace libserial

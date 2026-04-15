//  @ Copyright 2022-2025 Nestor Neto

#ifndef INCLUDE_LIBSERIAL_SERIAL_HPP_
#define INCLUDE_LIBSERIAL_SERIAL_HPP_

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <asm/ioctls.h>
#include <asm/termbits.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "libserial/serial_exception.hpp"
#include "libserial/serial_types.hpp"

/**
 * @brief Serial Interface Library libserial namespace
 * @author Nestor Pereira Neto
 *
 */

namespace libserial {

/**
 * @brief A C++ wrapper class for serial port communication
 *
 * The Serial class provides a high-level interface for serial port operations
 * on Linux systems. It encapsulates low-level POSIX system calls and provides
 * exception-based error handling for serial communication.
 *
 * @author Nestor Pereira Neto
 * @version 0.0.0
 */
class Serial {
public:
/**
 * @brief Default constructor
 *
 * Creates a Serial object without opening any port.
 * Use open() method to establish connection.
 * Initializes with default baud rate of 9600.
 */
Serial() = default;
Serial(const Serial&) = delete;
Serial& operator=(const Serial&) = delete;

/**
 * @brief Constructor with automatic port opening
 *
 * Creates a Serial object and immediately opens the specified port.
 *
 * @param port The device path (e.g., "/dev/ttyUSB0", "/dev/ttyS0")
 * @throws SerialException if the port cannot be opened
 */
explicit Serial(const std::string& port);

/**
 * @brief Destructor
 *
 * Automatically closes the serial port if it's open, ensuring
 * proper resource cleanup.
 */
~Serial() noexcept;

/**
 * @brief Opens a serial port for communication
 *
 * @param port The device path to open (e.g., "/dev/ttyUSB0")
 * @throws SerialException if the port doesn't exist or cannot be accessed
 * @throws PermissionDeniedException if insufficient permissions
 */
void open(const std::string& port);

/**
 * @brief Closes the currently open serial port
 *
 * @throws SerialException if an error occurs during closing
 */
void close();

/**
 * @brief Writes data to the serial port
 *
 * Sends the provided string data to the serial port. A carriage return
 * character ('\\r') is automatically appended to the data.
 *
 * @param data Shared pointer to the string data to write
 * @throws SerialException if write operation fails
 * @throws std::invalid_argument if data pointer is null
 *
 * @note The original string is not modified; a copy is made with the
 *       terminator appended.
 */
void write(std::shared_ptr<std::string> data);

/**
 * @brief Writes raw byte data to the serial port
 *
 * Sends the provided byte data to the serial port without any modification.
 *
 * @param data Vector containing the byte data to write
 * @param size Number of bytes to write from the data vector
 * @return Number of bytes actually written
 * @throws SerialException if write operation fails
 * @throws std::invalid_argument if data vector is empty
 */
ssize_t writeRaw(const uint8_t* data, size_t size);

/**
 * @brief Writes raw byte data to the serial port
 *
 * Overloaded version that accepts a vector of bytes. This is a convenience
 * method that simply calls the pointer-based writeRaw after checking for
 * an empty vector.
 *
 * @param data Vector containing the byte data to write
 * @return Number of bytes actually written
 * @throws SerialException if write operation fails
 * @throws std::invalid_argument if data vector is empty
 */
ssize_t writeRaw(const std::vector<uint8_t>& data);

/**
 * @brief Reads data from serial port into a shared pointer buffer
 *
 * Reads up to max_length bytes from the serial port and stores them
 * in the provided shared string buffer. This version provides better
 * memory management and avoids unnecessary string copies. Just works
 * in canonical mode.
 *
 * @param buffer Shared pointer to string where data will be stored
 * @return Number of bytes actually read
 * @throws SerialException if read operation fails
 * @throws SerialException if buffer is null
 *
 * @note The buffer will be resized to contain exactly the read data
 */
size_t read(std::shared_ptr<std::string> buffer);

/**
 * @brief Reads a specific number of bytes from the serial port
 *
 * Reads exactly num_bytes from the serial port and stores them
 * in the provided shared string buffer. Just works in non-canonical mode.
 *
 * @param buffer Shared pointer to string where data will be stored
 * @param num_bytes Number of bytes to read
 * @return Number of bytes actually read
 * @throws SerialException if read operation fails
 * @throws SerialException if buffer is null
 * @throws SerialException if num_bytes is zero
 *
 * @note The buffer will be resized to contain exactly the read data
 */
size_t readBytes(std::shared_ptr<std::string> buffer, size_t num_bytes);

/**
 * @brief Reads data until a specific terminator character is found
 *
 * Continues reading byte by byte until the specified terminator
 * character is encountered. The terminator is included in the result.
 * Works in both canonical and non-canonical modes.
 *
 * @param terminator The character to stop reading at
 * @return String containing all read data including the terminator
 * @throws SerialException if read operation fails
 *
 * @warning This method reads one byte at a time and may be slower
 *          for large amounts of data
 */
size_t readUntil(std::shared_ptr<std::string> buffer, char terminator);

/**
 * @brief Flushes the input buffer
 *
 * Discards any data that has been received but not yet read.
 * Useful for clearing stale data before starting fresh communication.
 *
 * @throws SerialException if flush operation fails
 */
void flushInputBuffer();

/**
 * @brief Sets the baud rate for serial communication
 *
 * Configures the speed of serial communication. Both input and output
 * speeds are set to the same value.
 *
 * @param baud_rate The desired baud rate (e.g., 9600, 115200)
 * @throws SerialException if baud rate cannot be set
 *
 * @note The port must be opened before calling this method
 */
void setBaudRate(unsigned int baud_rate);

/**
 * @brief Gets the number of bytes available for reading
 *
 * Returns the number of bytes currently available in the input buffer
 * without actually reading them. Useful for non-blocking operations.
 *
 * @return Number of bytes available for reading
 * @throws SerialException if operation fails
 */
int getAvailableData() const;

/**
 * @brief Sets the read timeout in milliseconds
 *
 * Configures the maximum time to wait for read operations before
 * timing out. A value of 0 means no timeout (blocking).
 *
 * @param timeout Timeout in milliseconds
 *
 * @note The system timeout is set in deciseconds (100ms units), so the value
 *       will be rounded down to the nearest multiple of 100ms. For example,
 *       1549ms will be set as 1500ms.
 */
void setReadTimeout(std::chrono::milliseconds timeout);

/**
 * @brief Sets the write timeout in milliseconds
 *
 * Configures the maximum time to wait for write operations before
 * timing out. A value of 0 means no timeout (blocking).
 *
 * @param timeout Timeout in milliseconds
 * @throws SerialException if setting cannot be applied
 */
void setWriteTimeout(std::chrono::milliseconds timeout);

/**
 * @brief Sets the number of data bits per byte
 *
 * Configures the number of data bits used in each byte of serial
 * communication.
 *
 * @param nbits The desired number of data bits (5, 6, 7, or 8)
 * @throws SerialException if number of bits cannot be set
 */
void setDataLength(DataLength nbits);

/**
 * @brief Sets the parity configuration
 *
 * Configures the parity checking mechanism for serial communication.
 *
 * @param parity The desired parity setting (ENABLE or DISABLE)
 * @throws SerialException if parity cannot be set
 */
void setParity(Parity parity);

/**
 * @brief Sets the stop bits configuration
 *
 * Configures the number of stop bits used in serial communication.
 *
 * @param stop_bits The desired stop bits setting (ONE, ONE_AND_HALF, or TWO)
 * @throws SerialException if stop bits cannot be set
 */
void setStopBits(StopBits stop_bits);

/**
 * @brief Sets the flow control configuration
 *
 * Configures the flow control mechanism for serial communication.
 *
 * @param flow_control The desired flow control setting (HARDWARE or SOFTWARE)
 * @throws SerialException if flow control cannot be set
 */
void setFlowControl([[maybe_unused]] FlowControl flow_control);

/**
 * @brief Sets canonical mode for input processing
 *
 * Configures whether the input is processed in canonical (line-based)
 * mode or non-canonical (raw) mode.
 *
 * @param canonical_mode The desired canonical mode setting (ENABLE or DISABLE)
 * @throws SerialException if canonical mode cannot be set
 */
void setCanonicalMode(CanonicalMode mode);

/**
 * @brief Sets the terminator character for readUntil operations
 *
 * Configures the character that signals the end of a readUntil operation.
 *
 * @param term The desired terminator character
 * @throws SerialException if terminator cannot be set
 */
void setTerminator(Terminator term);

/**
 * @brief Sets the read timeout in deciseconds
 *
 * @param time Timeout in deciseconds
 * @throws SerialException if setting cannot be applied
 */
void setTimeOut(uint16_t time);

/**
 * @brief Sets the minimum number of characters to read
 *
 * @param num Minimum number of characters to read
 * @throws SerialException if setting cannot be applied
 */
void setMinNumberCharRead(uint16_t);

/**
 * @brief Set the baud rate using BaudRate enum
 *
 * This overloaded version accepts a BaudRate enum value, providing
 * type safety and preventing invalid baud rate values.
 *
 * @param baud_rate The baud rate from BaudRate enum
 * @throws SerialException if baud rate cannot be set
 *
 * @note The port must be opened before calling this method
 */
void setBaudRate(BaudRate baud_rate);

/**
 * @brief Sets the maximum safe read size
 *
 * Configures the maximum number of bytes that can be read
 * in a single read operation to prevent excessive memory usage.
 *
 * @param size The desired maximum safe read size in bytes
 */
void setMaxSafeReadSize(size_t size);

/**
 * @brief Gets the maximum safe read size
 *
 * Retrieves the maximum number of bytes that can be read
 * in a single read operation to prevent excessive memory usage.
 *
 * @return The maximum safe read size in bytes
 */
size_t getMaxSafeReadSize() const;

/**
 * @brief Gets the current baud rate
 *
 * @return The current baud rate
 * @throws SerialException if unable to retrieve baud rate
 */
int getBaudRate() const;

/**
 * @brief Gets the current data length setting
 *
 * Retrieves the number of data bits configured for serial communication.
 *
 * @return The current data length (number of data bits)
 */
DataLength getDataLength() const;

/**
 * @brief Gets the current read timeout setting
 *
 * @return The current read timeout in milliseconds
 */
std::chrono::milliseconds getReadTimeout() const;

/**
 * @brief Gets the current minimum number of characters to read setting
 *
 * @return The current minimum number of characters to read
 */
uint16_t getMinNumberCharRead() const;

#ifdef BUILD_TESTING_ON
// WARNING: Test helper only! This function bypasses normal initialization
// and may leave the Serial object in an inconsistent state. It is intended
// strictly for unit tests to inject a file descriptor and should NEVER be
// used in production code.
void setFdForTest(int fd) {
  fd_serial_port_ = fd;
}
// WARNING: Test helper only! This function allows injection of custom
// system call functions for testing error handling. It should NEVER be
// used in production code.
void setPollSystemFunction(
  std::function<int(struct pollfd*, nfds_t, int)> poll_func) {
  poll_ = [poll_func](struct pollfd* f, nfds_t n, int t) {
            return poll_func(f, n, t);
          };
}

void setReadSystemFunction(
  std::function<ssize_t(int, void*, size_t)> read_func) {
  read_ = [read_func](int fd, void* buf, size_t sz) {
            return read_func(fd, buf, sz);
          };
}

/* *INDENT-OFF* */
void setIoctlSystemFunction(
  std::function<int(int, unsigned long, void*)> ioctl_func) {  // NOLINT
  ioctl_ = [ioctl_func](int fd, unsigned long request, void* arg) { // NOLINT
             return ioctl_func(fd, request, arg);
           };
}
/* *INDENT-ON* */
#endif

private:
/**
 * @brief Ioctl system call function wrapper
 *
 * Allows injection of custom ioctl function for testing.
 */
/* *INDENT-OFF* */
std::function<int(int, unsigned long, void*)> ioctl_ = // NOLINT
  [](int fd, unsigned long request, void* arg) {  // NOLINT
    return ::ioctl(fd, request, arg);
  };
/* *INDENT-ON* */

/**
 * @brief Poll system call function wrapper
 *
 * Allows injection of custom poll function for testing.
 */
std::function<int(struct pollfd*, nfds_t, int)> poll_ =
  [](struct pollfd* f, nfds_t n, int t) {
    return ::poll(f, n, t);
  };

/**
 * @brief Read system call function wrapper
 *
 * Allows injection of custom read function for testing.
 */
std::function<ssize_t(int, void*, size_t)> read_ =
  [](int fd, void* buf, size_t sz) {
    return ::read(fd, buf, sz);
  };

/**
 * @brief Applies terminal settings to the port
 *
 * Internal method to write the termios2 configuration
 * to the serial port file descriptor.
 *
 * @throws SerialException if ioctl operation fails
 */
void setTermios2();

/**
 * @brief Retrieves current terminal settings
 *
 * Internal method to read the current termios2 configuration
 * from the serial port file descriptor.
 *
 * @throws SerialException if ioctl operation fails
 */
void getTermios2() const;

/**
 * @brief Terminal configuration structure
 *
 * Holds the current serial port configuration including
 * baud rate, data bits, parity, stop bits, and other settings.
 */
mutable struct termios2 options_;

/**
 * @brief File descriptor for the serial port
 *
 * The low-level file descriptor used for all read/write operations
 * on the serial port. Set to -1 when no port is open.
 */
int fd_serial_port_{-1};

/**
 * @brief Read timeout in milliseconds
 *
 * Specifies the maximum time to wait for read operations
 * before timing out. Default is 1000ms.
 */
std::chrono::milliseconds read_timeout_ms_{1000};    ///< Read timeout in milliseconds (default 1000ms)

/**
 * @brief Write timeout in milliseconds
 *
 * Specifies the maximum time to wait for write operations
 * before timing out. Default is 1000ms.
 */
std::chrono::milliseconds write_timeout_ms_{1000};    ///< Write timeout in milliseconds (default 1000ms)

/**
 * @brief Maximum safe read size
 *
 * Defines the maximum number of bytes that can be read
 * in a single read operation to prevent excessive memory usage.
 * Default is 2048 bytes (2KB).
 */
size_t max_safe_read_size_{2048};  // 2KB limit

/**
 * @brief Minimum number of characters to read
 *
 * Specifies the minimum number of characters required for a read operation to return (default 0).
 */
uint16_t min_number_char_read_{0};

/**
 * @brief Canonical mode setting
 *
 * Determines whether canonical (line-based) input processing is enabled (default ENABLE).
 */
CanonicalMode canonical_mode_{CanonicalMode::ENABLE};

/**
 * @brief Line terminator character
 *
 * Specifies the character used to terminate lines (default LF).
 */
Terminator terminator_{Terminator::LF};
};

}  // namespace libserial

#endif  // INCLUDE_LIBSERIAL_SERIAL_HPP_

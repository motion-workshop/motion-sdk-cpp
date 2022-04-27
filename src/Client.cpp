// Copyright Motion Workshop. All Rights Reserved.
#include <Client.hpp>

#include <algorithm>

#if defined(_WIN32)
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN 1
#endif // WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#endif // _WIN32

// Require that these contants match up with the BSD standard, then we do not
// need to use them directly in the code.
#if defined(_WIN32)
static_assert(INVALID_SOCKET == -1, "INVALID_SOCKET must be -1");
static_assert(SOCKET_ERROR == -1, "SOCKET_ERROR must be -1");
static_assert(SD_BOTH == 2, "SD_BOTH must be 2");
#endif // _WIN32

// Create a macro to access error code for system socket calls.
#if defined(_WIN32)
#define ERROR_CODE WSAGetLastError()
#else
#define ERROR_CODE errno
#endif // _WIN32

// Create some error handler macros. Allow use to choose whether we want
// exceptions at library compile time.
#define CLIENT_ERROR(msg) \
  { \
    m_error_string.assign(msg); \
  }

namespace Motion { namespace SDK {

namespace {

/**
  The maximum length of a single message.

  This is used as an extra level of safety. You could set the maximum message
  size to a very large value. The real hard limit on the size of the message is
  the maximum length of an std::string.

  In practice, we assume that something is wrong if we receive a huge message.
*/
const std::size_t MaximumMessageLength = 65535;

/**
  The maximum size (in bytes) of a single send/receive to the underlying
  socket library. A Client object will allocate this buffer when instantiated.
*/
const std::size_t ReceiveBufferSize = 1024;

/**
  Set the address to this value if we get an empty string.
*/
const std::string DefaultAddress = "127.0.0.1";

/**
  Default value, in seconds, for the socket receive time out in the
  Client#waitForData method. Zero denotes blocking receive.
*/
const std::size_t TimeOutWaitForData = 5;

/**
  Default value, in seconds, for the socket receive time out in the
  Client#readData method.
*/
const std::size_t TimeOutReadData = 1;

/**
  Default value, in seconds, for the socket send time out in the
  Client#writeData method.
*/
const std::size_t TimeOutWriteData = 1;

/** Detect in stream XML messages with the following header bytes. */
const std::string XMLMagic = "<?xml";

// On Linux, disable the SIGPIPE signal for send and recv system calls. On
// macOS send a 0 flag and request no signal as a socket option (SO_NOSIGPIPE).
#if defined(MSG_NOSIGNAL)
const int SocketFlags = MSG_NOSIGNAL;
#else
const int SocketFlags = 0;
#endif // MSG_NOSIGNAL

#if defined(_WIN32)

typedef int timeval_type;

int make_timeval(std::size_t second)
{
  // Windows specifies this as an integer valued milliseconds.
  return static_cast<int>(second) * 1000;
}

#else

typedef timeval timeval_type;

timeval make_timeval(std::size_t second)
{
  // Linux specifies this as a timeval.
  timeval optionval;
  optionval.tv_sec = second;
  optionval.tv_usec = 0;

  return optionval;
}

int closesocket(int fd)
{
  return ::close(fd);
}

#endif // _WIN32

bool is_xml(const Client::data_type &data)
{
  if (data.size() < XMLMagic.size()) {
    return false;
  }

  return std::equal(XMLMagic.begin(), XMLMagic.end(), data.begin());
}

} // anonymous namespace

Client::Client()
  : m_socket(-1), m_host(), m_port(0), m_description(), m_xml_string(),
    m_intercept_xml(true), m_error_string(), m_initialize(false),
    m_buffer(ReceiveBufferSize), m_next_message(), m_time_out_second(0),
    m_time_out_second_send(0)
{
#if defined(_WIN32)
  if (!m_initialize) {
    // Winsock API requires per application or DLL initialization. We are
    // allowed to make multiple calls to WSAStartup as long as we make a
    // cooresponding call to WSACleanup. Choose to do this in the constructor
    // and destructor, to make it scoped initialization.
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0) {
      CLIENT_ERROR("failed to initialize Winsock at \"WSAStartup\"");
      return;
    }

    m_initialize = true;
  }
#endif // _WIN32

  socketInitialize();
}

Client::Client(const std::string &host, unsigned port) : Client{}
{
  {
    // Create the address structure to describe the remote host and
    // port we would like to connect to.
    sockaddr_in address;
    {
      address.sin_family = AF_INET;
      address.sin_port = htons(static_cast<unsigned short>(port));
      std::fill(std::begin(address.sin_zero), std::end(address.sin_zero), 0);

      // Implement a default value for the remote host name.
      const std::string &ip_address = !host.empty() ? host : DefaultAddress;

      const int result =
        inet_pton(AF_INET, ip_address.c_str(), &address.sin_addr);
      if (result <= 0) {
        // -1 indicates that the AF_INET family is not supported or inputs are
        // malformed.
        // 0 value indicates the address was not parseable as an AF_INET
        // address in "ddd.ddd.ddd.ddd" format.
        CLIENT_ERROR("failed conversion from host string to address structure");

        closesocket(m_socket);

        m_socket = -1;
        return;
      }
    }

    // Connect to the remote host on requested port.
    const int result = connect(
      m_socket, reinterpret_cast<sockaddr *>(&address), sizeof(sockaddr));

    // Check for connection failure. We may want to handle "connection refused"
    // on a different path than general function call failures.
    if (result == -1) {
      const int ec = ERROR_CODE;
#if _WIN32
      if (ec == WSAECONNREFUSED) {
#else
      if (ec == ECONNREFUSED) {
#endif // _WIN32
       // Connection refused.
       // No connection could be made because the target computer actively
       // refused it. This usually results from trying to connect to a service
       // that is inactive on the foreign host, one with no server application
       // running.
        CLIENT_ERROR("connection refused by remote host");
      } else {
        CLIENT_ERROR("failed to connect to remote host");
      }

      closesocket(m_socket);

      m_socket = -1;
      return;
    }
  }

  m_host = host;
  m_port = port;

  // Set send and receive buffer sizes to something larger than the default.
  {
    const int optionval = 65536;

    setsockopt(
      m_socket, SOL_SOCKET, SO_SNDBUF,
      reinterpret_cast<const char *>(&optionval), sizeof(optionval));

    setsockopt(
      m_socket, SOL_SOCKET, SO_RCVBUF,
      reinterpret_cast<const char *>(&optionval), sizeof(optionval));
  }

  // Read the first message from the service. It is a string description of the
  // remote service.
  setReceiveTimeout(TimeOutWaitForData);

  data_type data;
  if (readMessage(data)) {
    m_description.assign(data.begin(), data.end());
  }
}

Client::~Client()
{
  // Close an open socket. Do not call virtual methods in the destructor.
  if (m_socket > 0) {
    socketClose();
  }

#if defined(_WIN32)
  if (m_initialize) {
    if (WSACleanup() == 0) {
      m_initialize = false;
    }
  }
#endif // _WIN32
}

void Client::close()
{
  if (!isConnected()) {
    CLIENT_ERROR("failed to close client, not connected");
    return;
  }

  socketClose();
}

bool Client::isConnected() const
{
  return m_socket > 0;
}

bool Client::waitForData(int time_out_second)
{
  if (!isConnected()) {
    CLIENT_ERROR("failed to wait for incoming data, client is not connected");
    return false;
  }

  // A negative value of time_out_second (-1) indicates that we just want to use
  // the default value.
  if (time_out_second < 0) {
    time_out_second = TimeOutWaitForData;
  }
  if (time_out_second != static_cast<int>(m_time_out_second)) {
    setReceiveTimeout(time_out_second);
  }

  data_type data;
  if (!readMessage(data)) {
    return false;
  }

  // Intercept any incoming XML message. Store the most recent one in our
  // internal state. Match our magic prefix.
  if (is_xml(data)) {
    m_xml_string.assign(data.begin(), data.end());
  }

  return true;
}

bool Client::readData(data_type &data, int time_out_second)
{
  data.clear();

  if (!isConnected()) {
    CLIENT_ERROR("failed to read data, client is not connected");
    return false;
  }

  // A negative value of time_out_second (-1) indicates that we just want to use
  // the default value.
  if (time_out_second < 0) {
    time_out_second = TimeOutReadData;
  }
  if (time_out_second != static_cast<int>(m_time_out_second)) {
    setReceiveTimeout(time_out_second);
  }

  if (!readMessage(data)) {
    return false;
  }

  // Intercept any incoming XML message. Store the most recent one in our
  // internal state.
  if (is_xml(data)) {
    m_xml_string.assign(data.begin(), data.end());

    // We will not receive multiple consecutive XML messages in stream.
    if (!readMessage(data)) {
      return false;
    }
  }

  return true;
}

bool Client::writeData(const data_type &data, int time_out_second)
{
  if (!isConnected()) {
    CLIENT_ERROR("failed to read data, client is not connected");
    return false;
  }

  // A negative value of time_out_second (-1) indicates that we just want to use
  // the default value.
  if (time_out_second < 0) {
    time_out_second = TimeOutWriteData;
  }
  if (time_out_second != static_cast<int>(m_time_out_second_send)) {
    setSendTimeout(time_out_second);
  }

  return writeMessage(data);
}

bool Client::getXMLString(std::string &xml_string) const
{
  // Note that this does not enforce the connection state. This may return true
  // even if we are not connected.
  xml_string.assign(m_xml_string);

  return !xml_string.empty();
}

bool Client::getErrorString(std::string &message) const
{
  // Note that this does not enforce the connection state. This may return true
  // even if we are not connected.
  message.assign(m_error_string);

  return !message.empty();
}

bool Client::readMessage(data_type &data)
{
  data.clear();

  data_type buffer;
  bool timed_out = false;
  std::size_t num_bytes = sizeof(unsigned);

  // We may have saved bytes from the receive buffer of the previous message.
  if (!m_next_message.empty()) {
    buffer = std::move(m_next_message);
  }

  while (buffer.size() < num_bytes) {
    const int rc = socketReceive(&m_buffer[0], m_buffer.size(), timed_out);
    if (rc <= 0) {
      // This can indicate a graceful disconnection of the TCP socket stream.
      if (!timed_out) {
        close();
      }

      break;
    }

    buffer.insert(buffer.end(), m_buffer.begin(), m_buffer.begin() + rc);
  }

  // Need at least 4 bytes for the length header.
  if (buffer.size() < num_bytes) {
    CLIENT_ERROR(
      "communication protocol error, failed to read full message header");
    close();
    return false;
  }

  {
    // Copy the network ordered message length header. Make sure it is a
    // "reasonable" value.
    const unsigned length =
      ntohl(*reinterpret_cast<const unsigned *>(&buffer[0]));
    if ((length == 0) || (length > MaximumMessageLength)) {
      CLIENT_ERROR("communication protocol error, message header specifies "
                   "invalid length");
      close();
      return false;
    }

    num_bytes += static_cast<std::size_t>(length);
  }

  while (buffer.size() < num_bytes) {
    const int rc = socketReceive(&m_buffer[0], m_buffer.size(), timed_out);
    if (rc <= 0) {
      // This can indicate a graceful disconnection of the TCP socket stream.
      if (!timed_out) {
        close();
      }

      break;
    }

    buffer.insert(buffer.end(), m_buffer.begin(), m_buffer.begin() + rc);
  }

  // Need at least enough bytes for a complete message header and payload.
  if (buffer.size() < num_bytes) {
    CLIENT_ERROR(
      "communication protocol error, failed to read full message payload");
    close();
    return false;
  }

  // Copy payload into the output buffer.
  auto last = buffer.begin() + num_bytes;
  data.assign(buffer.begin() + sizeof(unsigned), last);

  // Save any bytes received that are part of the next logical message.
  if (buffer.size() > num_bytes) {
    m_next_message.assign(last, buffer.end());
  }

  return true;
}

bool Client::writeMessage(const data_type &data)
{
  // Precondition, non-empty message
  if (data.empty()) {
    CLIENT_ERROR("communication protocol error, message is empty");
    close();
    return false;
  }

  // Precondition, message not too long
  if (data.size() > MaximumMessageLength) {
    CLIENT_ERROR("communication protocol error, message too long to send");
    close();
    return false;
  }

  // Form a complete SDK message.
  // [uint32-be = N] [N bytes ...]
  const std::size_t message_size = sizeof(unsigned) + data.size();
  if (m_buffer.size() < message_size) {
    m_buffer.resize(message_size);
  }

  {
    // Create a message length header. 4 bytes of integer message length at
    // the beginning of the message, in network order.
    const unsigned length = htonl(static_cast<unsigned>(data.size()));

    // Copy the length header into the send buffer.
    auto itr = std::copy_n(
      reinterpret_cast<const char *>(&length), sizeof(unsigned),
      m_buffer.begin());

    // Copy byte payload into the send buffer.
    std::copy(data.begin(), data.end(), itr);
  }

  bool timed_out = false;
  std::size_t num_bytes = 0;
  while (num_bytes < message_size) {
    const int rc =
      socketSend(&m_buffer[num_bytes], message_size - num_bytes, timed_out);
    if (rc <= 0) {
      break;
    }

    num_bytes += static_cast<std::size_t>(rc);
  }

  if (num_bytes != message_size) {
    CLIENT_ERROR(
      "communication protocol error, failed to write complete message");
    close();
    return false;
  }

  return true;
}

int Client::socketSend(const char *buf, std::size_t len, bool &timed_out)
{
  const int rc = send(m_socket, buf, static_cast<int>(len), SocketFlags);
  if (rc >= 0) {
    return rc;
  }

  const int ec = ERROR_CODE;
#if defined(_WIN32)
  if (ec == WSAETIMEDOUT) {
#else
  if ((ec == ETIMEDOUT) || (ec == EAGAIN)) {
#endif
    // Connection timed out.
    // A connection attempt failed because the connected party did not
    // properly respond after a period of time, or the established connection
    // failed because the connected host has failed to respond.

    // We set a time out for some receive calls. Do not throw an exception if
    // we expect to time out.
    timed_out = true;
#if defined(_WIN32)
  } else if (ec == WSAEINTR) {
    // Interrupted function call. A blocking operation was interrupted by a call
    // to WSACancelBlockingCall.

    // An external thread may have called the #close method for this instance.
    // This may be unsafe, but consider it to be a "graceful close" condition.
#endif
  } else {
    CLIENT_ERROR("failed to write data to socket");
    return -1;
  }

  return 0;
}

int Client::socketReceive(char *buf, std::size_t len, bool &timed_out)
{
  const int rc = recv(m_socket, buf, static_cast<int>(len), SocketFlags);
  if (rc >= 0) {
    return rc;
  }

  const int ec = ERROR_CODE;
#if defined(_WIN32)
  if (ec == WSAETIMEDOUT) {
#else
  if ((ec == ETIMEDOUT) || (ec == EAGAIN)) {
#endif
    // Connection timed out.
    // A connection attempt failed because the connected party did not
    // properly respond after a period of time, or the established connection
    // failed because the connected host has failed to respond.

    // We set a time out for some receive calls. Do not throw an exception if
    // we expect to time out.
    timed_out = true;
#if defined(_WIN32)
  } else if (ec == WSAEINTR) {
    // Interrupted function call. A blocking operation was interrupted by a call
    // to WSACancelBlockingCall.

    // An external thread may have called the #close method for this instance.
    // This may be unsafe, but consider it to be a "graceful close" condition.
#endif
  } else {
    CLIENT_ERROR("failed to read data from socket");
    return -1;
  }

  return 0;
}

bool Client::setReceiveTimeout(std::size_t second)
{
  const auto optionval = make_timeval(second);

  const int result = setsockopt(
    m_socket, SOL_SOCKET, SO_RCVTIMEO,
    reinterpret_cast<const char *>(&optionval), sizeof(optionval));

  if (result == -1) {
    CLIENT_ERROR("failed to set client receive time out");
    return false;
  }

  m_time_out_second = second;
  return true;
}

bool Client::setSendTimeout(std::size_t second)
{
  const auto optionval = make_timeval(second);

  const int result = setsockopt(
    m_socket, SOL_SOCKET, SO_SNDTIMEO,
    reinterpret_cast<const char *>(&optionval), sizeof(optionval));

  if (result == -1) {
    CLIENT_ERROR("failed to set client send time out");
    return false;
  }

  m_time_out_second_send = second;
  return true;
}

void Client::socketInitialize()
{
  // Request a socket for a good old fashioned TCP connection.
  m_socket = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
  if (m_socket == -1) {
    CLIENT_ERROR("failed to create socket");
    return;
  }

#if defined(SO_NOSIGPIPE)
  // macOS does not have the MSG_NOSIGNAL flag. However, it does have this
  // connections based version.
  const int optionval = 1;

  const int result = setsockopt(
    m_socket, SOL_SOCKET, SO_NOSIGPIPE,
    reinterpret_cast<const char *>(&optionval), sizeof(optionval));

  if (result == -1) {
    CLIENT_ERROR("failed to set socket signal option");
  }
#endif // SO_NOSIGPIPE
}

void Client::socketClose()
{
  const int fd = m_socket;

  // Re-initialize local state. Do not clear out the error message.
  m_socket = -1;
  m_host.clear();
  m_port = 0;
  m_description.clear();
  m_xml_string.clear();

  std::fill(m_buffer.begin(), m_buffer.end(), 0);
  m_next_message.clear();

  // First, disable sends and receives on this socket. Notify the other side of
  // the connection that we are going away.
  if (shutdown(fd, 2) == -1) {
    CLIENT_ERROR("failed to shutdown socket communication");
  }

  if (closesocket(fd) == -1) {
    CLIENT_ERROR("failed to close socket handle");
  }
}

}} // namespace Motion::SDK

// Copyright Motion Workshop. All Rights Reserved.
#pragma once

#include <string>
#include <vector>

namespace Motion { namespace SDK {

/**
  Implements a simple socket based data client and the Motion Service binary
  message protocol. Provide a simple interface to develop external applications
  that can read real-time Motion data.

  This class only handles the socket connection and single message transfer. The
  @ref Format class implements interfaces to the service specific data formats.

  @code
  using Motion::SDK::Client;

  // Open connection to a Motion Service on the localhost, port 32079 is the
  // Preview data stream.
  Client client("", 32079);

  // This is application dependent. Use an outer loop to keep trying to read
  // data even after time outs.
  while (client.isConnected()) {
      // Wait until there is incoming data on the open connection, timing out
      //after 5 seconds.
      if (client.waitForData()) {
          // Read data samples in a loop. This will block by default for 1
          // second. Simply wait on an open connection until a data sample comes
          // in, or fall back to the outer wait loop.
          Client::data_type data;
          while (client.readData(data)) {
              // Do something useful with the current real-time sample
          }
      }
  }
  @endcode
*/
class Client {
public:
    /**
      Define the type of a single binary message. Buffer of bytes.
    */
    typedef std::vector<char> data_type;

    /**
      Create a client connection to a remote Motion Service.

      @param   host IP address of remote host, the empty string defaults to
               "127.0.0.1" aka localhost
      @param   port port name of remote host
      @pre     a service is accepting connections on the socket described by the
               host, port pair
      @post    this client is connected to the remote service described
               by the host, port pair
    */
    Client(const std::string& host, unsigned port);

    /**
      Close the client connection if it is open.
    */
    virtual ~Client();

    /**
      Disable the copy and move constructors.

      This is a resource object. Copy constructor semantics are unclear.
    */
    Client(const Client& rhs) = delete;
    Client(Client&& rhs) = delete;

    /**
      Disable the assignment operator.

      @see Client#Client(const Client &)
    */
    Client& operator=(const Client& lhs) = delete;
    Client& operator=(Client&& lhs) = delete;

    /**
      Close the current client connection.
    */
    virtual void close();

    /**
      Returns true iff the current socket connection is active.
    */
    virtual bool isConnected() const;

    /**
      Wait until there is incoming data on this client connection and then
      returns true.

      @param   time_out_second time out and return false after
               this many seconds, 0 value specifies no time out,
               negative value specifies default time out
      @pre     this object has an open socket connection
    */
    virtual bool waitForData(int time_out_second = -1);

    /**
      Read a variable length binary message into the output vector. The output
      vector will be resized as necessary.

      @param   time_out_second time out and return false after
               this many seconds, 0 value specifies no time out,
               negative value specifies default time out
      @pre     this object has an open socket connection
    */
    virtual bool readData(data_type& data, int time_out_second = -1);

    /**
      Write a variable length binary message to the socket link.

      @param   time_out_second time out and return false after
               this many seconds, 0 value specifies no time out,
               negative value specifies default time out
      @pre     this object has an open socket connection
      @pre     0 < data.size() < 2^16
    */
    virtual bool writeData(const data_type& data, int time_out_second = -1);

    /**
      Return the most recent XML message that this client connection received.
      The message could be anything so client applications need to use a
      robust parser.

      @param  xml_string will contain an XML tree string if there is one
              available
      @pre    returns true iff an XML message arrived in a previous
              call to waitForData or readData.
    */
    virtual bool getXMLString(std::string& xml_string) const;

    /**
      Return the most recent error message as a string.

      @param message will contain a simple string suitable for display,
             debugging, or an error log.
      @pre   returns true iff the internal error string is non-empty.
    */
    virtual bool getErrorString(std::string& message) const;

protected:
    /** Raw socket file descriptor. */
    int m_socket;

    /** Current host name this object is connected to. */
    std::string m_host;

    /** Current port name this object is connected to. */
    unsigned m_port;

    /** String description of the remote service. */
    std::string m_description;

    /** Last XML message received. Only set if m_intercept_xml is true. */
    std::string m_xml_string;

    /** Grab XML messages and store them in the string buffer. */
    bool m_intercept_xml;

    /** Last error message from a call to the public interface. */
    std::string m_error_string;

    /**
      Hide an empty constructor. Child classes can use this to skip the
      connection attempt in the public constructor.
    */
    Client();

    /**
      Read a single binary message defined by a length header.

      This will block until is receives a non-empty message. An empty result
      message indicates a graceful disconnection of the remote host.

      @param   message output storage for the next message, if
               <tt>message.empty()</tt> then the socket connection has been
               gracefully terminated
      @return  always a pointer to this
      @pre     this object has an open socket connection
      @post    message contains some complete domain specific binary message
    */
    bool readMessage(data_type& data);

    /**
      Write a single binary message defined by a length header.

      @param   message binary message to write to the socket link.
      @return  always a pointer to this
      @pre     this object has an open socket connection
      @pre     message is not empty
    */
    bool writeMessage(const data_type& data);

    /**
      Set the receive time out for this socket.

      @param   second specifies the number of seconds
      @pre     this object has an open socket connection
      @post    any calls to receive will time out after <code>second</code>
               seconds
    */
    bool setReceiveTimeout(std::size_t second);

    /**
      Set the send time out for this socket.

      @param   second specifies the number of seconds
      @pre     this object has an open socket connection
      @post    any calls to send will time out after <code>second</code>
               seconds
    */
    bool setSendTimeout(std::size_t second);

    /**
      Allow for external access to the socket descriptor and other state.
    */
    friend class ClientAccess;

private:
    /** Initialization flag. Specific to the Winsock API. */
    bool m_initialize;

    /** Fixed size input buffer for receiving raw data. */
    data_type m_buffer;

    /** Dynamically sized input buffer spans multiple high level receive calls.
     */
    data_type m_next_message;

    /** Set this internal value to the current socket receive time out. */
    std::size_t m_time_out_second;

    /** Set this internal value to the current socket send time out. */
    std::size_t m_time_out_second_send;

    /**
      Initialize network subsystems and create a socket descriptor. Set any
      basic communication flags.
    */
    void socketInitialize();

    /**
      Shutdown and close our socket description. Clear our working buffers.
     */
    void socketClose();

    /**
      Low level socket receive command. Read a chunk into our object local
      buffer. This will block until it receives a non-empty message.

      @param   buf output storage for the next message, if <tt>data.empty()</tt>
               after this call then the socket connection has been gracefully
               terminated while we were blocking for incoming data
      @param   timed_out will be set to <code>true</code> if the system recv
      call timed out
      @return  the number of bytes read from the open socket connnection or 0 if
               the socket connection has been closed
      @pre     this object has an open socket connection
      @post    buf contains N bytes of raw data from the socket connection
    */
    int socketReceive(char* buf, std::size_t len, bool& timed_out);

    /**
      Low level socket send command. Write a chunk of data to the remote socket
      endpoint.

      @param   buf bytes to write to the socket
      @param   len number of bytes pointed to by buf
      @param   timed_out will be set to true if the system send call timed out
      @return  the number of bytes writtend to the open socket connnection or 0
               if the socket connection has been closed
      @pre     this object has an open socket connection
      @pre     data is not empty
      @post    N bytes of data was written to the socket
    */
    int socketSend(const char* buf, std::size_t len, bool& timed_out);
};

}} // namespace Motion::SDK

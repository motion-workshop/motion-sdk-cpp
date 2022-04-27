// Copyright Motion Workshop. All Rights Reserved.
#pragma once

#include <string>
#include <utility>

namespace Motion { namespace SDK {

/**
   The LuaConsole class sends a Lua chunk to the Motion Service console, parses
   the result code, and returns the printed output. Requires an existing Client
   connection to the Motion Service console.

   Example usage:
   @code
   try {
     using Motion::SDK::Client;
     using Motion::SDK::LuaConsole;

     // Send this chunk. Print something out.
     std::string chunk = "print('Hello World')";

     // Connect to the Motion Service console port.
     Client client("", 32075);

     auto result = LuaConsole::SendChunk(client, chunk);
     if (result.first == LuaConsole::Success) {
       // This should be "Hello World\n"
       std::cout << result.second;
     } else if (result.first == LuaConsole::Continue) {
       std::cerr << "incomplete Lua chunk: " << result.second;
     } else {
       std::cerr << "command failed: " << result.second;
     }

   } catch (std::runtime_error &e) {
     // The Client class may throw std::runtime_error for any unrecoverable
     // conditions.
   }
   @endcode
*/
class LuaConsole {
public:
  enum ResultCode {
    // The Lua chunk was successfully parsed and executed. The printed results
    // are in the result string.
    Success = 0,
    // The Lua chunk failed to run due to a compile time or execution time
    // error. An error description is in the result string.
    Failure = 1,
    // The Lua chunk was incomplete. The Console service is waiting for a
    // complete chunk before it executes. For example, "if x > 1 then" is
    // incomplete since it requires and "end" token to close the "if" control
    // statement.
    Continue = 2
  };

  /**
    Bundle together a code and printed results, basic tuple.
  */
  typedef std::pair<ResultCode, std::string> result_type;

  /**
    Write a Lua chunk to the open Console service socket and read back the
    printed results and a status code.
  */
  template <typename Client, typename String>
  static result_type SendChunk(
    Client &client, const String &chunk, const int &time_out_second = -1)
  {
    auto result = result_type{Failure, {}};

    typename Client::data_type data(chunk.begin(), chunk.end());

    if (!client.writeData(data, time_out_second)) {
      result.second = "failed to write Lua chunk to Console service";
      return result;
    }

    if (!client.readData(data, time_out_second) || data.empty()) {
      result.second = "failed to read response from Console service";
      return result;
    }

    // First character is the response code.
    const char code = data.at(0);
    if ((code < Success) || (code > Continue)) {
      result.second = "unknown result code from Console service";
      return result;
    }

    result.first = static_cast<ResultCode>(code);

    // The rest of the message is any printed output from the Lua environment.
    if (data.size() > 1) {
      result.second.assign(data.begin() + 1, data.end());
    }

    return result;
  }
}; // class LuaConsole

}} // namespace Motion::SDK

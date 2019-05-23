#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <Client.hpp>
#include <Format.hpp>
#include <LuaConsole.hpp>

#include <sstream>

TEST_CASE(
  "Client can stream from Configurable service and parse messages",
  "[Client][Format]")
{
  using namespace Motion::SDK;

  Client client("", 32076);

  REQUIRE(client.isConnected());

  {
    const std::string xml =
      "<?xml version=\"1.0\"?><configurable><Lq/><c/></configurable>";

    Client::data_type data(xml.begin(), xml.end());
    REQUIRE(client.writeData(data));
  }

  REQUIRE(client.waitForData());

  std::string xml;
  REQUIRE(client.getXMLString(xml));
  REQUIRE(!xml.empty());

  for (int i = 0; i < 5; ++i) {
    Client::data_type data;
    REQUIRE(client.readData(data));
    REQUIRE(!data.empty());

    auto map = Format::Configurable(data.begin(), data.end());
    REQUIRE(!map.empty());

    for (auto &kvp : map) {
      REQUIRE(kvp.first > 0);

      auto &elem = kvp.second;
      REQUIRE(elem.key() == kvp.first);

      REQUIRE(elem.size() == 8);
      REQUIRE(elem.getRange(0, elem.size()) == elem.access());

      for (std::size_t i = 0; i < elem.size(); ++i) {
        REQUIRE(elem[i] == elem.getRange(i, 1).front());
        REQUIRE(elem[i] == elem.access().at(i));
      }
    }
  }
}

TEST_CASE(
  "Client can stream from Preview service and parse messages",
  "[Client][Format]")
{
  using namespace Motion::SDK;

  Client client("", 32079);

  REQUIRE(client.isConnected());

  REQUIRE(client.waitForData());

  std::string xml;
  REQUIRE(client.getXMLString(xml));
  REQUIRE(!xml.empty());

  for (int i = 0; i < 5; ++i) {
    Client::data_type data;
    REQUIRE(client.readData(data));
    REQUIRE(!data.empty());

    auto map = Format::Preview(data.begin(), data.end());
    REQUIRE(!map.empty());

    for (auto &kvp : map) {
      REQUIRE(kvp.first > 0);

      auto &elem = kvp.second;
      REQUIRE(elem.key() == kvp.first);

      auto Gq = elem.getQuaternion(false);
      REQUIRE(Gq.size() == 4);

      auto Lq = elem.getQuaternion(true);
      REQUIRE(Lq.size() == 4);

      auto r = elem.getEuler();
      REQUIRE(r.size() == 3);

      auto la = elem.getAccelerate();
      REQUIRE(la.size() == 3);

      auto Gm = elem.getMatrix(false);
      REQUIRE(Gm.size() == 16);

      auto A = Format::QuaternionToMatrix(Gq);

      // Bad input, 3 elements is not a quaternion. Returns identity.
      auto B = Format::QuaternionToMatrix(r);

      // Bad input, zero length quaternion.
      auto C = Format::QuaternionToMatrix({0, 0, 0, 0});

      REQUIRE(A == Gm);
      REQUIRE(A != B);
      REQUIRE(B == C);
    }
  }
}

TEST_CASE(
  "Client can stream from Sensor service and parse messages",
  "[Client][Format]")
{
  using namespace Motion::SDK;

  Client client("", 32078);

  REQUIRE(client.isConnected());

  REQUIRE(client.waitForData());

  std::string xml;
  REQUIRE(client.getXMLString(xml));
  REQUIRE(!xml.empty());

  for (int i = 0; i < 5; ++i) {
    Client::data_type data;
    REQUIRE(client.readData(data));
    REQUIRE(!data.empty());

    auto map = Format::Sensor(data.begin(), data.end());
    REQUIRE(!map.empty());

    for (auto &kvp : map) {
      REQUIRE(kvp.first > 0);

      auto &elem = kvp.second;
      REQUIRE(elem.key() == kvp.first);

      auto a = elem.getAccelerometer();
      REQUIRE(a.size() == 3);

      auto m = elem.getMagnetometer();
      REQUIRE(m.size() == 3);

      auto g = elem.getGyroscope();
      REQUIRE(g.size() == 3);
    }
  }
}

TEST_CASE(
  "Client can stream from Raw service and parse messages", "[Client][Format]")
{
  using namespace Motion::SDK;

  Client client("", 32077);

  REQUIRE(client.isConnected());

  REQUIRE(client.waitForData());

  std::string xml;
  REQUIRE(client.getXMLString(xml));
  REQUIRE(!xml.empty());

  for (int i = 0; i < 5; ++i) {
    Client::data_type data;
    REQUIRE(client.readData(data));
    REQUIRE(!data.empty());

    auto map = Format::Raw(data.begin(), data.end());
    REQUIRE(!map.empty());

    for (auto &kvp : map) {
      REQUIRE(kvp.first > 0);

      auto &elem = kvp.second;
      REQUIRE(elem.key() == kvp.first);

      auto A = elem.getAccelerometer();
      REQUIRE(A.size() == 3);

      auto M = elem.getMagnetometer();
      REQUIRE(M.size() == 3);

      auto G = elem.getGyroscope();
      REQUIRE(G.size() == 3);
    }
  }
}

TEST_CASE(
  "Client can connect to the Console service and send commands",
  "[Client][LuaConsole]")
{
  using namespace Motion::SDK;

  Client client("", 32075);

  REQUIRE(client.isConnected());

  SECTION("send a Lua command a print the results")
  {
    const std::string lua = "return true";
    auto result = LuaConsole::SendChunk(client, lua);

    REQUIRE(result.first == LuaConsole::ResultCode::Success);
    REQUIRE(result.second == "true\n");
  }

  SECTION("can not send a Lua command to a closed client")
  {
    client.close();

    const std::string lua = "return true";
    auto result = LuaConsole::SendChunk(client, lua);

    REQUIRE(result.first == LuaConsole::ResultCode::Failure);
    REQUIRE(!result.second.empty());
  }

#if 0
  SECTION("send an incomplete Lua command")
  {
    std::string lua = "for i=1,2 do ";
    auto result = LuaConsole::SendChunk(client, lua);

    REQUIRE(result.first == LuaConsole::ResultCode::Continue);

    lua = "print(i) end";
    result = LuaConsole::SendChunk(client, lua);

    REQUIRE(result.first == LuaConsole::ResultCode::Success);
    REQUIRE(result.second == "1\n2\n");
  }

  SECTION("send a malformed Lua command with a compile time error")
  {
    const std::string lua = "for in 1,2";
    auto result = LuaConsole::SendChunk(client, lua);

    REQUIRE(result.first == LuaConsole::ResultCode::Failure);
  }

  SECTION("send Lua command with a runtime error")
  {
    const std::string lua = "error('my error message')";
    auto result = LuaConsole::SendChunk(client, lua);

    REQUIRE(result.first == LuaConsole::ResultCode::Failure);
  }
#endif
}

TEST_CASE(
  "LuaConsole fails when connected to Configurable service",
  "[Client][LuaConsole]")
{
  using namespace Motion::SDK;

  Client client("", 32076);

  REQUIRE(client.isConnected());

  const std::string lua = "return true";
  auto result = LuaConsole::SendChunk(client, lua);
  REQUIRE(result.first == LuaConsole::ResultCode::Failure);
}

#if 0

int quick_start(std::ostream *os)
{
  using namespace Motion::SDK;

  // Connect to a data service running in the Shadow software on your local
  // computer. The SDK is network capable and uses TCP sockets for transport.
  Client client("", 32076);
  if (!client.isConnected()) {
    return -1;
  }

  // The Configurable service sends back any channels that we request at
  // connection time.
  {
    // This is a typical setup for skeletal animation streaming. Local joint
    // rotations and world space joint positions. Enable inactive nodes to
    // get all joints in the skeleton.
    // Lq = local quaternion rotation, 4 channels
    // c = global positional constraint, 4 channels
    const std::string xml =
      "<?xml version=\"1.0\"?>"
      "<configurable inactive=\"1\"><Lq/><c/></configurable>";

    Client::data_type data(xml.begin(), xml.end());
    if (!client.writeData(data)) {
      return -1;
    }
  }

  // Block for up to 5 seconds. Wait for the first sample to arrive from the
  // data service.
  if (!client.waitForData()) {
    return -1;
  }

  // Enter the sample loop. For this quick start, just read 5 samples.
  for (int i=0; i<5; ++i) {
    // Read a message. The SDK connections are stream oriented and messages
    // arrive in sequence.
    Client::data_type data;
    if (!client.readData(data)) {
      return -1;
    }

    // We have a binary sample message from the data service. Parse it with the
    // Format class. Returns a std::map from integer key to ConfigurableElement.
    auto map = Format::Configurable(data.begin(), data.end());
    for (auto &kvp : map) {
      auto &item = kvp.second;
      // The Configurable service sends a variable number of channels. We should
      // have 8 per device since that is what we asked for.
      for (std::size_t i=0; i<item.size(); ++i) {
        *os << item[i] << " ";
      }
    }
    *os << "\n";
  }

  return 0;
}

TEST_CASE("Can run example from quick start documentation", "[Client][Format]")
{
  std::ostringstream out;
  REQUIRE(quick_start(&out) == 0);
  REQUIRE(!out.str().empty());
}

#endif

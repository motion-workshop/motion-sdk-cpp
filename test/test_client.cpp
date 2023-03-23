#include <Client.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Client can connect to local server", "[Client]")
{
    using Motion::SDK::Client;

    Client client("", 32076);
    REQUIRE(client.isConnected());

    SECTION("close the connection")
    {
        client.close();
        REQUIRE(!client.isConnected());

        Client::data_type data;
        REQUIRE(!client.readData(data));

        data.resize(10, 'x');
        REQUIRE(!client.writeData(data));

        REQUIRE(!client.waitForData());

        client.close();
        REQUIRE(!client.isConnected());
    }

    SECTION("start session, send channel list and read back device list")
    {
        {
            const std::string xml =
                "<?xml version=\"1.0\"?><configurable><Lq/><c/></configurable>";

            Client::data_type data(xml.begin(), xml.end());
            REQUIRE(client.writeData(data));
        }

        REQUIRE(client.waitForData());

        {
            std::string xml;
            REQUIRE(client.getXMLString(xml));
        }

        SECTION("read 10 samples")
        {
            for (int i = 0; i < 10; ++i) {
                Client::data_type data;
                REQUIRE(client.readData(data));

                REQUIRE(!data.empty());
                REQUIRE(data.size() % 40 == 0);
            }
        }
    }

    SECTION("start session, use custom timeouts")
    {
        {
            const std::string xml =
                "<?xml version=\"1.0\"?><configurable><Lq/><c/></configurable>";

            Client::data_type data(xml.begin(), xml.end());
            REQUIRE(client.writeData(data, 2));
        }

        REQUIRE(client.waitForData(4));

        {
            std::string xml;
            REQUIRE(client.getXMLString(xml));
        }

        SECTION("read 5 samples")
        {
            for (int i = 0; i < 10; ++i) {
                Client::data_type data;
                REQUIRE(client.readData(data, i));

                REQUIRE(!data.empty());
                REQUIRE(data.size() % 40 == 0);
            }
        }
    }

    SECTION("start session, fail empty write")
    {
        Client::data_type data;
        REQUIRE(!client.writeData(data));

        REQUIRE(!client.isConnected());
    }

    SECTION("start session, fail to write message that is too big")
    {
        Client::data_type data(65536);
        REQUIRE(!client.writeData(data));

        REQUIRE(!client.isConnected());
    }

    SECTION("did not start session, fail to read data")
    {
        REQUIRE(!client.waitForData());

        Client::data_type data;
        REQUIRE(!client.readData(data));
    }
}

TEST_CASE("Client fails to connect to bad address", "[Client]")
{
    using Motion::SDK::Client;

    Client client("0.0.1.x", 12345);
    REQUIRE(!client.isConnected());

    std::string message;
    REQUIRE(client.getErrorString(message));
    REQUIRE(!message.empty());
}

TEST_CASE("Client fails to connect to local server with wrong port", "[Client]")
{
    using Motion::SDK::Client;

    Client client("", 51222);
    REQUIRE(!client.isConnected());

    std::string message;
    REQUIRE(client.getErrorString(message));
    REQUIRE(!message.empty());
}

TEST_CASE("Client fails to connect to bad remote server", "[Client]")
{
    using Motion::SDK::Client;

    Client client("0.0.1.2", 51222);
    REQUIRE(!client.isConnected());

    std::string message;
    REQUIRE(client.getErrorString(message));
    REQUIRE(!message.empty());
}

TEST_CASE("Client error checking works", "[Client]")
{
    using Motion::SDK::Client;

    Client client("", 32000);
    REQUIRE(client.isConnected());

    SECTION("read header length too short, dropped connection")
    {
        std::string message = "header";

        Client::data_type data(message.begin(), message.end());
        REQUIRE(client.writeData(data));

        REQUIRE(client.waitForData());

        REQUIRE(!client.readData(data));

        REQUIRE(data.empty());

        REQUIRE(!client.isConnected());

        REQUIRE(client.getErrorString(message));
        REQUIRE(!message.empty());
    }

    SECTION("read header length too short, timed out")
    {
        std::string message = "header timeout";

        Client::data_type data(message.begin(), message.end());
        REQUIRE(client.writeData(data));

        REQUIRE(!client.readData(data));

        REQUIRE(data.empty());

        REQUIRE(!client.isConnected());

        REQUIRE(client.getErrorString(message));
        REQUIRE(!message.empty());
    }

    SECTION("read header length out of bounds")
    {
        std::string message = "length";

        Client::data_type data(message.begin(), message.end());
        REQUIRE(client.writeData(data));

        REQUIRE(!client.readData(data));

        REQUIRE(data.empty());

        REQUIRE(!client.isConnected());

        REQUIRE(client.getErrorString(message));
        REQUIRE(!message.empty());
    }

    SECTION("read payload length too short, dropped connection")
    {
        std::string message = "payload";

        Client::data_type data(message.begin(), message.end());
        REQUIRE(client.writeData(data));

        REQUIRE(client.waitForData());

        REQUIRE(!client.readData(data));

        REQUIRE(data.empty());

        REQUIRE(!client.isConnected());

        REQUIRE(client.getErrorString(message));
        REQUIRE(!message.empty());
    }

    SECTION("read payload length too short, timed out")
    {
        std::string message = "payload timeout";

        Client::data_type data(message.begin(), message.end());
        REQUIRE(client.writeData(data));

        REQUIRE(client.waitForData());

        REQUIRE(!client.readData(data));

        REQUIRE(data.empty());

        REQUIRE(!client.isConnected());

        REQUIRE(client.getErrorString(message));
        REQUIRE(!message.empty());
    }

    SECTION("read short xml is a regular message")
    {
        std::string message = "xml";

        Client::data_type data(message.begin(), message.end());
        REQUIRE(client.writeData(data));

        REQUIRE(client.waitForData());

        REQUIRE(client.readData(data));
        REQUIRE(!data.empty());
    }

    SECTION("write fails when remote recv buffer fills")
    {
        int failed_i = -1;

        Client::data_type data(65535, 0);
        for (int i = 0; i < 100; ++i) {
            if (!client.writeData(data)) {
                failed_i = i;
                break;
            }
        }

        REQUIRE(failed_i >= 0);
    }
}

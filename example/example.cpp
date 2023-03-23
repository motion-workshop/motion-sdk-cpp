// Copyright Motion Workshop. All Rights Reserved.
#include <Client.hpp>
#include <Format.hpp>

#include <iostream>

// Example application for the Motion C++ SDK classes.
int main(int argc, char** argv)
{
    using namespace Motion::SDK;

    // Connect to a data service running in the Shadow software on your local
    // computer. The SDK is network capable and uses TCP sockets for transport.
    Client client("", 32076);
    if (!client.isConnected()) {
        std::cerr << "failed to connect data service at localhost:32076\n";
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
            std::cerr << "failed to write channel list to data service\n";
            return -1;
        }
    }

    // Block for up to 5 seconds. Wait for the first sample to arrive from the
    // data service.
    if (!client.waitForData()) {
        std::cerr
            << "no data available after 5 seconds, device not connected\n";
        return -1;
    }

    // Enter the sample loop. For this quick start, just read 5 samples.
    for (int i = 0; i < 5; ++i) {
        // Read a message. The SDK connections are stream oriented and messages
        // arrive in sequence.
        Client::data_type data;
        if (!client.readData(data)) {
            std::cerr << "failed to read sample, data stream interrupted\n";
            return -1;
        }

        // We have a binary sample message from the data service. Parse it with
        // the Format class. Returns a std::map from integer key to
        // ConfigurableElement.
        auto map = Format::Configurable(data.begin(), data.end());
        for (auto& kvp : map) {
            auto& item = kvp.second;
            // The Configurable service sends a variable number of channels. We
            // should have 8 per device since that is what we asked for.
            for (std::size_t j = 0; j < item.size(); ++j) {
                std::cout << item[j] << " ";
            }
        }
        std::cout << "\n";
    }

    return 0;
}

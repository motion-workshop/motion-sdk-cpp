# Motion SDK C++

![Build](https://github.com/motion-workshop/motion-sdk-cpp/actions/workflows/main.yml/badge.svg)

## Introduction

The Motion Software Development Kit (SDK) is a collection of classes that
provide live streaming access to the measurement data from the
[Shadow](https://www.motionshadow.com) and
[MotionNode](https://www.motionnode.com) hardware. The SDK is open source and
available in multiple programming languages.

## Data streams

The SDK attaches to data streams published by the Motion Service. You may
connect to a predetermined data set or request a list of channels. One frame
of all connected hardware arrives at once.

Outputs from the tracking algorithms:

- `Gq`, global rotation quaternion
- `Lq`, local rotation quaternion
- `Bq`, body space rotation quaternion
- `r`, local rotation in Euler angles
- `la`, global linear acceleration
- `lv`, global linear velocity
- `lt`, global linear translation/position
- `c`, positional constraint, position and contact point weight

Measurements from the hardware:

- `a`, accelerometer measurement in g
- `m`, magnetometer measurement in microtesla
- `g`, gyroscope measurement in degree/second
- `A`, raw accelerometer measurement in integer format
- `M`, raw magnetometer measurement in integer format
- `G`, raw gyroscope measurement in integer format
- `p`, pressure sensor measurement data
- `dt`, time step in seconds
- `timestamp`, seconds since connection started, monotonic
- `systemtime`, second since connection starter, system wall clock

## Quick Start

### Connect to a data service

The SDK streams over the network. In this example, you are connecting to the
data service that is running on your local computer.

```cpp
// Connect to a data service running in the Shadow software on your local
// computer. The SDK is network capable and uses TCP sockets for transport.
Client client("", 32076);
if (!client.isConnected()) {
  return -1;
}
```

### Request a list of channels

The Configurable data service sends back any channels that we request at
connection time. This is a one time setup and can not be changed for the life of
the data stream.

```cpp

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
```

### Wait for the first message

If the data streams are active in the Motion Service, samples will start to
arrive immediately. The `Client::waitForData` methods allows you to make sure
that sample data is available now and you can go ahead and start your sample
loop. This method will also read the channel header which lists the names of the
devices.

```cpp
// Block for up to 5 seconds. Wait for the first sample to arrive from the
// data service.
if (!client.waitForData()) {
  std::cerr << "no data available after 5 seconds, device not connected\n";
  return -1;
}
```

### Read samples in a loop

Stream messages as they arrive. Expect \~10 ms of delay in a response to the
`Client::readData` method. Use the Format class to parse the messages into a
container of objects.

```cpp
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
    for (std::size_t j=0; j<item.size(); ++j) {
      std::cout << item[j] << " ";
    }
  }
  std::cout << "\n";
}
```

### Full example

The example above builds with the Motion SDK.

[example/example.cpp](example/example.cpp)

## Build

Use CMake to build and test the Motion SDK.

```console
  cmake -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build --config Release
  cd build
  ctest -C Release
```

## Compiler Support

The Motion SDK requires C++11 support. Here are the minimum versions of the
compilers we tested.

- Microsoft Visual Studio 2017
- Clang 3.3
- GCC 4.8.1

## License

The Motion SDK is distributed under a permissive [BSD License](LICENSE).

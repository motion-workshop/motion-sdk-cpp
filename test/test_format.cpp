#include <Format.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>

TEST_CASE("Configurable method can create a list of elements", "[Format]")
{
  using Motion::SDK::Format;

  const std::size_t Dim = 2 * 4 + 8 * 4;
  const auto data = std::array<float, 8>{{0, 1, 2, 3, 4, 5, 6, 7}};

  std::vector<char> buffer;
  for (std::size_t j = 1; j <= 10; ++j) {

    buffer.resize(j * Dim);
    {
      auto itr = buffer.begin() + (j - 1) * Dim;

      *reinterpret_cast<unsigned *>(&(*itr)) = static_cast<unsigned>(j);
      std::advance(itr, 4);
      *reinterpret_cast<unsigned *>(&(*itr)) = 8;
      std::advance(itr, 4);

      std::copy(data.begin(), data.end(), reinterpret_cast<float *>(&(*itr)));

      std::advance(itr, data.size() * 4);

      REQUIRE(itr == buffer.end());
    }

    {
      auto map = Format::Configurable(buffer.begin(), buffer.end());

      REQUIRE(map.size() == j);

      auto itr = map.find(j);
      REQUIRE(itr != map.end());

      auto &elem = itr->second;
      REQUIRE(elem.size() == 8);

      for (int i = 0; i < 8; ++i) {
        REQUIRE(elem[i] == data[i]);
      }

      auto range = elem.getRange(0, 8);
      REQUIRE(std::equal(data.begin(), data.end(), range.begin()));
    }

    {
      auto list = Format::MakeList<Format::ConfigurableElement>(
        buffer.begin(), buffer.end());

      REQUIRE(list.size() == j);
    }
  }

  auto map = Format::Configurable(buffer.begin(), buffer.end());
  REQUIRE(!map.empty());

  // Range request is too big, returns 9 zeros.
  auto &elem = map.begin()->second;
  auto range = elem.getRange(0, 9);
  REQUIRE(range.size() == 9);
  REQUIRE(!std::equal(data.begin(), data.end(), range.begin()));

  // Not enough bytes for a key.
  map = Format::Configurable(buffer.begin(), buffer.begin() + 3);
  REQUIRE(map.empty());

  // Not enough bytes for a payload length.
  map = Format::Configurable(buffer.begin(), buffer.begin() + 6);
  REQUIRE(map.empty());

  // Not enough bytes for a payload.
  map = Format::Configurable(buffer.begin(), buffer.begin() + Dim - 6);
  REQUIRE(map.empty());

  // Duplicate device key.
  std::copy(buffer.begin(), buffer.begin() + 4, buffer.begin() + 2 * Dim);
  map = Format::Configurable(buffer.begin(), buffer.end());
  REQUIRE(map.empty());

  // Zero length device payload.
  std::fill(buffer.begin() + Dim + 4, buffer.begin() + Dim + 8, 0);
  map = Format::Configurable(buffer.begin(), buffer.begin() + Dim + 8);
  REQUIRE(map.size() == 1);
}

TEST_CASE("Preview method can create a list of elements", "[Format]")
{
  using Motion::SDK::Format;

  const std::size_t Dim = 4 + 14 * 4;

  std::vector<char> buffer;
  buffer.resize(Dim);
  *reinterpret_cast<unsigned *>(&buffer[0]) = 1;

  auto map = Format::Preview(buffer.begin(), buffer.end());

  REQUIRE(map.size() == 1);

  buffer.resize(2 * Dim);
  *reinterpret_cast<unsigned *>(&buffer[Dim]) = 2;

  map = Format::Preview(buffer.begin(), buffer.end());

  REQUIRE(map.size() == 2);

  auto list =
    Format::MakeList<Format::PreviewElement>(buffer.begin(), buffer.end());

  REQUIRE(list.size() == 2);
}

TEST_CASE("Sensor method can create a list of elements", "[Format]")
{
  using Motion::SDK::Format;

  const std::size_t Dim = 4 + 9 * 4;

  std::vector<char> buffer;
  buffer.resize(Dim);
  *reinterpret_cast<unsigned *>(&buffer[0]) = 1;

  auto map = Format::Sensor(buffer.begin(), buffer.end());

  REQUIRE(map.size() == 1);

  buffer.resize(2 * Dim);
  *reinterpret_cast<unsigned *>(&buffer[Dim]) = 2;

  map = Format::Sensor(buffer.begin(), buffer.end());

  REQUIRE(map.size() == 2);

  auto list =
    Format::MakeList<Format::SensorElement>(buffer.begin(), buffer.end());

  REQUIRE(list.size() == 2);
}

TEST_CASE("Raw method can create a list of elements", "[Format]")
{
  using Motion::SDK::Format;

  const std::size_t Dim = 4 + 9 * 2;

  std::vector<char> buffer;
  buffer.resize(Dim);
  *reinterpret_cast<unsigned *>(&buffer[0]) = 1;

  auto map = Format::Raw(buffer.begin(), buffer.end());

  REQUIRE(map.size() == 1);

  buffer.resize(2 * Dim);
  *reinterpret_cast<unsigned *>(&buffer[Dim]) = 2;

  map = Format::Raw(buffer.begin(), buffer.end());

  REQUIRE(map.size() == 2);

  auto list =
    Format::MakeList<Format::RawElement>(buffer.begin(), buffer.end());

  REQUIRE(list.size() == 2);
}

/*
  @file    include/Format.hpp
  @author  Luke Tokheim, luke@motionshadow.com
  @version 3.0

  Copyright (c) 2019, Motion Workshop
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef __MOTION_SDK_FORMAT_HPP_
#define __MOTION_SDK_FORMAT_HPP_

#include <algorithm>
#include <iterator>
#include <map>
#include <vector>

#include <detail/endian_to_native.hpp>

namespace Motion { namespace SDK {

/**
  The Format class methods read a single binary message from the Motion Service
  and returns an object representation of that message. This is layered (by
  default) on top of the @ref Client class which handles the socket level
  binary message protocol.

  Example usage, extends the @ref Client class example:
  @code
  using Motion::SDK::Client;
  using Motion::SDK::Format;

  // Connect to the Preview data service.
  Client client("", 32079);

  Client::data_type data;
  while (client.readData(data)) {
    // Create an object representation of the current binary message.
    auto map = Format::Preview(data.begin(), data.end());

    // Iterate through the list of [id] => PreviewElement objects. This is an
    // STL map and stored as a key-value pair.
    for (auto &kvp : map) {
      // Use the PreviewElement interface to access format specific data.
      auto &element = kvp.second;
    }
  }
  @endcode
*/
class Format {
public:
  /** Defines the type of the integer map key for all Format types. */
  typedef std::size_t id_type;
  typedef std::size_t key_type;

  /** Container size. */
  typedef std::size_t size_type;

  /**
    The Motion Service send a list of data elements. The @ref Format
    functions create a map from integer id to array packed data for each
    service specific format.

    This is an abstract base class to implement a single format specific
    data element. The idea is that a child class implements a format specific
    interface (API) to access individual components of an array of packed
    data. The template parameter defines the type of the packed data
    elements.

    For example, the @ref PreviewElement class extends this class
    and provides a @ref PreviewElement#getEuler method to access
    an array of <tt>{x, y, z}</tt> Euler angles.
  */
  template <typename T>
  class Element {
  public:
    typedef T value_type;
    typedef std::vector<T> data_type;

  protected:
    /**
      Constructor is protected. Only allow child classes to call this.

      @param data array of packed data values for this Format::Element
      @param length valid length of the <tt>data</tt> array
    */
    Element(key_type key, data_type data) : m_key(key), m_data(std::move(data))
    {
    }

    /**
      Utility function to copy portions of the packed data array into its
      component elements.

      @param base starting index to copy data from the internal data array
      @param length number of data values in this component element
      @pre <tt>base < m_data.size()</tt>, <tt>base + element_length <
      m_data.size()</tt>
      @return an array of <tt>element_length</tt> values, assigned to
      <tt>{m_data[i] ... m_data[i+element_length]}</tt> if there are valid
      values available or zeros otherwise
    */
    data_type getRange(size_type base, size_type length) const
    {
      if (base + length > m_data.size()) {
        return data_type(length);
      }

      auto first = m_data.begin() + base;
      auto last = first + length;
      return data_type(first, last);
    }

  public:
    const key_type &key() const
    {
      return m_key;
    }

    const data_type &access() const
    {
      return m_data;
    }

  private:
    const key_type m_key;

    /**
      Array of packed binary data for this element. If <tt>data.empty() ==
      false</tt> then it contains a sample for each of the <tt>N</tt> channels.

      Define this as a private member. Only allow access through the
      getData method, and only allow it to child classes.
    */
    const data_type m_data;

    /**
      Provide direct access to the internal data buffer from client programs.

      @code
      class ElementAccess {
      public:
        template <typename T>
        static const typename Format::Element<T>::data_type &
        get(const Format::Element<T> &element)
        {
          return element.m_data;
        }
      };
      @endcode
    */
    friend class ElementAccess;
  }; // class Element

  /**
    The Configurable data services provides access to all data streams in
    a single message. The client selects channels and ordering at the
    start of the connection. The Configurable service sends a map of
    <tt>N</tt> data elements. Each data element is an array of <tt>M</tt>
    single precision floating point numbers.
  */
  class ConfigurableElement : public Element<float> {
  public:
    /** Variable length channels. */
    const static std::size_t Length = 0;
    static std::string Name;

    ConfigurableElement(key_type key, data_type data);

    /**
      Get a single channel entry at specified index.
    */
    const value_type &operator[](size_type pos) const;

    /**
      Convenience method. Size accessor.
    */
    size_type size() const;

    /**
      Get a contiguous range of channel entries specified by start index and
      number of elements.
    */
    data_type getRange(size_type base, size_type length) const;
  }; // class ConfigurableElement

  /**
    The Preview service provides access to the current orientation output as
    a quaternion, a set of Euler angles, or a 4-by-4 rotation matrix. The
    Preview service sends a map of <tt>N</tt> Preview data elements. Use this
    class to wrap a single Preview data element such that we can access
    individual components through a simple API.

    Preview element format:
    id => [
      global quaternion, local quaternion, local euler, global acceleration
    ]
    id => {Gqw, Gqx, Gqy, Gqz, Lqw, Lqx, Lqy, Lqz, rx, ry, rz, ax, ay, az}
  */
  class PreviewElement : public Element<float> {
  public:
    typedef Format::Element<float>::data_type data_type;

    /** Two quaternion channels, two 3-axis channels. */
    const static std::size_t Length = 2 * 4 + 2 * 3;
    static std::string Name;

    /**
      Initialize this container identifier with a packed data
      array in the Preview format.

      @param data is a packed array of global quaternion, local
      quaternion, local Euler angle, and local translation channel
      data
      @pre <tt>data.size() == Length</tt>
    */
    PreviewElement(key_type key, data_type data);

    /**
      Get a set of x, y, and z Euler angles that define the
      current orientation. Specified in radians assuming <tt>x-y-z</tt>
      rotation order. Not necessarily continuous over time, each
      angle lies on the domain <tt>[-pi, pi]</tt>.

      Euler angles are computed on the server side based on the
      current local quaternion orientation.

      @return a three element array <tt>{x, y, z}</tt> of Euler angles
      in radians or zeros if there is no available data
    */
    data_type getEuler() const;

    /**
      Get a 4-by-4 rotation matrix from the current global or local quaternion
      orientation. Specified as a 16 element array in row-major order.

      @param local set local to true get the local orientation, set local
      to false to get the global orientation
    */
    data_type getMatrix(bool local) const;

    /**
      Get the global or local unit quaternion that defines the current
      orientation.

      @param local set local to true get the local orientation, set local
      to false to get the global orientation
      @return a four element array <tt>{w, x, y, z}</tt> that defines a
      unit length quaternion <tt>q = w + x*i + y*j + z*k</tt> or zeros
      if there is no available data
    */
    data_type getQuaternion(bool local) const;

    /**
      Get x, y, and z of the current estimate of linear acceleration.
      Specified in g.

      @return a three element array <tt>{x, y, z}</tt> of linear acceleration
      channels specified in g or zeros if there is no available data
    */
    data_type getAccelerate() const;
  }; // class PreviewElement

  /**
    The Sensor service provides access to the current un-filtered sensor signals
    in real units. The Sensor service sends a map of <tt>N</tt> data elements.
    Use this class to wrap a single Sensor data element such that we can access
    individual components through a simple API.

    Sensor element format:
    id => [accelerometer, magnetometer, gyroscope]
    id => {ax, ay, az, mx, my, mz, gx, gy, gz}
  */
  class SensorElement : public Element<float> {
  public:
    typedef Format::Element<float>::data_type data_type;

    /** Three 3-axis channels. */
    const static std::size_t Length = 3 * 3;
    static std::string Name;

    /**
      Initialize this container identifier with a packed data
      array in the Sensor format.

      @param data is a packed array of accelerometer, magnetometer,
      and gyroscope un-filtered signal data.
      @pre <tt>data.size() == Length</tt>
    */
    SensorElement(key_type key, data_type data);

    /**
      Get a set of x, y, and z values of the current un-filtered
      accelerometer signal. Specified in <em>g</em> where 1 <em>g</em>
      = <tt>9.80665 meter/second^2</tt>.

      Domain varies with configuration. Maximum is <tt>[-6, 6]</tt>
      <em>g</em>.

      @return a three element array <tt>{x, y, z}</tt> of acceleration
      in <em>g</em>s or zeros if there is no available data
    */
    data_type getAccelerometer() const;

    /**
      Get a set of x, y, and z values of the current un-filtered
      gyroscope signal. Specified in <tt>degree/second</tt>.

      Valid domain of the sensor is <tt>[-500, 500] degree/second</tt>.
      Expect values outside of this domain as the system does not crop
      the sensor outputs.

      @return a three element array <tt>{x, y, z}</tt> of angular velocity
      in <tt>degree/second</tt> or zeros if there is no available data
    */
    data_type getGyroscope() const;

    /**
      Get a set of x, y, and z values of the current un-filtered
      magnetometer signal. Specified in <tt>µT</tt> (microtesla).

      Domain varies with local magnetic field strength. Expect values
      on domain <tt>[-60, 60]</tt> <tt>µT</tt> (microtesla).

      @return a three element array <tt>{x, y, z}</tt> of magnetic field
      strength in <tt>µT</tt> (microtesla) or zeros if there is no
      available data
    */
    data_type getMagnetometer() const;
  }; // class SensorElement

  /**
    The Raw service provides access to the current uncalibrated, unprocessed
    sensor signals in signed integer format. The Raw service sends a map of
    <tt>N</tt> data elements. Use this class to wrap a single Raw data element
    such that we can access individual components through a simple API.

    Raw element format:
    id => [accelerometer, magnetometer, gyroscope]
    id => {ax, ay, az, mx, my, mz, gx, gy, gz}

    All sensors output 12-bit integers. Process as 16-bit short integers on
    the server side.
  */
  class RawElement : public Element<short> {
  public:
    typedef Format::Element<short>::data_type data_type;

    /** Three 3-axis channels. */
    const static std::size_t Length = 3 * 3;
    static std::string Name;

    /**
      Initialize this container identifier with a packed data array in the Raw
      format.

      @param data is a packed array of accelerometer, magnetometer, and
             gyroscope unprocessed signal data
      @pre <tt>data.size() == Length</tt>
    */
    RawElement(key_type key, data_type data);

    /**
      Get a set of x, y, and z values of the current unprocessed accelerometer
      signal.

      Valid domain is <tt>[0, 4095]</tt>.

      @return a three element array <tt>{x, y, z}</tt> of raw
      accelerometer output or zeros if there is no available data
    */
    data_type getAccelerometer() const;

    /**
      Get a set of x, y, and z values of the current unprocessed gyroscope
      signal.

      Valid domain is <tt>[0, 4095]</tt>.

      @return a three element array <tt>{x, y, z}</tt> of raw gyroscope output
              or zeros if there is no available data
    */
    data_type getGyroscope() const;

    /**
      Get a set of x, y, and z values of the current unprocessed
      magnetometer signal.

      Valid domain is <tt>[0, 4095]</tt>.

      @return a three element array <tt>{x, y, z}</tt> of raw
      magnetometer output or zeros if there is no available data
    */
    data_type getMagnetometer() const;
  }; // class RawElement

  /**
    Define the associative container type for PreviewElement entries.
  */
  typedef std::map<key_type, ConfigurableElement> configurable_service_type;

  /**
    Convert a range of binary data into an associative container (std::map) of
    ConfigurableElement entries.

    @pre     <tt>[first, last)</tt> is a valid range
    @return  an associative container ConfigurableElement entries
  */
  template <typename Iterator>
  static inline configurable_service_type
  Configurable(Iterator first, Iterator last)
  {
    return Apply<ConfigurableElement>(first, last);
  }

  /**
    Define the associative container type for PreviewElement entries.
  */
  typedef std::map<key_type, PreviewElement> preview_service_type;

  /**
    Convert a range of binary data into an associative container (std::map) of
    PreviewElement entries.

    @pre     <tt>[first, last)</tt> is a valid range
    @return  an associative container PreviewElement entries
  */
  template <typename Iterator>
  static inline preview_service_type Preview(Iterator first, Iterator last)
  {
    return Apply<PreviewElement>(first, last);
  }

  /**
    Define the associative container type for SensorElement entries.
  */
  typedef std::map<key_type, SensorElement> sensor_service_type;

  /**
    Convert a range of binary data into an associative container (std::map) of
    SensorElement entries.

    @pre     <tt>[first, last)</tt> is a valid range
    @return  an associative container SensorElement entries
  */
  template <typename Iterator>
  static inline sensor_service_type Sensor(Iterator first, Iterator last)
  {
    return Apply<SensorElement>(first, last);
  }

  /**
    Define the associative container type for RawElement entries.
  */
  typedef std::map<key_type, RawElement> raw_service_type;

  /**
    Convert a range of binary data into an associative container (std::map) of
    RawElement entries.

    @pre     <tt>[first, last)</tt> is a valid range
    @return  an associative container RawElement entries
  */
  template <typename Iterator>
  static inline raw_service_type Raw(Iterator first, Iterator last)
  {
    return Apply<RawElement>(first, last);
  }

  /**
    Convert a range of bytes into a flat list (std::vector) of Elem entries.

    @pre     <tt>[first, last)</tt> is a valid range
    @return  an container of Elem entries
  */
  template <typename Elem, typename Iterator>
  static std::vector<Elem> MakeList(Iterator first, Iterator last)
  {
    return IdToValueArray<Elem>(first, last);
  }

  /**
    Convert a quaternion in [w, x, y, z] order into a 4x4 rotation matrix in
    row-major order.
  */
  static PreviewElement::data_type
  QuaternionToMatrix(const PreviewElement::data_type &q);

private:
  /**
    Convert a binary packed data representation from the Motion Service into a
    std::map<Format::key_type, Format::*Element>. Use the IdToValueArray method
    to handle the low level message parsing.
  */
  template <typename Elem, typename Iterator>
  static std::map<key_type, Elem> Apply(Iterator first, Iterator last)
  {
    // Make a flat array of Element objects.
    auto list = IdToValueArray<Elem>(first, last);
    if (list.empty()) {
      return {};
    }

    // Move the flat array Element objects into an associative container.
    std::map<key_type, Elem> map;
    for (auto &item : list) {
      const auto key = item.key();

      if (!map.emplace(key, std::move(item)).second) {
        return {};
      }
    }

    return map;
  }

  /**
    Convert a binary packed data representation from the Motion Service into a
    std::vector<Format::*Element>.

    @pre <tt>[first, last)</tt> is a valid range
    @pre type <tt>Key</tt> is an integral type
    @pre type <tt>Value</tt> is a model of a Sequence (STL)
  */
  template <typename Elem, typename Iterator>
  static std::vector<Elem> IdToValueArray(Iterator first, Iterator last)
  {
    typedef typename Elem::value_type value_type;

    static_assert(
      std::is_base_of<Element<value_type>, Elem>::value,
      "require Element class");
    static_assert(sizeof(*first) == 1, "require byte iterator");

    std::vector<Elem> list;

    while (first != last) {
      // Read the integer id for this element. Unpack the unsigned 32-bit
      // integer into our host system key type.
      if (
        static_cast<size_type>(std::distance(first, last)) < sizeof(unsigned)) {
        // Not enough bytes remaining. Invalid message.
        break;
      }

      const unsigned key = detail::little_endian_to_native(
        *reinterpret_cast<const unsigned *>(&(*first)));
      std::advance(first, sizeof(unsigned));

      // Read optional length integer this element. Some of the services send
      // fixed length samples and do not have the length prefix. Unpack the
      // unsigned 32-bit integer into our host system key type.
      auto length = Elem::Length;
      if (length == 0) {
        if (
          static_cast<size_type>(std::distance(first, last)) <
          sizeof(unsigned)) {
          // Not enough bytes remaining. Invalid message.
          break;
        }

        length = detail::little_endian_to_native(
          *reinterpret_cast<const unsigned *>(&(*first)));
        std::advance(first, sizeof(unsigned));
      }

      if (length == 0) {
        continue;
      }

      const auto num_bytes = length * sizeof(value_type);
      if (static_cast<size_type>(std::distance(first, last)) < num_bytes) {
        // Not enough bytes remaining. Invalid message.
        break;
      }

      // Assumes that the binary type stored in Elem::data_type matches the
      // Motion Service format. All services are 32-bit float except for Raw
      // which is 16-bit short.
      typename Elem::data_type data(length);
      std::copy_n(first, num_bytes, reinterpret_cast<char *>(&data[0]));

#if MOTION_SDK_BIG_ENDIAN
      // All service data is little-endian. Swap bytes.
      std::transform(
        data.begin(), data.end(), data.begin(),
        detail::little_endian_to_native<value_type>);
#endif // MOTION_SDK_BIG_ENDIAN

      list.emplace_back(Elem{key, std::move(data)});

      std::advance(first, num_bytes);
    }

    // If we did not consume all of the input bytes this is an invalid message.
    // Roll back intermediate results.
    if (first != last) {
      list.clear();
    }

    return list;
  }

  /**
    Hide the constructor. There is no need to instantiate the Format class.
  */
  Format();
}; // class Format

}} // namespace Motion::SDK

#endif // __MOTION_SDK_FORMAT_HPP_

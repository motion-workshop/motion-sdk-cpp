// Copyright Motion Workshop. All Rights Reserved.
#include <Format.hpp>

#include <limits>
#include <string>

namespace Motion { namespace SDK {

#if defined(__GNUC__)
const std::size_t Format::PreviewElement::Length;
const std::size_t Format::SensorElement::Length;
const std::size_t Format::RawElement::Length;
const std::size_t Format::ConfigurableElement::Length;
#endif // __GNUC__

std::string Format::ConfigurableElement::Name("Configurable");
std::string Format::PreviewElement::Name("Preview");
std::string Format::SensorElement::Name("Sensor");
std::string Format::RawElement::Name("Raw");

Format::ConfigurableElement::ConfigurableElement(key_type key, data_type data)
  : Element(key, std::move(data))
{
}

const Format::ConfigurableElement::value_type &
Format::ConfigurableElement::operator[](size_type pos) const
{
  return access().at(pos);
}

Format::size_type Format::ConfigurableElement::size() const
{
  return access().size();
}

Format::ConfigurableElement::data_type
Format::ConfigurableElement::getRange(size_type base, size_type length) const
{
  return Element::getRange(base, length);
}

Format::PreviewElement::PreviewElement(key_type key, data_type data)
  : Element<data_type::value_type>(key, std::move(data))
{
}

Format::PreviewElement::data_type Format::PreviewElement::getEuler() const
{
  return getRange(8, 3);
}

Format::PreviewElement::data_type
Format::PreviewElement::getMatrix(bool local) const
{
  return QuaternionToMatrix(getQuaternion(local));
}

Format::PreviewElement::data_type
Format::PreviewElement::getQuaternion(bool local) const
{
  return getRange(local ? 4 : 0, 4);
}

Format::PreviewElement::data_type Format::PreviewElement::getAccelerate() const
{
  return getRange(11, 3);
}

Format::SensorElement::SensorElement(key_type key, data_type data)
  : Element(key, std::move(data))
{
}

Format::SensorElement::data_type Format::SensorElement::getAccelerometer() const
{
  return getRange(0, 3);
}

Format::SensorElement::data_type Format::SensorElement::getGyroscope() const
{
  return getRange(6, 3);
}

Format::SensorElement::data_type Format::SensorElement::getMagnetometer() const
{
  return getRange(3, 3);
}

Format::RawElement::RawElement(key_type key, data_type data)
  : Element(key, std::move(data))
{
}

Format::RawElement::data_type Format::RawElement::getAccelerometer() const
{
  return getRange(0, 3);
}

Format::RawElement::data_type Format::RawElement::getGyroscope() const
{
  return getRange(6, 3);
}

Format::RawElement::data_type Format::RawElement::getMagnetometer() const
{
  return getRange(3, 3);
}

/**
  Ported from the Boost.Quaternion library at:
  http://www.boost.org/libs/math/quaternion/HSO3.hpp

  @param q defines a quaternion in the format [w x y z] where
  <tt>q = w + x*i + y*j + z*k = (w, x, y, z)</tt>
  @return an array of 16 elements that defines a 4-by-4 rotation
  matrix computed from the input quaternion or identity matrix if
  the input quaternion has zero length
*/
Format::PreviewElement::data_type
Format::QuaternionToMatrix(const PreviewElement::data_type &q)
{
  typedef Format::PreviewElement::value_type real_type;

  // Initialize the result matrix to the identity.
  PreviewElement::data_type result(16);
  {
    result[0] = result[5] = result[10] = result[15] = 1;
  }

  if (q.size() != 4) {
    return result;
  }

  const real_type &a = q[0];
  const real_type &b = q[1];
  const real_type &c = q[2];
  const real_type &d = q[3];

  const real_type aa = a * a;
  const real_type ab = a * b;
  const real_type ac = a * c;
  const real_type ad = a * d;
  const real_type bb = b * b;
  const real_type bc = b * c;
  const real_type bd = b * d;
  const real_type cc = c * c;
  const real_type cd = c * d;
  const real_type dd = d * d;

  const real_type norme_carre = aa + bb + cc + dd;

  if (norme_carre > 1e-6) {
    result[0] = (aa + bb - cc - dd) / norme_carre;
    result[1] = 2 * (-ad + bc) / norme_carre;
    result[2] = 2 * (ac + bd) / norme_carre;
    result[4] = 2 * (ad + bc) / norme_carre;
    result[5] = (aa - bb + cc - dd) / norme_carre;
    result[6] = 2 * (-ab + cd) / norme_carre;
    result[7] = 0;
    result[8] = 2 * (-ac + bd) / norme_carre;
    result[9] = 2 * (ab + cd) / norme_carre;
    result[10] = (aa - bb - cc + dd) / norme_carre;
  }

  return result;
}

}} // namespace Motion::SDK

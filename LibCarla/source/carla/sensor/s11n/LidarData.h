// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include "carla/rpc/Location.h"

#include <cstdint>
#include <vector>

namespace carla {
namespace sensor {
namespace s11n {

  /// Helper class to store and serialize the data generated by a Lidar.
  ///
  /// The header of a Lidar measurement consists of an array of uint32_t's in
  /// the following layout
  ///
  ///    {
  ///      Horizontal angle (float),
  ///      Channel count,
  ///      Point count of channel 0,
  ///      ...
  ///      Point count of channel n,
  ///    }
  ///
  /// The points are stored in an array of floats
  ///
  ///    {
  ///      X0, Y0, Z0, I0
  ///      ...
  ///      Xn, Yn, Zn, In
  ///    }
  ///

  class LidarDetection {
    public:
      geom::Location point;
      float intensity;

      LidarDetection() :
          point(0.0f, 0.0f, 0.0f), intensity{0.0f} { }
      LidarDetection(float x, float y, float z, float intensity) :
          point(x, y, z), intensity{intensity} { }
      LidarDetection(geom::Location p, float intensity) :
          point(p), intensity{intensity} { }

      void WritePlyHeaderInfo(std::ostream& out) const{
        out << "property float32 x\n" \
          "property float32 y\n" \
          "property float32 z\n" \
          "property float32 I\n";
      }

      void WriteDetection(std::ostream& out) const{
        out << point.x << ' ' << point.y << ' ' << point.z << ' ' << intensity;
      }
  };

  class LidarData {
    static_assert(sizeof(float) == sizeof(uint32_t), "Invalid float size");
    static const int SizeLidarDetection = 4;

    friend class LidarSerializer;
    friend class LidarHeaderView;

    enum Index : size_t {
      HorizontalAngle,
      ChannelCount,
      SIZE
    };

  public:
    explicit LidarData(uint32_t ChannelCount = 0u)
      : _header(Index::SIZE + ChannelCount, 0u) {
      _header[Index::ChannelCount] = ChannelCount;
    }

    LidarData &operator=(LidarData &&) = default;

    float GetHorizontalAngle() const {
      return reinterpret_cast<const float &>(_header[Index::HorizontalAngle]);
    }

    void SetHorizontalAngle(float angle) {
      std::memcpy(&_header[Index::HorizontalAngle], &angle, sizeof(uint32_t));
    }

    uint32_t GetChannelCount() const {
      return _header[Index::ChannelCount];
    }

    void Reset(uint32_t channel_point_count) {
      std::memset(_header.data() + Index::SIZE, 0, sizeof(uint32_t) * GetChannelCount());
      _max_channel_points = channel_point_count;

      _aux_points.resize(GetChannelCount());
      for (auto& aux : _aux_points) {
        aux.clear();
        aux.reserve(_max_channel_points);
      }
    }

    void WritePointAsync(uint32_t channel, LidarDetection detection) {
      DEBUG_ASSERT(GetChannelCount() > channel);
      _aux_points[channel].emplace_back(detection);
    }

    void SaveDetections() {
      _points.clear();
      _points.reserve(SizeLidarDetection * GetChannelCount() * _max_channel_points);

      for (auto idxChannel = 0u; idxChannel < GetChannelCount(); ++idxChannel) {
        _header[Index::SIZE + idxChannel] = static_cast<uint32_t>(_aux_points.size());
        for (auto& pt : _aux_points[idxChannel]) {
          _points.emplace_back(pt.point.x);
          _points.emplace_back(pt.point.y);
          _points.emplace_back(pt.point.z);
          _points.emplace_back(pt.intensity);
        }
      }
    }

  protected:
    std::vector<uint32_t> _header;
    std::vector<std::vector<LidarDetection>> _aux_points;
    uint32_t _max_channel_points;

  private:
    std::vector<float> _points;

  };

} // namespace s11n
} // namespace sensor
} // namespace carla
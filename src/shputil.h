
#pragma once

#include <stdint.h>
#include <vector>
#include <string>
#include <memory>

namespace shputil {

  enum class shape_type {
    null_shape=0, point=1, polyline=3, polygon=5, multipoint=8,
    pointz=11, polylinez=13, polygonz=15, multipointz=18,
    pointm=21, polylinem=23, multipointm=28, multipatch=31
  };
  
  class shape {
  public:
    virtual shputil::shape_type stype() { return(shputil::shape_type::null_shape); }
    virtual ~shape() {} 
  };

  class pointshape : public shape {
  public:
    pointshape() { x = 0.0; y = 0.0; }
    pointshape(double xin, double yin) { x = xin; y = yin; }
    shputil::shape_type stype() { return(shputil::shape_type::point); }
    double x;
    double y;
  };

  class multipointshape : public shape {
  public:
    shputil::shape_type stype() { return(shputil::shape_type::multipoint); }
    std::vector<pointshape> points;
  };

  class polypart {
  public:
    std::vector<pointshape> points;
  };
  
  class polyline : public shape {
  public:
    polyline() { }
    polyline(const polypart &line) { parts.push_back(line); }
    shputil::shape_type stype() { return(shputil::shape_type::polyline); }
    std::vector<polypart> parts;
  };

  class polygon : public shape {
  public:
    polygon() { }
    polygon(const polypart &ring) { rings.push_back(ring); }
    shputil::shape_type stype() { return(shputil::shape_type::polygon); }
    std::vector<polypart> rings;
  };

  using shape_ptr = std::shared_ptr<shape>;
  
  class shapefile {
  public:
    std::vector<shape_ptr> shapes;
  };
  
  bool read_shp(const std::string &path, shapefile &shpfile);
  bool write_shp(const std::string &path, const shapefile &shpfile);
  
} // shputil namespace

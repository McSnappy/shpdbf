
#include "shputil.h"
#include "logging.h"
#include <time.h>
#include <iostream>
#include <regex>

#ifdef __APPLE__
  #include <machine/endian.h>
#else
  #include <endian.h>
#endif

namespace shputil {

  static const int32_t SHAPEFILE_FILE_CODE = 9994;
  static const int32_t SHAPEFILE_VERSION = 1000;
  static const int32_t POLY_BASE_RECORD_SIZE = 44; // in bytes: int32_t shapetype, double bb[4], int32_t numparts, int32_t numpoints
  
  //
  // split the header into two chunks since when combined the struct gets padded by a few bytes
  //
  struct shapefile_main_header_base {
    int32_t file_code; // 9994  (BE)
    int32_t unused[5]; // (BE)
    int32_t file_length; // number of 16-bit words, including the 50 from this header (base+bb) (BE)
    int32_t version; // 1000, (LE)
    int32_t shape_type; // (LE)
    };

  
  struct shapefile_main_header_boundingbox {
    double xmin; // (LE)
    double ymin; // (LE)
    double xmax; // (LE)
    double ymax; // (LE)
    double zmin; // (LE)
    double zmax; // (LE)
    double mmin; // (LE)
    double mmax; // (LE)
  };

  
  struct shapefile_record_header {
    int32_t record_number;  // (BE)
    int32_t content_length; // (BE)
  };

  
  struct shapefile_record_reader {
    uint32_t file_length_bytes;
    uint32_t total_bytes_read;
    uint32_t alloc_size;
    uint8_t *record_buf;
    uint32_t current_content_bytes;
    shapefile_record_header current_record_header;
    FILE *fp;
  };

  
  static void init_record_reader(FILE *fp, const shapefile_main_header_base &header_base, shapefile_record_reader &reader) {
    reader.alloc_size = 0;
    reader.record_buf = 0;
    reader.current_content_bytes = 0;
    reader.total_bytes_read = sizeof(shapefile_main_header_base) + sizeof(shapefile_main_header_boundingbox);
    reader.file_length_bytes = 2 * header_base.file_length; // 2x since file_length is the # of 16-bit words
    reader.fp = fp;
  }

  
  static void handle_endianness(shapefile_record_header &record_header) {

#if BYTE_ORDER == BIG_ENDIAN

#elif BYTE_ORDER == LITTLE_ENDIAN
      record_header.record_number = __builtin_bswap32(record_header.record_number);
      record_header.content_length = __builtin_bswap32(record_header.content_length);
#else
      log_error("unknown endianness...\n");
      abort();
#endif
      
  }


#if BYTE_ORDER == BIG_ENDIAN
  static double swap_endianness_dbl(double dbl) {
    uint64_t tmp;
    memcpy(&tmp, &dbl, 8);
    tmp = __builtin_bswap64(tmp);
    double retdbl = 0.0;
    memcpy(&retdbl, &tmp, 8);
    return(retdbl);
  }
#endif

  
  static void handle_endianness(shapefile_main_header_base &header_base,
				shapefile_main_header_boundingbox &header_bb) {
    
    #if BYTE_ORDER == BIG_ENDIAN
      header_base.version = __builtin_bswap32(header_base.version);
      header_base.shape_type = __builtin_bswap32(header_base.shape_type);
      header_bb.xmin = swap_endianness_dbl(header_bb.xmin);
      header_bb.ymin = swap_endianness_dbl(header_bb.ymin);
      header_bb.xmax = swap_endianness_dbl(header_bb.xmax);
      header_bb.ymax = swap_endianness_dbl(header_bb.ymax);
      header_bb.zmin = swap_endianness_dbl(header_bb.zmin);
      header_bb.zmax = swap_endianness_dbl(header_bb.zmax);
      header_bb.mmin = swap_endianness_dbl(header_bb.mmin);
      header_bb.mmax = swap_endianness_dbl(header_bb.mmax);
    #elif BYTE_ORDER == LITTLE_ENDIAN
      header_base.file_length = __builtin_bswap32(header_base.file_length);
      header_base.file_code = __builtin_bswap32(header_base.file_code);
    #else
      log_error("unknown endianness...\n");
      abort();
    #endif
     
  }


  static bool read_main_header(FILE *fp, shapefile_main_header_base &header_base, shapefile_main_header_boundingbox &header_bb) {

    memset(&header_base, 0, sizeof(shapefile_main_header_base));
    memset(&header_bb, 0, sizeof(shapefile_main_header_boundingbox));

    if((fread(&header_base, sizeof(shapefile_main_header_base), 1, fp) != 1) ||
       (fread(&header_bb, sizeof(shapefile_main_header_boundingbox), 1, fp) != 1)) {
      log_error("couldn't read shapefile header...\n");
      return(false);
    }

    handle_endianness(header_base, header_bb);
    if(header_base.file_code != SHAPEFILE_FILE_CODE) {
      log_error("invalid shapefile file_code... fatal\n");
      return(false);
    }

    if(header_base.version != SHAPEFILE_VERSION) {
      log_error("invalid shapefile version... fatal\n");
      return(false);
    }
    
    log("file_code: %d\n", header_base.file_code);
    log("file_length: %d\n", header_base.file_length);
    log("version: %d\n", header_base.version);
    log("shape_type: %d\n", header_base.shape_type);
    log("xmin: %f\n", header_bb.xmin);
    log("ymin: %f\n", header_bb.ymin);
    log("xmax: %f\n", header_bb.xmax);
    log("ymax: %f\n", header_bb.ymax);
    log("zmin: %f\n", header_bb.zmin);
    log("zmax: %f\n", header_bb.zmax);
    log("mmin: %f\n", header_bb.mmin);
    log("mmax: %f\n", header_bb.mmax);

    return(true);
  }

  
  static bool read_record_header(FILE *fp, shapefile_record_header &record_header) {

    if(fread(&record_header, sizeof(shapefile_record_header), 1, fp) != 1) {
      log_error("couldn't read the record header...\n");
      return(false);
    }

    handle_endianness(record_header);

    if(record_header.content_length <= 0) {
      log_error("bogus record content length: %d\n", record_header.content_length);
      return(false);
    }
    
    return(true);
  }

  
  static bool read_shape_record(shapefile_record_reader &reader) {

    if(!reader.fp) {
      return(false);
    }
    
    if(reader.total_bytes_read >= reader.file_length_bytes) {
      return(false);
    }
     
    if(!read_record_header(reader.fp, reader.current_record_header)) {
      return(false);
    }

    reader.current_content_bytes = reader.current_record_header.content_length * 2; // 2x since content_length is the # of 16-bit words
    if(reader.current_content_bytes > reader.alloc_size) {

      if(reader.record_buf) {
	free(reader.record_buf);
      }
	
      reader.record_buf = (uint8_t *) malloc(reader.current_content_bytes);
      reader.alloc_size = reader.current_content_bytes;
	
    }
      
    if(!reader.record_buf) {
      log_error("couldn't allocate memory for record content: %d bytes\n", reader.current_content_bytes);
      return(false);
    }

    if(fread(reader.record_buf, reader.current_content_bytes, 1, reader.fp) != 1) {
      log_error("couldn't read record content\n");
      return(false);
    }
 
    reader.total_bytes_read += reader.current_content_bytes + sizeof(shapefile_record_header);
    
    return(true); 
  }

  
  static void close_record_reader(shapefile_record_reader &reader) {
    if(reader.record_buf) {
      free(reader.record_buf);
      reader.record_buf = 0;
    }
  }

      
  static int32_t fetch_LEint32(uint8_t *content) {
    
    if(!content) {
      return(0);
    }

    int32_t *ptr = (int32_t *)content;
    int32_t val = *ptr;
    #if BYTE_ORDER == BIG_ENDIAN
      val =  __builtin_bswap32(val);
    #endif

    return(val);
  }

  static double fetch_LEdouble(uint8_t *content) {

    if(!content) {
      return(0.0);
    }

    double *ptr = (double *)content;
    double val = *ptr;
#if BYTE_ORDER == BIG_ENDIAN
    val = swap_endianness_dbl(val);
#endif

    return(val);
  }

  
  static bool read_point_shapes(shapefile_record_reader &reader, shapefile &shpfile) {

    while(read_shape_record(reader)) {
      
      log("point record header: recnum %d, content len %d\n",
	  reader.current_record_header.record_number,
	  reader.current_record_header.content_length);

      if(reader.current_record_header.content_length != 10) {
	log_error("invalid point record size...\n");
	return(false);
      }

      int32_t stype = fetch_LEint32(reader.record_buf);
      if((shputil::shape_type)stype == shape_type::null_shape) {
	log("found null shape... skipping\n");
	continue;
      }
      
      if((shputil::shape_type)stype != shputil::shape_type::point) {
	log_error("invalid shape_type, expected point\n");
	return(false);
      }

      double x = fetch_LEdouble(reader.record_buf + sizeof(int32_t));
      double y = fetch_LEdouble(reader.record_buf + sizeof(int32_t) + sizeof(double));
      log("x,y = %.6f, %.6f\n", x, y);

      shpfile.shapes.push_back(std::make_shared<pointshape>(x, y));
    }
    
    return(true);
  }

  
  static bool read_polyline_shapes(shapefile_record_reader &reader, shapefile &shpfile) {

    while(read_shape_record(reader)) {
      
      log("polyline record header: recnum %d, content len %d\n",
	  reader.current_record_header.record_number,
	  reader.current_record_header.content_length);
      
      int32_t stype = fetch_LEint32(reader.record_buf);
      if((shputil::shape_type)stype == shape_type::null_shape) {
	log("found null shape... skipping\n");
	continue;
      }
      
      if((shputil::shape_type)stype != shape_type::polyline) {
	log_error("record shape_type mismatch, expected polyline...\n");
	return(false);
      }
      
      uint32_t offset = sizeof(int32_t) + (4 * sizeof(double)); // shape_type + 4 bb doubles
      int32_t num_parts = fetch_LEint32(reader.record_buf + offset);
      offset += sizeof(int32_t);
      int32_t num_points = fetch_LEint32(reader.record_buf + offset);
      offset += sizeof(int32_t);

      log("num_parts: %d\n", num_parts);
      log("num_points: %d\n\n", num_points);

      uint32_t parts_offset = offset;
      uint32_t points_offset = parts_offset + (num_parts * sizeof(int32_t));

      polypart allparts;
      uint8_t *part_points_start = (reader.record_buf + points_offset);
      uint32_t part_points_offset = 0;
      for(int point_idx = 0; point_idx < num_points; ++point_idx) {
	double x = fetch_LEdouble(part_points_start + part_points_offset);
	double y = fetch_LEdouble(part_points_start + part_points_offset + sizeof(double));
	//log("x,y = %.3f, %.3f\n", x, y);
	part_points_offset += (2 * sizeof(double));
	allparts.points.push_back(pointshape(x, y));
      }
      
      polyline pl;
      if(num_parts == 1) {
	pl.parts.push_back(allparts);
      }
      else {
	int32_t prev_part_start = 0;
	for(int32_t idx=1; idx < num_parts; ++idx) {
	  polypart part;
	  int32_t part_start = fetch_LEint32(reader.record_buf + parts_offset + (idx * sizeof(int32_t)));
	  //log("part_start = %d\n", part_start);
	  std::vector<pointshape>::const_iterator first = allparts.points.begin() + prev_part_start;
	  std::vector<pointshape>::const_iterator last = allparts.points.begin() + part_start;
	  part.points = {first, last};
	  prev_part_start = part_start;
	  pl.parts.push_back(part);
	}

	polypart final_part;
	std::vector<pointshape>::const_iterator first = allparts.points.begin() + prev_part_start;
	std::vector<pointshape>::const_iterator last = allparts.points.end();
	final_part.points = {first, last};
	pl.parts.push_back(final_part);
	
      }
      
      shpfile.shapes.push_back(std::make_shared<polyline>(pl));

    }

    return(true);
  }

  
  static bool read_polygon_shapes(shapefile_record_reader &reader, shapefile &shpfile) {
    
    while(read_shape_record(reader)) {
      
      log("polygon record header: recnum %d, content len %d\n",
	  reader.current_record_header.record_number,
	  reader.current_record_header.content_length);
      
      int32_t stype = fetch_LEint32(reader.record_buf);
      if((shputil::shape_type)stype == shape_type::null_shape) {
	log("found null shape... skipping\n");
	continue;
      }
      
      if((shputil::shape_type)stype != shape_type::polygon) {
	log_error("record shape_type mismatch..., expected polygon\n");
	return(false);
      }
      
      uint32_t offset = sizeof(int32_t) + (4 * sizeof(double)); // shape_type + 4 bb doubles
      int32_t num_rings = fetch_LEint32(reader.record_buf + offset);
      offset += sizeof(int32_t);
      int32_t num_points = fetch_LEint32(reader.record_buf + offset);
      offset += sizeof(int32_t);

      log("num_rings: %d\n", num_rings);
      log("num_points: %d\n\n", num_points);

      uint32_t rings_offset = offset;
      uint32_t points_offset = rings_offset + (num_rings * sizeof(int32_t));

      polypart allparts;
      uint8_t *ring_points_start = (reader.record_buf + points_offset);
      uint32_t ring_points_offset = 0;
      for(int point_idx = 0; point_idx < num_points; ++point_idx) {
	double x = fetch_LEdouble(ring_points_start + ring_points_offset);
	double y = fetch_LEdouble(ring_points_start + ring_points_offset + sizeof(double));
	//log("x,y = %.3f, %.3f\n", x, y);
	ring_points_offset += (2 * sizeof(double));
	allparts.points.push_back(pointshape(x, y));
      }

      //log("allparts size = %d\n", allparts.points.size());
      
      polygon pg;
      if(num_rings == 1) {
	pg.rings.push_back(allparts);
      }
      else {
	int32_t prev_part_start = 0;
	for(int32_t idx=1; idx < num_rings; ++idx) {
	  polypart part;
	  int32_t part_start = fetch_LEint32(reader.record_buf + rings_offset + (idx * sizeof(int32_t)));
	  //log("part_start = %d\n", part_start);
	  std::vector<pointshape>::const_iterator first = allparts.points.begin() + prev_part_start;
	  std::vector<pointshape>::const_iterator last = allparts.points.begin() + part_start;
	  part.points = {first, last};
	  prev_part_start = part_start;
	  pg.rings.push_back(part);
	}

	polypart final_part;
	std::vector<pointshape>::const_iterator first = allparts.points.begin() + prev_part_start;
	std::vector<pointshape>::const_iterator last = allparts.points.end();
	final_part.points = {first, last};
	pg.rings.push_back(final_part);
	
      }
      
      shpfile.shapes.push_back(std::make_shared<polygon>(pg));

    }

    return(true);
  }

  
  static bool read_multipoint_shapes(shapefile_record_reader &reader, shapefile &shpfile) {

    while(read_shape_record(reader)) {
      
      log("multipoint record header: recnum %d, content len %d\n",
	  reader.current_record_header.record_number,
	  reader.current_record_header.content_length);

      int32_t stype = fetch_LEint32(reader.record_buf);
      if((shputil::shape_type)stype == shape_type::null_shape) {
	log("found null shape... skipping\n");
	continue;
      }
      
      if((shputil::shape_type)stype != shputil::shape_type::multipoint) {
	log_error("invalid shape_type, expected multipoint\n");
	return(false);
      }

      uint32_t offset = sizeof(int32_t) + (4 * sizeof(double)); // shape_type + 4 bb doubles
      int32_t num_points = fetch_LEint32(reader.record_buf + offset);
      offset += sizeof(int32_t);

      multipointshape mpshape;
      for(int ii=0; ii < num_points; ++ii) {
	double x = fetch_LEdouble(reader.record_buf + offset);
	double y = fetch_LEdouble(reader.record_buf + offset + sizeof(double));
	log("x,y = %.6f, %.6f\n", x, y);
	mpshape.points.push_back(pointshape(x, y));
	offset += (2 * sizeof(double));
      }

      shpfile.shapes.push_back(std::make_shared<multipointshape>(mpshape));
    }
    
    return(true);
  }

  
  bool read_shp(const std::string &path, shapefile &shpfile) {

    shpfile.shapes.clear();

    FILE *fp = fopen(path.c_str(), "rb");
    if(!fp) {
      log_error("couldn't open shapefile: %s\n", path.c_str());
      return(false);
    }
    
    shapefile_main_header_base header_base;
    shapefile_main_header_boundingbox header_bb;
    if(!read_main_header(fp, header_base, header_bb)) {
      fclose(fp);
      return(false);
    }
    
    shapefile_record_reader reader;
    init_record_reader(fp, header_base, reader);
		       
    bool status = false;
    switch((shape_type)header_base.shape_type) {
    case shape_type::point: status = read_point_shapes(reader, shpfile); break;
    case shape_type::polyline: status = read_polyline_shapes(reader, shpfile); break;
    case shape_type::polygon: status = read_polygon_shapes(reader, shpfile); break;
    case shape_type::multipoint: status = read_multipoint_shapes(reader, shpfile); break;
    default:
      log_error("unsupported shape_type: %d\n", header_base.shape_type);
      break;
    }

    close_record_reader(reader);
    
    if(!status) {
      return(false);
    }
    
    fclose(fp);
    
    return(true);
  }


  static shputil::shape_type determine_shape_type(const shapefile &shpfile) {

    shputil::shape_type stype = shputil::shape_type::null_shape;

    for(auto &ptr : shpfile.shapes) {
      shputil::shape_type st = ptr->stype();
      if(st == shputil::shape_type::null_shape) {
	continue;
      }

      if(stype == shputil::shape_type::null_shape) {
	stype = st;
      }
      else {
	if(stype != st) {
	  log_error("found multiple shape types...\n");
	  return(shputil::shape_type::null_shape);
	}
      }
      
    }
    
    return(stype);
  }


  static bool write_main_header(FILE *fp, const shapefile_main_header_base &header_base_in,
				const shapefile_main_header_boundingbox &header_bb_in) {

    shapefile_main_header_base header_base = header_base_in;
    shapefile_main_header_boundingbox header_bb = header_bb_in;
    handle_endianness(header_base, header_bb);
	
    if((fwrite(&header_base, sizeof(shapefile_main_header_base), 1, fp) != 1) ||
       (fwrite(&header_bb, sizeof(shapefile_main_header_boundingbox), 1, fp) != 1)) {
      log_error("couldn't write shapefile header...\n");
      return(false);
    }

    return(true);
  }


  static void determine_point_shape_bb(const shapefile &shpfile, shapefile_main_header_boundingbox &header_bb) {

    memset(&header_bb, 0, sizeof(shapefile_main_header_boundingbox));
    bool first = true;
    for(auto &ptr : shpfile.shapes) {

      if(ptr->stype() != shape_type::point) {
	continue;
      }
      
      pointshape *ps = (pointshape *) ptr.get();
      if(first || (ps->x < header_bb.xmin)) {
	header_bb.xmin = ps->x;
      }

      if(first || (ps->x > header_bb.xmax)) {
	header_bb.xmax = ps->x;
      }

      if(first || (ps->y < header_bb.ymin)) {
	header_bb.ymin = ps->y;
      }

      if(first || (ps->y > header_bb.ymax)) {
	header_bb.ymax = ps->y;
      }

      first = false;
    }
    
  }

  
  static bool write_point_shapes(FILE *fp, FILE *shxfp, const shapefile &shpfile) {

    log("write_point_shapes: %d point(s)\n", shpfile.shapes.size());

    const int32_t POINT_RECORD_SIZE = 20; // in bytes: int32_t shapetype + double x + double y
    shapefile_main_header_base header_base;
    shapefile_main_header_boundingbox header_bb;
    memset(&header_base, 0, sizeof(shapefile_main_header_base));
    header_base.file_code = SHAPEFILE_FILE_CODE;
    header_base.version = SHAPEFILE_VERSION;
    header_base.shape_type = (int32_t) shape_type::point;
    header_base.file_length = sizeof(shapefile_main_header_base) + sizeof(shapefile_main_header_boundingbox) +
      (shpfile.shapes.size() * (POINT_RECORD_SIZE + sizeof(shapefile_record_header)));
    header_base.file_length = header_base.file_length / 2; // reported as the # of 16-bit words
    determine_point_shape_bb(shpfile, header_bb);
    
    if(!write_main_header(fp, header_base, header_bb)) {
      log_error("couldn't write shapefile main header\n");
      return(false);
    }

    //
    // write the shx header
    //
    header_base.file_length = (sizeof(shapefile_main_header_base) + sizeof(shapefile_main_header_boundingbox) + (2 * sizeof(int32_t) * shpfile.shapes.size())) / 2;
    if(!write_main_header(shxfp, header_base, header_bb)) {
      log_error("couldn't write shx header\n");
      return(false);
    }
    
    uint8_t record_content[POINT_RECORD_SIZE];
    int32_t stype = (int32_t) shape_type::point;
#if BYTE_ORDER == BIG_ENDIAN
    stype = __builtin_bswap32(stype);
#endif

    memcpy(record_content, &stype, sizeof(int32_t));
    int32_t recnum = 1;
    
    for(auto &ptr : shpfile.shapes) {

      shapefile_record_header rh;
      rh.record_number = recnum++;
      rh.content_length = POINT_RECORD_SIZE / 2; // # 16-bit words

      shapefile_record_header shx_rh;
      shx_rh.record_number = ftell(fp) / 2; // the offset in 16-bit words to the start of this record
      shx_rh.content_length = rh.content_length;  // shx reports the same content_length

      handle_endianness(shx_rh);
      if(fwrite(&shx_rh, sizeof(shapefile_record_header), 1, shxfp) != 1) {
	log_error("couldn't write point shx record\n");
	return(false);
      }
      
      handle_endianness(rh);
      if(fwrite(&rh, sizeof(shapefile_record_header), 1, fp) != 1) {
	log_error("couldn't write point record header\n");
	return(false);
      }
      
      pointshape *ps = (pointshape *) ptr.get();
      double x = ps->x;
      double y = ps->y;
#if BYTE_ORDER == BIG_ENDIAN
      x = swap_endianness_dbl(x);
      y = swap_endianness_dbl(y);
#endif
      memcpy(record_content + sizeof(int32_t), &x, sizeof(double));
      memcpy(record_content + sizeof(int32_t) + sizeof(double), &y, sizeof(double));
      if(fwrite(record_content, POINT_RECORD_SIZE, 1, fp) != 1) {
	log_error("couldn't write point shape record\n");
	return(false);
      }
    }

    return(true);
  }


  static void determine_multipoint_bb(const std::vector<pointshape> &points, shapefile_main_header_boundingbox &header_bb) {
    memset(&header_bb, 0, sizeof(shapefile_main_header_boundingbox));
    bool first = true;
    for(const pointshape &ps : points) {
      if(first || (ps.x < header_bb.xmin)) {
	header_bb.xmin = ps.x;
      }
      
      if(first || (ps.x > header_bb.xmax)) {
	header_bb.xmax = ps.x;
      }
      
      if(first || (ps.y < header_bb.ymin)) {
	header_bb.ymin = ps.y;
      }
      
      if(first || (ps.y > header_bb.ymax)) {
	header_bb.ymax = ps.y;
      }
      
      first = false;
    }
  }

  
  static int32_t determine_multipoint_shape_bb(const shapefile &shpfile, shapefile_main_header_boundingbox &header_bb) {

    //
    // returns the number of points within all the multipoint shapes
    //
   
    int32_t numpoints = 0;
    std::vector<pointshape> points;
    
    for(auto &ptr : shpfile.shapes) {

      if(ptr->stype() != shape_type::multipoint) {
	continue;
      }
      
      multipointshape *mps = (multipointshape *) ptr.get();
      numpoints += mps->points.size();
      points.insert(points.end(), mps->points.begin(), mps->points.end());
      
    }

    determine_multipoint_bb(points, header_bb);
    
    return(numpoints);
  }

  
  static bool write_multipoint_shapes(FILE *fp, FILE *shxfp, const shapefile &shpfile) {

    log("write_multipoint_shapes: %d shape(s)\n", shpfile.shapes.size());

    const int32_t MULTIPOINT_BASE_SIZE = 40; // in bytes: int32_t shapetype + double bb[4] + int32_t numpoints
    shapefile_main_header_base header_base;
    shapefile_main_header_boundingbox header_bb;
    int32_t numpoints = determine_multipoint_shape_bb(shpfile, header_bb);
    memset(&header_base, 0, sizeof(shapefile_main_header_base));
    header_base.file_code = SHAPEFILE_FILE_CODE;
    header_base.version = SHAPEFILE_VERSION;
    header_base.shape_type = (int32_t) shape_type::multipoint;
    header_base.file_length = sizeof(shapefile_main_header_base) + sizeof(shapefile_main_header_boundingbox) +
      (shpfile.shapes.size() * (MULTIPOINT_BASE_SIZE + sizeof(shapefile_record_header))) + (numpoints * 2 * sizeof(double));
    header_base.file_length = header_base.file_length / 2; // reported as the # of 16-bit words

    if(!write_main_header(fp, header_base, header_bb)) {
      log_error("couldn't write shapefile main header\n");
      return(false);
    }

    //
    // write the shx header
    //
    header_base.file_length = (sizeof(shapefile_main_header_base) + sizeof(shapefile_main_header_boundingbox) + (2 * sizeof(int32_t) * shpfile.shapes.size())) / 2;
    if(!write_main_header(shxfp, header_base, header_bb)) {
      log_error("couldn't write shx header\n");
      return(false);
    }

    
    int32_t recnum = 1;
    int32_t stype = (int32_t) shape_type::multipoint;
#if BYTE_ORDER == BIG_ENDIAN
    stype = __builtin_bswap32(stype);
#endif
    
    for(auto &ptr : shpfile.shapes) {
      multipointshape *mps = (multipointshape *) ptr.get();
      shapefile_main_header_boundingbox shape_bb; // used here just for bbox computation
      determine_multipoint_bb(mps->points, shape_bb);
      int32_t numpoints = mps->points.size();
      int32_t content_bytes = MULTIPOINT_BASE_SIZE + + (2 * sizeof(double) * numpoints);
      shapefile_record_header rh;
      rh.record_number = recnum++;
      rh.content_length = content_bytes / 2; // # 16-bit words

      shapefile_record_header shx_rh;
      shx_rh.record_number = ftell(fp) / 2; // the offset in 16-bit words to the start of this record
      shx_rh.content_length = rh.content_length;  // shx reports the same content_length

      handle_endianness(shx_rh);
      if(fwrite(&shx_rh, sizeof(shapefile_record_header), 1, shxfp) != 1) {
	log_error("couldn't write multipoint shx record\n");
	return(false);
      }
      
      handle_endianness(rh);
      if(fwrite(&rh, sizeof(shapefile_record_header), 1, fp) != 1) {
	log_error("couldn't write multipoint record header\n");
	return(false);
      }

#if BYTE_ORDER == BIG_ENDIAN
      shape_bb.xmin = swap_endianness_dbl(shape_bb.xmin);
      shape_bb.ymin = swap_endianness_dbl(shape_bb.ymin);
      shape_bb.xmax = swap_endianness_dbl(shape_bb.xmax);
      shape_bb.ymax = swap_endianness_dbl(shape_bb.ymax);
      numpoints = __builtin_bswap32(numpoints);
#endif

      if(fwrite(&stype, sizeof(int32_t), 1, fp) != 1) {
	log_error("couldn't write multipoint stype\n");
	return(false);
      }
      
      if(fwrite(&shape_bb, sizeof(double) * 4, 1, fp) != 1) {
	log_error("couldn't write multipoint bb\n");
	return(false);
      }

      if(fwrite(&numpoints, sizeof(int32_t), 1, fp) != 1) {
	log_error("couldn't write multipoint numpoints\n");
	return(false);
      }

      for(const pointshape &ps : mps->points) {
	double x = ps.x;
	double y = ps.y;
#if BYTE_ORDER == BIG_ENDIAN
	x = swap_endianness_dbl(x);
	y = swap_endianness_dbl(y);
#endif

	if((fwrite(&x, sizeof(double), 1, fp) != 1) ||
	   (fwrite(&y, sizeof(double), 1, fp) != 1)) {
	  log_error("couldn't write multipoint x,y\n");
	  return(false);
	}
	
      }
    }

    
    return(true);
  }

  
  static int32_t determine_polypart_bb(const std::vector<polypart> &parts, shapefile_main_header_boundingbox &header_bb) {

    //
    // returns the number of points within all the polyparts
    //

    int32_t numpoints = 0;
    
    memset(&header_bb, 0, sizeof(shapefile_main_header_boundingbox));
    bool first = true;
    
    for(const polypart &part : parts) {

      numpoints += part.points.size();
      
      for(const pointshape &ps : part.points) {
	
	if(first || (ps.x < header_bb.xmin)) {
	  header_bb.xmin = ps.x;
	}
	
	if(first || (ps.x > header_bb.xmax)) {
	  header_bb.xmax = ps.x;
	}
	
	if(first || (ps.y < header_bb.ymin)) {
	  header_bb.ymin = ps.y;
	}
	
	if(first || (ps.y > header_bb.ymax)) {
	  header_bb.ymax = ps.y;
	}
	
	first = false;
      }
    }

    return(numpoints);
  }

  
  static int32_t determine_polypart_bb(const shapefile &shpfile, shapefile_main_header_boundingbox &header_bb) {

    //
    // returns the number of bytes required to store this polyline/polygon
    //
    
    std::vector<polypart> parts;
    for(auto &ptr : shpfile.shapes) {
      
      if(ptr->stype() == shape_type::polyline) {
	polyline *pl = (polyline *) ptr.get();
	parts.insert(parts.end(), pl->parts.begin(), pl->parts.end());
      }
      else if(ptr->stype() == shape_type::polygon) {
	polygon *pg = (polygon *) ptr.get();
	parts.insert(parts.end(), pg->rings.begin(), pg->rings.end());
      }

    }
    
    int32_t numpoints = determine_polypart_bb(parts, header_bb);
    int32_t bytes_required = (shpfile.shapes.size() * (POLY_BASE_RECORD_SIZE + sizeof(shapefile_record_header))) +
			      (sizeof(int32_t) * parts.size()) + (2 * sizeof(double) * numpoints);    
    
    return(bytes_required);
  }


  static bool write_polypart_shapes(FILE *fp, FILE *shxfp, const shapefile &shpfile, shputil::shape_type shape_type) {
    
    shapefile_main_header_base header_base;
    shapefile_main_header_boundingbox header_bb;
    memset(&header_base, 0, sizeof(shapefile_main_header_base));
    header_base.file_code = SHAPEFILE_FILE_CODE;
    header_base.version = SHAPEFILE_VERSION;
    header_base.shape_type = (int32_t) shape_type;
    int32_t bytes_required = determine_polypart_bb(shpfile, header_bb);
    header_base.file_length = sizeof(shapefile_main_header_base) + sizeof(shapefile_main_header_boundingbox) + bytes_required;
    header_base.file_length = header_base.file_length / 2; // reported as the # of 16-bit words
    
    if(!write_main_header(fp, header_base, header_bb)) {
      log_error("couldn't write shapefile main header\n");
      return(false);
    }

    //
    // write the shx header
    //
    header_base.file_length = (sizeof(shapefile_main_header_base) + sizeof(shapefile_main_header_boundingbox) + (2 * sizeof(int32_t) * shpfile.shapes.size())) / 2;
    if(!write_main_header(shxfp, header_base, header_bb)) {
      log_error("couldn't write shx header\n");
      return(false);
    }
    
    int32_t recnum = 1;
    int32_t stype = (int32_t) shape_type;
#if BYTE_ORDER == BIG_ENDIAN
    stype = __builtin_bswap32(stype);
#endif
    
    for(auto &ptr : shpfile.shapes) {

      const std::vector<polypart> &parts = (shape_type == shape_type::polyline) ? ((polyline *)ptr.get())->parts : ((polygon *)ptr.get())->rings;
      shapefile_main_header_boundingbox shape_bb; // used here just for bbox computation
      int numpoints = determine_polypart_bb(parts, shape_bb);
      int numparts = parts.size();
    
      int32_t content_bytes = POLY_BASE_RECORD_SIZE + (sizeof(int32_t) * numparts) + (2 * sizeof(double) * numpoints);
      shapefile_record_header rh;
      rh.record_number = recnum++;
      rh.content_length = content_bytes / 2; // # 16-bit words

      shapefile_record_header shx_rh;
      shx_rh.record_number = ftell(fp) / 2; // the offset in 16-bit words to the start of this record
      shx_rh.content_length = rh.content_length;  // shx reports the same content_length

      handle_endianness(shx_rh);
      if(fwrite(&shx_rh, sizeof(shapefile_record_header), 1, shxfp) != 1) {
	log_error("couldn't write polypart shx record\n");
	return(false);
      }

      handle_endianness(rh);
      if(fwrite(&rh, sizeof(shapefile_record_header), 1, fp) != 1) {
	log_error("couldn't write polypart record header\n");
	return(false);
      }

#if BYTE_ORDER == BIG_ENDIAN
      shape_bb.xmin = swap_endianness_dbl(shape_bb.xmin);
      shape_bb.ymin = swap_endianness_dbl(shape_bb.ymin);
      shape_bb.xmax = swap_endianness_dbl(shape_bb.xmax);
      shape_bb.ymax = swap_endianness_dbl(shape_bb.ymax);
      numparts = __builtin_bswap32(numparts);
      numpoints = __builtin_bswap32(numpoints);
#endif

      if(fwrite(&stype, sizeof(int32_t), 1, fp) != 1) {
	log_error("couldn't write polypart stype\n");
	return(false);
      }
      
      if(fwrite(&shape_bb, sizeof(double) * 4, 1, fp) != 1) {
	log_error("couldn't write polypart bb\n");
	return(false);
      }

      if(fwrite(&numparts, sizeof(int32_t), 1, fp) != 1) {
	log_error("couldn't write polypart numparts\n");
	return(false);
      }

      if(fwrite(&numpoints, sizeof(int32_t), 1, fp) != 1) {
	log_error("couldn't write polypart numpoints\n");
	return(false);
      }

      //
      // write the starting point index for each part
      //
      int32_t start_idx = 0;
      for(const polypart &part : parts) {

	int32_t idxtowrite = start_idx;
#if BYTE_ORDER == BIG_ENDIAN
	idxtowrite = __builtin_bswap32(idxtowrite);
#endif

	if(fwrite(&idxtowrite, sizeof(int32_t), 1, fp) != 1) {
	  log_error("couldn't write polypart part idx\n");
	  return(false);
	}
	      
	start_idx += part.points.size();
      }

      //
      // write the points
      //
      for(const polypart &part : parts) {
	for(const pointshape &ps : part.points) {
	  double x = ps.x;
	  double y = ps.y;
#if BYTE_ORDER == BIG_ENDIAN
	  x = swap_endianness_dbl(x);
	  y = swap_endianness_dbl(y);
#endif
	  if((fwrite(&x, sizeof(double), 1, fp) != 1) ||
	     (fwrite(&y, sizeof(double), 1, fp) != 1)) {
	    log_error("couldn't write polyline x,y\n");
	    return(false);
	  }
	}
      }
      
    }

    return(true);
  }


  static bool write_polyline_shapes(FILE *fp, FILE *shxfp, const shapefile &shpfile) {
    log("write_polyline_shapes: %d polyline(s)\n", shpfile.shapes.size());
    return(write_polypart_shapes(fp, shxfp, shpfile, shape_type::polyline));
  }

  
  static bool write_polygon_shapes(FILE *fp, FILE *shxfp, const shapefile &shpfile) {
    log("write_polyline_shapes: %d polygon(s)\n", shpfile.shapes.size());
    return(write_polypart_shapes(fp, shxfp, shpfile, shape_type::polygon));
  }

    
  bool write_shp(const std::string &path, const shapefile &shpfile) {

    std::string shxpath = std::regex_replace(path, std::regex(".shp"), ".shx");
    if(shxpath == path) {
      log_error("file must have .shp extension\n");
      return(false);
    }
        
    FILE *fp = fopen(path.c_str(), "wb");
    if(!fp) {
      log_error("couldn't create shapefile: %s\n", path.c_str());
      return(false);
    }

    FILE *shxfp = fopen(shxpath.c_str(), "wb");
    if(!shxfp) {
      log_error("couldn't create shx index file\n");
      fclose(fp);
      return(false);
    }
    
    shputil::shape_type stype = determine_shape_type(shpfile);

    bool status = false;
    switch(stype) {
    case shape_type::point: status = write_point_shapes(fp, shxfp, shpfile); break;
    case shape_type::polyline: status = write_polyline_shapes(fp, shxfp, shpfile); break;
    case shape_type::polygon: status = write_polygon_shapes(fp, shxfp, shpfile); break;
    case shape_type::multipoint: status = write_multipoint_shapes(fp, shxfp, shpfile); break;
    default:
      log_error("unsupported shape_type: %d\n", stype);
      break;
    }
    
    fclose(fp);
    fclose(shxfp);
    
    return(status);
  }
  
} // namespace shputil


#pragma once

#include <stdint.h>
#include <vector>
#include <string>

namespace dbfutil {

  class dbffield_def {
  public:
    dbffield_def() { field_name = ""; field_type = "C"; field_length = 1;}
    dbffield_def(const std::string &fname, uint8_t flen) { field_name = fname; field_length = flen; field_type = "C";}
    dbffield_def(const std::string &fname, uint8_t flen, uint8_t fdc) { field_name = fname; field_length = flen; field_decimal_count = fdc; field_type = "F"; }
    dbffield_def(const std::string &fname, const std::string &ftype, uint8_t flen) { field_name = fname; field_type = ftype; field_length = flen; field_decimal_count = 0;}
    std::string field_name; 
    std::string field_type; // 'C', 'N', 'F'
    uint8_t field_length;
    uint8_t field_decimal_count;
  };
  
  class dbfheader {
  public:
    std::vector<dbffield_def> fields;
  };
  

  class dbffield_value {
  public:
    enum class vtype  { str, sint, uint, dbl };
    
    dbffield_value() { _vtype = vtype::str; value = ""; }
    dbffield_value(const std::string &s) { _vtype = vtype::str; value = s; }
    dbffield_value(int32_t s32) { _vtype = vtype::sint; _s32_val = s32; value = "";}
    dbffield_value(uint32_t u32) { _vtype = vtype::uint; _u32_val = u32; value = "";}
    dbffield_value(double dbl) { _vtype = vtype::dbl; _dbl_val = dbl; value = "";}

    vtype _vtype;
    int32_t _s32_val;
    uint32_t _u32_val;
    double _dbl_val;
    std::string value;
  };
  
  class dbfrow {
  public:
    std::vector<dbffield_value> values;
  };
  
  class dbftable {
  public:
    dbfheader header;
    std::vector<dbfrow> rows;
  };
  
  bool read_dbf(const std::string &path, dbftable &table);
  bool write_dbf(const std::string &path, const dbftable &table);
  
} // dbfutil namespace

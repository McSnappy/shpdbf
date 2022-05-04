
#include "dbfutil.h"
#include "logging.h"
#include <time.h>
#include <iostream>
#include <cstring>

#ifdef __APPLE__
  #include <machine/endian.h>
#else
  #include <endian.h>
#endif

namespace dbfutil {

  struct dBASE_header {
    uint8_t version;
    uint8_t lastupdate[3];  // yymmdd (yy is year - 1900)
    uint32_t table_records; // LE
    uint16_t header_bytes;  // LE
    uint16_t record_bytes;  // LE
    uint8_t  reserved[20];   
  };
  
  struct dBASE_fielddesc {
    char field_name[11];
    char field_type; 
    uint8_t address[4]; // unused
    uint8_t field_length;
    uint8_t field_decimal_count;
    uint8_t reserved1[2];
    uint8_t workarea_id;
    uint8_t reserved2[2];
    uint8_t setfields_flag;
    uint8_t reserved3[8];
  };
  

  static bool parse_dbl(const char *str, double *val) {
    
    if(!str || !val) {
      return(false);
    }

    char *temp=0;
    bool rc=true;
    errno = 0;
    
    *val = strtod(str, &temp);
    
    if((temp == str) || (*temp != '\0') || (errno == ERANGE)) {
      rc = false;
    }

    return rc;
  }
  
  static bool parse_int32(const char *str, int32_t *val) {
    
    if(!str || !val) {
      return(false);
    }

    char *temp=0;
    bool rc=true;
    errno = 0;
    
    *val = strtol(str, &temp, 0);
    
    if((temp == str) || (*temp != '\0') || (errno == ERANGE)) {
      rc = false;
    }

    return rc;
  }

  static bool parse_uint32(const char *str, uint32_t *val) {

    if(!str || !val) {
      return(false);
    }

    char *temp=0;
    bool rc=true;
    errno = 0;
    
    *val = strtoul(str, &temp, 0);
    
    if((temp == str) || (*temp != '\0') || (errno == ERANGE)) {
      rc = false;
    }

    return rc;
  }
  
  static std::string trim_leading_and_trailing_whitespace(const std::string &str) {
    size_t first = str.find_first_not_of(' ');
    if(first == std::string::npos) {
      return("");
    }
    
    size_t last = str.find_last_not_of(' ');
    return(str.substr(first, (last-first+1)));
  }

  static void handle_endianess(dBASE_header &raw_header) {
    
    #if BYTE_ORDER == BIG_ENDIAN
      raw_header.table_records = __builtin_bswap32(raw_header.table_records);
      raw_header.header_bytes = __builtin_bswap16(raw_header.header_bytes);
      raw_header.record_bytes = __builtin_bswap16(raw_header.record_bytes);
    #endif
      
  }
  
  static bool read_table_rows(dBASE_header raw_header, FILE *fp, dbftable &table) {
    
    uint16_t read_size = raw_header.record_bytes; // includes leading byte with record status
    uint8_t *record_buf = (uint8_t *) malloc(read_size); 
    if(!record_buf) {
      log_error("couldn't allocate record memory...\n");
      return(false);
    }
    
    for(uint32_t ii=0; ii < raw_header.table_records; ++ii) {
      memset(record_buf, 0, read_size);
      if(fread(record_buf, read_size, 1, fp) != 1) {
	free(record_buf);
	return(false);
      }
      
      if(record_buf[0] != 0x20) {
	log_warn("record deleted, skipping...\n");
	continue;
      }
      
      dbfrow row;
      int foffset = 1;
      for(const dbffield_def &fdef : table.header.fields) {
	
	if(foffset >= read_size) {
	  log_error("read past record buffer...\n");
	  free(record_buf);
	  return(false);
	}
	
	char vbuf[512];
	memcpy(vbuf, record_buf + foffset, fdef.field_length);
	vbuf[fdef.field_length] = 0;
	std::string str;
	str = trim_leading_and_trailing_whitespace(std::string(vbuf));
	dbffield_value fval(str);
	if(fdef.field_type == "N") {
	  bool parsed = false;
	  if(str.find("-") != std::string::npos) {
	    int32_t sval = 0;
	    parsed = parse_int32(str.c_str(), &sval);
	    fval = dbffield_value(sval);
	  }
	  else {
	    uint32_t uval = 0;
	    parsed = parse_uint32(str.c_str(), &uval);
	    fval = dbffield_value(uval);
	  }

	  fval.value = str;
	  
	  if(!parsed) {
	    log_error("couldn't parse numeric value for column: %s\n", fdef.field_name.c_str());
	    return(false);
	  }
	}
	else if(fdef.field_type == "F") {
	  double dbl = 0.0;
	  bool parsed = parse_dbl(str.c_str(), &dbl);
	  if(!parsed) {
	    log_error("couldn't parse double value for column: %s\n", fdef.field_name.c_str());
	    return(false);
	  }
	  fval = dbffield_value(dbl);
	  fval.value = str;
	}
	
	row.values.push_back(fval);
	foffset += fdef.field_length;
      }
      
      table.rows.push_back(row);
    }
    
    free(record_buf);
    return(true);
  }
  
  
  static bool read_field_descriptors(dBASE_header &raw_header, FILE *fp, dbftable &table) {
    
    int number_fields = (raw_header.header_bytes - sizeof(dBASE_header) - 1) / sizeof(dBASE_fielddesc); // -1 for the terminator
    //log("number_fields: %d\n", number_fields);
    
    for(int ii=0; ii < number_fields; ++ii) {
      dBASE_fielddesc fdesc;
      memset(&fdesc, 0, sizeof(dBASE_fielddesc));
      if(fread(&fdesc, sizeof(dBASE_fielddesc), 1, fp) != 1) {
	log_error("trouble while reading field descriptors...\n");
	return(false);
      }
      
      //log("field_name: %.11s\n", fdesc.field_name);
      //log("field_type: %c\n", fdesc.field_type);
      //log("field_length: %d\n", fdesc.field_length);
      //log("field_decimal_count: %d\n", fdesc.field_decimal_count);
      //log("\n");
      
      char fname[20];
      memset(fname, 0, 20);
      memcpy(fname, fdesc.field_name, 11);
      
      char ftype[2];
      ftype[0] = fdesc.field_type;
      ftype[1] = 0;

      if((fdesc.field_type == 'N') && (fdesc.field_decimal_count > 0)) {
	ftype[0] = 'F';  // let's always call a float a float
      }
      
      dbffield_def fdef;
      fdef.field_name = trim_leading_and_trailing_whitespace(std::string(fname));
      fdef.field_type = std::string(ftype);
      fdef.field_length = fdesc.field_length;
      fdef.field_decimal_count = fdesc.field_decimal_count;
      table.header.fields.push_back(fdef);
    }
    
    uint8_t terminator = 0;
    if((fread(&terminator, 1, 1, fp) != 1) || (terminator != 0x0d)) {
      log_error("didn't find field desc terminator\n");
      return(false);
    }
    
    
    return(true);
  }
  
  
  bool read_dbf(const std::string &path, dbftable &table) {
    
    //log("reading dbf table: %s\n", path.c_str());
    
    table.header.fields.clear();
    table.rows.clear();
    
    FILE *fp = fopen(path.c_str(), "rb");
    if(!fp) {
      log_error("couldn't open dbf for reading: %s\n", path.c_str());
      return(false);
    }
    
    dBASE_header raw_header;
    memset(&raw_header, 0, sizeof(dBASE_header));
    if(fread(&raw_header, sizeof(dBASE_header), 1, fp) != 1) {
      log_error("couldn't read dbf header...\n");
      return(false);
    }

    handle_endianess(raw_header);
  
    //log("version: %d\n", raw_header.version);
    //log("lastupdate: %d/%d/%d\n", 1900 + raw_header.lastupdate[0],
    //	raw_header.lastupdate[1],raw_header.lastupdate[2]);
    //log("table_records: %u\n", raw_header.table_records);
    //log("header_bytes: %u\n", raw_header.header_bytes);
    //log("record_bytes: %u\n", raw_header.record_bytes);
      
    if(!read_field_descriptors(raw_header, fp, table)) {
      log_error("trouble reading field descriptors...\n");
      fclose(fp);
      return(false);
    }
  
    if(!read_table_rows(raw_header, fp, table)) {
      log_error("trouble reading table rows...\n");
      fclose(fp);
      return(false);
    }
  
    fclose(fp);
  
    return(true);
  }


  bool write_field_descriptors(FILE *fp, const dbftable &table) {

    for(const dbffield_def &fielddef : table.header.fields) {

      if(fielddef.field_name == "") {
	log_error("field definition is missing its field name\n");
	return(false);
      }

      if(fielddef.field_length == 0) {
	log_error("field definition has zero length...\n");
	return(false);
      }

      if((fielddef.field_type != "C") &&
	 (fielddef.field_type != "N") &&
	 (fielddef.field_type != "F")) {
	log_error("field definition has bogus field type...\n");
	return(false);
      }
      
      dBASE_fielddesc fdesc;
      memset(&fdesc, 0, sizeof(dBASE_fielddesc));
      char fname[12];
      sprintf(fname, "%.11s", fielddef.field_name.c_str());
      memcpy(fdesc.field_name, fname, 11);
      fdesc.field_type = fielddef.field_type.c_str()[0];
      fdesc.field_length = fielddef.field_length;
      fdesc.field_decimal_count = fielddef.field_decimal_count;
      if(fwrite(&fdesc, sizeof(dBASE_fielddesc), 1, fp) != 1) {
	log_error("couldn't write field descriptor\n");
	return(false);
      }
    }

    uint8_t terminator = 0x0d;
    if(fwrite(&terminator, 1, 1, fp) != 1) {
      log_error("couldn't write field desc terminator\n");
      return(false);
    }
    
    return(true);
  }

  
  bool write_table_rows(FILE *fp, uint16_t record_bytes, const dbftable &table) {

    if(record_bytes == 0) {
      return(false);
    }
    
    uint8_t *record_buf = (uint8_t *) malloc(record_bytes); 
    if(!record_buf) {
      log_error("couldn't allocate record memory...\n");
      return(false);
    }
    
    for(const dbfrow &row : table.rows) {
      memset(record_buf, 0, record_bytes);
      record_buf[0] = ' '; // active status
      uint16_t recoff = 1;
      uint32_t ridx = 0;

      if(row.values.size() != table.header.fields.size()) {
	log_error("row length / header length mismatch\n");
	return(false);
      }
      
      for(const dbffield_def &fielddef : table.header.fields) {

	char buf[512];
	memset(buf, 0x20, 512);

	const dbffield_value &val = row.values[ridx];
	
	if(fielddef.field_type == "C") {
	  if(val._vtype != dbffield_value::vtype::str) {
	    log_error("field value type mismatch at column %s (expected str)\n", fielddef.field_name.c_str());
	    return(false);
	  }
	  sprintf(buf, "%*s", fielddef.field_length, val.value.c_str());
	}
	else if(fielddef.field_type == "N") {
	  char temp[512];
	  if(val._vtype == dbffield_value::vtype::sint) {
	    sprintf(temp, "%d", val._s32_val);
	  }
	  else if(val._vtype == dbffield_value::vtype::uint) {
	    sprintf(temp, "%u", val._u32_val);
	  }
	  else {
	    log_error("field value type mismatch at column %s (expected sint/uint)\n", fielddef.field_name.c_str());
	    return(false);
	  }

	  sprintf(buf, "%*s", fielddef.field_length, temp);
	}
	else if(fielddef.field_type == "F") {
	  if(val._vtype == dbffield_value::vtype::dbl) {
	    char temp[512];
	    sprintf(temp, "%.*e", fielddef.field_decimal_count, val._dbl_val);
	    sprintf(buf, "%*s", fielddef.field_length, temp);
	  }
	  else {
	    log_error("field value type mismatch at column %s (expected dbl)\n", fielddef.field_name.c_str());
	    return(false);
	  }
	}
	
	memcpy(record_buf + recoff, buf, fielddef.field_length);
	ridx += 1;
	recoff += fielddef.field_length;
      }

      if(fwrite(record_buf, record_bytes, 1, fp) != 1) {
	log_error("failed to write record...\n");
	free(record_buf);
	return(false);
      }
    }

    free(record_buf);

    uint8_t terminator = 0x1a;
    if(fwrite(&terminator, 1, 1, fp) != 1) {
      log_error("couldn't write file terminator\n");
      return(false);
    }
    
    return(true);
  }

  
  bool write_dbf(const std::string &path, const dbftable &table) {
    
    if(table.header.fields.empty()) {
      log_error("can't write a table that doesn't have columns...\n");
      return(false);
    }
    
    if(table.rows.empty()) {
      log_error("can't write a table that doesn't have rows...\n");
      return(false);
    }

    FILE *fp = fopen(path.c_str(), "wb");
    if(!fp) {
      log_error("can't open dbf for writing: %s\n", path.c_str());
      return(false);
    }

    dBASE_header raw_header;
    memset(&raw_header, 0, sizeof(dBASE_header));  

    raw_header.version = 0x03;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    raw_header.lastupdate[0] = tm ? tm->tm_year : (2022 - 1900); 
    raw_header.lastupdate[1] = tm ? (tm->tm_mon + 1) : 1;
    raw_header.lastupdate[2] = tm ? tm->tm_mday : 1;

    raw_header.table_records = table.rows.size();
    raw_header.header_bytes = 1 + sizeof(dBASE_header) + (sizeof(dBASE_fielddesc) * table.header.fields.size()); // +1 for the terminator
    raw_header.record_bytes = 1; // first byte is the record status
    for(const dbffield_def &fielddef : table.header.fields) {
      raw_header.record_bytes += fielddef.field_length;
    }

    uint16_t record_bytes = raw_header.record_bytes;
    handle_endianess(raw_header);

    bool status = true;
    if((fwrite(&raw_header, sizeof(dBASE_header), 1, fp) != 1) ||
       !write_field_descriptors(fp, table) ||
       !write_table_rows(fp, record_bytes, table)) {
      log_error("error while writing the dbf...\n");
      status = false;
    }

    fclose(fp);
    return(status);
  }

} // namespace dbfutil

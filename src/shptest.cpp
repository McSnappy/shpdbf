
#include <iostream>
#include "dbfutil.h"
#include "shputil.h"

void append_city(const std::string &city, const std::string &country,
		 double longitude, double latitude,
		 shputil::shapefile &shp, dbfutil::dbftable &dbf);


int main(int argc, char **argv) {

  shputil::shapefile world_cities_shp;
  dbfutil::dbftable world_cities_dbf;
  world_cities_dbf.header.fields.push_back(dbfutil::dbffield_def("City", "C", 50));
  world_cities_dbf.header.fields.push_back(dbfutil::dbffield_def("Country", "C", 50));
  world_cities_dbf.header.fields.push_back(dbfutil::dbffield_def("Longitude", 19, 11));
  world_cities_dbf.header.fields.push_back(dbfutil::dbffield_def("Latitude", 19, 11));

  append_city("New York", "USA", -74.006, 40.7128, world_cities_shp, world_cities_dbf);
  append_city("London", "England", -0.1276, 51.5072, world_cities_shp, world_cities_dbf);
  append_city("Tokyo", "Japan", 139.6503, 35.6762, world_cities_shp, world_cities_dbf);
  append_city("Sydney", "Australia", 151.2093, -33.8688, world_cities_shp, world_cities_dbf);
  append_city("Rio de Janeiro", "Brazil", -43.1729, -22.9068, world_cities_shp, world_cities_dbf);
  append_city("Cairo", "Egypt", 31.2357, 30.0444, world_cities_shp, world_cities_dbf);
  append_city("Honolulu", "USA", -157.8583, 21.3069, world_cities_shp, world_cities_dbf);
  
  if(!write_dbf("./world-cities.dbf", world_cities_dbf)) {
    std::cout << "write_dbf failed..." << std::endl;
    exit(1);
  }

  if(!write_shp("./world-cities.shp", world_cities_shp)) {
    std::cout << "write_shp failed..." << std::endl;
    exit(1);
  }
  
  
  return(0);
}


void append_city(const std::string &city, const std::string &country,
		 double longitude, double latitude,
		 shputil::shapefile &shp, dbfutil::dbftable &dbf) {

  dbfutil::dbfrow arow;
  arow.values.push_back(dbfutil::dbffield_value(city));
  arow.values.push_back(dbfutil::dbffield_value(country));
  arow.values.push_back(dbfutil::dbffield_value(longitude));
  arow.values.push_back(dbfutil::dbffield_value(latitude));
  dbf.rows.push_back(arow);

  shp.shapes.push_back(std::make_shared<shputil::pointshape>(longitude, latitude));
}

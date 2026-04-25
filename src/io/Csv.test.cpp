/**
 * @author Alexander Breuer (alex.breuer AT uni-jena.de)
 *
 * @section DESCRIPTION
 * Unit tests for the CSV-interface.
 **/
#include "../constants.h"
#include <catch2/catch.hpp>
#include <sstream>
#define private public
#include "Csv.h"
#undef public

TEST_CASE("Test the CSV-reader for bathymetry data.", "[CsvRead]") {
  std::stringstream l_stream;
  l_stream << "longitude,latitude,distance,bathymetry\n";
  l_stream << "141.024949,37.316569,0,14.6328872651\n";
  l_stream << "141.02756,37.316624,0.231656660144,-6.9276614277\n";
  l_stream << "# this is a comment\n";
  l_stream << "141.03018,37.316678,0.463313319336,-6.77041524482\n";

  tsunami_lab::t_idx l_nRows = 0;
  tsunami_lab::t_real* l_x = nullptr;
  tsunami_lab::t_real* l_b = nullptr;

  tsunami_lab::io::Csv::read(l_stream, l_nRows, l_x, l_b);

  REQUIRE(l_nRows == 3);

  // distance in km * 1000 = metres
  REQUIRE(l_x[0] == Approx(0.0f));
  REQUIRE(l_x[1] == Approx(231.656660144f));
  REQUIRE(l_x[2] == Approx(463.313319336f));

  // bathymetry values
  REQUIRE(l_b[0] == Approx(14.6328872651f));
  REQUIRE(l_b[1] == Approx(-6.9276614277f));
  REQUIRE(l_b[2] == Approx(-6.77041524482f));

  delete[] l_x;
  delete[] l_b;
}

TEST_CASE("Test the CSV-writer for 1D settings.", "[CsvWrite1d]") {
  // define a simple example
  tsunami_lab::t_real l_h[7] = {0, 1, 2, 3, 4, 5, 6};
  tsunami_lab::t_real l_hu[7] = {6, 5, 4, 3, 2, 1, 0};
  tsunami_lab::t_real l_b[7] = {0, 1, 2, 3, 4, 5, 6};

  std::stringstream l_stream0;
  tsunami_lab::io::Csv::write(0.5, 5, 1, 7, l_h + 1, l_b, l_hu + 1, nullptr,
                              1.5, l_stream0);

  std::string l_ref0 = R"V0G0N(# sim_time=1.5
x,y,height,bathymetry,momentum_x
0.25,0.25,1,0,5
0.75,0.25,2,1,4
1.25,0.25,3,2,3
1.75,0.25,4,3,2
2.25,0.25,5,4,1
)V0G0N";

  REQUIRE(l_stream0.str().size() == l_ref0.size());
  REQUIRE(l_stream0.str() == l_ref0);
}

TEST_CASE("Test the CSV-writer for 2D settings.", "[CsvWrite2d]") {
  // define a simple example
  tsunami_lab::t_real l_h[16] = {0, 1, 2,  3,  4,  5,  6,  7,
                                 8, 9, 10, 11, 12, 13, 14, 15};
  tsunami_lab::t_real l_hu[16] = {15, 14, 13, 12, 11, 10, 9, 8,
                                  7,  6,  5,  4,  3,  2,  1, 0};
  tsunami_lab::t_real l_hv[16] = {0, 4, 8,  12, 1, 5, 9,  13,
                                  2, 6, 10, 14, 3, 7, 11, 15};
  tsunami_lab::t_real l_b[16] = {0, 4, 8,  12, 1, 5, 9,  13,
                                 2, 6, 10, 14, 3, 7, 11, 15};

  std::stringstream l_stream1;
  tsunami_lab::io::Csv::write(10, 2, 2, 4, l_h + 4 + 1, l_b, l_hu + 4 + 1,
                              l_hv + 4 + 1, 42.0, l_stream1);

  std::string l_ref1 = R"V0G0N(# sim_time=42
x,y,height,bathymetry,momentum_x,momentum_y
5,5,5,0,10,5
15,5,6,4,9,9
5,15,9,1,6,6
15,15,10,5,5,10
)V0G0N";

  REQUIRE(l_stream1.str().size() == l_ref1.size());
  REQUIRE(l_stream1.str() == l_ref1);
}

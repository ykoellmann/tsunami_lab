/**
 * @author Yannik Köllmann (yannik.koellmann AT uni-jena.de)
 * @author Jan Vogt (jan.vogt AT uni-jena.de)
 * @author Mika Brückner (mika.brueckner AT uni-jena.de)
 *
 * @section DESCRIPTION
 * Unit tests for the TsunamiEvent1d setup.
 **/
#include "../../constants.h"
#include <catch2/catch.hpp>
#include <fstream>
#include <sstream>

#define private public
#include "TsunamiEvent1d.h"
#undef public

TEST_CASE("Test TsunamiEvent1d setup.", "[TsunamiEvent1d]") {
  // create a small test CSV file
  std::ofstream l_file("/tmp/test_bathymetry.csv");
  l_file << "longitude,latitude,distance,bathymetry\n";
  l_file << "0,0,0,-100\n";
  l_file << "0,0,0.25,-200\n";
  l_file << "0,0,0.5,-50\n";
  l_file << "0,0,0.75,30\n";
  l_file.close();

  tsunami_lab::setups::TsunamiEvent1d l_setup("/tmp/test_bathymetry.csv", 20);

  // check sample count
  REQUIRE(l_setup.m_nSamples == 4);

  // wet cell (b_in = -100 < 0): h = max(-b_in, delta) = max(100, 20) = 100
  REQUIRE(l_setup.getHeight(0, 0) == Approx(100.0f));

  // cell with positive b_in (b_in = 30): h = max(-30, 20) = 20
  REQUIRE(l_setup.getHeight(750, 0) == Approx(20.0f));

  // momentum is always 0
  REQUIRE(l_setup.getMomentumX(0, 0) == Approx(0.0f));
  REQUIRE(l_setup.getMomentumY(0, 0) == Approx(0.0f));

  // wet cell bathymetry (b_in = -100): min(b_in, -delta) + d = min(-100, -20) +
  // 0 = -100 (x=0 is outside displacement range)
  REQUIRE(l_setup.getBathymetry(0, 0) == Approx(-100.0f));

  // cell with positive b_in (b_in = 30): min(30, -20) + 0 = -20
  REQUIRE(l_setup.getBathymetry(750, 0) == Approx(-20.0f));

  // interpolation: x=125 is midpoint between sample 0 (x=0, b=-100) and sample
  // 1 (x=250, b=-200) expected b_in = -150
  REQUIRE(l_setup.getBathymetryRaw(125) == Approx(-150.0f));
}

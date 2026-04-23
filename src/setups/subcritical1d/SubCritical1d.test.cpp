/**
 * @author Yannik Köllmann (yannik.koellmann AT uni-jena.de)
 * @author Jan Vogt (jan.vogt AT uni-jena.de)
 * @author Mika Brückner (mika.brueckner AT uni-jena.de)
 *
 * @section DESCRIPTION
 * Tests the one dimensional subcritical setup.
 **/
#include "SubCritical1d.h"
#include <catch2/catch.hpp>

TEST_CASE("One-dimensional subcritical setup", "[SubCritical1d]") {
  tsunami_lab::setups::Subcritical1d l_setup;

  SECTION("height equals negative bathymetry in domain [0, 25]") {
    // outside bump: b = -2, so h = 2
    REQUIRE(l_setup.getHeight(0, 0) == Approx(2));
    REQUIRE(l_setup.getHeight(25, 0) == Approx(2));
    REQUIRE(l_setup.getHeight(5, 0) == Approx(2));
  }

  SECTION("height is reduced over the bump [8, 12]") {
    // at x=10: b = -1.8 - 0.05*(10-10)^2 = -1.8, so h = 1.8
    REQUIRE(l_setup.getHeight(10, 0) == Approx(1.8));
    // at x=8: b = -1.8 - 0.05*(8-10)^2 = -1.8 - 0.2 = -2.0, so h = 2.0
    REQUIRE(l_setup.getHeight(8, 0) == Approx(2.0));
    // at x=9: b = -1.8 - 0.05*(9-10)^2 = -1.8 - 0.05 = -1.85, so h = 1.85
    REQUIRE(l_setup.getHeight(9, 0) == Approx(1.85));
  }

  SECTION("height is zero outside domain") {
    REQUIRE(l_setup.getHeight(-1, 0) == Approx(0));
    REQUIRE(l_setup.getHeight(26, 0) == Approx(0));
  }

  SECTION("momentum in x is constant 4.42 in domain") {
    REQUIRE(l_setup.getMomentumX(0, 0) == Approx(4.42));
    REQUIRE(l_setup.getMomentumX(10, 0) == Approx(4.42));
    REQUIRE(l_setup.getMomentumX(25, 0) == Approx(4.42));
  }

  SECTION("momentum in x is zero outside domain") {
    REQUIRE(l_setup.getMomentumX(-1, 0) == Approx(0));
    REQUIRE(l_setup.getMomentumX(26, 0) == Approx(0));
  }

  SECTION("y-momentum is always zero") {
    REQUIRE(l_setup.getMomentumY(0, 0) == Approx(0));
    REQUIRE(l_setup.getMomentumY(10, 0) == Approx(0));
    REQUIRE(l_setup.getMomentumY(25, 0) == Approx(0));
  }

  SECTION("bathymetry is -2 outside bump") {
    REQUIRE(l_setup.getBathymetry(0, 0) == Approx(-2));
    REQUIRE(l_setup.getBathymetry(5, 0) == Approx(-2));
    REQUIRE(l_setup.getBathymetry(20, 0) == Approx(-2));
  }

  SECTION("bathymetry follows parabola over bump [8, 12]") {
    REQUIRE(l_setup.getBathymetry(10, 0) == Approx(-1.8));
    REQUIRE(l_setup.getBathymetry(9, 0) == Approx(-1.85));
    REQUIRE(l_setup.getBathymetry(11, 0) == Approx(-1.85));
  }

  SECTION("setup is time-independent") {
    REQUIRE(l_setup.getHeight(10, 5) == l_setup.getHeight(10, 0));
    REQUIRE(l_setup.getMomentumX(5, 99) == l_setup.getMomentumX(5, 0));
    REQUIRE(l_setup.getBathymetry(10, 42) == l_setup.getBathymetry(10, 0));
  }
}

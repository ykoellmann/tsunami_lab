/**
 * @author Yannik Köllmann (yannik.koellmann AT uni-jena.de)
 * @author Jan Vogt (jan.vogt AT uni-jena.de)
 * @author Mika Brückner (mika.brueckner AT uni-jena.de)
 *
 * @section DESCRIPTION
 * Tests the one dimensional supercritical setup.
 **/
#include "SuperCritical1d.h"
#include <catch2/catch.hpp>

TEST_CASE("One-dimensional supercritical setup", "[SuperCritical1d]") {
  tsunami_lab::setups::SuperCritical1d l_setup;

  SECTION("height equals negative bathymetry in domain [0, 25]") {
    // outside bump: b = -0.33, so h = 0.33
    REQUIRE(l_setup.getHeight(0, 0) == Approx(0.33));
    REQUIRE(l_setup.getHeight(25, 0) == Approx(0.33));
    REQUIRE(l_setup.getHeight(5, 0) == Approx(0.33));
  }

  SECTION("height is reduced over the bump [8, 12]") {
    // at x=10: b = -0.13 - 0.05*(10-10)^2 = -0.13, so h = 0.13
    REQUIRE(l_setup.getHeight(10, 0) == Approx(0.13));
    // at x=9:  b = -0.13 - 0.05*(9-10)^2  = -0.18, so h = 0.18
    REQUIRE(l_setup.getHeight(9, 0) == Approx(0.18));
    // at x=11: b = -0.13 - 0.05*(11-10)^2 = -0.18, so h = 0.18
    REQUIRE(l_setup.getHeight(11, 0) == Approx(0.18));
  }

  SECTION("height is zero outside domain") {
    REQUIRE(l_setup.getHeight(-1, 0) == Approx(0));
    REQUIRE(l_setup.getHeight(26, 0) == Approx(0));
  }

  SECTION("momentum in x is constant 0.18 in domain") {
    REQUIRE(l_setup.getMomentumX(0, 0) == Approx(0.18));
    REQUIRE(l_setup.getMomentumX(10, 0) == Approx(0.18));
    REQUIRE(l_setup.getMomentumX(25, 0) == Approx(0.18));
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

  SECTION("bathymetry is -0.33 outside bump") {
    REQUIRE(l_setup.getBathymetry(0, 0) == Approx(-0.33));
    REQUIRE(l_setup.getBathymetry(5, 0) == Approx(-0.33));
    REQUIRE(l_setup.getBathymetry(20, 0) == Approx(-0.33));
  }

  SECTION("bathymetry follows parabola over bump [8, 12]") {
    REQUIRE(l_setup.getBathymetry(10, 0) == Approx(-0.13));
    REQUIRE(l_setup.getBathymetry(9, 0) == Approx(-0.18));
    REQUIRE(l_setup.getBathymetry(11, 0) == Approx(-0.18));
  }

  SECTION("setup is time-independent") {
    REQUIRE(l_setup.getHeight(10, 5) == l_setup.getHeight(10, 0));
    REQUIRE(l_setup.getMomentumX(5, 99) == l_setup.getMomentumX(5, 0));
    REQUIRE(l_setup.getBathymetry(10, 42) == l_setup.getBathymetry(10, 0));
  }
}

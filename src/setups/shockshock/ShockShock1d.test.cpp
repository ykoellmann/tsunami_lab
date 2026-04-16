/**
* @author Yannik Köllmann (yannik.koellmann AT uni-jena.de)
 * @author Jan Vogt (jan.vogt AT uni-jena.de)
 * @author Mika Brückner (mika.brueckner AT uni-jena.de)
 *
 * @section DESCRIPTION
 * Tests the shock-shock setup.
 **/
#include "ShockShock1d.h"
#include <catch2/catch.hpp>

TEST_CASE("One-dimensional shock-shock setup", "[ShockShock1d]") {
  tsunami_lab::setups::ShockShock1d l_setup(25, 7, 3);

  SECTION("height is constant on both sides") {
    REQUIRE(l_setup.getHeight(2, 0) == Approx(25));
    REQUIRE(l_setup.getHeight(4, 0) == Approx(25));
    REQUIRE(l_setup.getHeight(2, 0) == l_setup.getHeight(4, 0));
  }

  SECTION("left side flows right (positive momentum)") {
    REQUIRE(l_setup.getMomentumX(2, 0) == Approx(7));
  }

  SECTION("right side flows left (negative momentum)") {
    REQUIRE(l_setup.getMomentumX(4, 0) == Approx(-7));
  }

  SECTION("momentum is antisymmetric across the discontinuity") {
    REQUIRE(l_setup.getMomentumX(2, 0) == Approx(-l_setup.getMomentumX(4, 0)));
  }

  SECTION("momentum at the discontinuity belongs to the left side") {
    // spec says x <= x_dis → left state
    REQUIRE(l_setup.getMomentumX(3, 0) == Approx(7));
  }

  SECTION("y-momentum is always zero in the 1d setup") {
    REQUIRE(l_setup.getMomentumY(2, 0) == Approx(0));
    REQUIRE(l_setup.getMomentumY(4, 0) == Approx(0));
  }

  SECTION("setup is time-independent") {
    REQUIRE(l_setup.getHeight(2, 5)     == l_setup.getHeight(2, 0));
    REQUIRE(l_setup.getMomentumX(4, 99) == l_setup.getMomentumX(4, 0));
  }
}
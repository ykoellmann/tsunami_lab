/**
 * @author Yannik Köllmann (yannik.koellmann AT uni-jena.de)
 * @author Jan Vogt (jan.vogt AT uni-jena.de)
 * @author Mika Brückner (mika.brueckner AT uni-jena.de)
 *
 * @section DESCRIPTION
 * Tests the two-dimensional circular dam break setup.
 **/
#include "CircularDamBreak2d.h"
#include <catch2/catch.hpp>

TEST_CASE("Test the two-dimensional circular dam break setup.",
          "[CircularDamBreak2d]") {
  // values from the lab description: domain [-50, 50]^2, circle of radius 10
  // around the origin, h = 10 inside, h = 5 outside, zero momentum
  tsunami_lab::setups::CircularDamBreak2d l_dam(10, 5, 0, 0, 10);

  // inside the circle
  REQUIRE(l_dam.getHeight(0, 0) == 10);
  REQUIRE(l_dam.getHeight(5, 5) == 10);
  REQUIRE(l_dam.getHeight(-5, -5) == 10);

  // outside the circle
  REQUIRE(l_dam.getHeight(10, 0) == 5);
  REQUIRE(l_dam.getHeight(0, 10) == 5);
  REQUIRE(l_dam.getHeight(50, 50) == 5);
  REQUIRE(l_dam.getHeight(-50, -50) == 5);

  // momentum is zero everywhere
  REQUIRE(l_dam.getMomentumX(0, 0) == 0);
  REQUIRE(l_dam.getMomentumY(0, 0) == 0);
  REQUIRE(l_dam.getMomentumX(40, 30) == 0);
  REQUIRE(l_dam.getMomentumY(40, 30) == 0);

  // bathymetry is flat
  REQUIRE(l_dam.getBathymetry(0, 0) == 0);
  REQUIRE(l_dam.getBathymetry(40, 30) == 0);
}

TEST_CASE("Test the circular dam break setup with an off-origin center.",
          "[CircularDamBreak2d]") {
  // circle of radius 3 around (5, 5)
  tsunami_lab::setups::CircularDamBreak2d l_dam(8, 2, 5, 5, 3);

  REQUIRE(l_dam.getHeight(5, 5) == 8); // center
  REQUIRE(l_dam.getHeight(7, 5) == 8); // inside (distance 2)
  REQUIRE(l_dam.getHeight(8, 5) == 2); // on boundary -> outer (strict <)
  REQUIRE(l_dam.getHeight(0, 0) == 2); // far outside
}

TEST_CASE("Test the circular dam break setup with a Gaussian obstacle.",
          "[CircularDamBreak2d]") {
  // dam break in [-50, 50]^2 with a Gaussian obstacle of amplitude 3 at
  // (20, 0), width = 5
  tsunami_lab::setups::CircularDamBreak2d l_dam(10, 5, 0, 0, 10, 3, 20, 0, 5);

  // peak of the obstacle equals the amplitude
  REQUIRE(l_dam.getBathymetry(20, 0) == Approx(3));

  // far away from the obstacle, the bathymetry decays to ~0
  REQUIRE(l_dam.getBathymetry(-50, -50) == Approx(0).margin(1e-6));

  // bathymetry decreases monotonically away from the peak along x
  REQUIRE(l_dam.getBathymetry(20, 0) > l_dam.getBathymetry(22, 0));
  REQUIRE(l_dam.getBathymetry(22, 0) > l_dam.getBathymetry(25, 0));

  // isotropy: same distance from the center yields the same value
  REQUIRE(l_dam.getBathymetry(25, 0) == Approx(l_dam.getBathymetry(20, 5)));
  REQUIRE(l_dam.getBathymetry(25, 0) == Approx(l_dam.getBathymetry(15, 0)));

  // adding the obstacle does not change the dam break itself
  REQUIRE(l_dam.getHeight(0, 0) == 10);
  REQUIRE(l_dam.getHeight(20, 0) == 5);
  REQUIRE(l_dam.getMomentumX(20, 0) == 0);
  REQUIRE(l_dam.getMomentumY(20, 0) == 0);
}

TEST_CASE("Circular dam break: bathymetry is flat when obstacle is disabled.",
          "[CircularDamBreak2d]") {
  // amplitude == 0 -> no obstacle
  tsunami_lab::setups::CircularDamBreak2d l_flatA(10, 5, 0, 0, 10, 0, 0, 0, 5);
  REQUIRE(l_flatA.getBathymetry(0, 0) == 0);
  REQUIRE(l_flatA.getBathymetry(20, 20) == 0);

  // width == 0 -> no obstacle (avoids division by zero in the Gaussian)
  tsunami_lab::setups::CircularDamBreak2d l_flatW(10, 5, 0, 0, 10, 3, 0, 0, 0);
  REQUIRE(l_flatW.getBathymetry(0, 0) == 0);
  REQUIRE(l_flatW.getBathymetry(20, 20) == 0);
}

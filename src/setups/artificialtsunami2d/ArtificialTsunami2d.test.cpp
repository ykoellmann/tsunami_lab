/**
 * @author Yannik Köllmann
 * @author Jan Vogt
 * @author Mika Brückner
 * @section DESCRIPTION
 * Unit tests for the 2D artificial tsunami setup.
 **/
#include "../../constants.h"
#include <catch2/catch.hpp>
#include <cmath>
#define private public
#include "ArtificialTsunami2d.h"
#undef private

TEST_CASE("ArtificialTsunami2d: water height (Eq. 5.2.1)", "[ArtTsunami2d]") {
  tsunami_lab::setups::ArtificialTsunami2d l_setup;

  // b_in = -100 < 0 everywhere → h = max(-b_in, delta) = max(100, 20) = 100
  REQUIRE(l_setup.getHeight(0.0f,     0.0f)     == Approx(100.0f));
  REQUIRE(l_setup.getHeight(50000.0f, 0.0f)     == Approx(100.0f));
  REQUIRE(l_setup.getHeight(0.0f,     50000.0f) == Approx(100.0f));
  REQUIRE(l_setup.getHeight(1e6f,     1e6f)     == Approx(100.0f));
}

TEST_CASE("ArtificialTsunami2d: momenta are zero", "[ArtTsunami2d]") {
  tsunami_lab::setups::ArtificialTsunami2d l_setup;

  REQUIRE(l_setup.getMomentumX(0.0f, 0.0f) == Approx(0.0f));
  REQUIRE(l_setup.getMomentumY(0.0f, 0.0f) == Approx(0.0f));
  REQUIRE(l_setup.getMomentumX(1e5f, 1e5f) == Approx(0.0f));
  REQUIRE(l_setup.getMomentumY(1e5f, 1e5f) == Approx(0.0f));
}

TEST_CASE("ArtificialTsunami2d: bathymetry (Eq. 5.2.1)", "[ArtTsunami2d]") {
  tsunami_lab::setups::ArtificialTsunami2d l_setup;

  // at origin: d = 5 * exp(0) = 5  → b = min(-100, -20) + 5 = -100 + 5 = -95
  REQUIRE(l_setup.getBathymetry(0.0f, 0.0f) == Approx(-95.0f));

  // at (25000, 0): d = 5 * exp(-0.5) ≈ 3.033
  const float l_sigma = 25000.0f;
  float l_dExpected = 5.0f * std::exp(-0.5f);
  REQUIRE(l_setup.getBathymetry(l_sigma, 0.0f) ==
          Approx(-100.0f + l_dExpected).epsilon(1e-5f));

  // at (25000, 25000): d = 5 * exp(-1) ≈ 1.839
  float l_dExpected2 = 5.0f * std::exp(-1.0f);
  REQUIRE(l_setup.getBathymetry(l_sigma, l_sigma) ==
          Approx(-100.0f + l_dExpected2).epsilon(1e-5f));

  // far from origin: displacement ≈ 0 → b ≈ -100
  REQUIRE(l_setup.getBathymetry(1e8f, 1e8f) == Approx(-100.0f).epsilon(1e-4f));
}

TEST_CASE("ArtificialTsunami2d: displacement helper", "[ArtTsunami2d]") {
  tsunami_lab::setups::ArtificialTsunami2d l_setup;

  // peak at origin
  REQUIRE(l_setup.getDisplacement(0.0f, 0.0f) == Approx(5.0f));

  // symmetry: d(x,0) == d(0,x) == d(x/sqrt(2), x/sqrt(2))
  REQUIRE(l_setup.getDisplacement(25000.0f, 0.0f) ==
          Approx(l_setup.getDisplacement(0.0f, 25000.0f)));

  // positive everywhere (Gaussian)
  REQUIRE(l_setup.getDisplacement(1e5f, 1e5f) > 0.0f);
}

TEST_CASE("ArtificialTsunami2d: custom delta", "[ArtTsunami2d]") {
  // with a larger delta the height formula still applies: max(-(-100), delta)
  tsunami_lab::setups::ArtificialTsunami2d l_setup(50.0f);
  REQUIRE(l_setup.getHeight(0.0f, 0.0f) == Approx(100.0f));
  // bathymetry: min(-100, -50) + d = -100 + 5 = -95
  REQUIRE(l_setup.getBathymetry(0.0f, 0.0f) == Approx(-95.0f));
}

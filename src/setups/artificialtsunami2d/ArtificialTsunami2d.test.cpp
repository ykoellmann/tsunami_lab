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

namespace {
constexpr float PI_F = 3.14159265358979323846f;
float refDispl(float x, float y) {
  if (x < -500 || x > 500 || y < -500 || y > 500)
    return 0.0f;
  float f = std::sin((x / 500.0f + 1.0f) * PI_F);
  float g = -(y / 500.0f) * (y / 500.0f) + 1.0f;
  return 5.0f * f * g;
}
} // namespace

TEST_CASE("ArtificialTsunami2d: water height (Eq. 5.2.1)", "[ArtTsunami2d]") {
  tsunami_lab::setups::ArtificialTsunami2d l_setup;

  // b_in = -100 < 0 everywhere → h = max(-b_in, delta) = max(100, 20) = 100
  REQUIRE(l_setup.getHeight(0.0f, 0.0f) == Approx(100.0f));
  REQUIRE(l_setup.getHeight(50000.0f, 0.0f) == Approx(100.0f));
  REQUIRE(l_setup.getHeight(0.0f, 50000.0f) == Approx(100.0f));
  REQUIRE(l_setup.getHeight(1e6f, 1e6f) == Approx(100.0f));
}

TEST_CASE("ArtificialTsunami2d: momenta are zero", "[ArtTsunami2d]") {
  tsunami_lab::setups::ArtificialTsunami2d l_setup;

  REQUIRE(l_setup.getMomentumX(0.0f, 0.0f) == Approx(0.0f));
  REQUIRE(l_setup.getMomentumY(0.0f, 0.0f) == Approx(0.0f));
  REQUIRE(l_setup.getMomentumX(1e5f, 1e5f) == Approx(0.0f));
  REQUIRE(l_setup.getMomentumY(1e5f, 1e5f) == Approx(0.0f));
}

TEST_CASE("ArtificialTsunami2d: displacement (Eq. 5.2.1)", "[ArtTsunami2d]") {
  tsunami_lab::setups::ArtificialTsunami2d l_setup;

  // f(0)=sin(pi)=0  → d(0,0) = 0
  REQUIRE(l_setup.getDisplacement(0.0f, 0.0f) == Approx(0.0f).margin(1e-6f));

  // peak: x=-250 → f = sin(0.5*pi) = 1,  y=0 → g = 1  →  d = 5
  REQUIRE(l_setup.getDisplacement(-250.0f, 0.0f) == Approx(5.0f));
  // trough: x=+250 → f = sin(1.5*pi) = -1 →  d = -5
  REQUIRE(l_setup.getDisplacement(250.0f, 0.0f) == Approx(-5.0f));

  // boundary of the square: g(±500) = 0 and f(±500) = 0 → d = 0
  REQUIRE(l_setup.getDisplacement(500.0f, 0.0f) == Approx(0.0f).margin(1e-5f));
  REQUIRE(l_setup.getDisplacement(0.0f, 500.0f) == Approx(0.0f).margin(1e-5f));
  REQUIRE(l_setup.getDisplacement(-500.0f, 0.0f) == Approx(0.0f).margin(1e-5f));

  // outside the square → 0
  REQUIRE(l_setup.getDisplacement(501.0f, 0.0f) == Approx(0.0f));
  REQUIRE(l_setup.getDisplacement(0.0f, -501.0f) == Approx(0.0f));
  REQUIRE(l_setup.getDisplacement(1e5f, 1e5f) == Approx(0.0f));

  // matches reference implementation at scattered interior points
  for (float x : {-490.0f, -250.0f, -75.0f, 5.0f, 123.0f, 410.0f}) {
    for (float y : {-480.0f, -200.0f, 0.0f, 50.0f, 333.0f, 495.0f}) {
      REQUIRE(l_setup.getDisplacement(x, y) ==
              Approx(refDispl(x, y)).epsilon(1e-5f));
    }
  }
}

TEST_CASE("ArtificialTsunami2d: bathymetry (Eq. 5.2.1)", "[ArtTsunami2d]") {
  tsunami_lab::setups::ArtificialTsunami2d l_setup;

  // at origin: d = 0  → b = min(-100, -20) + 0 = -100
  REQUIRE(l_setup.getBathymetry(0.0f, 0.0f) == Approx(-100.0f).margin(1e-5f));

  // at peak (-250, 0): d = 5  → b = -100 + 5 = -95
  REQUIRE(l_setup.getBathymetry(-250.0f, 0.0f) == Approx(-95.0f));

  // at trough (250, 0): d = -5 → b = -105
  REQUIRE(l_setup.getBathymetry(250.0f, 0.0f) == Approx(-105.0f));

  // far outside the displacement square: d = 0 → b = -100
  REQUIRE(l_setup.getBathymetry(1e5f, 1e5f) == Approx(-100.0f));
}

TEST_CASE("ArtificialTsunami2d: custom delta", "[ArtTsunami2d]") {
  tsunami_lab::setups::ArtificialTsunami2d l_setup(50.0f);
  // h = max(100, 50) = 100
  REQUIRE(l_setup.getHeight(0.0f, 0.0f) == Approx(100.0f));
  // bathymetry at origin: min(-100, -50) + 0 = -100
  REQUIRE(l_setup.getBathymetry(0.0f, 0.0f) == Approx(-100.0f).margin(1e-5f));
}

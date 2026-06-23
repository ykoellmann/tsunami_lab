/**
 * @author Mika Brückner
 * @section DESCRIPTION
 * Unit tests for the Gaussian seafloor displacement model.
 **/
#include "GaussianDisplacement.h"
#include <catch2/catch.hpp>
#include <cmath>

TEST_CASE("GaussianDisplacement: peak at centre", "[GaussianDispl]") {
  tsunami_lab::displacement::GaussianDisplacement l_model(5.0, 1000.0);

  // At the centre the exponent is zero → full amplitude.
  REQUIRE(l_model.verticalDisplacement(0.0, 0.0) == Approx(5.0));
}

TEST_CASE("GaussianDisplacement: radial symmetry & falloff",
          "[GaussianDispl]") {
  const double l_amp = 3.0;
  const double l_sigma = 2000.0;
  tsunami_lab::displacement::GaussianDisplacement l_model(l_amp, l_sigma);

  // Same radius in any direction yields the same displacement.
  double l_east = l_model.verticalDisplacement(1500.0, 0.0);
  double l_north = l_model.verticalDisplacement(0.0, 1500.0);
  double l_diag = l_model.verticalDisplacement(1500.0 / std::sqrt(2.0),
                                               1500.0 / std::sqrt(2.0));
  REQUIRE(l_east == Approx(l_north));
  REQUIRE(l_east == Approx(l_diag));

  // At exactly one sigma: d = amplitude * exp(-1/2).
  REQUIRE(l_model.verticalDisplacement(l_sigma, 0.0) ==
          Approx(l_amp * std::exp(-0.5)));

  // Monotonically decreasing with distance.
  REQUIRE(l_model.verticalDisplacement(0.0, 0.0) >
          l_model.verticalDisplacement(1000.0, 0.0));
  REQUIRE(l_model.verticalDisplacement(1000.0, 0.0) >
          l_model.verticalDisplacement(5000.0, 0.0));
}

TEST_CASE("GaussianDisplacement: negative amplitude (subsidence)",
          "[GaussianDispl]") {
  tsunami_lab::displacement::GaussianDisplacement l_model(-4.0, 1000.0);
  REQUIRE(l_model.verticalDisplacement(0.0, 0.0) == Approx(-4.0));
  REQUIRE(l_model.verticalDisplacement(500.0, 0.0) < 0.0);
}

TEST_CASE("GaussianDisplacement: influence radius is 3 sigma",
          "[GaussianDispl]") {
  tsunami_lab::displacement::GaussianDisplacement l_model(5.0, 1500.0);
  REQUIRE(l_model.influenceRadius() == Approx(4500.0));
}
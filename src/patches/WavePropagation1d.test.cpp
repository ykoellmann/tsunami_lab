/**
 * @author Alexander Breuer (alex.breuer AT uni-jena.de)
 * @author Yannik Köllmann (yannik.koellmann AT uni-jena.de)
 * @author Jan Vogt (jan.vogt AT uni-jena.de)
 * @author Mika Brückner (mika.brueckner AT uni-jena.de)
 *
 * @section DESCRIPTION
 * Unit tests for the one-dimensional wave propagation patch.
 **/
#include "WavePropagation1d.h"
#include <catch2/catch.hpp>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>

TEST_CASE("Test the 1d wave propagation solver with Roe solver.",
          "[WaveProp1dRoe]") {
  /*
   * Test case:
   *
   *   Single dam break problem between cell 49 and 50.
   *     left | right
   *       10 | 8
   *        0 | 0
   *
   *   Elsewhere steady state.
   *
   * The net-updates at the respective edge are given as
   * (see derivation in Roe solver):
   *    left          | right
   *      9.394671362 | -9.394671362
   *    -88.25985     | -88.25985
   */

  // construct solver and setup a dambreak problem
  tsunami_lab::patches::WavePropagation1d m_waveProp(100);

  for (std::size_t l_ce = 0; l_ce < 50; l_ce++) {
    m_waveProp.setHeight(l_ce, 0, 10);
    m_waveProp.setMomentumX(l_ce, 0, 0);
  }
  for (std::size_t l_ce = 50; l_ce < 100; l_ce++) {
    m_waveProp.setHeight(l_ce, 0, 8);
    m_waveProp.setMomentumX(l_ce, 0, 0);
  }

  // set outflow boundary condition
  m_waveProp.setGhostOutflow();

  // perform a time step
  m_waveProp.timeStep(0.1, "ROE");

  // steady state
  for (std::size_t l_ce = 0; l_ce < 49; l_ce++) {
    REQUIRE(m_waveProp.getHeight()[l_ce] == Approx(10));
    REQUIRE(m_waveProp.getMomentumX()[l_ce] == Approx(0));
  }

  // dam-break
  REQUIRE(m_waveProp.getHeight()[49] == Approx(10 - 0.1 * 9.394671362));
  REQUIRE(m_waveProp.getMomentumX()[49] == Approx(0 + 0.1 * 88.25985));

  REQUIRE(m_waveProp.getHeight()[50] == Approx(8 + 0.1 * 9.394671362));
  REQUIRE(m_waveProp.getMomentumX()[50] == Approx(0 + 0.1 * 88.25985));

  // steady state
  for (std::size_t l_ce = 51; l_ce < 100; l_ce++) {
    REQUIRE(m_waveProp.getHeight()[l_ce] == Approx(8));
    REQUIRE(m_waveProp.getMomentumX()[l_ce] == Approx(0));
  }
}

TEST_CASE("Sanity check using middle states for Roe and FWave solvers.",
          "[WaveProp1dMiddleStates]") {
  // gravity constant g = 9.80665 m/s^2
  tsunami_lab::t_real l_g = 9.80665;

  // run the sanity check for both solvers
  for (std::string l_solver : {"ROE", "FWAVE"}) {
    // the CSV contains Riemann problem initial conditions (hLeft, hRight,
    // huLeft, huRight) and the exact middle state water height h* that the
    // physics predicts at the discontinuity
    std::ifstream l_csv("ressources/middle_states.csv");
    REQUIRE(l_csv.is_open());

    // skip lines starting with '#' (comments), then skip the header line
    std::string l_line;
    while (std::getline(l_csv, l_line)) {
      if (!l_line.empty() && l_line[0] != '#')
        break; // first non-comment line is the header — consumed and discarded
    }

    // iterate over all Riemann problems in the CSV
    while (std::getline(l_csv, l_line)) {
      if (l_line.empty())
        continue;

      // parse the five comma-separated values per row
      std::stringstream l_ss(l_line);
      std::string l_token;
      tsunami_lab::t_real l_hL, l_hR, l_huL, l_huR, l_hStar;
      std::getline(l_ss, l_token, ',');
      l_hL = std::stof(l_token); // left water height
      std::getline(l_ss, l_token, ',');
      l_hR = std::stof(l_token); // right water height
      std::getline(l_ss, l_token, ',');
      l_huL = std::stof(l_token); // left momentum
      std::getline(l_ss, l_token, ',');
      l_huR = std::stof(l_token); // right momentum
      std::getline(l_ss, l_token, ',');
      l_hStar = std::stof(l_token); // expected middle state water height

      // set up a 100-cell domain with the discontinuity at the center:
      // cells 0–49 get the left state, cells 50–99 get the right state
      tsunami_lab::patches::WavePropagation1d l_waveProp(100);
      for (tsunami_lab::t_idx l_ce = 0; l_ce < 50; l_ce++) {
        l_waveProp.setHeight(l_ce, 0, l_hL);
        l_waveProp.setMomentumX(l_ce, 0, l_huL);
      }
      for (tsunami_lab::t_idx l_ce = 50; l_ce < 100; l_ce++) {
        l_waveProp.setHeight(l_ce, 0, l_hR);
        l_waveProp.setMomentumX(l_ce, 0, l_huR);
      }

      // compute the CFL-stable time step scaling dt/dx = 0.45 / lambda_max
      // lambda_max = |u| + sqrt(g*h) is the fastest wave speed in the domain
      tsunami_lab::t_real l_uL = l_huL / l_hL;
      tsunami_lab::t_real l_lambdaMax = std::abs(l_uL) + std::sqrt(l_g * l_hL);
      tsunami_lab::t_real l_scaling = 0.45f / l_lambdaMax;

      // run 25 time steps: with CFL=0.45 the waves travel ~11 cells from the
      // center, so cell 49 is well inside the middle state region
      for (int l_t = 0; l_t < 25; l_t++) {
        l_waveProp.setGhostOutflow();
        l_waveProp.timeStep(l_scaling, l_solver);
      }

      // cell 49 (directly left of the initial discontinuity) must match h*
      // within 1%
      REQUIRE(l_waveProp.getHeight()[49] == Approx(l_hStar).epsilon(0.01));
    }
  }
}

/**
 * @author Yannik Köllmann (yannik.koellmann AT uni-jena.de)
 * @author Jan Vogt (jan.vogt AT uni-jena.de)
 * @author Mika Brückner (mika.brueckner AT uni-jena.de)
 * @section DESCRIPTION
 * F-wave solver for the shallow water equations.
 **/


#include <catch2/catch.hpp>
#define private public
#include "FWave.h"
#undef public

TEST_CASE( "Test the derivation of the FWave speeds.", "[FWaveSpeeds]" ) {
  /*
   * Test case:
   *  h: 10 | 9
   *  u: -3 | 3
   *
   * roe height: 9.5
   * roe velocity: (sqrt(10) * -3 + 3 * 3) / ( sqrt(10) + sqrt(9) )
   *               = -0.0790021169691720
   * roe speeds: s1 = -0.079002116969172024 - sqrt(9.80665 * 9.5) = -9.7311093998375095
   *             s2 = -0.079002116969172024 + sqrt(9.80665 * 9.5) =  9.5731051658991654
   */
  float l_waveSpeedL = 0;
  float l_waveSpeedR = 0;
  tsunami_lab::solvers::FWave::waveSpeeds( 10,
                                         9,
                                         -3,
                                         3,
                                         l_waveSpeedL,
                                         l_waveSpeedR );

  REQUIRE( l_waveSpeedL == Approx( -9.7311093998375095 ) );
  REQUIRE( l_waveSpeedR == Approx(  9.5731051658991654 ) );
}

TEST_CASE("Test the calculation of the FWave flux.", "[FWaveFlux]") {
  /* Test case:
   *  h = 2
   *  hu = 4
   *
   *  expected: f[0] = 4
   *            f[1] = (4*4)/2 + 0.5 * 9.80665 * 2*2 = 8 + 19.6133 = 27.6133
   */

  float l_flux[2] = {0};
  tsunami_lab::solvers::FWave::flux(2,
                                   4,
                                        l_flux);

  REQUIRE(l_flux[0] == Approx(4.0f));
  REQUIRE(l_flux[1] == Approx(27.6133f));
}

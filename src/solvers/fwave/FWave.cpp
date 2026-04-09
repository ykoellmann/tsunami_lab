/**
 * @author Yannik Köllmann (yannik.koellmann AT uni-jena.de)
 * @author Jan Vogt (jan.vogt AT uni-jena.de)
 * @author Mika Brückner (mika.brueckner AT uni-jena.de)
 * @section DESCRIPTION
 * F-wave solver for the shallow water equations.
 **/

#include "FWave.h"

#include "cmath"

void tsunami_lab::solvers::FWave::waveSpeeds(t_real i_hL,
                                             t_real i_hR,
                                             t_real i_uL,
                                             t_real i_uR,
                                             t_real &o_waveSpeedL,
                                             t_real &o_waveSpeedR)
{
  // pre-compute square-root ops
  t_real l_hSqrtL = std::sqrt(i_hL);
  t_real l_hSqrtR = std::sqrt(i_hR);

  // compute FWave averages
  t_real l_hRoe = 0.5f * (i_hL + i_hR);
  t_real l_uRoe = l_hSqrtL * i_uL + l_hSqrtR * i_uR;
  l_uRoe /= l_hSqrtL + l_hSqrtR;

  // compute wave speeds
  t_real l_ghSqrtRoe = m_gSqrt * std::sqrt(l_hRoe);
  o_waveSpeedL = l_uRoe - l_ghSqrtRoe;
  o_waveSpeedR = l_uRoe + l_ghSqrtRoe;
}
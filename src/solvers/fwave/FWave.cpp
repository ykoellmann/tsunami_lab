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
                                             t_real& o_waveSpeedL,
                                             t_real& o_waveSpeedR) {
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

void tsunami_lab::solvers::FWave::flux(t_real i_h,
                                       t_real i_hu,
                                       t_real* o_flux) {
  // pre-compute u = hu / h
  t_real l_u = i_hu / i_h;

  // compute flux:  f(q) = [hu, hu*u + 0.5*g*h^2]^T
  o_flux[0] = i_hu;
  o_flux[1] = i_hu * l_u + 0.5f * m_g * i_h * i_h;
}

void tsunami_lab::solvers::FWave::waveStrengths(t_real i_hL,
                                                t_real i_hR,
                                                t_real i_huL,
                                                t_real i_huR,
                                                t_real i_waveSpeedL,
                                                t_real i_waveSpeedR,
                                                t_real& o_strengthL,
                                                t_real& o_strengthR) {
  // compute inverse of right eigenvector-matrix
  t_real l_detInv = 1 / (i_waveSpeedR - i_waveSpeedL);

  t_real l_rInv[2][2] = {{0}};
  l_rInv[0][0] = l_detInv * i_waveSpeedR;
  l_rInv[0][1] = -l_detInv;
  l_rInv[1][0] = -l_detInv * i_waveSpeedL;
  l_rInv[1][1] = l_detInv;

  // compute jump in fluxes: delta_f = f(q_r) - f(q_l)
  t_real l_fL[2] = {0};
  flux(i_hL, i_huL, l_fL);
  t_real l_fR[2] = {0};
  flux(i_hR, i_huR, l_fR);

  t_real l_diffF[2] = {0};
  l_diffF[0] = l_fR[0] - l_fL[0];
  l_diffF[1] = l_fR[1] - l_fL[1];

  // compute wave strengths: alpha = R^{-1} * delta_f
  o_strengthL = l_rInv[0][0] * l_diffF[0];
  o_strengthL += l_rInv[0][1] * l_diffF[1];

  o_strengthR = l_rInv[1][0] * l_diffF[0];
  o_strengthR += l_rInv[1][1] * l_diffF[1];
}

void tsunami_lab::solvers::FWave::netUpdates(t_real i_hL,
                                             t_real i_hR,
                                             t_real i_huL,
                                             t_real i_huR,
                                             t_real o_netUpdateL[2],
                                             t_real o_netUpdateR[2]) {
  // compute velocities: u = hu / h
  t_real l_uL = i_huL / i_hL;
  t_real l_uR = i_huR / i_hR;

  // compute wave speeds (Roe eigenvalues):  lambda_1 = u_Roe - sqrt(g * h_Roe);
  // lambda_2 = u_Roe + sqrt(g * h_Roe)
  t_real l_waveSpeedL = 0;
  t_real l_waveSpeedR = 0;

  waveSpeeds(i_hL, i_hR, l_uL, l_uR, l_waveSpeedL, l_waveSpeedR);

  // compute wave strengths (eigencoefficients): [a_1, a_2]^T = R⁻¹ * delta f;
  // delta f = f(q_r) - f(q_l)
  t_real l_waveStrengthL = 0;
  t_real l_waveStrengthR = 0;

  waveStrengths(i_hL, i_hR, i_huL, i_huR, l_waveSpeedL, l_waveSpeedR,
                l_waveStrengthL, l_waveStrengthR);

  // compute waves:  Z_p = a_p * r_p, with r_p = [1, lambda_p]^T
  t_real l_waveL[2] = {0};
  t_real l_waveR[2] = {0};

  l_waveL[0] = l_waveStrengthL;
  l_waveL[1] = l_waveStrengthL * l_waveSpeedL;

  l_waveR[0] = l_waveStrengthR;
  l_waveR[1] = l_waveStrengthR * l_waveSpeedR;

  // set net-updates depending on wave speeds:
  // A^- delta Q = sum of Z_p where lambda_p < 0  (left-going waves)
  // A^+ delta Q = sum of Z_p where lambda_p > 0 (right-going waves)
  for (unsigned short l_qt = 0; l_qt < 2; l_qt++) {
    // init net updates to zero
    o_netUpdateL[l_qt] = 0;
    o_netUpdateR[l_qt] = 0;

    // 1st wave: Z_1 goes left if lambda_1 < 0, right otherwise
    if (l_waveSpeedL < 0) {
      o_netUpdateL[l_qt] += l_waveL[l_qt];
    } else {
      o_netUpdateR[l_qt] += l_waveL[l_qt];
    }

    // 2nd wave:  Z_2 goes right if lambda_2 > 0, left otherwise
    if (l_waveSpeedR > 0) {
      o_netUpdateR[l_qt] += l_waveR[l_qt];
    } else {
      o_netUpdateL[l_qt] += l_waveR[l_qt];
    }
  }
}

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
                                             t_real &o_waveSpeedR) {
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
                                       t_real *o_flux) {
    t_real l_u = i_hu / i_h;
    o_flux[0] = i_hu;
    o_flux[1] = i_hu * l_u + 0.5f * m_g * i_h * i_h;
}

void tsunami_lab::solvers::FWave::waveStrengths(t_real i_hL,
                                                t_real i_hR,
                                                t_real i_huL,
                                                t_real i_huR,
                                                t_real i_waveSpeedL,
                                                t_real i_waveSpeedR,
                                                t_real &o_strengthL,
                                                t_real &o_strengthR) {
    // compute inverse of right eigenvector-matrix
    t_real l_detInv = 1 / (i_waveSpeedR - i_waveSpeedL);

    t_real l_rInv[2][2] = {0};
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
    
}

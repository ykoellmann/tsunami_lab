/**
 * @author Yannik Köllmann (yannik.koellmann AT uni-jena.de)
 * @author Jan Vogt (jan.vogt AT uni-jena.de)
 * @author Mika Brückner (mika.brueckner AT uni-jena.de)
 *
 * @section DESCRIPTION
 * One-dimensional subcritical problem.
 **/
#include "SubCritical1d.h"
#include <cmath>

tsunami_lab::setups::SubCritical1d::SubCritical1d() {}

tsunami_lab::t_real
tsunami_lab::setups::SubCritical1d::getHeight(t_real i_x, t_real) const {
  if (i_x <= 25 && i_x >= 0) {
    return -getBathymetry(i_x, 0);
  } else {
    return 0;
  }
}

tsunami_lab::t_real
tsunami_lab::setups::SubCritical1d::getMomentumX(t_real i_x, t_real) const {
  if (i_x <= 25 && i_x >= 0) {
    return 4.42;
  } else {
    return 0;
  }
}

tsunami_lab::t_real
tsunami_lab::setups::SubCritical1d::getMomentumY(t_real, t_real) const {
  return 0;
}

tsunami_lab::t_real
tsunami_lab::setups::SubCritical1d::getBathymetry(t_real i_x, t_real) const {
  if (i_x <= 12 && i_x >= 8) {
    return -1.8 - 0.05 * pow((i_x - 10), 2);
  } else {
    return -2;
  }
}

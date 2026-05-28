/**
 * @author Yannik Köllmann
 * @author Jan Vogt
 * @author Mika Brückner
 * @section DESCRIPTION
 * Setup that restores its state from a netCDF checkpoint file's last
 * stored time step.
 **/
#ifndef TSUNAMI_LAB_SETUPS_CHECKPOINT_H
#define TSUNAMI_LAB_SETUPS_CHECKPOINT_H

#include "../../io/netcdf/NetCDF.h"
#include "../Setup.h"

namespace tsunami_lab {
namespace setups {
class CheckPoint;
} // namespace setups
} // namespace tsunami_lab

class tsunami_lab::setups::CheckPoint : public tsunami_lab::setups::Setup {
private:
  io::NetCDF::CheckpointInfo m_info;

  // row-major (iy*nx + ix) cell-centered fields, allocated with new[]
  t_real* m_h = nullptr;
  t_real* m_hu = nullptr;
  t_real* m_hv = nullptr;
  t_real* m_b = nullptr;

  // map a point (x, y) onto a cell index. clamps to grid bounds.
  t_idx cellIndex(t_real i_x, t_real i_y) const;

public:
  /**
   * Loads the last time step from the given netCDF checkpoint file.
   **/
  CheckPoint(const char* i_path);

  ~CheckPoint();

  t_real getHeight(t_real i_x, t_real i_y) const override;
  t_real getMomentumX(t_real i_x, t_real i_y) const override;
  t_real getMomentumY(t_real i_x, t_real i_y) const override;
  t_real getBathymetry(t_real i_x, t_real i_y) const override;

  const io::NetCDF::CheckpointInfo& getInfo() const { return m_info; }
};

#endif

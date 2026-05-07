/**
 * @author Yannik Köllmann
 * @author Jan Vogt
 * @author Mika Brückner
 * @section DESCRIPTION
 * IO-routines for writing simulation data as NetCDF.
 **/
#ifndef TSUNAMI_LAB_NETCDF_H
#define TSUNAMI_LAB_NETCDF_H

#include "../../constants.h"
#include <netcdf.h>

namespace tsunami_lab {
namespace io {
class NetCDF;
} // namespace io
} // namespace tsunami_lab

class tsunami_lab::io::NetCDF {
private:
  t_idx m_nx = 0;
  t_idx m_ny = 0;
  t_real m_dx = 0;
  t_real m_dy = 0;
  t_real m_originX = 0;
  t_real m_originY = 0;

  int m_ncId = -1;
  int m_dimTime = -1, m_dimX = -1, m_dimY = -1;
  int m_varTime = -1, m_varX = -1, m_varY = -1;
  int m_varH = -1, m_varHu = -1, m_varHv = -1, m_varB = -1;

  t_idx m_timeStep = 0;
  bool m_bWritten = false;

  void checkErr(int i_err);

public:
  /**
   * Constructs a NetCDF writer and creates the output file.
   *
   * @param i_nx        number of interior cells in x-direction.
   * @param i_ny        number of interior cells in y-direction.
   * @param i_dx        cell width in x-direction [m].
   * @param i_dy        cell width in y-direction [m].
   * @param i_originX   x-coordinate of the left domain boundary [m].
   * @param i_originY   y-coordinate of the bottom domain boundary [m].
   * @param i_path      output file path.
   **/
  NetCDF(t_idx i_nx,
         t_idx i_ny,
         t_real i_dx,
         t_real i_dy,
         t_real i_originX,
         t_real i_originY,
         const char* i_path);

  ~NetCDF();

  /**
   * Writes the current simulation state as a new time step.
   *
   * @param i_simTime   simulation time [s].
   * @param i_h         water heights (interior cells, row-major x-major).
   * @param i_hu        x-momenta (interior cells).
   * @param i_hv        y-momenta (interior cells).
   * @param i_b         bathymetry (interior cells).
   * @param i_stride    stride between rows (= nCells_y + 2).
   **/
  void write(t_real i_simTime,
             t_real const* i_h,
             t_real const* i_hu,
             t_real const* i_hv,
             t_real const* i_b,
             t_idx i_stride);
};

#endif // TSUNAMI_LAB_NETCDF_H
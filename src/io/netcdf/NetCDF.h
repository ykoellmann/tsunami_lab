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

  static void checkErr(int i_err);

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

  /**
   * Reads a 2D scalar field (e.g. bathymetry, displacement) from a netCDF
   * file following COARDS conventions. The file must contain 1D coordinate
   * variables `x` (length nx) and `y` (length ny) and a 2D data variable
   * with the given name. The data variable's dimension order may be either
   * (y, x) or (x, y).
   *
   * Output arrays are allocated with new[] inside the function. The caller
   * is responsible for calling delete[] on each of o_x, o_y, o_z.
   * The output z-array is laid out row-major such that
   *     o_z[iy * nx + ix]
   * holds the value at (o_x[ix], o_y[iy]).
   *
   * @param i_path      input file path.
   * @param i_varName   name of the 2D data variable to read.
   * @param o_nx        number of x grid points.
   * @param o_ny        number of y grid points.
   * @param o_x         output x coordinates (length nx).
   * @param o_y         output y coordinates (length ny).
   * @param o_z         output data values, row-major (length nx * ny).
   **/
  static void read(const char* i_path,
                   const char* i_varName,
                   t_idx& o_nx,
                   t_idx& o_ny,
                   t_real*& o_x,
                   t_real*& o_y,
                   t_real*& o_z);
};

#endif // TSUNAMI_LAB_NETCDF_H
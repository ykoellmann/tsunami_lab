/**
 * @author Alexander Breuer (alex.breuer AT uni-jena.de)
 *
 * @section DESCRIPTION
 * IO-routines for writing a snapshot as Comma Separated Values (CSV).
 **/
#ifndef TSUNAMI_LAB_IO_CSV
#define TSUNAMI_LAB_IO_CSV

#include "../constants.h"
#include <cstring>
#include <iostream>

namespace tsunami_lab {
namespace io {
class Csv;
}
} // namespace tsunami_lab

class tsunami_lab::io::Csv {
public:
  /**
   * Writes the data as CSV to the given stream.
   *
   * @param i_dxy cell width in x- and y-direction.
   * @param i_nx number of cells in x-direction.
   * @param i_ny number of cells in y-direction.
   * @param i_stride stride of the data arrays in y-direction (x is assumed to
   * be stride-1).
   * @param i_h water height of the cells; optional: use nullptr if not
   * required.
   * @param i_b bathymetry data of the cells; optional: use nullptr if not
   * required.
   * @param i_hu momentum in x-direction of the cells; optional: use nullptr if
   * not required.
   * @param i_hv momentum in y-direction of the cells; optional: use nullptr if
   * not required.
   * @param io_stream stream to which the CSV-data is written.
   * @param i_simTime current simulation time in seconds.
   **/
  /**
   * Reads bathymetry data from a CSV stream.
   * Expected format: longitude,latitude,distance,bathymetry (with header).
   * Lines starting with '#' and empty lines are skipped.
   * The distance column (km) is converted to metres.
   *
   * @param i_stream input stream to read from.
   * @param o_nRows number of data rows read.
   * @param o_x array of distance values in metres (caller must delete[]).
   * @param o_b array of bathymetry values (caller must delete[]).
   **/
  static void
  read(std::istream& i_stream, t_idx& o_nRows, t_real*& o_x, t_real*& o_b);

  /**
   * Writes the data as CSV to the given stream.
   *
   * @param i_dxy cell width in x- and y-direction.
   * @param i_nx number of cells in x-direction.
   * @param i_ny number of cells in y-direction.
   * @param i_stride stride of the data arrays in y-direction (x is assumed to
   * be stride-1).
   * @param i_h water height of the cells; optional: use nullptr if not
   * required.
   * @param i_b bathymetry data of the cells; optional: use nullptr if not
   * required.
   * @param i_hu momentum in x-direction of the cells; optional: use nullptr if
   * not required.
   * @param i_hv momentum in y-direction of the cells; optional: use nullptr if
   * not required.
   * @param io_stream stream to which the CSV-data is written.
   * @param i_simTime current simulation time in seconds.
   **/
  static void write(t_real i_dxy,
                    t_idx i_nx,
                    t_idx i_ny,
                    t_idx i_stride,
                    t_real const* i_h,
                    t_real const* i_b,
                    t_real const* i_hu,
                    t_real const* i_hv,
                    t_real i_simTime,
                    std::ostream& io_stream);
};

#endif

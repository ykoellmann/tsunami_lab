/**
 * @author Yannik Köllmann
 * @author Jan Vogt
 * @author Mika Brückner
 * @section DESCRIPTION
 * IO-routines for Stations.
 **/

#ifndef TSUNAMI_LAB_IO_STATIONS_H
#define TSUNAMI_LAB_IO_STATIONS_H

#include <string>
#include <vector>
#include <fstream>

#include "../../constants.h"

namespace tsunami_lab {
namespace io {
class Stations;
} // namespace io
} // namespace tsunami_lab

class tsunami_lab::io::Stations {
private:
  //! Internal representation of a single station
  struct Station {
    std::string   name;
    tsunami_lab::t_real        x;
    tsunami_lab::t_real        y;
    tsunami_lab::t_idx         ix;     // corresponding grid cell in x
    tsunami_lab::t_idx         iy;     // corresponding grid cell in y
    std::ofstream file;
  };

  //! All registered stations
  std::vector<Station> m_stations;

  //! Output frequency in seconds
  t_real m_outputFrequency;

  //! Simulation time at which the last output was written
  t_real m_timeLastOutput = -1;

  //! Simulation domain: origin and cell size (needed to map x/y → cell index)
  t_real m_domainX;       // x-coordinate of left domain boundary
  t_real m_domainY;       // y-coordinate of bottom domain boundary
  t_real m_dx;
  t_real m_dy;

  //! Output directory
  std::string m_outputDir;

public:
  /**
   * Constructs the stations manager.
   *
   * @param i_outputFrequency output frequency in seconds.
   * @param i_domainX         x-coordinate of the left domain boundary.
   * @param i_domainY         y-coordinate of the bottom domain boundary.
   * @param i_dx              cell width in x-direction.
   * @param i_dy              cell width in y-direction.
   * @param i_outputDir       directory in which CSV files are written.
   **/
  Stations(t_real      i_outputFrequency,
           t_real      i_domainX,
           t_real      i_domainY,
           t_real      i_dx,
           t_real      i_dy,
           std::string i_outputDir = "stations");

  /**
   * Destructor — flushes and closes all open CSV files.
   **/
  ~Stations();

  /**
   * Registers a new station.
   *
   * @param i_name station name (also used as filename stem).
   * @param i_x    x-coordinate of the station.
   * @param i_y    y-coordinate of the station.
   **/
  void addStation(std::string const& i_name,
                  t_real             i_x,
                  t_real             i_y);

  /**
   * Writes output for all stations if the output frequency has elapsed.
   * Must be called once per simulated time step.
   *
   * @param i_simTime current simulation time in seconds.
   * @param i_h       water height array  (stride layout, incl. ghost cells).
   * @param i_hu      momentum-x array.
   * @param i_hv      momentum-y array.
   * @param i_b       bathymetry array.
   * @param i_stride  stride of the 2d arrays (= nCells_y + 2).
   **/
  void write(t_real        i_simTime,
             t_real const* i_h,
             t_real const* i_hu,
             t_real const* i_hv,
             t_real const* i_b,
             t_idx         i_stride);
};

#endif

/**
 * @author Alexander Breuer (alex.breuer AT uni-jena.de)
 *
 * @section DESCRIPTION
 * IO-routines for writing a snapshot as Comma Separated Values (CSV).
 **/
#include "Csv.h"
#include <sstream>
#include <string>

/**
 * Checks whether a line contains data (non-empty and not a comment).
 **/
static bool isDataLine(const std::string& i_line) {
  return !i_line.empty() && i_line[0] != '#';
}

void tsunami_lab::io::Csv::read(std::istream& i_stream,
                                t_idx& o_nRows,
                                t_real*& o_x,
                                t_real*& o_b) {
  std::string l_line;

  // --- pass 1: count data rows ---
  o_nRows = 0;

  // skip header
  std::getline(i_stream, l_line);

  while (std::getline(i_stream, l_line)) {
    if (isDataLine(l_line)) {
      o_nRows++;
    }
  }

  // --- reset stream ---
  i_stream.clear();
  i_stream.seekg(0);

  // --- allocate arrays ---
  o_x = new t_real[o_nRows];
  o_b = new t_real[o_nRows];

  // --- pass 2: parse data ---
  // skip header
  std::getline(i_stream, l_line);

  t_idx l_row = 0;
  while (std::getline(i_stream, l_line)) {
    if (!isDataLine(l_line)) {
      continue;
    }

    std::stringstream l_ss(l_line);
    std::string l_token;

    // skip longitude and latitude (columns 1-2)
    std::getline(l_ss, l_token, ',');
    std::getline(l_ss, l_token, ',');
    // distance in km (column 3) -> convert to metres
    std::getline(l_ss, l_token, ',');
    o_x[l_row] = static_cast<t_real>(std::stod(l_token)) * 1000;
    // bathymetry (column 4)
    std::getline(l_ss, l_token, ',');
    o_b[l_row] = static_cast<t_real>(std::stod(l_token));

    l_row++;
  }
}

void tsunami_lab::io::Csv::write(t_real i_dxy,
                                 t_idx i_nx,
                                 t_idx i_ny,
                                 t_idx i_stride,
                                 t_real const* i_h,
                                 t_real const* i_b,
                                 t_real const* i_hu,
                                 t_real const* i_hv,
                                 t_real i_simTime,
                                 std::ostream& io_stream) {

  // write simulation time as comment
  io_stream << "# sim_time=" << i_simTime << "\n";
  // write the CSV header
  io_stream << "x,y";
  if (i_h != nullptr)
    io_stream << ",height";
  if (i_b != nullptr)
    io_stream << ",bathymetry";
  if (i_hu != nullptr)
    io_stream << ",momentum_x";
  if (i_hv != nullptr)
    io_stream << ",momentum_y";
  io_stream << "\n";

  // iterate over all cells
  for (t_idx l_iy = 0; l_iy < i_ny; l_iy++) {
    for (t_idx l_ix = 0; l_ix < i_nx; l_ix++) {
      // derive coordinates of cell center
      t_real l_posX = (l_ix + 0.5) * i_dxy;
      t_real l_posY = (l_iy + 0.5) * i_dxy;

      t_idx l_id = l_iy * i_stride + l_ix;

      // write data
      io_stream << l_posX << "," << l_posY;
      if (i_h != nullptr)
        io_stream << "," << i_h[l_id];
      if (i_b != nullptr)
        io_stream << "," << i_b[l_id];
      if (i_hu != nullptr)
        io_stream << "," << i_hu[l_id];
      if (i_hv != nullptr)
        io_stream << "," << i_hv[l_id];
      io_stream << "\n";
    }
  }
  io_stream << std::flush;
}

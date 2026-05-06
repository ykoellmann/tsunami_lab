/**
 * @author Yannik Köllmann
 * @author Jan Vogt
 * @author Mika Brückner
 * @section DESCRIPTION
 * IO-routines for Stations.
 **/

#include "Stations.h"
#include <cmath>
#include <iostream>
#include <sys/stat.h>

static void mkdirp(std::string const& i_path) {
  for (std::size_t l_pos = 1; l_pos <= i_path.size(); ++l_pos) {
    if (l_pos == i_path.size() || i_path[l_pos] == '/') {
      mkdir(i_path.substr(0, l_pos).c_str(), 0755);
    }
  }
}

tsunami_lab::io::Stations::Stations(t_real i_outputFrequency,
                                    t_real i_domainX,
                                    t_real i_domainY,
                                    t_real i_dx,
                                    t_real i_dy,
                                    std::string i_outputDir)
    : m_outputFrequency(i_outputFrequency), m_domainX(i_domainX),
      m_domainY(i_domainY), m_dx(i_dx), m_dy(i_dy), m_outputDir(i_outputDir) {

  // create output directory if it does not exist yet
  mkdirp(m_outputDir);
}

void tsunami_lab::io::Stations::addStation(std::string const& i_name,
                                           t_real i_x,
                                           t_real i_y) {
  Station l_s;
  l_s.name = i_name;
  l_s.x = i_x;
  l_s.y = i_y;

  // map continuous coordinates to 0-based cell indices
  // ghost cells are at index 0 and nCells+1 → interior starts at 1
  l_s.ix = static_cast<t_idx>(std::floor((i_x - m_domainX) / m_dx));
  l_s.iy = static_cast<t_idx>(std::floor((i_y - m_domainY) / m_dy));

  // open CSV file and write header
  std::string l_path = m_outputDir + "/" + i_name + ".csv";
  l_s.file.open(l_path);
  if (!l_s.file.is_open()) {
    std::cerr << "Stations: could not open file " << l_path << std::endl;
  } else {
    l_s.file << "time,h,hu,hv,b\n";
  }
  // move into vector (ofstream is not copyable)
  m_stations.push_back(std::move(l_s));
}

tsunami_lab::io::Stations
tsunami_lab::io::Stations::fromXml(pugi::xml_node const& i_node,
                                   t_real i_domainX,
                                   t_real i_domainY,
                                   t_real i_dx,
                                   t_real i_dy,
                                   std::string i_outputDir) {
  t_real l_freq = i_node.attribute("output_frequency").as_float(1.0f);

  Stations l_stations(l_freq, i_domainX, i_domainY, i_dx, i_dy, i_outputDir);

  for (pugi::xml_node l_s : i_node.children("station")) {
    std::string l_name = l_s.attribute("name").as_string();
    t_real l_x = l_s.attribute("x").as_float();
    t_real l_y = l_s.attribute("y").as_float();

    if (l_name.empty()) {
      std::cerr << "Stations::fromXml: station missing 'name', skipping"
                << std::endl;
      continue;
    }

    l_stations.addStation(l_name, l_x, l_y);
  }

  return l_stations;
}

void tsunami_lab::io::Stations::write(t_real i_simTime,
                                      t_real const* i_h,
                                      t_real const* i_hu,
                                      t_real const* i_hv,
                                      t_real const* i_b,
                                      t_idx i_stride) {
  // check whether output frequency has elapsed
  if (m_timeLastOutput >= 0 &&
      i_simTime - m_timeLastOutput < m_outputFrequency) {
    return;
  }
  m_timeLastOutput = i_simTime;

  for (auto& l_s : m_stations) {
    if (!l_s.file.is_open())
      continue;

    // +1 offset because index 0 is the ghost cell
    t_idx l_idx = (l_s.ix + 1) + (l_s.iy + 1) * i_stride;

    l_s.file << i_simTime << "," << i_h[l_idx] << "," << i_hu[l_idx] << ","
             << i_hv[l_idx] << "," << i_b[l_idx] << "\n";
  }
}

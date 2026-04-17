/**
 * @author Alexander Breuer (alex.breuer AT uni-jena.de)
 * @author Yannik Köllmann (yannik.koellmann AT uni-jena.de)
 * @author Jan Vogt (jan.vogt AT uni-jena.de)
 * @author Mika Brückner (mika.brueckner AT uni-jena.de)
 *
 * @section DESCRIPTION
 * Entry-point for simulations.
 **/
#include "io/Csv.h"
#include "patches/WavePropagation1d.h"
#include "setups/dambreak/DamBreak1d.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <errno.h>
#include <fstream>
#include <iostream>
#include <limits>
#include <sys/stat.h>

#include "setups/rarerare/RareRare1d.h"
#include "setups/shockshock/ShockShock1d.h"

static void printUsage(const char* i_prog) {
  std::cerr << "usage: " << i_prog
            << " -n N_CELLS_X -s SOLVER_MODE -p SETUP_MODE" << std::endl;
  std::cerr << "  -n   number of cells in x-direction (>= 1)" << std::endl;
  std::cerr << "  -s   solver:  FWave | Roe" << std::endl;
  std::cerr << "  -p   setup:   DamBreak | RareRare | ShockShock" << std::endl;
}

int main(int i_argc, char* i_argv[]) {
  tsunami_lab::t_idx l_nx = 0;
  tsunami_lab::t_idx l_ny = 1;
  std::string l_solverMode;
  std::string l_setupMode;
  tsunami_lab::t_real l_dxy = 1;

  std::cout << "####################################" << std::endl;
  std::cout << "### Tsunami Lab                  ###" << std::endl;
  std::cout << "###                              ###" << std::endl;
  std::cout << "### https://scalable.uni-jena.de ###" << std::endl;
  std::cout << "####################################" << std::endl;

  // parse flags
  for (int l_i = 1; l_i < i_argc; ++l_i) {
    std::string l_arg = i_argv[l_i];

    if ((l_arg == "-n" || l_arg == "--cells") && l_i + 1 < i_argc) {
      l_nx = atoi(i_argv[++l_i]);
    } else if ((l_arg == "-s" || l_arg == "--solver") && l_i + 1 < i_argc) {
      l_solverMode = i_argv[++l_i];
      std::transform(l_solverMode.begin(), l_solverMode.end(),
                     l_solverMode.begin(), ::tolower);
    } else if ((l_arg == "-p" || l_arg == "--problem") && l_i + 1 < i_argc) {
      l_setupMode = i_argv[++l_i];
      std::transform(l_setupMode.begin(), l_setupMode.end(),
                     l_setupMode.begin(), ::tolower);
    } else {
      std::cerr << "unknown or incomplete argument: " << l_arg << std::endl;
      printUsage(i_argv[0]);
      return EXIT_FAILURE;
    }
  }

  if (l_nx < 1) {
    std::cerr << "invalid or missing number of cells (-n)" << std::endl;
    printUsage(i_argv[0]);
    return EXIT_FAILURE;
  }
  if (l_solverMode != "fwave" && l_solverMode != "roe") {
    std::cerr << "invalid or missing solver mode (-s)" << std::endl;
    printUsage(i_argv[0]);
    return EXIT_FAILURE;
  }
  if (l_setupMode != "dambreak" && l_setupMode != "rarerare" &&
      l_setupMode != "shockshock") {
    std::cerr << "invalid or missing setup mode (-e)" << std::endl;
    printUsage(i_argv[0]);
    return EXIT_FAILURE;
  }

  l_dxy = 10.0 / l_nx;

  std::cout << "runtime configuration" << std::endl;
  std::cout << "  number of cells in x-direction: " << l_nx << std::endl;
  std::cout << "  number of cells in y-direction: " << l_ny << std::endl;
  std::cout << "  cell size:                      " << l_dxy << std::endl;

  // construct setup
  tsunami_lab::setups::Setup* l_setup = nullptr;

  if (l_setupMode == "dambreak") {
    l_setup = new tsunami_lab::setups::DamBreak1d(10, 5, 5);
  } else if (l_setupMode == "rarerare") {
    l_setup = new tsunami_lab::setups::RareRare1d(10, 5, 5);
  } else if (l_setupMode == "shockshock") {
    l_setup = new tsunami_lab::setups::ShockShock1d(10, 5, 5);
  }

  // construct solver
  tsunami_lab::patches::WavePropagation* l_waveProp;
  l_waveProp = new tsunami_lab::patches::WavePropagation1d(l_nx);

  // maximum observed height in the setup
  tsunami_lab::t_real l_hMax =
      std::numeric_limits<tsunami_lab::t_real>::lowest();

  // set up solver
  for (tsunami_lab::t_idx l_cy = 0; l_cy < l_ny; l_cy++) {
    tsunami_lab::t_real l_y = l_cy * l_dxy;

    for (tsunami_lab::t_idx l_cx = 0; l_cx < l_nx; l_cx++) {
      tsunami_lab::t_real l_x = l_cx * l_dxy;

      // get initial values of the setup
      tsunami_lab::t_real l_h = l_setup->getHeight(l_x, l_y);
      l_hMax = std::max(l_h, l_hMax);

      tsunami_lab::t_real l_hu = l_setup->getMomentumX(l_x, l_y);
      tsunami_lab::t_real l_hv = l_setup->getMomentumY(l_x, l_y);

      // set initial values in wave propagation solver
      l_waveProp->setHeight(l_cx, l_cy, l_h);

      l_waveProp->setMomentumX(l_cx, l_cy, l_hu);

      l_waveProp->setMomentumY(l_cx, l_cy, l_hv);
    }
  }

  // derive maximum wave speed in setup; the momentum is ignored
  tsunami_lab::t_real l_speedMax = std::sqrt(9.81 * l_hMax);

  // derive constant time step; changes at simulation time are ignored
  tsunami_lab::t_real l_dt = 0.5 * l_dxy / l_speedMax;

  // derive scaling for a time step
  tsunami_lab::t_real l_scaling = l_dt / l_dxy;

  // set up time and print control
  tsunami_lab::t_idx l_timeStep = 0;
  tsunami_lab::t_idx l_nOut = 0;
  tsunami_lab::t_real l_endTime = 1.25;
  tsunami_lab::t_real l_simTime = 0;

  // create new directory for simulation
  // (simulation/SETUP_SOLVER_CELLNUMBER_TIMESTAMP
  std::string l_simBaseDir = "simulations/";
  auto l_ts = std::chrono::duration_cast<std::chrono::seconds>(
                  std::chrono::system_clock::now().time_since_epoch())
                  .count();
  std::string l_simName = l_setupMode + "_" + l_solverMode + "_" +
                          std::to_string(l_nx) + "_" + std::to_string(l_ts);
  std::string l_simDir = l_simBaseDir + l_simName;

  mkdir(l_simBaseDir.c_str(), 0755);
  if (mkdir(l_simDir.c_str(), 0755) != 0 && errno != EEXIST) {
    std::cerr << "Failed to create directory: " << l_simDir << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "entering time loop" << std::endl;

  // iterate over time
  while (l_simTime < l_endTime) {
    if (l_timeStep % 25 == 0) {
      std::cout << "  simulation time / #time steps: " << l_simTime << " / "
                << l_timeStep << std::endl;

      std::string l_path =
          l_simDir + "/solution_" + std::to_string(l_nOut) + ".csv";
      std::cout << "  writing wave field to " << l_path << std::endl;

      std::ofstream l_file;
      l_file.open(l_path);

      tsunami_lab::io::Csv::write(l_dxy, l_nx, 1, 1, l_waveProp->getHeight(),
                                  l_waveProp->getMomentumX(), nullptr, l_file);
      l_file.close();
      l_nOut++;
    }

    l_waveProp->setGhostOutflow();
    l_waveProp->timeStep(l_scaling, l_solverMode);

    l_timeStep++;
    l_simTime += l_dt;
  }

  std::cout << "finished time loop" << std::endl;

  // free memory
  std::cout << "freeing memory" << std::endl;
  delete l_setup;
  delete l_waveProp;

  // print output dir
  std::cout << "\033[32m" << "Simulation results written to: " << "\033[36m "
            << l_simDir << "\033[0m" << '\n';
  std::cout << "finished, exiting" << std::endl;
  return EXIT_SUCCESS;
}

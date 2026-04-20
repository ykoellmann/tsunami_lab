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
#include "setups/rarerare/RareRare1d.h"
#include "setups/shockshock/ShockShock1d.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <errno.h>
#include <fstream>
#include <iostream>
#include <limits>
#include <sys/stat.h>

static void printUsage(const char* i_prog) {
  std::cerr << "usage: " << i_prog
            << " -n N_CELLS_X -s SOLVER -p SETUP [PARAMS...] [-d DOMAIN_SIZE] "
               "[-t END_TIME]"
            << std::endl;
  std::cerr << std::endl;
  std::cerr << "mandatory parameters:" << std::endl;
  std::cerr << "  -n   number of cells in x-direction (>= 1)" << std::endl;
  std::cerr << "  -p   setup with configuration:" << std::endl;
  std::cerr << "        > DamBreak   <hLeft> <hRight> <location> [huLeft=0] "
               "[huRight=0]"
            << std::endl;
  std::cerr << "        > RareRare   <height> <momentum> <location>"
            << std::endl;
  std::cerr << "        > ShockShock <height> <momentum> <location>"
            << std::endl;
  std::cerr << std::endl;
  std::cerr << "optional parameters:" << std::endl;
  std::cerr << "  -s   solver:  FWave | Roe (default: FWave)" << std::endl;
  std::cerr << "  -d   total domain size in meters (default: 10.0)"
            << std::endl;
  std::cerr << "  -t   simulation end time in seconds (default: 1.25)"
            << std::endl;
  std::cerr << std::endl;
  std::cerr << "examples:" << std::endl;
  std::cerr << "  " << i_prog << " -n 100 -p DamBreak 10 5 5" << std::endl;
  std::cerr << "  " << i_prog
            << " -n 2500 -s Roe -d 25000 -t 7200 -p DamBreak 14 3.5 12500 0 0.7"
            << std::endl;
}

int main(int i_argc, char* i_argv[]) {
  tsunami_lab::t_idx l_nx = 0;
  tsunami_lab::t_idx l_ny = 1;
  tsunami_lab::t_real l_domainSize = 10.0;
  tsunami_lab::t_real l_endTime = 1.25;
  std::string l_solverMode = "fwave";
  std::string l_setupMode;

  tsunami_lab::t_real l_p1 = 0, l_p2 = 0, l_p3 = 0, l_p4 = 0, l_p5 = 0;

  tsunami_lab::setups::Setup* l_setup = nullptr;

  std::cout << "####################################" << std::endl;
  std::cout << "### Tsunami Lab                  ###" << std::endl;
  std::cout << "###                              ###" << std::endl;
  std::cout << "### https://scalable.uni-jena.de ###" << std::endl;
  std::cout << "####################################" << std::endl;

  // argument parsing
  for (int l_i = 1; l_i < i_argc; ++l_i) {
    std::string l_arg = i_argv[l_i];

    if (l_arg == "-h" || l_arg == "--help") {
      printUsage(i_argv[0]);
      return EXIT_SUCCESS;
    } else if ((l_arg == "-n" || l_arg == "--cells") && l_i + 1 < i_argc) {
      long l_val = std::atol(i_argv[++l_i]);
      if (l_val < 1) {
        std::cerr << "error: -n must be >= 1" << std::endl;
        printUsage(i_argv[0]);
        return EXIT_FAILURE;
      }
      l_nx = static_cast<tsunami_lab::t_idx>(l_val);

    } else if ((l_arg == "-s" || l_arg == "--solver") && l_i + 1 < i_argc) {
      l_solverMode = i_argv[++l_i];
      std::transform(l_solverMode.begin(), l_solverMode.end(),
                     l_solverMode.begin(), ::tolower);
      if (l_solverMode != "fwave" && l_solverMode != "roe") {
        std::cerr << "error: unknown solver '" << l_solverMode
                  << "' -- use FWave or Roe" << std::endl;
        printUsage(i_argv[0]);
        return EXIT_FAILURE;
      }

    } else if ((l_arg == "-d" || l_arg == "--domain") && l_i + 1 < i_argc) {
      l_domainSize = static_cast<tsunami_lab::t_real>(std::atof(i_argv[++l_i]));
      if (l_domainSize <= 0) {
        std::cerr << "error: -d (domain size) must be > 0" << std::endl;
        printUsage(i_argv[0]);
        return EXIT_FAILURE;
      }

    } else if ((l_arg == "-t" || l_arg == "--time") && l_i + 1 < i_argc) {
      l_endTime = static_cast<tsunami_lab::t_real>(std::atof(i_argv[++l_i]));
      if (l_endTime <= 0) {
        std::cerr << "error: -t (end time) must be > 0" << std::endl;
        printUsage(i_argv[0]);
        return EXIT_FAILURE;
      }

    } else if ((l_arg == "-p" || l_arg == "--problem") && l_i + 1 < i_argc) {
      l_setupMode = i_argv[++l_i];
      std::transform(l_setupMode.begin(), l_setupMode.end(),
                     l_setupMode.begin(), ::tolower);

      if (l_setupMode == "dambreak") {
        if (l_i + 3 >= i_argc) {
          std::cerr << "error: DamBreak requires at least 3 parameters: "
                       "<hLeft> <hRight> <location>"
                    << std::endl;
          printUsage(i_argv[0]);
          return EXIT_FAILURE;
        }
        l_p1 =
            static_cast<tsunami_lab::t_real>(std::atof(i_argv[++l_i])); // hLeft
        l_p2 = static_cast<tsunami_lab::t_real>(
            std::atof(i_argv[++l_i])); // hRight
        l_p3 = static_cast<tsunami_lab::t_real>(
            std::atof(i_argv[++l_i])); // location

        if (l_p1 <= 0 || l_p2 <= 0) {
          std::cerr << "error: DamBreak water heights must be > 0" << std::endl;
          printUsage(i_argv[0]);
          return EXIT_FAILURE;
        }

        if (l_i + 1 < i_argc && i_argv[l_i + 1][0] != '-') {
          l_p4 = static_cast<tsunami_lab::t_real>(std::atof(i_argv[++l_i]));
        }
        if (l_i + 1 < i_argc && i_argv[l_i + 1][0] != '-') {
          l_p5 = static_cast<tsunami_lab::t_real>(std::atof(i_argv[++l_i]));
        }
        l_setup =
            new tsunami_lab::setups::DamBreak1d(l_p1, l_p2, l_p3, l_p4, l_p5);

      } else if (l_setupMode == "rarerare") {
        if (l_i + 3 >= i_argc) {
          std::cerr << "error: RareRare requires 3 parameters: "
                       "<height> <momentum> <location>"
                    << std::endl;
          printUsage(i_argv[0]);
          return EXIT_FAILURE;
        }
        l_p1 = static_cast<tsunami_lab::t_real>(std::atof(i_argv[++l_i]));
        l_p2 = static_cast<tsunami_lab::t_real>(std::atof(i_argv[++l_i]));
        l_p3 = static_cast<tsunami_lab::t_real>(std::atof(i_argv[++l_i]));

        if (l_p1 <= 0) {
          std::cerr << "error: RareRare water height must be > 0" << std::endl;
          printUsage(i_argv[0]);
          return EXIT_FAILURE;
        }

        l_setup = new tsunami_lab::setups::RareRare1d(l_p1, l_p2, l_p3);

      } else if (l_setupMode == "shockshock") {
        if (l_i + 3 >= i_argc) {
          std::cerr << "error: ShockShock requires 3 parameters: "
                       "<height> <momentum> <location>"
                    << std::endl;
          printUsage(i_argv[0]);
          return EXIT_FAILURE;
        }
        l_p1 = static_cast<tsunami_lab::t_real>(std::atof(i_argv[++l_i]));
        l_p2 = static_cast<tsunami_lab::t_real>(std::atof(i_argv[++l_i]));
        l_p3 = static_cast<tsunami_lab::t_real>(std::atof(i_argv[++l_i]));

        if (l_p1 <= 0) {
          std::cerr << "error: ShockShock water height must be > 0"
                    << std::endl;
          printUsage(i_argv[0]);
          return EXIT_FAILURE;
        }

        l_setup = new tsunami_lab::setups::ShockShock1d(l_p1, l_p2, l_p3);

      } else {
        std::cerr << "error: unknown setup '" << l_setupMode
                  << "' -- use DamBreak, RareRare or ShockShock" << std::endl;
        printUsage(i_argv[0]);
        return EXIT_FAILURE;
      }

    } else {
      std::cerr << "error: unknown or incomplete argument: " << l_arg
                << std::endl;
      printUsage(i_argv[0]);
      return EXIT_FAILURE;
    }
  }

  // validate required arguments
  if (l_nx == 0) {
    std::cerr << "error: missing required argument -n" << std::endl;
    printUsage(i_argv[0]);
    return EXIT_FAILURE;
  }
  if (l_setup == nullptr) {
    std::cerr << "error: missing required argument -p" << std::endl;
    printUsage(i_argv[0]);
    return EXIT_FAILURE;
  }

  // derived quantities — computed after all args are parsed
  tsunami_lab::t_real l_dxy =
      l_domainSize / static_cast<tsunami_lab::t_real>(l_nx);

  std::cout << "runtime configuration" << std::endl;
  std::cout << "  number of cells in x-direction: " << l_nx << std::endl;
  std::cout << "  number of cells in y-direction: " << l_ny << std::endl;
  std::cout << "  domain size:                    " << l_domainSize << " m"
            << std::endl;
  std::cout << "  cell size:                      " << l_dxy << " m"
            << std::endl;
  std::cout << "  solver:                         " << l_solverMode
            << std::endl;
  std::cout << "  setup:                          " << l_setupMode << std::endl;
  std::cout << "  end time:                       " << l_endTime << " s"
            << std::endl;

  // construct solver and initialise cells
  tsunami_lab::patches::WavePropagation* l_waveProp =
      new tsunami_lab::patches::WavePropagation1d(l_nx);

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
  tsunami_lab::t_real l_speedMax = std::sqrt(9.81f * l_hMax);

  // derive constant time step; changes at simulation time are ignored
  tsunami_lab::t_real l_dt = 0.5f * l_dxy / l_speedMax;

  // derive scaling for a time step
  tsunami_lab::t_real l_scaling = l_dt / l_dxy;

  // output directory
  std::string l_simBaseDir = "simulations/";
  auto l_ts = std::chrono::duration_cast<std::chrono::seconds>(
                  std::chrono::system_clock::now().time_since_epoch())
                  .count();
  std::string l_simDir = l_simBaseDir + l_setupMode + "_" + l_solverMode + "_" +
                         std::to_string(l_nx) + "_" + std::to_string(l_ts);

  mkdir(l_simBaseDir.c_str(), 0755);
  if (mkdir(l_simDir.c_str(), 0755) != 0 && errno != EEXIST) {
    std::cerr << "error: failed to create output directory: " << l_simDir
              << std::endl;
    delete l_setup;
    delete l_waveProp;
    return EXIT_FAILURE;
  }

  // time loop
  tsunami_lab::t_idx l_timeStep = 0;
  tsunami_lab::t_idx l_nOut = 0;
  tsunami_lab::t_real l_simTime = 0;

  // ~20 snapshots evenly distributed over simulation time
  tsunami_lab::t_idx l_outInterval =
      std::max(static_cast<tsunami_lab::t_idx>(25),
               static_cast<tsunami_lab::t_idx>(0.05f * l_endTime / l_dt));

  std::cout << "entering time loop" << std::endl;

  // iterate over time
  while (l_simTime < l_endTime) {
    if (l_timeStep % l_outInterval == 0) {
      std::cout << "  simulation time / #time steps: " << l_simTime << " s / "
                << l_timeStep << std::endl;

      std::string l_path =
          l_simDir + "/solution_" + std::to_string(l_nOut) + ".csv";
      std::cout << "  writing wave field to " << l_path << std::endl;

      std::ofstream l_file(l_path);
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
  std::cout << "\033[32mSimulation results written to:\033[36m " << l_simDir
            << "\033[0m" << std::endl;
  std::cout << "finished, exiting" << std::endl;
  return EXIT_SUCCESS;
}

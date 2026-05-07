/**
 * @author Yannik Köllmann
 * @author Jan Vogt
 * @author Mika Brückner
 * @section DESCRIPTION
 * Unit tests for the 2D tsunami event setup (NetCDF-based).
 **/
#include "../../constants.h"
#include <catch2/catch.hpp>
#include <netcdf.h>
#include <string>
#define private public
#include "TsunamiEvent2d.h"
#undef private

// ---------------------------------------------------------------------------
// helpers to write small synthetic COARDS NetCDF files for testing
// ---------------------------------------------------------------------------

static void writeTestNc(const char* i_path,
                        const float* i_x, int i_nx,
                        const float* i_y, int i_ny,
                        const float* i_z, // row-major [iy*nx+ix]
                        const char* i_varName = "z") {
  int l_ncId;
  NC_NOERR == nc_create(i_path, NC_CLOBBER | NC_NETCDF4, &l_ncId) || 0;

  int l_dimX, l_dimY;
  nc_def_dim(l_ncId, "x", (size_t)i_nx, &l_dimX);
  nc_def_dim(l_ncId, "y", (size_t)i_ny, &l_dimY);

  int l_varX, l_varY, l_varZ;
  nc_def_var(l_ncId, "x", NC_FLOAT, 1, &l_dimX, &l_varX);
  nc_def_var(l_ncId, "y", NC_FLOAT, 1, &l_dimY, &l_varY);
  int l_dims2[2] = {l_dimY, l_dimX}; // (y, x) — COARDS convention
  nc_def_var(l_ncId, i_varName, NC_FLOAT, 2, l_dims2, &l_varZ);
  nc_enddef(l_ncId);

  nc_put_var_float(l_ncId, l_varX, i_x);
  nc_put_var_float(l_ncId, l_varY, i_y);
  nc_put_var_float(l_ncId, l_varZ, i_z); // layout already (y,x) row-major
  nc_close(l_ncId);
}

// ---------------------------------------------------------------------------
// fixtures
// ---------------------------------------------------------------------------

// 3 x 2 bathymetry grid
// x: 0, 100, 200   y: 0, 50
// values (iy*nx + ix):
//   (0,0)=-200  (0,1)=-150  (0,2)=-100
//   (1,0)=-180  (1,1)= 10   (1,2)=-120   <- (1,1) is above water
static const float g_xBath[3] = {0.f, 100.f, 200.f};
static const float g_yBath[2] = {0.f, 50.f};
static const float g_zBath[6] = {-200.f, -150.f, -100.f,
                                  -180.f,  10.f, -120.f};

// 2 x 2 displacement grid (smaller than bathymetry)
// x: 50, 150   y: 10, 40
// values: all 5 m
static const float g_xDispl[2] = {50.f, 150.f};
static const float g_yDispl[2] = {10.f, 40.f};
static const float g_zDispl[4] = {5.f, 5.f, 5.f, 5.f};

static const char* k_bathPath  = "/tmp/tsun2d_test_bath.nc";
static const char* k_displPath = "/tmp/tsun2d_test_displ.nc";

// ---------------------------------------------------------------------------
// tests
// ---------------------------------------------------------------------------

TEST_CASE("TsunamiEvent2d: nearest-neighbour bathymetry lookup", "[TsunamiEvent2d]") {
  writeTestNc(k_bathPath,  g_xBath, 3, g_yBath, 2, g_zBath);
  writeTestNc(k_displPath, g_xDispl, 2, g_yDispl, 2, g_zDispl);

  tsunami_lab::setups::TsunamiEvent2d l_setup(k_bathPath, k_displPath);

  // exact grid points
  REQUIRE(l_setup.getBathymetryRaw(0.f,   0.f)  == Approx(-200.f));
  REQUIRE(l_setup.getBathymetryRaw(100.f, 0.f)  == Approx(-150.f));
  REQUIRE(l_setup.getBathymetryRaw(200.f, 50.f) == Approx(-120.f));
  REQUIRE(l_setup.getBathymetryRaw(100.f, 50.f) == Approx(10.f));

  // between grid points — closest wins
  // x=60 is closer to 100 than to 0  →  ix=1
  REQUIRE(l_setup.getBathymetryRaw(60.f, 0.f)  == Approx(-150.f));
  // x=40 is closer to 0 than to 100  →  ix=0
  REQUIRE(l_setup.getBathymetryRaw(40.f, 0.f)  == Approx(-200.f));
  // y=30 is closer to 50 than to 0  →  iy=1
  REQUIRE(l_setup.getBathymetryRaw(0.f, 30.f)  == Approx(-180.f));

  // clamped at boundary
  REQUIRE(l_setup.getBathymetryRaw(-999.f, -999.f) == Approx(-200.f));
  REQUIRE(l_setup.getBathymetryRaw(9999.f,  9999.f) == Approx(-120.f));
}

TEST_CASE("TsunamiEvent2d: displacement zero outside grid", "[TsunamiEvent2d]") {
  writeTestNc(k_bathPath,  g_xBath, 3, g_yBath, 2, g_zBath);
  writeTestNc(k_displPath, g_xDispl, 2, g_yDispl, 2, g_zDispl);

  tsunami_lab::setups::TsunamiEvent2d l_setup(k_bathPath, k_displPath);

  // inside displacement grid
  REQUIRE(l_setup.getDisplacement(50.f,  10.f) == Approx(5.f));
  REQUIRE(l_setup.getDisplacement(100.f, 25.f) == Approx(5.f));
  REQUIRE(l_setup.getDisplacement(150.f, 40.f) == Approx(5.f));

  // outside displacement grid (but inside bathymetry grid)
  REQUIRE(l_setup.getDisplacement(0.f,   0.f)  == Approx(0.f));
  REQUIRE(l_setup.getDisplacement(200.f, 50.f) == Approx(0.f));
  REQUIRE(l_setup.getDisplacement(0.f,  25.f)  == Approx(0.f));
}

TEST_CASE("TsunamiEvent2d: height and bathymetry from Eq. 5.2.1", "[TsunamiEvent2d]") {
  writeTestNc(k_bathPath,  g_xBath, 3, g_yBath, 2, g_zBath);
  writeTestNc(k_displPath, g_xDispl, 2, g_yDispl, 2, g_zDispl);

  tsunami_lab::setups::TsunamiEvent2d l_setup(k_bathPath, k_displPath);

  // --- underwater cell: b_in = -200, d = 0 (outside displacement grid)
  // h = max(200, 20) = 200
  // b = min(-200, -20) + 0 = -200
  REQUIRE(l_setup.getHeight(0.f, 0.f)     == Approx(200.f));
  REQUIRE(l_setup.getBathymetry(0.f, 0.f) == Approx(-200.f));

  // --- underwater cell inside displacement grid: (100, 25)
  // b_in: x=100→ix=1, y=25 equidistant to 0 and 50 → lower → iy=0
  //   → m_bath[0*3+1] = -150
  // d: x=100 ∈ [50,150], y=25 ∈ [10,40] → inside grid → d = 5
  // h = max(150, 20) = 150
  // b = min(-150, -20) + 5 = -145
  REQUIRE(l_setup.getHeight(100.f, 25.f)     == Approx(150.f));
  REQUIRE(l_setup.getBathymetry(100.f, 25.f) == Approx(-145.f));

  // --- above-water cell: b_in = 10 >= 0
  // h = 0
  // b = max(10, 20) + d   (closest displacement: x=100,y=50 is inside grid? y=50>40 → outside → d=0)
  // b = max(10,20) + 0 = 20
  REQUIRE(l_setup.getHeight(100.f, 50.f)     == Approx(0.f));
  REQUIRE(l_setup.getBathymetry(100.f, 50.f) == Approx(20.f));
}

TEST_CASE("TsunamiEvent2d: momenta are always zero", "[TsunamiEvent2d]") {
  writeTestNc(k_bathPath,  g_xBath, 3, g_yBath, 2, g_zBath);
  writeTestNc(k_displPath, g_xDispl, 2, g_yDispl, 2, g_zDispl);

  tsunami_lab::setups::TsunamiEvent2d l_setup(k_bathPath, k_displPath);

  REQUIRE(l_setup.getMomentumX(0.f, 0.f) == Approx(0.f));
  REQUIRE(l_setup.getMomentumY(0.f, 0.f) == Approx(0.f));
}

TEST_CASE("TsunamiEvent2d: domain extent from bathymetry grid", "[TsunamiEvent2d]") {
  writeTestNc(k_bathPath,  g_xBath, 3, g_yBath, 2, g_zBath);
  writeTestNc(k_displPath, g_xDispl, 2, g_yDispl, 2, g_zDispl);

  tsunami_lab::setups::TsunamiEvent2d l_setup(k_bathPath, k_displPath);

  REQUIRE(l_setup.getDomainOriginX() == Approx(0.f));
  REQUIRE(l_setup.getDomainOriginY() == Approx(0.f));
  REQUIRE(l_setup.getDomainSizeX()   == Approx(200.f));
  REQUIRE(l_setup.getDomainSizeY()   == Approx(50.f));
}

TEST_CASE("TsunamiEvent2d: closestIdx helper", "[TsunamiEvent2d]") {
  float l_arr[5] = {0.f, 10.f, 20.f, 30.f, 40.f};

  using T = tsunami_lab::setups::TsunamiEvent2d;

  REQUIRE(T::closestIdx(l_arr, 5, -5.f) == 0);  // clamp left
  REQUIRE(T::closestIdx(l_arr, 5, 99.f) == 4);  // clamp right
  REQUIRE(T::closestIdx(l_arr, 5, 0.f)  == 0);  // exact
  REQUIRE(T::closestIdx(l_arr, 5, 40.f) == 4);  // exact
  REQUIRE(T::closestIdx(l_arr, 5, 5.f)  == 0);  // equidistant → lower
  REQUIRE(T::closestIdx(l_arr, 5, 6.f)  == 1);  // closer to 10
  REQUIRE(T::closestIdx(l_arr, 5, 4.f)  == 0);  // closer to 0
  REQUIRE(T::closestIdx(l_arr, 5, 25.f) == 2);  // equidistant 20/30 → lower
}

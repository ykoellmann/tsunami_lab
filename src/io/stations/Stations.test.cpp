/**
 * @author Yannik Köllmann
 * @author Jan Vogt
 * @author Mika Brückner
 *
 * @section DESCRIPTION
 * Unit tests for the Stations output.
 **/
#include "../../constants.h"
#include <catch2/catch.hpp>
#include <fstream>
#include <sstream>
#include <string>
#define private public
#include "Stations.h"
#undef public
#include <pugixml.hpp>

// helper: read entire file into a string
static std::string readFile(std::string const& i_path) {
  std::ifstream l_f(i_path);
  return std::string(std::istreambuf_iterator<char>(l_f),
                     std::istreambuf_iterator<char>());
}

TEST_CASE("Stations: cell-index mapping", "[StationsIndex]") {
  // domain origin at (0,0), dx=dy=1
  tsunami_lab::io::Stations l_st(9999, 0, 0, 1, 1, "/tmp/stations_test_idx");

  l_st.addStation("A", 0.5, 0.5); // → ix=0, iy=0
  l_st.addStation("B", 2.7, 1.3); // → ix=2, iy=1

  REQUIRE(l_st.m_stations[0].ix == 0);
  REQUIRE(l_st.m_stations[0].iy == 0);
  REQUIRE(l_st.m_stations[1].ix == 2);
  REQUIRE(l_st.m_stations[1].iy == 1);
}

TEST_CASE("Stations: output frequency gating", "[StationsFreq]") {
  // 2x2 grid with ghost cells → stride 4, interior cell (1,1) at flat index 5
  tsunami_lab::t_real l_h[16] = {};
  tsunami_lab::t_real l_hu[16] = {};
  tsunami_lab::t_real l_hv[16] = {};
  tsunami_lab::t_real l_b[16] = {};

  {
    // frequency = 2.0 s
    tsunami_lab::io::Stations l_st(2.0, 0, 0, 1, 1, "/tmp/stations_test_freq");
    l_st.addStation("S", 0.5, 0.5);

    l_h[5] = 3.0f;
    l_hu[5] = 1.0f;

    // t=0.0 → first write (m_timeLastOutput starts at -1)
    l_st.write(0.0, l_h, l_hu, l_hv, l_b, 4);
    // t=1.5 → too soon (< 2.0 since last write at 0.0), skipped
    l_h[5] = 99.0f;
    l_st.write(1.5, l_h, l_hu, l_hv, l_b, 4);
    // t=2.0 → exactly at frequency boundary → should write
    l_h[5] = 7.0f;
    l_st.write(2.0, l_h, l_hu, l_hv, l_b, 4);
  } // destructor flushes and closes files

  std::string l_content = readFile("/tmp/stations_test_freq/S.csv");
  std::string l_expected = "time,h,hu,hv,b\n"
                           "0,3,1,0,0\n"
                           "2,7,1,0,0\n"; // t=1.5 skipped

  REQUIRE(l_content == l_expected);
}

TEST_CASE("Stations: CSV header and values", "[StationsCSV]") {
  // stride = 3 (1 interior cell + 2 ghost cells per row)
  tsunami_lab::t_real l_h[9] = {0, 0, 0, 0, 2, 0, 0, 0, 0};
  tsunami_lab::t_real l_hu[9] = {0, 0, 0, 0, 3, 0, 0, 0, 0};
  tsunami_lab::t_real l_hv[9] = {0, 0, 0, 0, 4, 0, 0, 0, 0};
  tsunami_lab::t_real l_b[9] = {0, 0, 0, 0, 5, 0, 0, 0, 0};

  {
    tsunami_lab::io::Stations l_st(0.0, 0, 0, 10, 10, "/tmp/stations_test_csv");
    l_st.addStation("P", 5.0, 5.0); // ix=0, iy=0 → flat index 1+1*stride
    l_st.write(1.5, l_h, l_hu, l_hv, l_b, 3);
  } // destructor flushes and closes files

  std::string l_content = readFile("/tmp/stations_test_csv/P.csv");
  std::string l_expected = "time,h,hu,hv,b\n"
                           "1.5,2,3,4,5\n";

  REQUIRE(l_content == l_expected);
}

TEST_CASE("Stations: fromXml parses stations and output_frequency",
          "[StationsFromXml]") {
  pugi::xml_document l_doc;
  pugi::xml_node l_root = l_doc.append_child("stations");
  l_root.append_attribute("output_frequency") = 3.0f;

  pugi::xml_node l_s1 = l_root.append_child("station");
  l_s1.append_attribute("name") = "alpha";
  l_s1.append_attribute("x") = 5.0f;
  l_s1.append_attribute("y") = 0.0f;

  pugi::xml_node l_s2 = l_root.append_child("station");
  l_s2.append_attribute("name") = "beta";
  l_s2.append_attribute("x") = 15.0f;
  l_s2.append_attribute("y") = 0.0f;

  // nameless station should be skipped
  l_root.append_child("station").append_attribute("x") = 99.0f;

  {
    tsunami_lab::io::Stations l_st = tsunami_lab::io::Stations::fromXml(
        l_root, 0, 0, 10, 10, "/tmp/stations_test_xml");

    REQUIRE(l_st.m_outputFrequency == Approx(3.0f));
    REQUIRE(l_st.m_stations.size() == 2);

    REQUIRE(l_st.m_stations[0].name == "alpha");
    REQUIRE(l_st.m_stations[0].ix == 0);
    REQUIRE(l_st.m_stations[0].iy == 0);

    REQUIRE(l_st.m_stations[1].name == "beta");
    REQUIRE(l_st.m_stations[1].ix == 1);
    REQUIRE(l_st.m_stations[1].iy == 0);
  }

  // verify CSV files were created for the two named stations
  REQUIRE(readFile("/tmp/stations_test_xml/alpha.csv").substr(0, 14) ==
          "time,h,hu,hv,b");
  REQUIRE(readFile("/tmp/stations_test_xml/beta.csv").substr(0, 14) ==
          "time,h,hu,hv,b");
}

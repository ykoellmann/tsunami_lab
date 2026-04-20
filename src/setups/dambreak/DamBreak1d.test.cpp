/**
 * @author Alexander Breuer (alex.breuer AT uni-jena.de)
 *
 * @section DESCRIPTION
 * Tests the dam break setup.
 **/
#include "DamBreak1d.h"
#include <catch2/catch.hpp>

TEST_CASE("Test the one-dimensional dam break setup (classic, zero momentum).",
          "[DamBreak1d]") {
  tsunami_lab::setups::DamBreak1d l_damBreak(25, 0, 55, 0, 3);

  // left side
  REQUIRE(l_damBreak.getHeight(2, 0) == 25);
  REQUIRE(l_damBreak.getMomentumX(2, 0) == 0);
  REQUIRE(l_damBreak.getMomentumY(2, 0) == 0);

  REQUIRE(l_damBreak.getHeight(2, 5) == 25);
  REQUIRE(l_damBreak.getMomentumX(2, 5) == 0);
  REQUIRE(l_damBreak.getMomentumY(2, 2) == 0);

  // right side
  REQUIRE(l_damBreak.getHeight(4, 0) == 55);
  REQUIRE(l_damBreak.getMomentumX(4, 0) == 0);
  REQUIRE(l_damBreak.getMomentumY(4, 0) == 0);

  REQUIRE(l_damBreak.getHeight(4, 5) == 55);
  REQUIRE(l_damBreak.getMomentumX(4, 5) == 0);
  REQUIRE(l_damBreak.getMomentumY(4, 2) == 0);
}

TEST_CASE("Test the one-dimensional dam break setup with non-zero momentum on both sides.",
          "[DamBreak1d]") {
  // q_l = [h=14, hu=0.3], q_r = [h=3.5, hu=0.7], dam at x=3
  tsunami_lab::setups::DamBreak1d l_damBreak(14.0f, 0.3f, 3.5f, 0.7f, 3);

  // left side
  REQUIRE(l_damBreak.getHeight(2, 0)    == Approx(14.0f));
  REQUIRE(l_damBreak.getMomentumX(2, 0) == Approx(0.3f));
  REQUIRE(l_damBreak.getMomentumY(2, 0) == 0);

  // right side
  REQUIRE(l_damBreak.getHeight(4, 0)    == Approx(3.5f));
  REQUIRE(l_damBreak.getMomentumX(4, 0) == Approx(0.7f));
  REQUIRE(l_damBreak.getMomentumY(4, 0) == 0);
}

TEST_CASE("Test the one-dimensional dam break setup with negative momentum (reverse flow).",
          "[DamBreak1d]") {
  // Wasser fließt links nach rechts, rechts auch positiv -> beide nach rechts
  // Hier: links positiv, rechts negativ -> aufeinander zu (shock-shock-artig)
  tsunami_lab::setups::DamBreak1d l_damBreak(10, 2.0f, 10, -2.0f, 5);

  REQUIRE(l_damBreak.getMomentumX(2, 0) == Approx(2.0f));
  REQUIRE(l_damBreak.getMomentumX(8, 0) == Approx(-2.0f));
}
{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    python3Packages.sphinx
    python3Packages.sphinx-rtd-theme
    doxygen
    scons
  ];
}

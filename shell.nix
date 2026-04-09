{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    gcc14
    python3Packages.sphinx
    python3Packages.sphinx-rtd-theme
    doxygen
    scons
  ];
}

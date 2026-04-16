{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    gcc14
    python3Packages.sphinx
    python3Packages.sphinx-rtd-theme
    python3Packages.matplotlib
    python3Packages.pandas
    doxygen
    scons
    clang-tools
    bear
  ];
}

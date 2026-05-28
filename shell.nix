{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    gcc14
    python3
    python3Packages.sphinx
    python3Packages.sphinx-rtd-theme
    python3Packages.matplotlib
    python3Packages.pandas
    python3Packages.numpy
    doxygen
    scons
    clang-tools
    cppcheck
    bear
    gmt
    wget
    tree
    netcdf
    hdf5
    zlib
    paraview
  ];

  shellHook = ''
    git config core.hooksPath .githooks
  '';
}

{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    git
    gcc14
    python3
    python3Packages.sphinx
    python3Packages.sphinx-rtd-theme
    python3Packages.matplotlib
    python3Packages.pandas
    python3Packages.numpy
    python3Packages.glad
    doxygen
    cmake
    ninja
    clang-tools
    cppcheck
    bear
    gmt
    wget
    unzip
    tree
    netcdf
    hdf5
    zlib
    paraview
    glfw
    glm
  ];

  shellHook = ''
    git config core.hooksPath .githooks
    # On non-NixOS systems the NVIDIA/Mesa GLX vendor libs live in /usr/lib64.
    # libglvnd from nixpkgs needs them in LD_LIBRARY_PATH to find the vendor.
    export LD_LIBRARY_PATH=/usr/lib64:''${LD_LIBRARY_PATH:-}
  '';
}

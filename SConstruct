##
# @author Alexander Breuer (alex.breuer AT uni-jena.de)
#
# @section DESCRIPTION
# Entry-point for builds.
##
import SCons
import os

print( '####################################' )
print( '### Tsunami Lab                  ###' )
print( '###                              ###' )
print( '### https://scalable.uni-jena.de ###' )
print( '####################################' )
print()
print('runnning build script')

# configuration
vars = Variables()

vars.AddVariables(
  EnumVariable( 'mode',
                'compile modes, option \'san\' enables address and undefined behavior sanitizers',
                'release',
                allowed_values=('release', 'debug', 'release+san', 'debug+san' )
              ),
  EnumVariable( 'opt',
                'optimization level for release builds',
                'o2',
                allowed_values=('o2', 'o3', 'ofast' )
              ),
  EnumVariable( 'arch',
                'target architecture; \'native\' enables -march=native (e.g. AVX-512 on Ice Lake)',
                'none',
                allowed_values=('none', 'native', 'icelake-server' )
              ),
  BoolVariable( 'omp',
                'enable OpenMP shared-memory parallelism (-fopenmp)',
                False
              ),
  BoolVariable( 'report',
                'emit Clang optimization remarks (-Rpass=.* -Rpass-missed=.* -Rpass-analysis=.*)',
                False
              ),
  BoolVariable( 'inline',
                'allow function inlining; set to 0 (-fno-inline) for finer VTune profiling',
                True
              )
)

# exit in the case of unknown variables
if vars.UnknownVariables():
  print( "build configuration corrupted, don't know what to do with: " + str(vars.UnknownVariables().keys()) )
  exit(1)

# create environment
# forward the surrounding shell environment so that, e.g., module-provided
# compilers and their libraries are found on the cluster.
env = Environment( variables = vars,
                   ENV       = os.environ )

# allow overriding the C++ compiler via the CXX environment variable,
# e.g. `CXX=clang++ scons` (ch. 8, compilers task 1.1).
if 'CXX' in os.environ:
  env['CXX'] = os.environ['CXX']

# generate help message
Help( vars.GenerateHelpText( env ) )

# add default flags
env.Append( CXXFLAGS = [ '-std=c++11',
                         '-Wall',
                         '-Wextra',
                         '-Wpedantic',
                         '-Wno-keyword-macro' ] )

# set optimization mode
if 'debug' in env['mode']:
  env.Append( CXXFLAGS = [ '-g',
                           '-O0' ] )
else:
  # -Ofast enables -ffast-math: relaxes IEEE-754 compliance (reassociation,
  # no NaN/Inf handling, flush-to-zero) which may change numerical results.
  l_optFlags = { 'o2': '-O2', 'o3': '-O3', 'ofast': '-Ofast' }
  env.Append( CXXFLAGS = [ l_optFlags[ env['opt'] ] ] )

# enable architecture-specific codegen (vectorization), e.g. -march=native
if env['arch'] != 'none':
  env.Append( CXXFLAGS = [ '-march=' + env['arch'] ] )

# Under -ffast-math, oneAPI's Clang/icpx fold sin(pi*x)/cos(pi*x) into
# sinpif()/cospif() from Intel's math library (libimf). icpx links libimf
# automatically; raw clang++ does not, so add it explicitly. GCC + glibc
# never emit these symbols, so libimf is only needed for the LLVM compilers
# and only when -Ofast (which implies -ffast-math) is active.
if ('clang' in env['CXX'] or 'icpx' in env['CXX']) and \
   env['opt'] == 'ofast' and 'debug' not in env['mode']:
  env.Append( LINKFLAGS = [ '-limf' ] )

# enable OpenMP
if env['omp']:
  env.Append( CXXFLAGS  = [ '-fopenmp' ] )
  env.Append( LINKFLAGS = [ '-fopenmp' ] )

# disable inlining for finer-grained profiling (e.g. VTune, ch. 8 task)
if not env['inline']:
  env.Append( CXXFLAGS = [ '-g',
                           '-fno-inline' ] )

# Clang optimization-remark reports: vectorization, inlining, etc.
if env['report']:
  env.Append( CXXFLAGS = [ '-Rpass=.*',
                           '-Rpass-missed=.*',
                           '-Rpass-analysis=.*' ] )

# add sanitizers
if 'san' in  env['mode']:
  env.Append( CXXFLAGS =  [ '-g',
                            '-fsanitize=float-divide-by-zero',
                            '-fsanitize=bounds',
                            '-fsanitize=address',
                            '-fsanitize=undefined',
                            '-fno-omit-frame-pointer' ] )
  env.Append( LINKFLAGS = [ '-g',
                            '-fsanitize=address',
                            '-fsanitize=undefined' ] )
else:
  env.Append( CXXFLAGS = [ '-Werror' ] )

# add Catch2
env.Append( CXXFLAGS = [ '-isystem', 'submodules/Catch2/single_include' ] )

# add pugixml include path
env.Append( CPPPATH = [ '#submodules/pugixml/src' ] )

# link netCDF (must be available via nix-shell or system package)
import subprocess, os
try:
  netcdf_prefix = subprocess.check_output(['nc-config', '--prefix'], text=True).strip()
  env.Append( CPPPATH = [ netcdf_prefix + '/include' ] )
  env.Append( LIBPATH = [ netcdf_prefix + '/lib' ] )
except Exception:
  pass
env.Append( LIBS = [ 'netcdf' ] )

# compile pugixml with warnings suppressed (third-party code)
pugi_env = env.Clone()
pugi_env.Append( CXXFLAGS = [ '-w' ] )
l_pugixml = pugi_env.Object( target = 'build/pugixml',
                              source = 'submodules/pugixml/src/pugixml.cpp' )

# get source files
VariantDir( variant_dir = 'build/src',
            src_dir     = 'src' )

env.sources = []
env.tests = []

Export('env')
SConscript( 'build/src/SConscript' )
Import('env')

env.Program( target = 'build/tsunami_lab',
             source = env.sources + env.standalone + l_pugixml )

env.Program( target = 'build/tests',
             source = env.sources + env.tests + l_pugixml )

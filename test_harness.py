#!/usr/bin/python3

import sys
import argparse
import pathlib
import subprocess
import numpy as np
import json
import graphplot as grplt
import statistics as st


lib_category = { 'bdwgc' : ('gc', 'libgc.so') ,
                 'jemalloc' : ('manual', 'libc.so'),
                 'snmalloc' : ('manual', 'libsnmallocshim.so'),
                 'cheribumpalloc' : ('manual', 'libcheribumpalloc.so')
               }
num_proc = 4

run_bench = [
 'binary_tree.elf', 
 'richards.elf', 
 'espresso.elf largest.espresso', 
 'barnes.elf < input',
 'cfrac.elf 17545186520507317056371138836327483792789528',
 'random_mixed_alloc.elf',
 'small_fixed_alloc.elf',
]


additional_benchmarks = [ 
 #'glibc_bench_simple.elf', 
 #'glibc_bench_thread.elf',
 #f'mstress.elf {num_proc} 50 25', 
 #f'rptest.elf {num_proc} 0 1 2 500 1000 100 8 16000',
 #f'xmalloc.elf -w {num_proc} -t 5 -s 64'
] 

pmc_events = [
 'L1D_CACHE', 
 'L1I_CACHE', 
 'L2D_CACHE',
 'CPU_CYCLES',
 'INST_RETIRED',
 'MEM_ACCESS',
 'L1I_CACHE_REFILL', 
 'L1D_CACHE_REFILL', 
 'L2D_CACHE_REFILL', 
 'LL_CACHE_RD'
# 'LL_CACHE_MISS_RD',
# 'BUS_ACCESS', 
# 'BUS_ACCESS_RD_CTAG' 
] 

pmc_timeout = 1200 # in seconds

class Build:
  def __init__(self, cmdline):
    self.cmd = cmdline
    self.bench_opts = None
    self.arch_rdir = 'bench_bdwgc_' + self.cmd.arch
    self.append_basedir = lambda _rconn : _rconn[1] + "/"  if len(_rconn) > 1 else ""

  def build_dependencies(self):
    # Clone the cheribuild script
    ret = subprocess.run( ['git','clone', 
                           f'https://github.com/CTSRD-CHERI/cheribuild.git', 
                           f'{self.cmd.dependency_dir}'], check=True)
    ret.check_returncode()

    # Build the purecap toolchain
    ret = subprocess.run( [f'{self.cmd.dependency_dir}/cheribuild.py',
                           '-d', '-f', '-c', f'cheribsd-morello-purecap'], check=True)
    ret.check_returncode()

    # Build the hybrid toolchain
    ret = subprocess.run( [f'{self.cmd.dependency_dir}/cheribuild.py',
                           '-d', '-f', '--enable-hybrid-targets',
                           f'cheribsd-morello-hybrid'], check=True)
    ret.check_returncode()

    self.patch_bm_abi()

  def patch_bm_abi(self):
    # patch compiler config for benchmark API
    cheri_output = self.cmd.dependency_dir.home() / "cheri/output"
    compiler_path = cheri_output / "morello-sdk/bin" 
    bm_api_cfg = compiler_path / "cheribsd-morello-benchmarkabi.cfg"

    try: 
      with open(bm_api_cfg, "x") as fd: 
        fd.write(f"-target\n"
                 f"aarch64-unknown-freebsd13\n"
                 f"--sysroot={cheri_output}/rootfs-morello-purecap\n"
                 f"-B{compiler_path}\n"
                 f"-mcpu=rainier\n"
                 f"-march=morello\n"
                 f"-mabi=purecap-benchmark\n"
                 f"-Xclang\n"
                 f"-morello-vararg=new\n"
                 f"-morello-bounded-memargs\n")
    except FileExistsError as fee: 
     print("Benchmark ABI already written")



  def configure(self):
    cmake_build = ['cmake','-B', f'{self.cmd.build_dir}',
                   '-S', f'{pathlib.Path.cwd()}',
                   f'-DCMAKE_BUILD_TYPE={self.cmd.buildtype}',
                   f'-DCMAKE_INSTALL_PREFIX={self.cmd.install_dir}',
                   f'-Dgclib={self.cmd.libs[0]}',
                   f'-Dbm_logfile={self.cmd.args.output}',
                   f'-DCMAKE_TOOLCHAIN_FILE=morello-{self.cmd.arch}.cmake']
    if self.cmd.args.binlinkoption == 'static':
      cmake_build.append('-DBUILD_SHARED_LIBS=OFF')
    elif self.cmd.args.binlinkoption in ['dynamic', 'benchlib']:
      cmake_build.append('-DBENCHLIB=ON')

    ret = subprocess.run( cmake_build , check=True)
    ret.check_returncode()

  def build(self):
    ret = subprocess.run( ['cmake','--build', f'{self.cmd.build_dir}'], check=True)
    ret.check_returncode()

  def install(self):
    ret = subprocess.run( ['cmake','--install', f'{self.cmd.build_dir}'], check=True)
    ret.check_returncode()

  def remote_install(self):
    executables = [str(elf) for elf in (self.cmd.install_dir/ 'bin').glob('*.elf')] 
    libs = [str(lib) for lib in (self.cmd.install_dir/ 'lib').glob('lib*.so.*.*.*')] 
    
    print(f"Remote install  libraries -> {libs}")
    # Add all the shim libs if required
    if self.cmd.args.binlinkoption in ['dynamic', 'benchlib']:
      libs += [ str(self.cmd.install_dir / "lib" / f"lib{pathlib.Path(_exec).stem}_shim.so") for _exec in executables ]

    misc_data = [str(filename) for filename in (self.cmd.install_dir/ 'conf').rglob('*') if filename.is_file()] + \
                [str(filename) for filename in (self.cmd.install_dir/ 'data').rglob('*') if filename.is_file()] 

    _rconn_param = self._sanitize_basedir(self.cmd.args.remoteconnect.split(sep=':',maxsplit=1))

    # Create execution directory within remote base dir
    #mkdir_ = ['ssh', '-p', self.cmd.sshport, _rconn_param[0] , \
    #                       f'mkdir -p ' 
    #                       f'{self.append_basedir(_rconn_param)}'
    #                       f'{self.arch_rdir}']
    ret = subprocess.run( ['ssh', '-p', self.cmd.sshport, _rconn_param[0] , \
                           f'mkdir -p ' 
                           f'{self.append_basedir(_rconn_param)}'
                           f'{self.arch_rdir}'], check=True)
    ret.check_returncode()

    # Copy executables from install directory to remote-install dir
    #rinstall_ = ['scp', '-P', self.cmd.sshport] + executables + libs + misc_data +\
    #                      [f'{_rconn_param[0]}:{self.append_basedir(_rconn_param)}'
    #                       f'{self.arch_rdir}']
    #print(f"RINSTALL = {rinstall_}")
    ret = subprocess.run( ['scp', '-P', self.cmd.sshport] + executables + libs + misc_data +
                          [f'{_rconn_param[0]}:{self.append_basedir(_rconn_param)}'
                           f'{self.arch_rdir}'], check=True)
    ret.check_returncode()

    self.__link_remote_lib(_rconn_param, \
                           [lib for lib in (self.cmd.install_dir/ 'lib').glob('lib*.so.*.*.*')])

  def remote_exec(self):
    #_rconn_param = self._sanitize_basedir(self.cmd.args.remoteconnect.split(sep=':',maxsplit=1))
    executables = [elf for elf in (self.cmd.install_dir/ 'bin').glob('*.elf')] 
    assert self.cmd.libs[0] in lib_category.keys(), f"{self.cmd.libs[0]} not in {lib_category.keys()}"
    if lib_category[ self.cmd.libs[0] ][0] == 'manual':
      if self.cmd.libs[0] != "jemalloc": 
        env_libpath = f'LD_64_PRELOAD' if self.cmd.arch == "hybrid" else f'LD_PRELOAD'
        env_libpath += f'=$(pwd)/{lib_category[ self.cmd.libs[0]][1]}'
      else: 
          env_libpath = ""
    else:
      env_var = { "hybrid": "LD_64", 
                  "purecap": "LD", 
                  "benchmarkabi": "LD_64CB"}
      env_libpath = f"{env_var[self.cmd.arch]}_LIBRARY_PATH"
      env_libpath += f'=$(pwd) '

    _outfile = pathlib.Path(f'{self.cmd.out_data_dir}/{self.cmd.args.output}')
    if _outfile.exists():
      with open(_outfile, "r") as fd:
        results = json.load(fd)
    else : 
      results = {}  # { hyb-pure : { bm : {gc_cycles : <> , gc_time : <> } }}

    # Ensure hybrid version is executed first
    # to normalise purecap version against hybrid
    if self.cmd.arch in ["purecap"," benchmarkabi"] and "hybrid" not in results:
      sys.exit("Could not find hybrid results. Execute hybrid results before purecap and benchmarkabi")


    for _bm in run_bench:
      print(f"**** Running: {_bm} ****")
      self.__execute( env_libpath, f'{_bm}', self.cmd.args.repeat, self.cmd.out_data_dir, results)
      print(f"**** Done running {_bm} ****")
    else : 
      with open(_outfile, "w") as fd:
        json.dump(results, fd, indent=2,sort_keys=True)



  def _sanitize_basedir(self, rconn_param):
    if len(rconn_param) > 1 : 
      if rconn_param[1] == "" : # empty basedir was specified with remote connect params
        rconn_param.pop()
    return rconn_param
        

  # Link libraries in new directory
  def __link_remote_lib(self, rconn_params, libs):
    for _lib in libs:
      _libname = _lib.name.split(sep='.' , maxsplit=1)[0]
      ret = subprocess.run( ['ssh', '-p', self.cmd.sshport, rconn_params[0] , \
                             f'cd {self.append_basedir(rconn_params)}'
                             f'{self.arch_rdir}; ' 
                             f'ln -fs {_lib.name} {_libname}.so.1; '
                             f'ln -fs {_libname}.so.1 {_libname}.so;'], check=True)
      ret.check_returncode()

  def __execute(self, libpath, bm, repeat, host_data_dir, results):
    _rconn_param = self._sanitize_basedir(self.cmd.args.remoteconnect.split(sep=':',maxsplit=1))
    prologue = f'cd {self.append_basedir(_rconn_param)}' \
               f'{self.arch_rdir}; '

    _outfile = pathlib.Path(self.cmd.args.output)
    # Retrieve benchmark log from server for this run
    bm_p = pathlib.Path(bm.split()[0]).stem

    for _idx in range(1, self.cmd.args.repeat+1):
      print(f"{_idx}...",end="")
      # Execute benchmark on remote server 
      _tmpfile_time = pathlib.Path(f'{host_data_dir}/{self.cmd.args.output}.time')
      _tmpfile_time.unlink(True)

      # Add shim libs if required
      if self.cmd.args.binlinkoption == 'benchlib':
        #_exec_libpath = f'{libpath.rstrip()}:$(pwd)/lib{bm}_shim.so'
        _exec_libpath = f'{libpath}'
      else:
        _exec_libpath = f'{libpath}'

      with open( _tmpfile_time, "w") as _tmpfile_time_fd: 
        ret = subprocess.run( ['ssh', '-p', self.cmd.sshport, _rconn_param[0] , \
                               prologue + f'{_exec_libpath} time -l ./{bm}'], \
                               stderr=_tmpfile_time_fd, check=True)
        ret.check_returncode()


      _tmpfile = f'{host_data_dir}/{self.cmd.arch}_{_outfile.stem}_{bm_p.split()[0]}_{_idx:02}{"".join(_outfile.suffixes)}'
      # Copy logfile from install directory to remote-install dir
      ret = subprocess.run( ['scp', '-P', self.cmd.sshport, 
                             f'{_rconn_param[0]}:{self.append_basedir(_rconn_param)}'
                             f'{self.arch_rdir}/{self.cmd.args.output}',
                             _tmpfile], check=True)
      ret.check_returncode()

      # Execute benchmark on remote server with pmc hardware events
      _tmpfile_pmc = pathlib.Path(f'{host_data_dir}/{self.cmd.args.output}.pmc')
      _tmpfile_pmc.unlink(True)
      with open( _tmpfile_pmc, "w") as _tmpfile_pmc_fd:
        ret = subprocess.run( [ 'ssh', '-p', self.cmd.sshport, _rconn_param[0] , \
                               prologue + f'{_exec_libpath} pmcstat -d -w {pmc_timeout} ' + \
                               ' '.join([ f'-p {_event}' for _event in pmc_events]) + f' ./{bm}'], \
                               stderr=_tmpfile_pmc_fd, check=True)
        ret.check_returncode()

      with open(_tmpfile, "r") as fd:
        _data = json.load(fd)
        self.__extract_time_results(_data, pathlib.Path( _tmpfile_time))
        self.__extract_pmc_results(_data, pathlib.Path( _tmpfile_pmc))
        self.__append_results(results, _data, bm_p, _idx)
    else:
        _mean = lambda _arr: np.mean(_arr) if np.count_nonzero(_arr) == _arr.size else 0.0
        _gmean = lambda _arr : np.exp(np.log(_arr).mean()) if np.count_nonzero(_arr) == _arr.size else 0.0
        results[self.cmd.arch][bm_p]["gc-cycles"] = _mean(np.array(results[self.cmd.arch][bm_p]["raw-gc-cycles"])) 
        results[self.cmd.arch][bm_p]["gc-time"] = _mean(np.array(results[self.cmd.arch][bm_p]["raw-gc-time"]))
        results[self.cmd.arch][bm_p]["total-time"] = _mean(np.array(results[self.cmd.arch][bm_p]["raw-total-time"]))
        results[self.cmd.arch][bm_p]["rss-kb"] = _mean(np.array(results[self.cmd.arch][bm_p]["raw-rss-kb"]))
        if results[self.cmd.arch][bm_p]["total-time"] > 0.0: 
          results[self.cmd.arch][bm_p]["gc-load"] = results[self.cmd.arch][bm_p]["gc-time"] / results[self.cmd.arch][bm_p]["total-time"]
          self.normal(results, bm_p, "total-time") 

        self.normal(results, bm_p, "gc-cycles") 
        self.normal(results, bm_p, "gc-time") 
        self.normal(results, bm_p, "gc-load")
        self.normal(results, bm_p, "rss-kb") 

        for _event in pmc_events:
          results[self.cmd.arch][bm_p][_event] = _gmean(np.array(results[self.cmd.arch][bm_p][f"raw-{_event}"]))
          self.normal(results, bm_p, _event) 
        print(f"\n total_time:{results[self.cmd.arch][bm_p]['total-time']}, gc-time:{results[self.cmd.arch][bm_p]['gc-time']}")

  def normal(self, results, bm,  event):
    if results["hybrid"][bm][event] > 0.0 :
      results[self.cmd.arch][bm][f"normalised-{event}"] = results[self.cmd.arch][bm][event] / results["hybrid"][bm][event]


  # Extract Total time and RSS figures
  # From first run screen scraping from `time -l`
  def __extract_time_results(self, result, time_log):
    _lines = None
    with open(time_log) as fd: 
      _lines = fd.readlines()

    # _total_time =  _lines[0].strip().split()[2]   # user time 
    _total_time =  _lines[0].strip().split()[0]   # real time 
    assert _lines[0].strip().split()[3] == "user", f"file {time_log} does not have user time"
    result["total_time_ms"] = _total_time

    _c_lines = [(_ln.strip().split()[0], "-".join(_ln.strip().split()[1:])) for _ln in _lines[1:]]
    assert _c_lines[0][1] == "maximum-resident-set-size", f"file {time_log} does not have max resident set size"
    result["rss_kb"] = _c_lines[0][0]

  # Extract PMC HW events monitor
  # From first run screen scraping from `pmcstat`
  def __extract_pmc_results(self, result, pmc_log):
    _lines = None
    with open(pmc_log) as fd: 
      _lines = fd.readlines()
    assert _lines != None, f"Could not open pmc-file {pmc_log}"


    _metrics = [ _evt[len("p/"):] for _evt in _lines[0].strip().split()[1:]]
    _values =  _lines[1].strip().split()

    for _evt, _val in zip(_metrics, _values):
      result[_evt] = _val

  def __append_results(self, result, new_data, bm, run):
    if self.cmd.arch not in result:
      result[self.cmd.arch] = {} 
      
    if new_data["bm"] not in result[self.cmd.arch]: 
      result[self.cmd.arch][new_data["bm"]] = { "raw-gc-cycles" : [], 
                                                "gc-cycles" : 0.0, 
                                                "normalised-gc-cycles" : 0.0, 
                                                "raw-gc-time" : [], 
                                                "gc-time" : 0.0,
                                                "normalised-gc-time" : 0.0,
                                                "raw-total-time" : [], 
                                                "total-time" : 0.0,
                                                "normalised-total-time" : 0.0,
                                                "raw-rss-kb" : [], 
                                                "rss-kb" : 0.0, 
                                                "normalised-rss-kb" : 0.0, 
                                                "gc-load" : 0.0,
                                              } 
      for _event in pmc_events:
        result[self.cmd.arch][new_data["bm"]].setdefault(f"raw-{_event}", [])
        result[self.cmd.arch][new_data["bm"]].setdefault(f"{_event}", 0.0)
        result[self.cmd.arch][new_data["bm"]].setdefault(f"normalised-{_event}", 0.0)

    result[self.cmd.arch][new_data["bm"]]["raw-gc-cycles"] += [float(new_data["gc_cycles"])]
    result[self.cmd.arch][new_data["bm"]]["raw-gc-time"] += [float(new_data["gc_time_ms"])]
    result[self.cmd.arch][new_data["bm"]]["raw-total-time"] += [float(new_data["total_time_ms"]) * 1000]
    result[self.cmd.arch][new_data["bm"]]["raw-rss-kb"] += [int(new_data["rss_kb"])]

    # add all pmc hw events 
    for _event in pmc_events:
      result[self.cmd.arch][new_data["bm"]][f"raw-{_event}"] += [int(new_data[_event])]

class CommandLine:
  def __init__(self, name=None, desc=None, epilogue=None):
    self.parser = (name, desc, epilogue)
    self.__gen_options()

    self.args = self.parser.parse_args()
    self.arch = self.args.march
    self.sshport = self.args.sshport
    self.libs = self.args.libs

    self.buildtype = self.args.buildtype
    self.workdir = self.args.workdir
    self.build_dir = f"build-bench-{self.arch}"
    self.install_dir = f"install-bench-{self.arch}"
    self.out_data_dir = "output-data"
    self.dependency_dir = "cheribuild"

  @property
  def parser(self):
    return self._parser

  @parser.setter
  def parser(self, prog):
    self._parser = argparse.ArgumentParser(
                              prog = sys.argv[0] if prog[0] is None else prog[0] , 
                              description = prog[1] if prog[1] is not  None else 
                                                f"This test harness builds bdwgc for the requisite -march. "
                                                f"Each benchmark is linked with cheri-bdwgc library and installed in "
                                                f"the --workdir directory. The installation can optionally "
                                                f"build the cheribuild toolchain, download the bdwgc code and the benchmarks "
                                                f"execute and collate all the results in an output json file. "
                                                f"It will also optionally plot the graphs as pdf files",
                              epilog = prog[2] if prog[2] is not None else 
                                         f"Morello toolchains can be downloaded and installed "
                                         f"using cheribuild (https://github.com/CTSRD-CHERI/cheribuild)")

  @property
  def workdir(self):
    return self._workdir

  @workdir.setter
  def workdir(self, path):
    self._workdir = pathlib.Path(path).resolve()
    if not self._workdir.exists(): 
      print(f"{self._workdir} does not exist. Creating .... ")
    assert self.__check_outside_cwd(self._workdir), f"-w {path} should be outside source path"
    self._workdir.mkdir(mode=0o750, parents=True, exist_ok=True)

  @property
  def build_dir(self):
    return self._build_dir

  @build_dir.setter
  def build_dir(self, builddir):
    self._build_dir = self.workdir / builddir
    self._build_dir.mkdir(mode=0o750, parents=True, exist_ok=True)
  
  @property
  def install_dir(self):
    return self._install_dir

  @install_dir.setter
  def install_dir(self, installdir):
    self._install_dir = self.workdir/ installdir
    self._install_dir.mkdir(mode=0o750, parents=True, exist_ok=True)

  @property
  def out_data_dir(self):
    return self._out_data_dir

  @out_data_dir.setter
  def out_data_dir(self, path):
    self._out_data_dir = self.workdir / path
    self._out_data_dir.mkdir(mode=0o750, parents=True, exist_ok=True)

  @property
  def dependency_dir(self):
    return self._dependency_dir

  @dependency_dir.setter
  def dependency_dir(self, dep_dir):
    self.args = self.parser.parse_args()
    if "toolchain" in self.args.action:
      self._dependency_dir = self.workdir/ dep_dir
      self._dependency_dir.mkdir(mode=0o750, parents=True, exist_ok=True)
    else:
      self._dependency_dir = pathlib.Path("")

  def __gen_options(self):
    self.parser.add_argument('-m', '--march', help=f"morello architecture to build bdwgc for",
                            choices = ['hybrid','purecap', 'benchmarkabi'],
                            required = True)
    self.parser.add_argument('-w', '--workdir', required=True,
                                help=f"directory outside source-dir to build and install benchmarks")
    self.parser.add_argument('-t', '--buildtype',
                             choices = ['Debug','RelWithDebInfo','Release'],
                             default=f"RelWithDebInfo",
                             help=f"Build options to pass to cmake")
    self.parser.add_argument('-l', '--libs', nargs='*',
                             choices = [ _lib for _lib in lib_category ], 
                             default = ['bdwgc'],
                             help=f"set of allocator libs to execute code against")
    self.parser.add_argument('-a', '--action', nargs='*',
                             choices = ['toolchain', 'add-benchmark-abi', 'build', 'install', 'rinstall', 'rexec', 'plot'], 
                             default = ['build'], 
                             help=f"actions to execute. These may be combined together to perform a sequence of events")
    self.parser.add_argument('-r', '--remoteconnect', 
                             default = 'test@morello.dcs.gla.ac.uk:bench-test',
                             help=f"ssh auth-string with optional remote dir <test@morello.dcs.gla.ac.uk:bench-test>")
    self.parser.add_argument('-n', '--repeat', default = 5, type=int, 
                             help=f"number of times to repeat each benchmark for avg")
    self.parser.add_argument('-p', '--sshport', default="22",
                             help=f"ssh port to connect to. Useful for port forwarding")
    self.parser.add_argument('-b', '--binlinkoption', nargs='?',
                             default = 'dynamic',
                             const = 'dynamic',
                             choices = ['static','dynamic','benchlib'],
                             help=f"Compile and link option (static exec vs dynamic shared-lib")
    self.parser.add_argument('-o', '--output', 
                             default = 'out.json', 
                             help=f"output-data file name. This will be found in the directory specified with -w")
    self.parser.add_argument('-v', '--verbose', action='store_true',
                             default=False, help=f"print parsed cmdline options")

  # We assume that the build and install directories is out of source
  def __check_outside_cwd(self, workdir):
    curr_wd = pathlib.Path.cwd().resolve()
    workdir = workdir.resolve()

    if len(curr_wd.parts) > len(workdir.parts): 
      return True

    for curr_part, install_part in zip(curr_wd.parts,workdir.parts):
      if curr_part != install_part:
        break
    else :
      return False

    return True


def build_suite(repo):
  repo.configure()
  repo.build() 
  return

def install_suite(repo):
  repo.install() 
  return

def remote_install(repo):
  repo.remote_install() 
  return

def remote_exec(repo):
  global run_bench
  if 'bdwgc' not in repo.cmd.libs:
    run_bench += additional_benchmarks
  repo.remote_exec() 
  return

def build_dependencies(repo):
  repo.build_dependencies()
  return

def patch_bmabi_cfg(repo):
  repo.patch_bm_abi()
  return

def plot_result(repo):
  if 'bdwgc' in repo.cmd.libs:
    graphs_to_plot = (["gc-cycles","gc-time","total-time","rss-kb"] + pmc_events, ["gc-load"])
  else:
    graphs_to_plot = (["total-time","rss-kb"] + pmc_events, [])

  output_graph = pathlib.Path(f"{repo.cmd.out_data_dir}/{repo.cmd.args.output}")
  grplt.plot( "histogram", \
              pathlib.Path(f"{repo.cmd.out_data_dir}/{repo.cmd.args.output}"), \
              repo.cmd.out_data_dir / f"{output_graph.stem}.pdf", 
              graphs_to_plot, True, conf_interval=98)

def verbose(repo):
  print(f"--march                   : {repo.cmd.arch}")
  print(f"--repeat                  : {repo.cmd.args.repeat}")
  print(f"--workdir                 : {repo.cmd.workdir}")
  print(f"--libs                    : {repo.cmd.libs}")
  print(f"  | local build dir       : {repo.cmd.build_dir}")
  print(f"  | local install dir     : {repo.cmd.install_dir}")
  print(f"  | local dependency dir  : {repo.cmd.dependency_dir}")
  print(f"  | local output data dir : {repo.cmd.out_data_dir}")
  print(f"--buildtype               : {repo.cmd.buildtype}")
  print(f"--action                  : {repo.cmd.args.action}")
  print(f"--remoteconnect           : {repo.cmd.args.remoteconnect}")
  _rconn_param = repo._sanitize_basedir(repo.cmd.args.remoteconnect.split(sep=':',maxsplit=1))
  print(f"  | remote install dir    : " 
            f'{repo.append_basedir(_rconn_param)}'
            f'{repo.arch_rdir}')

  print(f"--sshport                 : {repo.cmd.sshport}")
  print(f"--output                  : {repo.cmd.args.output}")
  print(f"--binlinkoption           : {repo.cmd.args.binlinkoption}")


if __name__ == '__main__':
  repo = Build(CommandLine())

  if repo.cmd.args.verbose: 
    verbose(repo)

  if 'toolchain' in repo.cmd.args.action: 
    build_dependencies(repo)
  if 'add-benchmark-abi' in repo.cmd.args.action: 
    patch_bmabi_cfg(repo)
  if 'build' in repo.cmd.args.action: 
    build_suite(repo)
  if 'install' in repo.cmd.args.action: 
    install_suite(repo)
  if 'rinstall' in repo.cmd.args.action: 
    remote_install(repo)
  if 'rexec' in repo.cmd.args.action: 
    remote_exec(repo)
  if 'plot' in repo.cmd.args.action: 
    plot_result(repo)

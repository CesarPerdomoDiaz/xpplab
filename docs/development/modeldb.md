# ModelDB models

The example models that come with XPPAUT are the engine's regression tests,
and XPP Lab reproduces them bit for bit. Researchers' own models are a
larger and messier body of work. [ModelDB](https://modeldb.science) lists
XPPAUT as the simulator of 143 published models, and they are the test of
the promise that *your models just work*.

## Running the check

```bash
python -I tools/fetch_modeldb.py
python -I tests/modeldb_check.py --exe build/cli/xppaut-headless.exe --reference ../refbuild/xppaut-ref.exe
```

`fetch_modeldb.py` downloads each model's zip from ModelDB and unpacks it
into `build-modeldb/models/<id>/`. The files are not committed, because
every model carries its own licence. `tests/modeldb/manifest.json` records what was
fetched: the zip's SHA-256 and, for each model file and each table or
include file it reads, its name, URL and SHA-256. Later runs check the
downloads against it, and `--refresh` asks ModelDB again.

`modeldb_check.py` runs every model file in batch mode
(`-silent -outfile`) with XPP Lab's `xppaut-headless` and with
unmodified XPPAUT 8.0, each in a fresh copy of the model's folder, and
compares the output bit for bit. The reference is XPPAUT built from the
repository's first commit: on Windows against the null Xlib, on Linux with
`tests/build_reference.sh`. The ModelDB workflow
(`.github/workflows/modeldb.yml`, run by hand) does the same on Linux.
Results go to `build-modeldb/results.json`.

Each model file ends up in one of these classes:

| Class | Meaning |
|---|---|
| identical | Both ran and wrote the same numbers |
| fixed | They differ only because XPP Lab fixes an [upstream bug](../upstream-issues.md). Where it can, the check proves it by running XPPAUT again without the bug's trigger |
| differs | Both ran, the numbers differ |
| ours-fails | XPPAUT ran it, XPP Lab did not |
| ref-fails | XPP Lab ran it, XPPAUT did not |
| both-fail | Neither wrote output |

Some details matter for getting a fair comparison:

- **XPPAUT reads tables and include files relative to the current
  directory.** A model runs in its own folder, unless a table it names is
  only in a folder above it (model 150217 shares one `stim.tab` between five
  models); then it runs there, as its authors did.
- **An `@ output=` line overrides `-outfile`**, and a range run
  (`@ range=1`) writes `out.0`, `out.1`, ...; the check reads those.
- **Not every model is a `.ode` file.** Three are `.ode.txt` or `.txt`
  files, one a `.xpp` file exported by BioModels, one is inside a zip
  within the zip. The fetcher looks inside text files for XPP syntax.

## The downloads are untrusted

The fetcher and the check treat the models as data:

- Every model gets its own folder. Unpacking refuses absolute paths, `..`,
  symbolic links, names Windows cannot hold, files over 100 MB, and
  anything that is a program, script or library (by extension, and by the
  first bytes for Windows, Linux and Mac executables). This matters:
  **XPPAUT calls `system("clear")` every time it loads a model**, and
  Windows' `cmd.exe` looks for `clear.bat` or `clear.exe` in the current
  directory first. It also `dlopen()`s the library an `export` statement
  names, from the current directory.
- The check copies each model to a temporary folder and refuses to run it
  if a program or library is there.
- Run both scripts with `python -I`, so nothing in the current directory
  can shadow a Python module.

## Results

Run on 24 October 2026 on Windows, against unmodified XPPAUT 8.0 built
with the null Xlib, with a time limit of 10 minutes per run. 136 of the
143 ModelDB entries hold model files, 250 in all:

| Class | Model files |
|---|---|
| identical | 234 |
| fixed | 4 |
| differs | 0 |
| ours-fails | 0 |
| ref-fails | 0 |
| both-fail | 8 |
| too-slow | 4 |

**No model gives different numbers in XPP Lab, and none fails in XPP
Lab that runs in XPPAUT.** Four that crash XPPAUT now run. The other
twelve fail or time out the same way in both, for reasons in the models or
in XPPAUT 8.0's rules.

### Models that need attention

| ModelDB | Model file | Class | Why |
|---|---|---|---|
| [112079](https://modeldb.science/112079) | `NG108-ACO/NG108_Acon.ode` | fixed | `init ... inhibitedA=1.0, D=0.1, ...`: a 10-character name in an `init` statement overruns a buffer in XPPAUT (`compile_em()`), which crashes. XPP Lab runs it with every initial value set |
| [136097](https://modeldb.science/136097) | `mouse_ORN_cilia_ModelDB.ode`, `mouse_ORN_whole_cell_ModelDB.ode`, `mouse_ORN_whole_cell_osc_ModelDB.ode` | fixed | `meth=cvode` with `dt=0.00001` below `dtmin=0.0001`: XPPAUT stops the run at once and then crashes freeing CVODE memory it never allocated. XPP Lab stops with XPPAUT's own "Integration not completed". To run them, lower `dtmin` or raise `dt` |
| [64285](https://modeldb.science/64285) | `kuzkopwil2006/KuzKopWil2006.ode` | both-fail | `@ bell=off`: XPPAUT 8.0 accepts only `bell=0` or `bell=1` and stops |
| [266636](https://modeldb.science/266636) | `TabakEtAl2011/JNS_11b.ode.txt` | both-fail | `@ bell=off`, as above |
| [79238](https://modeldb.science/79238) | `muscleAP/muscleAP.ode` | both-fail | Names longer than XPPAUT's limit of 10 characters (`alpha_n_max`). The first formula that uses one fails: "Error allocating or compiling ALPHA_N" |
| [105528](https://modeldb.science/105528) | `SkM_AP/SkM_AP_KCa.ode` | both-fail | Names longer than 10 characters, as above |
| [120246](https://modeldb.science/120246) | `RatAP/RatAPtest01.ode` | both-fail | Names longer than 10 characters (`k_minus_htrpn`): "Error allocating or compiling J_TRPN" |
| [185334](https://modeldb.science/185334) | `BoseRubin2015/doublebvp.ode` | both-fail | A comment ends in `ghat \equiv 0`. XPPAUT continues a line at *any* backslash, so the comment swallows the next line, `vhat'=P*(I-vhat)`, and 8 equations meet 9 boundary conditions: "Too many boundary conditions". Without the backslash both run and agree. See [Upstream issues](../upstream-issues.md#open) |
| [167715](https://modeldb.science/167715) | `sd/sd-wild-type.ode` | both-fail | `@ maxstor=1000000000` reserves storage for a billion time points before the run; whether that succeeds depends on the free memory ("Cannot allocate sufficient storage"). Its twin `sd-mutation.ode` got the memory in this run and agrees |
| [267710](https://modeldb.science/267710) | `XPPAUT Model/6cModel Xppaut/main.ode` | both-fail | `@ output=../simulations/xpp.dat`: the `simulations` folder is not in the download. With it created, both run and agree |
| [112968](https://modeldb.science/112968) | `xppcodes/Fig5Bbot.ode`, `xppcodes/Fig5Bup.ode` | too-slow | 580 stiff equations; `Fig5Bbot.ode` alone was still running after 30 minutes |
| [149162](https://modeldb.science/149162) | `LiuEtAl2013/LiuEtAl2013.ode` | too-slow | Still running after 10 minutes in both |
| [235377](https://modeldb.science/235377) | `terman-ModelDB/waves.ode` | too-slow | Still running after 10 minutes in both |

The other four `Fig5` models of 112968 finish within the limit and agree.

### Entries without model files

Seven entries list XPP only as a web link and hold no model files on
ModelDB: 91893, 91898 and 91899 (pituitary models on Richard Bertram's
site), 118020 (DSSRT), 120320 (BioModels), 125649 (NeuronetExperimenter)
and 141835 (the models of *Mathematical Foundations of Neuroscience*, on
Bard Ermentrout's site). They could be fetched from those sites in a later
version of the check.

### What could make more models run

These are not engine bugs, so the engine keeps XPPAUT's behaviour, but the
desktop app could help:

- say which name is too long, instead of "Error allocating or compiling";
- accept `bell=off` and `bell=on`, which describe a beep and change no
  result;
- warn about a backslash inside a comment.

## Bugs this found

Running ModelDB's models found three upstream bugs, now fixed and listed in
[Upstream issues](../upstream-issues.md), and one surprising behaviour
listed there as open (any backslash continues a line, model 185334):

- **A 10-character name in an `init` statement overruns the stack**
  (`compile_em()`, `name[10]`). XPPAUT allows names of 10 characters, and
  the buffer had no room for the terminating zero. Optimised builds crash;
  unoptimised ones silently drop the initial values that follow on the
  line. Model 112079 (`init ... inhibitedA=1.0, D=0.1, ...`) crashed in
  XPPAUT and now runs with every initial value set.
- **CVODE with `dt` below `dtmin` crashes** (`end_cv()` frees memory that
  was never allocated). The three mouse olfactory receptor models of 136097
  now stop with XPPAUT's own "Integration not completed".
- **Exported variables read uninitialised memory when the library is
  missing** (`add_export_list()`). Found because the example models
  `efmx.ode`, `koho.ode`, `tstdll.ode` and others gave different results
  from run to run in a checkout without their `.so` files.

# Commands and keyboard shortcuts

## Keyboard shortcuts

| Keys | Action |
|---|---|
| F5 | Integrate (run) |
| Ctrl+Enter | Apply the edited model |
| Ctrl+O, Ctrl+S, Ctrl+N | Open, save, new model |
| Ctrl+Q | Quit (⌘ instead of Ctrl on a Mac) |
| F1 | Help |

## XPPAUT's two-key commands

XPPAUT users drive it with two-letter commands, and those keep working
here whenever the cursor is not in a text field. They can be switched off
under **View > XPPAUT keyboard commands**.

| Keys | XPPAUT menu | Action |
|---|---|---|
| I G | Initial conds > Go | Integrate |
| I L | Initial conds > Last | Start from the end of the last run |
| N N | Nullcline > New | Draw nullclines |
| N E | Nullcline > Erase | Hide nullclines |
| D D | Dir.field > Direct field | Toggle the direction field |
| S G | Sing pts > Go | Find an equilibrium |
| E | Erase | Clear kept curves |
| W F | Window > Fit | Fit the plots (or double-click a plot) |
| F Q | File > Quit | Quit |

## The command window

Type a command and press Enter. Use the Up and Down arrows to recall earlier
commands. `help` lists them all.

| Command | Action |
|---|---|
| `run` | Integrate. |
| `set I=0.5 total=200` | Set parameters, initial values or options. |
| `get I` | Show a value. |
| `whos` | List variables and parameters with their values. |
| `ic last` | Start the next run where the last one ended. |
| `eq` | Find an equilibrium and report its stability. |
| `nullclines`, `nullclines off` | Show or hide nullclines. |
| `dirfield` | Toggle the direction field. |
| `erase` | Remove kept curves. |
| `use hopf` | Apply a named parameter set. |
| `auto steady iapp` | Follow steady states as iapp varies. |
| `auto from 2 periodic` | Continue from labelled point 2. Actions: `periodic`, `two`, `extend`, `switch`, `doubling`, `fixed`. |
| `auto set ds=0.01 nmax=300` | AUTO numerics. |
| `auto reset` | Clear the diagram. |
| `sweep I 0 1.6 50 V` | Run 50 times with I from 0 to 1.6 and plot the max and min of V (XPPAUT's Range). Leave out `V` to keep each trajectory instead. |
| `spectrum V 1024` | Power spectral density of V in windows of 1024 points. `spectrum V W coherence` compares two columns (`psd`, `cross`, `coherence`). |
| `fourier V`, `power V` | Fourier series of V over the run: cos and sin, or amplitude and phase. |
| `hist V 50 -1 1` | Histogram of V in 50 bins from -1 to 1 (leave out the range to take it from the data). |
| `hist2d V W 20` | Two-dimensional histogram of V and W. |
| `xcorr V W 100` | Cross-correlation of V and W over about 100 lags; `xcorr V` is the autocorrelation. |
| `mean V` | Mean and standard deviation of V, and the run's largest Liapunov exponent. |
| `prc` | Phase response curves of the limit cycle (`prc 500` settles for 500 time units first). |
| `hfun V'-V; 0` | H function for weak coupling, one formula per variable; primed names are the other cell. |
| `fitdata v.dat V 2 gca,gk` | Fit gca and gk so that V matches column 2 of v.dat. |
| `open file.ode`, `new`, `save` | Model files. |
| `export run.csv` | Save the last run as CSV. |
| `export fig.svg phase` | Save a figure (`time`, `phase`, `auto`, `auto2`) as SVG; `.png` saves it as shown. |
| `continue 50` | Integrate 50 more time units from the end of the last run (C C). |
| `clc` | Clear the command window. |
| `help auto` | Open a manual page. |

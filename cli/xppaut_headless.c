/*
 * The original XPPAUT 8.0 entry point, linked against the X11-free engine.
 * It accepts every XPPAUT command-line option, but only batch mode
 * (-silent) is useful because there is no window system behind it.
 */
int xpp_upstream_main(int argc, char **argv);

int main(int argc, char **argv) { return xpp_upstream_main(argc, argv); }

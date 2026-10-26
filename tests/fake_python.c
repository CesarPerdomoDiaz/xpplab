/* A stand-in for the Python that python fetch downloads, for
   tests/pyfetch_check.py, which packs it as the archive's python. It answers
   pip itself (nothing to install: the test runs without the internet) and
   hands everything else to the Python in $XPPLAB_TEST_REAL_PYTHON, with the
   same arguments, input and output (and that Python's user packages), and
   returns its exit status. On Windows it is a window program, as
   pythonw.exe is. */
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <stdlib.h>
#include <wchar.h>

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdline, int show) {
  wchar_t real[1024], *cmd = GetCommandLineW(), *rest = cmd, *line, **argv;
  STARTUPINFOW si;
  PROCESS_INFORMATION pi;
  DWORD status = 1;
  int argc, i;
  size_t n;
  (void)inst, (void)prev, (void)cmdline, (void)show;
  argv = CommandLineToArgvW(cmd, &argc);
  for (i = 1; argv && i + 1 < argc; i++)
    if (wcscmp(argv[i], L"-m") == 0 && wcscmp(argv[i + 1], L"pip") == 0) {
      static const char msg[] = "Requirement already satisfied (a test stand-in for pip)\n";
      DWORD written;
      WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, sizeof msg - 1, &written, NULL);
      return 0;
    }
  if (!GetEnvironmentVariableW(L"XPPLAB_TEST_REAL_PYTHON", real, 1024)) return 2;
  /* The real Python may keep its packages in the user's folder. */
  SetEnvironmentVariableW(L"PYTHONNOUSERSITE", NULL);
  /* The arguments as they came, after this program's own name. */
  if (*rest == L'"') {
    for (rest++; *rest && *rest != L'"'; rest++) {}
    if (*rest) rest++;
  } else {
    while (*rest && *rest != L' ' && *rest != L'\t') rest++;
  }
  n = wcslen(real) + wcslen(rest) + 4;
  line = malloc(n * sizeof *line);
  if (!line) return 3;
  swprintf(line, n, L"\"%ls\"%ls", real, rest);
  ZeroMemory(&si, sizeof si);
  si.cb = sizeof si;
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
  si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
  if (!CreateProcessW(NULL, line, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
    return 3;
  WaitForSingleObject(pi.hProcess, INFINITE);
  GetExitCodeProcess(pi.hProcess, &status);
  return (int)status;
}
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
  char *real = getenv("XPPLAB_TEST_REAL_PYTHON");
  int i;
  for (i = 1; i + 1 < argc; i++)
    if (strcmp(argv[i], "-m") == 0 && strcmp(argv[i + 1], "pip") == 0) {
      puts("Requirement already satisfied (a test stand-in for pip)");
      return 0;
    }
  if (!real) return 2;
  unsetenv("PYTHONNOUSERSITE"); /* the real Python may keep packages there */
  argv[0] = real; /* Python finds its library from its own name */
  execv(real, argv);
  return 3;
}
#endif

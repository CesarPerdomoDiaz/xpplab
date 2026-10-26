/* The SHA-256 that checks the console's downloaded Python (app/src/sha256.c)
   against the test vectors of FIPS 180-4 and NIST's examples, fed whole
   and in pieces of awkward sizes. */
#include <stdio.h>
#include <string.h>

#include "sha256.h"

static int failures;

static void digest_of(const char *data, size_t n, size_t piece, char out[65]) {
  Sha256 s;
  size_t k;
  sha256_init(&s);
  for (k = 0; k < n; k += piece) sha256_update(&s, data + k, n - k < piece ? n - k : piece);
  sha256_hex(&s, out);
}

static void check(const char *what, const char *data, size_t n, const char *want) {
  static const size_t pieces[] = {1000000000, 1, 3, 55, 56, 63, 64, 65, 997};
  size_t i;
  char got[65];
  for (i = 0; i < sizeof pieces / sizeof pieces[0]; i++) {
    digest_of(data, n, pieces[i], got);
    if (strcmp(got, want) != 0) {
      printf("FAIL  %s (in pieces of %u): %s, not %s\n", what, (unsigned)pieces[i], got, want);
      failures++;
      return;
    }
  }
  printf("ok    %s\n", what);
}

int main(void) {
  static char million[1000000];
  const char *two_blocks = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  const char *long_one = "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
                         "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu";
  check("the empty message", "", 0,
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  check("\"abc\"", "abc", 3,
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  check("448 bits (padding in a second block)", two_blocks, strlen(two_blocks),
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
  check("896 bits", long_one, strlen(long_one),
        "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1");
  memset(million, 'a', sizeof million);
  check("a million a's", million, sizeof million,
        "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
  if (failures) printf("%d failure(s)\n", failures);
  return failures ? 1 : 0;
}

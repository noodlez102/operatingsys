#include "../tls.h"
#include <assert.h>

#define TLS_SIZE 5

int main() {
  assert(tls_create(TLS_SIZE) == 0);
  assert(tls_destroy() == 0);
}

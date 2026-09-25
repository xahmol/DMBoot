/*
Host test harness: builds DMBoot C modules with gcc as they behave under
Oscar64 on the C128: 32-bit long, packed structures (-fpack-struct=1),
bool built in. Include after all system headers.
*/
#include <stdbool.h>
#define long int

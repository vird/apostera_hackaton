#pragma once
// NOTE for strdup support
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>

#include "type.h"
#include "time.c"

//experimental

#define CLEAR(x) memset(&(x), 0, sizeof(x))


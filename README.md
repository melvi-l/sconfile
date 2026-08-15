# sconfile

`sconfile.h` ~~is~~ will be a single-header C library built around a minimal [S-expression](https://en.wikipedia.org/wiki/S-expression) parser 
shared by both the metaprogramming and runtime configuration layers.

Type definitions, enums and constraints such as ranges are written as S-expression data.
A build-time metagenerator parses these descriptions and generates the corresponding C types and
decoding functions.

Runtime configuration is stored in dedicated S-expression files. The same parser is used to read
these files and populate the generated C structures while enforcing the constraints defined by the 
type description.

The S-expression parser itself has no knowledge of configuration semantics. It only produces 
generic nodes; type generation, validation and config decoding are implemented as separate semantic
passes over the same representation.

```c
#define SCONFILE_IMPLEMENTATION
#include "sconfile.h"
```
No external parser, serializer, reflection system or runtime schema dependency.


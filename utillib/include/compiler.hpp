#pragma once

// Compiler-related stuff and will probably be mostly warning-handling
#define PUSH_WARN _Pragma( "GCC diagnostic push" )
#define POP_WARN _Pragma( "GCC diagnostic pop" )
#define WARN_DISABLE_SUBOBJECT_LINKAGE _Pragma( "GCC diagnostic ignored \"-Wsubobject-linkage\"" )

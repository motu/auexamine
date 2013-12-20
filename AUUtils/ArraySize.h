//
// Copyright (c) 2013 MOTU, Inc. All rights reserved.
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file.
//

#ifndef ARRAYSIZE_DWA080398_H_
#define ARRAYSIZE_DWA080398_H_

// This version will compile (erroneously) if x is a pointer type
# define ARRAY_SIZE( x ) ( sizeof( x ) / sizeof( x[0] ) )

#define ARRAY_BEGIN( x ) (x)
#define ARRAY_END(x) (x + ARRAY_SIZE(x))

#endif // #include guard


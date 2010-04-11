README.TXT for the Embedthis HTTP Server.
--------------------------------------------------------------------------------

The filters directory contains the source for the loadable filters. Filters
participate in the request processing pipeline and transform requests or data.

The key files are:

File                 Purpose
----------------------------
authFilter.c     - Authentication filter. Implements basic and digest authentication.
chunkFilter.c    - Chunked transfer encoding filter.
rangeFilter.c    - Ranged requests filter.
uploadFilter.c   - Form-based file upload filter.
.makedep         - Makefile dependencies
Makefile         - Makefile to build all modules

--------------------------------------------------------------------------------

Copyright (c) 2003-2010 Embedthis Software, LLC. All Rights Reserved.
Embedthis and Appweb are trademarks of Embedthis Software, LLC. Other 
brands and their products are trademarks of their respective holders.

See LICENSE.TXT for software license details.

--------------------------------------------------------------------------------

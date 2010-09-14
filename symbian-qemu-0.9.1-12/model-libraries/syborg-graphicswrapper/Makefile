# Copyright (c) 2010 Symbian Foundation.
# All rights reserved.
# This component and the accompanying materials are made available
# under the terms of the License "Eclipse Public License v1.0"
# which accompanies this distribution, and is available
# at the URL "http://www.eclipse.org/legal/epl-v10.html".
#
# Description:
# Toplevel Makefile to build syborg-graphicswrapper library in either debug or release mode

.phony: all clean install distclean


# Ensure DEBUG_LIBS is defined to make a debug build
ifneq ($(DEBUG_LIBS),)
build_dir=Debug 
else
build_dir=Release 
endif

all clean install distclean:
	$(MAKE) -C $(build_dir) $@ -f ../target.mak



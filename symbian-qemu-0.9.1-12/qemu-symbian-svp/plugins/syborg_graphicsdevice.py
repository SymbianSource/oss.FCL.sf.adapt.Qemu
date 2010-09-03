#
# Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).
# All rights reserved.
# This component and the accompanying materials are made available
# under the terms of "Eclipse Public License v1.0"
# which accompanies this distribution, and is available
# at the URL "http://www.eclipse.org/legal/epl-v10.html".
#
# Initial Contributors:
# Nokia Corporation - initial contribution.
#
# Contributors:
#
# Description: syborg_graphicsdevice.py
#
# Represents a graphics device register interface for quest OS in QEMU Syborg environment.
#
#

import ctypes
import qemu
import sys
import platform

class syborg_graphicsdevice(qemu.devclass):
    # Graphics device registers derived from VirtualVideoInterfaceConstants.h
    VVI_R_ID                                = 0x0000
    VVI_R_IRQ_ENABLE                        = 0x0004
    VVI_R_IRQ_STATUS                        = 0x0008
    VVI_R_COMMAND                           = 0x000c
    VVI_R_PARAMETER_LOAD                    = 0x0010
    VVI_R_ERROR                             = 0x0014
    VVI_R_INPUT_BUFFER_TAIL                 = 0x0018
    VVI_R_INPUT_BUFFER_HEAD                 = 0x001c
    VVI_R_INPUT_BUFFER_READ_COUNT           = 0x0020
    VVI_R_INPUT_BUFFER_WRITE_COUNT          = 0x0024
    VVI_R_INPUT_BUFFER_MAX_TAIL             = 0x0028
    VVI_R_REQUEST_ID                        = 0x002c
    VVI_R_SHARED_CMD_MEMORY_BASE            = 0x0030
    VVI_R_SHARED_FRAMEBUFFER_MEMORY_BASE    = 0x0034
    VVI_R_LASTREG                           = 0x0038  # not a register, address of last register
    
    VVI_EXECUTE                             = 0
    shared_cmd_memory_base                  = 0
    shared_framebuffer_memory_base          = 0
    m_request_id_reg = 0

    host_os                             = platform.system()
    # List of operating systems for this device
    OS_WINDOWS                          = "Windows"
    OS_LINUX                            = "Linux"

    def create(self):
        print "syborg_graphicsdevice: running on ", self.host_os
        
        # Add the supported and validated operating systems to the condition below
        if( (self.host_os != self.OS_WINDOWS) ):
            error_msg = "syborg_graphicsdevice: os support not validated: ", self.host_os
            sys.exit( error_msg )

        # Try open the syborg graphicswrapper library
        try:
            if( self.host_os == self.OS_WINDOWS ):
                libname = "syborg-graphicswrapper.dll"
            elif( self.host_os == self.OS_LINUX ):
                libname = "syborg-graphicswrapper.so"
            else:
                # We should never end up here since the operating system check is done above
                sys.exit( "syborg_graphicsdevice: library loading failed. Os not supported!" )
            self.library = ctypes.CDLL(libname)
        except Exception, e:
            print repr(e)
            error_msg = "syborg_graphicsdevice: " + libname + " load failed";
            sys.exit( error_msg )

        # Create an instance of syborg graphics wrapper
        self.obj = self.library.create_SyborgGraphicsWrapper()
            
        self.library.initialize_SyborgGraphicsWrapper( self.obj )
        self.initialize_graphics_callback()

        self.irqenable = 0
        self.irqstatus = 0
        self.command = 0
        self.parameterload = 0

    def updateIrq(self,new_value):
        self.set_irq_level(0, new_value)

    def graphics_request_callback(self, request_id):
        #print "graphics_request_callback: " , request_id
        self.m_request_id_reg = request_id
        self.updateIrq(1)
        return 0
        
    def initialize_graphics_callback(self):
        self.CALLBACKFUNC = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_int)
        self.graphics_callback = self.CALLBACKFUNC(self.graphics_request_callback)
        self.library.set_GraphicsCallBack( self.obj, self.graphics_callback )
            
    def read_reg(self, offset):
        offset >>= 2
        if offset == self.VVI_R_ID:
            return 0xDEADBEEF
        elif offset == self.VVI_R_IRQ_ENABLE:
            return self.irqenable
        elif offset == self.VVI_R_IRQ_STATUS:
            return self.irqstatus
        elif offset == self.VVI_R_COMMAND:
            return self.command
        elif offset == self.VVI_R_PARAMETER_LOAD:
            return self.parameterload
        elif offset == self.VVI_R_ERROR:
            self.lasterror = 0
            return self.lasterror
        elif offset == self.VVI_R_INPUT_BUFFER_TAIL:
            return self.library.get_InputBufferTail( self.obj )
        elif offset == self.VVI_R_INPUT_BUFFER_HEAD:
            return self.library.get_InputBufferHead( self.obj )
        elif offset == self.VVI_R_INPUT_BUFFER_READ_COUNT:
            return self.library.get_InputBufferReadCount( self.obj )
        elif offset == self.VVI_R_INPUT_BUFFER_WRITE_COUNT:
            return self.library.get_InputBufferWriteCount( self.obj )
        elif offset == self.VVI_R_INPUT_BUFFER_MAX_TAIL:
            return self.library.get_InputMaxTailIndex( self.obj )
        elif offset == self.VVI_R_REQUEST_ID:
            return self.m_request_id_reg
        elif offset == self.VVI_R_SHARED_CMD_MEMORY_BASE:
            return self.shared_cmd_memory_base
        elif offset == self.VVI_R_SHARED_FRAMEBUFFER_MEMORY_BASE:
            return self.shared_framebuffer_memory_base
        else:
            reg_read_error = "syborg_graphicsdevice: Illegal register read at: ", offset 
            sys.exit( reg_read_error )

    def write_reg(self, offset, value):
        offset >>= 2
        if offset == self.VVI_R_IRQ_STATUS:
            self.updateIrq(0);
            self.library.signal_outputbuffer_semafore( self.obj )
            self.library.execute_command( self.obj );
        elif offset == self.VVI_R_COMMAND:
            if value == self.VVI_EXECUTE:
                self.library.execute_command( self.obj );
            else:
                sys.exit("syborg_graphicsdevice: Unknown command issued!")
        elif offset == self.VVI_R_INPUT_BUFFER_TAIL:
            self.library.set_InputBufferTail(  self.obj, value );
        elif offset == self.VVI_R_INPUT_BUFFER_HEAD:
            self.library.set_InputBufferHead( self.obj, value );
        elif offset == self.VVI_R_INPUT_BUFFER_READ_COUNT:
            self.library.set_InputBufferReadCount( self.obj, value );
        elif offset == self.VVI_R_INPUT_BUFFER_WRITE_COUNT:
            self.library.set_InputBufferWriteCount( self.obj, value );
        elif offset == self.VVI_R_INPUT_BUFFER_MAX_TAIL:
            self.library.set_InputMaxTailIndex( self.obj, value );
        elif offset == self.VVI_R_SHARED_CMD_MEMORY_BASE:
            gmemsize = self.library.get_cmd_memsize()
            self.cmd_memregion = qemu.memregion( value, gmemsize )
            self.memregion_cmd_base = self.cmd_memregion.region_host_addr()
        elif offset == self.VVI_R_SHARED_FRAMEBUFFER_MEMORY_BASE:
            gmemsize = self.library.get_framebuffer_memsize()
            self.framebuffer_memregion = qemu.memregion( value, gmemsize )
            self.memregion_framebuffer_base = self.framebuffer_memregion.region_host_addr()
            # Ready to finalise graphics initialization
            if( self.library.reset_SyborgGraphicsWrapper( self.obj, self.memregion_framebuffer_base, self.memregion_cmd_base ) != 0 ):
                sys.exit("syborg_graphicsdevice: Syborg graphicsutils library not initialized correctly!")
        else:
            reg_write_error = "syborg_graphicsdevice: Illegal register write to: ", offset 
            sys.exit( reg_write_error )

    # Device class properties
    regions = [qemu.ioregion(0x1000, readl=read_reg, writel=write_reg)]
    irqs = 1
    name = "syborg,graphicsdevice"
    properties = {}

qemu.register_device(syborg_graphicsdevice)

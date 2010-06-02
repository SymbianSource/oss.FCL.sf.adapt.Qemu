import ctypes
import qemu
import sys
#import SyborgModule

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

    # Memory base id's from SyborgModule.h
    # SYBORG_CMDMEMBASE = 0
    # SYBORG_FRAMEBUFFERMEMBASE = 1
    
    class DllLoadExeption( Exception ):
        def __init__(self,value):
            self.value = value
        
        def __str__(self):
            return repr(self.value)

    class SyborgGraphicsWrapper():
        def __init__(self):
            try:
                self.library = ctypes.CDLL("syborg-graphicswrapper.dll")
            except:
                raise syborg_graphicsdevice.DllLoadExeption(1)
            self.obj = self.library.create_SyborgGraphicsWrapper()

        def initialize_graphics_utils(self):
            self.library.initialize_SyborgGraphicsWrapper( self.obj )

    def create(self):
        try:
            self.graphicsutils = self.SyborgGraphicsWrapper()
        except syborg_graphicsdevice.DllLoadExeption:
            #print "syborg_graphicsdevice: Graphics dll load failed"
            sys.exit("syborg_graphicsdevice: Graphics dll load failed")
            
            
        self.graphicsutils.initialize_graphics_utils()
        self.initialize_graphics_callback()
        # deliver the graphics ram region
        # self.gmembase = self.graphicsutils.library.get_membase()

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
        self.graphicsutils.library.set_GraphicsCallBack( self.graphicsutils.obj, self.graphics_callback )
            
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
            return self.graphicsutils.library.get_InputBufferTail( self.graphicsutils.obj )
        elif offset == self.VVI_R_INPUT_BUFFER_HEAD:
            return self.graphicsutils.library.get_InputBufferHead( self.graphicsutils.obj )
        elif offset == self.VVI_R_INPUT_BUFFER_READ_COUNT:
            return self.graphicsutils.library.get_InputBufferReadCount( self.graphicsutils.obj )
        elif offset == self.VVI_R_INPUT_BUFFER_WRITE_COUNT:
            return self.graphicsutils.library.get_InputBufferWriteCount( self.graphicsutils.obj )
        elif offset == self.VVI_R_INPUT_BUFFER_MAX_TAIL:
            return self.graphicsutils.library.get_InputMaxTailIndex( self.graphicsutils.obj )
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
            self.graphicsutils.library.signal_outputbuffer_semafore( self.graphicsutils.obj )
            self.graphicsutils.library.execute_command( self.graphicsutils.obj );
        elif offset == self.VVI_R_COMMAND:
            if value == self.VVI_EXECUTE:
                self.graphicsutils.library.execute_command( self.graphicsutils.obj );
            else:
                sys.exit("syborg_graphicsdevice: Unknown command issued!")
        elif offset == self.VVI_R_INPUT_BUFFER_TAIL:
            self.graphicsutils.library.set_InputBufferTail(  self.graphicsutils.obj, value );
        elif offset == self.VVI_R_INPUT_BUFFER_HEAD:
            self.graphicsutils.library.set_InputBufferHead( self.graphicsutils.obj, value );
        elif offset == self.VVI_R_INPUT_BUFFER_READ_COUNT:
            self.graphicsutils.library.set_InputBufferReadCount( self.graphicsutils.obj, value );
        elif offset == self.VVI_R_INPUT_BUFFER_WRITE_COUNT:
            self.graphicsutils.library.set_InputBufferWriteCount( self.graphicsutils.obj, value );
        elif offset == self.VVI_R_INPUT_BUFFER_MAX_TAIL:
            self.graphicsutils.library.set_InputMaxTailIndex( self.graphicsutils.obj, value );
        elif offset == self.VVI_R_SHARED_CMD_MEMORY_BASE:
            gmemsize = self.graphicsutils.library.get_cmd_memsize()
            self.cmd_memregion = qemu.memregion( value, gmemsize )
            self.memregion_cmd_base = self.cmd_memregion.region_host_addr()
            #SyborgModule.post_address( self.memregion_cmd_base, self.SYBORG_CMDMEMBASE )
        elif offset == self.VVI_R_SHARED_FRAMEBUFFER_MEMORY_BASE:
            gmemsize = self.graphicsutils.library.get_framebuffer_memsize()
            self.framebuffer_memregion = qemu.memregion( value, gmemsize )
            self.memregion_framebuffer_base = self.framebuffer_memregion.region_host_addr()
            #SyborgModule.post_address( self.memregion_framebuffer_base, self.SYBORG_FRAMEBUFFERMEMBASE )
            # Ready to finalise graphics initialization
            if( self.graphicsutils.library.reset_SyborgGraphicsWrapper( self.graphicsutils.obj, self.memregion_framebuffer_base, self.memregion_cmd_base ) != 0 ):
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

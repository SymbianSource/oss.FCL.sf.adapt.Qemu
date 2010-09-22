// This is the main DLL file.

#include "platformtypes.h"
#include <KhronosAPIWrapper.h>
#include <guestvideodriverinterfaceconstants.h>  //Registers and enums 
#include <platformthreading.h>  //mutex
#include <graphicsvhwcallback.h>
#include "syborg-graphicswrapper.h"

SyborgGraphicsWrapper::SyborgGraphicsWrapper()
    {															
    }

SyborgGraphicsWrapper::~SyborgGraphicsWrapper()
    {
    delete m_wrapper;
    m_wrapper = 0;

    Psu::platform_release_semaphore(m_outputBufferSemaphore);
    }

int SyborgGraphicsWrapper::Reset( uint32_t *aGraphicsMemBase,  uint32_t *aCommandMemBase )
    {
    int ret = -1;
    if ( m_wrapper )
        {
        delete m_wrapper;
	    }
    m_wrapper = NULL;

    uint8_t *cmd_buffer = (uint8_t *)aCommandMemBase;
    uint8_t *frame_buffer = (uint8_t *)aGraphicsMemBase;
    if( (cmd_buffer != NULL) && (frame_buffer != NULL) )
        {
        m_wrapper = new KhronosAPIWrapper( (MGraphicsVHWCallback*)this,
            frame_buffer, &cmd_buffer[VVHW_INPUT_BASE], &cmd_buffer[VVHW_OUTPUT_BASE] );
        //Reset synchronisation mechanisms
        Psu::platform_release_semaphore(m_outputBufferSemaphore);
        Psu::platform_create_semaphore(m_outputBufferSemaphore, 1, 1);
        ret = 0;
        }
    else
        {
        ret = -1;
        }
    return ret;
    }

void SyborgGraphicsWrapper::LockOutputBuffer()
    {
    #ifdef KHRONOS_API_W_MULTITHREAD
    Psu::platform_wait_for_signal(m_outputBufferSemaphore);
    #endif
    }

void SyborgGraphicsWrapper::ReleaseOutputBuffer(){}

void SyborgGraphicsWrapper::ProcessingDone(int i)
    {
    m_pythonCallBack( i );
    }

extern "C"
    {
    SYBORG_GRAPHICSWRAPPER_API SyborgGraphicsWrapper* create_SyborgGraphicsWrapper()
        {
        return new SyborgGraphicsWrapper();
        }
    SYBORG_GRAPHICSWRAPPER_API int initialize_SyborgGraphicsWrapper( SyborgGraphicsWrapper* aSyborgGraphicsWrapper )
        {
        Psu::platform_create_semaphore(m_outputBufferSemaphore, 1, 1);
        // Change to proper error handling
        return 0;
        }

    SYBORG_GRAPHICSWRAPPER_API int set_GraphicsCallBack( SyborgGraphicsWrapper* aSyborgGraphicsWrapper, int (*aGraphicsCallBack) (int) )
        {
        m_pythonCallBack = aGraphicsCallBack;
        // Change to proper error handling
        return 0;
        }

    SYBORG_GRAPHICSWRAPPER_API int reset_SyborgGraphicsWrapper(  SyborgGraphicsWrapper* aSyborgGraphicsWrapper, uint32_t *aGraphicsMemBase,  uint32_t *aCommandMemBase )
        {
        return aSyborgGraphicsWrapper->Reset( aGraphicsMemBase, aCommandMemBase );
        }

    SYBORG_GRAPHICSWRAPPER_API uint32_t get_InputBufferTail(  SyborgGraphicsWrapper* aSyborgGraphicsWrapper )
        {
        return m_wrapper->InputBufferTail();
        }
    SYBORG_GRAPHICSWRAPPER_API uint32_t get_InputBufferHead(  SyborgGraphicsWrapper* aSyborgGraphicsWrapper )
        {
        return m_wrapper->InputBufferHead( );
        }
    SYBORG_GRAPHICSWRAPPER_API uint32_t get_InputBufferReadCount(  SyborgGraphicsWrapper* aSyborgGraphicsWrapper )
        {
        return m_wrapper->InputBufferReadCount( );
        }
    SYBORG_GRAPHICSWRAPPER_API uint32_t get_InputBufferWriteCount(  SyborgGraphicsWrapper* aSyborgGraphicsWrapper )
        {
        return m_wrapper->InputBufferWriteCount( );
        }
    SYBORG_GRAPHICSWRAPPER_API uint32_t get_InputMaxTailIndex(  SyborgGraphicsWrapper* aSyborgGraphicsWrapper )
        {
        return m_wrapper->InputMaxTailIndex( );
        }
    SYBORG_GRAPHICSWRAPPER_API uint32_t get_cmd_memsize( void )
        {
        return (VVI_PARAMETERS_INPUT_MEMORY_SIZE +
                VVI_PARAMETERS_OUTPUT_MEMORY_SIZE);
        }
    SYBORG_GRAPHICSWRAPPER_API uint32_t get_framebuffer_memsize( void )
        {
        return VVI_FRAMEBUFFER_MEMORY_SIZE;
        }

    
    SYBORG_GRAPHICSWRAPPER_API unsigned int execute_command(  SyborgGraphicsWrapper* aSyborgGraphicsWrapper )
        {
        return m_wrapper->Execute( );
        }
    SYBORG_GRAPHICSWRAPPER_API void set_InputBufferTail(  SyborgGraphicsWrapper* aSyborgGraphicsWrapper, uint32_t aVal )
        {
        m_wrapper->SetInputBufferTail( aVal );
        }
    SYBORG_GRAPHICSWRAPPER_API void set_InputBufferHead(  SyborgGraphicsWrapper* aSyborgGraphicsWrapper, uint32_t aVal )
        {
        m_wrapper->SetInputBufferHead( aVal );
        }
    SYBORG_GRAPHICSWRAPPER_API void set_InputBufferReadCount(  SyborgGraphicsWrapper* aSyborgGraphicsWrapper, uint32_t aVal )
        {
        m_wrapper->SetInputBufferReadCount( aVal );
        }
    SYBORG_GRAPHICSWRAPPER_API void set_InputBufferWriteCount(  SyborgGraphicsWrapper* aSyborgGraphicsWrapper, uint32_t aVal )
        {
        m_wrapper->SetInputBufferWriteCount( aVal );
        }
    SYBORG_GRAPHICSWRAPPER_API void set_InputMaxTailIndex(  SyborgGraphicsWrapper* aSyborgGraphicsWrapper, uint32_t aVal )
        {
        m_wrapper->SetInputMaxTailIndex( aVal );
        }
    SYBORG_GRAPHICSWRAPPER_API void signal_outputbuffer_semafore(  SyborgGraphicsWrapper* aSyborgGraphicsWrapper )
        {
        #ifdef KHRONOS_API_W_MULTITHREAD
            Psu::platform_signal_semaphore(m_outputBufferSemaphore);
        #endif
        }

    }
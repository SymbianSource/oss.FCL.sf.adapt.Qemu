#ifndef _SYBORG_GRAPHICSWRAPPER_H
#define _SYBORG_GRAPHICSWRAPPER_H

#pragma once

#ifdef WIN32
#ifdef SYBORG_GRAPHICSWRAPPER_EXPORTS
#define SYBORG_GRAPHICSWRAPPER_API __declspec(dllexport)
#else
#define SYBORG_GRAPHICSWRAPPER_API __declspec(dllimport)
#endif
#else
#define SYBORG_GRAPHICSWRAPPER_API
#endif

Psu::PLATFORM_SEMAPHORE_T m_outputBufferSemaphore;
int (*m_pythonCallBack)(int); 
KhronosAPIWrapper* m_wrapper;


// Derived from GraphicsVirtualHW.lisa
static const int VVHW_BUFFER (0x3000000);
static const int VVHW_INPUT_BUFFER (0x1000000);
static const int VVHW_INPUT_BASE(0x0);
static const int VVHW_OUTPUT_BUFFER (0x1000000);
static const int VVHW_OUTPUT_BASE(0x1000000);
static const int VVHW_FRAME_BUFFER (0x1000000);
static const int VVHW_FRAME_BASE(0x2000000);

class SyborgGraphicsWrapper : public protocol_MGraphicsVHWCallback
    {
    public:

        SyborgGraphicsWrapper();
        ~SyborgGraphicsWrapper();

        int Reset( uint32_t *aGraphicsMemBase,  uint32_t *aCommandMemBase );

        virtual void LockOutputBuffer();
	    virtual void ReleaseOutputBuffer();
	    virtual void ProsessingDone(int i);

    private:
    };


#endif // _SYBORG_GRAPHICSWRAPPER_H
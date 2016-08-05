/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2016 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include "../../SDL_internal.h"

#if SDL_AUDIO_DRIVER_NACL

#include "SDL_naclaudio.h"

#include "SDL_audio.h"
#include "SDL_mutex.h"
#include "../SDL_audio_c.h"
#include "../SDL_audiodev_c.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi_simple/ps.h"
#include "ppapi_simple/ps_interface.h"
#include "ppapi_simple/ps_event.h"

/* The tag name used by NACL audio */
#define NACLAUD_DRIVER_NAME         "nacl"

#define SAMPLE_FRAME_COUNT 4096

/* Audio driver functions */
static void nacl_audio_callback(void* samples, uint32_t buffer_size, PP_TimeDelta latency, void* data);

/* FIXME: Make use of latency if needed */
static void nacl_audio_callback(void* samples, uint32_t buffer_size, PP_TimeDelta latency, void* data) {
    SDL_AudioDevice* _this = (SDL_AudioDevice*) data;
    
    SDL_LockMutex(private->mutex);  /* !!! FIXME: is this mutex necessary? */

    if (SDL_AtomicGet(&this->enabled) && !SDL_AtomicGet(&this->paused)) {
        if (_this->convert.needed) {
            SDL_LockMutex(_this->mixer_lock);
            (*_this->spec.callback) (_this->spec.userdata,
                                     (Uint8 *) _this->convert.buf,
                                     _this->convert.len);
            SDL_UnlockMutex(_this->mixer_lock);
            SDL_ConvertAudio(&_this->convert);
            SDL_memcpy(samples, _this->convert.buf, _this->convert.len_cvt);
        } else {
            SDL_LockMutex(_this->mixer_lock);
            (*_this->spec.callback) (_this->spec.userdata, (Uint8 *) samples, buffer_size);
            SDL_UnlockMutex(_this->mixer_lock);
        }
    } else {
        SDL_memset(samples, 0, buffer_size);
    }
    
    SDL_UnlockMutex(private->mutex);
}

static void NACLAUD_CloseDevice(SDL_AudioDevice *device) {
    const PPB_Core *core = PSInterfaceCore();
    const PPB_Audio *ppb_audio = PSInterfaceAudio();
    SDL_PrivateAudioData *hidden = (SDL_PrivateAudioData *) device->hidden;
    
    ppb_audio->StopPlayback(hidden->audio);
    SDL_DestroyMutex(hidden->mutex);
    core->ReleaseResource(hidden->audio);
}

static int
NACLAUD_OpenDevice(_THIS, void *handle, const char *devname, int iscapture) {
    PP_Instance instance = PSGetInstanceId();
    const PPB_Audio *ppb_audio = PSInterfaceAudio();
    const PPB_AudioConfig *ppb_audiocfg = PSInterfaceAudioConfig();
    
    private = (SDL_PrivateAudioData *) SDL_calloc(1, (sizeof *private));
    if (private == NULL) {
        SDL_OutOfMemory();
        return 0;
    }
    
    private->mutex = SDL_CreateMutex();
    _this->spec.freq = 44100;
    _this->spec.format = AUDIO_S16LSB;
    _this->spec.channels = 2;
    _this->spec.samples = ppb_audiocfg->RecommendSampleFrameCount(
        instance, 
        PP_AUDIOSAMPLERATE_44100, 
        SAMPLE_FRAME_COUNT);
    
    /* Calculate the final parameters for this audio specification */
    SDL_CalculateAudioSpec(&_this->spec);
    
    private->audio = ppb_audio->Create(
        instance,
        ppb_audiocfg->CreateStereo16Bit(instance, PP_AUDIOSAMPLERATE_44100, _this->spec.samples),
        nacl_audio_callback, 
        _this);
    
    /* Start audio playback while we are still on the main thread. */
    ppb_audio->StartPlayback(private->audio);
    
    return 1;
}

static int
NACLAUD_Init(SDL_AudioDriverImpl * impl)
{
    if (PSGetInstanceId() == 0) {
        return 0;
    }
    
    /* Set the function pointers */
    impl->OpenDevice = NACLAUD_OpenDevice;
    impl->CloseDevice = NACLAUD_CloseDevice;
    impl->OnlyHasDefaultOutputDevice = 1;
    impl->ProvidesOwnCallbackThread = 1;
    /*
     *    impl->WaitDevice = NACLAUD_WaitDevice;
     *    impl->GetDeviceBuf = NACLAUD_GetDeviceBuf;
     *    impl->PlayDevice = NACLAUD_PlayDevice;
     *    impl->Deinitialize = NACLAUD_Deinitialize;
     */
    
    return 1;
}

AudioBootStrap NACLAUD_bootstrap = {
    NACLAUD_DRIVER_NAME, "SDL NaCl Audio Driver",
    NACLAUD_Init, 0
};

#endif /* SDL_AUDIO_DRIVER_NACL */

/* vi: set ts=4 sw=4 expandtab: */

#ifndef IENGINECLIENT_H
#define IENGINECLIENT_H

#ifdef _WIN32
#pragma once
#endif

#include "interface.h"
#include "netadr.h"

struct ref_params_s;

class RenderTarget;


class IEngineClient : public IBaseInterface
{
public:
	virtual					~IEngineClient() {}

	// Formats a message and dispatches it through the down-print path
	virtual void			Con_DnPrintf( int pos, const char *pszMessage, ... ) = 0;

	// Draws an opaque text-message character (HUD text message system)
	virtual int				DrawCharacterOpq( int x, int y, int number, int r, int g, int b, int font ) = 0;

	// Draws an opaque character through the VGUI2 surface
	virtual int				VGUI2_DrawCharacterOpq( int x, int y, int number, int r, int g, int b, unsigned int font ) = 0;

	// Loads (or finds) a client-side model. Returns a model_t* as an opaque pointer
	virtual void			*LoadClientModel( const char *pszModelName ) = 0;

	// Format a network address into pBuffer
	// Use the engine buffer if pBuffer is NULL or nMaxLen is zero
	// onlyBase omits the port
	virtual char			*NET_AdrToString( netadr_t a, int onlyBase, char *pBuffer, size_t nMaxLen ) = 0;

	// Parses a network address string into 'a'. Returns true on success
	virtual qboolean		NET_StringToAdr( char *s, netadr_t *a ) = 0;

	// Writes the address of the connected server into 'pAddress' and returns it
	virtual netadr_t		*GetServerAddress( netadr_t *pAddress ) = 0;

	// Reloads the active skybox
	virtual void			ChangeSkymap( const char *pszSkyboxName ) = 0;

	// Overrides the world sky color (movevars). Returns &movevars on success
	virtual void			*ChangeSkycolor( float r, float g, float b ) = 0;

	// Queues a sound for playback while a demo is being played back
	virtual int				QueueDemoSound( float *origin, int entnum, const char *pszSample, float volume, float attenuation, int pitch, int flags ) = 0;

	// Binds the given render target and sets up the viewport / clears
	virtual void			PushView( RenderTarget *pRenderTarget, bool bClearColor, bool bClearDepth ) = 0;

	// Restores the previous (back-buffer / MSAA) render target and viewport
	virtual void			PopView( void ) = 0;

	// Copies the client refdef into the engine and renders the world
	virtual void			**RenderView( struct ref_params_s *pparams, bool bClear, bool bDrawViewModel, int nextView ) = 0;
};

extern IEngineClient *g_pEngineClient;

#define ENGINECLIENT_INTERFACE_VERSION "SCEngineClient002"

class IClientDLL : public IBaseInterface
{
public:
	virtual					~IClientDLL() {}

	virtual void			Init( CreateInterfaceFn factory ) = 0;
	virtual void			PlaySoundFX( float *origin, int entnum, const char *pszSample, float attenuation, float volume, int pitch, int flags ) = 0;
	virtual void			RenderBegin( struct ref_params_s *pparams ) = 0;
	virtual void			OnRenderViewDone( void ) = 0;
};

extern IClientDLL *g_pClientDLL;

#define CLIENTDLL_INTERFACE_VERSION "SCClientDLL001"

#endif // IENGINECLIENT_H

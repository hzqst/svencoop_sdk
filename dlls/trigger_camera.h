#ifndef TRIGGER_CAMERA_H
#define TRIGGER_CAMERA_H

class CASFunction;
class CBasePlayer;

class CTriggerCamera : public CBaseDelay
{
public:
	CTriggerCamera();
	~CTriggerCamera();

	void OnCreate( void );
	void Spawn( void );
	void KeyValue( KeyValueData *data );
	void Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value );
	void EXPORT FollowTarget( void );
	void Move( void );

	int Save( CSave &save );
	int Restore( CRestore &restore );
	int ObjectCaps( void ) { return 0; }
	static TYPEDESCRIPTION m_SaveData[];

	void CameraPlayer( CBasePlayer *player );
	void UnCameraPlayer( CBasePlayer *player );
	void FindAllPlayers( void );
	void TurnOff( void );
	void EnableMouse( CBasePlayer *player );
	void DisableMouse( CBasePlayer *player );
	int GetCurrentPlayerCount( void );
	int GetHideHUD( void );
	void MouseClicked( CBasePlayer *player, unsigned int event, unsigned int button,
	                   Vector2D position, Vector worldPosition );
	void KeyInput( CBasePlayer *player, int down, int keynum, const char *binding );

private:
	int          m_iMouseActions[3][2];
	string_t     m_iszMouseParameters[3][2];
	bool         m_bMouseBlockDrag[3][2];
	bool         m_bMouseButtonDown[3];
	float        m_flDigitalActionReset;
	float        m_flNextDigitalAction;
	string_t     m_iszASMouseEventCallbackName;
	CASFunction *m_pMouseEventCallback;
	string_t     m_iszASKeyInputCallbackName;
	CASFunction *m_pKeyInputCallback;
	string_t     m_iszOverridePlayerTargetname;
	string_t     m_iszTargetWhenPlayerStartsUsing;
	string_t     m_iszTargetWhenPlayerStopsUsing;
	string_t     m_iszTurnedOffTarget;
	int          m_iMaxPlayerCount;
	string_t     m_iszMaxPlayerTarget;

	CEntityHandle<CBaseEntity> m_hDraggedEntity;
	Vector       m_vecDragEntityOrigin;
	Vector       m_vecDragTraceOrigin;
	float        m_flDragDistance;

	bool         m_bHideFlashlight;
	bool         m_bHideWeapons;
	bool         m_bHideHealth;
	string_t     m_iszCursorSprite;
	bool         m_bCopiedPlayerModel;

	CEntityHandle<CBaseEntity> m_hTarget;
	CEntityHandle<CBaseEntity> m_hPath;
	string_t     m_sPath;
	float        m_flWait;
	float        m_flReturnTime;
	float        m_flStopTime;
	float        m_moveDistance;
	float        m_targetSpeed;
	float        m_initialSpeed;
	float        m_acceleration;
	float        m_deceleration;
	float        m_turnspeed;
	int          m_state;
};

class CTriggerCameraTarget : public CBaseDelay
{
public:
	CTriggerCameraTarget();
	void Spawn( void );
	void KeyValue( KeyValueData *data );
	void Use( CBaseEntity *activator, CBaseEntity *caller, USE_TYPE useType, float value );
	void MouseClicked( CBasePlayer *player, int event, int button, Vector2D position, Vector worldPosition );
	void MouseMoved( CBasePlayer *player, Vector2D position, Vector worldPosition );
	void MouseWheeled( CBasePlayer *player, int delta );

private:
	int      m_iMouseActions[3][2];
	string_t m_iszMouseParameters[3][2];
	bool     m_bMouseBlockDrag[3][2];
	string_t m_iszMouseTargets[3][2];
	float    m_flDigitalActionReset;
	float    m_flNextDigitalAction;
	bool     m_bActive;
};

bool HandleCameraCommand( CBasePlayer *player, const char *command );

#endif

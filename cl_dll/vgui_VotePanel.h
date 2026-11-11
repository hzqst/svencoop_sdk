#ifndef VGUI_VOTEPANEL_H
#define VGUI_VOTEPANEL_H

#include "vgui_Inventory.h"

// Other MapList values append maps
#define MAPLIST_RESET	0
#define MAPLIST_END		123

class CVotePanel : public CMenuPanel
{
public:
	CVotePanel(int iTrans, int iRemoveMe, int x, int y, int wide, int tall);

	virtual void Open(void);
	virtual void Update(void);
	virtual void SetActiveButton(CommandButton *button);
	virtual void FireAction(const char *action);
	void UpdatePlayers(void);

private:
	CommandButton		*m_pActiveButton;
	Label				*m_pTitle;
	Label				*m_pSelection;
	CommandButton		*m_pPlayers[MAX_PLAYERS];
	CTransparentPanel	*m_pActions;
	CommandButton		*m_pClose;
	CommandButton		*m_pKick;
	CommandButton		*m_pKill;
	CommandButton		*m_pBan;
	CommandButton		*m_pMap;
	unsigned int		m_Unknown384;
};

class CMapVotePanel : public CMenuPanel
{
public:
	CMapVotePanel(int iTrans, int iRemoveMe, int x, int y, int wide, int tall);

	virtual void Reset(void);
	virtual void Open(void);
	virtual void Update(void);
	virtual void SetActiveButton(CommandButton *button);
	virtual void FireAction(const char *action);
	virtual int MsgFunc_MapList(const char *pszName, int iSize, void *pbuf);

private:
	CommandButton		*m_pActiveButton;
	int					m_iMaps;
	bool				m_bReceivingMaps;
	Label				*m_pTitle;
	Label				*m_pSelection;
	ScrollPanel			*m_pScrollPanel;
	CScrollButton		**m_pMaps;
	CTransparentPanel	*m_pActions;
	CScrollButton		*m_pClose;
	CScrollButton		*m_pKick;
	CScrollButton		*m_pKill;
	CScrollButton		*m_pBan;
	CScrollButton		*m_pMap;
	unsigned int		m_Unknown272;
};

class CVotePopup : public CMenuPanel
{
public:
	CVotePopup(int iTrans, int iRemoveMe, int x, int y, int wide, int tall);

	virtual void Open(void);
	virtual bool SlotInput(int slot);
	virtual void Update(void);
	virtual int MsgFunc_VoteMenu(const char *pszName, int iSize, void *pbuf);

private:
	int					m_iVoteType;
	CTransparentPanel	*m_pBackground;
	Label				*m_pTitle;
	Label				*m_pText;
	CommandButton		*m_pYes;
	CommandButton		*m_pNo;
	unsigned int		m_Unknown244;
};

#endif

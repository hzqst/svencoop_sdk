#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "CASTextMenu.h"
#include "CASTextMenus.h"
#include "CASFunction.h"
#include "CASServerManager.h"
#include "CASLog.h"
#include "../external/angelscript/sdk/add_on/scriptany/scriptany.h"
#include "../external/angelscript/sdk/add_on/scriptarray/scriptarray.h"
#include <sstream>

#define TEXT_MENU_SLOT_BACK	8
#define TEXT_MENU_SLOT_MORE	9
#define TEXT_MENU_SLOT_EXIT	10

CASTextMenu::Item::Item( const CString &name, CScriptAny *userData )
    : m_szName( name ), m_pUserData( userData )
{
}

CASTextMenu::Item::Item( Item &&other ) : m_szName( other.m_szName ), m_pUserData( other.m_pUserData )
{
	other.m_pUserData = NULL;
}

CASTextMenu::Item::~Item()
{
	if ( m_pUserData )
		m_pUserData->Release();

	m_pUserData = NULL;
}

CASTextMenu::CASTextMenu( asIScriptFunction *inputCallback )
    : m_iId( -1 ),
      m_pInputCallback( CASFunction::Create( inputCallback, CASServerManager::GetInstance()->m_pActiveModule, true ) ),
      m_uiItemsPerPage( 7 )
{
	if ( !inputCallback )
		CASLog::GetInstance()->Warning( AS_LOG_DEFAULT, "CTextMenu::CTextMenu: no input callback provided!\n" );
}

CASTextMenu::~CASTextMenu()
{
	ClearCallbacks();

	if ( IsRegistered() )
		Unregister();
}

void CASTextMenu::Release() const
{
	m_bGCFlag = false;

	if ( InternalRelease() )
		delete this;
}

void CASTextMenu::EnumReferences( asIScriptEngine *engine )
{
	if ( m_pInputCallback )
		engine->GCEnumCallback( m_pInputCallback->GetFunction() );
}

void CASTextMenu::ReleaseReferences( asIScriptEngine *engine )
{
	ClearCallbacks();
}

void CASTextMenu::ClearCallbacks()
{
	if ( m_pInputCallback )
		m_pInputCallback->Release();

	m_pInputCallback = NULL;
}

TextMenuId_t CASTextMenu::GetId() const
{
	return m_iId;
}

bool CASTextMenu::IsRegistered() const
{
	return m_iId != -1;
}

bool CASTextMenu::Register()
{
	return CASTextMenus::GetInstance()->RegisterMenu( this );
}

void CASTextMenu::Unregister()
{
	CASTextMenus::GetInstance()->UnregisterMenu( this );
}

size_t CASTextMenu::GetItemCount() const
{
	return m_Items.size();
}

unsigned int CASTextMenu::GetPageCount() const
{
	unsigned int count = m_Items.size();

	if ( !m_uiItemsPerPage || count <= m_uiItemsPerPage + 2 )
		return 1;

	return count / m_uiItemsPerPage + ( count % m_uiItemsPerPage != 0 );
}

const CASTextMenu::Item *CASTextMenu::GetItem( size_t index ) const
{
	if ( index > m_Items.size() )
		return NULL;

	return m_Items.data() + index;
}

const CString &CASTextMenu::GetTitle() const
{
	return m_szTitle;
}

void CASTextMenu::SetTitle( const CString &title )
{
	m_szTitle = title;
}

void CASTextMenu::AddItem( const CString &name, CScriptAny *userData )
{
	m_Items.push_back( Item( name, userData ) );
}

unsigned int CASTextMenu::GetFirstItemForPage( unsigned int page ) const
{
	if ( !m_uiItemsPerPage )
		return 0;

	unsigned int first = page * m_uiItemsPerPage;

	return first > m_Items.size() ? 0 : first;
}

unsigned int CASTextMenu::GetItemCountForPage( unsigned int page ) const
{
	unsigned int count = m_Items.size();

	if ( count && m_uiItemsPerPage && count > m_uiItemsPerPage + 2 )
	{
		count -= page * m_uiItemsPerPage;

		if ( count > m_uiItemsPerPage )
			return m_uiItemsPerPage;
	}

	return count;
}

std::string CASTextMenu::GetPageText( CBasePlayer *player, unsigned int page, int &validSlots ) const
{
	unsigned int pageCount = GetPageCount();

	if ( page >= pageCount )
		return std::string();

	std::stringstream text;

	text << m_szTitle.Data();

	if ( pageCount > 1 )
		text << page + 1 << '/' << pageCount;

	text << '\n';
	validSlots = 1 << ( TEXT_MENU_SLOT_EXIT - 1 );

	unsigned int first = GetFirstItemForPage( page );
	unsigned int count = GetItemCountForPage( page );

	for ( unsigned int index = first; index < first + count; index++ )
	{
		unsigned int slot = index - first;

		text << slot + 1 << ": " << m_Items[index].m_szName.Data() << '\n';
		validSlots |= 1 << slot;
	}

	for ( unsigned int index = count; index < m_uiItemsPerPage; index++ )
		text << '\n';

	text << '\n';

	if ( page )
	{
		text << "8: Back\n";
		validSlots |= 1 << ( TEXT_MENU_SLOT_BACK - 1 );
	}
	else
		text << '\n';

	if ( page < pageCount - 1 )
	{
		text << "9: More\n";
		validSlots |= 1 << ( TEXT_MENU_SLOT_MORE - 1 );
	}
	else
		text << '\n';

	text << "0: Exit\n";
	return text.str();
}

void CASTextMenu::Open( int displayTime, unsigned int page, CScriptArray *players )
{
	if ( (unsigned int) displayTime > 255 )
		return;

	if ( !m_pInputCallback )
	{
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT, "CTextMenu::Open: No input callback provided, cannot open!\n" );
		return;
	}

	if ( !IsRegistered() )
	{
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT, "CTextMenu::Open: Menu is not registered!\n" );
		return;
	}

	if ( players )
	{
		for ( unsigned int index = 0; index < players->GetSize(); index++ )
		{
			// The array element address is used directly as an edict
			edict_t *entity = (edict_t *) players->At( index );
			CBasePlayer *player = NULL;

			if ( entity && !entity->free )
				player = (CBasePlayer *) entity->pvPrivateData;

			Open( displayTime, page, player );
		}

		players->Release();
	}
	else
	{
		for ( int index = 1; index <= gpGlobals->maxClients; index++ )
			Open( displayTime, page, (CBasePlayer *) UTIL_PlayerByIndex( index ) );
	}
}

void CASTextMenu::Open( int displayTime, unsigned int page, CBasePlayer *player )
{
	if ( (unsigned int) displayTime > 255 || !player )
		return;

	if ( !m_pInputCallback )
	{
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT, "CTextMenu::Open: No input callback provided, cannot open!\n" );
		return;
	}

	if ( !IsRegistered() )
	{
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT, "CTextMenu::Open: Menu is not registered!\n" );
		return;
	}

	if ( !GetItemCount() )
	{
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT, "CTextMenu::Open: No items to display!\n" );
		return;
	}

	int validSlots = 0;
	std::string text = GetPageText( player, page, validSlots );

	if ( text.empty() )
	{
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT, "CTextMenu::Open: Invalid page '%u'!\n", page );
		return;
	}

	ShowMenu( player, validSlots, displayTime, &text[0], page );
}

void CASTextMenu::ShowMenu( CBasePlayer *player, int validSlots, int displayTime, char *text, unsigned int page )
{
	if ( player && player->IsConnected() )
		player->ShowMenu( validSlots, displayTime, text, m_iId, page );
}

void CASTextMenu::InputReceived( CBasePlayer *player, int slot )
{
	switch ( slot )
	{
		case TEXT_MENU_SLOT_BACK:
			if ( player->m_uiTextMenuPage )
			{
				Open( 0, player->m_uiTextMenuPage - 1, player );
				return;
			}
			break;

		case TEXT_MENU_SLOT_MORE:
			if ( player->m_uiTextMenuPage < GetPageCount() - 1 )
			{
				Open( 0, player->m_uiTextMenuPage + 1, player );
				return;
			}
			break;

		case TEXT_MENU_SLOT_EXIT:
			m_pInputCallback->Call( 0, this, player, slot, (const Item *) NULL );
			return;
	}

	unsigned int index = (unsigned int) slot + m_uiItemsPerPage * player->m_uiTextMenuPage - 1;
	const Item *item = GetItem( index );

	if ( item )
		m_pInputCallback->Call( 0, this, player, slot, item );
	else
		CASLog::GetInstance()->Error( AS_LOG_DEFAULT, "CTextMenu::InputReceived: Item index '%u' was invalid!\n", index );
}

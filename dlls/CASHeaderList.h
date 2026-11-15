#ifndef CASHEADERLIST_H
#define CASHEADERLIST_H

#include "CASBaseClass.h"

class CCommand;

class CASHeaderList : public CASRefCountedBaseClass
{
public:
	CASHeaderList();
	~CASHeaderList();

	void Release(void) const;
	bool HasHeader(const char *name) const;
	void AddHeader(const char *name, const char *source);
	void Clear(void);

private:
	friend void as_generate_includes_Callback( const CCommand &command );
	friend class CASModuleBuilder;

	struct Header_t
	{
		Header_t	*m_pNext;
		char		*m_pszName;
		char		*m_pszSource;
	};

	Header_t		*m_pHead;
	Header_t		*m_pTail;
	unsigned int	m_uiCount;
};

#endif

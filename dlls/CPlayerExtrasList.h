#ifndef CPLAYEREXTRASLIST_H
#define CPLAYEREXTRASLIST_H

#include "donid.h"

class CPlayerExtrasList
{
public:
	CPlayerExtrasList();
	~CPlayerExtrasList();

	bool Load();
	bool LoadCSV( const char *text, size_t length );
	void Clear();
	unsigned int Count() const;
	const extrasinfo_s *Get( unsigned int index ) const;
	const extrasinfo_s *Find( const char *authId ) const;

private:
	CPlayerExtrasList( const CPlayerExtrasList & );
	CPlayerExtrasList &operator=( const CPlayerExtrasList & );

	struct Entry
	{
		extrasinfo_s	info;
		Entry			*next;
		Entry			*previous;
	};

	unsigned int	m_uiCount;
	Entry			*m_pFirst;
};

#endif

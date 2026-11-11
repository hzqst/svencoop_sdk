#ifndef CASGCBASECLASS_H
#define CASGCBASECLASS_H

#include "CASBaseClass.h"

template <class T>
class CASGCBaseClass : public T
{
public:
	void AddRef(void) const
	{
		m_bGCFlag = false;
		T::AddRef();
	}

	bool GetGCFlag(void) const { return m_bGCFlag; }
	void SetGCFlag(void) const { m_bGCFlag = true; }

protected:
	CASGCBaseClass() : m_bGCFlag(false) {}

private:
	friend class CASTextMenu;
	friend class CASVote;

	mutable bool m_bGCFlag;
};

#endif

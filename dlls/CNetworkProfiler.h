#ifndef CNETWORKPROFILER_H
#define CNETWORKPROFILER_H

#include "CNetworkMessages.h"
#include <string>

class CNetworkProfiler
{
public:
	enum ReportMode_t
	{
		REPORT_LIST = 1,
		REPORT_TOP_N = 2
	};

	struct UserMsgProfileData_t
	{
		unsigned int       m_uiIndex;
		unsigned long long m_uiBytes;
	};

	CNetworkProfiler();
	~CNetworkProfiler();

	static CNetworkProfiler *GetInstance();

	void Reset();
	void Think();
	void GenerateReport( std::string &output, ReportMode_t mode, unsigned int topN );
	void OutputToConsole( ReportMode_t mode, unsigned int topN );
	void OutputToFile( const char *fileName, ReportMode_t mode, unsigned int topN );
	void OutputUsage( const char *command );
	bool HandleCommand( const char *command );

private:
	struct UserMsgProfileDataSorter_t
	{
		bool operator()( const UserMsgProfileData_t *left, const UserMsgProfileData_t *right ) const;
	};

	void OutputArrayToString( std::string &output, const UserMsgProfileData_t *const *messages, unsigned int count );

	UserMsgProfileData_t        m_UserMessages[MAX_USER_MESSAGES];
	const UserMsgProfileData_t *m_pUserMessages[MAX_USER_MESSAGES];
};

#endif

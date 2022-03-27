#include "stdafx.h"
#include "Level.h"

extern	void	GetCDKey_FromRegistry(char* CDKeyStr);
char const * ComputeClientDigest(string128& dest, bool lower_case)
{
	return " ";
};

void CLevel::SendClientDigestToServer()
{
	string128 tmp_digest;
	NET_Packet P;
	P.w_begin			(M_SV_DIGEST);
	m_client_digest		= ComputeClientDigest(tmp_digest, true);
	P.w_stringZ			(m_client_digest);
	P.w_stringZ			(ComputeClientDigest(tmp_digest, false));
	SecureSend			(P, net_flags(TRUE, TRUE, TRUE, TRUE));
}
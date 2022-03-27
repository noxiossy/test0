#ifndef XR_DSA_SIGNER_INCLUDED
#define XR_DSA_SIGNER_INCLUDED


class xr_dsa_signer
{
public:
						xr_dsa_signer			();
						~xr_dsa_signer			();

	shared_str const	sign					();
	shared_str const	sign_mt					();
	
}; //xr_dsa_signer

#endif //#ifndef XR_DSA_SIGNER_INCLUDED
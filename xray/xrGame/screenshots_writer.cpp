#include "stdafx.h"
#include "screenshots_writer.h"
#include "screenshots_common.h"

namespace screenshots
{

writer::writer(u8* jpeg_data, u32 jpeg_size, u32 jpeg_buffer_size) :
	m_info_data(NULL, FALSE, FALSE, FALSE)
{
	VERIFY				(jpeg_buffer_size - jpeg_size >= info_max_size);
	m_buffer			= jpeg_data;
	m_buffer_size		= jpeg_buffer_size;
	m_buffer_info_pos	= jpeg_size;
}

writer::~writer()
{
}

char const * ss_info_secion			= "screenshot_info";
char const * ss_player_name_key		= "player_name";
char const * ss_player_digest_key	= "player_digest";
char const * ss_admin_name_key		= "admin_name";
char const * ss_digital_sign_key	= "digital_sign";
char const * ss_creation_date		= "creation_date";

void writer::set_player_name			(shared_str const & pname)
{
	m_info_data.w_string(ss_info_secion, ss_player_name_key, pname.c_str());
}
void writer::set_player_cdkey_digest	(shared_str const & cdkeydigest)
{
	m_info_data.w_string(ss_info_secion, ss_player_digest_key, cdkeydigest.c_str());
}

//signer 

signer::signer() : xr_dsa_signer()
{
	feel_private_dsa_key();
}

signer::~signer()
{
}

void signer::feel_private_dsa_key()
{
}

}
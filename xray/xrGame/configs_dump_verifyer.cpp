#include "stdafx.h"
#include "configs_dump_verifyer.h"
#include "configs_common.h"
#include "configs_dumper.h"

namespace mp_anticheat
{

dump_verifyer::dump_verifyer() :
	xr_dsa_verifyer()
{
}

dump_verifyer::~dump_verifyer()
{
}

configs_verifyer::configs_verifyer()
{
	m_original_config.start_dump	();
	while (m_original_config.dump_one(m_orig_config_body)) {};
	m_orig_config_end_pos			= m_orig_config_body.tell();
}

configs_verifyer::~configs_verifyer()
{
}

static char* search_info_section(u8* buffer, u32 buffer_size)
{
	u32	sstr_size		= xr_strlen(cd_info_secion);
	VERIFY				(buffer_size >= sstr_size);
	u8* rbegin			= buffer + (buffer_size - sstr_size);
	int r_size			= static_cast<int>(buffer_size - sstr_size);
	do
	{
		if (!memcmp(rbegin, cd_info_secion, sstr_size))
		{
			return static_cast<char*>((void*)rbegin);
		}
		--rbegin;
		--r_size;
	}
	while (r_size > 0);
	return NULL;
}

bool const configs_verifyer::verify_dsign()
{
	return true;
}

LPCSTR configs_verifyer::get_section_diff(CInifile::Sect* sect_ptr, CInifile & active_params, string256 & dst_diff)
{
	LPCSTR diff_str = NULL;
	bool	tmp_active_param = false;
	if (!strncmp(sect_ptr->Name.c_str(), "ap_", 3))
	{
		tmp_active_param = true;
	}

	for (CInifile::SectCIt cit = sect_ptr->Data.begin(),
			ciet = sect_ptr->Data.end(); cit != ciet; ++cit)
	{
		shared_str const &	tmp_value = cit->second;
		shared_str			real_value;
		if (tmp_active_param)
		{
			if (active_params.line_exist(sect_ptr->Name.c_str(), cit->first))
			{
				real_value = active_params.r_string(sect_ptr->Name.c_str(), cit->first.c_str());
				if (tmp_value != real_value)
				{
					LPCSTR	tmp_key_str = NULL;
					STRCONCAT(tmp_key_str,
						sect_ptr->Name.c_str(), "::", cit->first.c_str());
					STRCONCAT(diff_str,
						tmp_key_str,
						" = ",
						tmp_value.c_str(),
						",right = ",
						real_value.c_str());
					strncpy_s(dst_diff, diff_str, sizeof(dst_diff) - 1);
					dst_diff[sizeof(dst_diff) - 1] = 0;
					return dst_diff;
				}
				continue;
			}
		}
		if (!pSettings->line_exist(sect_ptr->Name, cit->first))
		{
			STRCONCAT(diff_str,
				"line ",
				sect_ptr->Name.c_str(),
				"::",
				cit->first.c_str(),
				" not found");
			strncpy_s(dst_diff, diff_str, sizeof(dst_diff) - 1);
			dst_diff[sizeof(dst_diff) - 1] = 0;
			return dst_diff;
		}
		real_value = pSettings->r_string(sect_ptr->Name.c_str(), cit->first.c_str());
		if (tmp_value != real_value)
		{
			LPCSTR	tmp_key_str = NULL;
			STRCONCAT(tmp_key_str,
				sect_ptr->Name.c_str(), "::", cit->first.c_str());
			STRCONCAT(diff_str,
				tmp_key_str,
				" = ",
				tmp_value.c_str(),
				",right = ",
				real_value.c_str());
			strncpy_s(dst_diff, diff_str, sizeof(dst_diff) - 1);
			dst_diff[sizeof(dst_diff) - 1] = 0;
			return dst_diff;
		}
	}
	return NULL;
}

LPCSTR configs_verifyer::get_diff(CInifile & received,
								  CInifile & active_params,
								  string256 & dst_diff)
{
	LPCSTR diff_str = NULL;
	for (CInifile::RootIt sit = received.sections().begin(),
		siet = received.sections().end(); sit != siet; ++sit)
	{
		CInifile::Sect*	tmp_sect = *sit;
		if (tmp_sect->Name == cd_info_secion)
			continue;
		if (tmp_sect->Name == active_params_section)
			continue;

		diff_str = get_section_diff(tmp_sect, active_params, dst_diff);
		if (diff_str)
		{
			return diff_str;
		}
	}
	strcpy_s(dst_diff, "unknown diff or currepted config dump");
	return dst_diff;
}

bool const configs_verifyer::verify(u8* data, u32 data_size, string256 & diff)
{
	IReader		tmp_reader(data, data_size);
	CInifile	tmp_ini(&tmp_reader);
	CInifile	tmp_active_params(NULL, FALSE, FALSE, FALSE);
	
	string16	tmp_digit;
	u32			ap_index = 1;
	sprintf_s	(tmp_digit, "%d", ap_index);
	while		(tmp_ini.line_exist(active_params_section, tmp_digit))
	{
		LPCSTR	tmp_ap_section		= tmp_ini.r_string(active_params_section, tmp_digit);
		tmp_active_params.w_string	(active_params_section, tmp_digit, tmp_ap_section);
		m_original_ap.load_to		(tmp_ap_section, tmp_active_params);
		++ap_index;
		sprintf_s					(tmp_digit, "%d", ap_index);
	}
	
	m_orig_config_body.seek			(m_orig_config_end_pos);
	tmp_active_params.save_as		(m_orig_config_body);

	if (!tmp_ini.line_exist(cd_info_secion, cd_player_name_key) ||
		!tmp_ini.line_exist(cd_info_secion, cd_player_digest_key) ||
		!tmp_ini.line_exist(cd_info_secion, cd_creation_date) ||
		!tmp_ini.line_exist(cd_info_secion, cd_digital_sign_key))
	{
		strcpy_s(diff, "invalid dump");
		return false;
	}

	LPCSTR		add_str = NULL;
	STRCONCAT	(add_str,
		tmp_ini.r_string(cd_info_secion, cd_player_name_key),
		tmp_ini.r_string(cd_info_secion, cd_player_digest_key),
		tmp_ini.r_string(cd_info_secion, cd_creation_date));

	m_orig_config_body.w_stringZ(add_str);
	
	return true;
}

} //namespace mp_anticheat